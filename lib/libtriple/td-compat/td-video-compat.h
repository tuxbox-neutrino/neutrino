/*
 * compatibility stuff for Tripledragon video API
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
 */

#ifndef __td_video_compat_h__
#define __td_video_compat_h__

#include <vid/vid_inf.h>
// types
#define video_format_t		vidDispSize_t
#define video_displayformat_t	vidDispMode_t
typedef enum {
	VIDEO_SOURCE_DEMUX = VID_SOURCE_DEMUX,
	VIDEO_SOURCE_MEMORY = VID_SOURCE_MEMORY
} video_stream_source_t;
typedef enum {
	VIDEO_STOPPED, /* Video is stopped */
	VIDEO_PLAYING, /* Video is currently playing */
	VIDEO_FREEZED  /* Video is freezed */
} video_play_state_t;
//#define video_play_state_t	vidState_t
// ioctls
#define VIDEO_SET_SYSTEM		MPEG_VID_SET_DISPFMT
#define VIDEO_SET_FORMAT		MPEG_VID_SET_DISPSIZE
#define VIDEO_SET_DISPLAY_FORMAT	MPEG_VID_SET_DISPMODE
#define VIDEO_SELECT_SOURCE		MPEG_VID_SELECT_SOURCE
#define VIDEO_PLAY			MPEG_VID_PLAY
#define VIDEO_STOP			MPEG_VID_STOP
#define VIDEO_SET_BLANK			MPEG_VID_SET_BLANK

#endif /* __td_video_compat_h__ */
