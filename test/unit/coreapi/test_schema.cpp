/*
 * test_schema.cpp - tests for the schema descriptions
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

#include "support/catch.hpp"
#include "coreapi/base/schema.h"

#include <cstring>

using namespace coreapi;

static const EnumValue kTwo[] = { { 0, "off" }, { 1, "on" } };
static const long kModes[] = { 2, 5, 9 };

static const Condition kOneCondition[] = {
	{ "other", CompareOp::Eq, 1, NULL, 0 }
};
static const Condition kTwoConditions[] = {
	{ "other", CompareOp::Ne, 0, NULL, 0 },
	{ "mode", CompareOp::In, 0, COREAPI_VALUES(kModes) }
};

namespace
{

struct FakeSetting
{
	const char *key;
	long        value;
};

int reads = 0;

// The table ends at the entry with no key. A key that is not in it is one the
// caller cannot answer for, which is not the same as one whose value is zero.
bool readFake(const char *key, long *value, void *context)
{
	++reads;
	const FakeSetting *t = static_cast<const FakeSetting *>(context);
	for (size_t i = 0; t[i].key != NULL; ++i)
	{
		if (std::strcmp(t[i].key, key) == 0)
		{
			*value = t[i].value;
			return true;
		}
	}
	return false;
}

} // namespace

/* Stand ins for what the macros generate. A row's field is two functions, and
   the rules below are about which of them a row carries rather than about what
   they do, so these do nothing. The struct they take is never touched and never
   defined here. */
namespace
{
long readNumber(const SNeutrinoSettings &) { return 0; }
void writeNumber(SNeutrinoSettings &, long) {}
bool fitsNumber(long) { return true; }
void readText(const SNeutrinoSettings &, std::string &) {}
void writeText(SNeutrinoSettings &, const std::string &) {}

const FieldRef kNumber = { readNumber, writeNumber, fitsNumber, NULL, NULL, NULL, NULL, NULL,
			   "number", FieldOrigin::Member };
const FieldRef kText = { NULL, NULL, NULL, readText, writeText, NULL, NULL, NULL,
			 "text", FieldOrigin::Member };
} // namespace

TEST_CASE("a field that can be read and not written is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, kNumber };
	REQUIRE(descriptorIsSane(d));

	d.field.write_number = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.read_number = NULL;
	d.field.write_number = writeNumber;
	REQUIRE_FALSE(descriptorIsSane(d));
}

// The third of the set is what refuses a value the field cannot hold, so a row
// that carries the other two would take one and store something else.
TEST_CASE("a number field that cannot say what fits is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, kNumber };
	REQUIRE(descriptorIsSane(d));

	d.field.fits_number = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.read_number = NULL;
	d.field.write_number = NULL;
	d.field.fits_number = fitsNumber;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a text field that can be read and not written is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
			 COREAPI_ALWAYS, kText };
	REQUIRE(descriptorIsSane(d));

	d.field.write_text = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.read_text = NULL;
	d.field.write_text = writeText;
	REQUIRE_FALSE(descriptorIsSane(d));
}

/* The name is what a check outside the compiler reads, and it is the only part
   of a field a table can write by hand. A field reached without one cannot be
   held to the key it belongs to, and a name over no field holds to nothing. */
TEST_CASE("a field reached but not named is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, kNumber };
	REQUIRE(descriptorIsSane(d));

	d.field.name = NULL;
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field.name = "";
	REQUIRE_FALSE(descriptorIsSane(d));

	Descriptor e = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(descriptorIsSane(e));

	e.field.name = "menu_left_exit";
	REQUIRE_FALSE(descriptorIsSane(e));
}

/* A String is carried by text and every other kind by a number, so a row whose
   field is of the other sort declares a value no read of it could answer. Both
   directions, because a rule written for one of them leaves the other free. */
TEST_CASE("a String whose field holds a number is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
			 COREAPI_ALWAYS, kNumber };
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field = kText;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("a setting that is not a String whose field holds text is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 1, NULL, false, false,
			 COREAPI_ALWAYS, kText };
	REQUIRE_FALSE(descriptorIsSane(d));

	d.field = kNumber;
	REQUIRE(descriptorIsSane(d));

	// And an Enum, because the rule is written against the one kind that is
	// text rather than against the one kind that is not.
	Descriptor e = { "k", ValueType::Enum, "s", "l", "h", 0, 0, COREAPI_ENUM(kTwo), 1, NULL, false, false, COREAPI_ALWAYS, kText };
	REQUIRE_FALSE(descriptorIsSane(e));
	e.field = kNumber;
	REQUIRE(descriptorIsSane(e));
}

// A row that reaches nothing is the ordinary case for a setting the program
// keeps out of its settings struct, so it stays sane.
TEST_CASE("a row that names no field at all is sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			 COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Int whose default sits outside its bounds is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 1, 5, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 3;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Enum whose default names no listed value is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, kTwo, 2, 2, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 1;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Enum with no values is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a String with no default string is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_string = "";
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an absent key or section is not sane and an absent label is", "[schema]")
{
	Descriptor ok = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(descriptorIsSane(ok));

	Descriptor no_key = ok;    no_key.key = NULL;         REQUIRE_FALSE(descriptorIsSane(no_key));
	Descriptor no_sect = ok;   no_sect.section = NULL;    REQUIRE_FALSE(descriptorIsSane(no_sect));
	Descriptor empty_key = ok; empty_key.key = "";        REQUIRE_FALSE(descriptorIsSane(empty_key));

	// The program has no name for most of what its settings file holds, and a
	// row saying so is what keeps it from borrowing the name of another item.
	Descriptor no_label = ok;  no_label.label_key = NULL; REQUIRE(descriptorIsSane(no_label));
}

TEST_CASE("an Int whose bounds are inverted is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 5, 1, NULL, 0, 3, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a section or a label that is present but empty is not sane", "[schema]")
{
	Descriptor ok = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(descriptorIsSane(ok));

	Descriptor empty_sect = ok;  empty_sect.section = "";    REQUIRE_FALSE(descriptorIsSane(empty_sect));
	Descriptor empty_label = ok; empty_label.label_key = ""; REQUIRE_FALSE(descriptorIsSane(empty_label));
}

TEST_CASE("an Int whose default sits below its floor is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Int, "s", "l", "h", 1, 5, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 1;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an Enum that counts values it does not carry is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, NULL, 2, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("the enum macro writes the values and the count as one pair", "[schema]")
{
	Descriptor d = { "k", ValueType::Enum, "s", "l", "h", 0, 0, COREAPI_ENUM(kTwo), 1, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(d.values == kTwo);
	REQUIRE(d.value_count == 2u);
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("a Bool whose default is neither of its two values is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 7, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));
	d.default_int = 1;
	REQUIRE(descriptorIsSane(d));
	d.default_int = 0;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("a type that is none of the four is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(descriptorIsSane(d));
	d.type = static_cast<ValueType>(99);
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a hint is the one thing a setting may leave out", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", NULL, 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("a setting with no condition is always shown and is sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(d.conditions == NULL);
	REQUIRE(d.condition_count == 0u);
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("conditions are carried as a list and every one of them is read", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(kTwoConditions), COREAPI_NO_FIELD };
	REQUIRE(d.conditions == kTwoConditions);
	REQUIRE(d.condition_count == 2u);
	REQUIRE(descriptorIsSane(d));

	// The second element is the one an evaluator would miss if the count were
	// ignored and only the first were read.
	Condition broken[] = { kTwoConditions[0], kTwoConditions[1] };
	broken[1].key = "";
	Descriptor with_broken = d;
	with_broken.conditions = broken;
	REQUIRE_FALSE(descriptorIsSane(with_broken));
}

TEST_CASE("a condition count without the conditions is not sane", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, NULL, 2, COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));

	// One is the count a dropped list leaves behind, and it is the boundary the
	// guard is written at.
	d.condition_count = 1;
	REQUIRE_FALSE(descriptorIsSane(d));
}

TEST_CASE("a condition that names no setting is not sane", "[schema]")
{
	Condition c[] = { { NULL, CompareOp::Eq, 1, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].key = "";
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].key = "other";
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("an In condition that lists no value is not sane", "[schema]")
{
	Condition c[] = { { "mode", CompareOp::In, 0, NULL, 2 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].values = kModes;
	c[0].value_count = 0;
	REQUIRE_FALSE(descriptorIsSane(d));

	c[0].value_count = 3;
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("only an In condition is asked for a value list", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(kOneCondition), COREAPI_NO_FIELD };
	REQUIRE(kOneCondition[0].values == NULL);
	REQUIRE(kOneCondition[0].value_count == 0u);
	REQUIRE(descriptorIsSane(d));
}

TEST_CASE("the value macro writes an In list and its count as one pair", "[schema]")
{
	REQUIRE(kTwoConditions[1].values == kModes);
	REQUIRE(kTwoConditions[1].value_count == 3u);
}

TEST_CASE("a malformed condition is refused whatever the type of the setting is", "[schema]")
{
	Condition bad[] = { { NULL, CompareOp::Eq, 1, NULL, 0 } };

	Descriptor as_bool = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			       COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD };
	Descriptor as_int = { "k", ValueType::Int, "s", "l", "h", 0, 9, NULL, 0, 3, NULL, false, false,
			      COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD };
	Descriptor as_string = { "k", ValueType::String, "s", "l", "h", 0, 0, NULL, 0, 0, "", false, false,
				 COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD };
	Descriptor as_enum = { "k", ValueType::Enum, "s", "l", "h", 0, 0, COREAPI_ENUM(kTwo), 1, NULL, false, false,
			       COREAPI_CONDITIONS(bad), COREAPI_NO_FIELD };

	REQUIRE_FALSE(descriptorIsSane(as_bool));
	REQUIRE_FALSE(descriptorIsSane(as_int));
	REQUIRE_FALSE(descriptorIsSane(as_string));
	REQUIRE_FALSE(descriptorIsSane(as_enum));

	// All four are sane once the condition names a setting, so it is the
	// condition being refused and not the descriptor around it.
	bad[0].key = "other";
	REQUIRE(descriptorIsSane(as_bool));
	REQUIRE(descriptorIsSane(as_int));
	REQUIRE(descriptorIsSane(as_string));
	REQUIRE(descriptorIsSane(as_enum));
}

TEST_CASE("a setting that carries no condition is shown", "[schema]")
{
	FakeSetting t[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS, COREAPI_NO_FIELD };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("a list of no conditions is shown though its first entry would not hold", "[schema]")
{
	FakeSetting t[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, kOneCondition, 0, COREAPI_NO_FIELD };
	REQUIRE(conditionsHold(d, lookup));

	d.condition_count = 1;
	REQUIRE_FALSE(conditionsHold(d, lookup));
}

TEST_CASE("every condition has to hold and not merely one of them", "[schema]")
{
	Condition both[] = {
		{ "a", CompareOp::Eq, 1, NULL, 0 },
		{ "b", CompareOp::Eq, 1, NULL, 0 }
	};
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(both), COREAPI_NO_FIELD };

	FakeSetting yes_yes[] = { { "a", 1 }, { "b", 1 }, { NULL, 0 } };
	FakeSetting yes_no[] = { { "a", 1 }, { "b", 0 }, { NULL, 0 } };
	FakeSetting no_yes[] = { { "a", 0 }, { "b", 1 }, { NULL, 0 } };
	FakeSetting no_no[] = { { "a", 0 }, { "b", 0 }, { NULL, 0 } };

	ValueLookup both_hold = { readFake, yes_yes };
	REQUIRE(conditionsHold(d, both_hold));

	// These two are what tells a conjunction from a disjunction: under an OR
	// both of them would show the setting.
	ValueLookup second_fails = { readFake, yes_no };
	REQUIRE_FALSE(conditionsHold(d, second_fails));
	ValueLookup first_fails = { readFake, no_yes };
	REQUIRE_FALSE(conditionsHold(d, first_fails));

	ValueLookup neither = { readFake, no_no };
	REQUIRE_FALSE(conditionsHold(d, neither));
}

TEST_CASE("each operator answers against the value its own condition carries", "[schema]")
{
	FakeSetting t[] = { { "a", 5 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	Condition c[] = { { "a", CompareOp::Eq, 5, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };

	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 4;
	REQUIRE_FALSE(conditionsHold(d, lookup));

	c[0].op = CompareOp::Ne;
	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 5;
	REQUIRE_FALSE(conditionsHold(d, lookup));

	// Written the other way round these two would answer the opposite, so they
	// pin the order of the comparison as well as its strictness.
	c[0].op = CompareOp::Lt;
	c[0].value = 6;
	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 5;
	REQUIRE_FALSE(conditionsHold(d, lookup));

	c[0].op = CompareOp::Gt;
	c[0].value = 4;
	REQUIRE(conditionsHold(d, lookup));
	c[0].value = 5;
	REQUIRE_FALSE(conditionsHold(d, lookup));
}

TEST_CASE("In holds for any listed value and for no other", "[schema]")
{
	Condition c[] = { { "mode", CompareOp::In, 0, COREAPI_VALUES(kModes) } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };

	FakeSetting first[] = { { "mode", 2 }, { NULL, 0 } };
	FakeSetting middle[] = { { "mode", 5 }, { NULL, 0 } };
	FakeSetting last[] = { { "mode", 9 }, { NULL, 0 } };
	FakeSetting between[] = { { "mode", 4 }, { NULL, 0 } };

	ValueLookup at_first = { readFake, first };
	ValueLookup at_middle = { readFake, middle };
	ValueLookup at_last = { readFake, last };
	ValueLookup at_none = { readFake, between };

	REQUIRE(conditionsHold(d, at_first));
	REQUIRE(conditionsHold(d, at_middle));
	REQUIRE(conditionsHold(d, at_last));
	REQUIRE_FALSE(conditionsHold(d, at_none));

	// In reads the list and never the single value, which is set here to the
	// one number the list does not hold.
	c[0].value = 4;
	REQUIRE_FALSE(conditionsHold(d, at_none));
	REQUIRE(conditionsHold(d, at_first));
}

TEST_CASE("a key the lookup does not know leaves the setting shown", "[schema]")
{
	Condition c[] = { { "absent", CompareOp::Eq, 1, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };

	FakeSetting without[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup cannot_answer = { readFake, without };
	REQUIRE(conditionsHold(d, cannot_answer));

	// The same condition is genuinely false once the key can be read, so the
	// answer above is the unknown key and not a condition that holds anyway.
	FakeSetting with[] = { { "absent", 0 }, { NULL, 0 } };
	ValueLookup can_answer = { readFake, with };
	REQUIRE_FALSE(conditionsHold(d, can_answer));
}

TEST_CASE("a lookup that reads nothing leaves every setting shown", "[schema]")
{
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(kTwoConditions), COREAPI_NO_FIELD };
	ValueLookup none = { NULL, NULL };
	REQUIRE(conditionsHold(d, none));
}

TEST_CASE("a count whose conditions are absent is shown rather than walked", "[schema]")
{
	FakeSetting t[] = { { "other", 0 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false, NULL, 2, COREAPI_NO_FIELD };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("an In condition whose list is absent is shown rather than walked", "[schema]")
{
	FakeSetting t[] = { { "mode", 4 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	Condition c[] = { { "mode", CompareOp::In, 0, NULL, 3 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("an operator that is none of the five leaves the setting shown", "[schema]")
{
	FakeSetting t[] = { { "a", 5 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	Condition c[] = { { "a", static_cast<CompareOp>(99), 1, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };
	REQUIRE(conditionsHold(d, lookup));
}

TEST_CASE("the closed comparisons hold at the bound where the strict ones do not", "[schema]")
{
	FakeSetting t[] = { { "a", 5 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	Condition c[] = { { "a", CompareOp::Lt, 5, NULL, 0 } };
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(c), COREAPI_NO_FIELD };

	// At the bound itself the strict one refuses and the closed one holds. That
	// difference is the whole reason both exist.
	REQUIRE_FALSE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Le;
	REQUIRE(conditionsHold(d, lookup));

	c[0].op = CompareOp::Gt;
	REQUIRE_FALSE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Ge;
	REQUIRE(conditionsHold(d, lookup));

	// Away from the bound each closed one answers like its strict partner, and
	// in the same direction, which is what these four pin.
	c[0].value = 4;
	c[0].op = CompareOp::Le;
	REQUIRE_FALSE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Ge;
	REQUIRE(conditionsHold(d, lookup));

	c[0].value = 6;
	c[0].op = CompareOp::Le;
	REQUIRE(conditionsHold(d, lookup));
	c[0].op = CompareOp::Ge;
	REQUIRE_FALSE(conditionsHold(d, lookup));
}

TEST_CASE("a closed range is two conditions and needs no arithmetic on a bound", "[schema]")
{
	Condition range[] = {
		{ "mode", CompareOp::Gt, 0, NULL, 0 },
		{ "mode", CompareOp::Le, 3, NULL, 0 }
	};
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(range), COREAPI_NO_FIELD };
	REQUIRE(descriptorIsSane(d));

	FakeSetting below[] = { { "mode", 0 }, { NULL, 0 } };
	FakeSetting at_floor[] = { { "mode", 1 }, { NULL, 0 } };
	FakeSetting inside[] = { { "mode", 2 }, { NULL, 0 } };
	FakeSetting at_ceiling[] = { { "mode", 3 }, { NULL, 0 } };
	FakeSetting above[] = { { "mode", 4 }, { NULL, 0 } };

	ValueLookup l_below = { readFake, below };
	ValueLookup l_at_floor = { readFake, at_floor };
	ValueLookup l_inside = { readFake, inside };
	ValueLookup l_at_ceiling = { readFake, at_ceiling };
	ValueLookup l_above = { readFake, above };

	REQUIRE_FALSE(conditionsHold(d, l_below));
	REQUIRE(conditionsHold(d, l_at_floor));
	REQUIRE(conditionsHold(d, l_inside));
	REQUIRE(conditionsHold(d, l_at_ceiling));
	REQUIRE_FALSE(conditionsHold(d, l_above));
}

TEST_CASE("a key named by two conditions is read once for each of them", "[schema]")
{
	Condition twice[] = {
		{ "mode", CompareOp::Gt, 0, NULL, 0 },
		{ "mode", CompareOp::Le, 3, NULL, 0 }
	};
	Descriptor d = { "k", ValueType::Bool, "s", "l", "h", 0, 1, NULL, 0, 0, NULL, false, false,
			 COREAPI_CONDITIONS(twice), COREAPI_NO_FIELD };

	FakeSetting t[] = { { "mode", 2 }, { NULL, 0 } };
	ValueLookup lookup = { readFake, t };

	reads = 0;
	REQUIRE(conditionsHold(d, lookup));
	REQUIRE(reads == 2);
}
