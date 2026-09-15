/*
 * static.h - serving files out of the web directory
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

#ifndef __httpd_static_h__
#define __httpd_static_h__

#include "endpoint.h"

#include <string>

namespace httpd
{

/* Where the pages are read from, and empty means none are read at all.

   Written once before the daemon exists and read by the threads it starts, which is
   the same arrangement the body ceiling has: the server installs what its
   configuration says while it is starting, so the file and the directory this serves
   from cannot drift apart.

   Empty is what a box that was never configured runs on, and it answers every request
   the way a box with an empty directory would: a docroot that failed to arrive serves
   nothing rather than serving whatever the process happens to be able to read. */
void setDocroot(const std::string &dir);
std::string docroot();

/* One file out of that directory, as the thing that goes back to the caller.

   url_path is the request target with its query already taken off and nothing
   decoded. accept_encoding and if_none_match are what the request carried under
   those names, joined with commas when it carried several.

   404 for a path this directory does not hold, 400 for one that leaves it, both as
   the document every other refusal in this server is written as, with a directive
   saying how long the answer may be kept. The 404 names no path and quotes nothing
   back: the directory is writable on a box, so what is in it is worth no more to a
   caller than what is not.

   Those two are not one answer and that is decided rather than left over. A path this
   server does not route and a path this directory does not hold are one answer,
   because telling those apart hands a caller a way to walk a directory nobody may
   list. A name that resolves out of the directory is the third and is answered as
   itself: what reaches it is a link somebody planted, far more often whoever
   installed the pages than an attacker.

   A target written with a separator after it names a directory, and no directory is a
   file, so it is 404 whatever it names.

   What the confinement covers and what it cannot. A target is refused before the
   filesystem is touched when it carries a zero byte, a dot segment or a doubled
   separator, and after it is touched when the path it resolves to is not the resolved
   directory or something under it. Resolution follows a symbolic link, so a link
   planted in the directory pointing outside it is refused however many hops it takes,
   and the file that is finally read is held to the device and inode resolution saw.

   A hard link is the one planted link this cannot see: it is genuinely a name inside
   the directory and there is nothing in the filesystem to ask that would say
   otherwise. So whoever may write into this directory can publish any file on the
   same filesystem that they can already read.

   Nothing here answers 406. A caller that takes no compressed form is given the
   identity one, because plain curl is the documented way to reach a page from a
   plugin and a plugin sends no Accept-Encoding at all. */
Response serveStatic(const std::string &url_path, const std::string &accept_encoding,
                     const std::string &if_none_match);

/* The same answer out of a directory named here rather than out of the one above.
   Everything the confinement above is written about holds word for word, the
   resolution and the refusals already carrying the root as a parameter.

   This is not a second docroot and nothing may make it one. It exists for the one
   name the server answers out of the directory a box may write into, which the server
   states as a whole path and not as a prefix, and a caller that could name the
   directory would have a way out of the first root that nobody watching the
   configuration would see. */
Response serveStaticFrom(const std::string &root_dir, const std::string &url_path,
                         const std::string &accept_encoding,
                         const std::string &if_none_match);

/* The directory a box may write into, as the server answers the one name out of it.
   It is the directory the build was configured with and nothing on the box moves it;
   the setter is reachable so a case can drive the one name against a directory it
   made itself, and nothing in the product calls it. */
std::string publicRoot();
void setPublicRootForTest(const std::string &dir);

/* Run between the moment a name has been resolved and checked and the moment the file
   behind it is opened, and installed by nothing in the product.

   That gap is the whole of what the identity check exists for. The directory is
   writable on a box, so between those two moments anything in the path can be
   renamed, and the open by name would then open a file the check never saw. A last
   component that became a link is refused by the open itself; a directory on the way
   that became a link is not, and what refuses that one is the device and inode the
   resolution saw, which is invisible to anything driving this from outside.

   So a case installs a probe here and does what a racer would have to win a race to
   do. Deterministic where a race is not: the reviewer who found this won it 4945
   times in 400000 attempts, and a case at that rate is one that passes when it
   fails. */
typedef void (*ResolvedProbe)(void);
void setResolvedProbeForTest(ResolvedProbe p);

/* The media type for a name, and application/octet-stream for a name this server has
   no type for. A trailing .gz is taken off before the extension is read, because a
   stored form is how a file is kept and not what it is. The old server read the type
   off the name it had and shipped every script as text/plain, which a browser now
   refuses to run from a module tag. */
const char *contentTypeFor(const std::string &filename);

/* Whether this caller said it takes the stored form.

   A token list with weights, not a string to search: gzip;q=0 is a refusal,
   notgzipatall holds the letters and is a different token, and a wildcard answers
   only for a coding nothing named outright. Every reading that cannot be resolved
   answers false, because the wrong answer in the other direction sends bytes the
   caller has no way to read.

   Reachable from outside so the parse can be driven on its own: the number of ways of
   writing a header that must not count is larger than the number of files a case can
   plant. */
bool acceptsGzip(const std::string &accept_encoding);

} // namespace httpd

#endif
