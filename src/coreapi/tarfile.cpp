/*
 * tarfile.cpp - a tar archive, compressed or not, read from its own headers
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/tarfile.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include <zlib.h>

namespace coreapi
{
namespace tarfile
{

namespace
{

int readFull(gzFile f, char *buf, int n)
{
	int done = 0;
	while (done < n)
	{
		const int got = gzread(f, buf + done, n - done);
		if (got <= 0)
			break;
		done += got;
	}
	return done;
}

std::string field(const char *p, size_t n)
{
	size_t len = 0;
	while (len < n && p[len] != '\0')
		++len;
	return std::string(p, len);
}

// Octal, or the base-256 form GNU tar writes a large number in.
bool number(const char *p, size_t n, unsigned long long &out)
{
	out = 0;
	if ((unsigned char) p[0] & 0x80)
	{
		if ((unsigned char) p[0] != 0x80)
			return false;
		for (size_t i = 1; i < n; ++i)
		{
			if (out >> 56)
				return false;
			out = (out << 8) | (unsigned char) p[i];
		}
		return true;
	}
	size_t i = 0;
	while (i < n && p[i] == ' ')
		++i;
	bool any = false;
	for (; i < n && p[i] >= '0' && p[i] <= '7'; ++i)
	{
		if (out >> 60)
			return false;
		out = out * 8 + (unsigned long long) (p[i] - '0');
		any = true;
	}
	for (; i < n; ++i)
		if (p[i] != ' ' && p[i] != '\0')
			return false;
	return any;
}

bool checksumOk(const char *h)
{
	unsigned long long stated;
	if (!number(h + 148, 8, stated))
		return false;
	unsigned long sum = 0;
	long ssum = 0;
	for (int i = 0; i < 512; ++i)
	{
		const char c = (i >= 148 && i < 156) ? ' ' : h[i];
		sum += (unsigned char) c;
		ssum += (signed char) c;
	}
	return stated == sum || (long long) stated == ssum;
}

bool readData(gzFile f, unsigned long long size, std::string &out)
{
	if (size > 64 * 1024)
		return false;
	const size_t blocks = (size_t) ((size + 511) / 512);
	std::string buf(blocks * 512, '\0');
	if (readFull(f, &buf[0], (int) buf.size()) != (int) buf.size())
		return false;
	out.assign(buf, 0, (size_t) size);
	return true;
}

bool skipData(gzFile f, unsigned long long size)
{
	unsigned long long left = ((size + 511) / 512) * 512;
	char buf[8192];
	while (left > 0)
	{
		const int n = left > sizeof(buf) ? (int) sizeof(buf) : (int) left;
		if (readFull(f, buf, n) != n)
			return false;
		left -= (unsigned long long) n;
	}
	return true;
}

// The path and size records of a pax extended header.
bool paxRecords(const std::string &data, std::string &path, bool &has_path,
		unsigned long long &size, bool &has_size)
{
	size_t at = 0;
	while (at < data.size())
	{
		size_t sp = data.find(' ', at);
		if (sp == std::string::npos)
			return false;
		/* A length longer than what is left is refused while it is read, before
		   it can grow past what the arithmetic below holds: one that wrapped
		   around would point back at a record already read, for ever. */
		unsigned long long len = 0;
		for (size_t i = at; i < sp; ++i)
		{
			if (data[i] < '0' || data[i] > '9')
				return false;
			len = len * 10 + (unsigned long long) (data[i] - '0');
			if (len > data.size() - at)
				return false;
		}
		if (len < (sp - at) + 3 || data[at + len - 1] != '\n')
			return false;
		const std::string record = data.substr(sp + 1, (size_t) (at + len - 1 - (sp + 1)));
		const size_t eq = record.find('=');
		if (eq == std::string::npos)
			return false;
		const std::string key = record.substr(0, eq);
		const std::string value = record.substr(eq + 1);
		// a sparse file lays its data out differently from what the header says
		if (key.compare(0, 11, "GNU.sparse.") == 0)
			return false;
		if (key == "path")
		{
			path = value;
			has_path = true;
		}
		else if (key == "size")
		{
			const char *v = value.c_str();
			char *end = NULL;
			errno = 0;
			size = strtoull(v, &end, 10);
			if (errno != 0 || end == v || *end != '\0')
				return false;
			has_size = true;
		}
		at += (size_t) len;
	}
	return true;
}

bool writeAll(int fd, const char *buf, size_t n)
{
	size_t done = 0;
	while (done < n)
	{
		const ssize_t w = write(fd, buf + done, n - done);
		if (w < 0 && errno == EINTR)
			continue;
		if (w <= 0)
			return false;
		done += (size_t) w;
	}
	return true;
}

} // namespace

class Reader::Impl
{
	public:
		Impl(const std::string &path, size_t max_extended)
			: f(gzopen(path.c_str(), "rb")), has_long(false), has_pax_path(false),
			  pax_size(0), has_pax_size(false), extended_(0), max_extended_(max_extended)
		{
		}

		~Impl()
		{
			if (f != NULL)
				gzclose(f);
		}

		bool opened() const
		{
			return f != NULL;
		}

		// The extended headers read so far.
		size_t extended() const
		{
			return extended_;
		}

		/* The next entry, its data still to be read or skipped: End at the two
		   zero blocks an archive ends with, Broken for anything that is not an
		   entry this reads the way tar does. */
		Got next(Member &m)
		{
			for (;;)
			{
				char h[512];
				if (readFull(f, h, 512) != 512)
					return Broken;
				bool zero = true;
				for (int i = 0; i < 512 && zero; ++i)
					zero = h[i] == '\0';
				if (zero)
				{
					/* The end is two zero blocks. GNU tar reads on past a lone one,
					   and unpacks what follows it, so a lone one is no end here
					   either. */
					char after[512];
					bool second = readFull(f, after, 512) == 512;
					for (int i = 0; i < 512 && second; ++i)
						second = after[i] == '\0';
					return second && !has_long && !has_pax_path && !has_pax_size ? End : Broken;
				}
				if (!checksumOk(h))
					return Broken;
				unsigned long long size;
				if (!number(h + 124, 12, size))
					return Broken;
				const std::string name = field(h, 100);
				const std::string prefixed = h[345] != '\0' ? field(h + 345, 155) + "/" + name : std::string();
				const char type = h[156] == '\0' ? '0' : h[156];

				if (type == 'L' || type == 'x')
				{
					// headers of their own count against the names an archive may hold
					if (++extended_ > max_extended_)
						return Broken;
					// two of a kind before one entry: which a tar takes is that tar's guess
					if ((type == 'L' && has_long) || (type == 'x' && (has_pax_path || has_pax_size)))
						return Broken;
					std::string data;
					if (!readData(f, size, data))
						return Broken;
					if (type == 'L')
					{
						long_name = field(data.data(), data.size());
						has_long = true;
					}
					else if (!paxRecords(data, pax_path, has_pax_path, pax_size, has_pax_size))
						return Broken;
					continue;
				}

				/* tar steps over the data by the pax record's size where there is
				   one, and this by the header's: told apart, the two would see
				   different entries after this one. Every size a box's settings
				   come to fits the header, so a pax size has to say the same. */
				if (has_pax_size && pax_size != size)
					return Broken;
				/* A directory has no data, and tar does not step over any for one,
				   whatever its header says: a size there would hide what follows it
				   from this read and not from tar. */
				if (type == '5' && size != 0)
					return Broken;
				unsigned long long mode;
				if (!number(h + 100, 8, mode))
					return Broken;
				// "ustar" and a NUL: a POSIX header, whose prefix field is part of the name
				const bool posix = std::memcmp(h + 257, "ustar", 6) == 0;
				const std::string path = has_pax_path ? pax_path : has_long ? long_name :
							 (posix && !prefixed.empty()) ? prefixed : name;
				/* Old tars take a file whose name ends in a slash for a directory,
				   so such a name is one only as one. The name it is unpacked as,
				   and not a header name a long name stands in for, which is cut at
				   100 bytes wherever that falls. */
				if (type != '5' && !path.empty() && path[path.size() - 1] == '/')
					return Broken;

				m = Member();
				m.names.push_back(name);
				if (!prefixed.empty())
					m.names.push_back(prefixed);
				if (has_long)
					m.names.push_back(long_name);
				if (has_pax_path)
					m.names.push_back(pax_path);
				m.path = path;
				m.type = type;
				m.size = size;
				m.mode = mode;
				has_long = has_pax_path = has_pax_size = false;
				return Entry;
			}
		}

		// Past the data of the entry next() answered.
		bool skip(const Member &m)
		{
			return skipData(f, m.size);
		}

		// The data of the entry next() answered, written to fd, and past its padding.
		bool copy(const Member &m, int fd)
		{
			char buf[8192];
			unsigned long long left = m.size;
			while (left > 0)
			{
				const int n = left > sizeof(buf) ? (int) sizeof(buf) : (int) left;
				if (readFull(f, buf, n) != n || !writeAll(fd, buf, (size_t) n))
					return false;
				left -= (unsigned long long) n;
			}
			const int pad = (int) (((m.size + 511) / 512) * 512 - m.size);
			return pad == 0 || readFull(f, buf, pad) == pad;
		}

	private:
		Impl(const Impl &);
		Impl &operator=(const Impl &);

		gzFile f;
		std::string long_name;
		bool has_long;
		std::string pax_path;
		bool has_pax_path;
		unsigned long long pax_size;
		bool has_pax_size;
		size_t extended_;
		size_t max_extended_;
};

Reader::Reader(const std::string &path, size_t max_extended) : impl(new Impl(path, max_extended))
{
}

Reader::~Reader()
{
	delete impl;
}

bool Reader::opened() const
{
	return impl->opened();
}

Reader::Got Reader::next(Member &m)
{
	return impl->next(m);
}

bool Reader::skip(const Member &m)
{
	return impl->skip(m);
}

bool Reader::copy(const Member &m, int fd)
{
	return impl->copy(m, fd);
}

size_t Reader::extended() const
{
	return impl->extended();
}

bool readMembers(const std::string &path, size_t max_members, unsigned long long max_unpacked,
		 const std::string &what, std::vector<Member> &out, std::string &why)
{
	why = "the archive cannot be read";
	Reader tar(path, max_members);
	if (!tar.opened())
		return false;
	unsigned long long unpacked = 0;
	for (;;)
	{
		Member m;
		const Reader::Got got = tar.next(m);
		if (got == Reader::End)
			return !out.empty();
		if (got != Reader::Entry)
		{
			if (tar.extended() > max_members)
				why = "the archive holds more names than " + what + " come to";
			return false;
		}
		// told by the header, before anything of the data is unpacked
		if (out.size() + 1 > max_members)
		{
			why = "the archive holds more names than " + what + " come to";
			return false;
		}
		if (m.size > max_unpacked - unpacked)
		{
			why = "the archive unpacks to more than " + what + " come to";
			return false;
		}
		if (!tar.skip(m))
			return false;
		out.push_back(m);
		unpacked += m.size;
	}
}

} // namespace tarfile
} // namespace coreapi
