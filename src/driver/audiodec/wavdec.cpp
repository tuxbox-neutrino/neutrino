/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2004 Zwen
	Copyright (C) 2010-2011 Stefan Seyfried

	Homepage: http://www.dbox2.info/

	Kommentar:

	wav audio decoder

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

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <sstream>
#include <driver/audioplay.h>
#include <linux/soundcard.h>
#include <hardware/audio.h>

#include "wavdec.h"

extern cAudio * audioDecoder;

#define ProgName "WavDec"
// nr of msecs to skip in ff/rev mode
#define MSECS_TO_SKIP 3000
// nr of msecs to play in ff/rev mode
#define MSECS_TO_PLAY 200

struct WavHeader
{
	char  ChunkID[4];
	int   ChunkSize;
	char  Format[4];
	char  Subchunk1ID[4];
	int   Subchunk1Size;
	short AudioFormat;
	short NumChannels;
	int   SampleRate;
	int   ByteRate;
	short BlockAlign;
	short BitsPerSample;
	char  Subchunk2ID[4];
	int   Subchunk2Size;
} __attribute__ ((packed));

int endianTest=1;
#define Swap32IfBE(l) \
    (*(char *)&endianTest ? (l) : \
                             ((((l) & 0xff000000) >> 24) | \
                             (((l) & 0x00ff0000) >> 8)  | \
                             (((l) & 0x0000ff00) << 8)  | \
                             (((l) & 0x000000ff) << 24)))
#define Swap16IfBE(l) \
    (*(char *)&endianTest ? (l) : \
                             ((((l) & 0xff00) >> 8) | \
                             (((l) &  0x00ff) << 8)))

#define MAX_OUTPUT_SAMPLES 2048 /* AVIA_GT_PCM_MAX_SAMPLES-1 */

CBaseDec::RetCode CWavDec::Decoder(FILE *in, int /*OutputFd*/, State* state, CAudioMetaData* meta_data, time_t* time_played, unsigned int* secondsToSkip)
{
	char* buffer;
	RetCode Status=OK;

	if (!SetMetaData(in, meta_data))
	{
		Status=DATA_ERR;
		return Status;
	}
	fseek(in, header_size, SEEK_SET);
//	int fmt;
	switch(mBitsPerSample)
	{
		case 8  : //fmt = AFMT_U8;
			break;
		case 16 : //fmt = header_size == 0 ? AFMT_S16_BE : AFMT_S16_LE;
			break;
		default:
			printf("%s: wrong bits per sample (%d)\n", ProgName, mBitsPerSample);
			Status=DATA_ERR;
			return Status;
	}
#if 0
	if (SetDSP(OutputFd, fmt, meta_data->samplerate , mChannels))
	{
		Status=DSPSET_ERR;
		return Status;
	}
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
	audioDecoder->PrepareClipPlay(mChannels, meta_data->samplerate, mBitsPerSample, 1);
#else
	audioDecoder->PrepareClipPlay(mChannels, meta_data->samplerate, mBitsPerSample, 0);
#endif
	int actSecsToSkip = (*secondsToSkip != 0) ? *secondsToSkip : MSECS_TO_SKIP / 1000;
	unsigned int oldSecsToSkip = *secondsToSkip;
	int jumppos=0;
	int bytes;
	int bytes_to_play = MSECS_TO_PLAY * meta_data->bitrate / 8000;
	int bytes_to_skip = actSecsToSkip * meta_data->bitrate / 8;
	int buffersize = MAX_OUTPUT_SAMPLES * mChannels * mBitsPerSample / 8;
	buffer = (char*) malloc (buffersize);
	do
	{
		while(*state==PAUSE)
			usleep(10000);

		if(*state==FF || *state==REV)
		{
			if (oldSecsToSkip != *secondsToSkip)
			{
				actSecsToSkip = (*secondsToSkip != 0) ? *secondsToSkip : MSECS_TO_SKIP / 1000;
				bytes_to_skip = actSecsToSkip * meta_data->bitrate / 8;
				oldSecsToSkip = *secondsToSkip;
			}
			//printf("skipping %d secs and %d bytes\n",actSecsToSkip,bytes_to_skip);
			if(std::abs(ftell(in)-jumppos) > bytes_to_play)
			{
				if(*state==FF)
				{
					fseek(in, bytes_to_skip, SEEK_CUR);
					jumppos=ftell(in);
				}
				else
				{
					if(ftell(in) < bytes_to_skip)
					{
						fseek(in, header_size, SEEK_SET);
						*state=PLAY;
					}
					else
					{
						fseek(in, -bytes_to_skip, SEEK_CUR);
						jumppos=ftell(in);
					}
				}
			}
			// if a custom value was set we only jump once
			if (*secondsToSkip != 0) {
				*state=PLAY;
			}
		}

		bytes = fread(buffer, 1, buffersize, in);
		//if (write(OutputFd, buffer, bytes) != bytes)
		if(audioDecoder->WriteClip((unsigned char*) buffer, bytes) != bytes)
		{
			fprintf(stderr,"%s: PCM write error (%s).\n", ProgName, strerror(errno));
			Status=WRITE_ERR;
		}
		*time_played = (meta_data->bitrate!=0) ? (ftell(in)-header_size)*8/meta_data->bitrate : 0;
	} while (bytes > 0 && *state!=STOP_REQ && Status==OK);
	audioDecoder->StopClip();
	free(buffer);
	return Status;
}

bool CWavDec::GetMetaData(FILE *in, const bool /*nice*/, CAudioMetaData* m)
{
	return SetMetaData(in, m);
}

CWavDec* CWavDec::getInstance()
{
	static CWavDec* WavDec = NULL;
	if(WavDec == NULL)
	{
		WavDec = new CWavDec();
	}
	return WavDec;
}

bool CWavDec::SetMetaData(FILE* in, CAudioMetaData* m)
{
	/* Set Metadata */
	struct WavHeader wh;

	header_size = 44;

	fseek(in, 0, SEEK_END);
	int filesize = ftell(in);
	fseek(in, 0, SEEK_SET);
	if(fread(&wh, sizeof(wh), 1, in)!=1)
		return false;
	if(memcmp(wh.ChunkID, "RIFF", 4)!=0 ||
		memcmp(wh.Format, "WAVE", 4)!=0 ||
		Swap16IfBE(wh.AudioFormat) != 1)
	{
		printf("%s: wrong format (header)\n", ProgName);
		return false;
	}
	m->type = CAudioMetaData::WAV;
	m->bitrate = Swap32IfBE(wh.ByteRate)*8;
	m->samplerate = Swap32IfBE(wh.SampleRate);
	mBitsPerSample = Swap16IfBE(wh.BitsPerSample);
	mChannels = Swap16IfBE(wh.NumChannels);
	m->total_time = (m->bitrate!=0) ? (filesize-header_size)*8 / m->bitrate : 0;
	std::stringstream ss;
	ss << "Riff/Wave / " << mChannels << "channel(s) / " << mBitsPerSample << "bit";
	m->type_info = ss.str();
	m->changed=true;
	return true;
}
