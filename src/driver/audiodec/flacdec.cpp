/*
	Neutrino-GUI  -   DBoxII-Project

	flac audio decoder
	uses flac library

	License: GPL

	ported from tuxbox-CVS flacdec.cpp CVS rev 1.2

	Neutrino-HD port and fixes for non-ppc architectures are:
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <flacdec.h>
#include <linux/soundcard.h>
#include <algorithm>
#include <sstream>

#include <hardware/audio.h>
#include <driver/netfile.h>
extern cAudio *audioDecoder;

#define ProgName "FlacDec"
// nr of msecs to skip in ff/rev mode
#define MSECS_TO_SKIP 3000
// nr of msecs to play in ff/rev mode
#define MSECS_TO_PLAY 200

#define MAX_OUTPUT_SAMPLES 1022 /* AVIA_GT_PCM_MAX_SAMPLES-1 */

/* at first, define our own callback functions used in */
/* tremor to access the data. These functions are simple mappers */
FLAC__StreamDecoderReadStatus flac_read(const FLAC__StreamDecoder *, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	CFlacDec * flacdec = (CFlacDec *)client_data;

	if(*bytes > 0) {
		*bytes = fread(buffer, sizeof(FLAC__byte), *bytes, flacdec->mIn);
		if(ferror(flacdec->mIn))
			return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
		else if(*bytes == 0)
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		else
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	}
	else
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

FLAC__StreamDecoderSeekStatus flac_seek(const FLAC__StreamDecoder *, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	CFlacDec * flacdec = (CFlacDec *)client_data;

	if(flacdec->mIn == stdin)
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	else if(fseek(flacdec->mIn, (off_t)absolute_byte_offset, SEEK_SET) < 0)
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	else
		return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}
#if 0 
//never used
int flac_close(void *)
{
	return 0;
}
#endif
FLAC__StreamDecoderTellStatus flac_tell(const FLAC__StreamDecoder *, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	CFlacDec * flacdec = (CFlacDec *)client_data;
	off_t pos;

	if(flacdec->mIn == stdin)
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
	else if((pos = ftell(flacdec->mIn)) < 0)
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
	else {
		*absolute_byte_offset = (FLAC__uint64)pos;
		return FLAC__STREAM_DECODER_TELL_STATUS_OK;
	}
}

FLAC__StreamDecoderLengthStatus flac_length(const FLAC__StreamDecoder *, FLAC__uint64 *stream_length, void *client_data)
{
	struct stat filestats;
	CFlacDec * flacdec = (CFlacDec *)client_data;

	if(flacdec->mIn == stdin)
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	else if(fstat(fileno(flacdec->mIn), &filestats) != 0)
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	else {
		*stream_length = (FLAC__uint64)filestats.st_size;
		return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
	}
}

FLAC__bool flac_eof(const FLAC__StreamDecoder *, void *client_data)
{
	return feof(((CFlacDec *)client_data)->mIn)? true : false;
}

FLAC__StreamDecoderWriteStatus flac_write(const FLAC__StreamDecoder * /*vf*/, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	CFlacDec * flacdec = (CFlacDec *)client_data;

	FLAC__uint32 test = 1;
	bool is_big_endian_host_ = (*((FLAC__byte*)(&test)))? false : true;

	if (flacdec->mFrameCount == 0) {
		int fmt;

		flacdec->mChannels = frame->header.channels;
		flacdec->mSampleRate = frame->header.sample_rate;
		flacdec->mBps = frame->header.bits_per_sample;
		flacdec->mBuffersize = MAX_OUTPUT_SAMPLES * flacdec->mChannels * flacdec->mBps / 8;
		switch(flacdec->mBps)
		{
			case 8  : fmt = AFMT_U8;
				break;
			// TODO:
			case 16 :
				fmt = is_big_endian_host_ ? AFMT_S16_BE : AFMT_S16_LE;
				break;
			default:
				printf("Flacdecoder: wrong bits per sample (%d)\n", flacdec->mBps);
				flacdec->Status=CFlacDec::DSPSET_ERR;
				return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		printf("SetDSP: mBps=%d, fmt = %d, Sample rate = %d, channels = %d\n", flacdec->mBps, fmt, flacdec->mSampleRate, flacdec->mChannels);
		audioDecoder->PrepareClipPlay(flacdec->mChannels, flacdec->mSampleRate, flacdec->mBps, fmt == AFMT_S16_LE ? 1 : 0);
#if 0
#ifdef DBOX
		if (flacdec->SetDSP(flacdec->mOutputFd, fmt, flacdec->mSampleRate, flacdec->mChannels)) //FLAC__stream_decoder_get_channels (vf)))
#else
		if (flacdec->SetDSP(flacdec->mOutputFd, AFMT_S16_NE, flacdec->mSampleRate, flacdec->mChannels))
#endif
		{
			flacdec->Status=CFlacDec::DSPSET_ERR;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
#endif
	}

	const unsigned bps = frame->header.bits_per_sample, channels = frame->header.channels;
	const unsigned shift = ((bps%8)? 8-(bps%8): 0);
	FLAC__bool is_big_endian = false;
	FLAC__bool is_unsigned_samples = bps<=8;
	unsigned wide_samples = frame->header.blocksize, wide_sample, sample, channel, byte;
//	unsigned frame_bytes = 0;

	static FLAC__int8 s8buffer[FLAC__MAX_BLOCK_SIZE * FLAC__MAX_CHANNELS * sizeof(FLAC__int32)]; /* WATCHOUT: can be up to 2 megs */
	FLAC__uint8  *u8buffer  = (FLAC__uint8  *)s8buffer;
	FLAC__int16  *s16buffer = (FLAC__int16  *)s8buffer;
	FLAC__uint16 *u16buffer = (FLAC__uint16 *)s8buffer;
	FLAC__int32  *s32buffer = (FLAC__int32  *)s8buffer;
	FLAC__uint32 *u32buffer = (FLAC__uint32 *)s8buffer;
	size_t bytes_to_write = 0;

	/* sanity-check the bits-per-sample */
	if(flacdec->mBps) {
		if(bps != flacdec->mBps) {
			if(flacdec->mFrameCount)
				printf("ERROR, bits-per-sample is %u in frame but %u in STREAMINFO\n", bps, flacdec->mBps);
			else
				printf("ERROR, bits-per-sample is %u in this frame but %u in previous frames\n", bps, flacdec->mBps);
		}
	}

	/* sanity-check the #channels */
	if(flacdec->mChannels) {
		if(channels != flacdec->mChannels) {
			if(flacdec->mFrameCount)
				printf("ERROR, channels is %u in frame but %u in STREAMINFO\n", channels, flacdec->mChannels);
			else
				printf("ERROR, channels is %u in this frame but %u in previous frames\n", channels, flacdec->mChannels);
		}
	}

	/* sanity-check the sample rate */
	if(flacdec->mSampleRate) {
		if(frame->header.sample_rate != flacdec->mSampleRate) {
			if(flacdec->mFrameCount)
				printf("ERROR, sample rate is %u in frame but %u in STREAMINFO\n", frame->header.sample_rate, flacdec->mSampleRate);
			else
				printf("ERROR, sample rate is %u in this frame but %u in previous frames\n", frame->header.sample_rate, flacdec->mSampleRate);
		}
	}

	if(wide_samples > 0) {
		flacdec->mSamplesProcessed += wide_samples;
		flacdec->mFrameCount++;
			// ripped from libFLAC examples/plugins
			if(shift)/* && !decoder_session->replaygain.apply) */{
				for(wide_sample = 0; wide_sample < wide_samples; wide_sample++)
					for(channel = 0; channel < channels; channel++)
						((FLAC__int32**)buffer)[channel][wide_sample] <<= shift;/*@@@@@@un-const'ing the buffer is hacky but safe*/
			}

			/* first some special code for common cases */
			if(is_big_endian == is_big_endian_host_ && !is_unsigned_samples && channels == 2 && bps+shift == 16) {
				FLAC__int16 *buf1_ = s16buffer + 1;
				if(is_big_endian)
					memcpy(s16buffer, ((FLAC__byte*)(buffer[0]))+2, sizeof(FLAC__int32) * wide_samples - 2);
				else
					memcpy(s16buffer, buffer[0], sizeof(FLAC__int32) * wide_samples);
				for(sample = 0; sample < wide_samples; sample++, buf1_+=2)
					*buf1_ = (FLAC__int16)buffer[1][sample];
				bytes_to_write = 4 * sample;
			}
			else if(is_big_endian == is_big_endian_host_ && !is_unsigned_samples && channels == 1 && bps+shift == 16) {
				FLAC__int16 *buf1_ = s16buffer;
				for(sample = 0; sample < wide_samples; sample++)
					*buf1_++ = (FLAC__int16)buffer[0][sample];
				bytes_to_write = 2 * sample;
			}
			/* generic code for the rest */
			else if(bps+shift == 16) {
				if(is_unsigned_samples) {
					if(channels == 2) {
						for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++) {
							u16buffer[sample++] = (FLAC__uint16)(buffer[0][wide_sample] + 0x8000);
							u16buffer[sample++] = (FLAC__uint16)(buffer[1][wide_sample] + 0x8000);
						}
					}
					else if(channels == 1) {
						for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
							u16buffer[sample++] = (FLAC__uint16)(buffer[0][wide_sample] + 0x8000);
					}
					else { /* works for any 'channels' but above flavors are faster for 1 and 2 */
						for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
							for(channel = 0; channel < channels; channel++, sample++)
								u16buffer[sample] = (FLAC__uint16)(buffer[channel][wide_sample] + 0x8000);
					}
				}
				else {
					if(channels == 2) {
						for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++) {
							s16buffer[sample++] = (FLAC__int16)(buffer[0][wide_sample]);
							s16buffer[sample++] = (FLAC__int16)(buffer[1][wide_sample]);
						}
					}
					else if(channels == 1) {
						for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
							s16buffer[sample++] = (FLAC__int16)(buffer[0][wide_sample]);
					}
					else { /* works for any 'channels' but above flavors are faster for 1 and 2 */
						for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
							for(channel = 0; channel < channels; channel++, sample++)
								s16buffer[sample] = (FLAC__int16)(buffer[channel][wide_sample]);
					}
				}
				if(is_big_endian != is_big_endian_host_) {
					unsigned char tmp;
					const unsigned bytes = sample * 2;
					for(byte = 0; byte < bytes; byte += 2) {
						tmp = u8buffer[byte];
						u8buffer[byte] = u8buffer[byte+1];
						u8buffer[byte+1] = tmp;
					}
				}
				bytes_to_write = 2 * sample;
			}
			else if(bps+shift == 24) {
				if(is_unsigned_samples) {
					for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
						for(channel = 0; channel < channels; channel++, sample++)
							u32buffer[sample] = buffer[channel][wide_sample] + 0x800000;
				}
				else {
					for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
						for(channel = 0; channel < channels; channel++, sample++)
							s32buffer[sample] = buffer[channel][wide_sample];
				}
				if(is_big_endian != is_big_endian_host_) {
					unsigned char tmp;
					const unsigned bytes = sample * 4;
					for(byte = 0; byte < bytes; byte += 4) {
						tmp = u8buffer[byte];
						u8buffer[byte] = u8buffer[byte+3];
						u8buffer[byte+3] = tmp;
						tmp = u8buffer[byte+1];
						u8buffer[byte+1] = u8buffer[byte+2];
						u8buffer[byte+2] = tmp;
					}
				}
				if(is_big_endian) {
					unsigned lbyte;
					const unsigned bytes = sample * 4;
					for(lbyte = byte = 0; byte < bytes; ) {
						byte++;
						u8buffer[lbyte++] = u8buffer[byte++];
						u8buffer[lbyte++] = u8buffer[byte++];
						u8buffer[lbyte++] = u8buffer[byte++];
					}
				}
				else {
					unsigned lbyte;
					const unsigned bytes = sample * 4;
					for(lbyte = byte = 0; byte < bytes; ) {
						u8buffer[lbyte++] = u8buffer[byte++];
						u8buffer[lbyte++] = u8buffer[byte++];
						u8buffer[lbyte++] = u8buffer[byte++];
						byte++;
					}
				}
				bytes_to_write = 3 * sample;
			}
			else if(bps+shift == 8) {
				if(is_unsigned_samples) {
					for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
						for(channel = 0; channel < channels; channel++, sample++)
							u8buffer[sample] = (FLAC__uint8)(buffer[channel][wide_sample] + 0x80);
				}
				else {
					for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
						for(channel = 0; channel < channels; channel++, sample++)
							s8buffer[sample] = (FLAC__int8)(buffer[channel][wide_sample]);
				}
				bytes_to_write = sample;
			}
			else {
//				FLAC__ASSERT(0);
				/* double protection */
				return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
			}
	}
	if(bytes_to_write > 0) {
		ssize_t cnt;
		unsigned int j=0;
		while (j<bytes_to_write) {
			if (j+flacdec->mBuffersize > bytes_to_write)
				cnt = bytes_to_write - j;
			else
				cnt = flacdec->mBuffersize;

			//if (write(flacdec->mOutputFd, &u8buffer[j], cnt) != (ssize_t)cnt)
			if (audioDecoder->WriteClip(&u8buffer[j], cnt) != cnt)
			{
				/* if a pipe closed when writing to stdout, we let it go without an error message */
				return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
			}
			j += cnt;
		}
	}
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void flac_metadata(const FLAC__StreamDecoder *, const FLAC__StreamMetadata *metadata, void *client_data)
{
	CFlacDec * flacdec = (CFlacDec *)client_data;

	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
		{
			printf("STREAMINFO block received\n");
			flacdec->mTotalSamples = metadata->data.stream_info.total_samples;
			flacdec->mBps = metadata->data.stream_info.bits_per_sample;
			flacdec->mChannels = metadata->data.stream_info.channels;
			flacdec->mSampleRate = metadata->data.stream_info.sample_rate;

			{
				/* with VC++ you have to spoon feed it the casting from uint64->int64->double */

				FLAC__uint64 l = (FLAC__uint64)((flacdec->mTotalSamples * 1000ULL) / flacdec->mSampleRate);
				if (l > INT_MAX)
					l = INT_MAX;
				flacdec->mLengthInMsec = (int)l;
			}
			break;
		}
		case FLAC__METADATA_TYPE_PADDING: 	printf("PADDING block received\n"); break;
		case FLAC__METADATA_TYPE_APPLICATION: 	printf("APPLICATION block received\n"); break;
		case FLAC__METADATA_TYPE_SEEKTABLE: 	printf("SEEKTABLE block received\n"); break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		{
			printf("VORBISCOMMENT block (a.k.a. FLAC tags) received\n");
			// if there is an old metadata -> clear it
			if (flacdec->mMetadata != NULL)
				FLAC__metadata_object_delete(flacdec->mMetadata);
			flacdec->mMetadata = FLAC__metadata_object_clone(metadata);
			break;
		}
		case FLAC__METADATA_TYPE_CUESHEET: 	printf("CUESHEET block received\n"); break;
		case FLAC__METADATA_TYPE_PICTURE: 	printf("PICTURE block received\n"); break;
		case FLAC__METADATA_TYPE_UNDEFINED:
		default:				printf("Undefined block received\n"); break;
	}
}

void flac_error(const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus, void *)
{
}

CBaseDec::RetCode CFlacDec::Decoder(FILE *in, const int /*OutputFd*/, State* const state, CAudioMetaData* meta_data, time_t* const time_played, unsigned int* const secondsToSkip)
{
	int rval;
//	FLAC__uint64 jumptime=0;
	Status = OK;
	mState = state;
	mTimePlayed = time_played;
	mFrameCount = 0;
	mSamplesProcessed = 0;
	struct stat s;
	FLAC__uint64 filesize = 0;

	mFlacDec = FLAC__stream_decoder_new();

	if (!Open(in, mFlacDec))
	{
		Status=DATA_ERR;
		return Status;
	}

	if (fstat(fileno(in), &s))
	{
		perror("CFlacDec::Decoder fstat");
		*time_played = 0;
	}
	else
		filesize = (FLAC__uint64)s.st_size;

	/* up and away ... */
	mSlotSize = MAX_OUTPUT_SAMPLES * 2 * FLAC__stream_decoder_get_channels(mFlacDec);
//	State oldstate=*state;
	int jumppos=0;
	int actSecsToSkip = (*secondsToSkip != 0) ? *secondsToSkip : MSECS_TO_SKIP / 1000;
	int bytes_to_skip = actSecsToSkip * meta_data->bitrate / 8;
	int bytes_to_play = MSECS_TO_PLAY * meta_data->bitrate / 8000;
	unsigned int oldSecsToSkip = *secondsToSkip;
	FLAC__uint64 position;

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
			printf("skipping %d secs and %d bytes\n",actSecsToSkip,bytes_to_skip);
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
						fseek(in, 0, SEEK_SET);
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

		if (FLAC__stream_decoder_get_state(mFlacDec) == FLAC__STREAM_DECODER_END_OF_STREAM) {
			rval = false;
			break;
		}

		rval = FLAC__stream_decoder_process_single(mFlacDec);

		/* update playback position */
		if (filesize > 0 && FLAC__stream_decoder_get_decode_position(mFlacDec, &position))
			*time_played = position / 1000ULL * mLengthInMsec / filesize;

	} while (rval && *state != STOP_REQ && Status == OK);

	// let buffer run dry
	printf("let buffer run dry\n");
	while (rval == true && *state != STOP_REQ && Status == OK /* && mReadSlot != mWriteSlot*/)
	{
		printf("...drying - *state=%x, Status=%x\n", *state, Status);
		usleep(100000);
	}

	/* clean up the junk from the party */
	if (mMetadata)
		FLAC__metadata_object_delete(mMetadata);
	mMetadata = NULL;
	FLAC__stream_decoder_finish(mFlacDec);
	FLAC__stream_decoder_delete(mFlacDec);
	mFlacDec = NULL;

	audioDecoder->StopClip();

	/* and drive home ;) */
	return Status;
}


bool CFlacDec::GetMetaData(FILE *in, const bool /*nice*/, CAudioMetaData* m)
{
	FLAC__StreamDecoder *vf;

	vf = FLAC__stream_decoder_new();
	FLAC__stream_decoder_set_metadata_ignore_all(vf);
	// we need streaminfo (default on) and FLAC__METADATA_TYPE_VORBIS_COMMENT (default off) for ID3 tags
	FLAC__stream_decoder_set_metadata_respond(vf, FLAC__METADATA_TYPE_STREAMINFO);
	FLAC__stream_decoder_set_metadata_respond(vf, FLAC__METADATA_TYPE_VORBIS_COMMENT);
	if (!Open(in, vf))
	{
		return false;
	}

	if (FLAC__stream_decoder_process_until_end_of_metadata(vf)) {
		// if the metadata callback was called mMetadata should be filled with infos
		if (mMetadata) {
			SetMetaData(mMetadata, m);
		}
	}
	// clean up stuff, the mMetadata may be used later again so let the decoder free it
	FLAC__stream_decoder_finish(vf);
	FLAC__stream_decoder_delete(vf);

	return true;
}

CFlacDec* CFlacDec::getInstance()
{
	static CFlacDec* FlacDec = NULL;
	if(FlacDec == NULL)
	{
		FlacDec = new CFlacDec();
	}
	return FlacDec;
}

void CFlacDec::ParseUserComments(FLAC__StreamMetadata_VorbisComment* vc, CAudioMetaData* m)
{
	if(vc->vendor_string.entry)
		printf("vendor_string = %d: %s\n", vc->vendor_string.length, vc->vendor_string.entry);

	printf("num_comments = %u\n", vc->num_comments);
	for(unsigned int i = 0; i < vc->num_comments; i++) {
		if(vc->comments[i].entry) {
			printf("comments[%u] = %d: %s\n", i, vc->comments[i].length, vc->comments[i].entry);
			const char *search;
			if((search=strstr((const char*)vc->comments[i].entry,"Artist"))!=NULL ||
			   (search=strstr((const char*)vc->comments[i].entry,"ARTIST"))!=NULL)
				m->artist = search+7;
			else if((search=strstr((const char*)vc->comments[i].entry,"Album"))!=NULL ||
			        (search=strstr((const char*)vc->comments[i].entry,"ALBUM"))!=NULL)
				m->album = search+6;
			else if((search=strstr((const char*)vc->comments[i].entry,"Title"))!=NULL ||
			        (search=strstr((const char*)vc->comments[i].entry,"TITLE"))!=NULL)
				m->title = search+6;
			else if((search=strstr((const char*)vc->comments[i].entry,"Genre"))!=NULL ||
			        (search=strstr((const char*)vc->comments[i].entry,"GENRE"))!=NULL)
				m->genre = search+6;
			else if((search=strstr((const char*)vc->comments[i].entry,"Date"))!=NULL ||
			        (search=strstr((const char*)vc->comments[i].entry,"DATE"))!=NULL)
				m->date = search+5;
			else if((search=strstr((const char*)vc->comments[i].entry,"TrackNumber"))!=NULL ||
			        (search=strstr((const char*)vc->comments[i].entry,"TRACKNUMBER"))!=NULL)
				m->track = search+12;
		}
	}
}

void CFlacDec::SetMetaData(FLAC__StreamMetadata *metadata, CAudioMetaData* m)
{
	/* Set Metadata */
	m->type = CAudioMetaData::FLAC;
	m->bitrate = mBps * mSampleRate;
	m->samplerate = mSampleRate;

	m->total_time = (time_t) mLengthInMsec / 1000;

	std::stringstream ss;
	ss << "FLAC V." << FLAC__VERSION_STRING << " / " <<  mChannels << "channel(s)";

	m->type_info = ss.str();

	ParseUserComments((FLAC__StreamMetadata_VorbisComment*)&(metadata->data.vorbis_comment), m);
	m->changed=true;
}

bool CFlacDec::Open(FILE* in, FLAC__StreamDecoder* vf)
{
	FLAC__StreamDecoderInitStatus rval;

	mIn = in;

	/* we need to use our own functions, because we have */
	/* the netfile layer hooked in here. If we would not */
	/* provide callbacks, the tremor lib and the netfile */
	/* layer would clash and steal each other the data   */
	/* from the stream !                                 */

	/* test the dope ... */
	rval = FLAC__stream_decoder_init_stream(vf,
						flac_read,
						flac_seek,
						flac_tell,
						flac_length,
						flac_eof,
						flac_write,
						flac_metadata,
						flac_error,
						(void *)this);
	/* and tell our friends about the quality of the stuff */
	// initialize the sound device here

	if(rval<0)
	{
		switch(rval)
		{
			/* err_txt from netfile.cpp */
			case FLAC__STREAM_DECODER_INIT_STATUS_OK: 			sprintf(err_txt, "Initialization was successful"); break;
			case FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER:	sprintf(err_txt, "The library was not compiled with support for the given container format"); break;
			case FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS:	sprintf(err_txt, "A required callback was not supplied"); break;
			case FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR:	sprintf(err_txt, "An error occurred allocating memory"); break;
			case FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE:	sprintf(err_txt, "fopen() failed in FLAC__stream_decoder_init_file() or FLAC__stream_decoder_init_ogg_file()"); break;
			case FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED:	sprintf(err_txt, "FLAC__stream_decoder_init_*() was called when the decoder was already initialized, usually because FLAC__stream_decoder_finish() was not called"); break;
			default:							sprintf(err_txt, "unknown error, code: %d", rval);
		}
//		fprintf(stderr,"%s: %s\n", ProgName, err_txt);
		printf("%s: %s\n", ProgName, err_txt);
		return false;
	}

	/* finish the opening and ignite the joint */
//	mSeekable = true;
	mSeekable = false;

	return true;
}
