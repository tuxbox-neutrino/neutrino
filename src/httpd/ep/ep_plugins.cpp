/*
 * ep_plugins.cpp - routes for plugins and scripts
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

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/plugins.h"
#include "coreapi/base/result.h"
#include "coreapi/base/types.h"

#include <cstddef>
#include <string>
#include <utility>

namespace httpd
{

namespace
{

/* Two paths and not one.

   A plugin is something the box found, holds in a list and starts. A script is a file
   in one directory that this runs and hands the output of back, and the directory
   holds whatever was put there. The surface this replaces answered for both under one
   name while also searching nine directories for the second, two of which could come
   out empty and turn the search into a walk of the filesystem root.

   The second path is also what keeps the level visible. Starting a plugin is what a
   caller that drives this box does; running a file out of a directory by name is the
   whole of the box, and it is the only route here that says System. */

const char *kindName(coreapi::PluginKind k)
{
	switch (k)
	{
		case coreapi::PluginKind::Disabled: return "disabled";
		case coreapi::PluginKind::Game:     return "game";
		case coreapi::PluginKind::Tool:     return "tool";
		case coreapi::PluginKind::Script:   return "script";
		case coreapi::PluginKind::Lua:      return "lua";
		case coreapi::PluginKind::Unknown:  break;
	}
	// A sort this box has and this server has no word for. Named rather than
	// left out, so a reader sees a plugin it cannot classify instead of a
	// plugin it cannot see.
	return "unknown";
}

const FieldDesc kPluginFields[] = {
	HTTPD_MEMBER("name", FieldType::String,
		"what the box starts it by, which is the name of its files and is what the start route takes"),
	HTTPD_MEMBER("title", FieldType::String,
		"what the plugin calls itself, which is what a person reads and is not what starts it"),
	HTTPD_MEMBER("description", FieldType::String,
		"what the plugin says it does, empty for one that says nothing"),
	HTTPD_MEMBER_OF_SET("kind", "disabled,game,tool,script,lua,unknown",
		"what sort of thing it is, and unknown for a sort this server has no word for",
		"disabled: the box classifies it as disabled\n"
		"game: a game plugin\n"
		"tool: a plugin offered as a tool rather than a game\n"
		"script: a shell script the box runs as the plugin\n"
		"lua: a Lua script the box runs through its own Lua engine\n"
		"unknown: the box reports a sort this server has no word for"),
	HTTPD_MEMBER("hidden", FieldType::Bool,
		"whether the box keeps it out of its own menus, which says nothing about whether it can be started"),
};

const Schema kPluginSchema = { "plugin", HTTPD_FIELDS(kPluginFields) };

const FieldDesc kPluginListFields[] = {
	HTTPD_LIST_OF("items", &kPluginSchema,
		"every plugin this box found, in the order it keeps them, and empty for a box carrying none"),
};

const Schema kPluginListSchema = { "plugin-list", HTTPD_FIELDS(kPluginListFields) };

const FieldDesc kScriptOutputFields[] = {
	HTTPD_MEMBER("output", FieldType::String,
		"what the script wrote on its standard output, as it wrote it and read by nothing here"),
};

const Schema kScriptOutputSchema = { "script-output", HTTPD_FIELDS(kScriptOutputFields) };

Response listPlugins(const Request &)
{
	coreapi::Result<coreapi::PluginList> got = coreapi::plugins::list();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::PluginList all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 160 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		j.key("name");
		j.value(all[i].name);
		j.key("title");
		j.value(all[i].title);
		j.key("description");
		j.value(all[i].description);
		j.key("kind");
		j.value(std::string(kindName(all[i].kind)));
		j.key("hidden");
		j.value(all[i].hidden);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response startPlugin(const Request &r)
{
	coreapi::Result<void> done = coreapi::plugins::start(r.asString("name"));
	if (!done.ok())
		return problemFor(done.error());
	// Accepted and not done: what went out is a message, and nothing comes back
	// from what reads it.
	return accepted();
}

Response reloadPlugins(const Request &)
{
	coreapi::Result<void> done = coreapi::plugins::reload();
	if (!done.ok())
		return problemFor(done.error());
	/* The same answer and for a sharper reason: the list is rebuilt by the
	   thread that draws the screens, so this server could not say it has
	   happened without saying something it has no way of having learnt. A
	   caller that wants to know asks for the list. */
	return accepted();
}

/* The one write here that touches a file rather than sending a message.

   Write and not System: it changes which of the box's own plugins the box
   offers in its menus, which is the sort of thing every other setting on this
   interface is, and it reaches one line of one file the box already owns. The
   route below that runs a named file out of a directory is the System one, and
   the difference between the two is the point of their being two routes. */
Response hidePlugin(const Request &r)
{
	coreapi::Result<void> done =
		coreapi::plugins::setHidden(r.asString("name"), r.asBool("hidden"));
	if (!done.ok())
		return problemFor(done.error());
	/* Done and not accepted, because this one is. The file is written before
	   the answer goes out; what has not happened is the box reading its
	   directories again, and that is a route of its own rather than something
	   this could promise. */
	return noContent();
}

Response runScript(const Request &r)
{
	coreapi::Result<std::string> got =
		coreapi::plugins::run(r.asString("name"), r.asString("args"));
	if (!got.ok())
		return problemFor(got.error());

	const std::string written = std::move(got).value();

	// In a document and not as the bytes themselves. What a script writes is
	// of no stated kind, and an answer carrying it raw would be this server
	// naming a type it cannot know.
	Response out = okJson();
	Json j(out.body, 32 + written.size());
	j.beginObject();
	j.key("output");
	j.value(written);
	j.endObject();
	return out;
}

/* Bounded well above the longest name either of these has and well below what
   a value may be. Nothing quotes the name back, so the bound is not what keeps
   a caller from choosing how long its own words come back to it; it is what
   keeps a name that could never be one from being carried any further. */
const Param kNameParams[] = {
	HTTPD_SEGMENT_TEXT("name", "the plugin's name field, as GET /api/v1/plugins lists it, not its title", 64),
};

const Param kHideParams[] = {
	HTTPD_SEGMENT_TEXT("name", "the plugin's name field, as GET /api/v1/plugins lists it, not its title", 64),
	/* Required, and one route rather than one per direction, for the reason
	   the standby route states: hiding what is already hidden changes nothing
	   and is taken, so a caller that meant one and sent the other would be
	   told nothing about it. */
	HTTPD_BODY_REQUIRED("hidden", ParamType::Bool,
		"whether the box is to keep this plugin out of the menus it draws itself"),
};

const Param kScriptParams[] = {
	HTTPD_SEGMENT_TEXT("name", "the script, as a bare name: the file of that name with the script extension, directly inside the one directory this runs out of",
		64),
	/* One value and not a row per position. What the surface this replaces took was
	   numbered parameters joined with spaces and split at whitespace again a layer
	   down, so the numbering never reached the script; a table of numbered rows here
	   would state a highest position, and nothing could derive that number. */
	HTTPD_BODY_TEXT("args", "what to hand the script, split at whitespace, each piece reaching it as one argument whatever it holds",
		1024),
};

const RouteRefusal kRunScriptRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadName,
		"that is not a name a script of this box carries"),
	HTTPD_REFUSES(NotFound, ScriptMissing,
		"there is no script of that name"),
};

const Endpoint kPluginEndpoints[] = {
	{ Method::Get, "/api/v1/plugins", AuthLevel::Read,
	  "every plugin this box found and what each of them is", "Lists every "
	  "plugin the box found the last time it read its plugin directories, with "
	  "its name, title, description, kind and whether it is hidden from the "
	  "box's own menus. A box carrying none answers with an empty list rather "
	  "than with a refusal.\n\n"
	  "**Related:** `POST /api/v1/plugins/{name}/start`, "
	  "`PUT /api/v1/plugins/{name}/hidden`, `POST /api/v1/plugins/reload`.",
	  NULL, 0, &kPluginListSchema, &listPlugins, false,
	  Answers200, HTTPD_NO_REFUSALS },
	/* Above a write, as on the old surface: a plugin and a script are programs run as
	   the account this server runs as, which is the whole of the box. */
	{ Method::Post, "/api/v1/plugins/{name}/start", AuthLevel::System,
	  "asks the box to start one plugin", "Asks the box to start the plugin "
	  "`name` names, as choosing it from a plugin menu would. `202` means the "
	  "box has queued the request; this server does not learn whether a plugin "
	  "of that name exists or whether it actually started, because starting one "
	  "runs on the thread that also draws the screens and the box answers "
	  "nothing back to this layer. A binary plugin takes over the screen and the "
	  "remote control until it exits; a game plugin that draws into the live "
	  "video is skipped if no live picture is currently playing. Starting a "
	  "second plugin while one is already running is not prevented here: what "
	  "happens then is up to the plugin itself. A shell or Lua plugin's output, "
	  "if any, is shown on screen as a message once it finishes; nothing on this "
	  "interface returns that output, unlike `POST /api/v1/scripts/{name}`.",
	  HTTPD_PARAMS(kNameParams), NULL, &startPlugin, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Put, "/api/v1/plugins/{name}/hidden", AuthLevel::Write,
	  "writes whether the box keeps one plugin out of the menus it draws itself",
	  "Writes the `hidden` flag of the plugin `name` names into that plugin's "
	  "own configuration file, replacing the file as a whole so a write that "
	  "stops partway cannot leave it unreadable. `204` means the file has been "
	  "written; the flag says nothing about whether the plugin can still be "
	  "started through `POST /api/v1/plugins/{name}/start`, which does not check "
	  "it.\n\n"
	  "**Side effects:** rewrites the plugin's configuration file. The running "
	  "menus still show the old state until they are rebuilt, which "
	  "`POST /api/v1/plugins/reload` or a restart of the box does.\n\n"
	  "**Related:** `GET /api/v1/plugins`, `POST /api/v1/plugins/reload`.",
	  HTTPD_PARAMS(kHideParams), NULL, &hidePlugin, false,
	  Answers204, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/plugins/reload", AuthLevel::Write,
	  "asks the box to read its plugin directories again", "Asks the box to "
	  "read its plugin directories again and rebuild the list "
	  "`GET /api/v1/plugins` answers from. `202` means the box has queued the "
	  "request: the rebuild runs on the thread that draws the screens, so this "
	  "server cannot say when it is done. Read `GET /api/v1/plugins` afterwards "
	  "to see the new list.\n\n"
	  "**Related:** `GET /api/v1/plugins`.",
	  NULL, 0, NULL, &reloadPlugins, false,
	  Answers202, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/scripts/{name}", AuthLevel::System,
	  "runs one script out of the box's script directory and answers what it wrote",
	  "Runs the script `name` names from the box's own script directory and "
	  "waits for it to finish, then answers what it wrote on its standard "
	  "output as `output`. `args` is split on whitespace and each piece reaches "
	  "the script as one argument, in order; no shell is involved, so quoting "
	  "characters reach the script literally. `name` is a bare name and not a "
	  "path: the file run is that name with the script extension, directly "
	  "inside the script directory and nowhere else.\n\n"
	  "**Preconditions:** the script finishes within the box's own timeout; one "
	  "that does not is stopped and refused. The script runs as the account the "
	  "server itself runs as, which can do anything on the box, so the route "
	  "needs the `system` level: a login, or a token configured at that level.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-name`: `name` is empty, is `.` or `..`, starts with `-`, or "
	  "holds a character other than letters, digits, `.`, `-` or `_`.\n"
	  "- `404 script-missing`: the script directory holds no script of that "
	  "name.",
	  HTTPD_PARAMS(kScriptParams), &kScriptOutputSchema, &runScript, false,
	  Answers200, HTTPD_REFUSALS(kRunScriptRefusals) },
};

} // namespace

extern const RouteTable pluginsTable = {
	HTTPD_TABLE("plugins", kPluginEndpoints)
};

} // namespace httpd
