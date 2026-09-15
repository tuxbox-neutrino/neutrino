/*
 * settingsfield.h - one settings field: its type, range, and where it lives
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

#ifndef __coreapi_settingsfield_h__
#define __coreapi_settingsfield_h__

#include "coreapi/base/schema.h"

#include <stdint.h>
#include <stdio.h>

#include <system/settings.h>

/* What a table row writes to say where its setting lives. Separate from the
   header that declares the row, because this one carries the program's whole
   settings struct with it.

   The pair of functions a row gets is generated for the type of the field it
   names, which makes a wrong pairing a compiler error rather than a value read
   as nonsense. The build refuses one of each, see
   test/unit/scan/check-fieldtypes.sh. */

namespace coreapi
{

// The struct keeps its numbers in seven different types, so a row says whether
// a value fits the one it names before anything stores it.
template <typename T, T SNeutrinoSettings::*M>
struct NumberField
{
	/* A value travels through this layer as a long, four bytes wide on the box,
	   so a field wider than that is one no table compiled for both can carry.
	   The struct has three of those and they are channel ids; two are carried
	   as text instead, see ChannelIdField below, and the third is an array no
	   single key names. */
	static_assert(sizeof(T) <= sizeof(int32_t),
		      "a settings field wider than a long on the box cannot be carried");

	static long read(const SNeutrinoSettings &s) { return (long) (s.*M); }
	static void write(SNeutrinoSettings &s, long v) { s.*M = (T) v; }

	static bool fits(long v)
	{
		T narrowed = (T) v;
		return (long) narrowed == v;
	}
};

/* One bit of a field beside it. The screen offers the three bits of one mask as
   three questions and folds them back as it leaves.

   The bits the mask does not name are left where they are: two rows over one
   field are two settings, and a write of one that cleared the other would be a
   setting changing a setting nobody asked about. */
template <typename T, T SNeutrinoSettings::*M, unsigned long Mask>
struct MaskBitField
{
	static_assert(sizeof(T) <= sizeof(int32_t),
		      "a settings field wider than a long on the box cannot be carried");
	static_assert(Mask != 0, "a bit of a mask is not the absence of one");

	static long read(const SNeutrinoSettings &s)
	{
		return (((unsigned long) (s.*M) & Mask) != 0) ? 1 : 0;
	}

	static void write(SNeutrinoSettings &s, long v)
	{
		const unsigned long held = (unsigned long) (s.*M);
		s.*M = (T) (v != 0 ? (held | Mask) : (held & ~Mask));
	}

	// The two the bit has, and not what the field behind it holds: the field is
	// the whole mask and would take every value a byte has.
	static bool fits(long v) { return v == 0 || v == 1; }
};

/* Read and written under the lock the struct declares for its text, because
   neither side of this pair is on the box's own loop with the screens that
   write the same member: the read is served on one of the web server's threads
   and the write runs from the loop with a request already answered. */
template <std::string SNeutrinoSettings::*M>
struct TextField
{
	static void read(const SNeutrinoSettings &s, std::string &out) { out = settingsText(s.*M); }
	static void write(SNeutrinoSettings &s, const std::string &v) { setSettingsText(s.*M, v); }
};

/* A sixty four bit identifier the struct holds, carried as the text a channel is
   named by everywhere else in this layer. Text and not a number, because the
   long a number travels in is four bytes wide on the box.

   A spelling the reader refuses leaves the field standing. What keeps one from
   ever arriving here is the rule the write is held to before the store is
   touched; this runs later, on another thread, and has nobody left to answer.*/
template <typename T, T SNeutrinoSettings::*M>
struct ChannelIdField
{
	static_assert(sizeof(T) == 8, "a channel identifier is sixty four bits wide");

	static void read(const SNeutrinoSettings &s, std::string &out)
	{
		char buf[24];
		snprintf(buf, sizeof(buf), "%llx", (unsigned long long) (s.*M));
		out = buf;
	}

	static void write(SNeutrinoSettings &s, const std::string &v)
	{
		unsigned long long id = 0;
		if (!readChannelIdText(v, id))
			return;
		s.*M = (T) id;
	}
};

} // namespace coreapi

// decltype rather than a type the row spells out, so a row cannot name a field
// and then claim it is of a type it is not. The name comes from the same
// argument as the functions, so a row cannot point at one field and be checked
// against another.
#define COREAPI_NUMBER_FIELD(f) \
	{ &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::read, \
	  &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::write, \
	  &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::fits, \
	  NULL, NULL, NULL, NULL, NULL, #f, coreapi::FieldOrigin::Member }

// The same field, with the set of values it offers asked for at run time rather
// than written down beside the row. c names the set to whoever answers.
#define COREAPI_NUMBER_FIELD_ASKED(f, c) \
	{ &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::read, \
	  &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::write, \
	  &coreapi::NumberField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::fits, \
	  NULL, NULL, NULL, NULL, (c), #f, coreapi::FieldOrigin::Member }

#define COREAPI_TEXT_FIELD(f) \
	{ NULL, NULL, NULL, \
	  &coreapi::TextField<&SNeutrinoSettings::f>::read, \
	  &coreapi::TextField<&SNeutrinoSettings::f>::write, \
	  NULL, NULL, NULL, #f, coreapi::FieldOrigin::Member }

/* f is the member the screen binds the question to and the name this row is
   found under, m is the mask the value really lives in, and b is the bit of it.
   The row needs both: the value is stored in the mask, and what the screens say
   about this question they say beside f. */
#define COREAPI_MASK_BIT_FIELD(f, m, b) \
	{ &coreapi::MaskBitField<decltype(SNeutrinoSettings::m), &SNeutrinoSettings::m, (b)>::read, \
	  &coreapi::MaskBitField<decltype(SNeutrinoSettings::m), &SNeutrinoSettings::m, (b)>::write, \
	  &coreapi::MaskBitField<decltype(SNeutrinoSettings::m), &SNeutrinoSettings::m, (b)>::fits, \
	  NULL, NULL, NULL, NULL, NULL, \
	  (sizeof(&SNeutrinoSettings::f) > 0 ? #f : #f), coreapi::FieldOrigin::MaskBit }

#define COREAPI_CHANNEL_ID_FIELD(f) \
	{ NULL, NULL, NULL, \
	  &coreapi::ChannelIdField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::read, \
	  &coreapi::ChannelIdField<decltype(SNeutrinoSettings::f), &SNeutrinoSettings::f>::write, \
	  NULL, NULL, NULL, #f, coreapi::FieldOrigin::ChannelIdField }

/* A value a daemon holds. a asks it and t tells it, and f is the member the
   program keeps as the screen's buffer for that value, named so the checks
   outside the compiler can find what the screens say about it.

   The member is named in an unevaluated sizeof, which holds the name to a real
   member without reading one: a name that is not a member forms no pointer to
   member and does not compile, and nothing here is called, so a table of these
   stays a constant. */
#define COREAPI_SERVICE_FIELD(f, a, t) \
	{ NULL, NULL, NULL, NULL, NULL, (a), (t), NULL, \
	  (sizeof(&SNeutrinoSettings::f) > 0 ? #f : #f), coreapi::FieldOrigin::Service }

#endif
