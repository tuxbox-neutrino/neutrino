/*
 * test_browserplay.cpp - tests for which channels a browser can be handed
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

#include "coreapi/browserplay.h"

#include <string>
#include <vector>

/* The three roads a channel can take to a browser, driven on their own.

   On their own because everything that decides is a pair of codec names and
   the deciding is the whole of it: no box, no tuner, no program started. What
   a case here catches is the difference between a channel that is handed over
   and a channel that is refused, and the refusal is the thing worth being sure
   of. A channel wrongly refused is a sender somebody is told does not work; a
   channel wrongly offered is a black rectangle with no reason under it, which
   is the fault this whole road exists to avoid.

   The command is checked as a list and not as a line, because that is what it
   is. What matters about it is that the picture is copied and never encoded,
   which is the one decision the weakest box this runs on cannot survive being
   got wrong. */

using coreapi::browserplay::Path;

namespace
{

// Whether an argument list carries a pair, in that order and next to each
// other. An option and its value are one thing and finding them apart is
// finding neither.
bool carries(const std::vector<std::string> &argv, const std::string &a, const std::string &b)
{
	for (size_t i = 0; i + 1 < argv.size(); ++i)
	{
		if (argv[i] == a && argv[i + 1] == b)
			return true;
	}
	return false;
}

bool carries(const std::vector<std::string> &argv, const std::string &a)
{
	for (size_t i = 0; i < argv.size(); ++i)
	{
		if (argv[i] == a)
			return true;
	}
	return false;
}

std::vector<std::string> tvCommand(Path p)
{
	return coreapi::browserplay::commandFor(p, false, "http://127.0.0.1:31339/id=abc", 0);
}

} // namespace

TEST_CASE("a picture no browser decodes is not offered", "[browserplay]")
{
	/* The decision this whole road rests on. Standard definition on a
	   satellite is MPEG-2, no browser has decoded it for years, and the box
	   cannot encode a picture, so the honest answer is that the channel is not
	   offered rather than a player that draws nothing. */
	CHECK(coreapi::browserplay::pathFor("mpeg2", "mp2") == Path::NotOffered);
	CHECK(coreapi::browserplay::pathFor("mpeg2", "aac") == Path::NotOffered);
	CHECK(coreapi::browserplay::pathFor("mpeg2", "ac3") == Path::NotOffered);
	// Nothing outside China decodes this one, and nothing at all in a browser.
	CHECK(coreapi::browserplay::pathFor("cavs", "aac") == Path::NotOffered);
	// A name this program does not know is not a name to guess about.
	CHECK(coreapi::browserplay::pathFor("vp9", "aac") == Path::NotOffered);
	CHECK(coreapi::browserplay::pathFor("", "aac") == Path::NotOffered);
}

TEST_CASE("sound the box cannot decode ends it whatever the picture is", "[browserplay]")
{
	CHECK(coreapi::browserplay::pathFor("h264", "") == Path::NotOffered);
	CHECK(coreapi::browserplay::pathFor("none", "") == Path::NotOffered);
}

TEST_CASE("a channel nobody has been on is opened rather than refused", "[browserplay]")
{
	/* THE ORDINARY CASE, not an odd one. A box learns what a channel carries
	   by tuning it and a scan writes none of it down, so every channel is cold
	   until somebody watches it. Refusing them all would be refusing the box. */
	CHECK(coreapi::browserplay::pathFor("unknown", "unknown") == Path::Blind);
	CHECK(coreapi::browserplay::pathFor("h264", "unknown") == Path::Blind);
	CHECK(coreapi::browserplay::pathFor("unknown", "mp2") == Path::Blind);
	CHECK(coreapi::browserplay::pathFor("none", "unknown") == Path::Blind);
	// Even a picture that is known to be hopeless: what decides is the half
	// that is unknown, and this one is not it.
	CHECK(coreapi::browserplay::pathFor("mpeg2", "unknown") == Path::Blind);

	/* And it does the one thing that is right whatever the channel turns out
	   to be. The command is the converting one, because a sound that was
	   already AAC costs an encode nobody notices and a sound that was not is
	   the whole reason this road exists. */
	const std::vector<std::string> blind = tvCommand(Path::Blind);
	REQUIRE(!blind.empty());
	CHECK(blind == tvCommand(Path::ConvertSound));
}

TEST_CASE("a picture a browser decodes goes through untouched", "[browserplay]")
{
	// Already AAC, so there is nothing at all for the box to do but copy.
	CHECK(coreapi::browserplay::pathFor("h264", "aac") == Path::Copy);
	CHECK(coreapi::browserplay::pathFor("hevc", "aac") == Path::Copy);

	/* The common case on German satellite television: the picture is H.264 and
	   the sound is Layer II or AC-3, neither of which any browser plays. */
	CHECK(coreapi::browserplay::pathFor("h264", "mp2") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("h264", "ac3") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("h264", "eac3") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("h264", "aacplus") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("hevc", "ac3") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("h264", "dts") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("h264", "dtshd") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("h264", "lpcm") == Path::ConvertSound);
}

TEST_CASE("radio is a channel with no picture", "[browserplay]")
{
	// Most radio on a satellite is Layer II, so the cheap case is the rare one.
	CHECK(coreapi::browserplay::pathFor("none", "mp2") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("none", "aac") == Path::Copy);
	CHECK(coreapi::browserplay::pathFor("none", "aacplus") == Path::ConvertSound);
	CHECK(coreapi::browserplay::pathFor("none", "ac3") == Path::ConvertSound);
}

TEST_CASE("the picture is copied and never encoded", "[browserplay]")
{
	const std::vector<std::string> copy = tvCommand(Path::Copy);
	const std::vector<std::string> convert = tvCommand(Path::ConvertSound);

	REQUIRE(!copy.empty());
	REQUIRE(!convert.empty());

	CHECK(carries(copy, "-c:v", "copy"));
	CHECK(carries(convert, "-c:v", "copy"));

	/* Said as a refusal and not only as an expectation: an encoder named for
	   the picture is the one mistake this program cannot make on a box that
	   decodes in hardware and has nothing left over. */
	bool encodes_picture = false;
	for (size_t i = 0; i + 1 < convert.size(); ++i)
	{
		if (convert[i] == "-c:v" && convert[i + 1] != "copy")
			encodes_picture = true;
	}
	CHECK_FALSE(encodes_picture);
}

TEST_CASE("the sound is copied or encoded and nothing else", "[browserplay]")
{
	const std::vector<std::string> copy = tvCommand(Path::Copy);
	const std::vector<std::string> convert = tvCommand(Path::ConvertSound);

	CHECK(carries(copy, "-c:a", "copy"));
	CHECK(carries(convert, "-c:a", "aac"));
	// Folded to two channels: a browser plays through a laptop, and six
	// channels cost more to encode than the two anybody hears.
	CHECK(carries(convert, "-ac", "2"));
	CHECK_FALSE(carries(copy, "-ac", "2"));
}

TEST_CASE("a channel that is not offered has no command", "[browserplay]")
{
	CHECK(tvCommand(Path::NotOffered).empty());
	CHECK(coreapi::browserplay::commandFor(Path::Copy, false, "", 0).empty());
	CHECK(coreapi::browserplay::typeFor(Path::NotOffered, false) == NULL);
	CHECK(coreapi::browserplay::typeFor(Path::NotOffered, true) == NULL);
}

TEST_CASE("radio is muxed as ADTS and television as a transport stream", "[browserplay]")
{
	const std::vector<std::string> radio =
		coreapi::browserplay::commandFor(Path::ConvertSound, true,
		                                 "http://127.0.0.1:31339/id=abc", 0);
	const std::vector<std::string> tv = tvCommand(Path::ConvertSound);

	CHECK(carries(radio, "-f", "adts"));
	CHECK(carries(tv, "-f", "mpegts"));

	// Radio has no picture to map, and mapping one would end the program on a
	// stream the source does not carry.
	CHECK_FALSE(carries(radio, "-map", "0:v:0"));
	CHECK(carries(tv, "-map", "0:v:0"));

	CHECK(std::string(coreapi::browserplay::typeFor(Path::ConvertSound, true)) == "audio/aac");
	CHECK(std::string(coreapi::browserplay::typeFor(Path::Copy, false)) == "video/mp2t");
}

TEST_CASE("the sound is taken from the pid the caller named", "[browserplay]")
{
	const std::vector<std::string> first =
		coreapi::browserplay::commandFor(Path::ConvertSound, false,
		                                 "http://127.0.0.1:31339/id=abc", 0);
	const std::vector<std::string> named =
		coreapi::browserplay::commandFor(Path::ConvertSound, false,
		                                 "http://127.0.0.1:31339/id=abc", 0x1f4);

	// Nothing named means the first the source carries, which is what a
	// channel the box has never tuned still has.
	CHECK(carries(first, "-map", "0:a:0"));
	// A pid names a stream by its identifier, which in a transport stream is
	// the pid, so it survives a programme map that moves the streams about.
	CHECK(carries(named, "-map", "0:i:0x1f4"));
	CHECK_FALSE(carries(named, "-map", "0:a:0"));
}

TEST_CASE("the source is passed as one argument and never through a shell", "[browserplay]")
{
	/* A caller cannot reach past the argument it is in. The identifier comes
	   out of a request, and a command line assembled around one would be a
	   line a caller can put its own commands into. */
	const std::string nasty = "http://127.0.0.1:31339/id=abc;rm -rf /";
	const std::vector<std::string> argv =
		coreapi::browserplay::commandFor(Path::Copy, false, nasty, 0);

	REQUIRE(!argv.empty());
	CHECK(argv[0] == "/usr/bin/ffmpeg");
	CHECK(carries(argv, "-i", nasty));
	CHECK_FALSE(carries(argv, "sh"));
	CHECK_FALSE(carries(argv, "-c"));
}

TEST_CASE("every path has a name that crosses the wire", "[browserplay]")
{
	CHECK(std::string(coreapi::browserplay::pathName(Path::Copy)) == "copy");
	CHECK(std::string(coreapi::browserplay::pathName(Path::ConvertSound)) == "convert-sound");
	CHECK(std::string(coreapi::browserplay::pathName(Path::Blind)) == "blind");
	CHECK(std::string(coreapi::browserplay::pathName(Path::NotOffered)) == "not-offered");
}
