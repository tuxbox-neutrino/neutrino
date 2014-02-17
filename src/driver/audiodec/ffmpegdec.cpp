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

#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <sstream>
#include <driver/audioplay.h>
#include <zapit/include/audio.h>
#include <eitd/edvbstring.h> // UTF8
#include "ffmpegdec.h"
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include <OpenThreads/ScopedLock>

#include <driver/netfile.h>
#include <system/helpers.h>

extern cAudio * audioDecoder;

//#define FFDEC_DEBUG

#define ProgName "FfmpegDec"

#define COVERDIR "/tmp/cover"

static OpenThreads::Mutex mutex;

static int cover_count = 0;

#if 0
static void log_callback(void *, int, const char *format, va_list ap)
{
	vfprintf(stderr, format, ap);
}
#endif

CFfmpegDec::CFfmpegDec(void)
{
	//av_log_set_callback(log_callback);
	meta_data_valid = false;
	buffer_size = 0x1000;
	buffer = NULL;
	avc = NULL;
	avcodec_register_all();
	av_register_all();
}

CFfmpegDec::~CFfmpegDec(void)
{
	DeInit();
}

int CFfmpegDec::Read(void *buf, size_t buf_size)
{
	return (int) fread(buf, 1, buf_size, (FILE *) in);
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	return ((CFfmpegDec *) opaque)->Read(buf, (size_t) buf_size);
}

int64_t CFfmpegDec::Seek(int64_t offset, int whence)
{
	if (whence == AVSEEK_SIZE)
		return (int64_t) -1;

	fseek((FILE *) in, (long) offset, whence);
	return (int64_t) ftell((FILE *) in);
}

static int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
	return ((CFfmpegDec *) opaque)->Seek(offset, whence);
}

bool CFfmpegDec::Init(void *_in, const CFile::FileType /* ft */)
{
        title = "";
        artist = "";
        date = "";
        album = "";
        genre = "";
        type_info = "";
	total_time = 0;
	bitrate = 0;

#ifdef FFDEC_DEBUG
	av_log_set_level(AV_LOG_DEBUG);
#endif

	AVIOContext *avioc = NULL;
	in = _in;
	is_stream = fseek((FILE *)in, 0, SEEK_SET);
	buffer = (unsigned char *) av_malloc(buffer_size);
	if (!buffer)
		return false;
	avc = avformat_alloc_context();
	if (!avc) {
		av_freep(&buffer);
		return false;
	}

	if (is_stream)
		avc->probesize = 128 * 1024;

	av_opt_set_int(avc, "analyzeduration", 1000000, 0);

	avioc = avio_alloc_context (buffer, buffer_size, 0, this, read_packet, NULL, seek_packet);
	if (!avioc) {
		av_freep(&buffer);
		avformat_free_context(avc);
		return false;
	}
	avc->pb = avioc;
	avc->flags |= AVFMT_FLAG_CUSTOM_IO|AVFMT_FLAG_KEEP_SIDE_DATA;

	AVInputFormat *input_format = NULL;

#if 0
	switch (ft) {
	case CFile::FILE_OGG:
		input_format = av_find_input_format("ogg");
		break;
	case CFile::FILE_MP3:
		input_format = av_find_input_format("mp3");
		break;
	case CFile::FILE_WAV:
		input_format = av_find_input_format("wav");
		break;
	case CFile::FILE_FLAC:
		input_format = av_find_input_format("flac");
		break;
	case CFile::FILE_AAC:
		input_format = av_find_input_format("aac");
		break;
	default:
		break;
	}
#endif

	int r = avformat_open_input(&avc, "", input_format, NULL);
	if (r) {
		char buf[200]; av_strerror(r, buf, sizeof(buf));
		fprintf(stderr, "%d %s %d: %s\n", __LINE__, __func__,r,buf);
		if (avioc)
			av_freep(avioc);
		if (avc) {
			avformat_close_input(&avc);
			avformat_free_context(avc);
			avc = NULL;
		}
		return false;
	}
	return true;
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
//	if (buffer)
//		av_freep(&buffer);
	in = NULL;
}

CBaseDec::RetCode CFfmpegDec::Decoder(FILE *_in, int /*OutputFd*/, State* state, CAudioMetaData* _meta_data, time_t* time_played, unsigned int* secondsToSkip)
{
	in = _in;
	RetCode Status=OK;
	is_stream = fseek((FILE *)in, 0, SEEK_SET);

	if (!SetMetaData((FILE *)in, _meta_data, true)) {
		DeInit();
		Status=DATA_ERR;
		return Status;
	}

	AVCodecContext *c = avc->streams[best_stream]->codec;

	mutex.lock();
	int r = avcodec_open2(c, codec, NULL);
	mutex.unlock();
	if (r)
	{
		DeInit();
		Status=DATA_ERR;
		return Status;
	}

	SwrContext *swr = swr_alloc();
	if (!swr) {
		mutex.lock();
		avcodec_close(c);
		mutex.unlock();
		DeInit();
		Status=DATA_ERR;
		return Status;
	}

	mSampleRate = samplerate;
	mChannels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
#if __BYTE_ORDER == __LITTLE_ENDIAN
	audioDecoder->PrepareClipPlay(mChannels, mSampleRate, 16, 1);
#else
	audioDecoder->PrepareClipPlay(mChannels, mSampleRate, 16, 0);
#endif

	AVFrame *frame = NULL;
	AVPacket rpacket;
	av_init_packet(&rpacket);
	c->channel_layout = c->channel_layout ? c->channel_layout : AV_CH_LAYOUT_STEREO;

	av_opt_set_int(swr, "in_channel_layout",	c->channel_layout,	0);
	//av_opt_set_int(swr, "out_channel_layout",	c->channel_layout,	0);
	av_opt_set_int(swr, "out_channel_layout",	AV_CH_LAYOUT_STEREO,	0);
	av_opt_set_int(swr, "in_sample_rate",		c->sample_rate,		0);
	av_opt_set_int(swr, "out_sample_rate",		c->sample_rate,		0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt",	c->sample_fmt,          0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt",   	AV_SAMPLE_FMT_S16,      0);

	if (( swr_init(swr)) < 0) {
		Status=DATA_ERR;
		return Status;
	}

	uint8_t *outbuf = NULL;
	int outsamples = 0;
	int outsamples_max = 0;

	int64_t pts = 0, start_pts = 0, next_skip_pts = 0;
	uint64_t skip = 0;
	int seek_flags = 0;

	do
	{
		int actSecsToSkip = *secondsToSkip;
		if (!is_stream && (actSecsToSkip || *state==FF || *state==REV) && avc->streams[best_stream]->time_base.num) {
			if (!next_skip_pts || pts >= next_skip_pts) {
				skip = avc->streams[best_stream]->time_base.den / avc->streams[best_stream]->time_base.num;
				if (actSecsToSkip)
					skip *= actSecsToSkip;
				if (*state == REV) {
					next_skip_pts = pts - skip;
					pts = next_skip_pts - skip/4;
					seek_flags = AVSEEK_FLAG_BACKWARD;
					if (pts < start_pts) {
						pts = start_pts;
						*state = PAUSE; 
					}
				} else {
					pts += skip;
					next_skip_pts = pts + skip/4;
					seek_flags = 0;
				}
				av_seek_frame(avc, best_stream, pts, seek_flags);
				skip = 0;
				// if a custom value was set we only jump once
				if (actSecsToSkip != 0) {
					*state=PLAY;
					*secondsToSkip = 0;
				}
			}
		}

		while(*state==PAUSE && !is_stream)
			usleep(10000);

		if (av_read_frame(avc, &rpacket)) {
			Status=DATA_ERR;
			break;
		}

		if (rpacket.stream_index != best_stream) {
			av_free_packet(&rpacket);
			continue;
		}

		AVPacket packet = rpacket;
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
				mutex.lock();
				avcodec_close(c);
				avcodec_open2(c, codec, NULL);
				mutex.unlock();
				continue;
			}
			if (got_frame && *state!=PAUSE) {
				int out_samples;
				outsamples = av_rescale_rnd(swr_get_delay(swr, c->sample_rate) + frame->nb_samples,
					c->sample_rate, c->sample_rate, AV_ROUND_UP);
				if (outsamples > outsamples_max) {
					av_free(outbuf);
					if (av_samples_alloc(&outbuf, &out_samples, mChannels, //c->channels,
								frame->nb_samples, AV_SAMPLE_FMT_S16, 1) < 0) {
						Status=WRITE_ERR;
						packet.size = 0;
						break;
					}
					outsamples_max = outsamples;
				}
				outsamples = swr_convert(swr, &outbuf, outsamples,
							(const uint8_t **) &frame->data[0], frame->nb_samples);
				int outbuf_size = av_samples_get_buffer_size(&out_samples, mChannels, //c->channels,
									  outsamples, AV_SAMPLE_FMT_S16, 1);

				if(audioDecoder->WriteClip((unsigned char*) outbuf, outbuf_size) != outbuf_size)
				{
					fprintf(stderr,"%s: PCM write error (%s).\n", ProgName, strerror(errno));
					Status=WRITE_ERR;
				}
				pts = av_frame_get_best_effort_timestamp(frame);
				if (!start_pts)
					start_pts = pts;
			}
			packet.size -= len;
			packet.data += len;
		}
		if (time_played && avc->streams[best_stream]->time_base.den)
			*time_played = (pts - start_pts) * avc->streams[best_stream]->time_base.num / avc->streams[best_stream]->time_base.den;
		av_free_packet(&rpacket);
	} while (*state!=STOP_REQ && Status==OK);

	audioDecoder->StopClip();
	meta_data_valid = false;

	swr_free(&swr);
	av_free(outbuf);
	av_free_packet(&rpacket);
	avcodec_free_frame(&frame);
	avcodec_close(c);
	//av_free(avcc);

	DeInit();
	if (_meta_data->cover_temporary && !_meta_data->cover.empty()) {
		_meta_data->cover_temporary = false;
		unlink(_meta_data->cover.c_str());
	}
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

bool CFfmpegDec::SetMetaData(FILE *_in, CAudioMetaData* m, bool save_cover)
{
	if (!meta_data_valid)
	{
		if (!Init(_in, (const CFile::FileType) m->type))
			return false;

		mutex.lock();
		int ret = avformat_find_stream_info(avc, NULL);
		if (ret < 0) {
			mutex.unlock();
			DeInit();
			printf("avformat_find_stream_info error %d\n", ret);
			return false;
		}
		mutex.unlock();
		if (!is_stream) {
			GetMeta(avc->metadata);
			for(unsigned int i = 0; i < avc->nb_streams; i++) {
				if (avc->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
					GetMeta(avc->streams[i]->metadata);
			}
		}

		//fseek((FILE *) in, 0, SEEK_SET);
#ifdef FFDEC_DEBUG
		av_dump_format(avc, 0, "", 0);
#endif

		codec = NULL;
		best_stream = av_find_best_stream(avc, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

		if (best_stream < 0) {
			DeInit();
			return false;
		}

		if (!codec)
			codec = avcodec_find_decoder(avc->streams[best_stream]->codec->codec_id);
		samplerate = avc->streams[best_stream]->codec->sample_rate;
		mChannels = av_get_channel_layout_nb_channels(avc->streams[best_stream]->codec->channel_layout);

		std::stringstream ss;

		if (codec && codec->long_name != NULL)
			type_info = codec->long_name;
		else if(codec && codec->name != NULL)
			type_info = codec->name;
		else
			type_info = "unknown";
		ss << " / " << mChannels << " channel" << ( mChannels > 1 ? "s" : "");
		type_info += ss.str();

		bitrate = 0;
		total_time = 0;

		if (avc->duration != int64_t(AV_NOPTS_VALUE))
			total_time = avc->duration / int64_t(AV_TIME_BASE);
		printf("CFfmpegDec: format %s (%s) duration %ld\n", avc->iformat->name, type_info.c_str(), total_time);

		for(unsigned int i = 0; i < avc->nb_streams; i++) {
			if (avc->streams[i]->codec->bit_rate > 0)
				bitrate += avc->streams[i]->codec->bit_rate;
			if (save_cover && (avc->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
				mkdir(COVERDIR, 0755);
				std::string cover(COVERDIR);
				cover += "/" + to_string(cover_count++) + ".jpg";
				FILE *f = fopen(cover.c_str(), "wb");
				if (f) {
					AVPacket *pkt = &avc->streams[i]->attached_pic;
					fwrite(pkt->data, pkt->size, 1, f);
					fclose(f);
					m->cover = cover;
					m->cover_temporary = true;
				}
			}
		}
		if(!total_time && m->filesize && bitrate)
			total_time = 8 * m->filesize / bitrate;

		meta_data_valid = true;
		m->changed = true;
	}
	if (!is_stream) {
		m->title = title;
		m->artist = artist;
		m->date = date;
		m->album = album;
		m->genre = genre;
		m->total_time = total_time;
	}
	m->type_info = type_info;
	// make sure bitrate is set to prevent refresh metadata from gui, its a flag
	m->bitrate = bitrate ? bitrate : 1; 
	m->samplerate = samplerate;

	return true;
}

void CFfmpegDec::GetMeta(AVDictionary * metadata)
{
	AVDictionaryEntry *tag = NULL;
	while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
		if(!strcasecmp(tag->key,"Title")) {
			if (title.empty()) {
				title = isUTF8(tag->value) ? tag->value : convertLatin1UTF8(tag->value);
				title = trim(title);
			}
			continue;
		}
		if(!strcasecmp(tag->key,"Artist")) {
			if (artist.empty()) {
				artist = isUTF8(tag->value) ? tag->value : convertLatin1UTF8(tag->value);
				artist = trim(artist);
			}
			continue;
		}
		if(!strcasecmp(tag->key,"Year")) {
			if (date.empty()) {
				date = isUTF8(tag->value) ? tag->value : convertLatin1UTF8(tag->value);
				date = trim(date);
			}
			continue;
		}
		if(!strcasecmp(tag->key,"Album")) {
			if (album.empty()) {
				album = isUTF8(tag->value) ? tag->value : convertLatin1UTF8(tag->value);
				album = trim(album);
			}
			continue;
		}
		if(!strcasecmp(tag->key,"Genre")) {
			if (genre.empty()) {
				genre = isUTF8(tag->value) ? tag->value : convertLatin1UTF8(tag->value);
				genre = trim(genre);
			}
			continue;
		}
	}
}
