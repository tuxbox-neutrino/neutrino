/*
 * displaypicture.cpp - the picture a front display writes to a file
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (fixes)
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include <zlib.h>

namespace coreapi
{

namespace
{

// A picture larger than this is no front display's.
const size_t kMaxPicture = 8u * 1024u * 1024u;

// What the image data of a front display's picture can unpack to at most.
const size_t kMaxPixels = 64u * 1024u * 1024u;

uint32_t bigEndian32(const unsigned char *p)
{
	return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}

/* Whether the image data, the IDAT chunks' contents in order, is one whole
   zlib stream: unpacked to its end, where zlib checks the stream's own
   checksum, with nothing after it. Chunks that are each whole can still come
   from two pictures; the stream's checksum is what tells. The unpacked bytes
   are thrown away as they come and bounded. */
bool wholeImageData(const std::vector<std::pair<const unsigned char *, uint32_t> > &idat)
{
	z_stream z;
	memset(&z, 0, sizeof(z));
	if (inflateInit(&z) != Z_OK)
		return false;
	unsigned char out[16384];
	size_t unpacked = 0;
	int r = Z_OK;
	bool ok = true;
	for (size_t i = 0; ok && i < idat.size(); ++i)
	{
		z.next_in = (Bytef *) idat[i].first;
		z.avail_in = idat[i].second;
		// input after the end of the stream is not image data
		if (r == Z_STREAM_END && z.avail_in > 0)
			ok = false;
		while (ok && r != Z_STREAM_END && z.avail_in > 0)
		{
			z.next_out = out;
			z.avail_out = sizeof(out);
			r = inflate(&z, Z_NO_FLUSH);
			unpacked += sizeof(out) - z.avail_out;
			ok = (r == Z_OK || r == Z_STREAM_END) && unpacked <= kMaxPixels;
		}
		if (ok && r == Z_STREAM_END && z.avail_in > 0)
			ok = false;
	}
	// all input given: what zlib still holds back, up to the stream's end
	while (ok && r == Z_OK)
	{
		z.next_out = out;
		z.avail_out = sizeof(out);
		r = inflate(&z, Z_NO_FLUSH);
		unpacked += sizeof(out) - z.avail_out;
		ok = (r == Z_OK || r == Z_STREAM_END) && unpacked <= kMaxPixels;
	}
	inflateEnd(&z);
	return ok && r == Z_STREAM_END;
}

} // namespace

bool wholePng(const std::string &bytes)
{
	static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	if (bytes.size() < 8 || memcmp(bytes.data(), signature, 8) != 0)
		return false;
	const unsigned char *p = (const unsigned char *) bytes.data();
	std::vector<std::pair<const unsigned char *, uint32_t> > idat;
	size_t at = 8;
	while (bytes.size() - at >= 12)
	{
		const uint32_t length = bigEndian32(p + at);
		if (length > bytes.size() - at - 12)
			return false;
		// the checksum covers the chunk's type and its data
		const uLong crc = crc32(crc32(0L, Z_NULL, 0), p + at + 4, length + 4);
		if (crc != bigEndian32(p + at + 8 + length))
			return false;
		if (memcmp(p + at + 4, "IDAT", 4) == 0)
			idat.push_back(std::make_pair(p + at + 8, length));
		const bool last = memcmp(p + at + 4, "IEND", 4) == 0;
		at += 12 + length;
		if (last)
			return at == bytes.size() && !idat.empty() && wholeImageData(idat);
	}
	return false;
}

bool displayPictureLive(const char *path, bool running)
{
	struct stat st;
	return running && ::stat(path, &st) == 0 && ::access(path, R_OK) == 0;
}

Status copyDisplayPicture(const char *from, const std::string &to)
{
	/* LCD4Linux writes its file in place, so a read may meet it half written,
	   paused in the middle, or rewritten while it is read. Its time and size
	   cannot tell all of that, so the copy is read whole and checked as a
	   picture before anything is written. Read twice at most. */
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		// a writer caught in the middle gets a moment to finish
		if (attempt > 0)
			usleep(30 * 1000);
		FILE *in = fopen(from, "rb");
		if (in == NULL)
			return Status::NotSupported;
		std::string bytes;
		char buf[4096];
		size_t n = 0;
		bool read = true;
		while (read && (n = fread(buf, 1, sizeof(buf), in)) > 0)
		{
			read = bytes.size() + n <= kMaxPicture;
			if (read)
				bytes.append(buf, n);
		}
		read = read && !ferror(in);
		fclose(in);
		if (!read)
			return Status::Internal;
		if (!wholePng(bytes))
			continue;

		/* Written aside and renamed, so that the name the server opens after the
		   lock is given back only ever holds a whole picture. */
		const std::string aside = to + ".tmp";
		FILE *out = fopen(aside.c_str(), "wb");
		if (out == NULL)
			return Status::Internal;
		bool written = fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size();
		written = (fclose(out) == 0) && written;
		if (!written || rename(aside.c_str(), to.c_str()) != 0)
		{
			remove(aside.c_str());
			return Status::Internal;
		}
		return Status::Ok;
	}
	/* Still not whole: the last whole copy stays under the name and is the
	   answer, a moment older; without one there is nothing to send. Said in
	   the log, so a writer whose files this never takes shows up there. */
	fprintf(stderr, "coreapi: %s was not a whole picture on two reads\n", from);
	struct stat st;
	return ::stat(to.c_str(), &st) == 0 ? Status::Ok : Status::Internal;
}

} // namespace coreapi
