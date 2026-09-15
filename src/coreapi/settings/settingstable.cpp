/*
 * settingstable.cpp - the table binding settings names to fields
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

#include <vector>

#include <timerdclient/timerdtypes.h>

namespace coreapi
{

namespace
{

/* The keys, bounds, defaults and labels below are the program's own, each read
   off the line that loads the setting and the screen that offers it. A bound
   that is nearly right is the failure to watch for here: it is invisible to
   every check there is, and the first two rows written from memory rather than
   from the screen both had one.

   What is left here is what belongs to no screen file of its own. */

/* What the tables leave out is not left out silently: every member of the
   settings struct a row could have carried and no row declares is listed with
   its reason in settingsundeclared.cpp, and what says the two together are the
   whole struct is a check that reads the struct rather than a number. */
const Descriptor kSettings[] =
{
	// The name of a locale file, which is why an empty one is a default and
	// means the box picks.
	{
		"language", ValueType::String, "general",
		"languagesetup.osd", NULL,
		0, 0, NULL, 0, 0, "", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(language)
	},
	/* The name of a zone as the box's own list spells it, which is why this is
	   text and not a choice: the screen reads the list out of /etc/timezone.xml
	   at run time, src/gui/osdlang_setup.cpp:162. The running box links its
	   clock to the zone at start, src/neutrino.cpp:3009, and the screen's
	   notifier does it again; a value written here reaches neither. */
	// src/neutrino.cpp:774 src/gui/osdlang_setup.cpp:162
	{
		"timezone", ValueType::String, "general",
		"mainsettings.timezone", "menu.hint_timezone",
		0, 0, NULL, 0, 0, "(GMT+01:00) Amsterdam, Berlin, Bern, Rome, Vienna",
		true, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD(timezone)
	},
	// src/neutrino.cpp:747 src/gui/osdlang_setup.cpp:251
	{
		"auto_lang", ValueType::Bool, "general",
		"audiomenu.auto_lang", "menu.hint_auto_lang",
		0, 1, NULL, 0, 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(auto_lang)
	},
	// src/neutrino.cpp:748 src/gui/osdlang_setup.cpp:268
	{
		"auto_subs", ValueType::Bool, "general",
		"audiomenu.auto_subs", "menu.hint_auto_subs",
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(auto_subs)
	},
	/* The one mask here, of the three bits the screen offers as three
	   questions. Its field is a byte and only those three bits are read, so
	   the ceiling is the three of them together rather than what a byte holds. */
	{
		"recording_audio_pids_default", ValueType::Int, "recording",
		"recordingmenu.apids", NULL,
		0, TIMERD_APIDS_STD | TIMERD_APIDS_ALT | TIMERD_APIDS_AC3, NULL, 0,
		TIMERD_APIDS_STD | TIMERD_APIDS_AC3,
		NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(recording_audio_pids_default)
	},
};

/* The sections live in files of their own, so what the whole table is has to be
   put together somewhere. Once, on the first call and never again: the program
   hands the pointer and the count to the source at startup and keeps them for as
   long as it runs. Built by a function rather than filled in place, because that
   is what makes the first call safe against a second thread arriving during it. */
std::vector<Descriptor> join()
{
	std::vector<Descriptor> v(kSettings, kSettings + sizeof(kSettings) / sizeof(kSettings[0]));

	size_t count = 0;
	const Descriptor *audio = settingsTableAudio(count);
	v.insert(v.end(), audio, audio + count);

	const Descriptor *osd = settingsTableOsd(count);
	v.insert(v.end(), osd, osd + count);

	const Descriptor *misc = settingsTableMisc(count);
	v.insert(v.end(), misc, misc + count);

	const Descriptor *video = settingsTableVideo(count);
	v.insert(v.end(), video, video + count);

	const Descriptor *recording = settingsTableRecording(count);
	v.insert(v.end(), recording, recording + count);

	const Descriptor *channel = settingsTableChannel(count);
	v.insert(v.end(), channel, channel + count);

	const Descriptor *network = settingsTableNetwork(count);
	v.insert(v.end(), network, network + count);

	const Descriptor *weather = settingsTableWeather(count);
	v.insert(v.end(), weather, weather + count);

	const Descriptor *keys = settingsTableKeys(count);
	v.insert(v.end(), keys, keys + count);

	const Descriptor *display = settingsTableDisplay(count);
	v.insert(v.end(), display, display + count);

	const Descriptor *player = settingsTablePlayer(count);
	v.insert(v.end(), player, player + count);

	const Descriptor *parental = settingsTableParental(count);
	v.insert(v.end(), parental, parental + count);

	const Descriptor *cam = settingsTableCam(count);
	v.insert(v.end(), cam, cam + count);

	const Descriptor *hdd = settingsTableHdd(count);
	v.insert(v.end(), hdd, hdd + count);

	const Descriptor *update = settingsTableUpdate(count);
	v.insert(v.end(), update, update + count);

	return v;
}

const std::vector<Descriptor> &all()
{
	static const std::vector<Descriptor> v = join();
	return v;
}

// What a case put in front of the shipped table, as a pair so the two cannot
// disagree.
const Descriptor *g_table = NULL;
size_t g_count = 0;

} // anonymous namespace

const Descriptor *settingsTable()
{
	if (g_table != NULL)
		return g_table;
	return all().empty() ? NULL : &all()[0];
}

size_t settingsTableCount()
{
	if (g_table != NULL)
		return g_count;
	return all().size();
}

void setSettingsTable(const Descriptor *table, size_t count)
{
	g_table = table;
	// A count kept from a table that is no longer installed would outlive the
	// array it counted.
	g_count = (table != NULL) ? count : 0;
}

} // namespace coreapi
