/*
 * ep_settings.cpp - routes for settings
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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

#include "coreapi/base/deps.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/couple.h"
#include "coreapi/settings/settings.h"

#include <cstddef>
#include <cstdio>
#include <map>
#include <set>
#include <cstring>
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
		case coreapi::ValueType::Key:    return "key";
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
		case coreapi::CompareOp::TextValid: return "text-valid";
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

// The words the two members below are stated with, held to the layer's own by name.
const char *pairWritesName(const char *writes)
{
	if (std::strcmp(writes, "id") == 0)
		return "id";
	return "both";
}

const char *channelKindName(const char *kind)
{
	if (std::strcmp(kind, "radio") == 0)
		return "radio";
	return "tv";
}

/* label is not optional here the way the setting's own is: every choice carries words,
   either the catalog text its key names or fixed text of its own, and label is always
   that text. check-locale-catalog.sh holds every key a choice names to the catalog.
   appendDescriptor still guards against absence rather than trusting that guard from a
   distance. */
const FieldDesc kEnumValueFields[] = {
	HTTPD_MEMBER("value", FieldType::Int,
		"the number the box stores when this choice is picked, matched against the setting's own stored value; "
		"0 for a choice of a text setting, which stores the text below"),
	HTTPD_MEMBER_OPTIONAL("text", FieldType::String,
		"what a text setting stores when this choice is picked, matched against its stored value; "
		"present only for a setting of type string"),
	HTTPD_MEMBER("label", FieldType::String,
		"the text the box shows for this choice: its catalog text resolved into the box's configured "
		"language, or the fixed words the box shows in every language, such as a size or a format name"),
};

const Schema kEnumValueSchema = { "setting-choice", HTTPD_FIELDS(kEnumValueFields) };

// What each operator means, said once for both shapes that carry one.
const char kConditionOpDocs[] =
	"eq: the setting's current value equals the one number given\n"
	"ne: the setting's current value does not equal the one number given\n"
	"lt: the setting's current value is less than the one number given\n"
	"le: the setting's current value is less than or equal to the one number given\n"
	"gt: the setting's current value is greater than the one number given\n"
	"ge: the setting's current value is greater than or equal to the one number given\n"
	"in: the setting's current value is one of the numbers given\n"
	"text-valid: the setting's current text is not empty and is not the text given";

/* One comparison inside a group. A group inside a group is never answered, so
   this shape carries no group of its own.

   values: plain numbers and so no shape beside it. One list whatever the numeric
   operator, a comparison against a single value being a list of one, so a reader
   has one member to read rather than two that depend on which operator arrived. */
const FieldDesc kAlternativeFields[] = {
	HTTPD_MEMBER("key", FieldType::String, "the setting whose current value this reads"),
	HTTPD_MEMBER_OF_SET("op", "eq,ne,lt,le,gt,ge,in,text-valid",
		"how the value is held against the numbers or the text below", kConditionOpDocs),
	HTTPD_MEMBER_OPTIONAL("text", FieldType::String,
		"the placeholder text-valid holds the setting's text against, present only for that operator"),
	HTTPD_LIST_OF_VALUES_OPTIONAL("values", ElementType::Int,
		"the numbers the value is compared against, one of them for every operator but in; "
		"absent for text-valid, which compares text"),
};

const Schema kAlternativeSchema = { "setting-condition-alternative", HTTPD_FIELDS(kAlternativeFields) };

/* A comparison, carrying key and op, or a group, carrying any and nothing else.
   One shape with the members of both optional rather than two a reader has to
   tell apart first. */
const FieldDesc kConditionFields[] = {
	HTTPD_MEMBER_OPTIONAL("key", FieldType::String,
		"the setting whose current value this reads, absent for a group"),
	HTTPD_MEMBER_OF_SET_OPTIONAL("op", "eq,ne,lt,le,gt,ge,in,text-valid",
		"how the value is held against the numbers or the text below, absent for a group", kConditionOpDocs),
	HTTPD_MEMBER_OPTIONAL("text", FieldType::String,
		"the placeholder text-valid holds the setting's text against, present only for that operator"),
	HTTPD_LIST_OF_VALUES_OPTIONAL("values", ElementType::Int,
		"the numbers the value is compared against, one of them for every operator but in; "
		"absent for text-valid, which compares text, and for a group"),
	HTTPD_LIST_OF_OPTIONAL("any", &kAlternativeSchema,
		"present only for a group, which holds when any one of these comparisons holds"),
};

const Schema kConditionSchema = { "setting-condition", HTTPD_FIELDS(kConditionFields) };

const FieldDesc kSettingFields[] = {
	HTTPD_MEMBER("id", FieldType::String,
		"what every route here names this setting by, which is the key the box stores it under"),
	HTTPD_MEMBER_OF_SET("type", "bool,int,string,enum,key",
		"what kind of value this setting holds, which decides how the rest of this descriptor is read",
		"bool: stores 0 or 1 and is shown as a toggle\n"
		"int: a whole number, bounded by min and max\n"
		"string: free text or an identifier, with no numeric bounds; where the box offers a list of the "
		"texts it takes, they are listed under values\n"
		"enum: one of a fixed or box reported set of choices, listed under values\n"
		"key: the code of a remote control key, stored as a whole number between min and max and shown "
		"by the name GET /api/v1/settings/keys gives it; a code that list does not name is accepted too "
		"where a remote control can send it"),
	HTTPD_MEMBER("section", FieldType::String,
		"which page of the settings it belongs to, as GET /api/v1/settings/sections lists it"),
	HTTPD_MEMBER_OPTIONAL("label", FieldType::String,
		"the text the box shows for it, absent where the box offers the setting on no screen "
		"or the catalog carries no text for its name"),
	HTTPD_MEMBER_OPTIONAL("hint", FieldType::String,
		"the name of the longer text beside it, absent where there is none"),
	HTTPD_MEMBER_OPTIONAL("hint_text", FieldType::String,
		"that longer text itself, resolved into the box's configured language as the label is, "
		"the help the box shows beside the setting on its own screen; absent where there is no "
		"hint or the catalog carries no text for it"),
	HTTPD_MEMBER_OPTIONAL("min", FieldType::Int,
		"the lowest whole number this setting accepts on this box now, present only when type is int or "
		"key; where the box decides the range as it runs, this is the range as it stands, and the value "
		"already stored and the default are still taken outside it once the box has moved it"),
	HTTPD_MEMBER_OPTIONAL("max", FieldType::Int,
		"the highest whole number this setting accepts on this box now, present only when type is int or "
		"key; where the box decides the range as it runs, this is the range as it stands, and the value "
		"already stored and the default are still taken outside it once the box has moved it"),
	HTTPD_MEMBER_OPTIONAL("unit", FieldType::String,
		"the name of the text that follows the number, such as unit.short.hour, for the box's own "
		"language catalog and not the text itself; present only when type is int and the box shows "
		"the number with a unit, so a client draws its own text for the name and nothing for one it "
		"does not know"),
	HTTPD_LIST_OF_OPTIONAL("values", &kEnumValueSchema,
		"what it accepts, for a setting that offers a set; for an int, each number the box "
		"shows in words instead, such as off, which it accepts beside min to max; for a text setting "
		"the texts the box offers right now, where an empty list is a box that cannot say and any "
		"text the setting's other rules allow is taken"),
	HTTPD_MEMBER_OPTIONAL("values_from", FieldType::String,
		"instead of values, for a list that several settings of this answer carry alike: the name of that list "
		"under the answer's value_lists, whose entries are read exactly as values would be"),
	HTTPD_MEMBER_OPTIONAL("listed", FieldType::Bool,
		"present and true only for an int whose values are every number it accepts on this box, "
		"each named, such as the tuners it has, beside the value already stored and the default, "
		"which are taken too; a client offers it as that list and not as a number, and an int "
		"without it treats its values as names for some numbers of its range"),
	HTTPD_MEMBER_OPTIONAL("text_kind", FieldType::String,
		"what sort of text a string setting holds, present only for a string setting with a rule: "
		"plain, directory, file, pin, host, number, name or paths"),
	HTTPD_MEMBER_OPTIONAL("min_length", FieldType::Int,
		"the fewest bytes a string setting with a rule accepts; 0 for no minimum"),
	HTTPD_MEMBER_OPTIONAL("max_length", FieldType::Int,
		"the most bytes a string setting with a rule accepts; 0 for no limit"),
	HTTPD_MEMBER_OPTIONAL("allowed_chars", FieldType::String,
		"the only characters a string setting accepts, absent where any character is taken"),
	HTTPD_MEMBER_OPTIONAL("must_exist", FieldType::String,
		"whether the place a string setting names must exist on the box: no, yes, "
		"not-memory (and not flash or memory backed storage) or not-flash (and not flash storage)"),
	HTTPD_MEMBER_OPTIONAL("extensions", FieldType::String,
		"the file name endings a file setting accepts, separated by commas, absent for any"),
	HTTPD_MEMBER("default", FieldType::String,
		"what the box falls back to, rendered the way a value is, and empty for a setting held to be a credential"),
	HTTPD_MEMBER_OPTIONAL("needs_restart", FieldType::Bool,
		"present and true only where the box has to be restarted before the setting takes effect; absent means false"),
	HTTPD_MEMBER_OPTIONAL("secret", FieldType::Bool,
		"present and true only for a credential, which is described here and never valued; absent means false"),
	HTTPD_LIST_OF("conditions", &kConditionSchema,
		"every comparison that has to hold before the setting is worth showing, all of them together, empty for one always shown"),
	HTTPD_MEMBER_OPTIONAL("locked", FieldType::Bool,
		"present and true only while the box's parental lock fixes the setting, so every write of it is "
		"refused; absent means false"),
	HTTPD_MEMBER_OPTIONAL("available", FieldType::Bool,
		"present and false only where this box lacks what the setting controls, such as a fan, or cannot "
		"say; absent means true. A setting that is not available is offered on no screen and every write "
		"of it is refused, while its value still reads"),
	HTTPD_MEMBER_OPTIONAL("pair", FieldType::String,
		"the other half of a setting that is one fact in two parts, present on both halves; how the two "
		"are written is pair_writes"),
	HTTPD_MEMBER_OF_SET_OPTIONAL("pair_writes", "both,id",
		"how a pair is written, present wherever pair is",
		"both: the two are written in one request or not at all; either alone is refused\n"
		"id: the identifier alone is written and the box fills the other half from it; the other half "
		"written alone is refused"),
	HTTPD_MEMBER_OF_SET_OPTIONAL("channel_kind", "tv,radio",
		"present only on a start channel row: which channel list its channel comes from",
		"tv: the television list, GET /api/v1/channels with mode tv\n"
		"radio: the radio list, GET /api/v1/channels with mode radio"),
};

const Schema kSettingSchema = { "setting", HTTPD_FIELDS(kSettingFields) };

const FieldDesc kSettingListFields[] = {
	HTTPD_NAMED_LISTS_OPTIONAL("value_lists", &kEnumValueSchema,
		"present only when two or more settings of the answer carry the same list of choices: each such list "
		"once, under the name those settings give in values_from; a list only one setting carries stays with it"),
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

const FieldDesc kKeyNameFields[] = {
	HTTPD_MEMBER("code", FieldType::Int,
		"the number a key setting stores for this key, matched against the setting's own stored value"),
	HTTPD_MEMBER("name", FieldType::String,
		"what the box calls the key, which is the text its own key chooser shows for the code"),
};

const Schema kKeyNameSchema = { "setting-key-name", HTTPD_FIELDS(kKeyNameFields) };

const FieldDesc kKeyNameListFields[] = {
	HTTPD_LIST_OF("items", &kKeyNameSchema,
		"every key the box names: no key first, then each named key, pressed and then held"),
};

const Schema kKeyNameListSchema = { "setting-key-name-list", HTTPD_FIELDS(kKeyNameListFields) };

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

// One comparison, on its own or as an alternative inside a group.
void appendComparison(Json &j, const coreapi::Condition &c)
{
	j.beginObject();
	j.key("key");
	j.value(c.key != NULL ? c.key : "");
	j.key("op");
	j.value(compareOpName(c.op));
	if (c.op == coreapi::CompareOp::TextValid)
	{
		j.key("text");
		j.value(c.text != NULL ? c.text : "");
		j.endObject();
		return;
	}
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

/* One declared setting. Nothing here withholds anything: the layer below answers a
   schema with the default of a credential already taken out, and reading the tables
   directly from here would hand a caller the value that the read of it refuses. */
/* Built before any row is written, because whether a list is shared depends on all rows. */
struct RowValues
{
	RowValues() : has(false), listed(false) {}
	bool        has;
	bool        listed;
	std::string json;
	std::string from;
};

void valuesOf(const coreapi::Descriptor &d, RowValues &out)
{
	Json j(out.json);

	// Every value the box shows in words rather than as the number.
	if (d.type == coreapi::ValueType::Int && d.choices_from == NULL && d.values != NULL && d.value_count > 0)
	{
		out.has = true;
		j.beginArray();
		for (size_t i = 0; i < d.value_count; ++i)
		{
			const coreapi::EnumValue &named = d.values[i];
			std::string words;
			coreapi::settings::resolveLabel(named.label_key, words);
			j.beginObject();
			j.key("value");
			j.value((long) named.value);
			j.key("label");
			j.value(words);
			j.endObject();
		}
		j.endArray();
		return;
	}

	if (d.type == coreapi::ValueType::Enum)
	{
		/* A setting whose values the box decides carries none of its own, so the layer is
		   asked for them and a caller reads one member either way.

		   Every row reaching this branch is a choice, and one whose values are the box's own
		   answers nothing while the box cannot be asked. A caller that finds it empty has a
		   setting it cannot draw a chooser for at this moment, not one it may offer as free
		   text, and a write is refused for as long as that lasts, but for the value already
		   stored and the default, which are no new pick. */
		out.has = true;
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
			// An entry this box lacks is not offered, as the write turns it down.
			if (!coreapi::settings::entryOfferedNow(d.values[i]))
				continue;
			j.beginObject();
			j.key("value");
			j.value(d.values[i].value);
			/* The words of an entry the box names itself are the text. The schema declares
			   this member always present, which check-locale-catalog.sh makes true for the
			   rest. A schema entry is not the compiler, so this still asks, and answers the one
			   way a resolve that somehow failed could be told apart from one that held: leaving
			   the field out rather than answering a choice with its key. */
			std::string text;
			if (d.values[i].label_text != NULL)
			{
				j.key("label");
				j.value(d.values[i].label_text);
			}
			else if (coreapi::settings::resolveLabel(d.values[i].label_key, text))
			{
				j.key("label");
				j.value(text);
			}
			j.endObject();
		}
		j.endArray();
		return;
	}

	if ((d.type == coreapi::ValueType::String || d.type == coreapi::ValueType::Int) && d.choices_from != NULL)
	{
		/* The values the box offers, a text each under the text it stores and a number under
		   its number. Empty is a box that cannot say what it has, and then the row's other
		   rules alone hold: a caller offers free text or a free number rather than nothing. */
		out.has = true;
		j.beginArray();
		coreapi::Result<std::vector<coreapi::SettingChoice> > asked =
			coreapi::settings::choices(d.key);
		if (asked.ok())
		{
			const std::vector<coreapi::SettingChoice> &offered = asked.value();
			for (size_t i = 0; i < offered.size(); ++i)
			{
				j.beginObject();
				j.key("value");
				j.value(d.type == coreapi::ValueType::String ? 0L : offered[i].value);
				if (d.type == coreapi::ValueType::String)
				{
					j.key("text");
					j.value(offered[i].text);
				}
				j.key("label");
				j.value(offered[i].label);
				j.endObject();
			}
		}
		j.endArray();
		// A number with its list stated is every number it takes on this box.
		out.listed = d.type == coreapi::ValueType::Int && asked.ok();
	}
}

/* The name a list is stated under when several rows share it. The text of the list itself
   decides it, so in practice the same list has the same name whichever section the answer
   was narrowed to; a hash collision inside one answer renames, so a client reads the name
   per answer. */
std::string valueListName(const std::string &json)
{
	unsigned long h = 2166136261UL;
	for (size_t i = 0; i < json.size(); ++i)
		h = ((h ^ (unsigned char) json[i]) * 16777619UL) & 0xffffffffUL;
	char buf[24];
	std::snprintf(buf, sizeof(buf), "list-%08lx", h);
	return buf;
}

void appendDescriptor(Json &j, const coreapi::Descriptor &d, const RowValues &values)
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

		/* The text beside the name, so a page shows the same help the screen at the box
		   shows and needs no catalog of its own. Absent for the reason a label is. */
		std::string hint_text;
		if (coreapi::settings::resolveLabel(d.hint_key, hint_text))
		{
			j.key("hint_text");
			j.value(hint_text);
		}
	}

	// Only a whole number and a key code are bounded by these. Every other kind
	// leaves both at nought, and a pair of noughts written out would read as a
	// setting that takes nothing but nought.
	const coreapi::Bounds now = coreapi::boundsNow(d);
	if (d.type == coreapi::ValueType::Int || d.type == coreapi::ValueType::Key)
	{
		j.key("min");
		j.value(now.min);
		j.key("max");
		j.value(now.max);
		if (d.type == coreapi::ValueType::Int && d.unit_key != NULL)
		{
			j.key("unit");
			j.value(d.unit_key);
		}
	}

	if (values.has)
	{
		if (values.from.empty())
		{
			j.key("values");
			j.raw(values.json.c_str());
		}
		else
		{
			j.key("values_from");
			j.value(values.from);
		}
		if (values.listed)
		{
			j.key("listed");
			j.value(true);
		}
	}

	if (d.text != NULL)
	{
		static const char *const kinds[] = { "plain", "directory", "file", "pin", "host", "number", "name", "paths" };
		static const char *const places[] = { "no", "yes", "not-memory", "not-flash" };
		j.key("text_kind");
		j.value(kinds[(int) d.text->kind]);
		j.key("min_length");
		j.value((long) d.text->min_length);
		j.key("max_length");
		j.value((long) d.text->max_length);
		if (d.text->allowed != NULL)
		{
			j.key("allowed_chars");
			j.value(d.text->allowed);
		}
		j.key("must_exist");
		j.value(places[(int) d.text->must_exist]);
		if (d.text->extensions != NULL)
		{
			j.key("extensions");
			j.value(d.text->extensions);
		}
	}

	j.key("default");
	j.value((d.type == coreapi::ValueType::String)
	        ? std::string(d.default_string != NULL ? d.default_string : "")
	        : decimal(coreapi::defaultInt(d)));
	/* A member at its default is left out: needs_restart and secret false. Most rows are at
	   both, and a list that states them anyway is a good part of the answer. */
	if (d.needs_restart)
	{
		j.key("needs_restart");
		j.value(true);
	}
	if (d.secret)
	{
		j.key("secret");
		j.value(true);
	}

	if (coreapi::settings::lockedNow(d.key))
	{
		j.key("locked");
		j.value(true);
	}
	if (!coreapi::settings::availableNow(d))
	{
		j.key("available");
		j.value(false);
	}

	const char *writes = NULL;
	const char *partner = coreapi::settings::pairPartner(d.key, &writes);
	if (partner != NULL)
	{
		j.key("pair");
		j.value(partner);
		j.key("pair_writes");
		j.value(pairWritesName(writes));
	}
	const char *kind = coreapi::settings::startChannelKind(d.key);
	if (kind != NULL)
	{
		j.key("channel_kind");
		j.value(channelKindName(kind));
	}

	j.key("conditions");
	j.beginArray();
	for (size_t i = 0; d.conditions != NULL && i < d.condition_count; ++i)
	{
		const coreapi::Condition &c = d.conditions[i];
		if (!coreapi::conditionIsGroup(c))
		{
			appendComparison(j, c);
			continue;
		}
		/* A group inside a group is refused by the sanity check, and the
		   evaluator answers it as holding, which makes the group around it hold.
		   Such a group is left out, which a reader answers the same way, rather
		   than written with a member that reads a setting with no name. */
		bool nested = false;
		for (size_t m = 0; c.any_of != NULL && m < c.any_count; ++m)
			nested = nested || coreapi::conditionIsGroup(c.any_of[m]);
		if (nested)
			continue;
		j.beginObject();
		j.key("any");
		j.beginArray();
		for (size_t m = 0; c.any_of != NULL && m < c.any_count; ++m)
			appendComparison(j, c.any_of[m]);
		j.endArray();
		j.endObject();
	}
	j.endArray();
	j.endObject();
}

Response settingsSchema(const Request &r)
{
	coreapi::Result<std::vector<coreapi::Descriptor> > got = coreapi::settings::schema();
	if (!got.ok())
		return problemFor(got.error());

	const std::vector<coreapi::Descriptor> rows = std::move(got).value();

	const bool narrowed = r.has("section");
	const std::string &section = r.asString("section");
	if (narrowed)
	{
		bool have = false;
		for (size_t i = 0; !have && i < rows.size(); ++i)
			have = rows[i].section != NULL && section == rows[i].section;
		if (!have)
			return problemResponse(StatusNotFound, coreapi::ErrorCode::NoSuchName,
			                       "no setting is declared under a section of that name");
	}

	std::vector<size_t> chosen;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		if (!narrowed || (rows[i].section != NULL && section == rows[i].section))
			chosen.push_back(i);
	}

	/* A list that two or more rows of this answer carry is stated once under value_lists, and
	   those rows name it. One row's list stays with the row, and a list no row of this answer
	   carries is not here. */
	std::vector<RowValues> values(chosen.size());
	std::map<std::string, size_t> uses;
	for (size_t i = 0; i < chosen.size(); ++i)
	{
		valuesOf(rows[chosen[i]], values[i]);
		if (values[i].has && values[i].json != "[]")
			++uses[values[i].json];
	}
	std::map<std::string, std::string> names;
	std::set<std::string> taken;
	for (size_t i = 0; i < chosen.size(); ++i)
	{
		if (!values[i].has || values[i].json == "[]" || uses[values[i].json] < 2)
			continue;
		std::map<std::string, std::string>::const_iterator known = names.find(values[i].json);
		if (known == names.end())
		{
			// Two different lists with one hash would otherwise answer as one.
			std::string name = valueListName(values[i].json);
			while (taken.count(name) != 0)
				name += "x";
			taken.insert(name);
			known = names.insert(std::make_pair(values[i].json, name)).first;
		}
		values[i].from = known->second;
	}

	Response out = okJson();
	Json j(out.body, 32 + 320 * chosen.size());
	j.beginObject();
	if (!names.empty())
	{
		j.key("value_lists");
		j.beginObject();
		// First-use order keeps the answer the same from call to call.
		std::set<std::string> written;
		for (size_t i = 0; i < chosen.size(); ++i)
		{
			if (values[i].from.empty() || !written.insert(values[i].from).second)
				continue;
			j.key(values[i].from.c_str());
			j.raw(values[i].json.c_str());
		}
		j.endObject();
	}
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < chosen.size(); ++i)
		appendDescriptor(j, rows[chosen[i]], values[i]);
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

Response settingsKeys(const Request &)
{
	const std::vector<coreapi::KeyName> all = coreapi::keySource().all();

	Response out = okJson();
	Json j(out.body, 32 + 48 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		j.key("code");
		j.value(all[i].code);
		j.key("name");
		j.value(all[i].name);
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
	// The settings that took this one down with them, none where its own value was refused.
	std::vector<std::string> depends_on;

	Written() : code(StatusOk), failed(false) {}
};

Response oneKeyRefused(const Written &w)
{
	// The whole error, so the settings a condition names go back as in a 207.
	return problemResponse(w.code, w.error);
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
			if (!w.depends_on.empty())
			{
				j.key("depends_on");
				j.beginArray();
				for (size_t k = 0; k < w.depends_on.size(); ++k)
					j.value(w.depends_on[k]);
				j.endArray();
			}
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

	/* The three passes of a write of several settings, as the settings layer's writeBatch()
	   makes them, here and not through it because this answer names every key: check() on
	   each member, settleBatch() over what passed, set() on what is left. */
	coreapi::settings::BatchOverlay batch;
	coreapi::settings::Refusals refused;

	for (size_t i = 0; i < members.size(); ++i)
	{
		/* The caller's spelling, because for a key nobody declares there is no other, and a
		   result a caller cannot match to what it sent is one it cannot act on. Everything that
		   reaches the answer goes through the writer, which escapes it. */
		const std::string &key = members[i].name;

		const coreapi::Descriptor *d = rowFor(rows, key);
		if (d == NULL || d->section == NULL || section != d->section)
		{
			/* One answer for a key nothing declares and for a key declared on another page. Both
			   are the same act from where the caller sits, and a second code for it would tell a
			   caller which keys exist elsewhere on the box, one guess at a time. */
			refused.push_back(std::make_pair(key,
				coreapi::Error(coreapi::Status::NotFound, coreapi::ErrorCode::UnknownSetting,
				               "this section declares no setting under that key")));
			continue;
		}

		/* Text this server can answer back, asked here and not below. Not one of the settings
		   layer's rules: the file carries these bytes perfectly well and so does the store. It
		   is this layer's, because the writer that answers a value replaces a byte it cannot
		   read with the character that says so, and a value stored as sent and read back
		   substituted is a round trip that never settles. */
		if (!isUtf8(members[i].text.data(), members[i].text.size()))
		{
			refused.push_back(std::make_pair(key,
				coreapi::Error(coreapi::Status::InvalidArgument, coreapi::ErrorCode::BadString,
				               "the value is not text this server can answer back unchanged")));
			continue;
		}

		/* Every other rule about the value is the layer below's: what one line of the settings
		   file can carry, what the row declares, and what the field behind it holds. That
		   includes the one this route exists beside: a row marked secret refuses an empty
		   value, so a form redrawn from a read that answered nothing cannot wipe the
		   credential. Only what passed goes on, since a value that never lands must not allow
		   another one. */
		coreapi::Result<void> checked = coreapi::settings::check(key, members[i].text);
		if (!checked.ok())
		{
			refused.push_back(std::make_pair(key, checked.error()));
			continue;
		}
		batch.values.push_back(std::make_pair(key, members[i].text));
	}

	/* The couplings and the conditions, judged on what the whole write leaves behind, so a
	   pair does not land in part and a setting and the one that allows it can be sent in
	   either order. What falls is in refused under its own key; a member the box holds
	   already is left out, as done. */
	coreapi::settings::settleBatch(batch, refused);

	std::vector<Written> results;
	results.reserve(members.size() + batch.values.size());
	bool all_ok = true;

	/* The caller's keys in the caller's order, then what the write brought with them, which
	   a caller finds in the answer under its own key. What a coupling would have brought and
	   the settling took out fell with the member that brought it, whose answer says why, so
	   it is not answered again. Only the store can still refuse one written here, which
	   nothing beforehand can foresee. */
	for (size_t round = 0; round < 2; ++round)
	{
		const size_t count = round == 0 ? members.size() : batch.values.size();
		for (size_t i = 0; i < count; ++i)
		{
			Written w;
			const std::string *value = NULL;
			if (round == 0)
			{
				w.key = members[i].name;
				const coreapi::Error *no = NULL;
				for (size_t n = 0; no == NULL && n < refused.size(); ++n)
				{
					if (refused[n].first == w.key)
						no = &refused[n].second;
				}
				if (no != NULL)
				{
					w.failed = true;
					w.error = *no;
					w.depends_on = no->depends_on;
					w.code = httpStatus(w.error.status);
					all_ok = false;
					results.push_back(w);
					continue;
				}
				for (size_t b = 0; value == NULL && b < batch.values.size(); ++b)
				{
					if (batch.values[b].first == w.key)
						value = &batch.values[b].second;
				}
				// Left out by the settling as the value the box holds already: done.
				if (value == NULL)
				{
					results.push_back(w);
					continue;
				}
			}
			bool answered = false;
			if (round == 1)
			{
				/* A key the caller named is answered in the first round. One whose own value
				   was refused can still be here, put back by a coupling, as the name of the
				   channel an id written alone brings: that value is written, or the pair would
				   land in part, and the caller's refusal stays its answer. */
				w.key = batch.values[i].first;
				value = &batch.values[i].second;
				bool named = false;
				bool own_refused = false;
				for (size_t m = 0; !named && m < members.size(); ++m)
					named = members[m].name == w.key;
				for (size_t f = 0; named && !own_refused && f < refused.size(); ++f)
					own_refused = refused[f].first == w.key;
				if (named && !own_refused)
					continue;
				answered = named;
				/* What a coupling brought that the box holds already is no change: answered as
				   written and not written again, as the source's write does. */
				if (coreapi::settings::holdsStored(w.key, *value))
				{
					if (!answered)
						results.push_back(w);
					continue;
				}
			}

			coreapi::Result<void> done = coreapi::settings::set(w.key, *value, &batch);
			if (answered)
			{
				// The key has its answer; a store that failed here split the pair after all.
				if (!done.ok())
					all_ok = false;
				continue;
			}
			if (!done.ok())
			{
				w.failed = true;
				w.error = done.error();
				w.depends_on = done.error().depends_on;
				w.code = httpStatus(w.error.status);
				all_ok = false;
			}
			results.push_back(w);
		}
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
	HTTPD_SEGMENT_FROM_ASKED_SET("section", "the section's id, take it from GET /api/v1/settings/sections", &sectionNames),
};

const Param kSchemaParams[] = {
	HTTPD_QUERY_FROM_ASKED_SET("section", "only the settings of this section, as GET /api/v1/settings/sections names it", &sectionNames),
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
	HTTPD_SEGMENT_FROM_ASKED_SET("section", "the section's id, take it from GET /api/v1/settings/sections", &sectionNames),
	HTTPD_BODY_IS_MAP_OF("settings", ParamType::String,
		"one member per setting to write, named by its key as GET /api/v1/settings/schema names it, carrying the new value as text",
		1, (long) kMaxBodyMembers),
};

/* The two written out below answer ahead of the one that binds a segment, which is
   settled where the tables are read and not by the order here. A section called schema or
   sections would therefore be unreachable, and none of the sixteen the program declares
   is called either. */
const Param kClearParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("key", "the key of the credential to empty, as GET /api/v1/settings/schema names it", 256),
};

const RouteRefusal kSettingsSectionRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchName,
		"no settings are declared under a section of that name"),
};

const RouteRefusal kSettingsSchemaRefusals[] = {
	HTTPD_REFUSES(NotFound, NoSuchName,
		"no setting is declared under a section of that name"),
};

const RouteRefusal kClearSecretRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, NotACredential,
		"the setting is not a credential and is not cleared here"),
	HTTPD_REFUSES(InvalidArgument, BadString,
		"the credential's text rule does not take an empty value"),
	HTTPD_REFUSES(NotFound, UnknownSetting,
		"no setting is declared under that key"),
};

const RouteRefusal kSettingsWriteRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, DuplicateParameter,
		"the body names one setting twice and there is no saying which value was meant"),
	HTTPD_REFUSES(InvalidArgument, EmptyCredential,
		"the setting is a credential and is not cleared by writing nothing"),
	HTTPD_REFUSES(InvalidArgument, MissingParameter,
		"the body names no setting to write"),
	HTTPD_REFUSES(InvalidArgument, OutOfRange,
		"the setting takes 0 to 100"),
	HTTPD_REFUSES(NotFound, NoSuchName,
		"no settings are declared under a section of that name"),
	HTTPD_REFUSES(NotFound, UnknownSetting,
		"this section declares no setting under that key"),
	HTTPD_REFUSES(Conflict, SettingConditionNotMet,
		"the setting is one fact with another and is written together with it: startchanneltv_id"),
	HTTPD_REFUSES(NotFound, NoSuchChannel,
		"no channel with that id"),
	HTTPD_REFUSES(InvalidArgument, NotAListedValue,
		"the channel is a radio channel, and this start channel takes a television one"),
	HTTPD_REFUSES(Internal, ChannelListUnavailable,
		"the channel list could not be read"),
	HTTPD_REFUSES(InvalidArgument, ChoicesUnavailable,
		"the values this setting offers are the box's own and nothing has said what they are"),
	HTTPD_REFUSES(Conflict, SettingLocked,
		"the box's parental lock fixes this setting"),
	HTTPD_REFUSES(Conflict, SettingNotOnThisBox,
		"this box does not have what the setting controls"),
};

const Endpoint kSettingsEndpoints[] = {
	{ Method::Get, "/api/v1/settings/schema", AuthLevel::Read,
	  "every setting the box declares, and what each of them is",
	  "Lists every setting the box declares, independent of any one section: its key, what kind of "
	  "value it holds, which section it belongs to, the label and hint text to show beside it where "
	  "the locale catalog carries one, the bounds or choices it accepts, its default, whether changing "
	  "it needs a restart, and whether it is a credential. A setting marked a credential never carries "
	  "its real default here; its default is always reported as an empty string.\n"
	  "\n"
	  "Each item's `conditions` list states every comparison against another setting's current value "
	  "that must hold before this setting is worth showing; an empty list means it is always shown.\n"
	  "\n"
	  "`available` is false for a setting whose hardware this box lacks or cannot say it has; such a "
	  "setting is not worth showing and refuses every write, while its value still reads. `locked` is "
	  "true for a setting the box's parental lock fixes right now, which refuses every write too. Both "
	  "are left out where they do not apply.\n"
	  "\n"
	  "A member at its default is left out: `needs_restart`, `secret` and `listed` are false unless "
	  "the item says otherwise. A list of choices that several items carry alike is stated once in the "
	  "answer's `value_lists` object, and each such item names it in `values_from` instead of carrying "
	  "`values`; read its entries the same way.\n"
	  "\n"
	  "`section` narrows the list to one section, which keeps the answer small.\n"
	  "\n"
	  "**Refusals:**\n"
	  "- `404 no-such-name`: no setting is declared under a section of that name. Take section ids "
	  "from `GET /api/v1/settings/sections`.\n"
	  "\n"
	  "**Related:** `GET /api/v1/settings/sections`, `GET /api/v1/settings/{section}`, "
	  "`PATCH /api/v1/settings/{section}`.",
	  HTTPD_PARAMS(kSchemaParams), &kSettingListSchema, &settingsSchema, false,
	  Answers200, HTTPD_REFUSALS(kSettingsSchemaRefusals) },
	{ Method::Get, "/api/v1/settings/sections", AuthLevel::Read,
	  "the sections the settings are laid out in",
	  "Lists the sections the settings are grouped into, in the order the schema first names each one. "
	  "Each item carries only the section's id; the settings belonging to it are read with "
	  "`GET /api/v1/settings/{section}`, and a setting's own `section` member in the schema names the "
	  "same id.\n"
	  "\n"
	  "**Related:** `GET /api/v1/settings/schema`, `GET /api/v1/settings/{section}`.",
	  NULL, 0, &kSectionListSchema, &settingsSections, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/settings/keys", AuthLevel::Read,
	  "the remote control keys a key setting takes, with their names",
	  "Lists the keys the box has a name for and the name it shows for each: no key first, then each "
	  "named key, pressed and then held. A key setting stores the code, so a frontend shows the name "
	  "from this list and writes the code back. A setting may hold a code this list does not name, "
	  "because the box accepts the code of any key a remote control can send, and a frontend keeps "
	  "such a code rather than dropping it.\n"
	  "\n"
	  "**Related:** `GET /api/v1/settings/schema`, `PATCH /api/v1/settings/{section}`.",
	  NULL, 0, &kKeyNameListSchema, &settingsKeys, false,
	  Answers200, HTTPD_NO_REFUSALS },
	{ Method::Get, "/api/v1/settings/{section}", AuthLevel::Read,
	  "what one section's settings are set to",
	  "Reads every setting declared under one section and what it is currently set to. Each item "
	  "carries the setting's key and its value rendered as text, the same rendering a write accepts "
	  "back; a credential's value is always reported as an empty string, since reading one never shows "
	  "what is stored.\n"
	  "\n"
	  "**Refusals:**\n"
	  "- `404 no-such-name`: no setting is declared under a section of that name. Take section ids "
	  "from `GET /api/v1/settings/sections`.\n"
	  "\n"
	  "**Related:** `GET /api/v1/settings/schema`, `PATCH /api/v1/settings/{section}`.",
	  HTTPD_PARAMS(kSectionParams), &kValueListSchema, &settingsSection, false,
	  Answers200, HTTPD_REFUSALS(kSettingsSectionRefusals) },
	/* Written out and so answering ahead of the route that binds a segment, by the same
	   rule the two reads above do. A section called secret would be unreachable, and none of
	   the sixteen the program declares is called that. */
	{ Method::Post, "/api/v1/settings/secret/clear", AuthLevel::System,
	  "empties one credential, which is the one thing writing to it will not do",
	  "Empties one credential setting, which is the only way to clear one: writing an empty value to "
	  "it through `PATCH /api/v1/settings/{section}` is refused on purpose, so that a form redrawn "
	  "from a read that answered nothing cannot wipe the value by accident. The answer is the same "
	  "shape a section listing carries for one setting, and its `value` is empty, which is what the "
	  "read of a credential always shows.\n"
	  "\n"
	  "**Preconditions:** `key` must name a setting the schema marks `secret`, and it must be a string "
	  "typed setting, which every credential in this program is. A credential whose text rule wants "
	  "more than nothing, such as a pin of four digits, cannot be emptied.\n"
	  "\n"
	  "**Side effects:** takes the empty value at once, so a read answers with it from then on; the "
	  "box carries it into its running settings and saves its configuration file on its own loop, "
	  "and while a text input is open on the box it waits until that is closed.\n"
	  "\n"
	  "**Refusals:**\n"
	  "- `400 not-a-credential`: the setting is not a credential, or is a credential of a kind that has "
	  "nothing to clear; this route is not a second way to write any other setting.\n"
	  "- `400 bad-string`: the credential's text rule does not take an empty value, such as a pin.\n"
	  "- `404 no-such-setting`: no setting is declared under that key. Take keys from "
	  "`GET /api/v1/settings/schema`.\n"
	  "\n"
	  "**Related:** `GET /api/v1/settings/schema`, `PATCH /api/v1/settings/{section}`.",
	  HTTPD_PARAMS(kClearParams), &kValueSchema, &clearSecret, false,
	  Answers200, HTTPD_REFUSALS_AND_BODY(kClearSecretRefusals, "{\"key\":\"tmdb_api_key\"}") },
	{ Method::Patch, "/api/v1/settings/{section}", AuthLevel::System,
	  "writes settings of one section, answering the section as it reads now when every one of them landed and a result per key when they did not all agree",
	  "Writes one or more settings of a single section in one request. Every key is checked and, where "
	  "valid, taken before any answer is built, so a read answers with it from then on and a value "
	  "already landed is never rolled back by a later key failing. The box carries the values into its "
	  "running settings and saves its configuration file on its own loop right after, and while a text "
	  "input is open on the box it waits until that is closed.\n"
	  "\n"
	  "When every key lands, the answer is `200` and is exactly the section's own `GET` answer: the "
	  "section as it reads immediately afterward, which already reflects the new values, even though "
	  "carrying them into the running program and telling the daemons that hold them "
	  "happens afterward on the box's own loop. When more than one key was named "
	  "and at least one of them failed, the answer is `207` with a `results` object naming each key "
	  "and, for a failed one, its own status, `code` and `detail`; a request naming only one key that "
	  "failed is instead answered as that one refusal, at its own status.\n"
	  "\n"
	  "Two settings that are one fact in two parts carry `pair` and `pair_writes` in the schema. "
	  "The id of a start channel sent alone is taken, and the box writes the name beside it from its "
	  "channel list; that name is answered under its own key like a key that was sent. The name sent "
	  "alone is refused, and a name sent with the id that its own row refuses is answered so while "
	  "the id is taken as sent alone and the name of its channel is written beside it. The city and the location of the weather are refused when sent without each "
	  "other, and sending a place other than the one the box holds empties `weather_postalcode`; a "
	  "postal code sent with text in the same "
	  "request is refused together with the city. Turning `epg_save` on turns `epg_read` on in the "
	  "same write, answered under its own key, and `epg_read` sent off with it is refused together "
	  "with it. A change of `lcd4l_display_type` brings `lcd4l_brightness` and "
	  "`lcd4l_brightness_standby` down to what the new panel takes. Every value of a request is "
	  "checked before any of "
	  "them is taken, so a pair is not left half written by a value that could be refused beforehand; "
	  "only a failure to store can still split one. A setting refused because another one was is "
	  "answered `setting-condition-not-met`, and in a `207` its `depends_on` lists the settings that "
	  "took it down.\n"
	  "\n"
	  "A setting is also held to the `conditions` the schema states for it, the ones a screen of the "
	  "box greys the setting out by: while they do not hold, it is refused "
	  "`setting-condition-not-met`, and `depends_on` names the settings its conditions read that do "
	  "not allow it. An entry of `values` that carries conditions of its own is refused the same "
	  "way while they do not hold, with no `depends_on`. Both are judged on what the whole request leaves "
	  "behind, so a setting and the one that allows it can be sent together in either order; in "
	  "separate requests, the one that allows it goes first. A key refused for any reason counts as "
	  "not sent, so what depends on it is refused too. A key that passes every check and then fails "
	  "to be saved cannot be foreseen, so a setting depending on it may still land. A value equal "
	  "to the one the box holds, or to the default of a key it never stored, passes the conditions "
	  "of its setting, so a page may send a whole section back; for an entry with conditions of its "
	  "own, the default passes as well. Such a value is answered as written and not written again. "
	  "A credential is never taken as unchanged.\n"
	  "\n"
	  "A setting marked `needs_restart` in the schema takes effect only the next time the box "
	  "restarts, whatever this answers.\n"
	  "\n"
	  "**Preconditions:** the section must be one the schema declares, and every key named must belong "
	  "to that section.\n"
	  "\n"
	  "**Side effects:** takes the value of every key that passes its checks, also in a `207` "
	  "answer that names others as failed, and the box saves its configuration file when its own "
	  "loop carries them. A settings-changed event follows on `GET /api/v1/events` once the box's "
	  "own loop has saved a value that changed.\n"
	  "\n"
	  "**Refusals:**\n"
	  "- `400 duplicate-parameter`: the same setting key is named twice in the body; neither value is "
	  "written.\n"
	  "- `400 empty-credential`: a key marked a credential was sent an empty value; clear it with "
	  "`POST /api/v1/settings/secret/clear` instead.\n"
	  "- `400 missing-parameter`: the body names no setting at all.\n"
	  "- `400 choices-unavailable`: the setting's values are the box's own and the box cannot say "
	  "now what they are; only the value already stored and the default are taken then.\n"
	  "- `400 not-a-listed-value`: the value is not one the setting offers, among them the id of a "
	  "radio channel sent for the television start channel or the other way round.\n"
	  "- `400 out-of-range`: a whole number value falls outside the min and max the schema states for "
	  "that key, and is neither the value already stored nor the default, which stay writable after the "
	  "box moved a range.\n"
	  "- `404 no-such-name`: the section itself is not one the schema declares. Take section ids from "
	  "`GET /api/v1/settings/sections`.\n"
	  "- `404 no-such-channel`: the id sent alone for a start channel names no channel the box holds, "
	  "so there is no name to write beside it.\n"
	  "- `404 no-such-setting`: a named key does not belong to this section, whether or not it exists "
	  "elsewhere on the box.\n"
	  "- `409 setting-locked`: the schema marks the key `locked` (the parental lock); no write "
	  "changes it.\n"
	  "- `409 setting-not-on-this-box`: the schema marks the key not `available`; this box does not "
	  "have what it controls.\n"
	  "- `409 setting-condition-not-met`: the setting's `conditions` do not hold on what the request "
	  "leaves behind, or the entry the value names is offered only while other settings allow it, "
	  "or one half of a pair was sent without the other, together with a value that contradicts "
	  "it, or beside a partner that was refused; `depends_on` and `detail` name the other "
	  "settings, in a single refusal as in a `207`.\n"
	  "- `500 channel-list-unavailable`: the id of a start channel was sent alone and the channel list "
	  "could not be read to name it.\n"
	  "\n"
	  "**Related:** `GET /api/v1/settings/schema`, `GET /api/v1/settings/{section}`, "
	  "`POST /api/v1/settings/secret/clear`.",
	  HTTPD_PARAMS(kWriteParams), NULL, &settingsWrite, false,
	  Answers200 | Answers207, HTTPD_REFUSALS_AND_BODY(kSettingsWriteRefusals,
		"{\"auto_subs\":\"1\"}") },
};

} // namespace

extern const RouteTable settingsTable = {
	HTTPD_TABLE("settings", kSettingsEndpoints)
};

} // namespace httpd
