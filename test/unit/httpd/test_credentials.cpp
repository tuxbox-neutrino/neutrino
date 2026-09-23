/*
 * test_credentials.cpp - tests for the stored credentials
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

#include "support/catch.hpp"
#include "httpd/credentials.h"
#include "httpd/randomsource.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace
{

/* A file of a known size, so that a source which runs out before it has
   answered can be driven. The device the program reads from cannot be made to
   do that on demand, and the rule under test is exactly what happens when it
   does. */
class SizedFile
{
	public:
		explicit SizedFile(size_t bytes) : path_("/tmp/ni-cred-XXXXXX"), ok_(false)
		{
			std::vector<char> name(path_.begin(), path_.end());
			name.push_back('\0');

			const int fd = mkstemp(&name[0]);
			if (fd < 0)
				return;

			path_.assign(&name[0]);

			ok_ = true;
			if (bytes != 0)
			{
				const std::vector<char> filler(bytes, 'A');
				ok_ = (write(fd, &filler[0], bytes) == static_cast<ssize_t>(bytes));
			}
			close(fd);
		}

		~SizedFile()
		{
			if (ok_)
				unlink(path_.c_str());
		}

		bool ok() const { return ok_; }
		const char *path() const { return path_.c_str(); }

	private:
		SizedFile(const SizedFile &);
		SizedFile &operator=(const SizedFile &);

		std::string path_;
		bool        ok_;
};

bool allZero(const unsigned char *p, size_t n)
{
	for (size_t i = 0; i < n; ++i)
	{
		if (p[i] != 0)
			return false;
	}
	return true;
}

// A stored form built by hand out of parts, so that one rule at a time can be
// broken and the rest left as this file writes them.
std::string storedForm(const std::string &scheme, const std::string &count,
	const std::string &salt, const std::string &hash)
{
	return scheme + "$" + count + "$" + salt + "$" + hash;
}

// The salt and hash fields of a form this really produced, so that a case
// breaking one field is otherwise holding a form that verifies.
bool fieldsOf(const std::string &stored, std::string &salt, std::string &hash)
{
	const size_t a = stored.find('$');
	if (a == std::string::npos)
		return false;
	const size_t b = stored.find('$', a + 1);
	if (b == std::string::npos)
		return false;
	const size_t c = stored.find('$', b + 1);
	if (c == std::string::npos)
		return false;

	salt = stored.substr(b + 1, c - b - 1);
	hash = stored.substr(c + 1);
	return true;
}

} // namespace

TEST_CASE("a hashed secret verifies and a wrong one does not", "[cred]")
{
	const std::string h = httpd::hashSecret("ni");
	REQUIRE(httpd::verifySecret("ni", h));
	REQUIRE_FALSE(httpd::verifySecret("nj", h));
	REQUIRE_FALSE(httpd::verifySecret("", h));
	REQUIRE_FALSE(httpd::verifySecret("ni ", h));
}

TEST_CASE("a count past the ceiling derives nothing rather than working for hours", "[cred]")
{
	/* The ceiling in the derivation, a second one behind the ceiling the reader of a
	   stored form has: that one guards a number out of a file, this one a number a
	   caller handed in.

	   Ten million rounds is already more than a box does in a session. Past it nothing
	   is derived at all and what a caller gets back is the empty form, which verifies
	   against nothing; without the ceiling the call returns a perfectly good stored form
	   after a quarter of an hour of a set-top box's processor. The count just inside is
	   asked for as well, or the refusal above would be a refusal of everything. */
	REQUIRE(httpd::hashSecret("ni", 10000001u).empty());
	REQUIRE_FALSE(httpd::hashSecret("ni", 1000u).empty());
}

TEST_CASE("the same secret hashes differently every time", "[cred]")
{
	// A shared salt would let one comparison answer for every account, and on a
	// box with one account it would make the hash a constant.
	REQUIRE(httpd::hashSecret("ni") != httpd::hashSecret("ni"));
}

TEST_CASE("the stored form carries its parameters", "[cred]")
{
	const std::string h = httpd::hashSecret("ni", 12345);
	REQUIRE(h.compare(0, 14, "pbkdf2-sha256$") == 0);
	REQUIRE(h.find("$12345$") != std::string::npos);
	// A check that used the current default instead of the count written down
	// here would derive a different key and refuse this, which is what raising
	// the default would then do to every credential already stored.
	REQUIRE(httpd::verifySecret("ni", h));
	REQUIRE_FALSE(httpd::verifySecret("nj", h));
}

TEST_CASE("a hash written at a lower count than the default still verifies", "[cred]")
{
	// The direction the rule exists for: what is already on the box was written
	// before the default was raised, and it has to go on working after.
	const std::string old_form = httpd::hashSecret("ni", 1000);
	REQUIRE(old_form.find("$1000$") != std::string::npos);
	REQUIRE(httpd::verifySecret("ni", old_form));

	const std::string new_form = httpd::hashSecret("ni", 20000);
	REQUIRE(new_form.find("$20000$") != std::string::npos);
	REQUIRE(httpd::verifySecret("ni", new_form));

	// and the two are not the same derivation, so the count is really read
	std::string old_salt, old_hash, new_salt, new_hash;
	REQUIRE(fieldsOf(old_form, old_salt, old_hash));
	REQUIRE(fieldsOf(new_form, new_salt, new_hash));
	REQUIRE(old_hash != new_hash);
}

TEST_CASE("a count the stored form does not carry cannot be guessed", "[cred]")
{
	/* The same salt and the same password at two counts, so that a check which
	   ignored the count and used a fixed one would have to answer true to at
	   most one of these two and false to the other. */
	const std::string a = httpd::hashSecret("ni", 4321);
	std::string salt, hash;
	REQUIRE(fieldsOf(a, salt, hash));

	REQUIRE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "4321", salt, hash)));
	REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "4322", salt, hash)));
	REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "10000", salt, hash)));
}

TEST_CASE("a stored form that is not one refuses rather than accepts", "[cred]")
{
	REQUIRE_FALSE(httpd::verifySecret("ni", ""));
	REQUIRE_FALSE(httpd::verifySecret("ni", "ni"));
	REQUIRE_FALSE(httpd::verifySecret("ni", "pbkdf2-sha256$"));
	REQUIRE_FALSE(httpd::verifySecret("ni", "pbkdf2-sha256$0$c2FsdA==$aGFzaA=="));
	REQUIRE_FALSE(httpd::verifySecret("", ""));
}

TEST_CASE("each way a stored form can be wrong is refused on its own", "[cred]")
{
	/* The line above breaks several rules at once, so a check that stopped
	   enforcing one of them would still refuse it. These break one each,
	   against a form that otherwise verifies. */
	const std::string good = httpd::hashSecret("ni", 5000);
	std::string salt, hash;
	REQUIRE(fieldsOf(good, salt, hash));
	REQUIRE(httpd::verifySecret("ni", good));

	SECTION("a count of zero")
	{
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "0", salt, hash)));
	}
	SECTION("a count that is not a number")
	{
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "-1", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "+5000", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", " 5000", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000x", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "0x1388", salt, hash)));
	}
	SECTION("a count no box could answer within a request")
	{
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "4294967295", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "99999999999999999999", salt, hash)));
	}
	SECTION("another scheme")
	{
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha512", "5000", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("", "5000", salt, hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("PBKDF2-SHA256", "5000", salt, hash)));
	}
	SECTION("a hash of the wrong length that is otherwise unrelated")
	{
		// the length alone, with nothing right about the bytes
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", salt,
			"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHg==")));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", salt,
			"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8g")));
	}
	SECTION("a salt of no length")
	{
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", "", hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", "QQ==", hash)));
	}
	SECTION("base64 that does not decode")
	{
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", "!!!!", hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", "AAAAA", hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", "====", hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", "A=AA", hash)));
		REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000", salt,
			hash.substr(0, hash.size() - 1))));
	}
	SECTION("a field count that is not four")
	{
		REQUIRE_FALSE(httpd::verifySecret("ni", "pbkdf2-sha256$5000$" + salt));
		REQUIRE_FALSE(httpd::verifySecret("ni", good + "$extra"));
		REQUIRE_FALSE(httpd::verifySecret("ni", "$" + good));
	}
	SECTION("a form truncated at every length it has")
	{
		// none of these may read past what it was handed, which is what the
		// runner's own fatal condition handler would report if one did
		for (size_t i = 0; i < good.size(); ++i)
			REQUIRE_FALSE(httpd::verifySecret("ni", good.substr(0, i)));
	}
}

/* One derivation with everything about it written down, so what this produces is held
   to the algorithm the scheme name states and not only to itself. A round trip through
   its own hasher passes whatever the digest is, whatever order the password and the salt
   are handed over in, and whether or not the count is used at all.

   The vector: pbkdf2 with hmac-sha256, the password "ni", the sixteen bytes zero through
   fifteen as the salt, five thousand iterations, thirty two bytes out. */
static const char kVectorSalt[] = "AAECAwQFBgcICQoLDA0ODw==";
static const char kVectorHash[] = "u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIQQ=";

TEST_CASE("the derivation is the one the scheme name states", "[cred]")
{
	REQUIRE(httpd::verifySecret("ni",
		storedForm("pbkdf2-sha256", "5000", kVectorSalt, kVectorHash)));
	REQUIRE_FALSE(httpd::verifySecret("nj",
		storedForm("pbkdf2-sha256", "5000", kVectorSalt, kVectorHash)));
	REQUIRE_FALSE(httpd::verifySecret("ni",
		storedForm("pbkdf2-sha256", "5001", kVectorSalt, kVectorHash)));
}

TEST_CASE("a stored hash that agrees up to a point is still refused", "[cred]")
{
	/* This is what a comparison that stops early looks like from the outside. Each of
	   these is thirty two bytes, agrees with the right hash for as many leading bytes as
	   it names and differs in the byte after that, so a comparison that reads that many
	   bytes or fewer accepts it and one that reads all of them does not.

	   Without these, shortening the comparison is a false accept for one guess in two
	   hundred and fifty six and every case here stays green, because every wrong guess a
	   case makes derives a key whose first byte is almost certainly wrong anyway. */
	struct Forgery
	{
		int         agrees;
		const char *hash;
	};

	static const Forgery forged[] =
	{
		{  1, "u4e5rMcT3NVl0imKtw/NNdutKitEwHIn12wwXWLU3vs=" },
		{  2, "u3i5rMcT3NVl0imKtw/NNdutKitEwHIn12wwXWLU3vs=" },
		{  4, "u3hGU8cT3NVl0imKtw/NNdutKitEwHIn12wwXWLU3vs=" },
		{  8, "u3hGUzjsIypl0imKtw/NNdutKitEwHIn12wwXWLU3vs=" },
		{ 16, "u3hGUzjsIyqaLdZ1SPAyytutKitEwHIn12wwXWLU3vs=" },
		{ 24, "u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43Y12wwXWLU3vs=" },
		{ 31, "u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIfs=" }
	};

	for (size_t i = 0; i < sizeof(forged) / sizeof(forged[0]); ++i)
	{
		INFO("agreeing for " << forged[i].agrees << " bytes");
		REQUIRE_FALSE(httpd::verifySecret("ni",
			storedForm("pbkdf2-sha256", "5000", kVectorSalt, forged[i].hash)));
	}

	// and the whole of it, so that the refusals above are the comparison and
	// not something else about these forms
	REQUIRE(httpd::verifySecret("ni",
		storedForm("pbkdf2-sha256", "5000", kVectorSalt, kVectorHash)));
}

TEST_CASE("a stored hash that is a piece of the right one is still refused", "[cred]")
{
	/* The two shapes a length check that is not there lets through, against a stored
	   hash that is right as far as it goes. Without the check, a comparison over the
	   shorter of the two lengths accepts the first, and one over a fixed thirty two
	   bytes accepts the second while reading a byte past the end of what it was handed.
	   A wrong length filled with unrelated bytes cannot tell a missing check from a
	   present one: it is refused either way. */

	// the right hash with its last byte cut off
	REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000",
		kVectorSalt, "u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIQ==")));

	// the right hash with one byte added after it
	REQUIRE_FALSE(httpd::verifySecret("ni", storedForm("pbkdf2-sha256", "5000",
		kVectorSalt, "u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIQQA")));

	// and the whole of it, so that the two above are refused for their length
	// and not because the vector itself is wrong
	REQUIRE(httpd::verifySecret("ni",
		storedForm("pbkdf2-sha256", "5000", kVectorSalt, kVectorHash)));
}

TEST_CASE("a salt outside the width this reads is refused for its width", "[cred]")
{
	/* Every one of these is a real derivation: the hash beside each salt is the output
	   for that salt, that password and that count, so each one verifies or is refused
	   for its length alone. Without them the bound is a check nothing can fail on: the
	   section that looks like it covers this refuses its two entries for other reasons,
	   so widening the bound to anything at all leaves the suite green. */
	struct Vector
	{
		size_t      bytes;
		bool        inside;
		const char *salt;
		const char *hash;
	};

	static const Vector vectors[] =
	{
		{  1, false, "AA==",
		   "cWcqrK/Gcs1jvsR3BZ5TVKrSTIc1ENbVjNqjTJMfLL8=" },
		{  7, false, "AAECAwQFBg==",
		   "axMSq3iDH92Dg+f6ztpjeiefjVa7kgf6Bc7ej6dHxtg=" },
		{  8, true,  "AAECAwQFBgc=",
		   "hfM7pZTwo9FPF6auuuuw9ySGt/D1df+SXWYW5qv7h74=" },
		{ 16, true,  "AAECAwQFBgcICQoLDA0ODw==",
		   "u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIQQ=" },
		{ 64, true,
		   "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+Pw==",
		   "Mdk1hIeW/tTCGAyiWl8TdnRewbYXZ7kx702+Sv908Nk=" },
		{ 65, false,
		   "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0A=",
		   "Gq0pztzy0T2yFOk+BoKTcNbM0G93Q+tpOyQ/BHghoJw=" }
	};

	for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i)
	{
		INFO("salt of " << vectors[i].bytes << " bytes");
		const std::string stored =
			storedForm("pbkdf2-sha256", "5000", vectors[i].salt, vectors[i].hash);
		REQUIRE(httpd::verifySecret("ni", stored) == vectors[i].inside);
		// and none of them is the right answer for the wrong password, so an
		// entry inside the bound is not passing for some other reason
		REQUIRE_FALSE(httpd::verifySecret("nj", stored));
	}
}

TEST_CASE("a count that wraps into the range is refused, not read", "[cred]")
{
	/* A count is accumulated a digit at a time into a value that is four bytes wide, so
	   a field of enough digits comes back round and lands on a small number. Each of
	   these states a count that is a multiple of two to the thirty second away from five
	   thousand, so a reader that only looks at the total after the last digit sees five
	   thousand and verifies. The counts written out in full in the section above cannot
	   see this: both wrap to values that are still over the ceiling. */
	const char *const wrapping[] =
	{
		"4294972296",           // two to the thirty second, plus five thousand
		"8589939592",           // twice that, plus five thousand
		"4294967301000",        // a thousand times that, plus five thousand
		"18446744073709556616"  // two to the sixty fourth, plus five thousand
	};

	for (size_t i = 0; i < sizeof(wrapping) / sizeof(wrapping[0]); ++i)
	{
		INFO("count " << wrapping[i]);
		REQUIRE_FALSE(httpd::verifySecret("ni",
			storedForm("pbkdf2-sha256", wrapping[i], kVectorSalt, kVectorHash)));
	}

	// and one that comes back round to nothing, which the other end of the
	// same check answers for
	REQUIRE_FALSE(httpd::verifySecret("ni",
		storedForm("pbkdf2-sha256", "4294967296", kVectorSalt, kVectorHash)));

	// and the count all four of them reduce to is one that really does verify,
	// so the refusals above are the guard and not a hash that never matched
	REQUIRE(httpd::verifySecret("ni",
		storedForm("pbkdf2-sha256", "5000", kVectorSalt, kVectorHash)));
}

TEST_CASE("one credential has one spelling", "[cred]")
{
	/* A base64 group that is padded carries bits that stand for nothing and are dropped
	   on the way out. Written as anything but zero they decode to the same bytes, so
	   without a rule about them sixteen spellings of a padded salt field and four of a
	   padded hash field name the same record. Each of these is the shipped vector with
	   exactly one such character changed, decodes to exactly the same bytes, and is
	   refused. */
	REQUIRE(httpd::verifySecret("ni",
		storedForm("pbkdf2-sha256", "5000", kVectorSalt, kVectorHash)));

	const char *const loose_salt[] =
	{
		"AAECAwQFBgcICQoLDA0ODx==",
		"AAECAwQFBgcICQoLDA0ODy==",
		"AAECAwQFBgcICQoLDA0ODz==",
		"AAECAwQFBgcICQoLDA0OD/=="
	};
	for (size_t i = 0; i < sizeof(loose_salt) / sizeof(loose_salt[0]); ++i)
	{
		INFO("salt spelled " << loose_salt[i]);
		REQUIRE_FALSE(httpd::verifySecret("ni",
			storedForm("pbkdf2-sha256", "5000", loose_salt[i], kVectorHash)));
	}

	const char *const loose_hash[] =
	{
		"u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIQR=",
		"u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIQS=",
		"u3hGUzjsIyqaLdZ1SPAyyiRS1dS7P43YKJPPop0rIQT="
	};
	for (size_t i = 0; i < sizeof(loose_hash) / sizeof(loose_hash[0]); ++i)
	{
		INFO("hash spelled " << loose_hash[i]);
		REQUIRE_FALSE(httpd::verifySecret("ni",
			storedForm("pbkdf2-sha256", "5000", kVectorSalt, loose_hash[i])));
	}

	// what this writes itself is the one spelling, whatever the salt drawn
	for (int i = 0; i < 8; ++i)
	{
		const std::string h = httpd::hashSecret("ni", 1000);
		INFO("round " << i << " wrote " << h);
		REQUIRE(httpd::verifySecret("ni", h));
	}
}

TEST_CASE("a hash this cannot produce is an empty string and not a weak one", "[cred]")
{
	REQUIRE(httpd::hashSecret("ni", 0).empty());
	REQUIRE(httpd::hashSecret("ni", 4294967295u).empty());
	// and what it answers with is a form nothing verifies against
	REQUIRE_FALSE(httpd::verifySecret("ni", httpd::hashSecret("ni", 0)));
}

TEST_CASE("an empty secret is hashed rather than treated as a special case", "[cred]")
{
	// whether an account may have no password is a decision for whatever sets
	// one, and a hasher that refused here would push that decision into a
	// caller that has no way to say so
	const std::string h = httpd::hashSecret("");
	REQUIRE_FALSE(h.empty());
	REQUIRE(httpd::verifySecret("", h));
	REQUIRE_FALSE(httpd::verifySecret("x", h));
}

TEST_CASE("a secret carrying a zero byte is read whole", "[cred]")
{
	// the length and not the terminator decides, so a password with a zero
	// byte in it is not silently the part in front of it
	std::string full("ni");
	full.push_back('\0');
	full += "more";

	const std::string h = httpd::hashSecret(full);
	REQUIRE(httpd::verifySecret(full, h));
	REQUIRE_FALSE(httpd::verifySecret("ni", h));
}

TEST_CASE("a token is hex, long enough and different each time", "[cred]")
{
	const std::string a = httpd::randomToken();
	const std::string b = httpd::randomToken();
	REQUIRE(a.size() == 64);
	REQUIRE(a != b);
	REQUIRE(a.find_first_not_of("0123456789abcdef") == std::string::npos);
	REQUIRE(httpd::tokenLookupPrefix(a) == a.substr(0, 8));
}

TEST_CASE("a token this cannot draw is no token at all", "[cred]")
{
	REQUIRE(httpd::randomToken(0).empty());
	REQUIRE(httpd::randomToken(100000).empty());
	REQUIRE(httpd::randomToken(1).size() == 2);
}

TEST_CASE("a lookup key is only ever built out of something that is a token", "[cred]")
{
	// A key derived from an unchecked string is a key the caller of this
	// server chooses, and answering nothing leaves no record found, which is
	// what an unknown token deserves.
	REQUIRE(httpd::tokenLookupPrefix("").empty());
	REQUIRE(httpd::tokenLookupPrefix("abc").empty());
	REQUIRE(httpd::tokenLookupPrefix("abcdef").empty());
	REQUIRE(httpd::tokenLookupPrefix("abcdefg").empty());
	REQUIRE(httpd::tokenLookupPrefix("ABCDEF01").empty());
	REQUIRE(httpd::tokenLookupPrefix("abcdef0g").empty());
	REQUIRE(httpd::tokenLookupPrefix("../../etc").empty());
	REQUIRE(httpd::tokenLookupPrefix("0123456789abcdef!").empty());

	REQUIRE(httpd::tokenLookupPrefix("0123456789abcdef") == "01234567");
	REQUIRE(httpd::tokenLookupPrefix("deadbeef") == "deadbeef");
}

TEST_CASE("a source that runs out is a failure and not a shorter secret", "[cred]")
{
	/* The buffer is filled with a byte that is not zero before every call that
	   is expected to fail, and read back after it. A check on the return value
	   alone would pass for a helper that answered false and left the caller
	   holding the four bytes the source did manage, which is a shorter salt
	   under another name. */
	unsigned char got[16];

	SECTION("the device answers in full")
	{
		std::memset(got, 0, sizeof(got));
		REQUIRE(httpd::randomBytes(got, sizeof(got)));

		unsigned char again[16];
		REQUIRE(httpd::randomBytes(again, sizeof(again)));
		REQUIRE(std::memcmp(got, again, sizeof(got)) != 0);
	}
	SECTION("a source with fewer bytes than were asked for")
	{
		SizedFile few(4);
		REQUIRE(few.ok());
		std::memset(got, 0xAA, sizeof(got));
		REQUIRE_FALSE(httpd::randomBytesFrom(few.path(), got, sizeof(got)));
		REQUIRE(allZero(got, sizeof(got)));
	}
	SECTION("a source with exactly as many as were asked for")
	{
		SizedFile exact(sizeof(got));
		REQUIRE(exact.ok());
		std::memset(got, 0xAA, sizeof(got));
		REQUIRE(httpd::randomBytesFrom(exact.path(), got, sizeof(got)));
		REQUIRE(got[0] == 'A');
		REQUIRE(got[sizeof(got) - 1] == 'A');
	}
	SECTION("a source with nothing in it")
	{
		SizedFile none(0);
		REQUIRE(none.ok());
		std::memset(got, 0xAA, sizeof(got));
		REQUIRE_FALSE(httpd::randomBytesFrom(none.path(), got, sizeof(got)));
		REQUIRE(allZero(got, sizeof(got)));

		std::memset(got, 0xAA, sizeof(got));
		REQUIRE_FALSE(httpd::randomBytesFrom("/dev/null", got, sizeof(got)));
		REQUIRE(allZero(got, sizeof(got)));
	}
	SECTION("a source that cannot be opened at all")
	{
		/* The path the contract is easiest to get wrong on: nothing was read,
		   so there is nothing to wipe on the way out unless the wipe is written
		   where every failure passes through it. */
		const char *const unopenable[] =
		{
			"/nonexistent/ni-web-random",
			"/nonexistent/deeper/still",
			""
		};

		for (size_t i = 0; i < sizeof(unopenable) / sizeof(unopenable[0]); ++i)
		{
			INFO("path " << unopenable[i]);
			std::memset(got, 0xAA, sizeof(got));
			REQUIRE_FALSE(httpd::randomBytesFrom(unopenable[i], got, sizeof(got)));
			REQUIRE(allZero(got, sizeof(got)));
		}
	}
	SECTION("a source that opens and cannot be read")
	{
		// a directory opens and then fails in the read, which is the other
		// side of the same rule
		std::memset(got, 0xAA, sizeof(got));
		REQUIRE_FALSE(httpd::randomBytesFrom("/tmp", got, sizeof(got)));
		REQUIRE(allZero(got, sizeof(got)));
	}
	SECTION("no path at all")
	{
		std::memset(got, 0xAA, sizeof(got));
		REQUIRE_FALSE(httpd::randomBytesFrom(0, got, sizeof(got)));
		REQUIRE(allZero(got, sizeof(got)));
	}
	SECTION("nothing to fill or nowhere to put it")
	{
		// these two answer before the buffer is touched, so there is nothing to
		// read back: a count of zero says wipe nothing and a null out says
		// there is nowhere to wipe
		REQUIRE_FALSE(httpd::randomBytes(got, 0));
		REQUIRE_FALSE(httpd::randomBytes(0, sizeof(got)));
		REQUIRE_FALSE(httpd::randomBytesFrom("/dev/urandom", 0, sizeof(got)));
	}
}
