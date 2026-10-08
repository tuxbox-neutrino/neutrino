/*
 * bootmode.h - what the boot command line says about picture in picture
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

#ifndef __COREAPI_BOOTMODE_H__
#define __COREAPI_BOOTMODE_H__

#include <string.h>

namespace coreapi
{

/* Whether the way the box was started leaves room for a second picture.

   Split from the read of the command line so that the answer can be had for any
   text. has_modes says the box is one that starts in numbered modes at all: on
   the others the line says nothing about it and the answer is yes. readable
   says the line could be read; a box that cannot read its own line is not
   refused, as before. Mode 12 is the one with room for the picture. */
inline bool bootModeAllowsPip(bool has_modes, bool readable, const char *cmdline)
{
	if (!has_modes || !readable || cmdline == NULL)
		return true;
	return strstr(cmdline, "boxmode=12") != NULL;
}

} // namespace coreapi

#endif
