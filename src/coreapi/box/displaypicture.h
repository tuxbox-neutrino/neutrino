/*
 * displaypicture.h - the picture a front display writes to a file
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

#ifndef __coreapi_displaypicture__
#define __coreapi_displaypicture__

#include "coreapi/base/result.h"

#include <string>

namespace coreapi
{

/* Whether the program that writes the file is running and the file is readable.
   Its age says nothing: LCD4Linux writes only when the picture changes, so a
   display that stands still has an old file and is still live. */
bool displayPictureLive(const char *path, bool running);

/* A whole copy of the file at the path given, written aside and renamed. NotSupported
   where there is no such file, Internal where the copy could not be made. */
Status copyDisplayPicture(const char *from, const std::string &to);

} // namespace coreapi

#endif
