/*
 * settingsundeclared.cpp - settings present in the file but absent from the table
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

/* The settings struct holds members no row declares. They are listed here rather
   than left out silently, because a setting missing for no stated reason reads
   the same as one nobody noticed. The field is written by the same macro a row
   writes, so a name that is not a member of the struct does not compile. */
const UndeclaredSetting kUndeclared[] =
{
	// Nothing in the settings file names them, so no row could pass the case
	// that holds a declared key to the keys the program reads.
	// src/neutrino.cpp:1414
	{ "copied at load out of whichever of the sixteen the resolution and the preset name",
	  COREAPI_NUMBER_FIELD(screen_StartX) },
	{ "copied at load out of whichever of the sixteen the resolution and the preset name",
	  COREAPI_NUMBER_FIELD(screen_StartY) },
	{ "copied at load out of whichever of the sixteen the resolution and the preset name",
	  COREAPI_NUMBER_FIELD(screen_EndX) },
	{ "copied at load out of whichever of the sixteen the resolution and the preset name",
	  COREAPI_NUMBER_FIELD(screen_EndY) },
	{ "the size the box is drawing at, taken from the framebuffer",
	  COREAPI_NUMBER_FIELD(screen_width) },
	{ "the size the box is drawing at, taken from the framebuffer",
	  COREAPI_NUMBER_FIELD(screen_height) },
	// src/gui/channellist.cpp:918
	{ "what the channel list is showing at the moment, which it writes as the keys are pressed",
	  COREAPI_NUMBER_FIELD(channellist_descmode) },
	{ "what the channel list is showing at the moment, which it writes as the keys are pressed",
	  COREAPI_NUMBER_FIELD(channellist_displaymode) },

	// Nothing in the repository reads them, so a row would offer a setting that
	// does nothing, which is worse than no row at all.
	{ "nothing in the tree reads it", COREAPI_NUMBER_FIELD(network_ntpatboot) },
	{ "nothing in the tree reads it", COREAPI_TEXT_FIELD(downloadcache_dir) },
	{ "nothing in the tree reads it", COREAPI_NUMBER_FIELD(osd_colorsettings_advanced_mode) },
	{ "nothing in the tree reads it", COREAPI_NUMBER_FIELD(softupdate_mode) },
	{ "nothing in the tree reads it", COREAPI_NUMBER_FIELD(apply_kernel) },

	// The one of these the shipped web page does offer. It reads and writes the
	// key in the settings file itself, so what no consumer reads is the member,
	// and a row would still offer a setting the box does nothing with.
	{ "the shipped web page writes the key and no consumer reads the member",
	  COREAPI_NUMBER_FIELD(recording_epg_for_filename) },

	// A written value cannot reach anything.
	// src/neutrino.cpp:1331
	{ "only the fallback for the two window sizes beside it, and both of those are saved every time",
	  COREAPI_NUMBER_FIELD(window_size) },
	// src/neutrino.cpp:1552
	{ "the mark the migrations read, which the loader writes over with the running version at every start",
	  COREAPI_TEXT_FIELD(version_pseudo) },

};

} // anonymous namespace

const UndeclaredSetting *settingsUndeclared(size_t &count)
{
	count = sizeof(kUndeclared) / sizeof(kUndeclared[0]);
	return kUndeclared;
}

} // namespace coreapi
