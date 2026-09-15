/*
 * schema.h - schema descriptions for the endpoint answers
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

#ifndef __httpd_schema_h__
#define __httpd_schema_h__

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace httpd
{

/* The shape of what an endpoint answers with, as data beside the route rather than as
   prose somewhere else.

   Here and not in coreapi, although the settings descriptor lives there: that one is
   where it is because coreapi::settings declares against it, and nothing in coreapi
   declares against this one. coreapi::ValueType can name neither an array nor an object
   nor a moment, and a type widened to carry both jobs would be one both layers have to
   agree about. Read only data with no constructor, so a table of these is a constant the
   linker writes. */
enum class FieldType
{
	Bool,
	Int,
	UInt,
	// A number that is not whole. Separate from Int and UInt because a reader
	// that turns the answer into a typed record picks a different member for
	// it, and because the writer prints one differently.
	Number,
	String,
	// Seconds since the epoch, written as a number. Named apart from Int so
	// that a reader can render a moment as one without being told which of the
	// numbers in an answer are moments.
	Time,
	// An identifier as the text it travels as: hexadecimal, and a string rather than a
	// number because it is sixty four bits wide and the numbers a browser parses out of JSON
	// carry fifty three. Named apart from String so a reader is told the shape of it.
	ChannelId,
	Object,
	Array
};

/* What the plain values of an Array are, for an Array that carries no shape. None is
   every member that is not such an array: an object's members are named by its shape,
   and a list of objects is described by the shape of one of them.

   Its own naming and not FieldType, although the kinds repeat: neither Object nor Array
   is an element this layer can describe, there being no second shape beside a member. */
enum class ElementType
{
	None,
	Bool,
	Int,
	UInt,
	Number,
	String,
	Time,
	ChannelId
};

struct Schema;

/* One member of an answer.

   optional says the member is absent from some answers rather than present and
   empty, and it is the exception here: an answer whose shape depends on what it
   carries makes a reader learn which members go together before it can read any of
   them. It is written where absence is the only truthful answer, which is a value
   the box could not read at all: nought is a real owner and a real size, so a
   listing that printed zeroes for a name it could not stat would be stating them.

   nested carries the shape of an Object member and of the elements of an Array
   whose elements are objects, and is NULL for everything else. An Array of plain
   values carries none, and what those values are is what element says.

   values is the whole of what the member can be, comma separated, and NULL for the
   many members whose text is not out of a set at all. Written only where the
   handler cannot answer anything else, and held to that by a guard beside the cases:
   a set stated here that this server may step outside of is worse than saying
   nothing, because a client generated from the document then turns down an answer
   the box was right to give.

   element says what the plain values of an Array are and is None for every other
   member. Without it a list of numbers and a list of words are the same row. */
/* A set of values this table cannot state, asked for where the document is written.
   Same shape as the one a parameter of a route uses and deliberately spelled again
   rather than shared: those two headers do not include one another, so a table of
   routes compiles without knowing how an answer is described. One function answers
   both wherever a value travels in both directions. */
typedef void (*AskedSet)(std::vector<std::string> &out);

struct FieldDesc
{
	const char   *name;
	FieldType     type;
	bool          optional;
	const char   *doc;
	const Schema *nested;
	const char   *values;
	ElementType   element;
	/* The set of a member whose set the build decides, which a table cannot write down: the
	   daemons this box drives are a list the layer below keeps, and a copy typed here would
	   be a second list to keep in step. */
	AskedSet      asks;
};

struct Schema
{
	const char      *name;
	const FieldDesc *fields;
	size_t           count;
};

/* Writes a table's field array and its count as one pair, so the two cannot
   disagree. Written by hand they can, and a count one too large reads a
   FieldDesc that is not there, which nothing at run time can see. */
#define HTTPD_FIELDS(a) (a), (sizeof(a) / sizeof((a)[0]))

/* One member of an answer, written as what that member is.

   A row written out field by field is a row that every later field of FieldDesc has
   to be added to by hand, and the build refuses such rows one at a time: the rows
   the server and the cases hold between them are a hundred and seventy eight.

   Named after what the member is and not after the shape of the row, because a row
   reaching for the wrong one of these is otherwise as silent as a field left out: a
   list of objects and a list of plain values are the same five fields in the same
   order. Where the two can be told apart from the row alone they are, so the check
   over a shape refuses a list of plain values that does not say what its values are
   and an object carrying no shape. */
#define HTTPD_MEMBER(name, type, doc) \
	(name), (type), false, (doc), NULL, NULL, httpd::ElementType::None, NULL

/* A member some answers leave out. The exception, and the one a reader has to
   test for before it reads it: absence is written where it is the only
   truthful answer, which is a value the box could not read at all. */
#define HTTPD_MEMBER_OPTIONAL(name, type, doc) \
	(name), (type), true, (doc), NULL, NULL, httpd::ElementType::None, NULL

/* A member whose text is one of a stated set, and never anything else. Written only
   where the handler cannot answer outside the set, which is what check-answer-sets.sh
   holds every one of these to: a set the server may step outside of makes a generated
   client refuse an answer the box was right to give. */
#define HTTPD_MEMBER_OF_SET(name, set, doc) \
	(name), httpd::FieldType::String, false, (doc), NULL, (set), httpd::ElementType::None, NULL

// A member that is an object of a named shape. Always a named one: an object
// with no shape beside it is a member a reader is told nothing about.
#define HTTPD_OBJECT(name, shape, doc) \
	(name), httpd::FieldType::Object, false, (doc), (shape), NULL, httpd::ElementType::None, NULL

// A member that is a list of objects of a named shape.
#define HTTPD_LIST_OF(name, shape, doc) \
	(name), httpd::FieldType::Array, false, (doc), (shape), NULL, httpd::ElementType::None, NULL

// The same, left out of some answers.
#define HTTPD_LIST_OF_OPTIONAL(name, shape, doc) \
	(name), httpd::FieldType::Array, true, (doc), (shape), NULL, httpd::ElementType::None, NULL

// A member that is a list of plain values, which says what those values are
// because there are no members to name them.
#define HTTPD_LIST_OF_VALUES(name, element, doc) \
	(name), httpd::FieldType::Array, false, (doc), NULL, NULL, (element), NULL

/* A member whose text is one of a set the build decides, which a table cannot write
   down.

   The counterpart of the same thing on a route's own segment, and here for the same
   reason: where a value the box answers is the same value a route takes back, saying
   so in one of the two places and not the other leaves a reader with nothing to hold
   the two together. Asked rather than copied, so the set a reader is handed cannot
   be a second list gone stale.

   Not held by check-answer-sets.sh, and not needing to be: that guard exists because
   a set typed into a row and the function that writes the member are two texts that
   can drift, and here they are one function. */
#define HTTPD_MEMBER_FROM_ASKED_SET(name, doc, asks) \
	(name), httpd::FieldType::String, false, (doc), NULL, NULL, httpd::ElementType::None, (asks)

/* A member spelled out field by field, for a case about the row itself: a set beside a
   kind that is not text, a list of plain values that says nothing about them, an object
   carrying no shape. Every one of those is a row the macros above cannot write. Nothing
   that means its row to be right should reach for this. */
#define HTTPD_MEMBER_AS_WRITTEN(name, type, optional, doc, shape, set, element) \
	(name), (type), (optional), (doc), (shape), (set), (element), NULL

/* Whether a schema is one this layer wrote right. A schema nothing checks is decoration:
   it is read by no handler, so a member renamed in the writer and left alone here would
   drift with nothing said about it. The cases that hold an answer to its schema ask this
   first, so a schema wrong in itself is reported as that.

   why, when it is not null, is left naming what was found wrong, always a string literal,
   so this allocates nothing and can be called from a consumer built without exceptions.
   depth bounds the walk rather than trusting the tables not to name each other. */
inline bool schemaIsSane(const Schema &s, const char **why = NULL, size_t depth = 0)
{
	const size_t kMaxDepth = 8;

	if (why != NULL)
		*why = "";
	if (depth > kMaxDepth)
	{
		if (why != NULL)
			*why = "the schemas name each other deeper than an answer nests";
		return false;
	}
	if (s.name == NULL || s.name[0] == '\0')
	{
		if (why != NULL)
			*why = "the schema has no name";
		return false;
	}
	if (s.count > 0 && s.fields == NULL)
	{
		if (why != NULL)
			*why = "the schema counts fields it does not carry";
		return false;
	}
	if (s.count == 0)
	{
		// An answer with no members is one no reader could do anything with,
		// and a count left at nought beside a filled array is the mistake the
		// pair macro exists to stop.
		if (why != NULL)
			*why = "the schema describes no members";
		return false;
	}

	for (size_t i = 0; i < s.count; ++i)
	{
		const FieldDesc &f = s.fields[i];
		if (f.name == NULL || f.name[0] == '\0')
		{
			if (why != NULL)
				*why = "a member has no name";
			return false;
		}
		if (f.doc == NULL || f.doc[0] == '\0')
		{
			if (why != NULL)
				*why = "a member says nothing about itself";
			return false;
		}

		for (size_t j = 0; j < i; ++j)
		{
			if (s.fields[j].name != NULL && std::strcmp(s.fields[j].name, f.name) == 0)
			{
				if (why != NULL)
					*why = "a member is named twice";
				return false;
			}
		}

		const bool may_carry = (f.type == FieldType::Object || f.type == FieldType::Array);
		if (f.nested != NULL && !may_carry)
		{
			if (why != NULL)
				*why = "a member carries a shape and is not an object or an array";
			return false;
		}
		if (f.nested == NULL && f.type == FieldType::Object)
		{
			if (why != NULL)
				*why = "a member is an object and carries no shape";
			return false;
		}

		/* What a list carries is said once. A list of objects is described by
		   the shape beside it, a list of plain values by its element, and a
		   row that says both is a row two readers would read differently. */
		const bool plain_list = (f.type == FieldType::Array && f.nested == NULL);
		if (f.element != ElementType::None && !plain_list)
		{
			if (why != NULL)
				*why = "a member says what its elements are and is not a list of plain values";
			return false;
		}
		if (f.element == ElementType::None && plain_list)
		{
			if (why != NULL)
				*why = "a member is a list of plain values and says nothing about them";
			return false;
		}

		/* One set or the other and never both. A member that listed its values and named
		   something to ask as well would be two sets, and a reader has no way to tell which of
		   them the box holds to. */
		if (f.asks != NULL)
		{
			if (f.values != NULL)
			{
				if (why != NULL)
					*why = "a member states a set and names one to ask for as well";
				return false;
			}
			if (f.type != FieldType::String)
			{
				if (why != NULL)
					*why = "a member names a set to ask for and is not text";
				return false;
			}
		}

		/* A set is the whole of what a member can be, so it belongs to a member that is text.
		   Beside a number or a moment it would be a second opinion about a value the kind already
		   describes, and beside an identifier it would be a list and not a kind. */
		if (f.values != NULL)
		{
			if (f.type != FieldType::String)
			{
				if (why != NULL)
					*why = "a member states a set and is not text";
				return false;
			}
			if (f.values[0] == '\0')
			{
				if (why != NULL)
					*why = "a member states a set with nothing in it";
				return false;
			}
			for (const char *q = f.values;;)
			{
				const char *e = std::strchr(q, ',');
				const size_t n = (e != NULL) ? (size_t)(e - q) : std::strlen(q);
				if (n == 0)
				{
					if (why != NULL)
						*why = "a member states a set with an empty value in it";
					return false;
				}
				// A value named twice is a set that says the same thing twice,
				// and a reader turning one into a type has two of one name.
				for (const char *r = f.values; r < q; )
				{
					const char *re = std::strchr(r, ',');
					const size_t rn = (re != NULL) ? (size_t)(re - r) : std::strlen(r);
					if (rn == n && std::strncmp(r, q, n) == 0)
					{
						if (why != NULL)
							*why = "a member states one value of its set twice";
						return false;
					}
					if (re == NULL)
						break;
					r = re + 1;
				}
				if (e == NULL)
					break;
				q = e + 1;
			}
		}

		if (f.nested != NULL && !schemaIsSane(*f.nested, why, depth + 1))
			return false;
	}

	return true;
}

} // namespace httpd

#endif
