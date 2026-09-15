/*
 * browserplay.cpp - which channels a browser can be handed, and how
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

#include "browserplay.h"

#include <cstdio>

namespace coreapi
{
namespace browserplay
{

namespace
{

/* The one program that does the work, named absolutely. A name looked up on a
   path is a name whatever is first on that path answers to, and this process
   spawns it as the user the box runs as. */
const char kFfmpeg[] = "/usr/bin/ffmpeg";

/* Stereo and 128 kbit/s whatever came in, including a channel that carried six
   channels of AC-3. A browser plays through a laptop or a telephone, encoding
   six channels costs a quarter more than encoding two, and a five channel mix
   folded down by the box is a mix somebody can hear. */
const char kAudioRate[] = "128k";
const char kAudioChannels[] = "2";

/* How much of the source is read before the muxing starts. The defaults are five
   megabytes and five seconds, which on a live stream is five seconds of somebody
   looking at nothing; a transport stream states its programme in the first
   tables it carries, so half a megabyte and one second is enough. */
const char kProbeSize[] = "500k";
const char kAnalyseDuration[] = "1000000";

bool isPicture(const std::string &codec)
{
	// cavs is deliberately absent: no browser decodes it, so it belongs with
	// the pictures that are not offered.
	return codec == "h264" || codec == "hevc";
}

/* Whether the box has a decoder for this sound. Every name here is a decoder the
   buildsystem switches on. */
bool canDecodeSound(const std::string &codec)
{
	return codec == "mp2" || codec == "ac3" || codec == "eac3" ||
	       codec == "aac" || codec == "aacplus" || codec == "dts" ||
	       codec == "dtshd" || codec == "lpcm";
}

void addPair(std::vector<std::string> &out, const char *a, const std::string &b)
{
	out.push_back(a);
	out.push_back(b);
}

} // namespace

Path pathFor(const std::string &video_codec, const std::string &audio_codec)
{
	/* Nothing known about one half is nothing known, and a box that refused
	   every channel nobody had been on would refuse almost all of them: what a
	   channel carries is learned by tuning it, and the scan does not write it
	   down. */
	if (video_codec == "unknown" || audio_codec == "unknown")
		return Path::Blind;

	/* Sound the box cannot decode ends it whatever the picture is. An unnamed
	   codec never reaches here: the gate above takes "unknown" and opens the
	   channel instead, so what this refuses is a name this build has no decoder
	   for. */
	if (!canDecodeSound(audio_codec))
		return Path::NotOffered;

	/* Radio, which the channel route spells as a channel with no picture. */
	if (video_codec == "none")
		return (audio_codec == "aac") ? Path::Copy : Path::ConvertSound;

	if (!isPicture(video_codec))
		return Path::NotOffered;

	return (audio_codec == "aac") ? Path::Copy : Path::ConvertSound;
}

const char *pathName(Path p)
{
	switch (p)
	{
		case Path::Copy:         return "copy";
		case Path::ConvertSound: return "convert-sound";
		case Path::Blind:        return "blind";
		case Path::NotOffered:   return "not-offered";
	}
	return "not-offered";
}

std::vector<std::string> commandFor(Path p, bool radio, const std::string &source,
                                    unsigned audio_pid)
{
	std::vector<std::string> argv;
	if (p == Path::NotOffered || source.empty())
		return argv;

	argv.push_back(kFfmpeg);
	// Nothing of this process's own input belongs to it, and a program that
	// asks a question on a terminal that is not there waits for an answer that
	// is never coming.
	argv.push_back("-nostdin");
	argv.push_back("-hide_banner");
	addPair(argv, "-loglevel", "error");
	// One thread. What this does is a demux, one audio codec and a mux, and a
	// second thread on a box with two of them is a thread taken off the picture
	// somebody is watching on the television.
	addPair(argv, "-threads", "1");
	addPair(argv, "-probesize", kProbeSize);
	addPair(argv, "-analyzeduration", kAnalyseDuration);
	// The reader is a browser that wants the first frame now, not a file being
	// written where a full buffer costs nothing.
	addPair(argv, "-fflags", "+nobuffer");
	addPair(argv, "-i", source);

	// The picture first, so the stream a player finds at position zero is the
	// one it draws.
	if (!radio)
	{
		addPair(argv, "-map", "0:v:0");
		addPair(argv, "-c:v", "copy");
	}

	/* The sound is named by its pid where the caller knows it, because a channel
	   carries several and the one a person chose is not always the first. In a
	   transport stream the identifier of a stream is its pid, so this is the pid
	   and not a position that shifts when the programme map changes. */
	if (audio_pid != 0)
	{
		char spec[32];
		std::snprintf(spec, sizeof(spec), "0:i:0x%x", audio_pid);
		addPair(argv, "-map", spec);
	}
	else
	{
		addPair(argv, "-map", "0:a:0");
	}

	if (p == Path::Copy)
	{
		addPair(argv, "-c:a", "copy");
	}
	else
	{
		// Blind and converting are the same command. What separates them is
		// what the box is able to say about it beforehand, not what it does.
		addPair(argv, "-c:a", "aac");
		addPair(argv, "-b:a", kAudioRate);
		addPair(argv, "-ac", kAudioChannels);
	}

	addPair(argv, "-f", radio ? "adts" : "mpegts");
	if (!radio)
	{
		// The mpegts muxer holds back half a second by default so that a file
		// starts cleanly. Nothing reads this as a file.
		addPair(argv, "-muxdelay", "0");
		addPair(argv, "-muxpreload", "0");
	}
	argv.push_back("pipe:1");

	return argv;
}

const char *typeFor(Path p, bool radio)
{
	if (p == Path::NotOffered)
		return NULL;
	/* aac and not mpeg for the sound alone: what goes out is an ADTS stream,
	   which is what an audio element takes without anything in front of it. */
	return radio ? "audio/aac" : "video/mp2t";
}

} // namespace browserplay
} // namespace coreapi
