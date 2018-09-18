/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2002 Bjoern Kalkbrenner <terminar@cyberphoria.org>
   (C) 2002,2003,2004 Zwen <Zwen@tuxbox.org>
	Copyright (C) 2010-2011 Stefan Seyfried

	libmad MP3 low-level core
	Homepage: http://www.cyberphoria.org/

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


/****************************************************************************
 * Includes																	*
 ****************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sstream>
#include <errno.h>
#include <string>
#include <linux/soundcard.h>
#include <assert.h>
#include <cmath>

#include <id3tag.h>

#include <global.h>
#include <neutrino.h>
#include <zapit/include/audio.h>
#include "mp3dec.h"
#include <driver/netfile.h>
#include <driver/display.h>
extern cAudio * audioDecoder;

//#define SPECTRUM

#ifdef SPECTRUM
void sanalyzer_render_freq(short data[1024]);
#endif
/* libid3tag extension: This is neccessary in order to call fclose
   on the file. Normally libid3tag closes the file implicit.
   For the netfile extension to work properly netfiles fclose must be called.
   To close an id3 file (usually by calling id3_file_close) without fclosing it,
   call following i3_finish_file function. It's just a copy of libid3tags
   finish_file function. */
extern "C"
{
//void id3_tag_addref(struct id3_tag *);
void my_id3_tag_delref(struct id3_tag *);
struct filetag {
	struct id3_tag *tag;
	unsigned long location;
	id3_length_t length;
};
struct id3_file {
	FILE *iofile;
	enum id3_file_mode mode;
	char *path;
	int flags;
	struct id3_tag *primary;
	unsigned int ntags;
	struct filetag *tags;
};
void id3_finish_file(struct id3_file* file);
}

// Frames to skip in ff/rev mode
#define FRAMES_TO_SKIP 50 // 75
// nr of frames to play after skipping in rev/ff mode
#define FRAMES_TO_PLAY 5

#define ProgName "CMP3Dec"

/****************************************************************************
 * Global variables.														*
 ****************************************************************************/

/****************************************************************************
 * Return an error string associated with a mad error code.					*
 ****************************************************************************/
/* Mad version 0.14.2b introduced the mad_stream_errorstr() function.
 * For previous library versions a replacement is provided below.
 */
#if (MAD_VERSION_MAJOR>=1) || \
    ((MAD_VERSION_MAJOR==0) && \
     (((MAD_VERSION_MINOR==14) && \
       (MAD_VERSION_PATCH>=2)) || \
      (MAD_VERSION_MINOR>14)))
#define MadErrorString(x) mad_stream_errorstr(x)
#else
const char *CMP3Dec::MadErrorString(const struct mad_stream *Stream)
{
	switch(Stream->error)
	{
		/* Generic unrecoverable errors. */
		case MAD_ERROR_BUFLEN:
			return("input buffer too small (or EOF)");
		case MAD_ERROR_BUFPTR:
			return("invalid (null) buffer pointer");
		case MAD_ERROR_NOMEM:
			return("not enough memory");

		/* Frame header related unrecoverable errors. */
		case MAD_ERROR_LOSTSYNC:
			return("lost synchronization");
		case MAD_ERROR_BADLAYER:
			return("reserved header layer value");
		case MAD_ERROR_BADBITRATE:
			return("forbidden bitrate value");
		case MAD_ERROR_BADSAMPLERATE:
			return("reserved sample frequency value");
		case MAD_ERROR_BADEMPHASIS:
			return("reserved emphasis value");

		/* Recoverable errors */
		case MAD_ERROR_BADCRC:
			return("CRC check failed");
		case MAD_ERROR_BADBITALLOC:
			return("forbidden bit allocation value");
		case MAD_ERROR_BADSCALEFACTOR:
			return("bad scalefactor index");
		case MAD_ERROR_BADFRAMELEN:
			return("bad frame length");
		case MAD_ERROR_BADBIGVALUES:
			return("bad big_values count");
		case MAD_ERROR_BADBLOCKTYPE:
			return("reserved block_type");
		case MAD_ERROR_BADSCFSI:
			return("bad scalefactor selection info");
		case MAD_ERROR_BADDATAPTR:
			return("bad main_data_begin pointer");
		case MAD_ERROR_BADPART3LEN:
			return("bad audio data length");
		case MAD_ERROR_BADHUFFTABLE:
			return("bad Huffman table select");
		case MAD_ERROR_BADHUFFDATA:
			return("Huffman data overrun");
		case MAD_ERROR_BADSTEREO:
			return("incompatible block_type for JS");

		/* Unknown error. This swich may be out of sync with libmad's
		 * defined error codes.
		 */
		default:
			return("Unknown error code");
	}
}
#endif

/****************************************************************************
 * Converts a sample from mad's fixed point number format to a signed       *
 * short (16 bits).                                                         *
 ****************************************************************************/
#if 1
struct audio_dither {
  mad_fixed_t error[3];
  mad_fixed_t random;
};
static struct audio_dither left_dither, right_dither;
static inline
unsigned long prng(unsigned long state)
{
  return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

inline signed short CMP3Dec::MadFixedToSShort(const mad_fixed_t Fixed, bool left)
{
  unsigned int scalebits;
  mad_fixed_t output, mask, random;
  struct audio_dither *dither = left ? &left_dither : &right_dither;
  unsigned int bits = 16;
  mad_fixed_t sample = Fixed;

  enum {
    MIN = -MAD_F_ONE,
    MAX =  MAD_F_ONE - 1
  };

  /* noise shape */
  sample += dither->error[0] - dither->error[1] + dither->error[2];

  dither->error[2] = dither->error[1];
  dither->error[1] = dither->error[0] / 2;

  /* bias */
  output = sample + (1L << (MAD_F_FRACBITS + 1 - bits - 1));

  scalebits = MAD_F_FRACBITS + 1 - bits;
  mask = (1L << scalebits) - 1;

  /* dither */
  random  = prng(dither->random);
  output += (random & mask) - (dither->random & mask);

  dither->random = random;

  /* clip */
#if 0
  if (output >= MAD_F_ONE)
        output = 32767;
  else if (output < -MAD_F_ONE)
        output = -32768;
#endif

  if (output > MAX)
        output = MAX;
  else if (output < MIN)
        output = MIN;

  /* quantize */
  output &= ~mask;

  /* error feedback */
  dither->error[0] = sample - output;

  /* scale */
  return (signed short) (output >> scalebits);
}
#else

inline signed short CMP3Dec::MadFixedToSShort(const mad_fixed_t Fixed)
{
	/* A fixed point number is formed of the following bit pattern:
	 *
	 * SWWWFFFFFFFFFFFFFFFFFFFFFFFFFFFF
	 * MSB                          LSB
	 * S ==> Sign (0 is positive, 1 is negative)
	 * W ==> Whole part bits
	 * F ==> Fractional part bits
	 *
	 * This pattern contains MAD_F_FRACBITS fractional bits, one
	 * should alway use this macro when working on the bits of a fixed
	 * point number. It is not guaranteed to be constant over the
	 * different platforms supported by libmad.
	 *
	 * The unsigned short value is formed by the least significant
	 * whole part bit, followed by the 15 most significant fractional
	 * part bits. Warning: this is a quick and dirty way to compute
	 * the 16-bit number, madplay includes much better algorithms.
	 */
	if (Fixed >= MAD_F_ONE)
		return 32767;
	else if (Fixed < -MAD_F_ONE)
		return -32768;

	return (signed short)(Fixed >> (MAD_F_FRACBITS + 1 - 16));
}
#endif
/****************************************************************************
 * Print human readable informations about an audio MPEG frame.             *
 ****************************************************************************/
void CMP3Dec::CreateInfo(CAudioMetaData* m, int FrameNumber)
{
	if ( !m )
		return;

	if ( !m->hasInfoOrXingTag )
	{
		m->total_time = m->avg_bitrate != 0 ?
			static_cast<int>(m->filesize / m->avg_bitrate)
			: 0;
	}

	if ( FrameNumber == 1 )
	{
		using namespace std;
		string Layer, Mode;

	/* Convert the layer number to it's printed representation. */
		switch(m->layer)
	{
		case MAD_LAYER_I:
			Layer="I";
			break;
		case MAD_LAYER_II:
			Layer="II";
			break;
		case MAD_LAYER_III:
			Layer="III";
			break;
		default:
			Layer="?";
			break;
	}

	/* Convert the audio mode to it's printed representation. */
		switch(m->mode)
	{
		case MAD_MODE_SINGLE_CHANNEL:
			Mode="single channel";
			break;
		case MAD_MODE_DUAL_CHANNEL:
			Mode="dual channel";
			break;
		case MAD_MODE_JOINT_STEREO:
			Mode="joint stereo";
			break;
		case MAD_MODE_STEREO:
			Mode="normal stereo";
			break;
		default:
			Mode="unkn. mode";
			break;
	}

#ifdef INCLUDE_UNUSED_STUFF
		const char *Emphasis, *Vbr;

	/* Convert the emphasis to it's printed representation. */
		switch(m->emphasis)
	{
		case MAD_EMPHASIS_NONE:
			Emphasis="no";
			break;
		case MAD_EMPHASIS_50_15_US:
			Emphasis="50/15 us";
			break;
		case MAD_EMPHASIS_CCITT_J_17:
			Emphasis="CCITT J.17";
			break;
		default:
			Emphasis="(unexpected emphasis value)";
			break;
	}

		if(m->vbr)
      Vbr="VBR ";
   else
      Vbr="";
#endif /* INCLUDE_UNUSED_STUFF */

		m->type_info = string("MPEG Layer ") + Layer + string(" / ") + Mode;
	}

	m->changed = true;
}

/****************************************************************************
 * Main decoding loop. This is where mad is used.                           *
 ****************************************************************************/
#define INPUT_BUFFER_SIZE	(2*8192) //(5*8192) /* enough to skip big id3 tags */
#define OUTPUT_BUFFER_SIZE      8192
#define SPECTRUM_CNT 4
CBaseDec::RetCode CMP3Dec::Decoder(FILE *InputFp, const int /*OutputFd*/,
								   State* const state,
								   CAudioMetaData* meta_data,
								   time_t* const time_played,
								   unsigned int* const secondsToSkip)
{
	struct mad_stream	Stream;
	struct mad_frame	Frame;
	struct mad_synth	Synth;
	mad_timer_t			Timer;
	unsigned char		InputBuffer[INPUT_BUFFER_SIZE],
						OutputBuffer[OUTPUT_BUFFER_SIZE],
						*OutputPtr=OutputBuffer;
	const unsigned char	*OutputBufferEnd=OutputBuffer+OUTPUT_BUFFER_SIZE;
	RetCode				Status=OK;
	int 					ret;
	unsigned long		FrameCount=0;

#ifdef SPECTRUM
        bool update_lcd = false;
        int i = 0, j = 0;
        signed short data[1024];
	if(g_settings.spectrum)
		CVFD::getInstance ()->Lock ();
	int scnt = 0;
#endif
        signed short ll, rr;

	SaveCover(InputFp, meta_data);
	/* First the structures used by libmad must be initialized. */
	mad_stream_init(&Stream);
	mad_frame_init(&Frame);
	mad_synth_init(&Synth);
	mad_timer_reset(&Timer);

	// used as kind of mutex for single jumps in the file
	// to make sure the amount of frames is calculated
	// before jumping (without this state/secondsToSkip could change
	// anytime within the loop)
	bool jumpDone=false;

	/* Decoding options can here be set in the options field of the
	 * Stream structure.
	 */

	/* This is the decoding loop. */
	do
	{
		int secondsToJump = *secondsToSkip;
		if(*state==PAUSE)
		{
			// in pause mode do nothing
			usleep(100000);
			continue;
		}
		/* The input bucket must be filled if it becomes empty or if
		 * it's the first execution of the loop.
		 */
		if(Stream.buffer==NULL || Stream.error==MAD_ERROR_BUFLEN)
		{
			size_t			ReadSize,
							Remaining;
			unsigned char	*ReadStart;

			/* {1} libmad may not consume all bytes of the input
			 * buffer. If the last frame in the buffer is not wholly
			 * contained by it, then that frame's start is pointed by
			 * the next_frame member of the Stream structure. This
			 * common situation occurs when mad_frame_decode() fails,
			 * sets the stream error code to MAD_ERROR_BUFLEN, and
			 * sets the next_frame pointer to a non NULL value. (See
			 * also the comment marked {2} bellow.)
			 *
			 * When this occurs, the remaining unused bytes must be
			 * put back at the beginning of the buffer and taken in
			 * account before refilling the buffer. This means that
			 * the input buffer must be large enough to hold a whole
			 * frame at the highest observable bit-rate (currently 448
			 * kb/s). XXX=XXX Is 2016 bytes the size of the largest
			 * frame? (448000*(1152/32000))/8
			 */
			if(Stream.next_frame!=NULL)
			{
				Remaining=Stream.bufend-Stream.next_frame;
				memmove(InputBuffer,Stream.next_frame,Remaining);
				ReadStart=InputBuffer+Remaining;
				ReadSize=INPUT_BUFFER_SIZE-Remaining;
			}
			else
				ReadSize=INPUT_BUFFER_SIZE,
					ReadStart=InputBuffer,
					Remaining=0;

			/* Fill-in the buffer. If an error occurs print a message
			 * and leave the decoding loop. If the end of stream is
			 * reached we also leave the loop but the return status is
			 * left untouched.
			 */
			ReadSize=fread(ReadStart,1,ReadSize,InputFp);
			if(ReadSize<=0)
			{
				if(ferror(InputFp))
				{
					fprintf(stderr,"%s: read error on bitstream (%s)\n",
							ProgName,strerror(errno));
					Status=READ_ERR;
				}
				if(feof(InputFp))
					fprintf(stderr,"%s: end of input stream\n",ProgName);
				break;
			}

			/* Pipe the new buffer content to libmad's stream decoder
			 * facility.  */
			mad_stream_buffer(&Stream,InputBuffer,ReadSize+Remaining);
			Stream.error=(mad_error)0;
		}

		long actFramesToSkip=FRAMES_TO_SKIP;
		// Calculate the amount of frames within the custom period
		if(((*state==FF) || (*state==REV)) && ((secondsToJump)!=0))
		{
			jumpDone=false;

			// what is 1152? grabbed it from the documentation above {1} ;)
			actFramesToSkip=secondsToJump * meta_data->samplerate/1152;
			//printf("secsToSkip: %d, framesToSkip: %ld\n", secondsToJump,actFramesToSkip);
		}

		/* Decode the next mpeg frame. The streams is read from the
		 * buffer, its constituents are break down and stored the the
		 * Frame structure, ready for examination/alteration or PCM
		 * synthesis. Decoding options are carried in the Frame
		 * structure from the Stream structure.
		 *
		 * Error handling: mad_frame_decode() returns a non zero value
		 * when an error occurs. The error condition can be checked in
		 * the error member of the Stream structure. A mad error is
		 * recoverable or fatal, the error status is checked with the
		 * MAD_RECOVERABLE macro.
		 *
		 * {2} When a fatal error is encountered all decoding
		 * activities shall be stopped, except when a MAD_ERROR_BUFLEN
		 * is signaled. This condition means that the
		 * mad_frame_decode() function needs more input to achieve
		 * it's work. One shall refill the buffer and repeat the
		 * mad_frame_decode() call. Some bytes may be left unused at
		 * the end of the buffer if those bytes forms an incomplete
		 * frame. Before refilling, the remainign bytes must be moved
		 * to the begining of the buffer and used for input for the
q		 * next mad_frame_decode() invocation. (See the comments marked
		 * {1} earlier for more details.)
		 *
		 * Recoverable errors are caused by malformed bit-streams, in
		 * this case one can call again mad_frame_decode() in order to
		 * skip the faulty part and re-sync to the next frame.
		 */
		// decode 'FRAMES_TO_PLAY' frames each 'FRAMES_TO_SKIP' frames in ff/rev mode
		if( (*state!=FF &&
			  *state!=REV) ||
		    FrameCount % actFramesToSkip < FRAMES_TO_PLAY )
			ret=mad_frame_decode(&Frame,&Stream);
		else if(*state==FF) // in FF mode just decode the header, this sets bufferptr to next frame and also gives stats about the frame for totals
			if (secondsToJump != 0 && !jumpDone)
			{
				jumpDone=true;
				// jump forwards
				long bytesForward = (Stream.bufend - Stream.this_frame) + ((ftell(InputFp)+Stream.this_frame-Stream.bufend) / FrameCount)*(actFramesToSkip + FRAMES_TO_PLAY);
				//printf("jump forwards by %d secs and %ld bytes",secondsToJump, bytesForward);

				if (fseek(InputFp, bytesForward, SEEK_CUR)!=0)
				{
					// Reached end, do nothing
				}
				else
				{
					// Calculate timer
					mad_timer_t m;
					mad_timer_set(&m, 0, 32 * MAD_NSBSAMPLES(&Frame.header) *(actFramesToSkip + FRAMES_TO_PLAY), Frame.header.samplerate);
					Timer.seconds += m.seconds;
					if((Timer.fraction + m.fraction)*MAD_TIMER_RESOLUTION>=1)
					{
						Timer.seconds++;
						Timer.fraction-= m.fraction;
					}
					else
						Timer.fraction+= m.fraction;
					// in case we calculated wrong...
					if(Timer.seconds < 0)
					{
						Timer.seconds=0;
						Timer.fraction=0;
					}
					*time_played=Timer.seconds;
					FrameCount+=actFramesToSkip + FRAMES_TO_PLAY;
				}
				Stream.buffer=NULL;
				Stream.next_frame=NULL;
				// if a custom value was set we only jump once
				*state=PLAY;
				continue;
			} else
			{
				ret=mad_header_decode(&Frame.header,&Stream);
			}
		else
		{ //REV
			// Jump back
			long bytesBack = (Stream.bufend - Stream.this_frame) + ((ftell(InputFp)+Stream.this_frame-Stream.bufend) / FrameCount)*(actFramesToSkip + FRAMES_TO_PLAY);

			if (secondsToJump!=0)
			{
				jumpDone=true;
				//printf("jumping backwards by %d secs and %ld bytes\n",secondsToJump, bytesBack);
			}
			if (fseek(InputFp, -1*(bytesBack), SEEK_CUR)!=0)
			{
				// Reached beginning
				fseek(InputFp, 0, SEEK_SET);
				Timer.fraction=0;
				Timer.seconds=0;
				FrameCount=0;
				*state=PLAY;
			}
			else
			{
				// Calculate timer
				mad_timer_t m;
				mad_timer_set(&m, 0, 32 * MAD_NSBSAMPLES(&Frame.header) *(actFramesToSkip + FRAMES_TO_PLAY), Frame.header.samplerate);
				Timer.seconds -= m.seconds;
				if(Timer.fraction < m.fraction)
				{
					Timer.seconds--;
					Timer.fraction+= MAD_TIMER_RESOLUTION - m.fraction;
				}
				else
					Timer.fraction-= m.fraction;
				// in case we calculated wrong...
				if(Timer.seconds < 0)
				{
					Timer.seconds=0;
					Timer.fraction=0;
				}
				*time_played=Timer.seconds;
				FrameCount-=actFramesToSkip + FRAMES_TO_PLAY;
			}
			Stream.buffer=NULL;
			Stream.next_frame=NULL;
			// if a custom value was set we only jump once
			if (secondsToJump != 0) {
				*state=PLAY;
			}
			continue;
		}

		if(ret)
		{
			if(MAD_RECOVERABLE(Stream.error))
			{
				// no errrors in FF mode
				if(*state!=FF &&
					*state!=REV)
				{
					fprintf(stderr,"%s: recoverable frame level error (%s)\n",
						ProgName,MadErrorString(&Stream));
					fflush(stderr);
				 }
				continue;
			}
			else
				if(Stream.error==MAD_ERROR_BUFLEN)
					continue;
				else
				{
					fprintf(stderr,"%s: unrecoverable frame level error (%s).\n",
						ProgName,MadErrorString(&Stream));
					Status=DATA_ERR;
					break;
				}
		}

		/* On first frame set DSP & save header info
		 * The first frame is representative of the entire
		 * stream.
		 */
		FrameCount++;
		if (FrameCount == 1)
		{
#if 0
			if (SetDSP(OutputFd, AFMT_S16_NE, Frame.header.samplerate, 2))
			{
				Status=DSPSET_ERR;
				break;
			}
#endif
			audioDecoder->PrepareClipPlay(2, Frame.header.samplerate, 16, 1);

			if ( !meta_data )
			{
				meta_data = new CAudioMetaData;
			}
			meta_data->samplerate = Frame.header.samplerate;
			meta_data->bitrate = Frame.header.bitrate;
			meta_data->mode = Frame.header.mode;
			meta_data->layer = Frame.header.layer;
			CreateInfo( meta_data, FrameCount );
		}
		else
		{
			if ( meta_data->bitrate != Frame.header.bitrate )
			{
				/* bitrate of actual frame */
				meta_data->vbr = true;
				meta_data->bitrate = Frame.header.bitrate;

				/* approximate average bitrate */
				meta_data->avg_bitrate -=
					meta_data->avg_bitrate / FrameCount;
				meta_data->avg_bitrate +=
					Frame.header.bitrate / FrameCount;
				CreateInfo( meta_data, FrameCount );
			}
		}

		// if played time was modified from outside, take this value...
		if(*time_played!=Timer.seconds)
		{
			mad_timer_reset(&Timer);
			Timer.seconds = *time_played;
		}

		/* Accounting. The computed frame duration is in the frame
		 * header structure. It is expressed as a fixed point number
		 * whole data type is mad_timer_t. It is different from the
		 * samples fixed point format and unlike it, it can't directly
		 * be added or substracted. The timer module provides several
		 * functions to operate on such numbers. Be careful there, as
		 * some functions of mad's timer module receive some of their
		 * mad_timer_t arguments by value!
		 */
		mad_timer_add(&Timer,Frame.header.duration);
		//mad_timer_string(Timer,m_timePlayed,"%lu:%02lu",
      //                 MAD_UNITS_MINUTES,MAD_UNITS_MILLISECONDS,0);
		*time_played = Timer.seconds;


		// decode 5 frames each 75 frames in ff mode
		if( *state!=FF || FrameCount % actFramesToSkip < FRAMES_TO_PLAY)
		{

			/* Once decoded the frame is synthesized to PCM samples. No errors
			 * are reported by mad_synth_frame();
			 */
			mad_synth_frame(&Synth, &Frame);


			/* Synthesized samples must be converted from mad's fixed
			 * point number to the consumer format. Here we use signed
			 * 16 bit native endian integers on two channels. Integer samples
			 * are temporarily stored in a buffer that is flushed when
			 * full.
			 */

			if (MAD_NCHANNELS(&Frame.header) == 2)
			{
				mad_fixed_t * leftchannel = Synth.pcm.samples[0];
				mad_fixed_t * rightchannel = Synth.pcm.samples[1];

				while (Synth.pcm.length-- > 0)
				{
                                        ll = MadFixedToSShort(*(leftchannel++), true);
                                        rr = MadFixedToSShort(*(rightchannel++), false);
                                        *((signed short *)OutputPtr) = ll;
                                        *(((signed short *)OutputPtr) + 1) = rr;

					OutputPtr += 4;
#ifdef SPECTRUM
#define DDIFF 0
					if(g_settings.spectrum) {
#if HAVE_DBOX2
					if(scnt == SPECTRUM_CNT-2)
#endif
					{
                                        	j += 4;
                                        	if((i < 512) && (j > DDIFF)) {
//if(i == 0) printf("Filling on j = %d\n", j);
                                                        int tmp = (ll+rr)/2;
                                                        data[i] = (signed short)tmp;
                                        	        i++;
                                        	}
					}
					}
#endif

					/* Flush the buffer if it is full. */
					if (OutputPtr == OutputBufferEnd)
					{
						//if (write(OutputFd, OutputBuffer, OUTPUT_BUFFER_SIZE) != OUTPUT_BUFFER_SIZE)
						if(audioDecoder->WriteClip(OutputBuffer, OUTPUT_BUFFER_SIZE) != OUTPUT_BUFFER_SIZE)
						{
							fprintf(stderr,"%s: PCM write error in stereo (%s).\n", ProgName, strerror(errno));
							Status = WRITE_ERR;
							Synth.pcm.length = 0; /* discard buffer */
							OutputPtr = OutputBuffer;
							break;
						}

						OutputPtr = OutputBuffer;
#ifdef SPECTRUM
						if(g_settings.spectrum) {
                                                	i = 0;
                                                	j = 0;
                                                	update_lcd = true;

//gettimeofday (&tv2, NULL);
//int ms = (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000;
//if(ms)printf("Write takes %dms\n", ms);
                                                	//sanalyzer_render_vu(data);
							scnt++;
#if HAVE_DBOX2
							if(scnt >= SPECTRUM_CNT)
#endif
							{
								scnt = 0;
                                                		sanalyzer_render_freq(data);
							}
						}
#endif
					}
				}
			}
			else
			{
				mad_fixed_t * leftchannel = Synth.pcm.samples[0];

				while (Synth.pcm.length-- > 0)
				{
					/* Left channel => copy to right channel */

					//*(((signed short *)OutputPtr) + 1) = *((signed short *)OutputPtr) = MadFixedToSShort(*(leftchannel++));
					ll = MadFixedToSShort(*(leftchannel++), false);
					rr = ll;
					*(((signed short *)OutputPtr) + 1) = *((signed short *)OutputPtr) = ll;

					OutputPtr += 4;
#ifdef SPECTRUM
#define DDIFF 1024
					if(g_settings.spectrum) {
#if HAVE_DBOX2
					if(scnt == SPECTRUM_CNT-2)
#endif
					{
                                        	j += 4;
                                        	if((i < 512) && (j > DDIFF)) {
//if(i == 0) printf("Filling on j = %d\n", j);
                                                        int tmp = (ll+rr)/2;
                                                        data[i] = (signed short)tmp;
                                        	        i++;
                                        	}
					}
					}
#endif

					/* Flush the buffer if it is full. */
					if (OutputPtr == OutputBufferEnd)
					{
						//if (write(OutputFd, OutputBuffer, OUTPUT_BUFFER_SIZE) != OUTPUT_BUFFER_SIZE)
						if(audioDecoder->WriteClip(OutputBuffer, OUTPUT_BUFFER_SIZE) != OUTPUT_BUFFER_SIZE)
						{
							fprintf(stderr,"%s: PCM write error in mono (%s).\n", ProgName, strerror(errno));
							Status = WRITE_ERR;
							Synth.pcm.length = 0; /* discard buffer */
							OutputPtr = OutputBuffer;
							break;
						}

						OutputPtr = OutputBuffer;
#ifdef SPECTRUM
						if(g_settings.spectrum) {
                                                	i = 0;
                                                	j = 0;
                                                	update_lcd = true;

//gettimeofday (&tv2, NULL);
//int ms = (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000;
//if(ms)printf("Write takes %dms\n", ms);
#if HAVE_DBOX2
							if(scnt >= SPECTRUM_CNT)
#endif
							{
                                                		sanalyzer_render_freq(data);
							}
						}
#endif
					}
				}
			}
		}

		// if a custom value was set we only jump once
		if ((*state==FF || *state==REV) && secondsToJump != 0 && !jumpDone) {
			jumpDone=true;
			*state=PLAY;
		}

	}while(*state!=STOP_REQ);

	/* Mad is no longer used, the structures that were initialized must
     * now be cleared.
	 */
	mad_synth_finish(&Synth);
	mad_frame_finish(&Frame);
	mad_stream_finish(&Stream);

	/* If the output buffer is not empty and no error occured during
     * the last write, then flush it.
	 */
	if(OutputPtr!=OutputBuffer && Status!=WRITE_ERR)
	{
		ssize_t	BufferSize=OutputPtr-OutputBuffer;

		//if(write(OutputFd, OutputBuffer, BufferSize)!=BufferSize)
		if(audioDecoder->WriteClip(OutputBuffer, BufferSize) != BufferSize)
  		{
			fprintf(stderr,"%s: PCM write error at the end (%s).\n", ProgName,strerror(errno));
			Status=WRITE_ERR;
		}
	}
	audioDecoder->StopClip();
#ifdef SPECTRUM
	CVFD::getInstance ()->Unlock ();
#endif
	/* Accounting report if no error occured. */
	if(Status==OK)
	{
		/* The duration timer is converted to a human readable string
		 * with the versatile but still constrained mad_timer_string()
		 * function, in a fashion not unlike strftime(). The main
		 * difference is that the timer is break-down into several
		 * values according some of it's arguments. The units and
		 * fracunits arguments specify the intended conversion to be
		 * executed.
		 *
		 * The conversion unit (MAD_UNIT_MINUTES in our example) also
		 * specify the order and kind of conversion specifications
		 * that can be used in the format string.
		 *
		 * It is best to examine mad's timer.c source-code for details
		 * of the available units, fraction of units, their meanings,
		 * the format arguments, etc.
		 */
	   //		mad_timer_string(Timer,m_timePlayed,"%lu:%02lu",
		//				 MAD_UNITS_MINUTES,MAD_UNITS_MILLISECONDS,0);
		*time_played = Timer.seconds;

//		      fprintf(stderr,"%s: %lu frames decoded (%s).\n",
//				ProgName,FrameCount,Buffer);
	}
	if (!meta_data->cover.empty())
		unlink(meta_data->cover.c_str());

	/* That's the end of the world (in the H. G. Wells way). */
	return Status;
}

CMP3Dec* CMP3Dec::getInstance()
{
	static CMP3Dec* MP3Dec = NULL;
	if(MP3Dec == NULL)
	{
		MP3Dec = new CMP3Dec();
	}
	return MP3Dec;
}

bool CMP3Dec::GetMetaData(FILE* in, const bool nice, CAudioMetaData* const m)
{
	bool res;
	if ( in && m )
	{
		res = GetMP3Info(in, nice, m);
		GetID3(in, m);
		//SaveCover(in, m);
	}
	else
	{
		res = false;
	}

	return res;
}

/*
 * Scans MP3 header for Xing, Info and Lame tag.  Returns -1 on failure,
 * >= 0 on success.  The returned value specifies the location of the
 * first audio frame.
 *
 * @author Christian Schlotter
 * @date   2004
 *
 * Based on scan_header() from Robert Leslie's "MAD Plug-in for Winamp".
 */
#define BUFFER_SIZE (5*8192) // big enough to skip id3 tags containing jpegs
long CMP3Dec::scanHeader( FILE* input, struct mad_header* const header,
						  struct tag* const ftag, const bool nice )
{
	struct mad_stream stream;
	struct mad_frame frame;
	unsigned char buffer[BUFFER_SIZE];
	unsigned int buflen = 0;
	int count = 0;
	short refillCount = 4; /* buffer may be refilled refillCount times */
	long filePos = 0; /* return value */

	mad_stream_init( &stream );
	mad_frame_init( &frame );

	if ( ftag )
		tag_init( ftag );

	while ( true )
	{
		if ( buflen < sizeof(buffer) )
		{
			if ( nice )
				usleep( 15000 );

			filePos = ftell( input ); /* remember where reading started */
			if ( filePos == -1 )
			{
				perror( "ftell()" );
			}

			/* fill buffer */
			int readbytes = fread( buffer+buflen, 1, sizeof(buffer)-buflen,
								   input );
			if ( readbytes <= 0 )
			{
				if ( readbytes == -1 )
					filePos = -1;
				break;
			}

			buflen += readbytes;
		}

		mad_stream_buffer( &stream, buffer, buflen );

		while ( true )
		{
			const unsigned char* const actualFrame = stream.this_frame;
			if ( mad_frame_decode( &frame, &stream ) == -1 )
			{
				if ( !MAD_RECOVERABLE( stream.error ) )
					break;

				/* check if id3 tag is in the way */
				long tagsize =
					id3_tag_query( stream.this_frame,
								   stream.bufend - stream.this_frame );

				if ( tagsize > 0 ) /* id3 tag recognized */
				{
					mad_stream_skip( &stream, tagsize );
					continue;
				}
				else if ( mad_stream_sync( &stream ) != -1 ) /* try to sync */
				{
					continue;
				}
				else /* syncing attempt failed */
				{
					/* we have to set some limit here, otherwise we would scan
					   junk files completely */
					if ( refillCount-- )
					{
						stream.error = MAD_ERROR_BUFLEN;
					}
					break;
				}
			}

			if ( count++ || ( ftag && tag_parse(ftag, &stream) == -1 ) )
			{
				filePos += actualFrame - buffer; /* start of audio data */
				break;
			}
		}

		if ( count || stream.error != MAD_ERROR_BUFLEN )
			break;

		if ( refillCount-- )
		{
			memmove( buffer, stream.next_frame,
					 buflen = &buffer[buflen] - stream.next_frame );
		}
		else
		{
			break;
		}
	}

	if ( count )
	{
		if ( header )
		{
			*header = frame.header;
		}
	}
	else
	{
		filePos = -1;
	}

	mad_frame_finish( &frame );
	mad_stream_finish( &stream );

	return filePos;
}

/*
 * Function retrieves MP3 metadata.  Returns false on failure, true on success.
 *
 * Inspired by get_fileinfo() from Robert Leslie's "MAD Plug-in for Winamp" and
 * decode_filter() from Robert Leslie's "madplay".
 */
bool CMP3Dec::GetMP3Info( FILE* input, const bool nice,
						  CAudioMetaData* const meta )
{
	struct mad_header header;
	struct tag ftag;
	mad_header_init( &header );
	tag_init( &ftag );
	bool result = true;

	if ( ( meta->audio_start_pos = scanHeader(input, &header, &ftag, nice) )
		 != -1 )
	{
		meta->type = CAudioMetaData::MP3;
		meta->bitrate = header.bitrate;
		meta->layer = header.layer;
		meta->mode = header.mode;
		meta->samplerate = header.samplerate;

		if ( fseek( input, 0, SEEK_END ) )
		{
			perror( "fseek()" );
			result = false;
		}
		/* this is still not 100% accurate, because it does not take
		   id3 tags at the end of the file in account */
		meta->filesize = ( ftell( input ) - meta->audio_start_pos ) * 8;

		/* valid Xing vbr tag present? */
		if ( ( ftag.flags & TAG_XING ) &&
			 ( ftag.xing.flags & TAG_XING_FRAMES ) )
		{
			meta->hasInfoOrXingTag = true;
			mad_timer_t timer = header.duration;
			mad_timer_multiply( &timer, ftag.xing.frames );

			meta->total_time = mad_timer_count( timer, MAD_UNITS_SECONDS );
		}
		else /* no valid Xing vbr tag present */
		{
			meta->total_time = header.bitrate != 0
				? meta->filesize / header.bitrate : 0;
		}

		/* vbr file */
		if ( ftag.flags & TAG_VBR )
		{
			meta->vbr = true;
			meta->avg_bitrate = meta->total_time != 0
				? static_cast<int>(meta->filesize / meta->total_time)
				: 0;
		}
		else /* we do not know wether the file is vbr or not */
		{
			meta->vbr = false;
			meta->avg_bitrate = header.bitrate;

		}
	}
	else /* scanning failed */
	{
		result = false;
	}

	if ( !result )
	{
		meta->clear();
	}

	return result;
}


//------------------------------------------------------------------------
void CMP3Dec::GetID3(FILE* in, CAudioMetaData* const m)
{
	unsigned int i;
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_utf8_t *utf8;
	char const spaces[] = "          ";

	struct
		{
		char const *id;
		char const *name;
	} const info[] =
		{
			{ ID3_FRAME_TITLE,  "Title"},
			{ "TIT3",           0},	 /* Subtitle */
			{ "TCOP",           0,},  /* Copyright */
			{ "TPRO",           0,},  /* Produced */
			{ "TCOM",           "Composer"},
			{ ID3_FRAME_ARTIST, "Artist"},
			{ "TPE2",           "Orchestra"},
			{ "TPE3",           "Conductor"},
			{ "TEXT",           "Lyricist"},
			{ ID3_FRAME_ALBUM,  "Album"},
			{ ID3_FRAME_YEAR,   "Year"},
			{ ID3_FRAME_TRACK,  "Track"},
			{ "TPUB",           "Publisher"},
			{ ID3_FRAME_GENRE,  "Genre"},
			{ "TRSN",           "Station"},
			{ "TENC",           "Encoder"}
		};

	/* text information */

	struct id3_file *id3file = id3_file_fdopen(fileno(in), ID3_FILE_MODE_READONLY);
	if(id3file == 0)
		printf("error open id3 file\n");
	else
	{
		id3_tag *tag=id3_file_tag(id3file);
		if(tag)
		{
			for(i = 0; i < sizeof(info) / sizeof(info[0]); ++i)
			{
				union id3_field const *field;
				unsigned int nstrings, namelen, j;
				char const *name;

				frame = id3_tag_findframe(tag, info[i].id, 0);
				if(frame == 0)
					continue;

				field    = &frame->fields[1];
				nstrings = id3_field_getnstrings(field);

				name = info[i].name;
				namelen = name ? strlen(name) : 0;
				assert(namelen < sizeof(spaces));

				for(j = 0; j < nstrings; ++j)
				{
					ucs4 = id3_field_getstrings(field, j);
					assert(ucs4);

					if(strcmp(info[i].id, ID3_FRAME_GENRE) == 0)
						ucs4 = id3_genre_name(ucs4);

					utf8 = id3_ucs4_utf8duplicate(ucs4);
					if (utf8 == NULL)
						goto fail;

					if (j == 0 && name)
					{
						if(strcmp(name,"Title") == 0)
							m->title = (char *) utf8;
						if(strcmp(name,"Artist") == 0)
							m->artist = (char *) utf8;
						if(strcmp(name,"Year") == 0)
							m->date = (char *) utf8;
						if(strcmp(name,"Album") == 0)
							m->album = (char *) utf8;
						if(strcmp(name,"Genre") == 0)
							m->genre = (char *) utf8;
					}
					else
					{
						if(strcmp(info[i].id, "TCOP") == 0 || strcmp(info[i].id, "TPRO") == 0)
						{
							//printf("%s  %s %s\n", spaces, (info[i].id[1] == 'C') ? ("Copyright (C)") : ("Produced (P)"), latin1);
						}
						//else
						//printf("%s  %s\n", spaces, latin1);
					}

					free(utf8);
				}
			}

#ifdef INCLUDE_UNUSED_STUFF
			/* comments */

			i = 0;
			while((frame = id3_tag_findframe(tag, ID3_FRAME_COMMENT, i++)))
			{
				id3_utf8_t *ptr, *newline;
				int first = 1;

				ucs4 = id3_field_getstring(&frame->fields[2]);
				assert(ucs4);

				if(*ucs4)
					continue;

				ucs4 = id3_field_getfullstring(&frame->fields[3]);
				assert(ucs4);

				utf8 = id3_ucs4_utf8duplicate(ucs4);
				if (utf8 == 0)
					goto fail;

				ptr = utf8;
				while(*ptr)
				{
					newline = (id3_utf8_t *) strchr((char*)ptr, '\n');
					if(newline)
						*newline = 0;

					if(strlen((char *)ptr) > 66)
					{
						id3_utf8_t *linebreak;

						linebreak = ptr + 66;

						while(linebreak > ptr && *linebreak != ' ')
							--linebreak;

						if(*linebreak == ' ')
						{
							if(newline)
								*newline = '\n';

							newline = linebreak;
							*newline = 0;
						}
					}

					if(first)
					{
						char const *name;
						unsigned int namelen;

						name    = "Comment";
						namelen = strlen(name);
						assert(namelen < sizeof(spaces));
						mp3->Comment = (char *) ptr;
						//printf("%s%s: %s\n", &spaces[namelen], name, ptr);
						first = 0;
					}
					else
						//printf("%s  %s\n", spaces, ptr);

						ptr += strlen((char *) ptr) + (newline ? 1 : 0);
				}

				free(utf8);
				break;
			}
#endif
			id3_tag_delete(tag);
		}
		else
			printf("error open id3 tag\n");

		id3_finish_file(id3file);
	}
	if(0)
	{
		fail:
			printf("id3: not enough memory to display tag\n");
	}
}

static int cover_count = 0;

bool CMP3Dec::SaveCover(FILE * in, CAudioMetaData * const m)
{
	struct id3_frame const *frame;

	/* text information */
	struct id3_file *id3file = id3_file_fdopen(fileno(in), ID3_FILE_MODE_READONLY);

	if(id3file == 0)
	{
		printf("CMP3Dec::SaveCover: error open id3 file\n");
		return false;
	}
	else
	{
		id3_tag * tag = id3_file_tag(id3file);
		if(tag)
		{
			frame = id3_tag_findframe(tag, "APIC", 0);

			if (frame)
			{
				printf("CMP3Dec::SaveCover: Cover found\n");
				// Picture file data
				unsigned int j;
				union id3_field const *field;

				for (j = 0; (field = id3_frame_field(frame, j)); j++)
				{
					switch (id3_field_type(field))
					{
						case ID3_FIELD_TYPE_BINARYDATA:
							id3_length_t size;
							id3_byte_t const *data;

							data = id3_field_getbinarydata(field, &size);
							if ( data )
							{
								mkdir(COVERDIR_TMP, 0755);
								std::ostringstream cover;
								cover.str(COVERDIR_TMP);
								cover << "/cover_" << cover_count++ << ".jpg";
								FILE * pFile;
								pFile = fopen ( cover.str().c_str() , "wb" );
								if (pFile)
								{
									fwrite (data , 1 , size , pFile );
									fclose (pFile);
									m->cover = cover.str().c_str();
									m->changed = true;
								}
							}
							break;

						case ID3_FIELD_TYPE_INT8:
							//pic->type = id3_field_getint(field);
							break;

						default:
							break;
					}
				}
			}

			id3_tag_delete(tag);
		}
		else
		{
			printf("CMP3Dec::SaveCover: error open id3 tag\n");
			return false;
		}

		id3_finish_file(id3file);
	}

	if(0)
	{
		printf("CMP3Dec::SaveCover:id3: not enough memory to display tag\n");
		return false;
	}

	return true;
}

// this is a copy of static libid3tag function "finish_file"
// which cannot be called from outside
void id3_finish_file(struct id3_file* file)
{
	unsigned int i;

	if (file->path)
		free(file->path);

	if (file->primary) {
		my_id3_tag_delref(file->primary);
		id3_tag_delete(file->primary);
	}

	for (i = 0; i < file->ntags; ++i) {
		struct id3_tag *tag;

		tag = file->tags[i].tag;
		if (tag) {
			my_id3_tag_delref(tag);
			id3_tag_delete(tag);
		}
	}

	if (file->tags)
		free(file->tags);

	free(file);
}

/* copy of id3_tag_delref, since a properly built libid3tag does
 * not export this (it's not part of the public API... */
void my_id3_tag_delref(struct id3_tag *tag)
{
	assert(tag && tag->refcount > 0);
	--tag->refcount;
}
