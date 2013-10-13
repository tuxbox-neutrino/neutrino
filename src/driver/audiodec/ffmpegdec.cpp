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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <sstream>
#include <driver/audioplay.h>
#include <zapit/include/audio.h>
#include "ffmpegdec.h"
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <ao/ao.h>
}
#include <driver/netfile.h>

extern cAudio * audioDecoder;

#define ProgName "FfmpegDec"

static void log_callback(void*, int, const char*format, va_list ap)
{
	vfprintf(stderr, format, ap);

}

CFfmpegDec::CFfmpegDec(void)
{
	av_log_set_callback(log_callback);
	meta_data_valid = false;
	buffer_size = 0x1000;
	buffer = NULL;
	avc = NULL;
}

CFfmpegDec::~CFfmpegDec(void)
{
}

int CFfmpegDec::Read(void *buf, size_t buf_size)
{
	return fread(buf, buf_size, 1, (FILE *) in);
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	int res = ((CFfmpegDec *) opaque)->Read(buf, (size_t) buf_size);
	return res;
}

int64_t CFfmpegDec::Seek(int64_t offset, int whence)
{
	return (int64_t) fseek((FILE *) in, (long) offset, whence);
}

static int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
	return ((CFfmpegDec *) opaque)->Seek(offset, whence);
}

bool CFfmpegDec::Init(void)
{
	AVIOContext *avioc = NULL;
	if (!avc) {
		buffer = (unsigned char *) av_malloc(buffer_size);
		if (!buffer)
			return false;
		avcodec_register_all();
		av_register_all();
		avc = avformat_alloc_context();
		if (!avc)
			return false;

		avc->probesize = 128 * 1024;

		avioc = avio_alloc_context (buffer, buffer_size, 0, this, read_packet, NULL, seek_packet);
		if (!avioc) {
			av_freep(&buffer);
			avformat_free_context(avc);
			return false;
		}
		avc->pb = avioc;
		avc->flags = AVFMT_FLAG_CUSTOM_IO;
		if (!avformat_open_input(&avc, "", NULL, 0))
			return true;
	}
	if (avioc)
		av_freep(avioc);
	avc = NULL;
	return false;
}

void CFfmpegDec::DeInit(void)
{
	if (avc) {
		avformat_close_input(&avc);
#if 0
		av_freep(&avc->pb);
#endif
		avformat_free_context(avc);
		avc = NULL;
	}
#if 0
	if (buffer) {
		av_freep(&buffer);
	}
#endif
}

CBaseDec::RetCode CFfmpegDec::Decoder(FILE *_in, int /*OutputFd*/, State* state, CAudioMetaData* _meta_data, time_t* time_played, unsigned int* /*secondsToSkip*/)
{
	in = _in;
	RetCode Status=OK;

	if (!Init()) {
		Status=DATA_ERR;
		return Status;
	}


	if (!SetMetaData((FILE *)in, _meta_data)) {
		DeInit();
		Status=DATA_ERR;
		return Status;
	}

	AVCodecContext *c = avc->streams[best_stream]->codec;

	if(avcodec_open2(c, codec, NULL))
	{
		DeInit();
		Status=DATA_ERR;
		return Status;
	}

	SwrContext *swr = swr_alloc();
	if (!swr) {
		avcodec_close(c);
		DeInit();
		Status=DATA_ERR;
		return Status;
	}

	meta_data_valid = true;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	audioDecoder->PrepareClipPlay(mChannels, mSampleRate, 32, 1);
#else
	audioDecoder->PrepareClipPlay(mChannels, mSampleRate, 32, 0);
#endif

	AVFrame *frame = NULL;
	AVPacket packet;
	av_init_packet(&packet);

	av_opt_set_int(swr, "in_channel_layout",	c->channel_layout,	0);
	av_opt_set_int(swr, "out_channel_layout",	c->channel_layout,	0);
	av_opt_set_int(swr, "in_sample_rate",		c->sample_rate,		0);
	av_opt_set_int(swr, "out_sample_rate",		c->sample_rate,		0);
	av_opt_set_int(swr, "in_sample_fmt",		c->sample_fmt,		0);
	av_opt_set_int(swr, "out_sample_fmt",		AV_SAMPLE_FMT_S16,	0);

	swr_init(swr);

	uint8_t *outbuf = NULL;
	int outsamples = 0;
	int outsamples_max = 0;

	time_t starttime = time(NULL);

	do
	{
		while(*state==PAUSE)
			usleep(10000);

		if (av_read_frame(avc, &packet)) {
			Status=DATA_ERR;
			break;
		}

		if (packet.stream_index != best_stream)
			continue;

		while (packet.size > 0) {
			int got_frame = 0;
			if (!frame) {
				if (!(frame = avcodec_alloc_frame())) {
					Status=DATA_ERR;
					break;
				}
			} else
				avcodec_get_frame_defaults(frame);

			int len = avcodec_decode_audio4(c, frame, &got_frame, &packet);
			if (len < 0) {
				// skip frame
				packet.size = 0;
				avcodec_flush_buffers(c);
				avcodec_close(c);
				avcodec_open2(c, codec, NULL);
				continue;
			}
			if (got_frame) {
				int out_samples;
				outsamples = av_rescale_rnd(swr_get_delay(swr, c->sample_rate) + frame->nb_samples,
					c->sample_rate, c->sample_rate, AV_ROUND_UP);
				if (outsamples > outsamples_max) {
					av_free(outbuf);
					if (av_samples_alloc(&outbuf, &out_samples, c->channels,
								frame->nb_samples, AV_SAMPLE_FMT_S16, 1) < 0) {
						Status=WRITE_ERR;
						packet.size = 0;
						break;
					}
					outsamples_max = outsamples;
				}
				outsamples = swr_convert(swr, &outbuf, outsamples,
							(const uint8_t **) &frame->data[0], frame->nb_samples);
				int outbuf_size = av_samples_get_buffer_size(&out_samples, c->channels,
									  outsamples, AV_SAMPLE_FMT_S16, 1);

				if(audioDecoder->WriteClip((unsigned char*) outbuf, outbuf_size) != outbuf_size)
				{
					fprintf(stderr,"%s: PCM write error (%s).\n", ProgName, strerror(errno));
					Status=WRITE_ERR;
				}
			}
			packet.size -= len;
			packet.data += len;
		}
		if (time_played)
			*time_played = time(NULL) - starttime;
	} while (*state!=STOP_REQ && Status==OK);

	audioDecoder->StopClip();
	meta_data_valid = false;

	swr_free(&swr);
	av_free(outbuf);
	av_free_packet(&packet);
	avcodec_free_frame(&frame);
	avcodec_close(c);
	//av_free(avcc);

	DeInit();
	return Status;
}

bool CFfmpegDec::GetMetaData(FILE *_in, const bool /*nice*/, CAudioMetaData* m)
{
	return SetMetaData(_in, m);
}

CFfmpegDec* CFfmpegDec::getInstance()
{
	static CFfmpegDec* FfmpegDec = NULL;
	if(FfmpegDec == NULL)
	{
		FfmpegDec = new CFfmpegDec();
	}
	return FfmpegDec;
}

bool CFfmpegDec::SetMetaData(FILE * /* _in */, CAudioMetaData* m)
{
	bool needsInit = (avc == NULL);
	if (!meta_data_valid)
	{
		if (needsInit && !Init())
			return false;
		if (0 > avformat_find_stream_info(avc, NULL))
			return false;

		av_dump_format(avc, 0, "", 0);

		codec = NULL;
		best_stream = av_find_best_stream(avc, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
		if (best_stream < 0) {
			if (needsInit)
				DeInit();
			return false;
		}

		if (!codec)
			codec = avcodec_find_decoder(avc->streams[best_stream]->codec->codec_id);
		mSampleRate = avc->streams[best_stream]->codec->sample_rate;
		mChannels = av_get_channel_layout_nb_channels(avc->streams[best_stream]->codec->channel_layout);
	}
	m->samplerate = mSampleRate;
	std::stringstream ss;
	if (codec)
		ss << std::string(codec->long_name) + " / " << mChannels << " channel(s)";
	m->type_info = ss.str();
	m->changed=true;
	if (needsInit)
		DeInit();
	return true;
}
