/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2004 Zwen
	base decoder class
	Homepage: http://www.cyberphoria.org/

	Kommentar:

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
#include <linux/soundcard.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <OpenThreads/ScopedLock>

#include <driver/audioplay.h> // for ShoutcastCallback()

#include <global.h>
#include <neutrino.h>
#include <zapit/client/zapittools.h>

#include "basedec.h"
#include "ffmpegdec.h"

#include <driver/netfile.h>

unsigned int CBaseDec::mSamplerate=0;
OpenThreads::Mutex CBaseDec::metaDataMutex;
std::map<const std::string,CAudiofile> CBaseDec::metaDataCache;

void ShoutcastCallback(void *arg)
{
	CAudioPlayer::getInstance()->sc_callback(arg);
}

CBaseDec::RetCode CBaseDec::DecoderBase(CAudiofile* const in,
										const int OutputFd, State* const state,
										time_t* const t,
										unsigned int* const secondsToSkip)
{
	RetCode Status = OK;

	FILE* fp = fopen( in->Filename.c_str(), "r" );
	if ( fp == NULL )
	{
		fprintf( stderr, "Error opening file %s for decoding.\n",
				 in->Filename.c_str() );
		Status = INTERNAL_ERR;
	}
	/* jump to first audio frame; audio_start_pos is only set for FILE_MP3 */
	else if ( in->MetaData.audio_start_pos &&
			  fseek( fp, in->MetaData.audio_start_pos, SEEK_SET ) == -1 )
	{
		fprintf( stderr, "fseek() failed.\n" );
		Status = INTERNAL_ERR;
	}

	if ( Status == OK )
	{
		CFile::FileType ft;
		if( in->FileType == CFile::STREAM_AUDIO )
		{
			if ( fstatus( fp, ShoutcastCallback ) < 0 )
			{
				fprintf( stderr, "Error adding shoutcast callback: %s", err_txt );
			}

			if (ftype(fp, "ogg"))
				ft = CFile::FILE_OGG;
			else if (ftype(fp, "mpeg"))
				ft = CFile::FILE_MP3;
			else
				ft = CFile::FILE_UNKNOWN;
		}
		else
		{
			struct stat st;
			if (!fstat(fileno(fp), &st))
						in->MetaData.filesize = st.st_size;

			ft = in->FileType;
		}

		Status = CFfmpegDec::getInstance()->Decoder(fp, ft, OutputFd, state, &in->MetaData, t, secondsToSkip );

		if ( fclose( fp ) == EOF )
		{
			fprintf( stderr, "Could not close file %s.\n", in->Filename.c_str() );
		}
	}

	return Status;
}

bool CBaseDec::LookupMetaData(CAudiofile* const in)
{
	bool res = false;
	metaDataMutex.lock();
	std::map<const std::string,CAudiofile>::const_iterator it = metaDataCache.find(in->Filename);
	if (it != metaDataCache.end()) {
		*in = it->second;
		res = true;
	}
	metaDataMutex.unlock();
	return res;
}

void CBaseDec::CacheMetaData(CAudiofile* const in)
{
	metaDataMutex.lock();
	// FIXME: This places a limit on the cache size. A LRU scheme would be more appropriate.
	if (metaDataCache.size() > 128)
		metaDataCache.clear();
	metaDataCache[in->Filename] = *in;
	metaDataMutex.unlock();
}

void CBaseDec::ClearMetaData()
{
	metaDataMutex.lock();
	metaDataCache.clear();
	metaDataMutex.unlock();
}

bool CBaseDec::GetMetaDataBase(CAudiofile* const in, const bool nice)
{
	if (in->FileType == CFile::STREAM_AUDIO)
		return true;

	if (LookupMetaData(in))
		return true;

	bool Status = true;
	FILE* fp = fopen( in->Filename.c_str(), "r" );
	if ( fp == NULL )
	{
		fprintf( stderr, "Error opening file %s for meta data reading.\n",
				 in->Filename.c_str() );
		Status = false;
	}
	else
	{
		struct stat st;
		if (!fstat(fileno(fp), &st))
			in->MetaData.filesize = st.st_size;

		CFfmpegDec d;
		Status = d.GetMetaData(fp, in->FileType, nice, &in->MetaData);
		if (Status)
			CacheMetaData(in);

		if ( fclose( fp ) == EOF )
		{
			fprintf( stderr, "Could not close file %s.\n",
					 in->Filename.c_str() );
		}
	}

	return Status;
}

bool CBaseDec::SetDSP(int soundfd, int fmt, unsigned int dsp_speed, unsigned int channels)
{
	bool crit_error=false;

	if (::ioctl(soundfd, SNDCTL_DSP_RESET))
		printf("reset failed\n");
	if(::ioctl(soundfd, SNDCTL_DSP_SETFMT, &fmt))
		printf("setfmt failed\n");
	if(::ioctl(soundfd, SNDCTL_DSP_CHANNELS, &channels))
		printf("channel set failed\n");
	if (dsp_speed != mSamplerate)
	{
		// mute audio to reduce pops when changing samplerate (avia_reset)
		//bool was_muted = avs_mute(true);
		if (::ioctl(soundfd, SNDCTL_DSP_SPEED, &dsp_speed))
		{
			printf("speed set failed\n");
			crit_error=true;
		}
		else
		{
#if 0
			unsigned int rs = 0;
			::ioctl(soundfd, SNDCTL_DSP_SPEED, &rs);
			mSamplerate = dsp_speed;
			// disable iec aka digi out (avia reset enables it again)
			//g_Zapit->IecOff();
#endif
		}
		//usleep(400000);
		//if (!was_muted)
		//	avs_mute(false);
	}
//printf("Debug: SNDCTL_DSP_RESET %d / SNDCTL_DSP_SPEED %d / SNDCTL_DSP_CHANNELS %d / SNDCTL_DSP_SETFMT %d\n",
//					SNDCTL_DSP_RESET, SNDCTL_DSP_SPEED, SNDCTL_DSP_CHANNELS, SNDCTL_DSP_SETFMT);
	return crit_error;
}

bool CBaseDec::avs_mute(bool /*mute*/)
{
	return true;
}

void CBaseDec::Init()
{
	mSamplerate=0;
}

// vim:ts=4
