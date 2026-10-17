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
   underscores, with a manifest.json the pages read. The shell scripts it ships
   in its scripts/ directory are copied beside the pages' own scripts, where the
   old exec route runs them; scripts names those. Whether it is shown, and where,
   is the pages' business (webui.conf, coreapi::webui). */
struct Extension
{
	std::string dir;
	std::vector<std::string> scripts;
};

// More than an extension comes to; a larger archive is refused.
const size_t kMaxArchiveBytes = 16u * 1024u * 1024u;

/* Every installed extension, by name, with a manifest.json, and the scripts it
   ships. A directory of another name, a link, or one without a manifest is not
   an extension and left out. A box without the directory has none. */
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

   A script it ships (scripts/<name>.sh, a regular file directly in scripts/,
   the name of letters, digits, dots, dashes and underscores) may not take the
   name of one the pages ship themselves, whatever is there, nor of any other
   script the pages have unless it is this extension's own from an earlier
   install, still as that install left it: Conflict ScriptNameTaken naming it,
   and nothing changes.

   It is then unpacked beside the extensions under a name of this install's
   own, files readable by all and directories enterable, its scripts are
   written beside the pages' own under working names, executable, and only then
   does it take the place of an installed one of that name, in one step where
   the filesystem can exchange two names; the scripts take their names, and
   the ones the earlier install shipped and this one does not are taken away
   where they are still that install's. Internal ExtensionRefused where the disc
   refused, the installed one as it was, except for a script that could not
   take its name once the extension was in place, which the detail names.
   Conflict ExtensionRefused while another install or removal is under way. The
   upload goes in every case. */
Result<Extension> install(const std::string &file);

/* Takes an installed extension away, and with it the scripts it shipped, each
   only where the pages' script of that name is still the one it installed and
   is not one the pages ship themselves.
   NotFound NoSuchName for none of that name, InvalidArgument BadName for a name
   no extension carries, Conflict ExtensionRefused while another install or
   removal is under way, Internal ExtensionRefused where the disc refused. */
Result<void> remove(const std::string &dir);

// Whether a name is one an extension can carry.
bool extensionName(const std::string &name);

namespace internal
{
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
