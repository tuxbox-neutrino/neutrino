/*
 * extensionindex.h - the signed index extensions are installed and updated from
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_extensionindex_h__
#define __coreapi_extensionindex_h__

#include "coreapi/base/result.h"

#include <stddef.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace extensions
{

/* One extension an index offers: its directory name, which is its identity
   and the one directory its archive holds; the version and what the pages
   show of it; the https address the browser loads the archive from, and the
   SHA-256 of that archive, which is what the box holds the archive to. */
struct IndexEntry
{
	std::string id;
	std::string name;
	std::string version;
	std::string description;
	std::string url;
	std::string sha256;
};

// More than an index comes to; a larger one is refused.
const size_t kMaxIndexBytes = 256u * 1024u;

/* Reads an index the caller uploaded to the temporary directory, after
   checking it against its signature: the Ed25519 signature, base64, over the
   file's bytes as they are, with the public key the image carries
   (internal::index_key, PEM). The box fetches neither.

   file is a plain name in the temporary directory, of letters, digits, dots,
   dashes and underscores, ending in .json: InvalidArgument BadName otherwise,
   NotFound NoSuchName for a name with no file behind it. The upload goes in
   every case.

   The index is a JSON object with "format": 1 and "extensions", a list of
   objects each with "id" (a name an extension carries, once in the list),
   "version" (up to 32 letters, digits, dots, pluses and dashes), "url" (an
   https address of up to 2048 bytes without a space or a control byte) and
   "sha256" (64 hexadecimal digits), and "name" and "description" as text of
   up to 256 bytes where given.

   NotSupported ExtensionRefused where the image carries no key, or none of
   that kind; InvalidArgument ExtensionRefused naming why for a signature that
   does not hold, an index larger than kMaxIndexBytes or one not of that
   shape. */
Result<std::vector<IndexEntry> > readIndex(const std::string &file, const std::string &signature);

/* The SHA-256 of a regular file, in lower case hexadecimal; empty where it
   cannot be read. */
std::string fileSha256(const std::string &path);

namespace internal
{
// Where the image keeps the public key indexes are checked with; a case points it at its own.
extern const char *index_key;
} // namespace internal

} // namespace extensions
} // namespace coreapi

#endif
