/*
 * settingstable_display.cpp - front display settings, one row per field
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, rework, fixes)
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
#include "boxdefaults.h"
#include "predicates.h"
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
constexpr EnumValue kLcdInfoLine[] =
{
	option(0).label("lcd_info_line_channelname"),
	option(1).label("lcd_info_line_clock")
};

// src/gui/vfd_setup.cpp:106
constexpr EnumValue kLedMode[] =
{
	option(0).label("ledcontroler.off"),
	option(1).label("ledcontroler.on.all"),
	option(2).label("ledcontroler.on.led1"),
	option(3).label("ledcontroler.on.led2")
};

// src/gui/lcd4l_setup.cpp:55
constexpr EnumValue kLcd4lSupport[] =
{
	option(0).label("lcd4l_support_off"),
	option(1).label("lcd4l_support_auto"),
	option(2).label("lcd4l_support_on")
};

#if defined(ENABLE_GRAPHLCD) || defined(ENABLE_LCD4LINUX)
/* Both standby brightnesses are offered only where the box really goes to
   standby. The flag reads nought as on, src/gui/widget/menue_options.h:8, which
   is why the comparison is against nought and not against one. */
constexpr Condition kNotRealShutdown[] =
{
	when("shutdown_real").is(0)
};
#endif

#ifdef ENABLE_GRAPHLCD
long glcdEnableDefault()
{
	return hasGraphicPanel() ? 1 : 0;
}

long glcdScrollSpeedDefault()
{
	if (boxdefault::kScrollSpeedOne)
		return 1;
	if (boxdefault::kScrollSpeedTwo)
		return 2;
	return 5;
}
#endif

constexpr Descriptor kSettings[] =
{
	/* The channel line the front panel shows. The wording of nought is the
	   box's: src/gui/vfd_setup.cpp:117 calls it the channel number on the H7
	   and the BRE2ZE4K and the channel name everywhere else. */
	// src/neutrino.cpp:636 src/gui/vfd_setup.cpp:189
	enumRow("lcd_info_line")
		.section("display")
		.label("lcd_info_line")
		.hint("menu.hint_vfd_infoline")
		.defaultValue(0)
		.values(kLcdInfoLine)
		.field(COREAPI_NUMBER_FIELD_ON(lcd_info_line, vfdEnabled, NULL)),
	// src/neutrino.cpp:638 src/gui/vfd_setup.cpp:211
	boolRow("lcd_notify_rclock")
		.section("display")
		.label("lcdmenu.notify_rclock")
		.hint("menu.hint_vfd_notify_rclock")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(lcd_notify_rclock, vfdEnabled, NULL)),
	/* How many times a line too long for the panel is scrolled past, and nought
	   is off. The second item the screen offers for it is this number held to
	   its nought and its one, which is a file on the box and not a setting to
	   condition on. src/gui/vfd_setup.cpp:194 */
	// src/neutrino.cpp:651 src/gui/vfd_setup.cpp:197
	intRow("lcd_scroll")
		.section("display")
		.label("lcdmenu.scroll_repeats")
		.hint("menu.hint_vfd_scroll")
		.range(0, 999)
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(lcd_scroll)),
	/* The floor is what off means rather than nought, which is a brightness of
	   none. The ceiling is the panel's and the build decides it. */
	// src/neutrino.cpp:635 src/gui/vfd_setup.cpp:324 src/gui/vfd_setup.cpp:326
	intRow("lcd_dim_brightness")
		.section("display")
		.label("lcdmenu.dim_brightness")
		.hint("menu.hint_vfd_brightnessdim")
#ifdef ENABLE_LCD
		.range(-1, 255)
#else
		.range(-1, 15)
#endif
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(lcd_setting_dim_brightness, canSetPanelBrightness, NULL)),
	/* Minutes kept as text. The screen takes three digits and nothing else,
	   src/gui/vfd_setup.cpp:333, and the driver reads the text with atoi,
	   src/driver/vfd.cpp. A String row carries neither the length nor the
	   digits. */
	// src/neutrino.cpp:634 src/gui/vfd_setup.cpp:335
	textRow("lcd_dim_time")
		.section("display")
		.label("lcdmenu.dim_time")
		.hint("menu.hint_vfd_dimtime")
		.defaultValue("0")
		.field(COREAPI_TEXT_FIELD_ON(lcd_setting_dim_time, canSetPanelBrightness, NULL)),
	// src/neutrino.cpp:644 src/gui/vfd_setup.cpp:359
	enumRow("led_tv_mode")
		.section("display")
		.label("ledcontroler.mode.tv")
		.hint("menu.hint_leds_tv")
		.defaultValue(2)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_tv_mode, hasLedMenu, NULL)),
	// src/neutrino.cpp:645 src/gui/vfd_setup.cpp:363
	enumRow("led_standby_mode")
		.section("display")
		.label("ledcontroler.mode.standby")
		.hint("menu.hint_leds_standby")
		.defaultValue(3)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_standby_mode, hasLedMenu, NULL)),
	// src/neutrino.cpp:646 src/gui/vfd_setup.cpp:367
	enumRow("led_deep_mode")
		.section("display")
		.label("ledcontroler.mode.deepstandby")
		.hint("menu.hint_leds_deepstandby")
		.defaultValue(3)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_deep_mode, hasLedMenu, NULL)),
	// src/neutrino.cpp:647 src/gui/vfd_setup.cpp:371
	enumRow("led_rec_mode")
		.section("display")
		.label("ledcontroler.mode.record")
		.hint("menu.hint_leds_record")
		.defaultValue(1)
		.values(kLedMode)
		.field(COREAPI_NUMBER_FIELD_ON(led_rec_mode, hasLedMenu, NULL)),
	// src/neutrino.cpp:648 src/gui/vfd_setup.cpp:375
	boolRow("led_blink")
		.section("display")
		.label("ledcontroler.blink")
		.hint("menu.hint_leds_blink")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(led_blink, hasLedMenu, NULL)),
	// src/neutrino.cpp:640 src/gui/vfd_setup.cpp:385
	boolRow("backlight_tv")
		.section("display")
		.label("ledcontroler.backlight.tv")
		.hint("menu.hint_leds_tv")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD_ON(backlight_tv, hasBacklight, NULL)),
	// src/neutrino.cpp:641 src/gui/vfd_setup.cpp:389
	boolRow("backlight_standby")
		.section("display")
		.label("ledcontroler.mode.standby")
		.hint("menu.hint_leds_standby")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(backlight_standby, hasBacklight, NULL)),
	// src/neutrino.cpp:642 src/gui/vfd_setup.cpp:393
	boolRow("backlight_deepstandby")
		.section("display")
		.label("ledcontroler.mode.deepstandby")
		.hint("menu.hint_leds_deepstandby")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD_ON(backlight_deepstandby, hasBacklight, NULL)),
#ifdef ENABLE_GRAPHLCD
	/* The default is the box's own display and no constant states it: the
	   loader asks the hardware whether the panel is a graphical one. Nought is
	   what a box without one gets. */
	// src/neutrino.cpp:415 src/gui/glcdsetup.cpp:306
	boolRow("glcd_enable")
		.section("display")
		.label("glcd.enable")
		.defaultValue(0)
		.defaultFrom(glcdEnableDefault)
		.field(COREAPI_NUMBER_FIELD(glcd_enable)),
	/* An index into the driver list the graphlcd configuration file holds, so
	   no constant states the ceiling. Anything outside it the box turns into
	   nought, src/driver/glcd/glcd.cpp:929, which is why a wide one is safe. */
	// src/neutrino.cpp:432 src/gui/glcdsetup.cpp:308
	intRow("glcd_selected_config")
		.section("display")
		.label("glcd.display")
		.range(0, 99)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(glcd_selected_config)),
	// src/neutrino.cpp:416 src/gui/glcdsetup.cpp:315
	textRow("glcd_logodir")
		.section("display")
		.label("glcd.logodir")
		.defaultValue(TARGET_ROOT "/media/sda1/logos")
		.field(COREAPI_TEXT_FIELD(glcd_logodir)),
	// src/neutrino.cpp:418 src/gui/glcdsetup.cpp:413
	intRow("glcd_brightness")
		.section("display")
		.label("glcd.brightness")
		.range(0, 10)
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS)
		.field(COREAPI_NUMBER_FIELD(glcd_brightness)),
	// src/neutrino.cpp:421 src/gui/glcdsetup.cpp:417
	intRow("glcd_brightness_standby")
		.section("display")
		.label("glcd.brightness_standby")
		.range(0, 10)
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS_STANDBY)
		.changeableWhen(kNotRealShutdown)
		.field(COREAPI_NUMBER_FIELD(glcd_brightness_standby)),
	// src/neutrino.cpp:419 src/gui/glcdsetup.cpp:423
	intRow("glcd_brightness_dim")
		.section("display")
		.label("glcd.brightness_dim")
		.range(0, 10)
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS_DIM)
		.field(COREAPI_NUMBER_FIELD(glcd_brightness_dim)),
	/* Seconds kept as text, five digits on the screen and no more,
	   src/gui/glcdsetup.cpp:427. A String row carries neither. */
	// src/neutrino.cpp:420 src/gui/glcdsetup.cpp:428
	textRow("glcd_brightness_dim_time")
		.section("display")
		.label("glcd.brightness_dim_time")
		.defaultValue(GLCD_DEFAULT_BRIGHTNESS_DIM_TIME)
		.field(COREAPI_TEXT_FIELD(glcd_brightness_dim_time)),
	// src/neutrino.cpp:424 src/gui/glcdsetup.cpp:321
	boolRow("glcd_scroll")
		.section("display")
		.label("glcd.scroll")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(glcd_scroll)),
	/* The default is the box model's: src/neutrino.cpp:425 gives the VU Uno 4K
	   SE one and :427 gives four more VU boxes two. Five is what every other
	   box gets. */
	// src/neutrino.cpp:430 src/gui/glcdsetup.cpp:323
	intRow("glcd_scroll_speed")
		.section("display")
		.label("glcd.scroll_speed")
		.range(1, 63)
		.defaultValue(5)
		.defaultFrom(glcdScrollSpeedDefault)
		.field(COREAPI_NUMBER_FIELD(glcd_scroll_speed)),
	// src/neutrino.cpp:422 src/gui/glcdsetup.cpp:325
	boolRow("glcd_mirror_osd")
		.section("display")
		.label("glcd.mirror_osd")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(glcd_mirror_osd)),
	// src/neutrino.cpp:423 src/gui/glcdsetup.cpp:327
	boolRow("glcd_mirror_video")
		.section("display")
		.label("glcd.mirror_video")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(glcd_mirror_video)),
#endif
#ifdef ENABLE_LCD4LINUX
	// src/neutrino.cpp:436 src/gui/lcd4l_setup.cpp:185
	enumRow("lcd4l_support")
		.section("display")
		.label("lcd4l_support")
		.hint("menu.hint_lcd4l_support")
		.defaultValue(0)
		.values(kLcd4lSupport)
		.field(COREAPI_NUMBER_FIELD(lcd4l_support)),
	// src/neutrino.cpp:437 src/gui/lcd4l_setup.cpp:201
	textRow("lcd4l_logodir")
		.section("display")
		.label("lcd4l_logodir")
		.hint("menu.hint_lcd4l_logodir")
		.defaultValue(TARGET_ROOT "/media/sda1/logos")
		.field(COREAPI_TEXT_FIELD(lcd4l_logodir)),
	/* A number and not a choice: the screen names the four panels with literal
	   sizes rather than locales, src/gui/lcd4l_setup.cpp:63, so a choice here
	   would offer words the program does not have. The range is the enum's,
	   src/driver/lcd4l.h:48. */
	// src/neutrino.cpp:438 src/gui/lcd4l_setup.cpp:191
	intRow("lcd4l_display_type")
		.section("display")
		.label("lcd4l_display_type")
		.hint("menu.hint_lcd4l_display_type")
		.range(0, 3)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_display_type)),
	/* A number and not a choice for a different reason: the screen offers two
	   different lists for it and picks by the panel,
	   src/gui/lcd4l_setup.cpp:307-310. The ceiling is the highest either lists
	   and the values between are ones neither offers. */
	// src/neutrino.cpp:439 src/gui/lcd4l_setup.cpp:308
	intRow("lcd4l_skin")
		.section("display")
		.label("lcd4l_skin")
		.hint("menu.hint_lcd4l_skin")
		.range(0, 100)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_skin)),
	// src/neutrino.cpp:440 src/gui/lcd4l_setup.cpp:314
	boolRow("lcd4l_skin_radio")
		.section("display")
		.label("lcd4l_skin_radio")
		.hint("menu.hint_lcd4l_skin_radio")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_skin_radio)),
	/* The ceiling is the panel's and the box asks it at run time,
	   src/driver/lcd4l.cpp:270: seven for the Pearl panel and ten for the three
	   Samsung ones. Ten is the wider and holds every value either offers. */
	// src/neutrino.cpp:441 src/gui/lcd4l_setup.cpp:318
	intRow("lcd4l_brightness")
		.section("display")
		.label("lcd4l_brightness")
		.hint("menu.hint_lcd4l_brightness")
		.range(1, 10)
		.defaultValue(7)
		.field(COREAPI_NUMBER_FIELD(lcd4l_brightness)),
	// src/neutrino.cpp:442 src/gui/lcd4l_setup.cpp:322
	intRow("lcd4l_brightness_standby")
		.section("display")
		.label("lcd4l_brightness_standby")
		.hint("menu.hint_lcd4l_brightness_standby")
		.range(1, 10)
		.defaultValue(3)
		.changeableWhen(kNotRealShutdown)
		.field(COREAPI_NUMBER_FIELD(lcd4l_brightness_standby)),
	// src/neutrino.cpp:443 src/gui/lcd4l_setup.cpp:223
	boolRow("lcd4l_convert")
		.section("display")
		.label("lcd4l_convert")
		.hint("menu.hint_lcd4l_convert")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(lcd4l_convert)),
	/* Read by the display list of the REST interface: LCD4Linux is listed and
	   pictured only while this and lcd4l_support are on. The old box control
	   menu of the web interface read it out of the saved file; that menu is
	   gone. */
	// src/neutrino.cpp:444 src/gui/lcd4l_setup.cpp:227
	boolRow("lcd4l_screenshots")
		.section("display")
		.label("lcd4l_screenshots")
		.hint("menu.hint_lcd4l_screenshots")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(lcd4l_screenshots)),
#endif

#ifdef ENABLE_GRAPHLCD
	/* The name of the panel theme, picked from a file list,
	   src/gui/glcdthemes.cpp:456, and read at start. Its default is a migration
	   name where the box has no settings file yet and empty otherwise, which is
	   what every box that has saved once gets. No item names it.
	   src/neutrino.cpp:412 */
	// src/neutrino.cpp:412
	textRow("glcd_theme_name")
		.section("display")
		.defaultValue("")
		.needsRestart()
		.field(COREAPI_TEXT_FIELD(glcd_theme_name)),
#endif
};

} // anonymous namespace

const Descriptor *settingsTableDisplay(size_t &count)
{
	count = sizeof(kSettings) / sizeof(kSettings[0]);
	return kSettings;
}

} // namespace coreapi
