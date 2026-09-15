/*
 * json.h - JSON reading and writing for the endpoints
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

#ifndef __httpd_json_h__
#define __httpd_json_h__

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{

/* The bytes of s escaped into out, without the quotes around them, so a caller already
   writing a quoted member pays for one buffer and not two.

   A length rather than a terminator, because a value that came off the box can carry a
   zero byte and stopping at it would shorten the answer without saying so.

   Everything from 0x20 to 0x7f is copied as it stands. Above that the bytes are read as
   utf-8 and a sequence that is not one is replaced by U+FFFD, one for each byte that
   begins nothing. That check is here rather than at the callers because there is one
   writer and there are many of them, and because a channel name comes off a broadcast
   stream, which promises no encoding at all: a body carrying a byte that is not utf-8 is
   a body a standards compliant reader refuses whole. The tree's own parser accepts one,
   which is why this cannot be left to be noticed downstream.

   Forward slash is not escaped: doing so is allowed and buys nothing. */
void appendEscaped(std::string &out, const char *s, size_t n);

/* Whether these bytes are text the writer above can carry back unchanged, read the same
   way so the two cannot disagree about what a sequence is.

   The substitution above is silent by design and has to be: an answer that refused to
   name a channel whose name is not utf-8 would be worse than one naming it with a
   replacement character. What that costs is a value written through this server and read
   back that is not the value that was sent, so a caller that means to store text asks
   this first. */
bool isUtf8(const char *s, size_t n);

/* A number the C library has printed, appended with whatever the current locale calls a
   decimal point rewritten to the one JSON has. Only the first occurrence, a printed
   number carrying at most one.

   The formatting of a double reads LC_NUMERIC, and a window elsewhere in this program
   sets the process locale out of the environment and never puts it back. On a German box
   that turns one number into two members and the whole body stops parsing. The point is
   a parameter so a case can drive the substitution on a machine that has no locale
   installed but the C one. */
void appendNumber(std::string &out, const char *printed, const char *decimal_point);

/* What one member of a body carries, which decides how the text beside it is read
   back. A number and a boolean are kept as the text they arrived as, so the router
   holds a body value to the same row, through the same reader, as a query value. */
enum class JsonValueKind
{
	String,
	Number,
	Bool
};

struct JsonMember
{
	std::string   name;
	std::string   text;
	JsonValueKind kind;

	JsonMember() : kind(JsonValueKind::String) {}
};

/* One flat JSON object read into its members, and false for anything that is not one.

   Flat, and a reader rather than a parser, because every body this server takes is a
   handful of named values. An object or an array under a member is refused rather than
   flattened or ignored: a caller that sent one meant something this server does not
   offer, and taking the outer half of it would act on half a request. Null is refused
   for the same reason.

   Nothing recurses, so a body cannot spend this server's stack however it is written,
   and the members are bounded so a body the size of the ceiling cannot be turned into a
   list that costs many times what it did. Both matter because the one route reachable
   without a credential carries a body.

   A name given twice is not refused here and both members are kept. Which of the two was
   meant is not this reader's to settle, and the layer above already answers a value given
   twice under its own code.

   Empty is an object with no members and not a fault. */
bool readFlatObject(const std::string &in, std::vector<JsonMember> &out);

/* The most members one body may carry and the longest a member's name may be.
   Public because the sentence a caller answers a refusal with names them, and
   a number written out there could drift from the one enforced here. */
const size_t kMaxBodyMembers = 256;
const size_t kMaxMemberNameBytes = 4096;

/* What came of reading a body that is one array of strings.

   Three answers and not a bool, because a caller says something different about each
   refusal: a body that is not such an array is one to write again, and one carrying
   more strings than were asked for is one to send in pieces. A body that is a list
   rather than a record is the shape this reads, an object wrapped round it naming
   the one member a route of that shape already names in its path. */
enum class ArrayRead
{
	Ok,
	NotAnArray,
	TooMany
};

/* One flat JSON array of strings read into its elements.

   Flat for the reason the object reader beside it is: an object, an array, a number, a
   boolean and a null under an element are all refused rather than flattened.

   max_items is what the route the body arrived at takes, and the count is answered for
   before the element that would pass it is read, so a body cannot turn into a list that
   costs many times what the body did. Nothing is returned for a refusal of either kind:
   a half read list is one no caller asked for.

   Nothing about what the strings spell is read here. */
ArrayRead readStringArray(const std::string &in, size_t max_items,
                          std::vector<std::string> &out);

/* JSON written by appending to a buffer rather than by building a tree.

   A document listing every endpoint this server has would be a node per member through a
   tree, which is about a megabyte of transient heap per request on the smallest box this
   runs on. Appending costs the bytes of the answer and nothing besides. The writer holds
   no buffer of its own: it appends to the string it was handed and never rewrites what
   that string already held.

   Nothing here checks that the calls make a document: the caller states the shape. What
   is checked is a set of mistakes that would put bytes on a wire that look like a
   document and are not, and every one of those stops the process: opening past MaxDepth,
   closing more than was opened, closing a level with the other kind of bracket, a key
   with no name, a key with no value after it, a value with no key inside an object, and
   an empty raw fragment.

   What is not checked, and cannot be from in here, is a document that was never finished.
   A handler returning early between beginObject and endObject leaves a truncated body,
   and this writer has no destructor to object with, an abort while an exception unwinds
   being worse than the truncation. complete() is what a caller or a case asks instead. */
class Json
{
	public:
		/* reserve_hint is room for what this writer is about to add and not a
		   total, so a writer starting on a buffer that already holds a header does not
		   ask for less than it needs. Zero leaves a buffer somebody else sized alone. */
		explicit Json(std::string &out, size_t reserve_hint = 0);

		void beginObject();
		void endObject();
		void beginArray();
		void endArray();

		// The name of the member that the next value written belongs to. That
		// value takes no separator of its own, the two being one member.
		void key(const char *k);

		/* A null pointer is what a C interface answers when it has no string,
		   and the absence of a value is what null means. Ending the process
		   over one would take a whole reply down with it. */
		void value(const char *s);
		void value(const std::string &s);
		void value(long v);
		void value(unsigned long v);
		/* Every width has an overload of its own because the box has a different
		   ABI from the machine the suite is built on. Where the box runs long is four
		   bytes: a size is the narrow unsigned, and identifiers and file sizes are long
		   long. A width left out names several candidates and matches none, which is a
		   build that fails at the cross compiler and nowhere earlier. */
		void value(int v);
		void value(unsigned v);
		void value(long long v);
		void value(unsigned long long v);
		void value(double v);
		void value(bool v);
		void null();

		/* An already serialised fragment, copied in as it stands. Nothing
		   quotes or checks it, which is the whole of its use: a document held
		   in a buffer already goes in without a parse and a second write. */
		void raw(const char *json);

		/* Whether nothing is left open, which is the one thing above that the writer
		   cannot stop on its own. A handler that may return early asks this before
		   it sends. True of a writer that has written nothing. */
		bool complete() const;

		/* How deep a document written here may nest. A document this server answers
		   with is nested by the shape of the answer and not by anything a request
		   carries, so this bounds handlers and not input. Public because a case that has
		   to reach it should not have to transcribe it. */
		static const size_t MaxDepth = 32;

	private:
		// Two writers appending to one buffer would each count separators for
		// a document the other is also writing.
		Json(const Json &);
		Json &operator=(const Json &);

		// is_name says the write is itself a member's name, which is the one
		// write inside an object that does not need a name in front of it.
		void beforeValue(bool is_name);
		void push(bool object);
		void pop(bool object);

		std::string *out_;
		size_t       depth_;
		bool         after_key_;
		/* Which of the two each open level is, so that a close can be held to
		   the thing it closes. Index zero is the document itself and is never
		   read, nothing outside a container being closable. */
		bool         is_object_[MaxDepth + 1];
		/* One flag per open level rather than one for the document: a value after a
		   nested array has closed follows a comma, and the first value inside that array
		   did not. Only the levels up to depth_ have been written. */
		bool         need_comma_[MaxDepth + 1];
};

} // namespace httpd

#endif
