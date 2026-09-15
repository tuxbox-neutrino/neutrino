/*
 * plugins.h - the plugins the box knows, and starting one
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

#ifndef __coreapi_plugins_h__
#define __coreapi_plugins_h__

#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <string>

namespace coreapi
{
namespace plugins
{

// Every plugin the box found the last time it read its directories. A box
// carrying none answers with an empty list rather than with a refusal.
Result<PluginList> list();

/* Asks the box to start the plugin of that name, which is the name of its files
   and not the name it calls itself.

   A command: posted and forgotten, so an Ok says the message was taken and
   nothing about what the box did with it. Not even that a plugin of that name
   exists, because what knows that is the list on the other side of the message. */
Result<void> start(const std::string &name);

/* Writes whether the box keeps this plugin out of its own menus.

   The flag lives in the plugin's own configuration file and nowhere else, so
   this is a write to that file and not a message. The file is replaced rather
   than written over, because a write that stops partway leaves a plugin the box
   cannot read at all, and every line of it but the one is carried across
   untouched.

   NotFound for a name this box has no plugin of, NotSupported for a plugin whose
   source cannot say which file it came from, and PluginNotChanged for a file
   that did not take the write.

   The box is not told. What draws the plugin menus reads the list it already
   holds, so the change reaches the television when the directories are read
   again, and asking for that is a route of its own. */
Result<void> setHidden(const std::string &name, bool hidden);

/* Asks the box to read its plugin directories again.

   A message and not a call, for the reason the seam behind the listing gives:
   the list is the one the screens are drawing from. So an Ok says the box was
   asked and not that the box has read them. */
Result<void> reload();

/* Runs one script out of the script root and answers what it wrote on its
   standard output.

   name is a name and not a path: the script run is the file of that name with
   the script extension, directly inside the root, and nothing else. args is
   split on whitespace and the pieces reach the script as its arguments in the
   order they were written, each as one argument whatever it holds. No shell is
   involved anywhere.

   What the checks on the name are worth: they answer for a name at the moment
   they are made, and not for what is opened under that name afterwards. Anything
   in it can be put back differently in between, the last component as readily as
   the root itself, because replacing a directory needs a write to its parent and
   not to the directory. So the confinement holds only where every directory from
   the filesystem root down to and including the root, and the contents of the
   root, are beyond the reach of anyone who can reach this.

   InvalidArgument for a name that is not one and for an argument that would be
   read as an option, NotFound for a name the root holds no script for, Busy for
   a script that outran the ceiling, and Internal for a root this layer was never
   given and for a script that failed. */
Result<std::string> run(const std::string &name, const std::string &args);

/* Where those scripts are. A trailing separator is dropped, so the joined name
   never carries a doubled one.

   Settable, because the directory the image installs to is one no machine
   running this outside a box has. */
void setScriptRoot(const std::string &dir);
std::string scriptRoot();

} // namespace plugins
} // namespace coreapi

#endif
