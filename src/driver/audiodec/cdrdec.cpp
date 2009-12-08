/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2004 thegoodguy
	
	Homepage: http://www.dbox2.info/

	Kommentar:

	cdr audio decoder
	
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <cdrdec.h>

CCdrDec* CCdrDec::getInstance()
{
	static CCdrDec* CdrDec = NULL;
	if (CdrDec == NULL)
	{
		CdrDec = new CCdrDec();
	}
	return CdrDec;
}

bool CCdrDec::SetMetaData(FILE* in, CAudioMetaData* m)
{
	header_size = 0;

	fseek(in, 0, SEEK_END);
	int filesize = ftell(in);

	m->type = CAudioMetaData::CDR;
	m->bitrate = 44100 * 2 * 2 * 8;
	m->samplerate = 44100;
	mBitsPerSample = 16;
	mChannels = 2;
	m->total_time = filesize / (44100 * 2 * 2);
	m->type_info = "CDR / 2 channels / 16 bit";
	m->changed=true;
	return true;
}


