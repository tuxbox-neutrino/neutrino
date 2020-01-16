/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: non-nil; c-basic-offset: 4 -*- */
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

#include <unistd.h>
#include <driver/audiometadata.h>

// constructor
CAudioMetaData::CAudioMetaData()
{
#ifndef ENABLE_FFMPEGDEC
	layer = MAD_LAYER_III;
	mode =  MAD_MODE_STEREO;
#endif
	cover_temporary = false;
	clear();
}

// destructor
CAudioMetaData::~CAudioMetaData()
{
	if (cover_temporary && !cover.empty())
		unlink(cover.c_str());
}

// copy constructor
CAudioMetaData::CAudioMetaData( const CAudioMetaData& src )
  : type( src.type ), type_info( src.type_info ),
	filesize( src.filesize ), bitrate( src.bitrate ),
	avg_bitrate( src.avg_bitrate ), samplerate( src.samplerate ),
#ifndef ENABLE_FFMPEGDEC
	layer( src.layer ), mode( src.mode ),
#endif
	total_time( src.total_time ),
	audio_start_pos( src.audio_start_pos ), vbr( src.vbr ),
	hasInfoOrXingTag( src.hasInfoOrXingTag ), artist( src.artist ),
	title( src.title ), album( src.album ), sc_station( src.sc_station ),
	date( src.date ), genre( src.genre ), track( src.track ),cover(src.cover),
	logo( src.logo ), url( src.url ),
	cover_temporary( false ),
	changed( src.changed )
{
}

// assignment operator
void CAudioMetaData::operator=( const CAudioMetaData& src )
{
    // self assignment check
	if ( &src == this )
		return;

	type = src.type;
	type_info = src.type_info;
	filesize = src.filesize;
	bitrate = src.bitrate;
	avg_bitrate = src.avg_bitrate;
	samplerate = src.samplerate;
#ifndef ENABLE_FFMPEGDEC
	layer = src.layer;
	mode = src.mode;
#endif
	total_time = src.total_time;
	audio_start_pos = src.audio_start_pos;
	vbr = src.vbr;
	hasInfoOrXingTag = src.hasInfoOrXingTag;
	artist = src.artist;
	title = src.title;
	album = src.album;
	date = src.date;
	genre = src.genre;
	track = src.track;
	cover = src.cover;
	logo = src.logo;
	url = src.url;
	sc_station = src.sc_station;
	changed = src.changed;
	cover_temporary = false;
}

void CAudioMetaData::clear()
{
	type=0;
	type_info.clear();
	filesize=0;
	bitrate=0;
	avg_bitrate=0;
	samplerate=0;
	total_time=0;
	audio_start_pos=0;
	vbr=false;
	hasInfoOrXingTag=false;
	artist.clear();
	title.clear();
	album.clear();
	sc_station.clear();
	date.clear();
	genre.clear();
	track.clear();
	if (cover_temporary && !cover.empty())
		unlink(cover.c_str());
	cover.clear();
	logo.clear();
	url.clear();
	cover_temporary=false;
	changed=false;
}
