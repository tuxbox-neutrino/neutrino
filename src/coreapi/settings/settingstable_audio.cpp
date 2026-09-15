/*
 * settingstable_audio.cpp - audio settings, one row per field
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

/* The audio section. Two of the four pass through settings below are declared
   and the other two are not, and which two is not a choice made here: the
   program gives its settings struct one pair on one hardware and the other pair
   on the other, so a row for the pair this build does not have would not
   compile. */

const EnumValue kAnalogMode[] =
{
	// src/gui/audio_setup.cpp:90
	{ 0, "audiomenu.stereo" },
	{ 1, "audiomenu.monoleft" },
	{ 2, "audiomenu.monoright" }
};

#if !(HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE)
const EnumValue kHdmiDD[] =
{
	// src/gui/audio_setup.cpp:121 lib/hardware/coolstream/hd1/libcoolstream/audio_cs.h:41
	{ 0, "options.off" },
	{ 1, "audiomenu.hdmi_dd_auto" },
	{ 2, "audiomenu.hdmi_dd_force" }
};
#endif

const EnumValue kAvSync[] =
{
	// src/gui/audio_setup.cpp:113
	{ 0, "options.off" },
	{ 1, "options.on" },
	{ 2, "audiomenu.avsync_am" }
};

/* The third choice the screen offers only on one box model is left out, so
   that no frontend offers a value the driver behind it was never given. */
const EnumValue kSrsAlgo[] =
{
	// src/gui/audio_setup.cpp:102
	{ 0, "audio.srs_algo_light" },
	{ 1, "audio.srs_algo_normal" }
};

// The screen hands srs_enable in as the active flag of the three below, so a
// nonzero value is what makes them editable.
const Condition kSrsOn[] =
{
	{ "srs_enable", CompareOp::Ne, 0, NULL, 0 }
};

const Descriptor kAudio[] =
{
	// src/neutrino.cpp:742 src/gui/audio_setup.cpp:137
	{
		"audio_AnalogMode", ValueType::Enum, "audio",
		"audiomenu.analog_mode", "menu.hint_audio_analog_mode",
		0, 0, COREAPI_VALUES(kAnalogMode), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_AnalogMode)
	},
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// src/neutrino.cpp:536 src/gui/audio_setup.cpp:145
	{
		"ac3_pass", ValueType::Bool, "audio",
		"audiomenu.ac3", "menu.hint_audio_ac3",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(ac3_pass)
	},
	// src/neutrino.cpp:537 src/gui/audio_setup.cpp:148
	{
		"dts_pass", ValueType::Bool, "audio",
		"audiomenu.dts", "menu.hint_audio_dts",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(dts_pass)
	},
#else
	// The screen offers this one only where the box reports an HDMI socket,
	// which is a capability read at run time and not a setting to condition on.
	// src/neutrino.cpp:539 src/gui/audio_setup.cpp:154
	{
		"hdmi_dd", ValueType::Enum, "audio",
		"audiomenu.hdmi_dd", "menu.hint_audio_hdmi_dd",
		0, 0, COREAPI_VALUES(kHdmiDD), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(hdmi_dd)
	},
	// src/neutrino.cpp:540 src/gui/audio_setup.cpp:159
	{
		"spdif_dd", ValueType::Bool, "audio",
		"audiomenu.spdif_dd", "menu.hint_audio_spdif_dd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(spdif_dd)
	},
#endif
	// src/neutrino.cpp:743 src/gui/audio_setup.cpp:141
	{
		"audio_DolbyDigital", ValueType::Bool, "audio",
		"audiomenu.dolbydigital", "menu.hint_audio_dd",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_DolbyDigital)
	},
	// src/neutrino.cpp:544 src/gui/audio_setup.cpp:168
	{
		"avsync", ValueType::Enum, "audio",
		"audiomenu.avsync", "menu.hint_audio_avsync",
		0, 0, COREAPI_VALUES(kAvSync), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(avsync)
	},
	// src/neutrino.cpp:509 src/gui/audio_setup.cpp:172
	{
		"current_volume_step", ValueType::Int, "audio",
		"audiomenu.volume_step", "menu.hint_audio_volstep",
		1, 25, NULL, 0, 5, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(current_volume_step)
	},
	/* The floor is the one value that is not a volume: the screen names it
	   separately and it says the box keeps whatever was last set.

	   The one row here a restart applies. The program reads it in the pass that
	   loads its settings and nowhere else, where it seeds the running volume,
	   so a change to it moves nothing until the box loads its settings again.
	   The screen offers it with no notifier for that reason. */
	// src/neutrino.cpp:510 src/gui/audio_setup.cpp:176
	{
		"start_volume", ValueType::Int, "audio",
		"audiomenu.volume_start", "menu.hint_audio_volstart",
		-1, 100, NULL, 0, -1, NULL, true, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(start_volume)
	},
	// src/neutrino.cpp:531 src/gui/audio_setup.cpp:196
	{
		"srs_enable", ValueType::Bool, "audio",
		"audio.srs_iq", "menu.hint_audio_srs",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(srs_enable)
	},
	// src/neutrino.cpp:532 src/gui/audio_setup.cpp:183
	{
		"srs_algo", ValueType::Enum, "audio",
		"audio.srs_algo", "menu.hint_audio_srs_algo",
		0, 0, COREAPI_VALUES(kSrsAlgo), 1, NULL, false, false, COREAPI_CONDITIONS(kSrsOn),
		COREAPI_NUMBER_FIELD(srs_algo)
	},
	// src/neutrino.cpp:534 src/gui/audio_setup.cpp:187
	{
		"srs_nmgr_enable", ValueType::Bool, "audio",
		"audio.srs_nmgr", "menu.hint_audio_srs_nmgr",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(kSrsOn),
		COREAPI_NUMBER_FIELD(srs_nmgr_enable)
	},
	// src/neutrino.cpp:533 src/gui/audio_setup.cpp:191
	{
		"srs_ref_volume", ValueType::Int, "audio",
		"audio.srs_volume", "menu.hint_audio_srs_volume",
		1, 100, NULL, 0, 75, NULL, false, false, COREAPI_CONDITIONS(kSrsOn),
		COREAPI_NUMBER_FIELD(srs_ref_volume)
	},
	// src/neutrino.cpp:515 src/gui/audio_setup.cpp:204
	{
		"audio_volume_percent_ac3", ValueType::Int, "audio",
		"audiomenu.volume_adjustment_ac3", "menu.hint_audio_adjust_vol_ac3",
		0, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_volume_percent_ac3)
	},
	// src/neutrino.cpp:516 src/gui/audio_setup.cpp:209
	{
		"audio_volume_percent_pcm", ValueType::Int, "audio",
		"audiomenu.volume_adjustment_pcm", "menu.hint_audio_adjust_vol_pcm",
		0, 100, NULL, 0, 100, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(audio_volume_percent_pcm)
	},

	/* The volume the box is at, which it writes on every change,
	   src/driver/volume.cpp, and reads at the next start where the start volume
	   beside it says to keep it. No item names it. */
	// src/neutrino.cpp:507
	{
		"current_volume", ValueType::Int, "audio",
		NULL, NULL,
		0, 100, NULL, 0, 75, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(current_volume)
	},
	/* The analogue output, offered by the audio track chooser rather than by a
	   settings screen, src/gui/audio_select.cpp:137. */
	// src/neutrino.cpp:541 src/gui/audio_select.cpp:137
	{
		"analog_out", ValueType::Bool, "audio",
		"audiomenu.analog_out", NULL,
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(analog_out)
	},
};

} // anonymous namespace

const Descriptor *settingsTableAudio(size_t &count)
{
	count = sizeof(kAudio) / sizeof(kAudio[0]);
	return kAudio;
}

} // namespace coreapi
