/*
 * library.h - the directories the movie browser looks for films in
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

#ifndef __coreapi_library_h__
#define __coreapi_library_h__

#include <string>
#include <vector>

namespace coreapi
{
namespace library
{

// The movie browser's screen offers this many directories of its own.
const int kMaxDirs = 8;

struct Directory
{
	std::string path;   // as typed, may be empty
	bool        used;
};

struct Dirs
{
	std::string            record;   // the box's record directory
	bool                   record_used;
	std::vector<Directory> own;      // kMaxDirs, in the movie browser's order
};

// Read the way the movie browser reads them, with its defaults for keys it never wrote.
Dirs read();

// The directories the movie browser scans, in its order, each once, without a trailing slash.
std::vector<std::string> roots();

} // namespace library
} // namespace coreapi

#endif
