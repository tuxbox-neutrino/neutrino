/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2004 Zwen
	
	Decoder base class
	Homepage: http://www.dbox2.info/

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


#ifndef __BASE_DEC__
#define __BASE_DEC__

#include <stdio.h>
#include <driver/audiofile.h>
#include <driver/audiometadata.h>

class CBaseDec
{
public:
	enum State {STOP = 0, STOP_REQ, PLAY, PAUSE, FF, REV};
	enum RetCode { OK = 0, READ_ERR, WRITE_ERR, DSPSET_ERR, DATA_ERR, INTERNAL_ERR };

	// the follwing two methods have to be implemented for new decoders
	//@param secondsToSkip: a value of 0 indicates that normal FF/REV operation was requested
	//                      a value  > 0 indicates that *one* jump forwards (FF) or backwards (REV) was requested
	virtual RetCode Decoder(FILE *, const int, State* const, CAudioMetaData*, time_t* const, unsigned int* const)=0;
	virtual bool GetMetaData(FILE *in, const bool nice, CAudioMetaData* m)=0;
	
	static RetCode DecoderBase(CAudiofile* const in, const int OutputFd,
							   State* const state, time_t* const t,
							   unsigned int* const secondsToSkip);
	static bool GetMetaDataBase(CAudiofile* const in, const bool nice);
	static void Init();

	CBaseDec(){};
	static bool SetDSP(int soundfd, int fmt, unsigned int dsp_speed, unsigned int channels);
private:
	static bool avs_mute(bool mute);
	unsigned static int mSamplerate;
};


#endif

