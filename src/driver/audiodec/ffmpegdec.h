/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2004 Zwen
	Copyright (C) 2013 martii
	
	ffmpeg audio decoder layer
	Homepage: http://forum.tuxbox.org/forum

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


#ifndef __FFMPEG_DEC__
#define __FFMPEG_DEC__

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include "basedec.h"
# if __WORDSIZE == 64
#  define UINT64_C(c)   c ## UL
# else
#  define UINT64_C(c)   c ## ULL
# endif
extern "C" {
#include <libavformat/avformat.h>
}

class CFfmpegDec : public CBaseDec
{
private:
	bool meta_data_valid;
	int mChannels;
	int mSampleRate;
	size_t buffer_size;
	unsigned char *buffer;
	AVFormatContext *avc;
	AVCodec *codec;
	int best_stream;
	void *in;
	bool Init(void);
	void DeInit(void);

public:
	static CFfmpegDec* getInstance();
	virtual RetCode Decoder(FILE *, int, State*, CAudioMetaData* m, time_t* t, unsigned int* secondsToSkip);
	bool GetMetaData(FILE *in, const bool nice, CAudioMetaData* m);
	CFfmpegDec();
	~CFfmpegDec();
	int Read(void *buf, size_t buf_size);
	int64_t Seek(int64_t offset, int whence);

protected:
	virtual bool SetMetaData(FILE* in, CAudioMetaData* m);
};
#endif
