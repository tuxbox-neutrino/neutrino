/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2002 Bjoern Kalkbrenner <terminar@cyberphoria.org>
	Copyright (C) 2002,2003 Dirch
	Copyright (C) 2002,2003,2004 Zwen
	
	libmad MP3 low-level core
	Homepage: http://www.dbox2.info/

	Kommentar:

	based on
	************************************
	*** madlld -- Mad low-level      ***  v 1.0p1, 2002-01-08
	*** demonstration/decoder        ***  (c) 2001, 2002 Bertrand Petit
	************************************

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __AUDIO_METADATA__
#define __AUDIO_METADATA__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>

#ifndef ENABLE_FFMPEGDEC
#include <mad.h>
#endif

class CAudioMetaData
{
public:
	// constructor
	CAudioMetaData();
	// destructor
	~CAudioMetaData();
	// copy constructor
	CAudioMetaData( const CAudioMetaData& src );
	// assignment operator
	void operator=( const CAudioMetaData& src );
	void clear();

	enum AudioType
	{
		NONE,
		CDR,
		MP3,
		OGG,
		WAV,
		FLAC
	};
	int type;
	std::string type_info;

	long filesize; /* filesize in bits (for mp3: without leading id3 tag) */

	unsigned int bitrate; /* overall bitrate, vbr file: current bitrate */
	unsigned int avg_bitrate; /* average bitrate in case of vbr file */
	unsigned int samplerate;
#ifndef ENABLE_FFMPEGDEC
	enum mad_layer layer;
	enum mad_mode mode;
#endif
	time_t total_time;
	long audio_start_pos; /* position of first audio frame */
	bool vbr;
	/* if the variable hasInfoOrXingTag is true, this means the values of
	   VBR and Duration are correct and should not be changed by the
	   decoder */
	bool hasInfoOrXingTag;

	std::string artist;
	std::string title;
	std::string album;
	std::string sc_station;
	std::string date;
	std::string genre;
	std::string track;
	std::string cover;
	bool cover_temporary;
	bool changed;
};
#endif /* __AUDIO_METADATA__ */
