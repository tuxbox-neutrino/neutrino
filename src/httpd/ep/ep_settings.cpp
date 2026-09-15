/*
 * ep_settings.cpp - routes for settings
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
#include "coreapi/base/result.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settings.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace httpd
{

namespace
{

const char *valueTypeName(coreapi::ValueType t)
{
	switch (t)
	{
		case coreapi::ValueType::Bool:   return "bool";
		case coreapi::ValueType::Int:    return "int";
		case coreapi::ValueType::String: return "string";
		case coreapi::ValueType::Enum:   return "enum";
	}
	// Unreachable while the compiler holds the switch to the enumeration, an
	// unhandled enumerator being an error in this directory.
	return "string";
}

const char *compareOpName(coreapi::CompareOp op)
{
	switch (op)
	{
		case coreapi::CompareOp::Eq: return "eq";
		case coreapi::CompareOp::Ne: return "ne";
		case coreapi::CompareOp::Lt: return "lt";
		case coreapi::CompareOp::Le: return "le";
		case coreapi::CompareOp::Gt: return "gt";
		case coreapi::CompareOp::Ge: return "ge";
		case coreapi::CompareOp::In: return "in";
	}
	return "eq";
}

/* A number as the text the wire carries it in, which is the rendering the layer below
   answers a value with, so what a caller reads out of the schema and out of a section
   are the same kind of thing. */
std::string decimal(long v)
{
	char buf[32];
	// Not a locale aware conversion: a window elsewhere in this program sets
	// the process locale out of the environment and never puts it back.
	std::snprintf(buf, sizeof(buf), "%ld", v);
	return std::string(buf);
}

/* label is not optional here the way the setting's own is: an Enum's own choice never
   leaves label_key NULL, and check-locale-catalog.sh holds every one of those to the
   catalog they name. appendDescriptor still guards against absence rather than
   trusting that guard from a distance. */
const FieldDesc kEnumValueFields[] = {
	HTTPD_MEMBER("value", FieldType::Int, "what the box stores for this choice"),
	HTTPD_MEMBER("label", FieldType::String, "the text the box shows for this choice"),
};

const Schema kEnumValueSchema = { "setting-choice", HTTPD_FIELDS(kEnumValueFields) };

const FieldDesc kConditionFields[] = {
	HTTPD_MEMBER("key", FieldType::String, "the setting whose current value this reads"),
	HTTPD_MEMBER_OF_SET("op", "eq,ne,lt,le,gt,ge,in", "how the value is held against the numbers below"),
	/* Plain numbers and so no shape beside it. One list whatever the operator, a
	   comparison against a single value being a list of one, so a reader has one member to
	   read rather than two that depend on which operator arrived. */
	HTTPD_LIST_OF_VALUES("values", ElementType::Int,
		"the numbers the value is compared against, one of them for every operator but in"),
};

const Schema kConditionSchema = { "setting-condition", HTTPD_FIELDS(kConditionFields) };

const FieldDesc kSettingFields[] = {
	HTTPD_MEMBER("id", FieldType::String,
		"what every route here names this setting by, which is the key the box stores it under"),
	HTTPD_MEMBER_OF_SET("type", "bool,int,string,enum", "what kind of value the setting holds"),
	HTTPD_MEMBER("section", FieldType::String, "which page of the settings it belongs to"),
	HTTPD_MEMBER_OPTIONAL("label", FieldType::String,
		"the text the box shows for it, absent where the box offers the setting on no screen "
		"or the catalog carries no text for its name"),
	HTTPD_MEMBER_OPTIONAL("hint", FieldType::String,
		"the name of the longer text beside it, absent where there is none"),
	HTTPD_MEMBER_OPTIONAL("min", FieldType::Int, "the lowest value it takes, only for a whole number"),
	HTTPD_MEMBER_OPTIONAL("max", FieldType::Int, "the highest, only for a whole number"),
	HTTPD_LIST_OF_OPTIONAL("values", &kEnumValueSchema,
		"what it accepts, only for a setting that offers a set"),
	HTTPD_MEMBER("default", FieldType::String,
		"what the box falls back to, rendered the way a value is, and empty for a setting held to be a credential"),
	HTTPD_MEMBER("needs_restart", FieldType::Bool,
		"whether the box has to be restarted before it takes effect"),
	HTTPD_MEMBER("secret", FieldType::Bool,
		"whether the setting is a credential, which is described here and never valued"),
	HTTPD_LIST_OF("conditions", &kConditionSchema,
		"every comparison that has to hold before the setting is worth showing, all of them together, empty for one always shown"),
};

const Schema kSettingSchema = { "setting", HTTPD_FIELDS(kSettingFields) };

const FieldDesc kSettingListFields[] = {
	HTTPD_LIST_OF("items", &kSettingSchema,
		"every setting declared, in the order the tables state them"),
};

const Schema kSettingListSchema = { "setting-list", HTTPD_FIELDS(kSettingListFields) };

const FieldDesc kSectionFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "what the route that lists a section's values names it by"),
};

const Schema kSectionSchema = { "setting-section", HTTPD_FIELDS(kSectionFields) };

const FieldDesc kSectionListFields[] = {
	HTTPD_LIST_OF("items", &kSectionSchema,
		"each section once, in the order the schema first names it"),
};

const Schema kSectionListSchema = { "setting-section-list", HTTPD_FIELDS(kSectionListFields) };

const FieldDesc kValueFields[] = {
	HTTPD_MEMBER("id", FieldType::String, "the setting, which is the key the box stores it under"),
	HTTPD_MEMBER("value", FieldType::String,
		"what the box is running on, rendered the way a value is written back, and empty for a credential"),
};

const Schema kValueSchema = { "setting-value", HTTPD_FIELDS(kValueFields) };

const FieldDesc kValueListFields[] = {
	HTTPD_LIST_OF("items", &kValueSchema, "every setting of the section and what it is set to"),
};

const Schema kValueListSchema = { "setting-value-list", HTTPD_FIELDS(kValueListFields) };

/* One declared setting. Nothing here withholds anything: the layer below answers a
   schema with the default of a credential already taken out, and reading the tables
   directly from here would hand a caller the value that the read of it refuses. */
void appendDescriptor(Json &j, const coreapi::Descriptor &d)
{
	j.beginObject();
	j.key("id");
	j.value(d.key);
	j.key("type");
	j.value(valueTypeName(d.type));
	j.key("section");
	j.value(d.section);

	/* Left out rather than answered with the key: label_key is the name of a text and
	   never the text itself. Absent either because the box offers this setting on no screen
	   or because it names a text the catalog does not carry, and resolveLabel answers false
	   for both. Printing label_key for the second is the defect this route exists to
	   close. */
	std::string label;
	if (coreapi::settings::resolveLabel(d.label_key, label))
	{
		j.key("label");
		j.value(label);
	}
	if (d.hint_key != NULL)
	{
		j.key("hint");
		j.value(d.hint_key);
	}

	// Only a whole number is bounded by these. Every other kind leaves both at
	// nought, and a pair of noughts written out would read as a setting that
	// takes nothing but nought.
	if (d.type == coreapi::ValueType::Int)
	{
		j.key("min");
		j.value(d.min);
		j.key("max");
		j.value(d.max);
	}

	if (d.type == coreapi::ValueType::Enum)
	{
		/* A setting whose values the box decides carries none of its own, so the layer is
		   asked for them and a caller reads one member either way.

		   Every row reaching this branch is a choice, and one whose values are the box's own
		   answers nothing while the box cannot be asked. A caller that finds it empty has a
		   setting it cannot draw a chooser for at this moment, not one it may offer as free
		   text, and a write of any value is refused for as long as that lasts. */
		j.key("values");
		j.beginArray();
		if (d.field.choices != NULL)
		{
			coreapi::Result<std::vector<coreapi::SettingChoice> > asked =
				coreapi::settings::choices(d.key);
			if (asked.ok())
			{
				const std::vector<coreapi::SettingChoice> &offered = asked.value();
				for (size_t i = 0; i < offered.size(); ++i)
				{
					j.beginObject();
					j.key("value");
					j.value(offered[i].value);
					/* Always here, unlike the branch below: these words are the text itself and not the
					   name of one, so there is no catalog that could fail to carry them. An empty one is a
					   value the box offers under no wording, which a caller may still write. */
					j.key("label");
					j.value(offered[i].label);
					j.endObject();
				}
			}
		}
		for (size_t i = 0; d.values != NULL && i < d.value_count; ++i)
		{
			j.beginObject();
			j.key("value");
			j.value(d.values[i].value);
			/* The schema declares this member always present, which check-locale-catalog.sh is
			   what makes true. A schema entry is not the compiler, so this still asks, and answers
			   the one way a resolve that somehow failed could be told apart from one that held:
			   leaving the field out rather than answering a choice with its key. */
			std::string label;
			if (coreapi::settings::resolveLabel(d.values[i].label_key, label))
			{
				j.key("label");
				j.value(label);
			}
			j.endObject();
		}
		j.endArray();
	}

	j.key("default");
	j.value((d.type == coreapi::ValueType::String)
	        ? std::string(d.default_string != NULL ? d.default_string : "")
	        : decimal(d.default_int));
	j.key("needs_restart");
	j.value(d.needs_restart);
	j.key("secret");
	j.value(d.secret);

	j.key("conditions");
	j.beginArray();
	for (size_t i = 0; d.conditions != NULL && i < d.condition_count; ++i)
	{
		const coreapi::Condition &c = d.conditions[i];
		j.beginObject();
		j.key("key");
		j.value(c.key);
		j.key("op");
		j.value(compareOpName(c.op));
		/* One list whatever the operator, so a reader has one member to read rather than two
		   that depend on which operator arrived. Every operator but in compares against a
		   single value, and a list of one is what that is. */
		j.key("values");
		j.beginArray();
		if (c.op == coreapi::CompareOp::In)
		{
			for (size_t v = 0; c.values != NULL && v < c.value_count; ++v)
				j.value(c.values[v]);
		}
		else
		{
			j.value(c.value);
		}
		j.endArray();
		j.endObject();
	}
	j.endArray();
	j.endObject();
}

Response settingsSchema(const Request &)
{
	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::Descriptor> rows = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 320 * rows.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < rows.size(); ++i)
		appendDescriptor(j, rows[i]);
	j.endArray();
	j.endObject();
	return out;
}

Response settingsSections(const Request &)
{
	coreapi::Result<std::vector<std::string> > got = coreapi::settings::sections();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<std::string> names = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 32 * names.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < names.size(); ++i)
	{
		j.beginObject();
		j.key("id");
		j.value(names[i]);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response settingsSection(const Request &r)
{
	const std::string &section = r.asString("section");

	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::Descriptor> rows = std::move(got).value();

	/* Every value is read before any of the answer is written. A read that
	   fails half way through a document leaves a body that stops mid member,
	   and the writer has no way to take back what it has already appended. */
	std::vector<const coreapi::Descriptor *> mine;
	std::vector<std::string> values;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].section == NULL || section != rows[i].section)
			continue;

		/* Through the read the layer below offers and never out of the store. That read is
		   what answers nothing for a credential, and a section that went to the store itself
		   would hand back the very values the schema beside it withholds. */
		coreapi::Result<std::string> value = coreapi::settings::get(rows[i].key);
		if (!value.ok())
			return problemFor(value.error());

		mine.push_back(&rows[i]);
		values.push_back(std::move(value).value());
	}

	/* A section nobody declared, which is a name that was asked for and not a section that
	   happens to be empty: every section this answers for is one the schema names, and the
	   schema names a section only where a row carries it. */
	if (mine.empty())
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
		                       "no settings are declared under a section of that name");

	Response out = okJson();
	Json j(out.body, 32 + 64 * mine.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < mine.size(); ++i)
	{
		j.beginObject();
		j.key("id");
		j.value(mine[i]->key);
		j.key("value");
		j.value(values[i]);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

/* One row of the declaration by its key, out of a schema already read. Read once
   per request and walked, rather than one describe() per key: a write of a dozen
   settings would otherwise read the whole declaration a dozen times. */
const coreapi::Descriptor *rowFor(const std::vector<coreapi::Descriptor> &rows,
                                  const std::string &key)
{
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].key != NULL && key == rows[i].key)
			return &rows[i];
	}
	return NULL;
}

bool sectionIsDeclared(const std::vector<coreapi::Descriptor> &rows, const std::string &section)
{
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (rows[i].section != NULL && section == rows[i].section)
			return true;
	}
	return false;
}

/* What one key of a write came to. The error is carried whole rather than as a
   code, because the answer states the same three things a refusal on its own
   would and they have to be the same three. */
struct Written
{
	std::string   key;
	int           code;
	bool          failed;
	coreapi::Error error;

	Written() : code(StatusOk), failed(false) {}
};

Response oneKeyRefused(const Written &w)
{
	return problemResponse(w.code, w.error.code, w.error.message);
}

/* Several outcomes as one answer, with a result per key.

   Per key and not one code for the lot, because a single code cannot say that some of
   them landed and one did not: the worst of them would report the ones that landed as
   though they had not, and the best would hide the one that was discarded. A caller that
   sent four settings and got one number back has no way to find out which of the four
   the box is running on without reading them all again.

   The members are named by the settings that were written, so no shape can be declared
   beside this route: a schema states the members an answer carries and these are named by
   the request. The wrapper is there so anything added to this answer later has somewhere
   to go that a setting's key cannot collide with.

   One member per result, which is a document only while the keys are distinct. They are:
   a body naming one setting twice is refused before any of this is reached. */
Response perKeyAnswer(const std::vector<Written> &results)
{
	Response out = okJson();
	out.code = StatusMultiStatus;

	Json j(out.body, 64 + 96 * results.size());
	j.beginObject();
	j.key("results");
	j.beginObject();
	for (size_t i = 0; i < results.size(); ++i)
	{
		const Written &w = results[i];
		j.key(w.key.c_str());
		j.beginObject();
		j.key("status");
		j.value(w.code);
		if (w.failed)
		{
			j.key("code");
			j.value(coreapi::codeString(w.error.code));
			j.key("detail");
			j.value(w.error.message);
		}
		j.endObject();
	}
	j.endObject();
	j.endObject();
	return out;
}

Response settingsWrite(const Request &r)
{
	const std::string &section = r.asString("section");

	/* The members are named by whatever settings the caller means to write, so the table
	   cannot declare them and the router does not bind them. What the table does declare is
	   that the body is an object of them and what one value is. */
	std::vector<JsonMember> members;
	if (!readFlatObject(r.body(), members))
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::BadString,
		                       "the body is not one flat object of strings, numbers and booleans");

	if (members.empty())
		return problemResponse(StatusBadRequest, coreapi::ErrorCode::MissingParameter,
		                       "the body names no setting to write");

	/* A name written twice, refused before anything is written and answered under the code
	   the router answers a value given twice with.

	   This handler reads its own body, so the router's own check never ran over these
	   names, and without this the two halves of one server disagreed: the route beside this
	   one declares its key and was answered duplicate-parameter, while a repeat here was
	   taken, both writes ran, the first was discarded with nothing said, and the answer
	   named the key twice. */
	for (size_t i = 0; i < members.size(); ++i)
	{
		for (size_t j = 0; j < i; ++j)
		{
			if (members[i].name != members[j].name)
				continue;
			// Names no key. The name is one of the parts of this a caller
			// wrote, and this answer travels back to places that render it.
			return problemResponse(StatusBadRequest, coreapi::ErrorCode::DuplicateParameter,
			                       "the body names one setting twice and there is no saying which value was meant");
		}
	}

	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::Descriptor> rows = std::move(got).value();

	/* A section nobody declared is answered before any key is looked at, so a
	   request that named the wrong page is told that rather than told its
	   settings are all unknown. */
	if (!sectionIsDeclared(rows, section))
		return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
		                       "no settings are declared under a section of that name");

	std::vector<Written> results;
	results.reserve(members.size());
	bool all_ok = true;

	for (size_t i = 0; i < members.size(); ++i)
	{
		Written w;
		/* The caller's spelling, because for a key nobody declares there is no other, and a
		   result a caller cannot match to what it sent is one it cannot act on. Everything that
		   reaches the answer goes through the writer, which escapes it. */
		w.key = members[i].name;

		const coreapi::Descriptor *d = rowFor(rows, w.key);
		if (d == NULL || d->section == NULL || section != d->section)
		{
			/* One answer for a key nothing declares and for a key declared on another page. Both
			   are the same act from where the caller sits, and a second code for it would tell a
			   caller which keys exist elsewhere on the box, one guess at a time. */
			w.failed = true;
			w.code = StatusNotFound;
			w.error = coreapi::Error(coreapi::Status::NotFound, coreapi::ErrorCode::UnknownSetting,
			                         "this section declares no setting under that key");
			all_ok = false;
			results.push_back(w);
			continue;
		}

		/* Text this server can answer back, asked here and not below. Not one of the settings
		   layer's rules: the file carries these bytes perfectly well and so does the store. It
		   is this layer's, because the writer that answers a value replaces a byte it cannot
		   read with the character that says so, and a value stored as sent and read back
		   substituted is a round trip that never settles. */
		if (!isUtf8(members[i].text.data(), members[i].text.size()))
		{
			w.failed = true;
			w.code = StatusBadRequest;
			w.error = coreapi::Error(coreapi::Status::InvalidArgument, coreapi::ErrorCode::BadString,
			                         "the value is not text this server can answer back unchanged");
			all_ok = false;
			results.push_back(w);
			continue;
		}

		/* Every other rule about the value is the layer below's: what one line of the settings
		   file can carry, what the row declares, and what the field behind it holds. That
		   includes the one this route exists beside: a row marked secret refuses an empty value,
		   so a form redrawn from a read that answered nothing cannot wipe the credential. */
		coreapi::Result<void> done = coreapi::settings::set(w.key, members[i].text);
		if (!done.ok())
		{
			w.failed = true;
			w.error = done.error();
			w.code = httpStatus(w.error.status);
			all_ok = false;
		}
		results.push_back(w);
	}

	/* One key is answered as itself. There is nothing for a per key answer to say that the
	   code and the document do not already say, and a caller writing one setting would have
	   to learn the shape above to read a refusal it can read everywhere else. */
	if (results.size() == 1)
	{
		if (results[0].failed)
			return oneKeyRefused(results[0]);
	}
	else if (!all_ok)
	{
		return perKeyAnswer(results);
	}

	/* Every one of them landed. Answered as the section reads now rather than as what was
	   sent: a write is held by the store and carried to the box on its own loop, and the
	   read below is what a caller would get if it asked, so the two cannot disagree. */
	return settingsSection(r);
}

Response clearSecret(const Request &r)
{
	const std::string &key = r.asString("key");

	/* Which keys may be cleared and what clearing one means are both the layer below's. A
	   row that is not a credential is refused there under a code of its own, because the key
	   is right and telling a caller there is no such setting would send it looking for a name
	   it already has. */
	coreapi::Result<void> done = coreapi::settings::clearSecret(key);
	if (!done.ok())
		return problemFor(done.error());

	coreapi::Result<std::string> now = coreapi::settings::get(key);
	if (!now.ok())
		return problemFor(now.error());

	/* The same shape one setting has in a section listing, so a caller reading this and a
	   caller reading the section read the same thing. What it says is what the read of a
	   credential always says, which is nothing: clearing one is not what makes it
	   unreadable. */
	Response out = okJson();
	Json j(out.body, 96);
	j.beginObject();
	j.key("id");
	j.value(key);
	j.key("value");
	j.value(now.value());
	j.endObject();
	return out;
}

/* The sections, for the document and for nothing else. Asked for rather than written
   down here: they come out of the settings tables, whose rows this box's model decides,
   so a list typed into this file would be right for one box and quietly wrong for
   another. Nothing is said when the layer below cannot answer, which leaves the segment
   described as text. */
void sectionNames(std::vector<std::string> &out)
{
	coreapi::Result<std::vector<std::string> > got = coreapi::settings::sections();
	if (got.ok())
		out = std::move(got).value();
}

const Param kSectionParams[] = {
	HTTPD_SEGMENT_FROM_ASKED_SET("section", "the section, as the section list names it", &sectionNames),
};

/* The route that writes a section takes a body the table cannot list, its members being
   whichever settings the caller means to write, and the second row is the one that says
   so rather than leaving a reader of the document with nothing to send.

   Its own array and not the read's: a read carries no body, and a row carried in the body
   of a GET is refused where the tables are checked.

   String because that is what a value of a setting is in this API: the read of a section
   answers every value as a string, so the two directions name one kind and a caller can
   send back what it read. A number or a boolean written unquoted is taken as the text it
   was written as, which is the body reader's own nicety.

   The two numbers count settings and not characters. */
const Param kWriteParams[] = {
	HTTPD_SEGMENT_FROM_ASKED_SET("section", "the section, as the section list names it", &sectionNames),
	HTTPD_BODY_IS_MAP_OF("settings", ParamType::String,
		"one member per setting to write, named as the section's schema names it, carrying the value as text",
		1, (long) kMaxBodyMembers),
};

/* The two written out below answer ahead of the one that binds a segment, which is
   settled where the tables are read and not by the order here. A section called schema or
   sections would therefore be unreachable, and none of the sixteen the program declares
   is called either. */
const Param kClearParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("key", "the credential to empty, as the schema names it", 256),
};

const Endpoint kSettingsEndpoints[] = {
	{ Method::Get, "/api/v1/settings/schema", AuthLevel::Read,
	  "every setting the box declares, and what each of them is",
	  NULL, 0, &kSettingListSchema, &settingsSchema, false },
	{ Method::Get, "/api/v1/settings/sections", AuthLevel::Read,
	  "the sections the settings are laid out in",
	  NULL, 0, &kSectionListSchema, &settingsSections, false },
	{ Method::Get, "/api/v1/settings/{section}", AuthLevel::Read,
	  "what one section's settings are set to",
	  HTTPD_PARAMS(kSectionParams), &kValueListSchema, &settingsSection, false },
	/* Written out and so answering ahead of the route that binds a segment, by the same
	   rule the two reads above do. A section called secret would be unreachable, and none of
	   the sixteen the program declares is called that. */
	{ Method::Post, "/api/v1/settings/secret/clear", AuthLevel::System,
	  "empties one credential, which is the one thing writing to it will not do",
	  HTTPD_PARAMS(kClearParams), &kValueSchema, &clearSecret, false },
	{ Method::Patch, "/api/v1/settings/{section}", AuthLevel::Write,
	  "writes settings of one section, answering the section as it reads now when every one of them landed and a result per key when they did not all agree",
	  HTTPD_PARAMS(kWriteParams), NULL, &settingsWrite, false },
};

} // namespace

extern const RouteTable settingsTable = {
	HTTPD_TABLE("settings", kSettingsEndpoints)
};

} // namespace httpd
