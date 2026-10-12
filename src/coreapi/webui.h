/*
 * webui.h - the settings the web interface keeps of its own
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_webui_h__
#define __coreapi_webui_h__

#include "coreapi/base/result.h"

#include <stddef.h>

#include <map>
#include <string>

namespace coreapi
{
namespace webui
{

/* What the web interface keeps beside the box's own settings, in yWeb.conf: the
   start page, the box's name and colour, the devices it wakes over the network,
   which extensions it shows and where, and what an extension stores for itself.
   Key and value as text, the way the file holds them. */
typedef std::map<std::string, std::string> Settings;

/* Long enough for the keys of any extension the installer takes: ext_, a
   directory name of up to 255 bytes, and _enabled. */
const size_t kMaxKeyLength = 300;
const size_t kMaxValueLength = 512;
const size_t kMaxKeys = 256;

/* Held around every read and write of the file, here and by the old
   configuration route, which reads and writes the same file in the same
   program: without it each could read the file before the other saved and
   then write over what the other wrote. */
class FileLock
{
	public:
		FileLock();
		~FileLock();

	private:
		FileLock(const FileLock &);
		FileLock &operator=(const FileLock &);
};

/* Whether a key names something that is not read back: a password, a PIN, a
   token or a key to a service, by the names the old configuration route already
   kept to itself. Such a key is written and never answered. */
bool isSecret(const std::string &key);

/* The file as it stands. A box that never wrote it answers empty. */
Settings read();

/* Sets every key in set to its value in one write of the whole file and answers
   the file as it then reads. An empty value takes the key out, which is how the
   pages have always read a key that is not there.

   InvalidArgument BadName for a key that is not a letter followed by up to
   kMaxKeyLength - 1 letters, digits, underscores, dots and dashes (an
   extension's directory name goes into its keys); BadString for a value with a
   byte below a space, with "{=" or "=}", which the template engine of the old
   pages takes for its own, with < > " ' ` or a backslash, which the old pages
   would carry into their markup and scripts as they are, or with #, which the
   file reads as the start of a comment and would cut the value at;
   ValueTooLong for a value over kMaxValueLength bytes; Conflict TooManySettings when the file would hold more
   than kMaxKeys keys; SettingNotWritten when the file could not be saved
   whole, in which case the one there before is left in place. Nothing is
   written unless every key and value passes. */
Result<Settings> change(const Settings &set);

namespace internal
{
// Where the file is; a case points it at one of its own.
extern const char *config_path;
} // namespace internal

} // namespace webui
} // namespace coreapi

#endif
