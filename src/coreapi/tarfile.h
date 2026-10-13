/*
 * tarfile.h - a tar archive, compressed or not, read from its own headers
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_tarfile_h__
#define __coreapi_tarfile_h__

#include <stddef.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace tarfile
{

/* One entry of the archive as its headers state it: every name a tar could
   unpack it as - the header's name field alone, that field behind the ustar
   prefix field, and a GNU long name or a pax path - since which of them a tar
   honours is that tar's business, and old GNU headers keep times where ustar
   keeps the prefix. Every one of them is checked. The one it is unpacked as
   here is the one GNU tar takes: a pax path before a long name before the
   prefix of a POSIX header and its name field before the name field alone.
   The kind, the size, and the mode. */
struct Member
{
	std::vector<std::string> names;
	std::string path;
	char        type;
	unsigned long long size;
	unsigned long long mode;
};

/* An archive read entry by entry from its own headers and not from what a tar
   prints about it, so a name with a newline or a space in the owner's field
   cannot shift what is read. The one reader both the check and the unpacking
   go through: what is unpacked is what was checked, read the same way twice.
   gzread reads an archive that is not compressed as it is. */
class Reader
{
	public:
		enum Got { Entry, End, Broken };

		// Extended headers (GNU long names, pax records) past max_extended are Broken.
		Reader(const std::string &path, size_t max_extended);
		~Reader();

		bool opened() const;

		/* The next entry, its data still to be read or skipped: End at the two
		   zero blocks an archive ends with, Broken for anything that is not an
		   entry this reads the way tar does. */
		Got next(Member &m);

		// Past the data of the entry next() answered.
		bool skip(const Member &m);

		// The data of the entry next() answered, written to fd, and past its padding.
		bool copy(const Member &m, int fd);

		// The extended headers read so far.
		size_t extended() const;

	private:
		class Impl;
		Impl *impl;

		Reader(const Reader &);
		Reader &operator=(const Reader &);
};

/* Every entry of the archive. False, with why, for anything that is not a whole
   archive the reader takes, and for more entries (extended headers counted) or
   more bytes, as the headers state them before any data is read, than the
   limits; what names what the limits are for, as a sentence ends with it ("the
   archive holds more names than <what> come to"). */
bool readMembers(const std::string &path, size_t max_members, unsigned long long max_unpacked,
		 const std::string &what, std::vector<Member> &out, std::string &why);

} // namespace tarfile
} // namespace coreapi

#endif
