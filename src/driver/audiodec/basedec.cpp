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
#ifdef ENABLE_FFMPEGDEC
#include "ffmpegdec.h"
#else
#include "ffmpegdec.h"
#include "cdrdec.h"
#include "mp3dec.h"
#include "oggdec.h"
#include "wavdec.h"
#ifdef ENABLE_FLAC
#include "flacdec.h"
#endif
#endif

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
	FILE* fp;

	if ((in->FileType == CFile::STREAM_AUDIO) && (in->Filename.find(".flv") != std::string::npos))
	{
		fp = fopen( in->Filename.c_str(), "rc" );
	}
	else
		fp = fopen( in->Filename.c_str(), "r" );

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
#ifndef ENABLE_FFMPEGDEC
		if( in->FileType == CFile::STREAM_AUDIO )
		{
			if ( fstatus( fp, ShoutcastCallback ) < 0 )
				fprintf( stderr, "Error adding shoutcast callback: %s",
						err_txt );

			if (ftype(fp, "ogg"))
				Status = COggDec::getInstance()->Decoder( fp, OutputFd, state,
						&in->MetaData, t,
						secondsToSkip );
			else if (ftype(fp, "flv")) {
				Status = CFfmpegDec::getInstance()->Decoder(fp, OutputFd, state,
						&in->MetaData, t,
						secondsToSkip );
				in->MetaData.type = CFile::FILE_UNKNOWN;
			}
			else
				Status = CMP3Dec::getInstance()->Decoder( fp, OutputFd, state,
						&in->MetaData, t,
						secondsToSkip );
		}
		else if( in->FileType == CFile::FILE_MP3)
			Status = CMP3Dec::getInstance()->Decoder( fp, OutputFd, state,
					&in->MetaData, t,
					secondsToSkip );
		else if( in->FileType == CFile::FILE_OGG )
			Status = COggDec::getInstance()->Decoder( fp, OutputFd, state,
					&in->MetaData, t,
					secondsToSkip );
		else if( in->FileType == CFile::FILE_WAV )
			Status = CWavDec::getInstance()->Decoder( fp, OutputFd, state,
					&in->MetaData, t,
					secondsToSkip );
		else if( in->FileType == CFile::FILE_CDR )
			Status = CCdrDec::getInstance()->Decoder( fp, OutputFd, state,
					&in->MetaData, t,
					secondsToSkip );
#ifdef ENABLE_FLAC
		else if (in->FileType == CFile::FILE_FLAC)
			Status = CFlacDec::getInstance()->Decoder(fp, OutputFd, state,
					&in->MetaData, t,
					secondsToSkip );
#endif
		else if (in->FileType == CFile::FILE_FLV) {
			Status = CFfmpegDec::getInstance()->Decoder(fp, OutputFd, state,
					&in->MetaData, t,
					secondsToSkip );
			in->MetaData.type = CFile::FILE_UNKNOWN;
		}
		else
		{
			fprintf( stderr, "DecoderBase: Supplied filetype is not " );
			fprintf( stderr, "supported by Audioplayer.\n" );
			Status = INTERNAL_ERR;
		}

#else
		CFile::FileType ft = in->FileType;
		if( in->FileType == CFile::STREAM_AUDIO )
		{
			if ( fstatus( fp, ShoutcastCallback ) < 0 )
				fprintf( stderr, "Error adding shoutcast callback: %s", err_txt );

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

		}
		in->MetaData.type = ft;

		Status = CFfmpegDec::getInstance()->Decoder(fp, OutputFd, state, &in->MetaData, t, secondsToSkip );
#endif

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
#ifndef ENABLE_FFMPEGDEC
	if (in->FileType == CFile::FILE_MP3 || in->FileType == CFile::FILE_OGG
			|| in->FileType == CFile::FILE_WAV || in->FileType == CFile::FILE_CDR
			|| in->FileType == CFile::FILE_FLV
#ifdef ENABLE_FLAC
			|| in->FileType == CFile::FILE_FLAC
#endif
	   )
#endif
	{
		FILE* fp = fopen( in->Filename.c_str(), "r" );
		if ( fp == NULL )
		{
			fprintf( stderr, "Error opening file %s for meta data reading.\n",
					in->Filename.c_str() );
			Status = false;
		}
		else
		{
#ifndef ENABLE_FFMPEGDEC
			if(in->FileType == CFile::FILE_MP3)
			{
				Status = CMP3Dec::getInstance()->GetMetaData(fp, nice,
						&in->MetaData);
			}
			else if(in->FileType == CFile::FILE_OGG)
			{
				Status = COggDec::getInstance()->GetMetaData(fp, nice,
						&in->MetaData);
			}
			else if(in->FileType == CFile::FILE_WAV)
			{
				Status = CWavDec::getInstance()->GetMetaData(fp, nice,
						&in->MetaData);
			}
			else if(in->FileType == CFile::FILE_CDR)
			{
				Status = CCdrDec::getInstance()->GetMetaData(fp, nice,
						&in->MetaData);
			}
#ifdef ENABLE_FLAC
			else if (in->FileType == CFile::FILE_FLAC)
			{
				CFlacDec FlacDec;
				Status = FlacDec.GetMetaData(fp, nice, &in->MetaData);
			}
#endif
			else if (in->FileType == CFile::FILE_FLV)
			{
				struct stat st;
				if (!fstat(fileno(fp), &st))
					in->MetaData.filesize = st.st_size;
				in->MetaData.type = in->FileType;

				CFfmpegDec d;
				Status = d.GetMetaData(fp, nice, &in->MetaData);
			}
#else
			struct stat st;
			if (!fstat(fileno(fp), &st))
				in->MetaData.filesize = st.st_size;
			in->MetaData.type = in->FileType;

			CFfmpegDec d;
			Status = d.GetMetaData(fp, nice, &in->MetaData);
#endif
			if (Status)
				CacheMetaData(in);
			if ( fclose( fp ) == EOF )
			{
				fprintf( stderr, "Could not close file %s.\n",
						in->Filename.c_str() );
			}
		}
	}
#ifndef ENABLE_FFMPEGDEC
	else
	{
		fprintf( stderr, "GetMetaDataBase: Filetype is not supported for " );
		fprintf( stderr, "meta data reading.\n" );
		Status = false;
	}
#endif

	return Status;
}

void CBaseDec::Init()
{
	mSamplerate=0;
}

// vim:ts=4
