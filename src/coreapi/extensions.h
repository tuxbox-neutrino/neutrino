/*
 * extensions.h - the web interface's extensions, installed from an archive
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_extensions_h__
#define __coreapi_extensions_h__

#include "coreapi/base/result.h"

#include <stddef.h>

#include <string>
#include <vector>

namespace coreapi
{
namespace extensions
{

/* An extension of the web interface: a directory of its own under the pages'
   extensions/ directory, named by a word of letters, digits, dashes and
   underscores, with a manifest.json the pages read. The shell scripts its
   manifest names under "scripts" (scripts/<name>.sh in its directory) are the
   ones the box runs for it, from there and nowhere else (runScript); scripts
   names those, without .sh. Whether it is shown, and where, is the pages'
   business (webui.conf, coreapi::webui). */
struct Extension
{
	std::string dir;
	std::vector<std::string> scripts;
};

// More than an extension comes to; a larger archive is refused.
const size_t kMaxArchiveBytes = 16u * 1024u * 1024u;

/* Every installed extension, by name, with a manifest.json, and the scripts its
   manifest names that are there. A directory of another name, a link, or one
   without a manifest is not an extension and left out. A box without the
   directory has none. */
Result<std::vector<Extension> > list();

/* Installs, or replaces, the extension in an archive the caller uploaded to the
   temporary directory, as tar or tar.gz. The archive is never fetched from
   anywhere by the box.

   file is a plain name in the temporary directory, of letters, digits, dots,
   dashes and underscores, ending in .tar.gz, .tgz or .tar: InvalidArgument
   BadName otherwise, NotFound NoSuchName for a name with no file behind it.

   The upload is copied into a directory of this program's own, no more than
   kMaxArchiveBytes of it, and the upload goes. The copy is read from its own
   headers (coreapi::tarfile) and has to hold exactly one top-level directory,
   named as above, with a manifest.json in it; only directories and regular
   files, no name that climbs, is absolute, has a . or an empty part or a
   control character, no more than 2000 names and 64 MiB unpacked:
   InvalidArgument ExtensionRefused naming why otherwise.

   The manifest is a JSON object; what it names under "scripts" is a list of
   names of letters, digits, dots, dashes and underscores, not beginning with a
   dash, each a regular file scripts/<name>.sh in the archive: InvalidArgument
   ExtensionRefused naming why otherwise.

   It is then unpacked beside the extensions under a name of this install's
   own, files readable by all and directories enterable, the scripts the
   manifest names executable and no other file, and only then does it take the
   place of an installed one of that name, in one step where the filesystem can
   exchange two names. Copies an earlier install of it left beside the pages'
   own scripts, as installs did before, go where they are still that install's.
   Internal ExtensionRefused where the disc refused, the installed one as it
   was. Conflict ExtensionRefused while another install or removal is under
   way. The upload goes in every case. */
Result<Extension> install(const std::string &file);

/* Takes an installed extension away, its scripts with it, and copies an
   earlier install of it left beside the pages' own scripts, each only where
   the pages' script of that name is still the one it installed and is not one
   the pages ship themselves.
   NotFound NoSuchName for none of that name, InvalidArgument BadName for a name
   no extension carries, Conflict ExtensionRefused while another install or
   removal is under way, Internal ExtensionRefused where the disc refused. */
Result<void> remove(const std::string &dir);

// Whether a name is one an extension can carry.
bool extensionName(const std::string &name);

// The longest an extension's script runs, the most of its output kept, the
// most arguments it is handed, and the longest one of them.
const int kScriptTimeoutMs = 10000;
const size_t kMaxScriptOutput = 65536;
const size_t kMaxScriptArgs = 32;
const size_t kMaxScriptArgument = 1024;

/* Runs one of an installed extension's scripts and answers what it wrote on
   its standard output: scripts/<name>.sh in the extension's directory, where
   the manifest names it under "scripts", each of args one argument in order,
   started without a shell (coreapi::shell::run), with kScriptTimeoutMs and
   kMaxScriptOutput.

   InvalidArgument BadName for a name no extension or script carries,
   InvalidArgument BadString for an argument longer than kMaxScriptArgument,
   ValueHasZeroByte for one holding a zero byte, LeadingDash for one that would
   be read as an option;
   the caller keeps to kMaxScriptArgs. NotFound
   NoSuchName for no extension of that name, NotFound ScriptMissing for a script
   its manifest does not name or that is not there, executable, inside its
   directory; Busy Timeout for one that outran its time, Internal ExitStatus for
   one that failed, Internal OutputTooLarge for one that wrote more than
   kMaxScriptOutput, and the runner's other codes as it gives them. */
Result<std::string> runScript(const std::string &dir, const std::string &name,
			      const std::vector<std::string> &args);

namespace internal
{
// How long a script may run, kScriptTimeoutMs unless a case wants it shorter.
extern int script_timeout_ms;
// The pages' directory and the temporary directory; a case points them at its own.
extern const char *web_root;
extern const char *tmp_dir;
// What an archive may come to, unpacked.
extern size_t max_members;
extern unsigned long long max_unpacked;
// Holds the turn every install and removal takes, as another one under way would.
void holdTurnForTest(bool hold);
} // namespace internal

} // namespace extensions
} // namespace coreapi

#endif
