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


#ifndef __MP3_DEC__
#define __MP3_DEC__

#include <mad.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <driver/audiometadata.h>

#include "basedec.h"
extern "C"
{
#include "tag.h"
}

class CMP3Dec : public CBaseDec
{
private:
#if !((MAD_VERSION_MAJOR>=1) || \
	((MAD_VERSION_MAJOR==0) && \
	 (((MAD_VERSION_MINOR==14) && \
	   (MAD_VERSION_PATCH>=2)) || \
	  (MAD_VERSION_MINOR>14))))
	const char*  MadErrorString(const struct mad_stream *Stream);
#endif
	signed short MadFixedToSShort(const mad_fixed_t Fixed, bool left = false);
	void CreateInfo(CAudioMetaData* const, const int);
	bool GetMP3Info(FILE*, const bool, CAudioMetaData* const);
	void GetID3(FILE*, CAudioMetaData* const);
	long scanHeader( FILE*, struct mad_header* const, struct tag* const,
					 const bool );

public:
	static CMP3Dec* getInstance();
	virtual RetCode Decoder(FILE *InputFp, const int OutputFd,
							State* const state, CAudioMetaData* m,
							time_t* const t, unsigned int* const secondsToSkip);
	bool GetMetaData(FILE *in, const bool nice, CAudioMetaData* const m);
	bool SaveCover(FILE*, CAudioMetaData * const m);
	CMP3Dec(){};

};


#endif

