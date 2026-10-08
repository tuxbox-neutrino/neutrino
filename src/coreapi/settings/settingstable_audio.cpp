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
#include "predicates.h"
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

constexpr EnumValue kAnalogMode[] =
{
	// src/gui/audio_setup.cpp:90
	option(0).label("audiomenu.stereo"),
	option(1).label("audiomenu.monoleft"),
	option(2).label("audiomenu.monoright")
};

#if !(HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE)
constexpr EnumValue kHdmiDD[] =
{
	// src/gui/audio_setup.cpp:121 lib/hardware/coolstream/hd1/libcoolstream/audio_cs.h:41
	option(0).label("options.off"),
	option(1).label("audiomenu.hdmi_dd_auto"),
	option(2).label("audiomenu.hdmi_dd_force")
};
#endif

constexpr EnumValue kAvSync[] =
{
	// src/gui/audio_setup.cpp:113
	option(0).label("options.off"),
	option(1).label("options.on"),
	option(2).label("audiomenu.avsync_am")
};

/* The third choice the screen offers only on one box model is left out, so
   that no frontend offers a value the driver behind it was never given. */
constexpr EnumValue kSrsAlgo[] =
{
	// src/gui/audio_setup.cpp:102
	option(0).label("audio.srs_algo_light"),
	option(1).label("audio.srs_algo_normal")
};

// The screen hands srs_enable in as the active flag of the three below, so a
// nonzero value is what makes them editable.
constexpr Condition kSrsOn[] =
{
	when("srs_enable").isNot(0)
};

constexpr Descriptor kAudio[] =
{
	// src/neutrino.cpp:742 src/gui/audio_setup.cpp:137
	enumRow("audio_AnalogMode")
		.section("audio")
		.label("audiomenu.analog_mode")
		.hint("menu.hint_audio_analog_mode")
		.defaultValue(0)
		.values(kAnalogMode)
		.field(COREAPI_NUMBER_FIELD(audio_AnalogMode)),
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// src/neutrino.cpp:536 src/gui/audio_setup.cpp:145
	boolRow("ac3_pass")
		.section("audio")
		.label("audiomenu.ac3")
		.hint("menu.hint_audio_ac3")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(ac3_pass)),
	// src/neutrino.cpp:537 src/gui/audio_setup.cpp:148
	boolRow("dts_pass")
		.section("audio")
		.label("audiomenu.dts")
		.hint("menu.hint_audio_dts")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(dts_pass)),
#else
	// The screen offers this one only where the box reports an HDMI socket,
	// which is a capability read at run time and not a setting to condition on.
	// src/neutrino.cpp:539 src/gui/audio_setup.cpp:154
	enumRow("hdmi_dd")
		.section("audio")
		.label("audiomenu.hdmi_dd")
		.hint("menu.hint_audio_hdmi_dd")
		.defaultValue(0)
		.values(kHdmiDD)
		.field(COREAPI_NUMBER_FIELD_ON(hdmi_dd, hasHdmi, NULL)),
	// src/neutrino.cpp:540 src/gui/audio_setup.cpp:159
	boolRow("spdif_dd")
		.section("audio")
		.label("audiomenu.spdif_dd")
		.hint("menu.hint_audio_spdif_dd")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(spdif_dd)),
#endif
	// src/neutrino.cpp:743 src/gui/audio_setup.cpp:141
	boolRow("audio_DolbyDigital")
		.section("audio")
		.label("audiomenu.dolbydigital")
		.hint("menu.hint_audio_dd")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(audio_DolbyDigital)),
	// src/neutrino.cpp:544 src/gui/audio_setup.cpp:168
	enumRow("avsync")
		.section("audio")
		.label("audiomenu.avsync")
		.hint("menu.hint_audio_avsync")
		.defaultValue(1)
		.values(kAvSync)
		.field(COREAPI_NUMBER_FIELD(avsync)),
	// src/neutrino.cpp:509 src/gui/audio_setup.cpp:172
	intRow("current_volume_step")
		.section("audio")
		.label("audiomenu.volume_step")
		.hint("menu.hint_audio_volstep")
		.range(1, 25)
		.defaultValue(5)
		.field(COREAPI_NUMBER_FIELD(current_volume_step)),
	/* The floor is the one value that is not a volume: the screen names it
	   separately and it says the box keeps whatever was last set.

	   The one row here a restart applies. The program reads it in the pass that
	   loads its settings and nowhere else, where it seeds the running volume,
	   so a change to it moves nothing until the box loads its settings again.
	   The screen offers it with no notifier for that reason. */
	// src/neutrino.cpp:510 src/gui/audio_setup.cpp:176
	intRow("start_volume")
		.section("audio")
		.label("audiomenu.volume_start")
		.hint("menu.hint_audio_volstart")
		.range(-1, 100)
		.defaultValue(-1)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(start_volume)),
	// src/neutrino.cpp:531 src/gui/audio_setup.cpp:196
	boolRow("srs_enable")
		.section("audio")
		.label("audio.srs_iq")
		.hint("menu.hint_audio_srs")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(srs_enable)),
	// src/neutrino.cpp:532 src/gui/audio_setup.cpp:183
	enumRow("srs_algo")
		.section("audio")
		.label("audio.srs_algo")
		.hint("menu.hint_audio_srs_algo")
		.defaultValue(1)
		.values(kSrsAlgo)
		.changeableWhen(kSrsOn)
		.field(COREAPI_NUMBER_FIELD(srs_algo)),
	// src/neutrino.cpp:534 src/gui/audio_setup.cpp:187
	boolRow("srs_nmgr_enable")
		.section("audio")
		.label("audio.srs_nmgr")
		.hint("menu.hint_audio_srs_nmgr")
		.defaultValue(0)
		.changeableWhen(kSrsOn)
		.field(COREAPI_NUMBER_FIELD(srs_nmgr_enable)),
	// src/neutrino.cpp:533 src/gui/audio_setup.cpp:191
	intRow("srs_ref_volume")
		.section("audio")
		.label("audio.srs_volume")
		.hint("menu.hint_audio_srs_volume")
		.range(1, 100)
		.defaultValue(75)
		.changeableWhen(kSrsOn)
		.field(COREAPI_NUMBER_FIELD(srs_ref_volume)),
	// src/neutrino.cpp:515 src/gui/audio_setup.cpp:204
	intRow("audio_volume_percent_ac3")
		.section("audio")
		.label("audiomenu.volume_adjustment_ac3")
		.hint("menu.hint_audio_adjust_vol_ac3")
		.range(0, 100)
		.defaultValue(100)
		.unit("unit.short.percent")
		.field(COREAPI_NUMBER_FIELD(audio_volume_percent_ac3)),
	// src/neutrino.cpp:516 src/gui/audio_setup.cpp:209
	intRow("audio_volume_percent_pcm")
		.section("audio")
		.label("audiomenu.volume_adjustment_pcm")
		.hint("menu.hint_audio_adjust_vol_pcm")
		.range(0, 100)
		.defaultValue(100)
		.unit("unit.short.percent")
		.field(COREAPI_NUMBER_FIELD(audio_volume_percent_pcm)),

	/* The volume the box is at, which it writes on every change,
	   src/driver/volume.cpp, and reads at the next start where the start volume
	   beside it says to keep it. No item names it. */
	// src/neutrino.cpp:507
	intRow("current_volume")
		.section("audio")
		.range(0, 100)
		.defaultValue(75)
		.field(COREAPI_NUMBER_FIELD(current_volume)),
	/* The analogue output, offered by the audio track chooser rather than by a
	   settings screen, src/gui/audio_select.cpp:137. */
	// src/neutrino.cpp:541 src/gui/audio_select.cpp:137
	boolRow("analog_out")
		.section("audio")
		.label("audiomenu.analog_out")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(analog_out)),
};

} // anonymous namespace

const Descriptor *settingsTableAudio(size_t &count)
{
	count = sizeof(kAudio) / sizeof(kAudio[0]);
	return kAudio;
}

} // namespace coreapi
