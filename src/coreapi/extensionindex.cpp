/*
 * extensionindex.cpp - the signed index extensions are installed and updated from
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/extensionindex.h"
#include "coreapi/extensions.h"
#include "coreapi/base/errors.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json/json.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

namespace coreapi
{
namespace extensions
{

namespace internal
{
const char *index_key = DATADIR "/neutrino/webui-index.pub";
} // namespace internal

namespace
{

const size_t kSignatureBytes = 64;
const size_t kMaxUrl = 2048;
const size_t kMaxText = 256;
const size_t kMaxVersion = 32;

Failure refused(const std::string &why)
{
	return fail(Status::InvalidArgument, ErrorCode::ExtensionRefused, why);
}

bool endsWith(const std::string &s, const char *tail)
{
	const size_t n = std::strlen(tail);
	return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

bool plainIndexName(const std::string &file)
{
	if (file.empty() || file.size() > 255 || file[0] == '.')
		return false;
	for (size_t i = 0; i < file.size(); ++i)
	{
		const char c = file[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '.' || c == '-' || c == '_'))
			return false;
	}
	return endsWith(file, ".json");
}

/* A regular file read whole, not through a link, up to limit bytes; false
   where it cannot be read, and larger set where it holds more. */
bool readSmall(const std::string &path, size_t limit, std::string &out, bool &larger)
{
	larger = false;
	out.clear();
	const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return false;
	struct stat st;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode))
	{
		close(fd);
		return false;
	}
	char buf[8192];
	for (;;)
	{
		const ssize_t n = read(fd, buf, sizeof(buf));
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
		{
			close(fd);
			return false;
		}
		if (n == 0)
			break;
		out.append(buf, (size_t) n);
		if (out.size() > limit)
		{
			larger = true;
			break;
		}
	}
	close(fd);
	return true;
}

// Standard base64, padded, white space around it ignored.
bool decodeBase64(const std::string &in, std::string &out)
{
	std::string s;
	for (size_t i = 0; i < in.size(); ++i)
		if (!std::isspace((unsigned char) in[i]))
			s += in[i];
	if (s.empty() || s.size() % 4 != 0)
		return false;
	std::vector<unsigned char> buf(s.size() / 4 * 3 + 1);
	const int n = EVP_DecodeBlock(&buf[0], (const unsigned char *) s.data(), (int) s.size());
	if (n < 0)
		return false;
	size_t len = (size_t) n;
	if (s[s.size() - 1] == '=')
		--len;
	if (s[s.size() - 2] == '=')
		--len;
	out.assign((const char *) &buf[0], len);
	return true;
}

// The image's key, an Ed25519 public key as PEM; NULL where there is none of that kind.
EVP_PKEY *imageKey()
{
	FILE *f = std::fopen(internal::index_key, "r");
	if (f == NULL)
		return NULL;
	EVP_PKEY *key = PEM_read_PUBKEY(f, NULL, NULL, NULL);
	std::fclose(f);
	if (key != NULL && EVP_PKEY_id(key) != EVP_PKEY_ED25519)
	{
		EVP_PKEY_free(key);
		key = NULL;
	}
	return key;
}

bool signatureHolds(EVP_PKEY *key, const std::string &data, const std::string &signature)
{
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	const bool holds = ctx != NULL && EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, key) == 1 &&
			   EVP_DigestVerify(ctx, (const unsigned char *) signature.data(), signature.size(),
					    (const unsigned char *) data.data(), data.size()) == 1;
	EVP_MD_CTX_free(ctx);
	return holds;
}

bool plainText(const std::string &s, size_t most)
{
	if (s.size() > most)
		return false;
	for (size_t i = 0; i < s.size(); ++i)
		if ((unsigned char) s[i] < 0x20 || s[i] == 0x7f)
			return false;
	return true;
}

bool versionWord(const std::string &s)
{
	if (s.empty() || s.size() > kMaxVersion)
		return false;
	for (size_t i = 0; i < s.size(); ++i)
	{
		const char c = s[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		      c == '.' || c == '+' || c == '-'))
			return false;
	}
	return true;
}

// An https address the browser can load, as one word.
bool httpsAddress(const std::string &s)
{
	if (s.size() <= 8 || s.size() > kMaxUrl || s.compare(0, 8, "https://") != 0)
		return false;
	for (size_t i = 0; i < s.size(); ++i)
		if ((unsigned char) s[i] <= 0x20 || s[i] == 0x7f)
			return false;
	return true;
}

// 64 hexadecimal digits, answered in lower case.
bool sha256Digits(const std::string &s, std::string &lower)
{
	if (s.size() != 64)
		return false;
	lower.clear();
	for (size_t i = 0; i < s.size(); ++i)
	{
		const char c = (char) std::tolower((unsigned char) s[i]);
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return false;
		lower += c;
	}
	return true;
}

// A member that is text where given; false for one of another kind.
bool optionalText(const Json::Value &o, const char *key, std::string &out)
{
	out.clear();
	if (!o.isMember(key))
		return true;
	if (!o[key].isString())
		return false;
	out = o[key].asString();
	return plainText(out, kMaxText);
}

} // namespace

std::string fileSha256(const std::string &path)
{
	const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return std::string();
	struct stat st;
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && ctx != NULL &&
		  EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1;
	char buf[16384];
	while (ok)
	{
		const ssize_t n = read(fd, buf, sizeof(buf));
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
		{
			ok = n == 0;
			break;
		}
		ok = EVP_DigestUpdate(ctx, buf, (size_t) n) == 1;
	}
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int len = 0;
	ok = ok && EVP_DigestFinal_ex(ctx, digest, &len) == 1 && len == 32;
	EVP_MD_CTX_free(ctx);
	close(fd);
	if (!ok)
		return std::string();
	std::string out;
	for (unsigned int i = 0; i < len; ++i)
	{
		char hex[3];
		std::snprintf(hex, sizeof(hex), "%02x", digest[i]);
		out += hex;
	}
	return out;
}

Result<std::vector<IndexEntry> > readIndex(const std::string &file, const std::string &signature)
{
	if (!plainIndexName(file))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "the index is a plain name in the temporary directory, of letters, digits, dots, "
			    "dashes and underscores, ending in .json");
	const std::string upload = std::string(internal::tmp_dir) + "/" + file;
	struct stat st;
	if (lstat(upload.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is no uploaded index of that name");

	// the upload goes whatever comes of it
	std::string text;
	bool larger = false;
	const bool read_ok = readSmall(upload, kMaxIndexBytes, text, larger);
	unlink(upload.c_str());
	if (!read_ok)
		return refused("the index cannot be read");
	if (larger)
		return refused("the index is larger than 256 KiB");

	std::string raw;
	if (!decodeBase64(signature, raw) || raw.size() != kSignatureBytes)
		return refused("the signature is not 64 bytes in base64");
	EVP_PKEY *key = imageKey();
	if (key == NULL)
		return fail(Status::NotSupported, ErrorCode::ExtensionRefused,
			    "this image carries no Ed25519 key to check an index with");
	const bool holds = signatureHolds(key, text, raw);
	EVP_PKEY_free(key);
	if (!holds)
		return refused("the index does not hold to its signature");

	Json::CharReaderBuilder builder;
	Json::CharReaderBuilder::strictMode(&builder.settings_);
	builder.settings_["stackLimit"] = 64;
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	Json::Value root;
	std::string errors;
	bool parsed = false;
	try
	{
		parsed = reader->parse(text.data(), text.data() + text.size(), &root, &errors);
	}
	catch (const std::exception &)
	{
		parsed = false;
	}
	if (!parsed || !root.isObject())
		return refused("the index is not a JSON object");
	if (!root["format"].isInt() || root["format"].asInt() != 1)
		return refused("the index is not of format 1");
	const Json::Value &list = root["extensions"];
	if (!list.isArray())
		return refused("the index lists its extensions in something other than a list");

	std::vector<IndexEntry> out;
	std::set<std::string> seen;
	for (Json::ArrayIndex i = 0; i < list.size(); ++i)
	{
		const Json::Value &o = list[i];
		char at[32];
		std::snprintf(at, sizeof(at), "entry %u", (unsigned) i + 1);
		if (!o.isObject())
			return refused(std::string(at) + " of the index is not an object");
		IndexEntry e;
		if (!o["id"].isString() || !extensionName(o["id"].asString()))
			return refused(std::string(at) + " of the index has no id an extension can carry");
		e.id = o["id"].asString();
		if (!seen.insert(e.id).second)
			return refused("the index lists " + e.id + " twice");
		if (!o["version"].isString() || !versionWord(o["version"].asString()))
			return refused("the index gives " + e.id + " no version of letters, digits, dots, pluses and dashes");
		e.version = o["version"].asString();
		if (!o["url"].isString() || !httpsAddress(o["url"].asString()))
			return refused("the index gives " + e.id + " no https address");
		e.url = o["url"].asString();
		if (!o["sha256"].isString() || !sha256Digits(o["sha256"].asString(), e.sha256))
			return refused("the index gives " + e.id + " no SHA-256 of 64 hexadecimal digits");
		if (!optionalText(o, "name", e.name) || !optionalText(o, "description", e.description))
			return refused("the index gives " + e.id + " a name or description that is not text of up to 256 bytes");
		out.push_back(e);
	}
	return Result<std::vector<IndexEntry> >::success(out);
}

} // namespace extensions
} // namespace coreapi
