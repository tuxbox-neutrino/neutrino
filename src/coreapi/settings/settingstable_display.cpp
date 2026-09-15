/*
 * settingstable_display.cpp - front display settings, one row per field
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "settingstable.h"
#include "settingsfield.h"

namespace coreapi
{

namespace
{

/* Everything the box shows on a display of its own: the front panel, the
   graphical LCD and the LCD4Linux panel.

   Two thirds of these sit behind a build condition, because that is where the
   settings struct puts the fields: src/system/settings.h:316 for the graphical
   LCD and :334 for LCD4Linux. Neither condition is one check-hardware.sh reads,
   so the rows behind them are compiled by nothing that runs here.

   lcd_scroll is offered twice and is one setting either way: how many times a
   line too long for the panel is scrolled past, which the driver writes to
   /proc/stb/lcd/scroll_repeats, src/driver/simple_display.cpp:521, and which
   nought turns off. The screen picks between a count and an on and an off by
   whether that file is there, src/gui/vfd_setup.cpp:194, and on the panels
   where it is not the driver does nothing with the number but read it as a
   flag, src/driver/vfd.cpp:811. So the row is the count, and the on and the off
   are its nought and its one. The two items carry two labels and the row names
   the one the count carries. */

// src/gui/vfd_setup.cpp:115
const EnumValue kLcdInfoLine[] =
{
	{ 0, "lcd_info_line_channelname" },
	{ 1, "lcd_info_line_clock" }
};

// src/gui/vfd_setup.cpp:106
const EnumValue kLedMode[] =
{
	{ 0, "ledcontroler.off" },
	{ 1, "ledcontroler.on.all" },
	{ 2, "ledcontroler.on.led1" },
	{ 3, "ledcontroler.on.led2" }
};

// src/gui/lcd4l_setup.cpp:55
const EnumValue kLcd4lSupport[] =
{
	{ 0, "lcd4l_support_off" },
	{ 1, "lcd4l_support_auto" },
	{ 2, "lcd4l_support_on" }
};

#if defined(ENABLE_GRAPHLCD) || defined(ENABLE_LCD4LINUX)
/* Both standby brightnesses are offered only where the box really goes to
   standby. The flag reads nought as on, src/gui/widget/menue_options.h:8, which
   is why the comparison is against nought and not against one. */
const Condition kNotRealShutdown[] =
{
	{ "shutdown_real", CompareOp::Eq, 0, NULL, 0 }
};
#endif

const Descriptor kSettings[] =
{
	/* The channel line the front panel shows. The wording of nought is the
	   box's: src/gui/vfd_setup.cpp:117 calls it the channel number on the H7
	   and the BRE2ZE4K and the channel name everywhere else. */
	// src/neutrino.cpp:636 src/gui/vfd_setup.cpp:189
	{
		"lcd_info_line", ValueType::Enum, "display",
		"lcd_info_line", "menu.hint_vfd_infoline",
		0, 0, COREAPI_ENUM(kLcdInfoLine), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd_info_line)
	},
	// src/neutrino.cpp:638 src/gui/vfd_setup.cpp:211
	{
		"lcd_notify_rclock", ValueType::Bool, "display",
		"lcdmenu.notify_rclock", "menu.hint_vfd_notify_rclock",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd_notify_rclock)
	},
	/* How many times a line too long for the panel is scrolled past, and nought
	   is off. The second item the screen offers for it is this number held to
	   its nought and its one, which is a file on the box and not a setting to
	   condition on. src/gui/vfd_setup.cpp:194 */
	// src/neutrino.cpp:651 src/gui/vfd_setup.cpp:197
	{
		"lcd_scroll", ValueType::Int, "display",
		"lcdmenu.scroll_repeats", "menu.hint_vfd_scroll",
		0, 999, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd_scroll)
	},
	/* The floor is what off means rather than nought, which is a brightness of
	   none. The ceiling is the panel's and the build decides it. */
	// src/neutrino.cpp:635 src/gui/vfd_setup.cpp:324 src/gui/vfd_setup.cpp:326
	{
		"lcd_dim_brightness", ValueType::Int, "display",
		"lcdmenu.dim_brightness", "menu.hint_vfd_brightnessdim",
#ifdef ENABLE_LCD
		-1, 255,
#else
		-1, 15,
#endif
		NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd_setting_dim_brightness)
	},
	/* Minutes kept as text. The screen takes three digits and nothing else,
	   src/gui/vfd_setup.cpp:333, and the driver reads the text with atoi,
	   src/driver/vfd.cpp. A String row carries neither the length nor the
	   digits. */
	// src/neutrino.cpp:634 src/gui/vfd_setup.cpp:335
	{
		"lcd_dim_time", ValueType::String, "display",
		"lcdmenu.dim_time", "menu.hint_vfd_dimtime",
		0, 0, NULL, 0, 0, "0", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(lcd_setting_dim_time)
	},
	// src/neutrino.cpp:644 src/gui/vfd_setup.cpp:359
	{
		"led_tv_mode", ValueType::Enum, "display",
		"ledcontroler.mode.tv", "menu.hint_leds_tv",
		0, 0, COREAPI_ENUM(kLedMode), 2, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_tv_mode)
	},
	// src/neutrino.cpp:645 src/gui/vfd_setup.cpp:363
	{
		"led_standby_mode", ValueType::Enum, "display",
		"ledcontroler.mode.standby", "menu.hint_leds_standby",
		0, 0, COREAPI_ENUM(kLedMode), 3, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_standby_mode)
	},
	// src/neutrino.cpp:646 src/gui/vfd_setup.cpp:367
	{
		"led_deep_mode", ValueType::Enum, "display",
		"ledcontroler.mode.deepstandby", "menu.hint_leds_deepstandby",
		0, 0, COREAPI_ENUM(kLedMode), 3, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_deep_mode)
	},
	// src/neutrino.cpp:647 src/gui/vfd_setup.cpp:371
	{
		"led_rec_mode", ValueType::Enum, "display",
		"ledcontroler.mode.record", "menu.hint_leds_record",
		0, 0, COREAPI_ENUM(kLedMode), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_rec_mode)
	},
	// src/neutrino.cpp:648 src/gui/vfd_setup.cpp:375
	{
		"led_blink", ValueType::Bool, "display",
		"ledcontroler.blink", "menu.hint_leds_blink",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(led_blink)
	},
	// src/neutrino.cpp:640 src/gui/vfd_setup.cpp:385
	{
		"backlight_tv", ValueType::Bool, "display",
		"ledcontroler.backlight.tv", "menu.hint_leds_tv",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(backlight_tv)
	},
	// src/neutrino.cpp:641 src/gui/vfd_setup.cpp:389
	{
		"backlight_standby", ValueType::Bool, "display",
		"ledcontroler.mode.standby", "menu.hint_leds_standby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(backlight_standby)
	},
	// src/neutrino.cpp:642 src/gui/vfd_setup.cpp:393
	{
		"backlight_deepstandby", ValueType::Bool, "display",
		"ledcontroler.mode.deepstandby", "menu.hint_leds_deepstandby",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(backlight_deepstandby)
	},
#ifdef ENABLE_GRAPHLCD
	/* The default is the box's own display and no constant states it: the
	   loader asks the hardware whether the panel is a graphical one. Nought is
	   what a box without one gets. */
	// src/neutrino.cpp:415 src/gui/glcdsetup.cpp:306
	{
		"glcd_enable", ValueType::Bool, "display",
		"glcd.enable", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_enable)
	},
	/* An index into the driver list the graphlcd configuration file holds, so
	   no constant states the ceiling. Anything outside it the box turns into
	   nought, src/driver/glcd/glcd.cpp:929, which is why a wide one is safe. */
	// src/neutrino.cpp:432 src/gui/glcdsetup.cpp:308
	{
		"glcd_selected_config", ValueType::Int, "display",
		"glcd.display", NULL,
		0, 99, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_selected_config)
	},
	// src/neutrino.cpp:416 src/gui/glcdsetup.cpp:315
	{
		"glcd_logodir", ValueType::String, "display",
		"glcd.logodir", NULL,
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/logos", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(glcd_logodir)
	},
	// src/neutrino.cpp:418 src/gui/glcdsetup.cpp:413
	{
		"glcd_brightness", ValueType::Int, "display",
		"glcd.brightness", NULL,
		0, 10, NULL, 0, GLCD_DEFAULT_BRIGHTNESS, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_brightness)
	},
	// src/neutrino.cpp:421 src/gui/glcdsetup.cpp:417
	{
		"glcd_brightness_standby", ValueType::Int, "display",
		"glcd.brightness_standby", NULL,
		0, 10, NULL, 0, GLCD_DEFAULT_BRIGHTNESS_STANDBY, NULL, false, false,
		COREAPI_CONDITIONS(kNotRealShutdown),
		COREAPI_NUMBER_FIELD(glcd_brightness_standby)
	},
	// src/neutrino.cpp:419 src/gui/glcdsetup.cpp:423
	{
		"glcd_brightness_dim", ValueType::Int, "display",
		"glcd.brightness_dim", NULL,
		0, 10, NULL, 0, GLCD_DEFAULT_BRIGHTNESS_DIM, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_brightness_dim)
	},
	/* Seconds kept as text, five digits on the screen and no more,
	   src/gui/glcdsetup.cpp:427. A String row carries neither. */
	// src/neutrino.cpp:420 src/gui/glcdsetup.cpp:428
	{
		"glcd_brightness_dim_time", ValueType::String, "display",
		"glcd.brightness_dim_time", NULL,
		0, 0, NULL, 0, 0, GLCD_DEFAULT_BRIGHTNESS_DIM_TIME, false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(glcd_brightness_dim_time)
	},
	// src/neutrino.cpp:424 src/gui/glcdsetup.cpp:321
	{
		"glcd_scroll", ValueType::Bool, "display",
		"glcd.scroll", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_scroll)
	},
	/* The default is the box model's: src/neutrino.cpp:425 gives the VU Uno 4K
	   SE one and :427 gives four more VU boxes two. Five is what every other
	   box gets. */
	// src/neutrino.cpp:430 src/gui/glcdsetup.cpp:323
	{
		"glcd_scroll_speed", ValueType::Int, "display",
		"glcd.scroll_speed", NULL,
		1, 63, NULL, 0, 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_scroll_speed)
	},
	// src/neutrino.cpp:422 src/gui/glcdsetup.cpp:325
	{
		"glcd_mirror_osd", ValueType::Bool, "display",
		"glcd.mirror_osd", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_mirror_osd)
	},
	// src/neutrino.cpp:423 src/gui/glcdsetup.cpp:327
	{
		"glcd_mirror_video", ValueType::Bool, "display",
		"glcd.mirror_video", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(glcd_mirror_video)
	},
#endif
#ifdef ENABLE_LCD4LINUX
	// src/neutrino.cpp:436 src/gui/lcd4l_setup.cpp:185
	{
		"lcd4l_support", ValueType::Enum, "display",
		"lcd4l_support", "menu.hint_lcd4l_support",
		0, 0, COREAPI_ENUM(kLcd4lSupport), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_support)
	},
	// src/neutrino.cpp:437 src/gui/lcd4l_setup.cpp:201
	{
		"lcd4l_logodir", ValueType::String, "display",
		"lcd4l_logodir", "menu.hint_lcd4l_logodir",
		0, 0, NULL, 0, 0, TARGET_ROOT "/media/sda1/logos", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(lcd4l_logodir)
	},
	/* A number and not a choice: the screen names the four panels with literal
	   sizes rather than locales, src/gui/lcd4l_setup.cpp:63, so a choice here
	   would offer words the program does not have. The range is the enum's,
	   src/driver/lcd4l.h:48. */
	// src/neutrino.cpp:438 src/gui/lcd4l_setup.cpp:191
	{
		"lcd4l_display_type", ValueType::Int, "display",
		"lcd4l_display_type", "menu.hint_lcd4l_display_type",
		0, 3, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_display_type)
	},
	/* A number and not a choice for a different reason: the screen offers two
	   different lists for it and picks by the panel,
	   src/gui/lcd4l_setup.cpp:307-310. The ceiling is the highest either lists
	   and the values between are ones neither offers. */
	// src/neutrino.cpp:439 src/gui/lcd4l_setup.cpp:308
	{
		"lcd4l_skin", ValueType::Int, "display",
		"lcd4l_skin", "menu.hint_lcd4l_skin",
		0, 100, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_skin)
	},
	// src/neutrino.cpp:440 src/gui/lcd4l_setup.cpp:314
	{
		"lcd4l_skin_radio", ValueType::Bool, "display",
		"lcd4l_skin_radio", "menu.hint_lcd4l_skin_radio",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_skin_radio)
	},
	/* The ceiling is the panel's and the box asks it at run time,
	   src/driver/lcd4l.cpp:270: seven for the Pearl panel and ten for the three
	   Samsung ones. Ten is the wider and holds every value either offers. */
	// src/neutrino.cpp:441 src/gui/lcd4l_setup.cpp:318
	{
		"lcd4l_brightness", ValueType::Int, "display",
		"lcd4l_brightness", "menu.hint_lcd4l_brightness",
		1, 10, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_brightness)
	},
	// src/neutrino.cpp:442 src/gui/lcd4l_setup.cpp:322
	{
		"lcd4l_brightness_standby", ValueType::Int, "display",
		"lcd4l_brightness_standby", "menu.hint_lcd4l_brightness_standby",
		1, 10, NULL, 0, 3, NULL, false, false,
		COREAPI_CONDITIONS(kNotRealShutdown),
		COREAPI_NUMBER_FIELD(lcd4l_brightness_standby)
	},
	// src/neutrino.cpp:443 src/gui/lcd4l_setup.cpp:223
	{
		"lcd4l_convert", ValueType::Bool, "display",
		"lcd4l_convert", "menu.hint_lcd4l_convert",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_convert)
	},
	/* Read by the web interface out of the saved file and by nothing in the
	   program, data/y-web/Y_Boxcontrol_Menue.yhtm:174, so a written value takes
	   effect when it has been saved and not before. */
	// src/neutrino.cpp:444 src/gui/lcd4l_setup.cpp:227
	{
		"lcd4l_screenshots", ValueType::Bool, "display",
		"lcd4l_screenshots", "menu.hint_lcd4l_screenshots",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(lcd4l_screenshots)
	},
#endif

#ifdef ENABLE_GRAPHLCD
	/* The name of the panel theme, picked from a file list,
	   src/gui/glcdthemes.cpp:456, and read at start. Its default is a migration
	   name where the box has no settings file yet and empty otherwise, which is
	   what every box that has saved once gets. No item names it.
	   src/neutrino.cpp:412 */
	// src/neutrino.cpp:412
	{
		"glcd_theme_name", ValueType::String, "display",
		NULL, NULL,
		0, 0, NULL, 0, 0, "", true, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(glcd_theme_name)
	},
#endif
};

} // anonymous namespace

const Descriptor *settingsTableDisplay(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
