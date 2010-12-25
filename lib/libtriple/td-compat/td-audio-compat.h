/*
 * compatibility stuff for Tripledragon audio API
 *
 * (C) 2009 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __td_audio_compat_h__
#define __td_audio_compat_h__

#include <aud/aud_inf.h>
// types
typedef enum {
	AUDIO_SOURCE_DEMUX = AUD_SOURCE_DEMUX,
	AUDIO_SOURCE_MEMORY = AUD_SOURCE_MEMORY
} audio_stream_source_t;
#define audio_channel_select_t	audChannel_t
// ioctls
#define AUDIO_CHANNEL_SELECT	MPEG_AUD_SELECT_CHANNEL
#define AUDIO_SELECT_SOURCE	MPEG_AUD_SELECT_SOURCE
#define AUDIO_PLAY		MPEG_AUD_PLAY
#define AUDIO_STOP		MPEG_AUD_STOP
#define AUDIO_SET_MUTE		MPEG_AUD_SET_MUTE

#endif /* __td_audio_compat_h__ */
