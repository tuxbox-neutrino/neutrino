/*
 * channelstreams_internal.h - reading a channel's stream layout, for the source and the tests
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

#ifndef __coreapi_channelstreams_internal_h__
#define __coreapi_channelstreams_internal_h__

#include "coreapi/base/types.h"

#include <zapit/channel.h>
#include <zapit/client/zapittypes.h>

/* Apart from the source that uses it, and written inline, so the suite can drive
   it against a channel it built itself: the source it belongs to reaches the
   whole channel stack and cannot be linked into a test binary at all. What is
   decided here is the whole of this answer: which channel counts as read, what a
   picture with no pid is, and which of the box's codec numbers becomes which
   word. */

namespace coreapi
{
namespace internal
{

/* What the channel's picture is coded in.

   The channel stack's number cannot be read on its own: its enumeration starts
   at nought with MPEG-2 and a channel is constructed with nought, so a channel
   nobody has looked at and a channel carrying MPEG-2 are the same value. The
   picture pid settles it, nought being the program association table and never a
   picture.

   A channel with no picture pid is None where the box files it as radio and
   Unknown otherwise, which covers the two ways a channel has no pid: a service
   that carries no picture, and one whose streams nobody has read. */
inline VideoCodec videoCodecOf(CZapitChannel &src)
{
	if (src.getVideoPid() == 0)
	{
		return (src.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE)
			? VideoCodec::None : VideoCodec::Unknown;
	}
	switch (src.type)
	{
		case CHANNEL_MPEG2: return VideoCodec::Mpeg2;
		case CHANNEL_MPEG4: return VideoCodec::H264;
		case CHANNEL_HEVC:  return VideoCodec::Hevc;
		case CHANNEL_CAVS:  return VideoCodec::Cavs;
		// The channel stack writes only the four above and a channel read out
		// of the service list carries whatever that file said, which is a
		// number nothing here has to recognise.
		default:            return VideoCodec::Unknown;
	}
}

inline AudioCodec audioCodecOf(CZapitAudioChannel::ZapitAudioChannelType t)
{
	switch (t)
	{
		case CZapitAudioChannel::MPEG:    return AudioCodec::Mp2;
		case CZapitAudioChannel::AC3:     return AudioCodec::Ac3;
		case CZapitAudioChannel::EAC3:    return AudioCodec::Eac3;
		case CZapitAudioChannel::AAC:     return AudioCodec::Aac;
		case CZapitAudioChannel::AACPLUS: return AudioCodec::AacPlus;
		case CZapitAudioChannel::DTS:     return AudioCodec::Dts;
		case CZapitAudioChannel::DTSHD:   return AudioCodec::DtsHd;
		case CZapitAudioChannel::LPCM:    return AudioCodec::Lpcm;
		default:                          return AudioCodec::Unknown;
	}
}

/* What the box holds about one channel's streams. The caller holds the channel
   lock and src is the channel where it sits, not a copy: a copy carries none of
   this by design.

   What decides known is the raw table the channel keeps. The channel stack
   stores that only when it read the layout off the transponder and throws it
   away during a scan, so it is the one field that separates a channel somebody
   switched to from a channel a scan wrote down. The picture pid would not do: a
   scan run with the box's pid option turned on writes one into the service list,
   and the track list it writes with it is a single pid carrying neither a codec
   nor a language.

   Which is why the track list is read only when known. A scan's one bare pid
   answered as a track would read as the channel's whole sound. */
inline void fillStreams(CZapitChannel &src, ChannelStreams &dst)
{
	int pmt_len = 0;
	dst.known = src.getRawPmt(pmt_len) != NULL;
	dst.video_codec = videoCodecOf(src);
	dst.video_pid = src.getVideoPid();
	dst.audio.clear();
	if (!dst.known)
		return;

	const unsigned char count = src.getAudioChannelCount();
	const unsigned char playing = src.getAudioChannelIndex();
	for (unsigned char i = 0; i < count; i++)
	{
		const CZapitAudioChannel *a = src.getAudioChannel(i);
		if (a == NULL)
			continue;
		AudioTrack t;
		t.pid = a->pid;
		t.codec = audioCodecOf(a->audioChannelType);
		t.description = a->description;
		t.selected = (i == playing);
		dst.audio.push_back(t);
	}
}

} // namespace internal
} // namespace coreapi

#endif
