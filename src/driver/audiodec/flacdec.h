/*
	Neutrino-GUI  -   DBoxII-Project

	FLAC audio decoder
	License: GPL

	ported from tuxbox-CVS flacdec.h CVS rev 1.1
	Neutrino-HD port is:
		(C) 2011 Stefan Seyfried, B1 Systems GmbH, Vohburg, Germany

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software Foundation,
	51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
*/


#ifndef __FLAC_DEC__
#define __FLAC_DEC__

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <basedec.h>
#include <driver/audiometadata.h>
#include <FLAC/all.h>

#define DECODE_SLOTS 30

class CFlacDec : public CBaseDec
{

public:
	static CFlacDec* getInstance();
	virtual RetCode Decoder(FILE *, const int, State* const, CAudioMetaData*, time_t* const, unsigned int* const);
	bool GetMetaData(FILE *in, const bool nice, CAudioMetaData* m);
	CFlacDec(){ mMetadata = NULL; };

	int mWriteSlot;
	int mReadSlot;
	int mSlotSize;
	unsigned long mFrameCount;
	unsigned int mChannels;
	unsigned int mSampleRate;
	unsigned long mBytes;
	unsigned int mBps;
	unsigned int mSamplesProcessed;
	unsigned int mBuffersize;
	unsigned long mTotalSamples;
	unsigned int mLengthInMsec;
	FLAC__StreamDecoder *mFlacDec;
	FILE* mIn;
	char* mPcmSlots[DECODE_SLOTS];
	FLAC__uint64 mSlotTime[DECODE_SLOTS];
	RetCode Status;
	FLAC__StreamMetadata *mMetadata;

private:
	void ParseUserComments(FLAC__StreamMetadata_VorbisComment*, CAudioMetaData*);
	bool Open(FILE*, FLAC__StreamDecoder*);
	void SetMetaData(FLAC__StreamMetadata*, CAudioMetaData*);
	State* mState;
	bool mSeekable;
	time_t* mTimePlayed;
};

#endif
