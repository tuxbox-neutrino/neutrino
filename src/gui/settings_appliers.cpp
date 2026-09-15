/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "settings_appliers.h"

#include <cstring>

#include <neutrino.h>

#include <system/debug.h>
#include <system/setting_helpers.h>

#include <gui/cam_menu.h>
#include <gui/infoicons.h>
#include <gui/keybind_setup.h>
#include <gui/record_setup.h>
#include <gui/update_settings.h>
#include <gui/vfd_setup.h>
#include <gui/videosettings.h>

#include <coreapi/settings/settings.h>

extern CAudioSetupNotifier *audioSetupNotifier;
extern CCAMMenuHandler *g_CamHandler;

/* Asked the way a screen asks it: with the option the setting is written under
   and nothing else. Which key that is comes out of the section's own list, so a
   notifier is only ever asked about a setting it branches on. */
bool CSectionSettingsApplier::apply(const char *key)
{
	if (key == NULL)
		return false;

	for (size_t i = 0; i < count; ++i)
	{
		if (std::strcmp(rows[i].key, key) == 0)
			return applyOption(key, rows[i].option);
	}
	return false;
}

bool CSectionSettingsApplier::numberOf(const char *key, int &out)
{
	coreapi::Result<coreapi::Descriptor> d = coreapi::settings::describe(key);
	if (!d.ok() || d.value().field.read_number == NULL)
		return false;

	out = (int) d.value().field.read_number(g_settings);
	return true;
}

namespace
{
// Each list is the section's notifier read off, branch by branch, under the same
// build conditions it carries. A row of the section that is not here is one the
// notifier does nothing for.

// src/system/setting_helpers.cpp CAudioSetupNotifier::changeNotify
const AppliedSetting kAudioRows[] =
{
	{ "audio_AnalogMode", LOCALE_AUDIOMENU_ANALOG_MODE },
	{ "analog_out", LOCALE_AUDIOMENU_ANALOG_OUT },
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	{ "ac3_pass", LOCALE_AUDIOMENU_AC3 },
	{ "dts_pass", LOCALE_AUDIOMENU_DTS },
#else
	{ "hdmi_dd", LOCALE_AUDIOMENU_HDMI_DD },
	{ "spdif_dd", LOCALE_AUDIOMENU_SPDIF_DD },
#endif
	{ "avsync", LOCALE_AUDIOMENU_AVSYNC },
	{ "srs_algo", LOCALE_AUDIO_SRS_ALGO },
	{ "srs_nmgr_enable", LOCALE_AUDIO_SRS_NMGR },
	{ "srs_ref_volume", LOCALE_AUDIO_SRS_VOLUME },
};

/* src/gui/videosettings.cpp CVideoSettings::changeNotify. The one option the
   screen answers with a question, videomenu.videomode, is left out: nobody is
   standing in front of the box when a setting is written from outside it, so the
   box would keep whichever mode the timeout picked. Three more options of that
   notifier belong to no declared row at all. */
const AppliedSetting kVideoRows[] =
{
	{ "video_Format", LOCALE_VIDEOMENU_VIDEOFORMAT },
	{ "video_43mode", LOCALE_VIDEOMENU_43MODE },
	{ "video_dbdr", LOCALE_VIDEOMENU_DBDR },
	{ "video_Mode", LOCALE_VIDEOMENU_VIDEOMODE },
	/* The screen offers the first of these under either of two labels depending
	   on the revision, src/gui/videosettings.cpp:389, and the notifier's two
	   branches do the same thing, :859 and :863. */
	{ "analog_mode1", LOCALE_VIDEOMENU_ANALOG_MODE },
	{ "analog_mode2", LOCALE_VIDEOMENU_CINCH },
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	{ "zappingmode", LOCALE_VIDEOMENU_ZAPPINGMODE },
	{ "hdmi_colorimetry", LOCALE_VIDEOMENU_HDMI_COLORIMETRY },
#endif
#ifdef BOXMODEL_CST_HD2
	{ "brightness", LOCALE_VIDEOMENU_BRIGHTNESS },
	{ "contrast", LOCALE_VIDEOMENU_CONTRAST },
	{ "saturation", LOCALE_VIDEOMENU_SATURATION },
	{ "enable_sd_osd", LOCALE_VIDEOMENU_SDOSD },
#endif
};

/* The four cache options the misc screen offers are not here, because the
   notifier behind them ends in a request to the section daemon over the shared
   client, and that client answers a request made on this thread by ending the
   process. It does so with an idle loop and before anything of this is on the
   stack, so the fault is not here and the screen's own items reach the same
   call; what a write from outside must not do is stand on the end of it. */
const AppliedSetting kMiscRows[] =
{
	{ "fan_speed", LOCALE_FAN_SPEED },
	{ "cpufreq", LOCALE_CPU_FREQ_NORMAL },
};

// src/gui/cam_menu.cpp CCAMMenuHandler::changeNotify. Its clock, pincode and
// routing options name no declared row.
const AppliedSetting kCamRows[] =
{
	{ "ci_tuner", LOCALE_CI_TUNER },
#if HAVE_LIBSTB_HAL
	{ "ci_check_live", LOCALE_CI_CHECK_LIVE_SLOT },
#endif
#if BOXMODEL_VUPLUS_ALL
	{ "ci_delay", LOCALE_CI_DELAY },
#endif
};

// src/gui/keybind_setup.cpp CKeybindSetup::changeNotify, which is the whole of
// what that screen applies: every other key of the section is read where it is
// used.
const AppliedSetting kKeybindingsRows[] =
{
	{ "repeat_blocker", LOCALE_KEYBINDINGMENU_REPEATBLOCK },
	{ "repeat_genericblocker", LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC },
};

/* src/gui/update_settings.cpp CUpdateSettings::changeNotify, both of whose
   options are declared. */
const AppliedSetting kUpdateRows[] =
{
	{ "softupdate_autocheck", LOCALE_FLASHUPDATE_AUTOCHECK },
#if ENABLE_PKG_MANAGEMENT
	{ "softupdate_autocheck_packages", LOCALE_FLASHUPDATE_AUTOCHECK_PACKAGES },
#endif
};

/* src/gui/vfd_setup.cpp CVfdSetup::changeNotify. Its three brightness options
   are answered out of the screen's own copy of the value rather than out of the
   settings, so a row that named one would be applied with whatever the screen
   last held. Its two scroll options belong to a field no row declares. */
const AppliedSetting kDisplayRows[] =
{
	{ "lcd_dim_brightness", LOCALE_LCDMENU_DIM_BRIGHTNESS },
	{ "led_tv_mode", LOCALE_LEDCONTROLER_MODE_TV },
	/* The notifier's branch takes either of the two locales the two items of
	   this one setting carry, src/gui/vfd_setup.cpp:427, and reads the member
	   rather than what it is handed, so the row's own is the one to name. */
	{ "lcd_scroll", LOCALE_LCDMENU_SCROLL_REPEATS },
#ifndef ENABLE_LCD
	{ "backlight_tv", LOCALE_LEDCONTROLER_BACKLIGHT_TV },
#endif
};

/* The one row of this section anything has to be told about, and the only
   applier here that is not a screen's notifier: the setup screen's own
   changeNotify does nothing but turn menu items of that screen on and off, and
   those items do not exist while nobody is standing in the menu.

   The skin beside it is not listed because the drawing reads it again on every
   pass, which is every five seconds. */
const AppliedSetting kOsdRows[] =
{
	{ "mode_icons", LOCALE_INFOICONS_MODEICON },
};

#define COREAPI_APPLIED(a) (a), (sizeof(a) / sizeof((a)[0]))
} // anonymous namespace

CAudioSettingsApplier::CAudioSettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kAudioRows)) {}

CVideoSettingsApplier::CVideoSettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kVideoRows)) {}

CMiscSettingsApplier::CMiscSettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kMiscRows)) {}

CCamSettingsApplier::CCamSettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kCamRows)) {}

CKeybindingsSettingsApplier::CKeybindingsSettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kKeybindingsRows)) {}

/* No list at all, and not an empty one, because an empty array is not a type.

   The screen's notifier acts on the two safety times and the three recording
   audio pids, and all five are declared, but this notifier is the wrong thing to
   reach for them: each of its branches reads the member the screen fills before
   it opens, and a write through the settings layer does not fill those members.
   The safety times go to the timer daemon and the three questions go to the bits
   of the mask, which is where the values are, and running the notifier
   afterwards would put whatever a screen last left in those members back over
   what was written. The suite refuses a list naming any of the five.

   The registration stands as the seam for a row of this section that does keep
   its value in the member it is named after. */
CRecordingSettingsApplier::CRecordingSettingsApplier()
	: CSectionSettingsApplier(NULL, 0) {}

CUpdateSettingsApplier::CUpdateSettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kUpdateRows)) {}

CDisplaySettingsApplier::CDisplaySettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kDisplayRows)) {}

COsdSettingsApplier::COsdSettingsApplier()
	: CSectionSettingsApplier(COREAPI_APPLIED(kOsdRows)) {}

// The value is not handed over because this notifier does not take one: it
// reads the program's settings, and the write has reached them before this runs.
bool CAudioSettingsApplier::applyOption(const char *, neutrino_locale_t option)
{
	if (audioSetupNotifier == NULL)
		return false;

	audioSetupNotifier->changeNotify(option, NULL);
	return true;
}

bool CVideoSettingsApplier::applyOption(const char *, neutrino_locale_t option)
{
	if (g_videoSettings == NULL)
		return false;

	/* The video system is set without the question the screen asks after it.
	   That question is a message box with no timeout,
	   src/gui/videosettings.cpp:840, and it is the safety net for a person
	   standing in front of the television: it reverts the mode nobody could
	   confirm because nobody could see the picture. Run from here it would stop
	   the box's own loop until somebody pressed a key, and there is nobody. What
	   takes its place is that a mode a frontend offers is one the box said it
	   has. */
	if (ARE_LOCALES_EQUAL(option, LOCALE_VIDEOMENU_VIDEOMODE))
	{
		g_videoSettings->setupVideoSystem(false);
		return true;
	}

	g_videoSettings->changeNotify(option, NULL);
	return true;
}

bool CMiscSettingsApplier::applyOption(const char *key, neutrino_locale_t option)
{
	int value = 0;

	// Two notifiers of their own rather than one screen's, and each reads the
	// value it is handed.
	switch (option)
	{
		case LOCALE_FAN_SPEED:
		{
			if (!numberOf(key, value))
				return false;

			CFanControlNotifier notifier;
			notifier.changeNotify(option, &value);
			return true;
		}
		case LOCALE_CPU_FREQ_NORMAL:
		{
			if (!numberOf(key, value))
				return false;

			CCpuFreqNotifier notifier;
			notifier.changeNotify(option, &value);
			return true;
		}
		default:
			return false;
	}
}

bool CCamSettingsApplier::applyOption(const char *key, neutrino_locale_t option)
{
	if (g_CamHandler == NULL)
		return false;

	// This notifier reads what it is handed for one of its options, so it is
	// handed the value rather than nothing.
	int value = 0;
	if (!numberOf(key, value))
		return false;

	g_CamHandler->changeNotify(option, &value);
	return true;
}

/* The four below are the setup screens' own notifiers. Each reads the program's
   settings rather than the items it would have drawn, so one standing outside
   its menu applies what it is asked to. Built on the first call rather than at
   start-up, because at start-up the remote control and the settings they read
   are not there yet. */
bool CKeybindingsSettingsApplier::applyOption(const char *, neutrino_locale_t option)
{
	static CKeybindSetup notifier;

	notifier.changeNotify(option, NULL);
	return true;
}

bool CRecordingSettingsApplier::applyOption(const char *, neutrino_locale_t option)
{
	static CRecordSetup notifier;

	notifier.changeNotify(option, NULL);
	return true;
}

bool CUpdateSettingsApplier::applyOption(const char *, neutrino_locale_t option)
{
	static CUpdateSettings notifier;

	notifier.changeNotify(option, NULL);
	return true;
}

bool CDisplaySettingsApplier::applyOption(const char *, neutrino_locale_t option)
{
	static CVfdSetup notifier;

	notifier.changeNotify(option, NULL);
	return true;
}

/* Started and stopped here rather than through enableInfoIcons, which is what
   the copied interface calls. That one begins by reading the very setting this
   has just changed and does nothing once it reads nought, so asking it to stop
   the drawing after the setting says the drawing is off stops nothing. The
   screen's own switch avoids that by stopping the drawing before it clears the
   setting, and an applier runs the other way round.

   Neither is given the word to put on the screen: nobody is standing in front of
   the box when a setting is written from outside it. */
bool COsdSettingsApplier::applyOption(const char *key, neutrino_locale_t option)
{
	int value = 0;

	switch (option)
	{
		case LOCALE_INFOICONS_MODEICON:
		{
			if (!numberOf(key, value))
				return false;

			if (value != 0)
				CInfoIcons::getInstance()->StartIcons();
			else
				CInfoIcons::getInstance()->StopIcons();
			return true;
		}
		default:
			return false;
	}
}

// One object and one name for as long as the program runs, because the registry
// keeps both pointers rather than copying them.
static CAudioSettingsApplier audio_settings_applier;
static CVideoSettingsApplier video_settings_applier;
static CMiscSettingsApplier misc_settings_applier;
static CCamSettingsApplier cam_settings_applier;
static CKeybindingsSettingsApplier keybindings_settings_applier;
static CRecordingSettingsApplier recording_settings_applier;
static CUpdateSettingsApplier update_settings_applier;
static CDisplaySettingsApplier display_settings_applier;
static COsdSettingsApplier osd_settings_applier;

namespace
{
struct Registration
{
	const char *section;
	coreapi::SettingsApplier *applier;
};

const Registration kAppliers[] =
{
	{ "audio",       &audio_settings_applier },
	{ "video",       &video_settings_applier },
	{ "misc",        &misc_settings_applier },
	{ "cam",         &cam_settings_applier },
	{ "keybindings", &keybindings_settings_applier },
	{ "recording",   &recording_settings_applier },
	{ "update",      &update_settings_applier },
	{ "display",     &display_settings_applier },
	{ "osd",         &osd_settings_applier },
};
}

void CNeutrinoApp::registerSettingsAppliers()
{
	for (size_t i = 0; i < sizeof(kAppliers) / sizeof(kAppliers[0]); ++i)
	{
		if (coreapi::registerSettingsApplier(kAppliers[i].section, kAppliers[i].applier) != coreapi::Status::Ok)
			dprintf(DEBUG_NORMAL, "[neutrino] the %s settings applier was not registered\n", kAppliers[i].section);
	}
}
