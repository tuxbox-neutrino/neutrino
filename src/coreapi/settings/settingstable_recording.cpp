/*
 * settingstable_recording.cpp - recording settings, one row per field
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

#include "coreapi/base/deps.h"

#include <timerdclient/timerdtypes.h>

namespace coreapi
{

namespace
{

/* The recording section. The screen has four submenus and the descriptor
   carries no sub-section, so that structure is not in the table.

   The two data pid rows reach the recorder only through
   CNeutrinoApp::setupRecordingDevice, src/neutrino.cpp:2817, which is a call an
   applier makes rather than a restart.

   The one row of this section declared elsewhere is
   recording_audio_pids_default, in src/coreapi/settings/settingstable.cpp. Three rows
   here are the bits of that mask, and the mask and its bits are one setting
   offered two ways rather than four settings. */

/* The two safety times, in the minutes the screen offers them in and not the
   seconds the daemon keeps them in. The screen divides by sixty on the way in
   and multiplies back on the way out, src/gui/record_setup.cpp:305 and :431, so
   the box cannot express ninety seconds at all. Seconds here would let a
   frontend set a value the television can neither show nor keep, and the next
   visit to that screen would round it away without saying so.

   Both are told at once because the daemon takes them together, and the other
   one comes from the daemon rather than from the settings struct: the members
   named after these are the screen's own buffer and hold whatever was last left
   in them, so folding one of those into the call would overwrite a value nobody
   asked about. */
const int kSecondsPerMinute = 60;

bool askSafetyBefore(long &out)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	out = before / kSecondsPerMinute;
	return true;
}

bool askSafetyAfter(long &out)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	out = after / kSecondsPerMinute;
	return true;
}

bool tellSafetyBefore(long value)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	return recordingSafetySource().write((int) value * kSecondsPerMinute, after) == Status::Ok;
}

bool tellSafetyAfter(long value)
{
	int before = 0, after = 0;
	if (recordingSafetySource().read(before, after) != Status::Ok)
		return false;
	return recordingSafetySource().write(before, (int) value * kSecondsPerMinute) == Status::Ok;
}

// src/gui/record_setup.cpp:162
constexpr EnumValue kEndOfRecording[] =
{
	option(0).label("recordingmenu.end_of_recording_max"),
	option(1).label("recordingmenu.end_of_recording_epg")
};

// src/gui/record_setup.cpp:168 src/gui/followscreenings.h:65
constexpr EnumValue kFollowScreenings[] =
{
	option(0).label("options.off"),
	option(1).label("options.on"),
	option(2).label("options.always")
};

/* The two the dropped item offers, src/gui/record_setup.cpp:156, under the two
   locales the program keeps for them and uses nowhere else,
   src/system/locals.h:2676. The numbers are src/neutrino.h:77. */
constexpr EnumValue kRecordingType[] =
{
	option(0).label("recording_type.off"),
	option(1).label("recording_type.file")
};

constexpr Descriptor kRecording[] =
{
	/* The screen refuses this one and the timeshift directory below it while a
	   recording is running, which is a state of the box and not a setting.
	   src/gui/record_setup.cpp:199 */
	// src/neutrino.cpp:836 src/gui/record_setup.cpp:199
	textRow("network_nfs_recordingdir")
		.section("recording")
		.label("recordingmenu.defdir")
		.hint("menu.hint_record_dir")
		.defaultValue(TARGET_ROOT "/media/sda1/movies")
		.field(COREAPI_TEXT_FIELD(network_nfs_recordingdir)),
	// src/neutrino.cpp:853 src/gui/record_setup.cpp:204
	boolRow("recording_save_in_channeldir")
		.section("recording")
		.label("recordingmenu.save_in_channeldir")
		.hint("menu.hint_record_chandir")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_save_in_channeldir)),
	// Hours. src/neutrino.cpp:843 src/gui/record_setup.cpp:209
	intRow("record_hours")
		.section("recording")
		.label("extra.record_time")
		.hint("menu.hint_record_time")
		.range(1, 24)
		.defaultValue(4)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(record_hours)),
	/* Loaded as a flag and offered as a choice, and the two words beside its
	   values are what it means rather than an on and an off, so a Bool row
	   would carry the value and lose the names. */
	// src/neutrino.cpp:850 src/gui/record_setup.cpp:215
	enumRow("recording_epg_for_end")
		.section("recording")
		.label("recordingmenu.end_of_recording_name")
		.hint("menu.hint_record_end")
		.defaultValue(1)
		.values(kEndOfRecording)
		.field(COREAPI_NUMBER_FIELD(recording_epg_for_end)),
	// src/neutrino.cpp:844 src/gui/record_setup.cpp:220
	boolRow("recording_already_found_check")
		.section("recording")
		.label("recordingmenu.already_found_check")
		.hint("menu.hint_record_already_found_check")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_already_found_check)),
	// src/neutrino.cpp:855 src/gui/record_setup.cpp:224
	// Local resolution: this fork falls back to the warning being on, not the source's off.
	boolRow("recording_slow_warning")
		.section("recording")
		.label("recordingmenu.slow_warn")
		.hint("menu.hint_record_slow_warn")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(recording_slow_warning)),
	/* Local resolution: the source declares recording_fill_warning here (per cent
	   of the disc before a warning). This fork stilled that option in 2021 -
	   load and save are commented out in src/neutrino.cpp and no screen offers
	   it - so the member is carried in settingsundeclared.cpp instead and is a
	   port candidate if the feature returns (WORK-114). */
	// src/neutrino.cpp:856 src/gui/record_setup.cpp:235
	boolRow("recording_startstop_msg")
		.section("recording")
		.label("recording.startstop_msg")
		.hint("menu.hint_record_startstop_msg")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(recording_startstop_msg)),
	// The key is not the field name here. src/neutrino.cpp:852
	// src/gui/record_setup.cpp:241
	textRow("recordingmenu.filename_template")
		.section("recording")
		.label("recordingmenu.filename_template")
		.hint("menu.hint_record_filename_template")
		.defaultValue("%C_%T_%d_%t")
		.field(COREAPI_TEXT_FIELD(recording_filename_template)),
	// src/neutrino.cpp:1043 src/gui/record_setup.cpp:245
	boolRow("auto_cover")
		.section("recording")
		.label("recordingmenu.auto_cover")
		.hint("menu.hint_record_auto_cover")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(auto_cover)),

	/* Megabytes, and the two below follow the settings struct into the
	   condition it puts the fields behind, src/system/settings.h:631. The
	   program loads and saves them under the same condition,
	   src/neutrino.cpp:845. Neither carries a hint: the screen states none. */
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	// src/neutrino.cpp:846 src/gui/record_setup.cpp:252
	intRow("recording_bufsize")
		.section("recording")
		.label("extra.record_bufsize")
		.range(1, 25)
		.defaultValue(4)
		.unit("unit.short.megabyte")
		.field(COREAPI_NUMBER_FIELD(recording_bufsize)),
	// src/neutrino.cpp:847 src/gui/record_setup.cpp:257
	intRow("recording_bufsize_dmx")
		.section("recording")
		.label("extra.record_bufsize_dmx")
		.range(1, 25)
		.defaultValue(2)
		.unit("unit.short.megabyte")
		.field(COREAPI_NUMBER_FIELD(recording_bufsize_dmx)),
#endif

	// timers
	// src/neutrino.cpp:862 src/gui/record_setup.cpp:328
	boolRow("recording_zap_on_announce")
		.section("recording")
		.label("recordingmenu.zap_on_announce")
		.hint("menu.hint_record_zap")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_zap_on_announce)),
	// Minutes. src/neutrino.cpp:704 src/gui/record_setup.cpp:334
	intRow("zapto_pre_time")
		.section("recording")
		.label("miscsettings.zapto_pre_time")
		.hint("menu.hint_record_zap_pre_time")
		.range(0, 10)
		.defaultValue(0)
		.unit("unit.short.minute")
		.field(COREAPI_NUMBER_FIELD(zapto_pre_time)),
	// Default: CFollowScreenings::FOLLOWSCREENINGS_ON, src/gui/followscreenings.h:66
	// src/neutrino.cpp:696 src/gui/record_setup.cpp:343
	enumRow("timer_followscreenings")
		.section("recording")
		.label("timersettings.followscreenings")
		.hint("menu.hint_timer_followscreenings")
		.defaultValue(1)
		.values(kFollowScreenings)
		.field(COREAPI_NUMBER_FIELD(timer_followscreenings)),

	// data pids, and neither key is its field name
	// src/neutrino.cpp:860 src/gui/record_setup.cpp:378
	boolRow("recordingmenu.stream_vtxt_pid")
		.section("recording")
		.label("recordingmenu.vtxt_pid")
		.hint("menu.hint_record_data_vtxt")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(recording_stream_vtxt_pid)),
	// src/neutrino.cpp:859 src/gui/record_setup.cpp:379
	boolRow("recordingmenu.stream_subtitle_pids")
		.section("recording")
		.label("recordingmenu.dvbsub_pids")
		.hint("menu.hint_record_data_dvbsub")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(recording_stream_subtitle_pids)),

	/* Timeshift. An empty directory is a default and means the box puts the
	   timeshift under the recording directory, src/neutrino.cpp:881. */
	// src/neutrino.cpp:873 src/gui/record_setup.cpp:395
	textRow("timeshiftdir")
		.section("recording")
		.label("recordingmenu.tsdir")
		.hint("menu.hint_record_tdir")
		.defaultValue("")
		.field(COREAPI_TEXT_FIELD(timeshiftdir)),
	// src/neutrino.cpp:877 src/gui/record_setup.cpp:401
	boolRow("timeshift_pause")
		.section("recording")
		.label("extra.timeshift_pause")
		.hint("menu.hint_record_timeshift_pause")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(timeshift_pause)),
	// Seconds, and nought is what the screen offers as off.
	// src/neutrino.cpp:874 src/gui/record_setup.cpp:405
	intRow("timeshift_auto")
		.section("recording")
		.label("extra.timeshift_auto")
		.hint("menu.hint_record_timeshift_auto")
		.range(0, 300)
		.defaultValue(0)
		.unit("unit.short.second")
		.field(COREAPI_NUMBER_FIELD(timeshift_auto)),
	// src/neutrino.cpp:875 src/gui/record_setup.cpp:411
	boolRow("timeshift_delete")
		.section("recording")
		.label("extra.timeshift_delete")
		.hint("menu.hint_record_timeshift_delete")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(timeshift_delete)),
	// src/neutrino.cpp:878 src/gui/record_setup.cpp:415
	boolRow("timeshift_temp")
		.section("recording")
		.label("extra.timeshift_temp")
		.hint("menu.hint_record_timeshift_temp")
		.defaultValue(1)
		.field(COREAPI_NUMBER_FIELD(timeshift_temp)),
	// Hours. src/neutrino.cpp:876 src/gui/record_setup.cpp:420
	intRow("timeshift_hours")
		.section("recording")
		.label("extra.record_time_ts")
		.hint("menu.hint_record_time_ts")
		.range(1, 24)
		.defaultValue(4)
		.unit("unit.short.hour")
		.field(COREAPI_NUMBER_FIELD(timeshift_hours)),

	/* Where the movie browser looks, which is not where recordings are
	   written: the browser shows it and does not offer it for editing,
	   src/gui/moviebrowser/mb_menu.cpp:282. */
	// src/neutrino.cpp:834 src/gui/moviebrowser/mb_menu.cpp:282
	textRow("network_nfs_moviedir")
		.section("recording")
		.label("moviebrowser.dir")
		.defaultValue(TARGET_ROOT "/media/sda1/movies")
		.field(COREAPI_TEXT_FIELD(network_nfs_moviedir)),
	/* Whether a direct recording asks where to put itself, and how far it
	   asks: the recorder reads one and two apart, src/driver/record.cpp:2318
	   and :2326, which is the only statement of the range there is. No item
	   names it. */
	// src/neutrino.cpp:849
	intRow("recording_choose_direct_rec_dir")
		.section("recording")
		.range(0, 2)
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_choose_direct_rec_dir)),
	/* Whether the box records at all. Its item is in a dropped block,
	   src/gui/record_setup.cpp:152, which is where the two words below come
	   from and the only statement of them there is; the whole record menu and
	   the channel list read the value, src/gui/channellist.cpp:716. */
	// src/neutrino.cpp:861 src/gui/record_setup.cpp:156
	enumRow("recording_type")
		.section("recording")
		.defaultValue(1)
		.values(kRecordingType)
		.field(COREAPI_NUMBER_FIELD(recording_type)),
	/* Whether a timer that woke the box was a recording one, which the box
	   writes itself as it goes to sleep, src/neutrino.cpp:5116, and reads once
	   as it wakes, :3141, before clearing it. No item names it, and a written
	   value lives until the next start. */
	// src/neutrino.cpp:863
	boolRow("shutdown_timer_record_type")
		.section("recording")
		.defaultValue(0)
		.needsRestart()
		.field(COREAPI_NUMBER_FIELD(shutdown_timer_record_type)),
	/* The two below reach the recorder through the same call as the data pid
	   flags beside them, src/neutrino.cpp:2820, and neither has an item or a
	   name: the record screen has a key for each and the program has no locale
	   for either. */
	// src/neutrino.cpp:857
	boolRow("recording_stopsectionsd")
		.section("recording")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_stopsectionsd)),
	// src/neutrino.cpp:858
	boolRow("recordingmenu.stream_pmt_pid")
		.section("recording")
		.defaultValue(0)
		.field(COREAPI_NUMBER_FIELD(recording_stream_pmt_pid)),
	/* The two that decide when a recording begins and ends. The settings file
	   names neither: the value is the timer daemon's, which keeps it in a file
	   of its own and falls back to nought before and ten minutes after on a box
	   that has none, src/timerd/timermanager.cpp:707. The bounds are the
	   screen's, src/gui/record_setup.cpp:315 and :322. */
	// src/gui/record_setup.cpp:314
	intRow("record_safety_time_before")
		.section("recording")
		.label("timersettings.record_safety_time_before")
		.hint("menu.hint_record_timebefore")
		.range(0, 99)
		.defaultValue(0)
		.unit("unit.short.minute")
		.field(COREAPI_SERVICE_FIELD(record_safety_time_before, askSafetyBefore, tellSafetyBefore)),
	// src/gui/record_setup.cpp:321
	intRow("record_safety_time_after")
		.section("recording")
		.label("timersettings.record_safety_time_after")
		.hint("menu.hint_record_timeafter")
		.range(0, 99)
		.defaultValue(10)
		.unit("unit.short.minute")
		.field(COREAPI_SERVICE_FIELD(record_safety_time_after, askSafetyAfter, tellSafetyAfter)),
	/* The three bits of recording_audio_pids_default, which the screen offers as
	   three questions and folds back into the mask as it leaves,
	   src/gui/record_setup.cpp:354 and :435. Both the bits and the mask are
	   declared, so a caller may write either, and they cannot disagree because
	   there is one field under them.

	   Each row is named after the member the screen binds its question to,
	   because those members are where the screens state what each question is
	   called and they are not where the value is: between two visits to that
	   screen each holds whatever was last left in it.

	   The defaults are the bits of the mask's own default, TIMERD_APIDS_STD
	   together with TIMERD_APIDS_AC3, src/neutrino.cpp:879. */
	boolRow("recording_audio_pids_std")
		.section("recording")
		.label("recordingmenu.apids_std")
		.hint("menu.hint_record_apid_std")
		.defaultValue(1)
		.field(COREAPI_MASK_BIT_FIELD(recording_audio_pids_std, recording_audio_pids_default, TIMERD_APIDS_STD)),
	// src/gui/record_setup.cpp:360
	boolRow("recording_audio_pids_alt")
		.section("recording")
		.label("recordingmenu.apids_alt")
		.hint("menu.hint_record_apid_alt")
		.defaultValue(0)
		.field(COREAPI_MASK_BIT_FIELD(recording_audio_pids_alt, recording_audio_pids_default, TIMERD_APIDS_ALT)),
	// src/gui/record_setup.cpp:361
	boolRow("recording_audio_pids_ac3")
		.section("recording")
		.label("recordingmenu.apids_ac3")
		.hint("menu.hint_record_apid_ac3")
		.defaultValue(1)
		.field(COREAPI_MASK_BIT_FIELD(recording_audio_pids_ac3, recording_audio_pids_default, TIMERD_APIDS_AC3)),
};

} // anonymous namespace

const Descriptor *settingsTableRecording(size_t &count)
{
	count = sizeof(kRecording) / sizeof(kRecording[0]);
	return kRecording;
}

} // namespace coreapi
