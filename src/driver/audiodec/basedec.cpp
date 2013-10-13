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
#ifdef ENABLE_FLAC
#include <flacdec.h>
#endif
#include <linux/soundcard.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <driver/audioplay.h> // for ShoutcastCallback()

#include <global.h>
#include <neutrino.h>
#include <zapit/client/zapittools.h>

#include "basedec.h"
#include "cdrdec.h"
#include "mp3dec.h"
#include "oggdec.h"
#include "wavdec.h"
#include "ffmpegdec.h"
#include <driver/netfile.h>

unsigned int CBaseDec::mSamplerate=0;

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
		if( in->FileType == CFile::STREAM_AUDIO )
		{
			if ( fstatus( fp, ShoutcastCallback ) < 0 )
			{
				fprintf( stderr, "Error adding shoutcast callback: %s",
						 err_txt );
			}
			if (ftype(fp, "ogg"))
			{
				Status = COggDec::getInstance()->Decoder( fp, OutputFd, state,
																		&in->MetaData, t,
																		secondsToSkip );
			}
			else if (ftype(fp, "mpeg"))
			{
				Status = CMP3Dec::getInstance()->Decoder( fp, OutputFd, state,
																		&in->MetaData, t,
																		secondsToSkip );
			}
			else
			{
				Status = CFfmpegDec::getInstance()->Decoder( fp, OutputFd, state,
																		&in->MetaData, t,
																		secondsToSkip );
			}
		}
		else if( in->FileType == CFile::FILE_MP3)
		{
			Status = CMP3Dec::getInstance()->Decoder( fp, OutputFd, state,
													  &in->MetaData, t,
													  secondsToSkip );
		}
		else if( in->FileType == CFile::FILE_OGG )
		{
			Status = COggDec::getInstance()->Decoder( fp, OutputFd, state,
													  &in->MetaData, t,
													  secondsToSkip );
		}
		else if( in->FileType == CFile::FILE_WAV )
		{
			Status = CWavDec::getInstance()->Decoder( fp, OutputFd, state,
													  &in->MetaData, t,
													  secondsToSkip );
		}
		else if( in->FileType == CFile::FILE_CDR )
		{
			Status = CCdrDec::getInstance()->Decoder( fp, OutputFd, state,
													  &in->MetaData, t,
													  secondsToSkip );
		}
#ifdef ENABLE_FLAC
		else if (in->FileType == CFile::FILE_FLAC)
		{
			Status = CFlacDec::getInstance()->Decoder(fp, OutputFd, state,
								  &in->MetaData, t,
								  secondsToSkip );
		}
#endif
		else
		{
			Status = CFfmpegDec::getInstance()->Decoder(fp, OutputFd, state,
								  &in->MetaData, t,
								  secondsToSkip );
		}

		if ( fclose( fp ) == EOF )
		{
			fprintf( stderr, "Could not close file %s.\n",
					 in->Filename.c_str() );
		}
	}

	return Status;
}

bool CBaseDec::GetMetaDataBase(CAudiofile* const in, const bool nice)
{
	bool Status = true;

	if (in->FileType == CFile::FILE_MP3 || in->FileType == CFile::FILE_OGG
	 || in->FileType == CFile::FILE_WAV || in->FileType == CFile::FILE_CDR
#ifdef ENABLE_FLAC
	 || in->FileType == CFile::FILE_FLAC
#endif
	   )
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
			if ( fclose( fp ) == EOF )
			{
				fprintf( stderr, "Could not close file %s.\n",
						 in->Filename.c_str() );
			}
		}
	}
	else
	{
		fprintf( stderr, "GetMetaDataBase: Filetype is not supported for " );
		fprintf( stderr, "meta data reading.\n" );
		Status = false;
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
