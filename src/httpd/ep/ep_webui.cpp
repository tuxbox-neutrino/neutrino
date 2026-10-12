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
};

} // namespace

extern const RouteTable webuiTable = {
	HTTPD_TABLE("webui", kWebuiEndpoints)
};

} // namespace httpd
