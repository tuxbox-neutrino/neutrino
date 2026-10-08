/*
 * displaypicture.cpp - the picture a front display writes to a file
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

#include <config.h>

#include "coreapi/box/displaypicture.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace coreapi
{

namespace
{

/* Written aside and renamed so that the name the server opens after the lock
   is given back only ever holds a whole file. The source is not made whole by
   this: LCD4Linux may rewrite it in place while it is read, which shows as a
   changed mtime or size, and then the copy is made once more. */
Status copyOnce(const char *from, const std::string &to, bool mayRetry, bool &again)
{
	again = false;
	FILE *in = fopen(from, "rb");
	if (in == NULL)
		return Status::NotSupported;
	struct stat before;
	const bool known = ::fstat(fileno(in), &before) == 0;
	const std::string aside = to + ".tmp";
	FILE *out = fopen(aside.c_str(), "wb");
	if (out == NULL)
	{
		fclose(in);
		return Status::Internal;
	}
	char buf[4096];
	size_t n = 0;
	bool whole = true;
	while (whole && (n = fread(buf, 1, sizeof(buf), in)) > 0)
		whole = fwrite(buf, 1, n, out) == n;
	whole = whole && !ferror(in);
	struct stat after;
	again = mayRetry && known && ::fstat(fileno(in), &after) == 0
		&& (after.st_mtime != before.st_mtime || after.st_size != before.st_size);
	fclose(in);
	whole = (fclose(out) == 0) && whole;
	if (again)
	{
		remove(aside.c_str());
		return Status::Ok;
	}
	if (!whole || rename(aside.c_str(), to.c_str()) != 0)
	{
		remove(aside.c_str());
		return Status::Internal;
	}
	return Status::Ok;
}

} // namespace

bool displayPictureLive(const char *path, bool running)
{
	struct stat st;
	return running && ::stat(path, &st) == 0 && ::access(path, R_OK) == 0;
}

Status copyDisplayPicture(const char *from, const std::string &to)
{
	Status s = Status::Internal;
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		bool again = false;
		s = copyOnce(from, to, attempt == 0, again);
		if (!again)
			break;
	}
	return s;
}

} // namespace coreapi
