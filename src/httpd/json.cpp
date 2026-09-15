/*
 * json.cpp - JSON reading and writing for the endpoints
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

#include "json.h"

#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace httpd
{

namespace
{

const char kHex[] = "0123456789abcdef";

// What one byte that is not text is answered with. Three bytes, U+FFFD, which
// is the character the standard sets aside for saying that something was here
// and could not be read.
const char kReplacement[] = "\xef\xbf\xbd";

/* How many bytes the well formed utf-8 sequence beginning at s occupies, and nought for
   a byte that begins none.

   Written out as ranges rather than as a length taken off the lead byte and a loop over
   continuations, because three of the things that make a sequence ill formed are
   invisible to that shorter reading and every one produces bytes a strict reader
   rejects: a sequence spelling a character in more room than it needs, which is how a
   check written against the shortest spelling is walked past; one naming half of a
   surrogate pair; and one naming a number above the last there is.

   left is what is left of the string, so a sequence cut off by the end of the value is
   not read past the end of it. */
size_t sequenceLength(const unsigned char *s, size_t left)
{
	const unsigned char c = s[0];

	size_t        need = 0;
	unsigned char low = 0x80;
	unsigned char high = 0xbf;

	if (c >= 0xc2 && c <= 0xdf)
		need = 2;
	else if (c == 0xe0)
	{
		need = 3;
		low = 0xa0;   // below this the character fits in two bytes
	}
	else if (c >= 0xe1 && c <= 0xec)
		need = 3;
	else if (c == 0xed)
	{
		need = 3;
		high = 0x9f;  // above this is the surrogate range
	}
	else if (c >= 0xee && c <= 0xef)
		need = 3;
	else if (c == 0xf0)
	{
		need = 4;
		low = 0x90;   // below this the character fits in three bytes
	}
	else if (c >= 0xf1 && c <= 0xf3)
		need = 4;
	else if (c == 0xf4)
	{
		need = 4;
		high = 0x8f;  // above this is past the last character there is
	}
	else
		return 0;

	if (left < need)
		return 0;
	if (s[1] < low || s[1] > high)
		return 0;
	for (size_t i = 2; i < need; i++)
	{
		if (s[i] < 0x80 || s[i] > 0xbf)
			return 0;
	}
	return need;
}

} // namespace

bool isUtf8(const char *s, size_t n)
{
	const unsigned char *p = (const unsigned char *) s;
	size_t i = 0;
	while (i < n)
	{
		if (p[i] < 0x80)
		{
			++i;
			continue;
		}
		// The one reading there is, so what this calls text and what the
		// writer copies through rather than replacing are the same set.
		const size_t got = sequenceLength(p + i, n - i);
		if (got == 0)
			return false;
		i += got;
	}
	return true;
}

void appendEscaped(std::string &out, const char *s, size_t n)
{
	for (size_t i = 0; i < n; i++)
	{
		const unsigned char c = (unsigned char) s[i];
		switch (c)
		{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20)
				{
					// The rest of the control range has no short form, and a
					// raw one of them ends the string for whoever reads it.
					out += "\\u00";
					out += kHex[c >> 4];
					out += kHex[c & 0x0f];
				}
				else if (c < 0x80)
				{
					out += (char) c;
				}
				else
				{
					/* A byte the box never converted, which is what a name off a
					   broadcast stream can carry. Answered with the replacement
					   character rather than sent on, because the tree's own parser
					   takes it and a strict reader stops on the whole body, so one
					   channel nobody can name would cost a client every answer that
					   names it.

					   One replacement per byte that begins nothing, rather than one per
					   run of them: counting how much of a run belonged together would
					   be a second reading of bytes that have no reading. */
					const size_t len =
						sequenceLength((const unsigned char *) s + i, n - i);
					if (len == 0)
					{
						out += kReplacement;
						break;
					}
					out.append(s + i, len);
					i += len - 1;
				}
				break;
		}
	}
}

void appendNumber(std::string &out, const char *printed, const char *decimal_point)
{
	if (decimal_point == NULL || decimal_point[0] == '\0')
	{
		// A locale with no decimal point at all is one the C standard does not
		// allow, and guessing which byte to rewrite would be worse than none.
		out += printed;
		return;
	}
	if (decimal_point[0] == '.' && decimal_point[1] == '\0')
	{
		out += printed;
		return;
	}

	const char *at = strstr(printed, decimal_point);
	if (at == NULL)
	{
		// A whole number prints without one, and so does an infinity, which
		// this is not reached for.
		out += printed;
		return;
	}

	out.append(printed, (size_t) (at - printed));
	out += '.';
	// Whatever the locale calls it may be more than one byte, so the tail
	// starts after all of it and not after one character.
	out += at + strlen(decimal_point);
}

// Needed where the constant is taken by reference, which the cases that reach
// the ceiling do.
const size_t Json::MaxDepth;

Json::Json(std::string &out, size_t reserve_hint)
	: out_(&out), depth_(0), after_key_(false)
{
	is_object_[0] = false;
	need_comma_[0] = false;
	if (reserve_hint > 0)
		out_->reserve(out_->size() + reserve_hint);
}

void Json::beforeValue(bool is_name)
{
	/* A value that follows a key is that key's own, and a comma between the two
	   would make it a member with no name beside a name with no value. */
	if (after_key_)
	{
		after_key_ = false;
		return;
	}
	/* Inside an object every value is a member and needs the name that makes it
	   one. Written without a key it produces bytes that look like a document
	   and are not, which is what the close guard stops for. */
	if (!is_name && depth_ > 0 && is_object_[depth_])
		std::abort();
	if (need_comma_[depth_])
		*out_ += ',';
	need_comma_[depth_] = true;
}

void Json::push(bool object)
{
	/* Past the ceiling the flags would be written outside the array that holds them,
	   and the document that came out of it is one no client can parse. A handler
	   nesting that deep is wrong about the answer it is writing, so this stops rather
	   than sends. */
	if (depth_ >= MaxDepth)
		std::abort();
	depth_++;
	is_object_[depth_] = object;
	need_comma_[depth_] = false;
}

void Json::pop(bool object)
{
	// A close with no open behind it would step off the bottom of the same
	// array, and leaves a document that will not parse just as surely.
	if (depth_ == 0)
		std::abort();
	/* A brace closing a bracket is the same mistake one step along: the bytes
	   go out looking like a document and no client can read them. Stopping is
	   worse for the one handler that is wrong and better for every client, so
	   it stops, as it does at the ceiling. */
	if (is_object_[depth_] != object)
		std::abort();
	/* A name with nothing after it closes as {"a":} and stops parsing, and the
	   value that was meant to follow it is missing from the answer either way.
	   Stopping says which handler forgot it. */
	if (after_key_)
		std::abort();
	depth_--;
}

void Json::beginObject()
{
	beforeValue(false);
	*out_ += '{';
	push(true);
}

void Json::endObject()
{
	pop(true);
	*out_ += '}';
}

void Json::beginArray()
{
	beforeValue(false);
	*out_ += '[';
	push(false);
}

void Json::endArray()
{
	pop(false);
	*out_ += ']';
}

void Json::key(const char *k)
{
	// A member with no name cannot be written at all, and the alternative to
	// stopping here is reading through the pointer anyway.
	if (k == NULL)
		std::abort();
	// Two names in a row leaves the first without a value, the same mistake a
	// name against a close makes.
	if (after_key_)
		std::abort();

	beforeValue(true);
	*out_ += '"';
	appendEscaped(*out_, k, strlen(k));
	*out_ += "\":";
	after_key_ = true;
}

void Json::value(const char *s)
{
	if (s == NULL)
	{
		null();
		return;
	}
	beforeValue(false);
	*out_ += '"';
	appendEscaped(*out_, s, strlen(s));
	*out_ += '"';
}

void Json::value(const std::string &s)
{
	beforeValue(false);
	*out_ += '"';
	// data() and size() rather than c_str(), because a value off the box can
	// carry a zero byte and the terminator is not where the string ends.
	appendEscaped(*out_, s.data(), s.size());
	*out_ += '"';
}

void Json::value(long v)
{
	beforeValue(false);
	char buf[32];
	snprintf(buf, sizeof(buf), "%ld", v);
	*out_ += buf;
}

void Json::value(unsigned long v)
{
	beforeValue(false);
	char buf[32];
	snprintf(buf, sizeof(buf), "%lu", v);
	*out_ += buf;
}

void Json::value(int v)
{
	value((long) v);
}

void Json::value(unsigned v)
{
	value((unsigned long) v);
}

void Json::value(long long v)
{
	beforeValue(false);
	char buf[32];
	snprintf(buf, sizeof(buf), "%lld", v);
	*out_ += buf;
}

void Json::value(unsigned long long v)
{
	beforeValue(false);
	char buf[32];
	snprintf(buf, sizeof(buf), "%llu", v);
	*out_ += buf;
}

void Json::value(double v)
{
	beforeValue(false);

	/* JSON has no spelling for an infinity or for a not-a-number and the C
	   library prints one anyway, so a reading that came back as a division by
	   zero would otherwise leave a body no client can parse. Null is the
	   absence of a number, which is what such a reading is. */
	if (!std::isfinite(v))
	{
		*out_ += "null";
		return;
	}

	// Enough digits that the number reads back as the one that was written.
	// Fewer would hand a client a different number from the one measured.
	char buf[64];
	snprintf(buf, sizeof(buf), "%.17g", v);
	/* What the C library wrote between the digits is whatever the process
	   locale calls a decimal point, and this program sets that out of the
	   environment from a window it never puts back. */
	appendNumber(*out_, buf, localeconv()->decimal_point);
}

void Json::value(bool v)
{
	beforeValue(false);
	*out_ += v ? "true" : "false";
}

void Json::null()
{
	beforeValue(false);
	*out_ += "null";
}

void Json::raw(const char *json)
{
	if (json == NULL)
	{
		null();
		return;
	}
	/* An empty fragment writes nothing but takes the separator with it, so the
	   next value in the same level arrives after a comma with nothing in front
	   of it. A caller with no fragment to add means null and can say so. */
	if (json[0] == '\0')
		std::abort();
	beforeValue(false);
	*out_ += json;
}

bool Json::complete() const
{
	return depth_ == 0 && !after_key_;
}

namespace
{

/* Everything below reads the buffer through an index that is checked against
   the end before every byte, so a document that stops in the middle of
   anything ends the read rather than walking past what it was handed. */

bool isSpace(char c)
{
	// The four the grammar has, and no other: a vertical tab or a form feed
	// between two members is a byte the document may not carry, and skipping
	// one here would accept a document a strict reader refuses.
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void skipSpace(const std::string &in, size_t &i)
{
	while (i < in.size() && isSpace(in[i]))
		++i;
}

int hexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

// One character as the bytes it occupies. Written out here rather than shared
// with the writer beside it, which turns bytes into a document and never a
// document back into bytes.
void appendCodepoint(std::string &out, unsigned long cp)
{
	if (cp < 0x80)
	{
		out += (char) cp;
		return;
	}
	if (cp < 0x800)
	{
		out += (char) (0xc0 | (cp >> 6));
		out += (char) (0x80 | (cp & 0x3f));
		return;
	}
	if (cp < 0x10000)
	{
		out += (char) (0xe0 | (cp >> 12));
		out += (char) (0x80 | ((cp >> 6) & 0x3f));
		out += (char) (0x80 | (cp & 0x3f));
		return;
	}
	out += (char) (0xf0 | (cp >> 18));
	out += (char) (0x80 | ((cp >> 12) & 0x3f));
	out += (char) (0x80 | ((cp >> 6) & 0x3f));
	out += (char) (0x80 | (cp & 0x3f));
}

// The four hexadecimal digits of one escape, and false for anything shorter or
// spelt with something else.
bool readFourHex(const std::string &in, size_t &i, unsigned long &out)
{
	if (in.size() - i < 4)
		return false;
	unsigned long v = 0;
	for (size_t k = 0; k < 4; ++k)
	{
		const int d = hexValue(in[i + k]);
		if (d < 0)
			return false;
		v = (v << 4) | (unsigned long) d;
	}
	i += 4;
	out = v;
	return true;
}

/* One quoted string, with the opening quote already read. A half of a surrogate pair
   that is not followed by its other half becomes U+FFFD rather than being refused:
   it is a spelling the grammar permits and names no character, so there is nothing
   to put there but the character the standard sets aside for saying so. Refusing
   would turn one unusable character into a whole request nobody can send. */
bool readString(const std::string &in, size_t &i, std::string &out)
{
	out.clear();
	while (i < in.size())
	{
		const unsigned char c = (unsigned char) in[i];
		if (c == '"')
		{
			++i;
			return true;
		}
		/* A raw control byte is not text a document may carry, and one that
		   ends a line would be read back as a line of its own by anything this
		   value is later written to. */
		if (c < 0x20)
			return false;
		if (c != '\\')
		{
			out += (char) c;
			++i;
			continue;
		}

		++i;
		if (i >= in.size())
			return false;
		const char e = in[i++];
		switch (e)
		{
			case '"':  out += '"';  break;
			case '\\': out += '\\'; break;
			case '/':  out += '/';  break;
			case 'b':  out += '\b'; break;
			case 'f':  out += '\f'; break;
			case 'n':  out += '\n'; break;
			case 'r':  out += '\r'; break;
			case 't':  out += '\t'; break;
			case 'u':
			{
				unsigned long cp = 0;
				if (!readFourHex(in, i, cp))
					return false;
				if (cp >= 0xd800 && cp <= 0xdbff)
				{
					unsigned long low = 0;
					size_t after = i;
					if (after + 1 < in.size() && in[after] == '\\' && in[after + 1] == 'u')
					{
						size_t at = after + 2;
						if (readFourHex(in, at, low) && low >= 0xdc00 && low <= 0xdfff)
						{
							cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
							i = at;
						}
						else
						{
							cp = 0xfffd;
						}
					}
					else
					{
						cp = 0xfffd;
					}
				}
				else if (cp >= 0xdc00 && cp <= 0xdfff)
				{
					// The second half of a pair with no first half, which
					// names no character either.
					cp = 0xfffd;
				}
				appendCodepoint(out, cp);
				break;
			}
			default:
				return false;
		}
	}
	// The document ended inside the string.
	return false;
}

/* One number as the text it was written with, held to the grammar rather than
   converted here. Converting it is the business of whichever row the value
   belongs to, and a reader that turned it into a double first would hand that
   row a value that had already lost something. */
bool readNumber(const std::string &in, size_t &i, std::string &out)
{
	const size_t begin = i;

	if (i < in.size() && in[i] == '-')
		++i;

	if (i >= in.size())
		return false;
	if (in[i] == '0')
	{
		++i;
	}
	else if (in[i] >= '1' && in[i] <= '9')
	{
		while (i < in.size() && in[i] >= '0' && in[i] <= '9')
			++i;
	}
	else
	{
		return false;
	}

	if (i < in.size() && in[i] == '.')
	{
		++i;
		const size_t first = i;
		while (i < in.size() && in[i] >= '0' && in[i] <= '9')
			++i;
		if (i == first)
			return false;
	}

	if (i < in.size() && (in[i] == 'e' || in[i] == 'E'))
	{
		++i;
		if (i < in.size() && (in[i] == '+' || in[i] == '-'))
			++i;
		const size_t first = i;
		while (i < in.size() && in[i] >= '0' && in[i] <= '9')
			++i;
		if (i == first)
			return false;
	}

	out.assign(in, begin, i - begin);
	return true;
}

bool readWord(const std::string &in, size_t &i, const char *word)
{
	const size_t n = std::strlen(word);
	if (in.size() - i < n)
		return false;
	if (in.compare(i, n, word) != 0)
		return false;
	i += n;
	return true;
}

} // namespace

bool readFlatObject(const std::string &in, std::vector<JsonMember> &out)
{
	out.clear();

	size_t i = 0;
	skipSpace(in, i);
	/* Nothing at all is an object with no members. A request to a route that
	   declares no value carried in a body sends no body, and answering that as
	   a document that could not be read would refuse it for a reason that has
	   nothing to do with what it asked. */
	if (i >= in.size())
		return true;

	if (in[i] != '{')
		return false;
	++i;

	skipSpace(in, i);
	if (i < in.size() && in[i] == '}')
	{
		++i;
		skipSpace(in, i);
		return i >= in.size();
	}

	for (;;)
	{
		skipSpace(in, i);
		if (i >= in.size() || in[i] != '"')
			return false;
		++i;

		JsonMember m;
		if (!readString(in, i, m.name))
			return false;
		if (m.name.size() > kMaxMemberNameBytes)
			return false;
		/* A name is compared against the table's names and, for one no table has,
		   written back into the answer that says so. Both of those read it as a C
		   string and would stop at a zero byte, so a name carrying one would be
		   compared as less than it is. */
		if (m.name.find('\0') != std::string::npos)
			return false;

		skipSpace(in, i);
		if (i >= in.size() || in[i] != ':')
			return false;
		++i;
		skipSpace(in, i);
		if (i >= in.size())
			return false;

		if (in[i] == '"')
		{
			++i;
			if (!readString(in, i, m.text))
				return false;
			m.kind = JsonValueKind::String;
		}
		else if (readWord(in, i, "true"))
		{
			m.text = "true";
			m.kind = JsonValueKind::Bool;
		}
		else if (readWord(in, i, "false"))
		{
			m.text = "false";
			m.kind = JsonValueKind::Bool;
		}
		else
		{
			/* Everything left is a number or is nothing this reads: an object,
			   an array and a null are each refused rather than taken apart or
			   passed over, for the reason the header beside this gives. */
			if (!readNumber(in, i, m.text))
				return false;
			m.kind = JsonValueKind::Number;
		}

		if (out.size() >= kMaxBodyMembers)
			return false;
		out.push_back(m);

		skipSpace(in, i);
		if (i >= in.size())
			return false;
		if (in[i] == ',')
		{
			++i;
			continue;
		}
		if (in[i] != '}')
			return false;
		++i;
		break;
	}

	// Anything after the object is a second document, and taking the first of
	// two would act on half of what arrived.
	skipSpace(in, i);
	return i >= in.size();
}

namespace
{

/* The walk itself. Written apart from the answer below so that every way out
   of it can say what it found and one place puts the list back to nothing,
   rather than each refusal having to remember to. */
ArrayRead walkStringArray(const std::string &in, size_t max_items,
                          std::vector<std::string> &out)
{
	size_t i = 0;
	skipSpace(in, i);
	/* Nothing at all is not an empty array. A route whose body is its request
	   was sent no request, and reading that as a list of nothing would hand a
	   handler the one list it is most likely to act on by mistake. */
	if (i >= in.size() || in[i] != '[')
		return ArrayRead::NotAnArray;
	++i;

	skipSpace(in, i);
	if (i < in.size() && in[i] == ']')
	{
		++i;
		skipSpace(in, i);
		return (i >= in.size()) ? ArrayRead::Ok : ArrayRead::NotAnArray;
	}

	for (;;)
	{
		skipSpace(in, i);
		/* Everything that is not a string is refused rather than taken apart
		   or passed over: a number, a boolean, a null, an object and an array
		   each mean something no route here offers. */
		if (i >= in.size() || in[i] != '"')
			return ArrayRead::NotAnArray;
		++i;

		// Counted before the element is read rather than after it is kept, so
		// that a body past the ceiling costs the elements up to it and no more.
		if (out.size() >= max_items)
			return ArrayRead::TooMany;

		std::string one;
		if (!readString(in, i, one))
			return ArrayRead::NotAnArray;
		out.push_back(one);

		skipSpace(in, i);
		if (i >= in.size())
			return ArrayRead::NotAnArray;
		if (in[i] == ',')
		{
			++i;
			continue;
		}
		if (in[i] != ']')
			return ArrayRead::NotAnArray;
		++i;
		break;
	}

	// Anything after the array is a second document, for the reason the object
	// reader gives.
	skipSpace(in, i);
	return (i >= in.size()) ? ArrayRead::Ok : ArrayRead::NotAnArray;
}

} // namespace

ArrayRead readStringArray(const std::string &in, size_t max_items,
                          std::vector<std::string> &out)
{
	out.clear();
	const ArrayRead read = walkStringArray(in, max_items, out);
	if (read != ArrayRead::Ok)
		out.clear();
	return read;
}

} // namespace httpd
