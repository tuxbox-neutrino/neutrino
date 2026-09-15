/*
 * test_httpjson.cpp - tests for JSON in and out of a request
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
#include "httpd/json.h"

// The tree's own parser, used here as a second opinion. A case that only
// compares strings cannot catch an expectation that is itself wrong about JSON.
#include "jsoncpp/json/json.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <memory>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

/* The child restores the default action for the abort signal and silences its
   output because it inherits the runner's fatal condition handler, which would
   otherwise report the deliberate abort as a failure. The same shape as the
   one the result type is held to, because the guard here is the same kind of
   guard. */
bool abortsInChild(void (*body)())
{
	// the child's freopen would otherwise flush the runner's buffered output a
	// second time and make one run look like two
	fflush(0);

	pid_t pid = fork();
	if (pid < 0)
		return false;
	if (pid == 0)
	{
		signal(SIGABRT, SIG_DFL);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
		body();
		_exit(0);
	}
	int st = 0;
	waitpid(pid, &st, 0);
	return WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
}

bool parseDocument(const std::string &doc, ::Json::Value &root)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	std::string errs;
	return reader->parse(doc.data(), doc.data() + doc.size(), &root, &errs);
}

bool parses(const std::string &doc)
{
	::Json::Value root;
	return parseDocument(doc, root);
}

/* A single written value, read back out of a parser. Wrapped in an array so
   that what is compared is a value inside a document and not a parser's
   willingness to accept a bare one. The failures answer in text so that a case
   which does not parse says so rather than comparing an empty string. */
std::string parsedBack(const std::string &written_value)
{
	::Json::Value root;
	if (!parseDocument("[" + written_value + "]", root))
		return "<did not parse>";
	if (!root.isArray() || root.size() != 1 || !root[0].isString())
		return "<not one string>";
	return root[0].asString();
}

void openPastTheCeiling()
{
	std::string out;
	httpd::Json j(out);
	for (size_t i = 0; i <= httpd::Json::MaxDepth; ++i)
		j.beginArray();
}

void closeMoreThanWasOpened()
{
	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.endArray();
	j.endArray();
}

void nameAMemberWithNothing()
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key(NULL);
}

void nameAMemberAndCloseWithoutIt()
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("a");
	j.endObject();
}

void nameTwoMembersInARow()
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("a");
	j.key("b");
}

void writeAValueIntoAnObjectWithNoName()
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.value(1L);
}

void addAnEmptyFragment()
{
	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.raw("");
}

void closeAnObjectWithABracket()
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.endArray();
}

void closeAnArrayWithABrace()
{
	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.endObject();
}

/* What the escape of one byte below space has to be, stated here rather than
   read off the writer, so that the case compares two independent answers. The
   five named forms are the ones a reader recognises; everything else in the
   range has a numeric form and no other. */
std::string expectedEscape(int c)
{
	switch (c)
	{
		case '\b': return "\\b";
		case '\f': return "\\f";
		case '\n': return "\\n";
		case '\r': return "\\r";
		case '\t': return "\\t";
		default:   break;
	}
	char buf[16];
	snprintf(buf, sizeof(buf), "\\u%04x", c);
	return std::string(buf);
}

} // namespace

TEST_CASE("an object writes its separators without a trailing comma", "[json]")
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("a"); j.value(1L);
	j.key("b"); j.value("x");
	j.endObject();
	REQUIRE(out == "{\"a\":1,\"b\":\"x\"}");
	REQUIRE(parses(out));
}

TEST_CASE("an empty object and an empty array are still valid", "[json]")
{
	std::string a, b;
	{ httpd::Json j(a); j.beginObject(); j.endObject(); }
	{ httpd::Json j(b); j.beginArray();  j.endArray();  }
	REQUIRE(a == "{}");
	REQUIRE(b == "[]");
	REQUIRE(parses(a));
	REQUIRE(parses(b));
}

TEST_CASE("nesting resets the separator state per level", "[json]")
{
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("list"); j.beginArray(); j.value(1L); j.value(2L); j.endArray();
	j.key("n");    j.value(3L);
	j.endObject();
	REQUIRE(out == "{\"list\":[1,2],\"n\":3}");
	REQUIRE(parses(out));
}

TEST_CASE("a list of objects separates the members and the elements apart", "[json]")
{
	/* Three levels, each of which has to be counting on its own: the outer
	   array between its elements, each element between its members, and the
	   inner array between its numbers. A writer holding one flag for the whole
	   document reads as correct on the case above and not on this one. */
	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.beginObject();
	j.key("id");   j.value(1L);
	j.key("tags"); j.beginArray(); j.value("a"); j.value("b"); j.endArray();
	j.endObject();
	j.beginObject();
	j.key("id");   j.value(2L);
	j.key("tags"); j.beginArray(); j.endArray();
	j.endObject();
	j.endArray();
	REQUIRE(out == "[{\"id\":1,\"tags\":[\"a\",\"b\"]},{\"id\":2,\"tags\":[]}]");
	REQUIRE(parses(out));
}

TEST_CASE("a member whose value is a container is still one member", "[json]")
{
	// The comma before a nested container belongs to the level the container
	// sits in, and the key it follows must not gain one of its own.
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("a"); j.beginObject(); j.endObject();
	j.key("b"); j.beginArray();  j.endArray();
	j.endObject();
	REQUIRE(out == "{\"a\":{},\"b\":[]}");
	REQUIRE(parses(out));
}

TEST_CASE("the writer appends to what the buffer already holds", "[json]")
{
	// A handler assembling a reply keeps the one buffer it has, so nothing here
	// may clear it or write over its front.
	std::string out = "prefix:";
	httpd::Json j(out);
	j.beginArray(); j.value(1L); j.endArray();
	REQUIRE(out == "prefix:[1]");
}

TEST_CASE("the two characters JSON must escape are escaped", "[json]")
{
	std::string out;
	httpd::Json j(out);
	const std::string in = "he said \"hi\" \\ done";
	j.value(in);
	REQUIRE(out == "\"he said \\\"hi\\\" \\\\ done\"");
	REQUIRE(parsedBack(out) == in);
}

TEST_CASE("every control byte below space leaves as an escape", "[json]")
{
	/* Compared against the whole document and not against the absence of the
	   raw byte: dropping the byte, replacing it with a question mark or writing
	   a numeric escape with the digits of some other byte all leave nothing raw
	   behind, and all three are wrong. */
	for (int c = 0; c < 0x20; ++c)
	{
		std::string in(1, (char) c);
		std::string out;
		httpd::Json j(out);
		j.value(in);
		INFO("byte " << c);
		REQUIRE(out == "\"" + expectedEscape(c) + "\"");
		// Nothing below space may appear raw, whatever form the escape takes.
		REQUIRE(out.find((char) c) == std::string::npos);
		// And what a parser reads back is the byte that went in, which is the
		// property the escape exists for rather than its spelling.
		REQUIRE(parsedBack(out) == in);
	}
}

TEST_CASE("the first byte at space is copied and not escaped", "[json]")
{
	/* The boundary the control range ends at, which a comparison off by one
	   would move: space is a character a label carries and delete is a byte
	   JSON does not ask anyone to escape. */
	std::string out;
	httpd::Json j(out);
	std::string in = " ";
	in += '\x7f';
	j.value(in);
	REQUIRE(out == "\" \x7f\"");
	REQUIRE(parsedBack(out) == in);
}

TEST_CASE("a forward slash is left alone", "[json]")
{
	// Escaping it is allowed and pointless, and every path this server answers
	// with carries several.
	std::string out;
	httpd::Json j(out);
	j.value("/errors/no-such-channel");
	REQUIRE(out == "\"/errors/no-such-channel\"");
	REQUIRE(parsedBack(out) == "/errors/no-such-channel");
}

TEST_CASE("a zero byte inside a value does not truncate it", "[json]")
{
	std::string in("a");
	in.push_back('\0');
	in.push_back('b');
	std::string out;
	httpd::Json j(out);
	j.value(in);
	REQUIRE(out == "\"a\\u0000b\"");
	// Three bytes back out of the parser, not one and not two.
	const std::string back = parsedBack(out);
	REQUIRE(back.size() == 3);
	REQUIRE(back == in);
}

TEST_CASE("bytes above ASCII pass through untouched", "[json]")
{
	// Text is copied and not re-encoded. What is checked one case below is the
	// other half, the bytes that are not text at all.
	const std::string in = "Ka\xc3\xa4se \xe2\x82\xac";
	std::string out;
	httpd::Json j(out);
	j.value(in);
	REQUIRE(out == "\"" + in + "\"");
	REQUIRE(parsedBack(out) == in);
}

TEST_CASE("a byte that is not text is replaced rather than sent on", "[json]")
{
	/* A body carrying one is not a document: a strict reader refuses the whole of it, so
	   one string nobody converted would cost a client every answer that carries it. The
	   tree's own parser takes such a body, which is why this is asked of the bytes.

	   Every shape that is not well formed, and not only the obvious one. Three of them
	   look right to a reading that takes a length off the lead byte and counts
	   continuations: a character spelled in more room than it needs, one half of a
	   surrogate pair, and a number above the last character there is. */
	struct Case
	{
		const char *name;
		const char *in;
		size_t      in_size;
		const char *out;
	};

	/* Every literal below ends its escapes before the letter that follows,
	   because a hex escape takes as many hex digits as it can reach and the
	   letter b is one of them. */
	static const char kFffd[] = "\xef\xbf\xbd";

	static const Case cases[] = {
		{ "a lone lead byte", "Pro\xdf" "Sieben", 10, "\"Pro\xef\xbf\xbd" "Sieben\"" },
		{ "a stray continuation", "a\x80" "b", 3, "\"a\xef\xbf\xbd" "b\"" },
		{ "a two byte overlong", "a\xc0\xaf" "b", 4, "\"a\xef\xbf\xbd\xef\xbf\xbd" "b\"" },
		{ "a three byte overlong", "a\xe0\x80\xaf" "b", 5,
		  "\"a\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd" "b\"" },
		{ "half a surrogate pair", "a\xed\xa0\x80" "b", 5,
		  "\"a\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd" "b\"" },
		{ "above the last character", "a\xf4\x90\x80\x80" "b", 6,
		  "\"a\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd" "b\"" },
		{ "a lead byte with nothing after it", "a\xe2\x82", 3,
		  "\"a\xef\xbf\xbd\xef\xbf\xbd\"" },
		{ "a byte that begins nothing", "a\xff" "b", 3, "\"a\xef\xbf\xbd" "b\"" },
	};

	// Named so that a reader can see what the replacement is without counting
	// bytes, and read here so the name is not unused.
	REQUIRE(std::string(kFffd).size() == 3);

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		INFO(cases[i].name);
		std::string out;
		httpd::Json j(out);
		j.value(std::string(cases[i].in, cases[i].in_size));
		REQUIRE(out == std::string(cases[i].out));
		// And what came out is a string a parser reads back, which is the
		// point of replacing rather than dropping.
		REQUIRE(parsedBack(out).size() > 0);
	}
}

TEST_CASE("text at every width is copied and not replaced", "[json]")
{
	/* The other direction, so the replacement above cannot be a writer that
	   replaces everything it does not recognise. One character of each length,
	   including the two the range checks sit either side of: the first three
	   byte character, and the last character there is. */
	const std::string in = "a\xc2\x80\xe0\xa0\x80\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\xed\x9f\xbf";
	std::string out;
	httpd::Json j(out);
	j.value(in);
	REQUIRE(out == "\"" + in + "\"");
	REQUIRE(parsedBack(out) == in);
}

TEST_CASE("a key is escaped the way a value is", "[json]")
{
	// A key comes from a table here today and from a settings name tomorrow,
	// and an unescaped one ends the document just as surely as a value does.
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("a\"b\\c\nd");
	j.value(1L);
	j.endObject();
	REQUIRE(out == "{\"a\\\"b\\\\c\\nd\":1}");
	REQUIRE(parses(out));
	// The name the parser hands back is the one that was written, escapes and
	// all, which a string comparison against an expectation cannot say.
	::Json::Value root;
	REQUIRE(parseDocument(out, root));
	REQUIRE(root.isMember("a\"b\\c\nd"));
}

TEST_CASE("a bool is a literal and not a number", "[json]")
{
	std::string out;
	httpd::Json j(out);
	j.beginObject(); j.key("t"); j.value(true); j.key("f"); j.value(false); j.endObject();
	REQUIRE(out == "{\"t\":true,\"f\":false}");
	::Json::Value root;
	REQUIRE(parseDocument(out, root));
	REQUIRE(root["t"].isBool());
	REQUIRE(root["t"].asBool() == true);
	REQUIRE(root["f"].asBool() == false);
}

TEST_CASE("null is written where there is no value", "[json]")
{
	std::string out;
	httpd::Json j(out);
	j.beginArray(); j.null(); j.null(); j.endArray();
	REQUIRE(out == "[null,null]");
	::Json::Value root;
	REQUIRE(parseDocument(out, root));
	REQUIRE(root[0].isNull());
}

TEST_CASE("a signed number keeps its sign and an unsigned one its range", "[json]")
{
	/* The unsigned end matters because the sizes and the identifiers this
	   server answers with are unsigned, and printing one through the signed
	   conversion turns the top of the range into a negative number. */
	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.value(-42L);
	j.value(0L);
	j.value((unsigned long) ULONG_MAX);
	j.endArray();

	char expected[128];
	snprintf(expected, sizeof(expected), "[-42,0,%lu]", ULONG_MAX);
	REQUIRE(out == std::string(expected));
	REQUIRE(parses(out));
}

TEST_CASE("a count reaches a number whatever width it was counted in", "[json]")
{
	/* Without the narrower two overloads this does not compile at all, and on
	   the machine the suite runs on a size is the wider unsigned and would
	   compile anyway. What is checked here is therefore that each width lands
	   on the right one of the two conversions, signed or not. */
	const int      i = -7;
	const unsigned u = 7u;
	const size_t   n = 7;
	const char     c = 'A';

	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.value(i);
	j.value(u);
	j.value(n);
	j.value(c);
	j.endArray();
	REQUIRE(out == "[-7,7,7,65]");
	REQUIRE(parses(out));
}

TEST_CASE("a double is written so that it reads back as itself", "[json]")
{
	// A tenth has no exact form in binary, so a writer printing the short
	// spelling of one hands back a different number from the one it was given.
	static const double values[] = { 0.1, -1.0 / 3.0, 1e-300, 2.5, 1.0, 0.0 };

	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
	{
		std::string out;
		httpd::Json j(out);
		j.value(values[i]);
		INFO(out);
		REQUIRE(strtod(out.c_str(), NULL) == values[i]);
		// A number and not a string that looks like one.
		REQUIRE(out.find('"') == std::string::npos);
		REQUIRE(parses("[" + out + "]"));
	}
}

TEST_CASE("a number that is not finite is written as the absence of one", "[json]")
{
	/* JSON has no spelling for either, and the C library prints one anyway, so
	   a reading that came back as a division by zero would otherwise leave a
	   body no client can parse. */
	const double zero = 0.0;
	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.value(1.0 / zero);
	j.value(-1.0 / zero);
	j.value(zero / zero);
	j.endArray();
	REQUIRE(out == "[null,null,null]");
	REQUIRE(parses(out));
}

TEST_CASE("a string that is not there is the absence of a value", "[json]")
{
	// A C interface answers a missing string with a null pointer, and reading
	// through it would take the whole reply down with it.
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("a"); j.value((const char *) NULL);
	j.key("b"); j.raw(NULL);
	j.endObject();
	REQUIRE(out == "{\"a\":null,\"b\":null}");
	REQUIRE(parses(out));
}

TEST_CASE("an already serialised fragment goes in as it stands", "[json]")
{
	// Nothing quotes or escapes it, which is the whole of its use: a document
	// held in a buffer already is copied in without a parse and a rewrite.
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("cached"); j.raw("{\"a\":[1,2]}");
	j.key("n");      j.value(1L);
	j.endObject();
	REQUIRE(out == "{\"cached\":{\"a\":[1,2]},\"n\":1}");
	REQUIRE(parses(out));
}

TEST_CASE("nesting as deep as the ceiling allows is written", "[json]")
{
	std::string out;
	httpd::Json j(out);
	for (size_t i = 0; i < httpd::Json::MaxDepth; ++i)
		j.beginArray();
	j.value(1L);
	for (size_t i = 0; i < httpd::Json::MaxDepth; ++i)
		j.endArray();

	std::string expected;
	for (size_t i = 0; i < httpd::Json::MaxDepth; ++i)
		expected += '[';
	expected += '1';
	for (size_t i = 0; i < httpd::Json::MaxDepth; ++i)
		expected += ']';
	REQUIRE(out == expected);
	// A ceiling that cannot be parsed at is a ceiling set in the wrong place.
	REQUIRE(parses(out));
}

TEST_CASE("opening past the ceiling aborts", "[json]")
{
	// The document that would come out of it is one no client can parse, and a
	// handler nesting that deep is wrong about the answer it is writing.
	REQUIRE(abortsInChild(openPastTheCeiling));
}

TEST_CASE("closing more than was opened aborts", "[json]")
{
	REQUIRE(abortsInChild(closeMoreThanWasOpened));
}

TEST_CASE("naming a member with nothing aborts", "[json]")
{
	REQUIRE(abortsInChild(nameAMemberWithNothing));
}

TEST_CASE("closing a level with the wrong bracket aborts", "[json]")
{
	// The bytes would go out looking like a document and no client could read
	// them, which is what the ceiling stops for as well.
	REQUIRE(abortsInChild(closeAnObjectWithABracket));
	REQUIRE(abortsInChild(closeAnArrayWithABrace));
}

TEST_CASE("the two kinds nested inside each other still close", "[json]")
{
	/* The guard above has to be refusing the mismatch and not the mixture: an
	   answer here is objects in arrays in objects, and a check that stopped
	   those would stop everything this server sends. */
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("rows");
	j.beginArray();
	j.beginObject();
	j.key("of"); j.beginArray(); j.value(1L); j.endArray();
	j.endObject();
	j.endArray();
	j.key("n"); j.value(1L);
	j.endObject();
	REQUIRE(out == "{\"rows\":[{\"of\":[1]}],\"n\":1}");
	REQUIRE(parses(out));
}

TEST_CASE("a decimal point the locale spells differently is rewritten", "[json]")
{
	/* The formatting of a double reads LC_NUMERIC, and a window elsewhere in this
	   program sets the process locale out of the environment and never puts it back. On
	   a German box that writes a comma, which turns one number into two members and
	   stops the whole body parsing. Driven with the point handed in rather than through
	   a locale, because no machine this suite is built on has one installed that spells
	   it any other way. */
	std::string out;

	out.clear(); httpd::appendNumber(out, "1,5", ",");
	REQUIRE(out == "1.5");

	out.clear(); httpd::appendNumber(out, "-1,1000000000000001e-300", ",");
	REQUIRE(out == "-1.1000000000000001e-300");

	// Whatever the locale calls it may be more than one byte, and the tail has
	// to start after all of it.
	out.clear(); httpd::appendNumber(out, "1\xd9\xab" "5", "\xd9\xab");
	REQUIRE(out == "1.5");

	// A whole number prints without one at all.
	out.clear(); httpd::appendNumber(out, "42", ",");
	REQUIRE(out == "42");

	// An exponent is not a second point, so only the first is rewritten.
	out.clear(); httpd::appendNumber(out, "1,5e-7", ",");
	REQUIRE(out == "1.5e-7");

	// A locale that already spells it the way JSON does changes nothing, and
	// neither does one that answers with nothing at all.
	out.clear(); httpd::appendNumber(out, "1.5", ".");
	REQUIRE(out == "1.5");
	out.clear(); httpd::appendNumber(out, "1.5", "");
	REQUIRE(out == "1.5");
	out.clear(); httpd::appendNumber(out, "1.5", NULL);
	REQUIRE(out == "1.5");

	// It appends, like everything else here.
	out = "x:"; httpd::appendNumber(out, "1,5", ",");
	REQUIRE(out == "x:1.5");
}

TEST_CASE("a double carries the point JSON has and no other", "[json]")
{
	// The end of the same path, under whatever locale the runner happens to be
	// in: what comes out is a number a parser reads as one.
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("temp"); j.value(1.5);
	j.key("p");    j.value(0.1);
	j.endObject();

	REQUIRE(out.find(',') != std::string::npos);   // the separator, and only it
	REQUIRE(out == "{\"temp\":1.5,\"p\":0.10000000000000001}");
	REQUIRE(parses(out));

	::Json::Value root;
	REQUIRE(parseDocument(out, root));
	REQUIRE(root["temp"].asDouble() == 1.5);
}

TEST_CASE("a name with no value after it aborts", "[json]")
{
	// Both spellings of the same mistake: the value that was meant to follow is
	// missing from the answer either way, and the bytes stop parsing as well.
	REQUIRE(abortsInChild(nameAMemberAndCloseWithoutIt));
	REQUIRE(abortsInChild(nameTwoMembersInARow));
}

TEST_CASE("a value with no name inside an object aborts", "[json]")
{
	REQUIRE(abortsInChild(writeAValueIntoAnObjectWithNoName));
}

TEST_CASE("an empty fragment aborts rather than leaving a bare comma", "[json]")
{
	// It writes nothing and takes the separator with it, so the next value in
	// the level arrives after a comma with nothing in front of it.
	REQUIRE(abortsInChild(addAnEmptyFragment));
}

TEST_CASE("a writer says whether it left anything open", "[json]")
{
	/* The one mistake of the set that cannot be stopped from in here: a handler
	   returning early between the two braces leaves a truncated body and this
	   writer has no destructor to object with. What it has instead is an answer
	   a handler or a case can ask for. */
	std::string out;
	httpd::Json j(out);
	REQUIRE(j.complete());

	j.beginObject();
	REQUIRE_FALSE(j.complete());

	j.key("a");
	REQUIRE_FALSE(j.complete());

	j.value(1L);
	REQUIRE_FALSE(j.complete());

	j.key("b");
	j.beginArray();
	REQUIRE_FALSE(j.complete());
	j.endArray();
	REQUIRE_FALSE(j.complete());

	j.endObject();
	REQUIRE(j.complete());
	REQUIRE(parses(out));
}

TEST_CASE("a truncated document is what complete refuses", "[json]")
{
	// The shape of an error path: something is written, the handler leaves, and
	// the bytes are not a document.
	std::string out;
	httpd::Json j(out);
	j.beginObject();
	j.key("a");
	j.value(1L);

	REQUIRE(out == "{\"a\":1");
	REQUIRE_FALSE(parses(out));
	REQUIRE_FALSE(j.complete());
}

TEST_CASE("the widths the box counts in all reach a number", "[json]")
{
	/* An identifier and a file size on the box are the widest of these, and
	   with any width left out the call names several candidates and matches
	   none of them better than the others. That is a build that fails at the
	   cross compiler and nowhere earlier, so every width is written here. */
	std::string out;
	httpd::Json j(out);
	j.beginArray();
	j.value((long long) -9007199254740993LL);
	j.value((unsigned long long) 18446744073709551615ULL);
	j.value((long long) 0);
	j.endArray();

	REQUIRE(out == "[-9007199254740993,18446744073709551615,0]");
	REQUIRE(parses(out));
}

TEST_CASE("the hint reserves without changing the output", "[json]")
{
	std::string a, b;
	{ httpd::Json j(a, 0);    j.beginArray(); j.value(1L); j.endArray(); }
	{ httpd::Json j(b, 4096); j.beginArray(); j.value(1L); j.endArray(); }
	REQUIRE(a == b);
	REQUIRE(b.capacity() >= 4096);
	// A buffer told nothing must not have reserved anyway, or the check above
	// would hold for a writer that ignores the number it was handed.
	REQUIRE(a.capacity() < 4096);
}

TEST_CASE("the hint is enough room to write that much without growing", "[json]")
{
	/* Capacity at or above the number asked for is also what a writer that
	   always reserves some large amount would show. What the hint is for is
	   that the buffer does not move while the document is written, so that is
	   what is compared. */
	const size_t hint = 4096;
	std::string out;
	httpd::Json j(out, hint);
	const size_t reserved = out.capacity();
	REQUIRE(reserved >= hint);

	j.beginArray();
	while (out.size() + 16 < hint)
		j.value(1234567890L);
	j.endArray();

	REQUIRE(out.size() > hint / 2);
	REQUIRE(out.capacity() == reserved);
}

TEST_CASE("the hint is room to add and not a total", "[json]")
{
	// A writer that starts on a buffer already holding a header would otherwise
	// ask for less than it needs and reallocate part way through.
	std::string out(1000, 'x');
	httpd::Json j(out, 4096);
	REQUIRE(out.capacity() >= 5096);
}

namespace
{
// Room for far more than any case here sends, so that what a case is about is
// what it wrote and never the ceiling.
const size_t kPlenty = 64;

httpd::ArrayRead readArray(const char *text, std::vector<std::string> &out)
{
	return httpd::readStringArray(std::string(text), kPlenty, out);
}
}

TEST_CASE("a body that is one array of strings is read into its elements", "[json]")
{
	std::vector<std::string> out;
	REQUIRE(readArray("[\"a\",\"bb\",\"ccc\"]", out) == httpd::ArrayRead::Ok);
	REQUIRE(out.size() == 3u);
	REQUIRE(out[0] == "a");
	REQUIRE(out[1] == "bb");
	REQUIRE(out[2] == "ccc");

	// The order is the document's, which is the whole of why a route would
	// carry a list rather than a set.
	REQUIRE(readArray("[\"ccc\",\"a\"]", out) == httpd::ArrayRead::Ok);
	REQUIRE(out.size() == 2u);
	REQUIRE(out[0] == "ccc");
	REQUIRE(out[1] == "a");

	// Whitespace between the parts is whitespace and nothing else.
	REQUIRE(readArray("  [ \"a\" ,\n\t\"b\" ]  ", out) == httpd::ArrayRead::Ok);
	REQUIRE(out.size() == 2u);
	REQUIRE(out[1] == "b");

	// An escape is read the way the object reader reads one, there being one
	// string reader under both.
	REQUIRE(readArray("[\"a\\u00e4b\"]", out) == httpd::ArrayRead::Ok);
	REQUIRE(out.size() == 1u);
	REQUIRE(out[0] == "a\xc3\xa4" "b");
}

TEST_CASE("an array with nothing in it is an array and not a fault", "[json]")
{
	/* The route above decides what an empty list means. Refusing it here would
	   make a body that says nothing read as a body that could not be read, and
	   a caller told to write it again would write the same thing. */
	std::vector<std::string> out;
	out.push_back("left over");
	REQUIRE(readArray("[]", out) == httpd::ArrayRead::Ok);
	REQUIRE(out.empty());
}

TEST_CASE("anything that is not an array of strings is refused whole", "[json]")
{
	std::vector<std::string> out;

	// Nothing at all. A route whose body is its request was sent no request,
	// and reading that as an empty list would hand a handler the one list it
	// is most likely to act on by mistake.
	REQUIRE(readArray("", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("   ", out) == httpd::ArrayRead::NotAnArray);

	// The shape every other body of this server has.
	REQUIRE(readArray("{\"a\":\"b\"}", out) == httpd::ArrayRead::NotAnArray);

	// An element that is not a string, one of each kind, because a reader that
	// took the outer half of one would act on half a request.
	REQUIRE(readArray("[1]", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[true]", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[null]", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[[\"a\"]]", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[{\"a\":\"b\"}]", out) == httpd::ArrayRead::NotAnArray);

	// Written wrong in the ways a document can be.
	REQUIRE(readArray("[\"a\"", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[\"a\",]", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[,\"a\"]", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[\"a\"] [\"b\"]", out) == httpd::ArrayRead::NotAnArray);
	REQUIRE(readArray("[\"a\"]x", out) == httpd::ArrayRead::NotAnArray);

	/* And nothing is handed back for any of them. A caller that read the list
	   before it read the answer would act on the part of a body that was well
	   formed, which is the half request this refuses. */
	REQUIRE(out.empty());
}

TEST_CASE("more elements than were asked for is its own answer", "[json]")
{
	/* Told apart from a body that is wrong, because a caller does something
	   else about it: one is a body to write again and the other is a request to
	   send in pieces. */
	std::vector<std::string> out;
	REQUIRE(httpd::readStringArray("[\"a\",\"b\"]", 2, out) == httpd::ArrayRead::Ok);
	REQUIRE(out.size() == 2u);

	REQUIRE(httpd::readStringArray("[\"a\",\"b\",\"c\"]", 2, out) == httpd::ArrayRead::TooMany);
	REQUIRE(out.empty());

	// Nothing at all is the ceiling a route would set to take no list, and an
	// array with something in it is then too many rather than unreadable.
	REQUIRE(httpd::readStringArray("[\"a\"]", 0, out) == httpd::ArrayRead::TooMany);
	REQUIRE(httpd::readStringArray("[]", 0, out) == httpd::ArrayRead::Ok);
}
