/*
 * test_channelstreams.cpp - tests for reading a channel's stream layout
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
#include "support/fakes.h"

#include "coreapi/channels.h"
#include "coreapi/box/channelstreams_internal.h"

#include <zapit/channel.h>
#include <zapit/client/zapittypes.h>

#include <string>

/* Real channels, built here the way the channel stack builds them, because
   what these hold is a rule about the box's own numbers and not about a record
   this suite filled in: the picture codec is a value whose nought means two
   things, and telling them apart is the whole of the answer.
   test_zapitchannel_copy.cpp defines the configuration object channel.cpp
   reaches, and one definition is all the binary may have. */

using namespace coreapi;

namespace
{

// The name has to be typed, or an empty literal picks the webtv constructor,
// which returns early on the null url.
CZapitChannel *makeChannel(unsigned char service_type)
{
	return new CZapitChannel(std::string("probe"), 0x2b66, service_type, 192, 11493);
}

CZapitChannel *makeTv()
{
	return makeChannel(ST_DIGITAL_TELEVISION_SERVICE);
}

// What the channel stack does when it has read a channel's table off the
// transponder: it keeps the section it read.
void markRead(CZapitChannel *c)
{
	c->setRawPmt(new unsigned char[8], 8);
}

} // anonymous namespace

TEST_CASE("a channel nobody has switched to says its streams are unknown", "[channelstreams]")
{
	CZapitChannel *cold = makeTv();

	ChannelStreams s;
	internal::fillStreams(*cold, s);

	REQUIRE_FALSE(s.known);
	REQUIRE(s.video_codec == VideoCodec::Unknown);
	REQUIRE(s.video_pid == 0);
	REQUIRE(s.audio.empty());

	delete cold;
}

TEST_CASE("nought is MPEG-2 and an unread channel at once, and the pid tells them apart",
          "[channelstreams]")
{
	/* The channel stack numbers MPEG-2 nought and constructs a channel with
	   nought, so the codec alone cannot say whether anything read it. Both
	   channels here carry type nought and they are two different answers. */
	CZapitChannel *unread = makeTv();
	REQUIRE(unread->type == CHANNEL_MPEG2);

	CZapitChannel *mpeg2 = makeTv();
	mpeg2->type = CHANNEL_MPEG2;
	mpeg2->setVideoPid(0x1fff);

	ChannelStreams a;
	ChannelStreams b;
	internal::fillStreams(*unread, a);
	internal::fillStreams(*mpeg2, b);

	REQUIRE(a.video_codec == VideoCodec::Unknown);
	REQUIRE(b.video_codec == VideoCodec::Mpeg2);

	delete unread;
	delete mpeg2;
}

TEST_CASE("every picture coding the channel stack writes becomes a word", "[channelstreams]")
{
	struct Case
	{
		int         type;
		VideoCodec  want;
	};
	const Case cases[] = {
		{ CHANNEL_MPEG2, VideoCodec::Mpeg2 },
		{ CHANNEL_MPEG4, VideoCodec::H264 },
		{ CHANNEL_HEVC,  VideoCodec::Hevc },
		{ CHANNEL_CAVS,  VideoCodec::Cavs },
		// A number out of a service list this box did not write. Read as
		// nothing known rather than as the codec it happens to sit beside.
		{ 99,            VideoCodec::Unknown },
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		INFO("channel type " << cases[i].type);
		CZapitChannel *c = makeTv();
		c->type = cases[i].type;
		c->setVideoPid(0x100);

		ChannelStreams s;
		internal::fillStreams(*c, s);
		REQUIRE(s.video_codec == cases[i].want);
		REQUIRE(s.video_pid == 0x100);
		delete c;
	}
}

TEST_CASE("a radio service carries no picture rather than an unknown one", "[channelstreams]")
{
	CZapitChannel *radio = makeChannel(ST_DIGITAL_RADIO_SOUND_SERVICE);
	markRead(radio);
	radio->addAudioChannel(0x100, CZapitAudioChannel::MPEG, "deu", 0);

	ChannelStreams s;
	internal::fillStreams(*radio, s);

	// None and not Unknown: the box knows this service carries no picture, and
	// a caller told unknown would go looking for one.
	REQUIRE(s.video_codec == VideoCodec::None);
	REQUIRE(s.video_pid == 0);
	REQUIRE(s.known);
	REQUIRE(s.audio.size() == 1);

	delete radio;
}

TEST_CASE("a channel the box has read carries its tracks with the playing one marked",
          "[channelstreams]")
{
	CZapitChannel *live = makeTv();
	live->type = CHANNEL_MPEG4;
	live->setVideoPid(0x17de);
	live->addAudioChannel(0x17e8, CZapitAudioChannel::MPEG, "deu", 0);
	live->addAudioChannel(0x17e9, CZapitAudioChannel::AC3, "deu", 1);
	live->addAudioChannel(0x17ea, CZapitAudioChannel::AACPLUS, "eng", 2);
	live->setAudioChannel(1);
	markRead(live);

	ChannelStreams s;
	internal::fillStreams(*live, s);

	REQUIRE(s.known);
	REQUIRE(s.video_codec == VideoCodec::H264);
	REQUIRE(s.video_pid == 0x17de);
	REQUIRE(s.audio.size() == 3);

	// In the order the table lists them, because a caller naming a track by
	// its place in this list is naming the box's own numbering.
	REQUIRE(s.audio[0].pid == 0x17e8);
	REQUIRE(s.audio[0].codec == AudioCodec::Mp2);
	REQUIRE(s.audio[0].description == "deu");
	REQUIRE_FALSE(s.audio[0].selected);

	REQUIRE(s.audio[1].pid == 0x17e9);
	REQUIRE(s.audio[1].codec == AudioCodec::Ac3);
	REQUIRE(s.audio[1].selected);

	REQUIRE(s.audio[2].codec == AudioCodec::AacPlus);
	REQUIRE(s.audio[2].description == "eng");
	REQUIRE_FALSE(s.audio[2].selected);

	delete live;
}

TEST_CASE("every sound coding the channel stack writes becomes a word", "[channelstreams]")
{
	struct Case
	{
		CZapitAudioChannel::ZapitAudioChannelType type;
		AudioCodec                                want;
	};
	const Case cases[] = {
		{ CZapitAudioChannel::MPEG,    AudioCodec::Mp2 },
		{ CZapitAudioChannel::AC3,     AudioCodec::Ac3 },
		{ CZapitAudioChannel::EAC3,    AudioCodec::Eac3 },
		{ CZapitAudioChannel::AAC,     AudioCodec::Aac },
		{ CZapitAudioChannel::AACPLUS, AudioCodec::AacPlus },
		{ CZapitAudioChannel::DTS,     AudioCodec::Dts },
		{ CZapitAudioChannel::DTSHD,   AudioCodec::DtsHd },
		{ CZapitAudioChannel::LPCM,    AudioCodec::Lpcm },
		{ CZapitAudioChannel::UNKNOWN, AudioCodec::Unknown },
	};
	const size_t count = sizeof(cases) / sizeof(cases[0]);
	// The whole of what the channel stack has words for, so a coding added
	// there and left out here is a case that stops holding.
	REQUIRE(count == 9);

	CZapitChannel *live = makeTv();
	markRead(live);
	for (size_t i = 0; i < count; ++i)
		live->addAudioChannel((unsigned short) (0x100 + i), cases[i].type, "deu", (unsigned char) i);

	ChannelStreams s;
	internal::fillStreams(*live, s);
	REQUIRE(s.audio.size() == count);
	for (size_t i = 0; i < count; ++i)
	{
		INFO("track " << i);
		REQUIRE(s.audio[i].codec == cases[i].want);
	}

	delete live;
}

TEST_CASE("what a scan wrote down is not answered as a track list", "[channelstreams]")
{
	/* A scan run with the box's pid option turned on writes the picture pid,
	   its coding and one bare sound pid into the service list, and the channel
	   stack loads all three back. What it does not write is a codec or a
	   language for that pid, so the track is not a track: answered as one it
	   would read as the whole of the channel's sound.

	   The picture is answered all the same, because it was read off a table
	   and is what the box holds. */
	CZapitChannel *scanned = makeTv();
	scanned->type = CHANNEL_MPEG4;
	scanned->setVideoPid(0x17de);
	scanned->setAudioPid(0x17e8);
	scanned->setPmtPid(0x1770);
	scanned->setPidsFlag();

	ChannelStreams s;
	internal::fillStreams(*scanned, s);

	REQUIRE_FALSE(s.known);
	REQUIRE(s.audio.empty());
	REQUIRE(s.video_codec == VideoCodec::H264);
	REQUIRE(s.video_pid == 0x17de);

	delete scanned;
}

TEST_CASE("the streams read forwards what the source says", "[channels]")
{
	FakeChannelSource fake;

	ChannelInfo c;
	c.id = 0xb9b0040200016dcbULL;
	c.name = "Das Erste HD";
	c.kind = ServiceKind::Tv;
	fake.channels.push_back(c);

	ChannelStreams known;
	known.known = true;
	known.video_codec = VideoCodec::H264;
	known.video_pid = 0x17de;
	AudioTrack t;
	t.pid = 0x17e8;
	t.codec = AudioCodec::Ac3;
	t.description = "deu";
	t.selected = true;
	known.audio.push_back(t);
	fake.streams[c.id] = known;

	InstalledChannelSource installed(&fake);

	Result<ChannelStreams> got = channels::streams(c.id);
	REQUIRE(got.ok());
	REQUIRE(got.value().known);
	REQUIRE(got.value().video_codec == VideoCodec::H264);
	REQUIRE(got.value().audio.size() == 1);
	REQUIRE(got.value().audio[0].codec == AudioCodec::Ac3);

	// A channel the box does not have is refused, and a channel it has and
	// knows nothing about is an answer.
	Result<ChannelStreams> missing = channels::streams(0xbdf8040300017a01ULL);
	REQUIRE_FALSE(missing.ok());
	REQUIRE(missing.error().code == ErrorCode::NoSuchChannel);

	ChannelInfo cold;
	cold.id = 0x1122334455667788ULL;
	cold.kind = ServiceKind::Tv;
	fake.channels.push_back(cold);
	Result<ChannelStreams> nothing = channels::streams(cold.id);
	REQUIRE(nothing.ok());
	REQUIRE_FALSE(nothing.value().known);
	REQUIRE(nothing.value().audio.empty());
}
