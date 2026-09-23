/*
 * credentials.cpp - the user and password the server checks against
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

#include "credentials.h"

#include "randomsource.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace httpd
{

namespace
{

const char kScheme[] = "pbkdf2-sha256";

// What the scheme name promises, so a stored hash of another width was written
// by something else and is refused rather than compared against a short read
// of itself.
const size_t kHashBytes = 32;

const size_t kSaltBytes = 16;

/* A salt shorter than this is not doing the job the salt is there for: a one byte
   salt puts every account on the box into one of 256 buckets, which is a table an
   attacker can build once. One longer than this is not something this ever wrote. A
   bound and not the fixed width above, for the reason the iteration count is read
   back rather than assumed. */
const size_t kMinSaltBytes = 8;
const size_t kMaxSaltBytes = 64;

/* The library takes the count as an int, so a value that does not fit would be handed over
   as a negative one and older versions turn that into a loop that does not end.

   The number itself bounds how long one refusal can hold a request open. Ten million costs
   about 1.3 seconds on the machine this is built on, measured; what it costs on the boxes
   it runs on has not been measured and is larger. So it is a bound and not a budget: a
   stored form above it is refused rather than answered slowly. */
const unsigned kMaxIterations = 10000000u;

// A token longer than this is a mistake at the call site rather than a
// stronger token, and drawing it would be a large read for nothing.
const size_t kMaxTokenBytes = 256;

const size_t kLookupPrefixChars = 8;

const char kHexDigits[] = "0123456789abcdef";

const char kBase64Alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void appendHex(std::string &out, const unsigned char *in, size_t n)
{
	for (size_t i = 0; i < n; ++i)
	{
		out += kHexDigits[(in[i] >> 4) & 0x0F];
		out += kHexDigits[in[i] & 0x0F];
	}
}

void appendBase64(std::string &out, const unsigned char *in, size_t n)
{
	for (size_t i = 0; i < n; i += 3)
	{
		const size_t left = n - i;
		const unsigned a = in[i];
		const unsigned b = (left > 1) ? in[i + 1] : 0u;
		const unsigned c = (left > 2) ? in[i + 2] : 0u;
		const unsigned v = (a << 16) | (b << 8) | c;

		out += kBase64Alphabet[(v >> 18) & 0x3F];
		out += kBase64Alphabet[(v >> 12) & 0x3F];
		out += (left > 1) ? kBase64Alphabet[(v >> 6) & 0x3F] : '=';
		out += (left > 2) ? kBase64Alphabet[v & 0x3F] : '=';
	}
}

// The value of one character of the alphabet, or a negative number for
// anything else, padding included: padding is only ever recognised where it is
// allowed to be and is a fault everywhere else.
int base64Value(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return (c - 'a') + 26;
	if (c >= '0' && c <= '9')
		return (c - '0') + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

/* Strict about what it will read, because this decodes a stored credential and a decoder
   that steps over what it does not recognise turns a truncated or hand edited field into a
   shorter secret without saying so. A length that is not a multiple of four, an empty
   field, a character outside the alphabet and padding anywhere but at the end are all
   refused.

   So is a final group whose unused bits are not zero. Those bits are dropped on the way
   out, so such a field decodes to the same bytes as the canonical spelling and would verify
   the same secret. Refusing it is what makes one credential one string: without the rule,
   sixteen spellings of a padded salt field and four of a padded hash field all name the
   same record. */
bool decodeBase64(const std::string &in, std::vector<unsigned char> &out)
{
	out.clear();
	if (in.empty() || (in.size() % 4) != 0)
		return false;

	size_t pad = 0;
	while (pad < 2 && in[in.size() - 1 - pad] == '=')
		++pad;

	out.reserve((in.size() / 4) * 3);

	for (size_t i = 0; i < in.size(); i += 4)
	{
		const bool last = (i + 4 == in.size());
		const size_t here = last ? pad : 0;
		unsigned v = 0;

		for (size_t j = 0; j < 4; ++j)
		{
			const char c = in[i + j];
			if (last && j >= (4 - here))
			{
				if (c != '=')
					return false;
				v <<= 6;
				continue;
			}
			const int d = base64Value(c);
			if (d < 0)
				return false;
			v = (v << 6) | static_cast<unsigned>(d);
		}

		// the bits the padding stands for, which are dropped below and so have
		// to have been written as zero for this to be the one spelling
		if (here == 1 && (v & 0xFFu) != 0)
			return false;
		if (here == 2 && (v & 0xFFFFu) != 0)
			return false;

		out.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
		if (here < 2)
			out.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
		if (here < 1)
			out.push_back(static_cast<unsigned char>(v & 0xFF));
	}
	return true;
}

/* Digits and nothing else. Written out rather than handed to the C library,
   which reads a leading sign and a leading space and would turn "-1" into the
   largest count there is, and which answers a count of zero for text that was
   never a number at all. */
bool readIterations(const std::string &text, unsigned &out)
{
	if (text.empty())
		return false;

	unsigned v = 0;
	for (size_t i = 0; i < text.size(); ++i)
	{
		const char c = text[i];
		if (c < '0' || c > '9')
			return false;
		v = (v * 10u) + static_cast<unsigned>(c - '0');
		// checked inside the loop rather than after it, so the accumulator
		// cannot wrap on a field of many digits and land back inside the range
		if (v > kMaxIterations)
			return false;
	}

	if (v == 0)
		return false;
	out = v;
	return true;
}

// The four fields of a stored form. The separator cannot occur inside any of
// them: two are a fixed word and digits, and the base64 alphabet does not hold
// it, so splitting on it is exact rather than a guess.
bool splitStored(const std::string &stored, std::string field[4])
{
	size_t at = 0;
	for (size_t i = 0; i < 4; ++i)
	{
		const size_t sep = stored.find('$', at);
		if (i < 3)
		{
			if (sep == std::string::npos)
				return false;
			field[i].assign(stored, at, sep - at);
			at = sep + 1;
		}
		else
		{
			// a fifth field means this is not the shape written here
			if (sep != std::string::npos)
				return false;
			field[i].assign(stored, at, stored.size() - at);
		}
	}
	return true;
}

bool derive(const std::string &plain, const unsigned char *salt, size_t salt_len,
	unsigned iterations, unsigned char *out, size_t out_len)
{
	// every length the library takes is an int, so anything that does not fit
	// is refused here rather than handed over as a negative one
	if (plain.size() > static_cast<size_t>(INT_MAX))
		return false;
	if (salt_len == 0 || salt_len > static_cast<size_t>(INT_MAX))
		return false;
	if (iterations == 0 || iterations > kMaxIterations)
		return false;

	return PKCS5_PBKDF2_HMAC(plain.data(), static_cast<int>(plain.size()),
			salt, static_cast<int>(salt_len),
			static_cast<int>(iterations), EVP_sha256(),
			static_cast<int>(out_len), out) == 1;
}

/* The read itself. Separate from the wipe below so that there is one place
   where a failure is turned into an answer, rather than a wipe repeated at
   each of the ways this can fail and forgotten at one of them. */
bool fillFrom(const char *path, unsigned char *out, size_t count)
{
	const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	size_t got = 0;
	bool whole = true;
	while (got < count)
	{
		const ssize_t n = ::read(fd, out + got, count - got);
		if (n < 0)
		{
			// a signal delivered mid read is not the source running out
			if (errno == EINTR)
				continue;
			whole = false;
			break;
		}
		if (n == 0)
		{
			whole = false;
			break;
		}
		got += static_cast<size_t>(n);
	}
	::close(fd);
	return whole;
}

} // namespace

bool randomBytesFrom(const char *path, unsigned char *out, size_t count)
{
	// nothing to wipe and nowhere to wipe it, so these two answer before the
	// buffer is touched at all
	if (out == 0 || count == 0)
		return false;

	if (path == 0 || !fillFrom(path, out, count))
	{
		/* Every failure leaves the caller zeroes and not the part of a secret
		   the source managed to answer with. A source that could not be opened
		   fails here as much as one that ran out halfway, because a caller that
		   ignores the answer cannot tell the two apart. */
		OPENSSL_cleanse(out, count);
		return false;
	}
	return true;
}

bool randomBytes(unsigned char *out, size_t count)
{
	return randomBytesFrom("/dev/urandom", out, count);
}

std::string hashSecret(const std::string &plain, unsigned iterations)
{
	unsigned char salt[kSaltBytes];
	if (!randomBytes(salt, sizeof(salt)))
		return std::string();

	unsigned char key[kHashBytes];
	if (!derive(plain, salt, sizeof(salt), iterations, key, sizeof(key)))
	{
		OPENSSL_cleanse(key, sizeof(key));
		return std::string();
	}

	char count[16];
	std::snprintf(count, sizeof(count), "%u", iterations);

	std::string out;
	out.reserve(sizeof(kScheme) + sizeof(count) + 72);
	out += kScheme;
	out += '$';
	out += count;
	out += '$';
	appendBase64(out, salt, sizeof(salt));
	out += '$';
	appendBase64(out, key, sizeof(key));

	OPENSSL_cleanse(key, sizeof(key));
	return out;
}

bool verifySecret(const std::string &plain, const std::string &stored)
{
	std::string field[4];
	if (!splitStored(stored, field))
		return false;

	if (field[0] != kScheme)
		return false;

	unsigned iterations = 0;
	if (!readIterations(field[1], iterations))
		return false;

	std::vector<unsigned char> salt;
	std::vector<unsigned char> want;
	if (!decodeBase64(field[2], salt) || !decodeBase64(field[3], want))
		return false;
	if (salt.size() < kMinSaltBytes || salt.size() > kMaxSaltBytes)
		return false;
	if (want.size() != kHashBytes)
		return false;

	unsigned char key[kHashBytes];
	const bool derived = derive(plain, &salt[0], salt.size(), iterations, key, sizeof(key));

	/* Over the whole key and through the library's own comparison, which does
	   not stop at the first byte that differs. A plain comparison returns as
	   soon as it can, and how soon that is says how much of a guess was right,
	   which is enough to find a secret one byte at a time. */
	const bool same = derived && CRYPTO_memcmp(key, &want[0], kHashBytes) == 0;

	OPENSSL_cleanse(key, sizeof(key));
	return same;
}

std::string randomToken(size_t bytes)
{
	if (bytes == 0 || bytes > kMaxTokenBytes)
		return std::string();

	std::vector<unsigned char> raw(bytes);
	if (!randomBytes(&raw[0], bytes))
		return std::string();

	std::string out;
	out.reserve(bytes * 2);
	appendHex(out, &raw[0], bytes);

	OPENSSL_cleanse(&raw[0], bytes);
	return out;
}

std::string tokenLookupPrefix(const std::string &token)
{
	if (token.size() < kLookupPrefixChars)
		return std::string();

	for (size_t i = 0; i < token.size(); ++i)
	{
		const char c = token[i];
		const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
		if (!hex)
			return std::string();
	}

	return token.substr(0, kLookupPrefixChars);
}

} // namespace httpd
