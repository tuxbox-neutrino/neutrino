/*
 * byterange.h - range requests, and the partial answers they get
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

#ifndef __httpd_byterange_h__
#define __httpd_byterange_h__

#include <string>

#include <stdint.h>

namespace httpd
{

/* What a request's Range header asks of a file of a known length. Whole means send
   all of it, which is what a request carrying no header at all asks for and also what
   every header this does not read asks for. Partial means send the stretch first to
   last, both ends counted in and both already inside the file. Unsatisfiable means
   the header was read and names nothing the file holds. */
enum class RangeAnswer
{
	Whole,
	Partial,
	Unsatisfiable
};

/* Reads one Range header against a file of length bytes.

   first and last are written only for Partial and are left alone otherwise, so a caller
   that ignores the answer and reads them anyway reads what it had.

   One range and not several. A header naming more than one stretch is answered Whole,
   which is the whole file and not a refusal: what asks for a range here is a player jumping
   into a recording or a client resuming a download, and each of those asks for one. A
   caller handed the whole file has the bytes it asked for and more; one handed a multipart
   document it did not expect has bytes it cannot find its way through.

   Everything else this cannot read is also Whole, and that is what the protocol asks for:
   a header whose unit is not bytes, one whose numbers are not numbers, one whose ends are
   the wrong way round, and one whose numbers are too large to hold. Unsatisfiable is kept
   for the one case where the header was understood completely and still names nothing: a
   first byte at or past the end, and a suffix of no bytes at all.

   A file of no bytes has no stretch to name, so every range against one is unsatisfiable.

   A suffix longer than the file is the file, which is the one place this stretches what was
   asked for. A last byte past the end is likewise brought back to the end. */
RangeAnswer byteRangeOf(const std::string &header, uint64_t length,
                        uint64_t &first, uint64_t &last);

} // namespace httpd

#endif
