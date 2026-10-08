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
	Enum,
	// A key code of the remote control, stored as the number the input layer
	// names the key by and shown by the name the box shows for it. Its bounds
	// are the codes the field holds; which codes are keys is asked at the write.
	Key
};

struct Condition;

// One choice an Enum offers. The number is what the setting stores and
// label_key names the text a frontend shows for it, so the wording can move
// without the stored value moving with it.
struct EnumValue
{
	int         value;
	const char *label_key;
	// Set instead of label_key where the screen names the entry without a locale.
	const char *label_text;
	// NULL is always offered. Evaluated per request, so it may ask the box.
	bool      (*available)();
	// The entry is offered only while every one of these holds, NULL for always.
	// Not read yet: the rows leave it empty until the lists that depend on
	// another setting are declared.
	const Condition *when;
	size_t            when_count;
};

// One test for whether an entry is shown, so the lists built from a row agree.
inline bool entryOffered(const EnumValue &e)
{
	return e.available == NULL || e.available();
}

/* One value a setting offers, with its label already the text the box shows.
   A value and not a range, because what the box offers is a set with holes in
   it. */
struct SettingChoice
{
	long        value;
	std::string label;
	// The locale key when the entry has one, empty for fixed text.
	std::string label_key;
	// What a String row stores for this entry. A number row leaves it empty and
	// a String row leaves value alone, since neither kind has a use for the other.
	std::string text;

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
	In,
	// The other setting's text is filled in and is not the placeholder named
	// beside it, which is how a key nobody has entered yet is told apart.
	TextValid
};

// One comparison against the current value of another setting, read as a
// conjunction with the rest, so anything the menus express with a chain of ands
// is declarable while the type stays a comparison and not an expression
// language: no grammar, no evaluator, checkable on either side of the API.
//
// A disjunction across keys is a group: an entry naming no key and listing its
// alternatives, which holds when any one of them does. One level only, so the
// list stays a conjunction of comparisons and alternatives of comparisons and
// never grows into an expression.
//
// What it still cannot say is a condition that calls a function, which would
// need an evaluator reaching into the GUI. A setting turning on one carries no
// condition and is always shown, and such settings are counted and reported as
// the sections are declared rather than quietly absorbed.
struct Condition
{
	const char      *key;
	CompareOp        op;
	long             value;
	const long      *values;
	size_t           value_count;
	// The placeholder TextValid holds the text against, NULL for every other op.
	const char      *text;
	// A group's alternatives, NULL for a single comparison.
	const Condition *any_of;
	size_t           any_count;
};

/* The other way a setting is offered where the box lacks what its row
   describes: another kind, label and set of values over the same field. The
   rest of the row, its key, default, conditions and field, is the same in
   both. The hint is the row's unless the shape names one.

   offered is for a row the box may lack altogether as well as have in two
   shapes: NULL for every box that lacks the first shape to have this one, and
   where set, this shape is offered only while it holds, so the row is in none
   of its shapes otherwise. Without it a second test on the row would have to
   stand for both questions, and the box lacking the first shape would be
   shown the second one on every board that has no panel at all. */
struct Shape
{
	ValueType        type;
	const char      *label_key;
	long             min;
	long             max;
	const EnumValue *values;
	size_t           value_count;
	const char      *hint_key;
	bool           (*offered)();
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
	/* Whether the box has what the setting controls, NULL for every box. Asked
	   per request and false when the box cannot say. Where it says no, the
	   setting takes the shape below, or without one it is not on this box: no
	   screen offers it and a write is refused, while its value still reads.
	   Here and not beside the row's kind because the macros that write a field
	   are the one place every row spells out in full. */
	bool (*available)();
	const Shape *otherwise;
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
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, coreapi::FieldOrigin::Nowhere, \
	  NULL, NULL }

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

/* What a String setting accepts, as data, so the layer that stores it and every
   frontend that offers it hold to the same rule. A row without one is plain
   text. */
enum class TextKind
{
	Plain,
	Directory,
	File,
	Pin,
	Host,
	NumberAsText,
	NameFromList,
	PathList
};

enum class MustExist
{
	No,
	Yes,
	// Exists, and is not on a memory backed file system.
	YesNotTmpfs,
	// Exists, and is not on flash storage; a memory backed file system is fine.
	YesNotFlash
};

struct TextRule
{
	TextKind    kind;
	// Shortest text taken, in bytes; 0 is no minimum. A PIN that is exactly four
	// digits says four here and in max_length.
	size_t      min_length;
	// Longest text taken, in bytes; 0 is no limit.
	size_t      max_length;
	// The only characters taken, NULL for any.
	const char *allowed;
	MustExist   must_exist;
	// The file name endings taken for a File, separated by commas and written
	// without the dot, NULL for any.
	const char *extensions;
	/* Whether the row may be left empty although it names a place or a file.
	   Only a row whose screen really lets the field empty says so; every other
	   row that names a place needs one. */
	bool        allow_empty;
};

// What a rule finds wrong with a text on its own, before any file is looked at.
enum class TextFault
{
	None,
	TooShort,
	TooLong,
	BadCharacter
};

/* The part of a rule that needs only the text, so the check a write is held to
   and the check a row's default is held to are one function. */
inline TextFault textFault(const TextRule &rule, const std::string &value)
{
	if (value.size() < rule.min_length)
		return TextFault::TooShort;
	if (rule.max_length != 0 && value.size() > rule.max_length)
		return TextFault::TooLong;
	if (rule.allowed != NULL && value.find_first_not_of(rule.allowed) != std::string::npos)
		return TextFault::BadCharacter;
	return TextFault::None;
}

// The values a setting offers right now, for a setting whose list is not
// fixed. False where the box cannot say. NULL in a row means the list is the
// row's own.
typedef bool (*ChoiceSource)(std::vector<SettingChoice> &out);

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
//
// values are an Enum's choices, a Bool's two words, or the values an Int shows
// in words instead of as a number.
//
// The members after field are all "none" in every row for now and say nothing
// until something reads them. The order of all the members is written out once,
// in RowBuilder below, and a table never lists them by position: that is why a
// member can be added here without a row being touched. unit_key names the
// locale text that follows the number and format_key one that holds the number
// itself as %d, for formats that are more than a unit; a row names at most one
// of the two. A frontend draws the unit beside the number and the format in
// its place, and the schema answers the unit's key and not the format's, whose
// %d only a box's own screen can fill. min_now and max_now answer the bound when
// the constant is not the whole story and the constant stays the fallback.
//
// default_fn is the number a box falls back to when it is not the same on every
// box, or depends on what the box has, for a Bool, an Int or an Enum. The
// constant stays what a box without an exception falls back to and is what the
// sanity test reads, because the function may ask the box and a test of the
// table has no box to ask. Everything that needs the default goes through
// defaultInt() so none of them reads the constant where the box says another.
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
	const char      *unit_key;
	const char      *format_key;
	const TextRule  *text;
	ChoiceSource     choices_from;
	long           (*min_now)();
	long           (*max_now)();
	long           (*default_fn)();
};

// The number a setting falls back to on this box.
inline long defaultInt(const Descriptor &d)
{
	return d.default_fn != NULL ? d.default_fn() : d.default_int;
}

// Each writes an array and its count as one pair, so the two cannot disagree: a
// count one too large reads an element nothing at run time can see. The tables
// go through the builders below; these are for a row written out by position,
// as the cases do.
#define COREAPI_ENUM(a) (a), (sizeof(a) / sizeof((a)[0]))
#define COREAPI_CONDITIONS(a) (a), (sizeof(a) / sizeof((a)[0]))
#define COREAPI_VALUES(a) (a), (sizeof(a) / sizeof((a)[0]))
#define COREAPI_ANY(a) (a), (sizeof(a) / sizeof((a)[0]))

// What a setting that is always shown writes, so no row spells out an empty list.
#define COREAPI_ALWAYS NULL, 0

/* How a table writes its rows, entries and conditions, so that no table spells
   a member's position and the order of the members is written out in this one
   place and nowhere else. Every call but the first of a row names what it
   sets; the one unnamed value is the key, or an entry's stored value.

     boolRow("key") .section("s") .label("k") .hint("k") .defaultValue(1) ... .field(...)

   The same holds for the other kinds: intRow, enumRow, textRow and keyRow.
   A row of a kind the table never uses has no builder yet and gets
   one with its first row. The call that ends a row is field(): it is the only
   one that returns a Descriptor, so a row that forgot it is a compile error and
   not a setting nobody can reach. availableIf() is for a row the box may lack and has to
   come before field(), which takes the test with it; a row whose field macro
   already names one keeps it unless availableIf() was called.

   Each builder is a single return of a constructed value, which is all C++11
   allows a constexpr function, so a table written with them is a constant the
   compiler has evaluated and not something a startup routine builds. The
   builders hand out copies and change nothing in place for the same reason.

   An entry is option(value) with label() or text() and, where it has them,
   availableIf() and offeredWhen(). A condition is when(key) with one of is,
   isNot, below, atMost, above, atLeast, oneOf or textValid, and anyOf() names a
   group whose alternatives are a list of such conditions. A list of entries or
   conditions stays an array a table names, because C++11 has no way to build
   one inline. */
class OptionBuilder
{
public:
	explicit constexpr OptionBuilder(int value) : e_{ value, NULL, NULL, NULL, NULL, 0 } {}

	constexpr OptionBuilder label(const char *key) const
	{
		return OptionBuilder(EnumValue{ e_.value, key, e_.label_text, e_.available, e_.when, e_.when_count });
	}

	constexpr OptionBuilder text(const char *words) const
	{
		return OptionBuilder(EnumValue{ e_.value, e_.label_key, words, e_.available, e_.when, e_.when_count });
	}

	constexpr OptionBuilder availableIf(bool (*test)()) const
	{
		return OptionBuilder(EnumValue{ e_.value, e_.label_key, e_.label_text, test, e_.when, e_.when_count });
	}

	template <size_t N>
	constexpr OptionBuilder offeredWhen(const Condition (&list)[N]) const
	{
		return OptionBuilder(EnumValue{ e_.value, e_.label_key, e_.label_text, e_.available, list, N });
	}

	constexpr operator EnumValue() const { return e_; }

private:
	explicit constexpr OptionBuilder(const EnumValue &e) : e_(e) {}

	EnumValue e_;
};

constexpr OptionBuilder option(int value)
{
	return OptionBuilder(value);
}

class WhenBuilder
{
public:
	explicit constexpr WhenBuilder(const char *key) : key_(key) {}

	constexpr Condition is(long v) const { return compare(CompareOp::Eq, v); }
	constexpr Condition isNot(long v) const { return compare(CompareOp::Ne, v); }
	constexpr Condition below(long v) const { return compare(CompareOp::Lt, v); }
	constexpr Condition atMost(long v) const { return compare(CompareOp::Le, v); }
	constexpr Condition above(long v) const { return compare(CompareOp::Gt, v); }
	constexpr Condition atLeast(long v) const { return compare(CompareOp::Ge, v); }

	template <size_t N>
	constexpr Condition oneOf(const long (&list)[N]) const
	{
		return Condition{ key_, CompareOp::In, 0, list, N, NULL, NULL, 0 };
	}

	// The placeholder is the text the setting holds before anybody entered one.
	constexpr Condition textValid(const char *placeholder) const
	{
		return Condition{ key_, CompareOp::TextValid, 0, NULL, 0, placeholder, NULL, 0 };
	}

private:
	constexpr Condition compare(CompareOp op, long v) const
	{
		return Condition{ key_, op, v, NULL, 0, NULL, NULL, 0 };
	}

	const char *key_;
};

constexpr WhenBuilder when(const char *key)
{
	return WhenBuilder(key);
}

// One alternative group: it holds when any condition of the list does.
template <size_t N>
constexpr Condition anyOf(const Condition (&list)[N])
{
	return Condition{ NULL, CompareOp::Eq, 0, NULL, 0, NULL, list, N };
}

/* The other shape of a row, written the way the rows are: shape(kind, label) with the parts
   it changes named after it, so a shape never lists its members by position and gains one
   without a site being touched.

     constexpr Shape kFlag = shape(ValueType::Bool, "k").range(0, 1).offered(vfdEnabled);

   Everything not named is none. range() is for the bounds of an Int or a Bool, values() for
   an Enum's list. */
class ShapeBuilder
{
public:
	constexpr ShapeBuilder(ValueType type, const char *label)
		: s_{ type, label, 0, 0, NULL, 0, NULL, NULL } {}

	constexpr ShapeBuilder range(long low, long high) const
	{
		return ShapeBuilder(Shape{ s_.type, s_.label_key, low, high, s_.values, s_.value_count,
		                           s_.hint_key, s_.offered });
	}

	template <size_t N>
	constexpr ShapeBuilder values(const EnumValue (&list)[N]) const
	{
		return ShapeBuilder(Shape{ s_.type, s_.label_key, s_.min, s_.max, list, N, s_.hint_key,
		                           s_.offered });
	}

	constexpr ShapeBuilder hint(const char *key) const
	{
		return ShapeBuilder(Shape{ s_.type, s_.label_key, s_.min, s_.max, s_.values, s_.value_count,
		                           key, s_.offered });
	}

	constexpr ShapeBuilder offered(bool (*test)()) const
	{
		return ShapeBuilder(Shape{ s_.type, s_.label_key, s_.min, s_.max, s_.values, s_.value_count,
		                           s_.hint_key, test });
	}

	constexpr operator Shape() const { return s_; }

private:
	explicit constexpr ShapeBuilder(const Shape &s) : s_(s) {}

	Shape s_;
};

constexpr ShapeBuilder shape(ValueType type, const char *label)
{
	return ShapeBuilder(type, label);
}

class RowBuilder
{
public:
	constexpr RowBuilder(const Descriptor &d, bool (*avail)()) : d_(d), avail_(avail) {}

	// A fresh row: every member none, and the bounds the kind starts from.
	static constexpr RowBuilder make(const char *key, ValueType t, long low, long high)
	{
		return RowBuilder(Descriptor{
			key, t, NULL, NULL, NULL, low, high, NULL, 0, 0, NULL, false, false, NULL, 0, COREAPI_NO_FIELD,
			NULL, NULL, NULL, NULL, NULL, NULL, NULL
		}, NULL);
	}

	constexpr RowBuilder section(const char *name) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, name, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values, d_.value_count,
			d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder label(const char *key) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, key, d_.hint_key, d_.min, d_.max, d_.values, d_.value_count,
			d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder hint(const char *key) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, key, d_.min, d_.max, d_.values, d_.value_count,
			d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder min(long v) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, v, d_.max, d_.values, d_.value_count,
			d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder max(long v) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, v, d_.values, d_.value_count,
			d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder range(long low, long high) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, low, high, d_.values, d_.value_count,
			d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder defaultValue(long v) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, v, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder defaultValue(int v) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, v, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder defaultValue(const char *text) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, text, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder defaultFrom(long (*fn)()) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, fn
		}, avail_);
	}

	template <size_t N>
	constexpr RowBuilder values(const EnumValue (&list)[N]) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, list, N, d_.default_int,
			d_.default_string, d_.needs_restart, d_.secret, d_.conditions, d_.condition_count, d_.field,
			d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now, d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder unit(const char *key) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder format(const char *key) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, key, d_.text, d_.choices_from, d_.min_now, d_.max_now,
			d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder text(const TextRule &rule) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, &rule, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder choicesFrom(ChoiceSource fn) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, fn, d_.min_now, d_.max_now,
			d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder minNow(long (*fn)()) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, fn,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder maxNow(long (*fn)()) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			fn, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder needsRestart() const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, true, d_.secret, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	constexpr RowBuilder secret() const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, true, d_.conditions,
			d_.condition_count, d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now,
			d_.max_now, d_.default_fn
		}, avail_);
	}

	template <size_t N>
	constexpr RowBuilder changeableWhen(const Condition (&list)[N]) const
	{
		return RowBuilder(Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, list, N,
			d_.field, d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now, d_.max_now,
			d_.default_fn
		}, avail_);
	}

	// Whether the box has what the setting controls, as FieldRef::available.
	constexpr RowBuilder availableIf(bool (*test)()) const
	{
		return RowBuilder(d_, test);
	}

	// Ends the row, with where the value lives.
	constexpr Descriptor field(const FieldRef &f) const
	{
		return Descriptor{
			d_.key, d_.type, d_.section, d_.label_key, d_.hint_key, d_.min, d_.max, d_.values,
			d_.value_count, d_.default_int, d_.default_string, d_.needs_restart, d_.secret, d_.conditions,
			d_.condition_count,
			FieldRef{
				f.read_number, f.write_number, f.fits_number, f.read_text, f.write_text, f.ask, f.tell,
				f.choices, f.name, f.origin, avail_ != NULL ? avail_ : f.available, f.otherwise
			},
			d_.unit_key, d_.format_key, d_.text, d_.choices_from, d_.min_now, d_.max_now, d_.default_fn
		};
	}

private:
	Descriptor d_;
	bool     (*avail_)();
};

constexpr RowBuilder boolRow(const char *key)
{
	return RowBuilder::make(key, ValueType::Bool, 0, 1);
}

constexpr RowBuilder intRow(const char *key)
{
	return RowBuilder::make(key, ValueType::Int, 0, 0);
}

constexpr RowBuilder enumRow(const char *key)
{
	return RowBuilder::make(key, ValueType::Enum, 0, 0);
}

constexpr RowBuilder textRow(const char *key)
{
	return RowBuilder::make(key, ValueType::String, 0, 0);
}

// The bounds are the codes the field holds, set with range() as an Int's are.
constexpr RowBuilder keyRow(const char *key)
{
	return RowBuilder::make(key, ValueType::Key, 0, 0);
}

// The row in the other shape it names, the rest of it kept.
inline void takeShape(Descriptor &d, const Shape &o)
{
	d.type = o.type;
	d.label_key = o.label_key;
	d.min = o.min;
	d.max = o.max;
	d.values = o.values;
	d.value_count = o.value_count;
	if (o.hint_key != NULL)
		d.hint_key = o.hint_key;
}

/* The row as this box offers it: unchanged where its test holds or it has
   none, in its other shape where the test says no and it has one. False where
   the box lacks the setting, and out is then the row unchanged. One function,
   so every reader of a row asks the box the same question. */
inline bool rowOnThisBox(const Descriptor &d, Descriptor &out)
{
	out = d;
	if (d.field.available == NULL || d.field.available())
		return true;
	if (d.field.otherwise == NULL)
		return false;
	if (d.field.otherwise->offered != NULL && !d.field.otherwise->offered())
		return false;
	takeShape(out, *d.field.otherwise);
	return true;
}

// The most values one Int names in words.
const size_t kMaxNamedNumbers = 4;

// The value an Int row names in words, NULL for a plain number, another kind or
// a row naming several. Whoever has to honour them all walks values itself.
inline const EnumValue *namedNumber(const Descriptor &d)
{
	if (d.type != ValueType::Int || d.values == NULL || d.value_count != 1)
		return NULL;
	return &d.values[0];
}

// A group is told apart by its alternatives, either half of the pair counting,
// so a group that lost one of them is still refused as a group.
inline bool conditionIsGroup(const Condition &c)
{
	return c.any_of != NULL || c.any_count > 0;
}

// One comparison as a table has to write it.
inline bool comparisonIsSane(const Condition &c)
{
	if (c.key == NULL || c.key[0] == '\0')
		return false;
	if (c.op == CompareOp::In && (c.values == NULL || c.value_count == 0))
		return false;
	if (c.op == CompareOp::TextValid && c.text == NULL)
		return false;
	return true;
}

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
		if (!conditionIsGroup(c))
		{
			if (!comparisonIsSane(c))
				return false;
			continue;
		}
		/* A group with nothing in it would hold for nobody's reason, a key
		   beside it would be read by nobody, and a group inside a group is the
		   expression language the type refuses to be. */
		if (c.any_of == NULL || c.any_count == 0 || c.key != NULL)
			return false;
		for (size_t m = 0; m < c.any_count; ++m)
		{
			if (conditionIsGroup(c.any_of[m]) || !comparisonIsSane(c.any_of[m]))
				return false;
		}
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
	const bool named = d.field.name != NULL && d.field.name[0] != '\0';
	const bool located = d.field.read_number != NULL || d.field.read_text != NULL ||
	                     d.field.ask != NULL;
	if (located != named)
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

	/* The members that say how a value is shown or offered belong to one kind
	   each, and a name that is empty names nothing. */
	if (d.unit_key != NULL && d.unit_key[0] == '\0')
		return false;
	if (d.format_key != NULL && d.format_key[0] == '\0')
		return false;
	if (d.unit_key != NULL && d.format_key != NULL)
		return false;
	if ((d.unit_key != NULL || d.format_key != NULL || d.min_now != NULL || d.max_now != NULL) &&
	    d.type != ValueType::Int)
		return false;
	if (d.text != NULL && d.type != ValueType::String)
		return false;
	/* A row whose own default breaks its rule would be one the box writes itself and
	   then refuses to take back. An empty default is no value and passes. */
	if (d.text != NULL)
	{
		if (d.text->max_length != 0 && d.text->min_length > d.text->max_length)
			return false;
		if (d.text->allow_empty && d.text->min_length > 0)
			return false;
		if (d.default_string != NULL && d.default_string[0] != '\0' &&
		    textFault(*d.text, d.default_string) != TextFault::None)
			return false;
	}
	if (d.default_fn != NULL && d.type == ValueType::String)
		return false;
	if (d.choices_from != NULL && d.type != ValueType::Int && d.type != ValueType::String)
		return false;
	for (size_t i = 0; i < d.value_count && d.values != NULL; ++i)
	{
		const EnumValue &e = d.values[i];
		// Only a choice is offered conditionally: the words of a flag or of a
		// number are shown as they are.
		if ((e.when != NULL || e.when_count > 0) && d.type != ValueType::Enum)
			return false;
		if (e.when_count > 0 && e.when == NULL)
			return false;
		for (size_t c = 0; c < e.when_count; ++c)
		{
			if (conditionIsGroup(e.when[c]) || !comparisonIsSane(e.when[c]))
				return false;
		}
	}

	/* A shape no test ever chooses is one nobody sees. The other shape is held
	   to every rule here as a row of its own, and it is no row in two shapes
	   itself. */
	if (d.field.otherwise != NULL)
	{
		if (d.field.available == NULL)
			return false;
		Descriptor other = d;
		takeShape(other, *d.field.otherwise);
		other.field.available = NULL;
		other.field.otherwise = NULL;
		if (!descriptorIsSane(other))
			return false;
	}

	switch (d.type)
	{
		case ValueType::Bool:
			/* A flag may name the words of its two values, and then names
			   exactly those two, each once and always offered: anything else
			   is a choice, which the type would hide. */
			if (d.values != NULL || d.value_count != 0)
			{
				if (d.values == NULL || d.value_count != 2)
					return false;
				for (size_t i = 0; i < 2; ++i)
				{
					const EnumValue &e = d.values[i];
					if ((e.label_key == NULL) == (e.label_text == NULL))
						return false;
					if (e.available != NULL)
						return false;
				}
				if (d.values[0].value + d.values[1].value != 1 ||
				    d.values[0].value * d.values[1].value != 0)
					return false;
			}
			return d.default_int == 0 || d.default_int == 1;

		case ValueType::Int:
			/* A number may name a few values in words, off or last used, and
			   then takes them beside its bounds. Each is the floor or outside
			   the bounds, never above the floor: there it would hide a number
			   the range offers. An entry without words is refused, and so is a
			   value named twice or a list longer than kMaxNamedNumbers. */
			if (d.values != NULL || d.value_count != 0)
			{
				if (d.values == NULL || d.value_count == 0 || d.value_count > kMaxNamedNumbers)
					return false;
				for (size_t i = 0; i < d.value_count; ++i)
				{
					const EnumValue &e = d.values[i];
					if (e.label_key == NULL || e.label_key[0] == '\0')
						return false;
					if (e.label_text != NULL || e.available != NULL)
						return false;
					if (e.value > d.min && e.value <= d.max)
						return false;
					for (size_t j = 0; j < i; ++j)
						if (d.values[j].value == e.value)
							return false;
				}
			}
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
				if ((d.values[i].label_key == NULL) == (d.values[i].label_text == NULL))
					return false;
			for (size_t i = 0; i < d.value_count; ++i)
			{
				if (d.values[i].value == d.default_int)
					return true;
			}
			return false;

		case ValueType::Key:
			// A code is one number between the bounds the field holds. The words of a
			// number are for an Int, and which codes are keys is asked at the write.
			if (d.values != NULL || d.value_count != 0)
				return false;
			return d.min <= d.max && d.default_int >= d.min && d.default_int <= d.max;
	}

	// A type is an int with a fixed set of names, not a promise that a table
	// wrote one of them.
	return false;
}

// How the evaluator below reads the current value of another setting. The
// readers are plain function pointers and context is handed back untouched.
// False means the setting is not known, which is not a value of zero. A read is
// called once per comparison and nothing caches. Numbers and text are separate
// readers because a long cannot carry a string; either may be NULL, and the
// comparisons that need it are then not answered.
//
// The only allocation is the string a text comparison reads into.
struct ValueLookup
{
	bool (*read)(const char *key, long *value, void *context);
	bool (*read_text)(const char *key, std::string *value, void *context);
	void *context;
};

/* One comparison. A comparison nobody can answer for holds: an unknown key, a
   lookup that reads nothing, and a list or an operator a table got wrong all
   leave the setting shown. A setting shown where it does not apply is a
   nuisance; one hidden where it does apply cannot be found at all. */
inline bool comparisonHolds(const Condition &c, const ValueLookup &lookup)
{
	if (c.key == NULL)
		return true;

	if (c.op == CompareOp::TextValid)
	{
		std::string current;
		if (lookup.read_text == NULL || !lookup.read_text(c.key, &current, lookup.context))
			return true;
		// Without a placeholder there is nothing to tell apart but an empty text.
		return !current.empty() && (c.text == NULL || current != c.text);
	}

	long current = 0;
	if (lookup.read == NULL || !lookup.read(c.key, &current, lookup.context))
		return true;

	switch (c.op)
	{
		case CompareOp::Eq:
			return current == c.value;

		case CompareOp::Ne:
			return current != c.value;

		case CompareOp::Lt:
			return current < c.value;

		case CompareOp::Le:
			return current <= c.value;

		case CompareOp::Gt:
			return current > c.value;

		case CompareOp::Ge:
			return current >= c.value;

		case CompareOp::In:
		{
			// A list that is not there cannot be missed.
			if (c.values == NULL)
				return true;
			for (size_t v = 0; v < c.value_count; ++v)
			{
				if (c.values[v] == current)
					return true;
			}
			return false;
		}

		case CompareOp::TextValid:
			break;
	}

	return true;
}

// Whether every condition a descriptor carries holds, which is whether the
// setting is worth showing. A conjunction, and a descriptor carrying none holds
// trivially. In holds when the current value is one of its list, and a group
// when any of its alternatives holds. An alternative that is itself a group
// names no key, and holds like any other comparison nobody can answer for.
//
// A frontend that reads the list itself and disagrees with this function is
// wrong.
inline bool conditionsHold(const Descriptor &d, const ValueLookup &lookup)
{
	if (d.conditions == NULL)
		return true;

	for (size_t i = 0; i < d.condition_count; ++i)
	{
		const Condition &c = d.conditions[i];

		bool holds = true;
		if (!conditionIsGroup(c))
			holds = comparisonHolds(c, lookup);
		else if (c.any_of != NULL && c.any_count > 0)
		{
			holds = false;
			for (size_t m = 0; !holds && m < c.any_count; ++m)
				holds = comparisonHolds(c.any_of[m], lookup);
		}

		if (!holds)
			return false;
	}

	return true;
}

} // namespace coreapi

#endif
