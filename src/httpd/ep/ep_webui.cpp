/*
 * ep_webui.cpp - the settings the web interface keeps of its own
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/extensions.h"
#include "coreapi/webui.h"

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{

namespace
{

/* The web interface's own file and not the box's settings: what /api/v1/settings
   reads and writes is neutrino.conf, declared row by row. This file has no rows.
   Its keys are whatever the pages and the extensions put there, so they are taken
   as they come, held to a shape a key and a value can have. */

const FieldDesc kPairFields[] = {
	HTTPD_MEMBER("key", FieldType::String, "the name the value is kept under"),
	HTTPD_MEMBER("value", FieldType::String, "the value, as text"),
};

const Schema kPairSchema = { "webui-setting", HTTPD_FIELDS(kPairFields) };

const FieldDesc kWebuiFields[] = {
	HTTPD_LIST_OF("items", &kPairSchema,
		"every key the file holds that is read back, by name, and empty for a box that never wrote it"),
	HTTPD_LIST_OF_VALUES("withheld", ElementType::String,
		"the keys the file holds whose values are not read back: passwords, PINs, tokens and keys to "
		"a service; written like any other"),
};

const Schema kWebuiSchema = { "webui-settings", HTTPD_FIELDS(kWebuiFields) };

Response answer(const coreapi::webui::Settings &all)
{
	Response out = okJson();
	Json j(out.body, 64 + 48 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (coreapi::webui::Settings::const_iterator it = all.begin(); it != all.end(); ++it)
	{
		if (coreapi::webui::isSecret(it->first))
			continue;
		j.beginObject();
		j.key("key");
		j.value(it->first);
		j.key("value");
		j.value(it->second);
		j.endObject();
	}
	j.endArray();
	j.key("withheld");
	j.beginArray();
	for (coreapi::webui::Settings::const_iterator it = all.begin(); it != all.end(); ++it)
		if (coreapi::webui::isSecret(it->first))
			j.value(it->first);
	j.endArray();
	j.endObject();
	return out;
}

Response readWebui(const Request &)
{
	return answer(coreapi::webui::read());
}

Response writeWebui(const Request &r)
{
	/* The members are named by whatever the caller keeps, so the table cannot
	   declare them and the router does not bind them, as with the box's settings. */
	std::vector<JsonMember> members;
	if (!readFlatObject(r.body(), members))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
		                       "the body is not one flat object of strings, numbers and booleans");
	if (members.empty())
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
		                       "the body names no key to write");

	coreapi::webui::Settings set;
	for (size_t i = 0; i < members.size(); ++i)
	{
		if (set.count(members[i].name) != 0)
			// Names no key: the name is a part of this a caller wrote, and this
			// answer travels back to places that render it.
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::DuplicateParameter,
			                       "the body names one key twice and there is no saying which value was meant");
		set[members[i].name] = members[i].text;
	}

	coreapi::Result<coreapi::webui::Settings> done = coreapi::webui::change(set);
	if (!done.ok())
		return problemFor(done.error());
	return answer(done.value());
}

const Param kWriteParams[] = {
	HTTPD_BODY_IS_MAP_OF("settings", ParamType::String,
		"one member per key to write, carrying its new value as text; an empty value takes the key out",
		1, (long) coreapi::webui::kMaxKeys),
};

const RouteRefusal kWriteRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadName,
		"a key is a letter followed by up to 299 letters, digits, underscores, dots and dashes"),
	HTTPD_REFUSES(InvalidArgument, BadString,
		"a value carries a control byte, {= or =}, one of < > \" ' ` or a backslash, which the old "
		"pages would carry into their markup, or #, which the file reads as a comment"),
	HTTPD_REFUSES(InvalidArgument, DuplicateParameter,
		"the body names one key twice and there is no saying which value was meant"),
	HTTPD_REFUSES(InvalidArgument, MissingParameter,
		"the body names no key to write"),
	HTTPD_REFUSES(InvalidArgument, ValueTooLong,
		"a value is 512 bytes at most"),
	HTTPD_REFUSES(Conflict, TooManySettings,
		"the file holds as many keys as it takes; take one out first"),
	HTTPD_REFUSES(Internal, SettingNotWritten,
		"the web interface's settings file could not be saved"),
};

/* THE EXTENSIONS of the web interface: the list the pages show and load, and
   installing and removing one (coreapi::extensions). The archive is always
   uploaded by the caller, who fetched it, and never fetched by the box. */

const FieldDesc kExtensionFields[] = {
	HTTPD_MEMBER("dir", FieldType::String,
		"the extension's directory under /extensions/, where its manifest.json is served"),
	HTTPD_LIST_OF_VALUES("scripts", ElementType::String,
		"the shell scripts it ships, which are installed beside the pages' own"),
};

const Schema kExtensionSchema = { "extension", HTTPD_FIELDS(kExtensionFields) };

const FieldDesc kExtensionsFields[] = {
	HTTPD_LIST_OF("items", &kExtensionSchema, "every installed extension, by name"),
};

const Schema kExtensionsSchema = { "extensions", HTTPD_FIELDS(kExtensionsFields) };

void writeExtension(Json &j, const coreapi::extensions::Extension &x)
{
	j.beginObject();
	j.key("dir");
	j.value(x.dir);
	j.key("scripts");
	j.beginArray();
	for (size_t i = 0; i < x.scripts.size(); ++i)
		j.value(x.scripts[i]);
	j.endArray();
	j.endObject();
}

Response listExtensions(const Request &)
{
	coreapi::Result<std::vector<coreapi::extensions::Extension> > got = coreapi::extensions::list();
	if (!got.ok())
		return problemFor(got.error());
	const std::vector<coreapi::extensions::Extension> all = std::move(got).value();
	Response out = okJson();
	Json j(out.body, 32 + 96 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
		writeExtension(j, all[i]);
	j.endArray();
	j.endObject();
	return out;
}

Response installExtension(const Request &r)
{
	coreapi::Result<coreapi::extensions::Extension> got = coreapi::extensions::install(r.asString("file"));
	if (!got.ok())
		return problemFor(got.error());
	Response out = createdJson();
	Json j(out.body, 128);
	writeExtension(j, got.value());
	return out;
}

Response removeExtension(const Request &r)
{
	coreapi::Result<void> done = coreapi::extensions::remove(r.asString("dir"));
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

const Param kInstallParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("file",
		"the archive's name in /tmp, where `PUT` and `PATCH /api/v1/storage/file` uploaded it: letters, "
		"digits, dots, dashes and underscores, ending in .tar.gz, .tgz or .tar", 255),
};

const Param kExtensionParams[] = {
	HTTPD_SEGMENT_TEXT("dir", "the extension's directory name", 64),
};

const RouteRefusal kInstallRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadName,
		"the archive is a plain name in the temporary directory, ending in .tar.gz, .tgz or .tar"),
	HTTPD_REFUSES(NotFound, NoSuchName, "there is no uploaded archive of that name"),
	HTTPD_REFUSES(InvalidArgument, ExtensionRefused,
		"the archive holds more than one top-level directory"),
	HTTPD_REFUSES(Conflict, ScriptNameTaken,
		"the pages already have a script named Y_Tools.sh, which is not this extension's own"),
	HTTPD_REFUSES(Conflict, ExtensionRefused, "another extension is being installed or removed"),
	HTTPD_REFUSES(Internal, ExtensionRefused, "the extension could not be put in place"),
};

const RouteRefusal kRemoveRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadName,
		"an extension is named with letters, digits, dashes and underscores"),
	HTTPD_REFUSES(NotFound, NoSuchName, "no extension of that name is installed"),
	HTTPD_REFUSES(Conflict, ExtensionRefused, "another extension is being installed or removed"),
	HTTPD_REFUSES(Internal, ExtensionRefused, "the extension could not be taken away"),
};

const Endpoint kWebuiEndpoints[] = {
	{ Method::Get, "/api/v1/webui/settings", AuthLevel::Read,
	  "what the web interface keeps of its own beside the box's settings",
	  "Reads the web interface's own settings file: the start page, the box's name and colour as "
	  "the pages show them, the devices it wakes over the network, which extensions it shows and "
	  "where, and what an extension keeps for itself. These are not the box's settings, which "
	  "`/api/v1/settings` reads and writes. A key whose name says it holds a password, a PIN, a "
	  "token or a key to a service is never answered; it is named in `withheld`.\n\n"
	  "**Related:** `PATCH /api/v1/webui/settings`.",
	  NULL, 0, &kWebuiSchema, &readWebui, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Patch, "/api/v1/webui/settings", AuthLevel::System,
	  "writes keys of the web interface's own settings, answering the file as it then reads",
	  "Writes the keys the body names into the web interface's own settings file, in one write of "
	  "the whole file, and answers it as `GET` does. A key left out keeps its value; an empty value "
	  "takes the key out, which the pages read the same as a key that is not there. Every key and "
	  "value is checked before anything is written, so a refused request leaves the file as it was. "
	  "System, because the old pages read one of these keys, `management_ip`, as who may reach "
	  "their management menus.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-name`: a key is not a letter followed by up to 299 letters, digits, "
	  "underscores, dots and dashes.\n"
	  "- `400 bad-string`: the body is not one flat object, or a value carries a control byte; "
	  "`{=` or `=}`, which the template engine of the old pages reads as its own; one of "
	  "`< > \" ' `` ` `` or a backslash, which the old pages carry into their markup and scripts as "
	  "they are; or `#`, which the file reads as the start of a comment.\n"
	  "- `400 duplicate-parameter`: the body names one key twice.\n"
	  "- `400 missing-parameter`: the body names no key.\n"
	  "- `400 value-too-long`: a value is longer than 512 bytes.\n"
	  "- `409 too-many-settings`: the file would hold more than 256 keys.\n"
	  "- `500 setting-not-written`: the file could not be saved whole; the one there before is "
	  "left in place.\n\n"
	  "**Related:** `GET /api/v1/webui/settings`.",
	  HTTPD_PARAMS(kWriteParams), &kWebuiSchema, &writeWebui, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kWriteRefusals, "{\"yweb_box_name\":\"Wohnzimmer\"}") },
	{ Method::Get, "/api/v1/webui/extensions", AuthLevel::Read,
	  "the extensions of the web interface that are installed",
	  "Lists the installed extensions of the web interface: each a directory under `/extensions/` "
	  "with a `manifest.json`, which the pages read from there, and the shell scripts it ships. "
	  "Which of them the pages show, and where, is in `GET /api/v1/webui/settings` "
	  "(`ext_<dir>_enabled`, `ext_<dir>_slot`).\n\n"
	  "**Related:** `POST /api/v1/webui/extensions`, `DELETE /api/v1/webui/extensions/{dir}`.",
	  NULL, 0, &kExtensionsSchema, &listExtensions, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Post, "/api/v1/webui/extensions", AuthLevel::System,
	  "installs or replaces an extension from an archive uploaded to /tmp",
	  "Installs the extension in an archive uploaded to /tmp beforehand, with `PUT` and `PATCH "
	  "/api/v1/storage/file` for one larger than a request; an installed extension of that name is "
	  "replaced, which is how one is updated. The box fetches nothing: a page that installs from "
	  "an address loads the archive itself and uploads it. The box takes the upload over into a "
	  "directory of its own and removes it, and reads the archive from its own headers: it has to "
	  "hold exactly one directory, named with letters, digits, dashes and underscores, with a "
	  "`manifest.json` in it, and only directories and regular files, no name that climbs or is "
	  "absolute, no more than 16 MiB, 2000 names and 64 MiB unpacked. The extension is unpacked "
	  "beside the installed ones, its shell scripts (its `scripts/*.sh`) are written beside the "
	  "pages' own, executable, and only then does it take the place of one of its name; a file or a "
	  "link in that place is replaced, never followed. A script "
	  "may not take the name of one the pages ship themselves, nor of any other script the pages "
	  "have unless it is this extension's own from an earlier install, still as that install left "
	  "it. The upload goes in every case.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-name`: `file` is not a plain name in /tmp ending in .tar.gz, .tgz or .tar.\n"
	  "- `404 no-such-name`: no archive of that name was uploaded.\n"
	  "- `400 extension-refused`: the archive is not an extension this box takes; the detail says why.\n"
	  "- `409 script-name-taken`: a script it ships would take the name of one the pages already have.\n"
	  "- `409 extension-refused`: another extension is being installed or removed.\n"
	  "- `500 extension-refused`: the disc refused; an installed extension of that name is as it "
	  "was, unless the detail says the new one is in place and names a script that could not take "
	  "its name.\n\n"
	  "**Related:** `GET /api/v1/webui/extensions`, `DELETE /api/v1/webui/extensions/{dir}`.",
	  HTTPD_PARAMS(kInstallParams), &kExtensionSchema, &installExtension, false,
	  Answers201, HTTPD_REFUSALS_AND_BODY(kInstallRefusals, "{\"file\":\"clock.tar.gz\"}") },
	{ Method::Delete, "/api/v1/webui/extensions/{dir}", AuthLevel::System,
	  "removes an extension and the scripts it shipped",
	  "Removes an installed extension, and the shell scripts it shipped, each only where the "
	  "pages' script of that name is still the one it installed and not one the pages ship "
	  "themselves. Its keys in the web interface's "
	  "settings are left; `PATCH /api/v1/webui/settings` takes them out.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-name`: no extension can carry that name.\n"
	  "- `404 no-such-name`: no extension of that name is installed.\n"
	  "- `409 extension-refused`: another extension is being installed or removed.\n"
	  "- `500 extension-refused`: the disc refused.\n\n"
	  "**Related:** `POST /api/v1/webui/extensions`.",
	  HTTPD_PARAMS(kExtensionParams), NULL, &removeExtension, false,
	  Answers204, HTTPD_REFUSALS(kRemoveRefusals) },
};

} // namespace

extern const RouteTable webuiTable = {
	HTTPD_TABLE("webui", kWebuiEndpoints)
};

} // namespace httpd
