/*
 * schema.h - field and schema descriptions shared by the tables
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

#ifndef __coreapi_schema_h__
#define __coreapi_schema_h__

#include <cstddef>
#include <string>
#include <vector>

// The program's settings, named and not included: a header the whole layer
// reads would otherwise carry them with it.
struct SNeutrinoSettings;

namespace coreapi
{

enum class ValueType
{
	Bool,
	Int,
	String,
	Enum
};

// One choice an Enum offers. The number is what the setting stores and
// label_key names the text a frontend shows for it, so the wording can move
// without the stored value moving with it.
struct EnumValue
{
	int         value;
	const char *label_key;
};

/* The text is what a person reads and not a name for the catalog: these lists
   are the screens' own and half of them carry words the catalog has no name
   for, which is why this is not an EnumValue. A value and not a range, because
   what the box offers is a set with holes in it. */
struct SettingChoice
{
	long        value;
	std::string label;

	SettingChoice() : value(0) {}
};

// Each ordering comparison comes in both strengths, because a closed range
// written as a strict comparison against the next value assumes the values are
// contiguous and nothing checks that. In reads a value list, so a setting shown
// for either of two modes is one condition over two numbers.
enum class CompareOp
{
	Eq,
	Ne,
	Lt,
	Le,
	Gt,
	Ge,
	In
};

// One comparison against the current value of another setting, read as a
// conjunction with the rest, so anything the menus express with a chain of ands
// is declarable while the type stays a comparison and not an expression
// language: no grammar, no evaluator, checkable on either side of the API.
//
// Two things it cannot say, and the menus have both: a condition that calls a
// function, which would need an evaluator reaching into the GUI, and a
// disjunction across two keys, which In does not give either because it widens
// one key rather than joining two. A setting turning on either carries no
// condition and is always shown, and both kinds are counted and reported as the
// sections are declared rather than quietly absorbed.
struct Condition
{
	const char *key;
	CompareOp   op;
	long        value;
	const long *values;
	size_t      value_count;
};

/* Where a setting's value lives in the program, as functions that read and
   write it. Between a load and a save the settings struct is the value in
   effect while the settings file is a copy of what was last written, so a
   setting is located by its field and not by its key in that file.

   Functions and not a member pointer, because the fields are of several types.
   The pair a row writes is generated for the field's own type by the macros in
   settingsfield.h, so a row naming a field of the wrong sort does not compile,
   and the write answers whether the value survived the field's width. Numbers
   and text are separate pairs because a long cannot carry a string.

   origin is here because the checks outside the compiler read it: each holds a
   row to the settings file, and for a row of any kind but Member the value is
   not in that file. */
enum class FieldOrigin
{
	// A setting whose value this layer cannot reach at all.
	Nowhere,
	// A field of the settings struct, which is what most rows are.
	Member,
	/* One bit of a field beside it. The screen splits the mask into a question
	   per bit on the way in and folds them back on the way out, so the bits are
	   what a person is offered and the mask is what is stored. */
	MaskBit,
	/* A sixty four bit identifier the struct holds, carried as text because a
	   long on the box is half that wide.

	   Field on the end, although a scoped enumerator reaches nobody who did not
	   spell the enum: gcc warns that the bare name shadows the type of the same
	   name in types.h, on every build of every box, and a warning nobody can act
	   on is one everybody learns to read past. */
	ChannelIdField,
	/* A value the program does not keep at all: a daemon holds it, and what the
	   struct has under that name is the buffer a screen fills when it opens.
	   Reading the buffer answers whatever was last left in it. */
	Service
};

struct FieldRef
{
	long (*read_number)(const SNeutrinoSettings &);
	void (*write_number)(SNeutrinoSettings &, long);
	// Whether the value survives the field's own type, which is narrower than a
	// long for every one of them. Asked before the value is taken, because what
	// takes it runs later and on another thread, where a refusal reaches nobody.
	bool (*fits_number)(long);
	void (*read_text)(const SNeutrinoSettings &, std::string &);
	void (*write_text)(SNeutrinoSettings &, const std::string &);
	/* The pair for a value a daemon holds. They take no settings struct and they
	   answer whether the daemon could be reached: a read of a field cannot fail
	   and one of these can. Called with no lock of this layer held, reaching the
	   daemon being a blocking exchange. */
	bool (*ask)(long &);
	bool (*tell)(long);
	/* Where the values the setting offers are asked for, NULL for a row whose
	   own list is the whole of them. The box numbers these itself, so a list
	   written down here would be wrong for the next box model. */
	const char *choices;
	/* The member the row is found under, as data, so what a row stands for can
	   be compared against what the program loads the row's key into: the
	   functions above carry a field but not its name, and a check outside the
	   compiler cannot read a pointer to a member.

	   For most rows this is the field the value is in. For a bit of a mask it is
	   the member the screen binds the question to, the value living in the mask
	   beside it. */
	const char *name;
	FieldOrigin origin;
};

/* Whether the value is in the member the row is named after. Two things follow
   from a false answer and both are held to elsewhere: the settings file carries
   no key for such a row, and no notifier may be run for it, each of them
   reading a member this layer never wrote. */
inline bool valueIsInNamedMember(const FieldRef &f)
{
	return f.origin == FieldOrigin::Member || f.origin == FieldOrigin::ChannelIdField;
}

// What a setting whose value this layer cannot reach writes.
#define COREAPI_NO_FIELD \
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, coreapi::FieldOrigin::Nowhere }

/* Hexadecimal, one to sixteen digits, no prefix. Read in either case and
   written back in lower. Beside the type rather than beside the field it stores
   into, because the rule that refuses a spelling and the store that takes a
   good one have to be one rule: written twice they drift, and a pair that has
   drifted takes a value one of them allowed and stores what the other made of
   it. False leaves out untouched. */
inline bool readChannelIdText(const std::string &text, unsigned long long &out)
{
	// Nothing names no channel, and seventeen digits taken as the last sixteen
	// would name whichever channel those happen to spell.
	if (text.empty() || text.size() > 16)
		return false;

	unsigned long long id = 0;
	for (size_t i = 0; i < text.size(); ++i)
	{
		const char c = text[i];
		int digit = -1;
		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = 10 + (c - 'a');
		else if (c >= 'A' && c <= 'F')
			digit = 10 + (c - 'A');
		if (digit < 0)
			return false;
		id = (id << 4) | (unsigned long long) digit;
	}

	out = id;
	return true;
}

// An aggregate with no constructor of its own, so a table of these is a
// constant in read only memory rather than something a startup routine builds.
//
// No conditions means always shown. An empty default_string is a default while
// NULL is the absence of one. label_key may be NULL, which says the program has
// no name for the setting rather than that the row forgot one: most of what the
// settings file holds is offered by no screen anywhere, and naming a locale
// that belongs to a neighbouring item reads as right on the page and is wrong
// on the screen. An empty string stays refused.
//
// needs_restart is here from the start rather than added the day a setting
// needs it: an aggregate initialiser that stops short of a member leaves it
// zero, so every table written before the member existed would go on claiming
// its settings take effect at once.
//
// secret says the value is a credential and is never answered by a read. The
// row stays in the schema, so a frontend knows the key is there.
struct Descriptor
{
	const char      *key;
	ValueType        type;
	const char      *section;
	const char      *label_key;
	const char      *hint_key;
	long             min;
	long             max;
	const EnumValue *values;
	size_t           value_count;
	long             default_int;
	const char      *default_string;
	bool             needs_restart;
	bool             secret;
	const Condition *conditions;
	size_t           condition_count;
	// Where the value is, which every row writes even when it is nowhere.
	FieldRef         field;
};

// Each writes a table's array and its count as one pair, so the two cannot
// disagree: a count one too large reads an element nothing at run time can see.
#define COREAPI_ENUM(a) (a), (sizeof(a) / sizeof((a)[0]))
#define COREAPI_CONDITIONS(a) (a), (sizeof(a) / sizeof((a)[0]))
#define COREAPI_VALUES(a) (a), (sizeof(a) / sizeof((a)[0]))

// What a setting that is always shown writes, so no row spells out an empty list.
#define COREAPI_ALWAYS NULL, 0

// Answers for the descriptor itself and not for a value offered against it.
// Inline and free of any throwing construct, because consumers built without
// exceptions include this header.
//
// An Int needs no rule against inverted bounds and an Enum none against an
// empty list: neither can hold the default that is checked below, so both rules
// are implied by that one check and by nothing else.
inline bool descriptorIsSane(const Descriptor &d)
{
	if (d.key == NULL || d.key[0] == '\0')
		return false;
	if (d.section == NULL || d.section[0] == '\0')
		return false;
	if (d.label_key != NULL && d.label_key[0] == '\0')
		return false;

	// A count without the array it counts reads memory that is not there. Unlike
	// an Enum, a count of none is the ordinary case and means always shown.
	if (d.condition_count > 0 && d.conditions == NULL)
		return false;
	for (size_t i = 0; i < d.condition_count; ++i)
	{
		const Condition &c = d.conditions[i];
		if (c.key == NULL || c.key[0] == '\0')
			return false;
		if (c.op == CompareOp::In && (c.values == NULL || c.value_count == 0))
			return false;
	}

	/* A field is reached by a whole set of functions or by none. Part of a set
	   would answer a read and drop the write beside it, and no caller could tell
	   that from a setting that cannot be written at all. */
	if ((d.field.read_number == NULL) != (d.field.write_number == NULL))
		return false;
	if ((d.field.read_number == NULL) != (d.field.fits_number == NULL))
		return false;
	if ((d.field.read_text == NULL) != (d.field.write_text == NULL))
		return false;
	if ((d.field.ask == NULL) != (d.field.tell == NULL))
		return false;

	/* A field reached but not named is one no check outside the compiler can
	   read, and a name without a field is a name nothing holds to. */
	bool located = d.field.read_number != NULL || d.field.read_text != NULL ||
	               d.field.ask != NULL;
	if (located != (d.field.name != NULL && d.field.name[0] != '\0'))
		return false;

	/* The kind and the functions say the same thing twice, and a row where they
	   disagree is held to the wrong thing: every check outside the compiler
	   reads the kind, and what the layer calls is the functions. */
	switch (d.field.origin)
	{
		case FieldOrigin::Nowhere:
			if (located)
				return false;
			break;

		case FieldOrigin::Member:
		case FieldOrigin::MaskBit:
			if (d.field.read_number == NULL && d.field.read_text == NULL)
				return false;
			if (d.field.ask != NULL)
				return false;
			break;

		case FieldOrigin::ChannelIdField:
			// Sixty four bits do not fit the long a number travels in here, so
			// the identifier is carried as the text a channel is named by.
			if (d.field.read_text == NULL || d.field.read_number != NULL)
				return false;
			break;

		case FieldOrigin::Service:
			if (d.field.ask == NULL)
				return false;
			if (d.field.read_number != NULL || d.field.read_text != NULL)
				return false;
			break;
	}

	/* A String's value is text and every other kind's is a number, so a row
	   whose field is of the other sort is one no read could answer. A daemon is
	   asked for a number, so a String row cannot be answered by one either. */
	if (d.type == ValueType::String)
	{
		if (d.field.read_number != NULL || d.field.ask != NULL)
			return false;
	}
	else if (d.field.read_text != NULL)
		return false;

	// Only a choice has a set of values, so only a choice can have one that is
	// asked for rather than written down.
	if (d.field.choices != NULL)
	{
		if (d.type != ValueType::Enum)
			return false;
		if (d.field.choices[0] == '\0')
			return false;
		/* Both at once would be two answers to one question, and which of them
		   a reader took would decide what the setting offers. */
		if (d.values != NULL || d.value_count != 0)
			return false;
	}

	switch (d.type)
	{
		case ValueType::Bool:
			return d.default_int == 0 || d.default_int == 1;

		case ValueType::Int:
			return d.default_int >= d.min && d.default_int <= d.max;

		case ValueType::String:
			return d.default_string != NULL;

		case ValueType::Enum:
			/* A row whose values are asked for cannot be held to them here: what
			   the box offers is known where the box is, and this header is read
			   by builds that never reach one. */
			if (d.field.choices != NULL)
				return true;
			if (d.values == NULL)
				return false;
			for (size_t i = 0; i < d.value_count; ++i)
			{
				if (d.values[i].value == d.default_int)
					return true;
			}
			return false;
	}

	// A type is an int with a fixed set of names, not a promise that a table
	// wrote one of them.
	return false;
}

// How the evaluator below reads the current value of another setting. A plain
// function pointer, so nothing here allocates, and context is handed back
// untouched. False means the setting is not known, which is not a value of
// zero. read is called once per condition and nothing caches.
struct ValueLookup
{
	bool (*read)(const char *key, long *value, void *context);
	void *context;
};

// Whether every condition a descriptor carries holds, which is whether the
// setting is worth showing. A conjunction, and a descriptor carrying none holds
// trivially. In holds when the current value is one of its list.
//
// A condition nobody can answer for does not hide the setting: an unknown key, a
// lookup that reads nothing, and a list or an operator a table got wrong all
// leave it shown. A setting shown where it does not apply is a nuisance; one
// hidden where it does apply cannot be found at all. A frontend that reads the
// list itself and disagrees with this function is wrong.
inline bool conditionsHold(const Descriptor &d, const ValueLookup &lookup)
{
	if (d.conditions == NULL || lookup.read == NULL)
		return true;

	for (size_t i = 0; i < d.condition_count; ++i)
	{
		const Condition &c = d.conditions[i];

		long current = 0;
		if (!lookup.read(c.key, &current, lookup.context))
			continue;

		bool holds = true;
		switch (c.op)
		{
			case CompareOp::Eq:
				holds = current == c.value;
				break;

			case CompareOp::Ne:
				holds = current != c.value;
				break;

			case CompareOp::Lt:
				holds = current < c.value;
				break;

			case CompareOp::Le:
				holds = current <= c.value;
				break;

			case CompareOp::Gt:
				holds = current > c.value;
				break;

			case CompareOp::Ge:
				holds = current >= c.value;
				break;

			case CompareOp::In:
				// A list that is not there cannot be missed.
				holds = c.values == NULL;
				for (size_t v = 0; !holds && v < c.value_count; ++v)
					holds = c.values[v] == current;
				break;
		}

		if (!holds)
			return false;
	}

	return true;
}

} // namespace coreapi

#endif
