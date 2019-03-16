/*
	Neutrino-GUI  -   DBoxII-Project

	(C) 2004-2009 tuxbox project contributors

	(C) 2011-2012,2015 Stefan Seyfried

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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>

extern "C" {
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
}

#include <OpenThreads/ScopedLock>

#include <gui/streaminfo2.h>

#include <global.h>
#include <neutrino.h>

#include <driver/abstime.h>
#include <driver/fade.h>
#include <driver/display.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <gui/color.h>
#include <gui/color_custom.h>
#include <gui/widget/icons.h>
#include <daemonc/remotecontrol.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>
#include <hardware/video.h>
#include <hardware/audio.h>
#include <hardware/dmx.h>
#include <zapit/satconfig.h>
#include <string>
#include <system/helpers.h>
#include <system/set_threadname.h>

extern cVideo *videoDecoder;
extern cAudio *audioDecoder;

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */

CStreamInfo2::CStreamInfo2() : fader(g_settings.theme.menu_Content_alpha)
{
	frameBuffer = CFrameBuffer::getInstance();
	pip = NULL;
	signalbox = NULL;

	font_head = SNeutrinoSettings::FONT_TYPE_FIXED_30_BOLD;
	font_info = SNeutrinoSettings::FONT_TYPE_FIXED_20_BOLD;
	font_small = SNeutrinoSettings::FONT_TYPE_FIXED_16_REGULAR;

	hheight = g_FixedFont[font_head]->getHeight();
	iheight = g_FixedFont[font_info]->getHeight();
	sheight = g_FixedFont[font_small]->getHeight();

	max_width = frameBuffer->getScreenWidth(true);
	max_height = frameBuffer->getScreenHeight(true);

	width = frameBuffer->getScreenWidth();
	height = frameBuffer->getScreenHeight();
	x = frameBuffer->getScreenX();
	y = frameBuffer->getScreenY();

	sigBox_pos = 0;
	paint_mode = 0;

	b_total = 0;
	bit_s = 0;
	abit_s = 0;

	signal.max_sig = 0;
	signal.max_snr = 0;
	signal.max_ber = 0;

	signal.min_sig = 0;
	signal.min_snr = 0;
	signal.min_ber = 0;

	rate.short_average = 0;
	rate.max_short_average = 0;
	rate.min_short_average = 0;
	box_h = 0;
	box_h2 = 0;
	dmxbuf = NULL;
	probebuf = NULL;
	probe_thread = 0;
}

CStreamInfo2::~CStreamInfo2()
{
	delete pip;
	ts_close();
}

/* based on ffmpeg-2.2.1/libavformat/utils.c:print_fps() */
static std::string fps_str(double d)
{
	uint64_t v = lrintf(d * 100);
	char buf[64];
	if (v % 100)
		snprintf(buf, sizeof(buf), "%3.2f", d);
	else if (v % (100 * 1000))
		snprintf(buf, sizeof(buf), "%1.0f", d);
	else
		snprintf(buf, sizeof(buf), "%1.0fk", d / 1000);

	return std::string(buf);
}

/* based on ffmpeg-2.2.1/libavformat/utils.c:dump_stream_format() */
void CStreamInfo2::analyzeStream(AVFormatContext *avfc, unsigned int idx)
{
	std::map<std::string, std::string> _m;
	streamdata.push_back(_m);
	std::map<std::string, std::string> &m = streamdata.back();

	AVStream *st = avfc->streams[idx];

	m["language"] = " ";
	AVDictionaryEntry *lang = av_dict_get(st->metadata, "language", NULL, 0);
	if (lang)
		m["language"] = getISO639Description(lang->value);
	else
	{
		lang = av_dict_get(st->metadata, "lang", NULL, 0);
		if (lang)
			m["language"] = getISO639Description(lang->value);
	}

#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
	AVCodecContext *avctx;
	int ret;

	avctx = avcodec_alloc_context3(NULL);
	if (!avctx)
		return;

	ret = avcodec_parameters_to_context(avctx, st->codecpar);
	if (ret < 0)
	{
		avcodec_free_context(&avctx);
		return;
	}
#endif

	char buf[256];
#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
	avcodec_string(buf, sizeof(buf), avctx, 0);
	avcodec_free_context(&avctx);
#else
	avcodec_string(buf, sizeof(buf), st->codec, 0);
#endif
	m["codec"] = buf;
	size_t pos = m["codec"].find_first_of(":");
	if (pos != std::string::npos)
		m["codec"] = m["codec"].erase(0, pos + 2);
#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
	m["codec_type"] = av_get_media_type_string(st->codecpar->codec_type);
#else
	m["codec_type"] = av_get_media_type_string(st->codec->codec_type);
#endif
	m["codec_type"][0] ^= 'a' ^ 'A';

	m["pid"] = to_string(st->id);

#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
	if (st->sample_aspect_ratio.num && av_cmp_q(st->sample_aspect_ratio, st->codecpar->sample_aspect_ratio))
#else
	if (st->sample_aspect_ratio.num && av_cmp_q(st->sample_aspect_ratio, st->codec->sample_aspect_ratio))
#endif
	{
		AVRational display_aspect_ratio;
		av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
			  st->codecpar->width * st->sample_aspect_ratio.num,
			  st->codecpar->height * st->sample_aspect_ratio.den,
#else
			  st->codec->width * st->sample_aspect_ratio.num,
			  st->codec->height * st->sample_aspect_ratio.den,
#endif
			  1024 * 1024);
		snprintf(buf, sizeof(buf), "%d:%d", st->sample_aspect_ratio.num, st->sample_aspect_ratio.den);
		m["sar"] = buf;
		snprintf(buf, sizeof(buf), "%d:%d", display_aspect_ratio.num, display_aspect_ratio.den);
		m["dar"] = buf;
	}
#if (LIBAVFORMAT_VERSION_MAJOR > 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR > 32))
	if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
#else
	if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO)
#endif
	{
		if (st->avg_frame_rate.den && st->avg_frame_rate.num)
			m["fps"] = fps_str(av_q2d(st->avg_frame_rate));
#if FF_API_R_FRAME_RATE
		if (st->r_frame_rate.den && st->r_frame_rate.num)
			m["tbf"] = fps_str(av_q2d(st->r_frame_rate));
#endif
		if (st->time_base.den && st->time_base.num)
			m["tbn"] = fps_str(1 / av_q2d(st->time_base));
#if (LIBAVFORMAT_VERSION_MAJOR < 57) || ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR < 33))
		if (st->codec->time_base.den && st->codec->time_base.num)
			m["tbc"] = fps_str(1 / av_q2d(st->codec->time_base));
#endif
	}

	m["disposition"] = "";
	if (st->disposition & AV_DISPOSITION_DEFAULT)
		m["disposition"] += " (default)";
	if (st->disposition & AV_DISPOSITION_DUB)
		m["disposition"] += " (dub)";
	if (st->disposition & AV_DISPOSITION_ORIGINAL)
		m["disposition"] += " (original)";
	if (st->disposition & AV_DISPOSITION_COMMENT)
		m["disposition"] += " (comment)";
	if (st->disposition & AV_DISPOSITION_LYRICS)
		m["disposition"] += " (lyrics)";
	if (st->disposition & AV_DISPOSITION_KARAOKE)
		m["disposition"] += " (karaoke)";
	if (st->disposition & AV_DISPOSITION_FORCED)
		m["disposition"] += " (forced)";
	if (st->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
		m["disposition"] += " (hearing impaired)";
	if (st->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
		m["disposition"] += " (visual impaired)";
	if (st->disposition & AV_DISPOSITION_CLEAN_EFFECTS)
		m["disposition"] += " (clean effects)";
	if (!m["disposition"].empty())
		m["disposition"].erase(0, 1);

	//for (std::map<std::string,std::string>::iterator it = m.begin(); it != m.end(); ++it)
	//	fprintf(stderr, "%s -> %s\n", it->first.c_str(), it->second.c_str());
}

void CStreamInfo2::analyzeStreams(AVFormatContext *avfc)
{
	// av_dump_format(avfc, 0, "", 0);

	streamdata.clear();
	for (unsigned int i = 0; i < avfc->nb_streams; i++)
		analyzeStream(avfc, i);
}

static cDemux *dmx = NULL;

#define TS_LEN		188
#define TS_BUF_SIZE	(1024 * 1024 * 8 / TS_LEN * TS_LEN)

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	CStreamInfo2 *me = (CStreamInfo2 *) opaque;
	return me->readPacket(buf, buf_size);
}

int CStreamInfo2::readPacket(uint8_t *buf, int buf_size)
{
	if (!probebuf)
		return 0;
	while (!abort_probing && (probebuf_length == probebuf_off) && (probebuf_off != probebuf_size))
		usleep(10000);

	if (!abort_probing)
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(probe_mutex);
		if (probebuf)
		{
			int len = std::min(buf_size, (int)(probebuf_length - probebuf_off));
			memcpy(buf, probebuf + probebuf_off, len);
			probebuf_off += len;
			return len;
		}
	}
	return -1;
}

//#define ENABLE_FFMPEG_LOGGING
#ifdef ENABLE_FFMPEG_LOGGING
static void log_callback(void *, int, const char *format, va_list ap)
{
	vfprintf(stderr, format, ap);
}
#endif

static int interrupt_cb(void *arg)
{
	CStreamInfo2 *me = (CStreamInfo2 *) arg;
	return me->abort_probing;
}

void CStreamInfo2::probeStreams()
{
	if (mp)
	{
		// not yet implemented in libcoolstream
		AVFormatContext *avfc = mp->getPlayback()->GetAVFormatContext();
		if (avfc)
		{
			analyzeStreams(avfc);
			mp->getPlayback()->ReleaseAVFormatContext();
		}
		probed = true;
	}
	else
	{
#ifdef ENABLE_FFMPEG_LOGGING
		av_log_set_callback(log_callback);
#endif
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
		avcodec_register_all();
		av_register_all();
#endif
		AVIOContext *avioc = NULL;
		int buffer_size = 188 * 128;
		unsigned char *buffer = (unsigned char *) av_malloc(buffer_size);
		AVFormatContext *avfc = avformat_alloc_context();
		if (!avfc)
		{
			av_freep(&buffer);
			goto bye;
		}

		avfc->interrupt_callback.callback = interrupt_cb;
		avfc->interrupt_callback.opaque = (void *) this;

		avioc = avio_alloc_context(buffer, buffer_size, 0, this, read_packet, NULL, NULL);
		if (!avioc)
		{
			av_freep(&buffer);
			avformat_free_context(avfc);
			goto bye;
		}

		avfc->pb = avioc;
		avfc->flags |= AVFMT_FLAG_CUSTOM_IO;
		avfc->probesize = probebuf_size/2;

		if (!avformat_open_input(&avfc, "", NULL, NULL))
		{
			avformat_find_stream_info(avfc, NULL);
			analyzeStreams(avfc);
		}

		avformat_close_input(&avfc);
		avformat_free_context(avfc);
		probed = true;
bye:
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(probe_mutex);
		if (probebuf)
		{
			delete [] probebuf;
			probebuf = NULL;
		}
	}
}

void *CStreamInfo2::probeStreams(void *arg)
{
	set_threadname(__func__);
	CStreamInfo2 *me = (CStreamInfo2 *) arg;
	me->probeStreams();
	pthread_exit(NULL);
}

int CStreamInfo2::exec(CMenuTarget *parent, const std::string &)
{

	if (parent)
		parent->hide();

	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv || CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio)
		mp = &CMoviePlayerGui::getInstance(true);
	else
		mp = &CMoviePlayerGui::getInstance();

	if (!mp->Playing())
		mp = NULL;

	frontend = mp ? NULL : CFEManager::getInstance()->getLiveFE();

	fader.StartFadeIn();
	paint(paint_mode);
	int res = doSignalStrengthLoop();
	hide();
	fader.StopFade();
	return res;
}

int CStreamInfo2::doSignalStrengthLoop()
{
	int res = menu_return::RETURN_REPAINT;

	bool fadeout = false;
	neutrino_msg_t msg;
	neutrino_msg_t postmsg = 0;
	uint64_t maxb, minb, lastb, tmp_rate;
	unsigned int current_pmt_version = pmt_version;
	int cnt = 0;
	char tmp_str[150];
	int delay_counter = 0;
	const int delay = 15;
	int sw = 2*g_FixedFont[font_info]->getRenderWidth(".") + 8*g_FixedFont[font_info]->getMaxDigitWidth();
	maxb = minb = lastb = tmp_rate = 0;
	bool repaint_bitrate = true;
	ts_setup();
	frameBuffer->blit();
	while (1)
	{
		neutrino_msg_data_t data;

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS(10);
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if (!mp)
		{
			signal.sig = 100 * (frontend->getSignalStrength() & 0xFFFF) >> 16;
			signal.snr = 100 * (frontend->getSignalNoiseRatio() & 0xFFFF) >> 16;
			signal.ber = 100 * (frontend->getBitErrorRate() & 0xFFFF) >> 16; // FIXME?
		}

		bool got_rate = update_rate();

		if (got_rate && (lastb != bit_s))
		{
			if (maxb < bit_s)
				rate.max_short_average = maxb = bit_s;
			if ((cnt > 10) && ((minb == 0) || (minb > bit_s)))
				rate.min_short_average = minb = bit_s;
		}

		if (paint_mode == 0)
		{
			if (cnt < 12)
				cnt++;
			int dx1 = x + OFFSET_INNER_MID;

			if (!mp && delay_counter > delay + 1)
			{
				CZapitChannel *channel = CZapit::getInstance()->GetCurrentChannel();
				if (channel)
					pmt_version = channel->getPmtVersion();
				if (pmt_version != current_pmt_version)
					delay_counter = 0;
			}
			if (got_rate && (rate.short_average || lastb) && (lastb != bit_s))
			{
				if (repaint_bitrate)
				{
					snprintf(tmp_str, sizeof(tmp_str), "%s:", g_Locale->getText(LOCALE_STREAMINFO_BITRATE));
					g_FixedFont[font_info]->RenderString(dx1, average_bitrate_pos, spaceoffset, tmp_str, COL_MENUCONTENT_TEXT);

					snprintf(tmp_str, sizeof(tmp_str), " (%s)", g_Locale->getText(LOCALE_STREAMINFO_AVERAGE_BITRATE));
					g_FixedFont[font_info]->RenderString(dx1 + spaceoffset + sw , average_bitrate_pos, box_width - spaceoffset - sw, tmp_str, COL_MENUCONTENT_TEXT);
					repaint_bitrate = false;
				}

				frameBuffer->paintBoxRel(dx1 + spaceoffset, average_bitrate_pos - iheight, sw, iheight, COL_MENUCONTENT_PLUS_0);
				char currate[140];
				snprintf(currate, sizeof(currate), "%u.%03u.%03u", (rate.short_average / 1000 / 1000) % 1000, (rate.short_average / 1000) % 1000, rate.short_average % 1000);
				g_FixedFont[font_info]->RenderString(dx1 + spaceoffset, average_bitrate_pos, sw, currate, COL_MENUCONTENT_TEXT);
				lastb = bit_s;
			}
			if ((!mp && pmt_version != current_pmt_version && delay_counter > delay) || probed)
			{
				probed = false;
				current_pmt_version = pmt_version;
				paint_techinfo(techinfo_xpos, techinfo_ypos);
				repaint_bitrate = true;
				continue;
			}
			if (!mp)
				showSNR();

			delay_counter++;
		}
		rate.short_average = abit_s;

		if (signal.max_ber < signal.ber)
			signal.max_ber = signal.ber;

		if (signal.max_sig < signal.sig)
			signal.max_sig = signal.sig;

		if (signal.max_snr < signal.snr)
			signal.max_snr = signal.snr;

		if ((signal.min_ber == 0) || (signal.min_ber > signal.ber))
			signal.min_ber = signal.ber;

		if ((signal.min_sig == 0) || (signal.min_sig > signal.sig))
			signal.min_sig = signal.sig;

		if ((signal.min_snr == 0) || (signal.min_snr > signal.snr))
			signal.min_snr = signal.snr;

		if (got_rate)
		{
			paint_signal_fe(rate, signal);
			signal.old_sig = signal.sig;
			signal.old_snr = signal.snr;
			signal.old_ber = signal.ber;
		}

		g_RCInput->getMsg_us(&msg, &data, 0);

		if ((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer()))
		{
			if (fader.FadeDone())
			{
				break;
			}
			continue;
		}
		if (fadeout && msg == CRCInput::RC_timeout)
		{
			if (fader.StartFadeOut())
			{
				msg = 0;
				continue;
			}
			else
			{
				break;
			}
		}

		// switch paint mode
		if (msg == CRCInput::RC_red || msg == CRCInput::RC_blue || msg == CRCInput::RC_green || msg == CRCInput::RC_yellow)
		{
			hide();
			paint_mode = !paint_mode;
			paint(paint_mode);
			repaint_bitrate = true;
			continue;
		}
		else if (msg == CRCInput::RC_setup || msg == CRCInput::RC_home)
		{
			res = menu_return::RETURN_EXIT_ALL;
			fadeout = true;
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			postmsg = msg;
			res = menu_return::RETURN_EXIT_ALL;
			fadeout = true;
		}
		else if (msg == (neutrino_msg_t) g_settings.key_screenshot)
		{
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
			continue;
		}

		// -- any key --> abort
		if (msg <= CRCInput::RC_MaxRC)
			fadeout = true;

		// -- push other events
		if (msg > CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout)
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		frameBuffer->blit();
	}
	delete signalbox;
	signalbox = NULL;
	ts_close();

	if (postmsg)
	{
		g_RCInput->postMsg(postmsg, 0);
	}

	return res;
}

void CStreamInfo2::hide()
{
	pip->hide();
	frameBuffer->paintBackgroundBoxRel(0, 0, max_width, max_height);
	frameBuffer->blit();
}

void CStreamInfo2::paint_signal_fe_box(int _x, int _y, int w, int h)
{
	std::string tname(g_Locale->getText(LOCALE_STREAMINFO_SIGNAL));
	tname += ": ";
	if (mp)
	{
		if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv || CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio)
			tname += "Web-Channel"; // TODO split into WebTV/WebRadio
		else
			tname += g_Locale->getText(LOCALE_MAINMENU_MOVIEPLAYER);
	}
	else
		tname += to_string(1 + frontend->getNumber()) + ": " + frontend->getName();

	g_FixedFont[font_small]->RenderString(_x, _y + sheight, w, tname, COL_MENUCONTENT_TEXT);

	sigBox_x = _x;
	sigBox_y = _y + sheight + OFFSET_INNER_SMALL;
	sigBox_w = w;
	sigBox_h = h - sheight - OFFSET_INNER_SMALL;
	frameBuffer->paintBoxRel(sigBox_x, sigBox_y, sigBox_w, sigBox_h, COL_BLACK);

	sig_text_y = _y + h + OFFSET_INNER_MID;
	sig_text_w = w/5;

	int y1 = sig_text_y + sheight;
	int fw = g_FixedFont[font_small]->getWidth();

	int col = 1;
	std::string str;

	if (!mp)
	{
		sig_text_ber_x = _x + sig_text_w*col;
		SignalRenderHead("BER [%]", sig_text_ber_x, y1, COL_RED);
		col++;

		sig_text_snr_x = _x + sig_text_w*col;
		SignalRenderHead("SNR [%]", sig_text_snr_x, y1, COL_LIGHT_BLUE);
		col++;

		sig_text_sig_x = _x + sig_text_w*col;
		SignalRenderHead("SIG [%]", sig_text_sig_x, y1, COL_GREEN);
		col++;
	}
	else
		col = 4;

	sig_text_rate_x = _x + sig_text_w*col;
	SignalRenderHead("BR [kbps]", sig_text_rate_x, y1, COL_YELLOW);

	y1 += OFFSET_INNER_SMALL;

	g_FixedFont[font_small]->RenderString(_x, y1 + (sheight*1), fw*3, "max", COL_MENUCONTENT_TEXT);
	g_FixedFont[font_small]->RenderString(_x, y1 + (sheight*2), fw*3, "now", COL_MENUCONTENT_TEXT);
	g_FixedFont[font_small]->RenderString(_x, y1 + (sheight*3), fw*3, "min", COL_MENUCONTENT_TEXT);

	sigBox_pos = 0;

	signal.old_sig = 1;
	signal.old_snr = 1;
	signal.old_ber = 1;
}

void CStreamInfo2::paint_signal_fe(struct bitrate br, struct feSignal s)
{
	int x_now = sigBox_pos;
	int yt = sig_text_y + OFFSET_INNER_SMALL + (sheight*2);
	int yd;
	static int old_x = 0, old_y = 0;
	sigBox_pos++;
	sigBox_pos %= sigBox_w;

	frameBuffer->paintVLine(sigBox_x + sigBox_pos, sigBox_y, sigBox_y + sigBox_h, COL_WHITE);
	frameBuffer->paintVLine(sigBox_x + x_now, sigBox_y, sigBox_y + sigBox_h + 1, COL_BLACK);

	long value = (long)(bit_s / 1000ULL);

	SignalRenderStr(value, sig_text_rate_x, yt + sheight);
	SignalRenderStr(br.max_short_average / 1000ULL, sig_text_rate_x, yt);
	SignalRenderStr(br.min_short_average / 1000ULL, sig_text_rate_x, yt + (sheight * 2));
	if (mp || g_RemoteControl->current_PIDs.PIDs.vpid > 0)
	{
		yd = y_signal_fe(value, scaling, sigBox_h); // Video + Audio
	}
	else
	{
		yd = y_signal_fe(value, 512, sigBox_h);  // Audio only
	}
	if ((old_x == 0 && old_y == 0) || sigBox_pos == 1)
	{
		old_x = sigBox_x + x_now;
		old_y = sigBox_y + sigBox_h - yd;
	}
	else
	{
		frameBuffer->paintLine(old_x, old_y, sigBox_x + x_now, sigBox_y + sigBox_h - yd, COL_YELLOW); //yellow
		old_x = sigBox_x + x_now;
		old_y = sigBox_y + sigBox_h - yd;
	}

	if (!mp)
	{
		if (s.ber != s.old_ber)
		{
			SignalRenderStr(s.ber,     sig_text_ber_x, yt + sheight);
			SignalRenderStr(s.max_ber, sig_text_ber_x, yt);
			SignalRenderStr(s.min_ber, sig_text_ber_x, yt + (sheight * 2));
		}
		yd = y_signal_fe(s.ber, 100, sigBox_h);
		frameBuffer->paintPixel(sigBox_x + x_now, sigBox_y + sigBox_h - yd, COL_RED);

		if (s.sig != s.old_sig)
		{
			SignalRenderStr(s.sig,     sig_text_sig_x, yt + sheight);
			SignalRenderStr(s.max_sig, sig_text_sig_x, yt);
			SignalRenderStr(s.min_sig, sig_text_sig_x, yt + (sheight * 2));
		}
		yd = y_signal_fe(s.sig, 100, sigBox_h);
		frameBuffer->paintPixel(sigBox_x + x_now, sigBox_y + sigBox_h - yd, COL_GREEN);

		if (s.snr != s.old_snr)
		{
			SignalRenderStr(s.snr,     sig_text_snr_x, yt + sheight);
			SignalRenderStr(s.max_snr, sig_text_snr_x, yt);
			SignalRenderStr(s.min_snr, sig_text_snr_x, yt + (sheight * 2));
		}
		yd = y_signal_fe(s.snr, 100, sigBox_h);
		frameBuffer->paintPixel(sigBox_x + x_now, sigBox_y + sigBox_h - yd, COL_LIGHT_BLUE);
	}
}

// -- calc y from max_range and max_y
int CStreamInfo2::y_signal_fe(unsigned long value, unsigned long max_value, int max_y)
{
	unsigned long long m;
	unsigned long l;

	if (!max_value)
		max_value = 1;

	// we use a 64 bits int here to detect integer overflow
	// and if it overflows, just return max_y
	m = (unsigned long long)value * max_y;
	if (m > 0xffffffff)
		return max_y;

	l = m / max_value;
	if (l > (unsigned long)max_y)
		l = max_y;

	return (int) l;
}

void CStreamInfo2::SignalRenderHead(std::string head, int _x, int _y, fb_pixel_t color)
{
	frameBuffer->paintBoxRel(_x, _y - sheight, sig_text_w, sheight, COL_MENUCONTENT_PLUS_0);
	int tw = g_FixedFont[font_small]->getRenderWidth(head);
	g_FixedFont[font_small]->RenderString(_x + (sig_text_w - tw)/2, _y, tw, head, color);
}

void CStreamInfo2::SignalRenderStr(unsigned int value, int _x, int _y)
{
	frameBuffer->paintBoxRel(_x, _y - sheight, sig_text_w, sheight, COL_MENUCONTENT_PLUS_0);
	std::string val = to_string(value);
	int tw = g_FixedFont[font_small]->getRenderWidth(val);
	g_FixedFont[font_small]->RenderString(_x + (sig_text_w - tw)/2, _y, tw, val, COL_MENUCONTENT_TEXT);
}

void CStreamInfo2::paint(int /*mode*/)
{
	const char *head_string;

	width = frameBuffer->getScreenWidth();
	height = frameBuffer->getScreenHeight();
	x = frameBuffer->getScreenX();
	y = frameBuffer->getScreenY();
	int ypos = y + OFFSET_INNER_SMALL;
	int xpos = x + OFFSET_INNER_MID;

	// paint backround
	frameBuffer->paintBoxRel(0, 0, max_width, max_height, COL_MENUCONTENT_PLUS_0);

	if (paint_mode == 0)
	{
		// tech infos, pip, signal graph
		head_string = g_Locale->getText(LOCALE_STREAMINFO_HEAD);
		CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, head_string);

		g_FixedFont[font_head]->RenderString(xpos, ypos + hheight, width, head_string, COL_MENUHEAD_TEXT);

		if (pip == NULL)
			pip = new CComponentsPIP(width - width/3 - OFFSET_INNER_MID, ypos, 33);
		pip->paint(CC_SAVE_SCREEN_NO);

		paint_signal_fe_box(pip->getXPos(), pip->getYPos() + pip->getHeight() + OFFSET_INNER_MID, pip->getWidth(), pip->getHeight());

		ypos += hheight + iheight;

		techinfo_xpos = xpos;
		techinfo_ypos = ypos;

		paint_techinfo(xpos, ypos);
	}
	else
	{
		delete signalbox;
		signalbox = NULL;

		// large signal graph
		paint_signal_fe_box(x, y, width, height - OFFSET_INNER_SMALL - 4*iheight);
	}
}

struct row
{
	std::string key;
	std::string val;
	Font *f;
	int col;

	row():
		f(g_FixedFont[SNeutrinoSettings::FONT_TYPE_FIXED_20_BOLD]),
		col(COL_MENUCONTENT_TEXT)
	{
	}
};

void CStreamInfo2::paint_techinfo(int xpos, int ypos)
{
	char buf[100];
	bool has_vpid = false;
	bool is_webchan = false;
	int xres = 0, yres = 0, aspectRatio = 0, framerate = -1, i = 0;
	// paint labels
	int ypos1 = ypos;
	box_width = width/3*2 - 2*OFFSET_INNER_MID - xpos;

	yypos = ypos;
	if (box_h > 0)
		frameBuffer->paintBoxRel(0, ypos, box_width, box_h, COL_MENUCONTENT_PLUS_0);

	CZapitChannel *channel = CZapit::getInstance()->GetCurrentChannel();
	if (!channel && !mp)
		return;

	ypos += iheight;

	std::vector<row> v;
	row r;

	if (mp)
	{
		if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webtv || CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_webradio)
		{
			// channel
			r.key = g_Locale->getText(LOCALE_TIMERLIST_CHANNEL);
			r.key += ": ";
			r.val = channel->getName().c_str();
			if (channel->getName() != channel->getRealname())
			{
				r.val += " << ";
				r.val += channel->getRealname().c_str();
			}
			v.push_back(r);

			// url
			r.key = "URL";
			r.key += ": ";
			r.val = channel->getUrl();
			v.push_back(r);

			// provider
			if (channel->pname)
			{
				std::string prov_name = channel->pname;
				prov_name.erase(std::remove(prov_name.begin(), prov_name.end(), '['), prov_name.end());
				prov_name.erase(std::remove(prov_name.begin(), prov_name.end(), ']'), prov_name.end());
				r.key = g_Locale->getText(LOCALE_CHANNELLIST_PROVS);
				r.key += ": ";
				r.val = prov_name.c_str();
				v.push_back(r);
			}
		}
		else
		{
			// file
			r.key = g_Locale->getText(LOCALE_MOVIEBROWSER_INFO_FILE);
			r.key += ": ";
			r.val = mp->GetFile();
			v.push_back(r);
		}

		scaling = 8000;
	}
	else
	{
		// channel
		r.key = g_Locale->getText(LOCALE_TIMERLIST_CHANNEL);
		r.key += ": ";
		r.val = channel->getName().c_str();
		if (channel->getName() != channel->getRealname())
		{
			r.val += " << ";
			r.val += channel->getRealname().c_str();
		}
		v.push_back(r);

		// provider
		if (channel->pname)
		{
			std::string prov_name = channel->pname;
			size_t pos = prov_name.find_first_of("]");
			if ((pos != std::string::npos) && (pos + 2 < prov_name.length()))
				prov_name = prov_name.substr(pos + 2);
			r.key = g_Locale->getText(LOCALE_CHANNELLIST_PROVS);
			r.key += ": ";
			r.val = prov_name.c_str();
			v.push_back(r);
		}

		transponder t;
		t = *frontend->getParameters();

		if (CFrontend::isSat(t.feparams.delsys))
			r.key = g_Locale->getText(LOCALE_SATSETUP_SATELLITE);
		else if (CFrontend::isCable(t.feparams.delsys))
			r.key = g_Locale->getText(LOCALE_CHANNELLIST_PROVS);
		else if (CFrontend::isTerr(t.feparams.delsys))
			r.key = g_Locale->getText(LOCALE_TERRESTRIALSETUP_AREA);
		else
			r.key = "Unknown:";

		r.key += ": ";
		r.val = CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition()).c_str();
		v.push_back(r);

		// ts frequency
		scaling = 27000;
		if (CFrontend::isSat(t.feparams.delsys) && t.feparams.delsys == DVB_S)
			scaling = 15000;
		r.key = g_Locale->getText(LOCALE_SCANTS_FREQDATA);
		r.val = t.description();
		v.push_back(r);
	}

	// empty line
	r.key = r.val = "";
	v.push_back(r);

	if (channel)
	{
		has_vpid   = channel->getVideoPid();
		is_webchan = IS_WEBCHAN(channel->getChannelID());
	}
	int _mode = CNeutrinoApp::getInstance()->getMode();
	if ((has_vpid ||
		(is_webchan && _mode == NeutrinoModes::mode_webtv) ||
		_mode == NeutrinoModes::mode_ts) &&
		!(videoDecoder->getBlank()))
	{
		videoDecoder->getPictureInfo(xres, yres, framerate);
		if (yres == 1088)
			yres = 1080;
		aspectRatio = videoDecoder->getAspectRatio();
	}

	// video resolution
	r.key = g_Locale->getText(LOCALE_STREAMINFO_RESOLUTION);
	r.key += ": ";
	snprintf(buf, sizeof(buf), "%dx%d", xres, yres);
	r.val = buf;
	v.push_back(r);

	std::string tmp_fps = "";
	std::string tmp_ar = "";
	for (std::vector<std::map<std::string, std::string> >::iterator it = streamdata.begin(); it != streamdata.end(); ++it)
	{
		if ((*it)["codec_type"].substr(0, 5) == "Video")
		{
			tmp_ar = (*it)["dar"];
			if (!tmp_ar.empty())
				tmp_ar += " / ";
			tmp_ar += (*it)["sar"];
			tmp_fps = (*it)["fps"];
		}
	}

	// aspect ratio
	r.key = g_Locale->getText(LOCALE_STREAMINFO_ARATIO);
	r.key += ": ";
	if (aspectRatio < 0 || aspectRatio > 4)
	{
		r.val = tmp_ar;
		if (r.val.empty())
			r.val = g_Locale->getText(LOCALE_STREAMINFO_ARATIO_UNKNOWN);
	}
	else
	{
		const char *arr[] = { "N/A", "4:3", "14:9", "16:9", "20:9" };
		r.val = arr[aspectRatio];
	}
	v.push_back(r);

	// video framerate
	r.key = g_Locale->getText(LOCALE_STREAMINFO_FRAMERATE);
	r.key += ": ";
	if (framerate < 0 || framerate > 7)
	{
		r.val = tmp_fps;
		if (r.val.empty())
			r.val = g_Locale->getText(LOCALE_STREAMINFO_FRAMERATE_UNKNOWN);
		else
			r.val += "fps";
	}
	else
	{
		const char *arr[] = { "23.976fps", "24fps", "25fps", "29,97fps", "30fps", "50fps", "59,94fps", "60fps" };
		r.val = arr[framerate];
	}
	v.push_back(r);

	// place for average bitrate
	average_bitrate_pos = ypos + iheight * v.size();
	r.key = r.val = "";
	v.push_back(r);

	if (mp)
	{
		std::string details(" ");
		for (std::vector<std::map<std::string, std::string> >::iterator it = streamdata.begin(); it != streamdata.end(); ++it)
		{
			if ((*it)["codec_type"].substr(0, 5) == "Video")
			{
				details = (*it)["language"];
				if (details != " ")
					details += ", ";
				details += (*it)["codec"];
				if (details == " ")
					details.clear();
				r.key = (*it)["codec_type"];
				r.key += ": ";
				r.val = details.c_str();
				v.push_back(r);
			}
		}
	}

	// audio
	r.key = g_Locale->getText(LOCALE_STREAMINFO_AUDIOTYPE);
	r.key += ": ";
	std::string desc = "N/A";
	if (!mp && !g_RemoteControl->current_PIDs.APIDs.empty())
		desc = g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].desc;
	r.val = mp ? mp->getAPIDDesc(mp->getAPID()).c_str() : desc.c_str();
	v.push_back(r);

	if (mp)
	{
		std::string details(" ");
		for (std::vector<std::map<std::string, std::string> >::iterator it = streamdata.begin(); it != streamdata.end(); ++it)
		{
			if ((*it)["codec_type"].substr(0, 5) == "Audio")
			{
				details = (*it)["language"];
				if (details != " ")
					details += ", ";
				details += (*it)["codec"];
				if (details == " ")
					details.clear();
				r.key = (*it)["codec_type"];
				r.key += ": ";
				r.val = details.c_str();
				v.push_back(r);
			}
		}
	}

	// empty line
	r.key = r.val = "";
	v.push_back(r);

	// osd resolution
	r.key = g_Locale->getText(LOCALE_STREAMINFO_OSD_RESOLUTION);
	r.key += ": ";
	snprintf(buf, sizeof(buf), "%dx%d", frameBuffer->getScreenWidth(true), frameBuffer->getScreenHeight(true));
	r.val = buf;
	r.f   = g_FixedFont[font_small];
	v.push_back(r);

	// channellogo
	if (CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_ts)
	{
		r.key = "Logo";
		r.key += ": ";
		snprintf(buf, sizeof(buf), "%llx.png", channel->getChannelID() & 0xFFFFFFFFFFFFULL);
		r.val = buf;
		v.push_back(r);
	}

	// empty line
	r.key = r.val = "";
	r.f   = g_FixedFont[font_info];
	v.push_back(r);

	if (!mp)
	{
		// onid
		r.key = "ONID: ";
		i = channel->getOriginalNetworkId();
		snprintf(buf, sizeof(buf), "0x%04X (%i)", i, i);
		r.val = buf;
		r.f   = g_FixedFont[font_small];
		v.push_back(r);

		// tsid
		r.key = "TSID: ";
		i = channel->getTransportStreamId();
		snprintf(buf, sizeof(buf), "0x%04X (%i)", i, i);
		r.val = buf;
		r.f   = g_FixedFont[font_small];
		v.push_back(r);

		// sid
		r.key = "SID: ";
		i = channel->getServiceId();
		snprintf(buf, sizeof(buf), "0x%04X (%i)", i, i);
		r.val = buf;
		r.f   = g_FixedFont[font_small];
		v.push_back(r);

		// pmtpid
		r.key = "PMT PID: ";
		i = channel->getPmtPid();
		pmt_version = channel->getPmtVersion();
		snprintf(buf, sizeof(buf), "0x%04X (%i) [0x%02X]", i, i, pmt_version);
		r.val = buf;
		r.f   = g_FixedFont[font_small];
		v.push_back(r);

		//vtxtpid
		if (g_RemoteControl->current_PIDs.PIDs.vtxtpid)
		{
			r.key = "VT PID: ";
			i = g_RemoteControl->current_PIDs.PIDs.vtxtpid;
			snprintf(buf, sizeof(buf), "0x%04X (%i)", i, i);
			r.val = buf;
			r.f   = g_FixedFont[font_small];
			v.push_back(r);
		}
	}

	// video pid
	if (g_RemoteControl->current_PIDs.PIDs.vpid)
	{
		r.key = "Video PID: ";
		i = g_RemoteControl->current_PIDs.PIDs.vpid;
		snprintf(buf, sizeof(buf), "0x%04X (%i)", i, i);
		r.val = buf;
		r.f   = g_FixedFont[font_small];
		v.push_back(r);
	}

	// audio pid(s)
	if (!g_RemoteControl->current_PIDs.APIDs.empty())
	{
		for (unsigned int li = 0; li < g_RemoteControl->current_PIDs.APIDs.size(); li++)
		{
			r.key = li ? "" : "Audio PID(s): ";
			i = g_RemoteControl->current_PIDs.APIDs[li].pid;
			std::string strpid = to_string(i);
			std::string details(" ");
			for (std::vector<std::map<std::string, std::string> >::iterator it = streamdata.begin(); it != streamdata.end(); ++it)
			{
				if ((*it)["pid"] == strpid)
				{
					details = (*it)["language"];
					if (details != " ")
						details += ", ";
					details += (*it)["codec"];
					break;
				}
			}
			if (details == " ")
				details.clear();
			snprintf(buf, sizeof(buf), "0x%04X (%i)%s", i, i, details.c_str());
			r.val = buf;
			r.col = (li == g_RemoteControl->current_PIDs.PIDs.selected_apid) ? COL_MENUCONTENT_TEXT : COL_MENUCONTENTINACTIVE_TEXT;
			r.f   = g_FixedFont[font_small];
			v.push_back(r);
		}
	}

	spaceoffset = 0;
	for (std::vector<row>::iterator it = v.begin(); it != v.end(); ++it)
		spaceoffset = std::max(spaceoffset, it->f->getRenderWidth(it->key));
	spaceoffset = std::max(spaceoffset, g_FixedFont[font_info]->getRenderWidth(std::string(g_Locale->getText(LOCALE_STREAMINFO_BITRATE)) + ": "));

	for (std::vector<row>::iterator it = v.begin(); it != v.end(); ++it)
	{
		it->f->RenderString(xpos, ypos, spaceoffset, it->key, COL_MENUCONTENT_TEXT);
		std::string text = it->val.c_str();
		it->f->RenderString(xpos + spaceoffset, ypos, box_width - spaceoffset, text, it->col);
		if (it < v.end() - 1)
			ypos += it->f->getHeight();
	}

	ypos += iheight;
	if (box_h == 0)
		box_h = ypos - ypos1;
	yypos = ypos;

	if (!mp)
		paintCASystem(xpos, ypos);
}

#define NUM_CAIDS 12
void CStreamInfo2::paintCASystem(int xpos, int ypos)
{
	int ypos1 = ypos;

	if (box_h2 > 0)
		frameBuffer->paintBoxRel(0, ypos, box_width, box_h2, COL_MENUCONTENT_PLUS_0);

	std::string casys[NUM_CAIDS] = {"Irdeto:", "Betacrypt:", "Seca:", "Viaccess:", "Nagra:", "Conax: ", "Cryptoworks:", "Videoguard:", "Biss:", "DreCrypt:", "PowerVU:", "Tandberg:"};
	bool caids[NUM_CAIDS];
	int array[NUM_CAIDS];
	char tmp[100];

	CZapitChannel *channel = CZapit::getInstance()->GetCurrentChannel();
	if (!channel)
		return;

	for (int i = 0; i < NUM_CAIDS; i++)
	{
		array[i] = g_FixedFont[font_small]->getRenderWidth(casys[i]);
		caids[i] = false;
	}

	int acaid = 0;
	FILE *f = fopen("/tmp/ecm.info", "rt");
	if (f)
	{
		char buf[80];
		if (fgets(buf, sizeof(buf), f) != NULL)
		{
			int i = 0;
			while (buf[i] != '0')
				i++;
			sscanf(&buf[i], "%X", &acaid);
		}
		fclose(f);
	}

	int off = 0;

	for (casys_map_iterator_t it = channel->camap.begin(); it != channel->camap.end(); ++it)
	{
		int idx = -1;
		switch (((*it) >> 8) & 0xFF)
		{
			case 0x06:
				idx = 0;
				break;
			case 0x17:
				idx = 1;
				break;
			case 0x01:
				idx = 2;
				break;
			case 0x05:
				idx = 3;
				break;
			case 0x18:
				idx = 4;
				break;
			case 0x0B:
				idx = 5;
				break;
			case 0x0D:
				idx = 6;
				break;
			case 0x09:
				idx = 7;
				break;
			case 0x26:
				idx = 8;
				break;
			case 0x4a:
				idx = 9;
				break;
			case 0x0E:
				idx = 10;
				break;
			case 0x10:
				idx = 11;
				break;
			default:
				break;
		}
		if (idx >= 0)
		{
			snprintf(tmp, sizeof(tmp), " 0x%04X", (*it));
			casys[idx] += tmp;
			caids[idx] = true;
			if (off < array[idx])
				off = array[idx];
		}
	}

	off += OFFSET_INNER_SMALL;
	bool cryptsystems = true;
	for (int ca_id = 0; ca_id < NUM_CAIDS; ca_id++)
	{
		if (caids[ca_id] == true)
		{
			if (cryptsystems)
			{
				ypos += sheight;
				std::string casys_locale(g_Locale->getText(LOCALE_STREAMINFO_CASYSTEMS));
				casys_locale += ":";
				g_FixedFont[font_small]->RenderString(xpos, ypos, box_width, casys_locale, COL_MENUCONTENT_TEXT);
				cryptsystems = false;
			}
			ypos += sheight;
			int width_txt = 0, index = 0;
			const char *tok = " ";
			std::string::size_type last_pos = casys[ca_id].find_first_not_of(tok, 0);
			std::string::size_type pos = casys[ca_id].find_first_of(tok, last_pos);
			while (std::string::npos != pos || std::string::npos != last_pos)
			{
				int col = COL_MENUCONTENT_TEXT;
				if (index > 0)
				{
					col = COL_MENUCONTENTINACTIVE_TEXT;
					int id;
					if (1 == sscanf(casys[ca_id].substr(last_pos, pos - last_pos).c_str(), "%X", &id) && acaid == id)
						col = COL_MENUCONTENT_TEXT;
				}
				g_FixedFont[font_small]->RenderString(xpos + width_txt, ypos, box_width, casys[ca_id].substr(last_pos, pos - last_pos), col);
				if (index == 0)
					width_txt = spaceoffset;
				else
					width_txt += g_FixedFont[font_small]->getRenderWidth(casys[ca_id].substr(last_pos, pos - last_pos)) + 10;
				index++;
				if (index > 5)
					break;
				last_pos = casys[ca_id].find_first_not_of(tok, pos);
				pos = casys[ca_id].find_first_of(tok, last_pos);
			}
		}
	}
	if (box_h2 == 0)
		box_h2 = ypos - ypos1;
}

/*
 * some definition
 */

static unsigned long timeval_to_ms(const struct timeval *tv)
{
	return (tv->tv_sec * 1000) + ((tv->tv_usec + 500) / 1000);
}

long delta_time_ms(struct timeval *tv, struct timeval *last_tv)
{
	return timeval_to_ms(tv) - timeval_to_ms(last_tv);
}

bool CStreamInfo2::ts_setup()
{
	if (mp)
	{
		mp->GetReadCount();
		if (pthread_create(&probe_thread, NULL, probeStreams, this))
			fprintf(stderr, "creating probe_thread failed\n");
	}
	else
	{
		probebuf_length = 0;
		probebuf_off = 0;
		abort_probing = false;
		probed = false;

		dmx = new cDemux(0);
		if (!dmx)
			return false;

		dmxbuf = new unsigned char[TS_BUF_SIZE];
		if (!dmxbuf)
		{
			delete dmx;
			dmx = NULL;
			return false;
		}
		dmx->Open(DMX_TP_CHANNEL, NULL, (8 * 1024 * 1024 / TS_LEN) * TS_LEN);

		std::vector<unsigned short> pids;

		for (unsigned int i = 0; i < g_RemoteControl->current_PIDs.APIDs.size(); i++)
			pids.push_back(g_RemoteControl->current_PIDs.APIDs[i].pid);
		probebuf_size = 2 * ((100000 * pids.size()) / TS_LEN) * TS_LEN;

		if (g_RemoteControl->current_PIDs.PIDs.vpid)
			pids.push_back(g_RemoteControl->current_PIDs.PIDs.vpid);
#if 0
		if (g_RemoteControl->current_PIDs.PIDs.vtxtpid)
			pids.push_back(g_RemoteControl->current_PIDs.PIDs.vtxtpid);

		pids.push_back(0);
		pids.push_back(g_RemoteControl->current_PIDs.PIDs.pmtpid);
#else
		if (pids.empty())
		{
			delete dmx;
			dmx = NULL;
			delete[] dmxbuf;
			dmxbuf = NULL;
			return false;
		}
#endif
		std::vector<unsigned short>::iterator it = pids.begin();
		dmx->pesFilter(*it);
		++it;
		for (; it != pids.end(); ++it)
			dmx->addPid(*it);
		probebuf = new unsigned char[probebuf_size];

		dmx->Start(true);
		if (pthread_create(&probe_thread, NULL, probeStreams, this))
		{
			fprintf(stderr, "creating probe_thread failed\n");
			delete[] probebuf;
			probebuf = NULL;
		}
	}

	gettimeofday(&first_tv, NULL);
	last_tv.tv_sec = first_tv.tv_sec;
	last_tv.tv_usec = first_tv.tv_usec;
	b_total = 0;
	return true;
}

bool CStreamInfo2::update_rate()
{

	if (!mp && !dmx)
		return 0;

	int timeout = 100;

	int b_len;
	if (mp)
	{
		usleep(timeout * 1000);
		b_len = mp->GetReadCount();
	}
	else
	{
		b_len = dmx->Read(dmxbuf, TS_BUF_SIZE, timeout);
		if (probebuf && b_len > TS_LEN - 1 && probebuf_length < probebuf_size)
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(probe_mutex);
			if (probebuf)
			{
				int len = (b_len / TS_LEN) * TS_LEN;
				uint16_t vpid = g_RemoteControl->current_PIDs.PIDs.vpid;
				if (vpid)
				{
					unsigned char *p = dmxbuf, *p_end = dmxbuf + len;
					for (; probebuf_size - TS_LEN > probebuf_length && p < p_end; p += TS_LEN)
					{
						if (vpid != (0x1fff & (p[1] << 8 | p[2])))
						{
							memcpy(probebuf + probebuf_length, p, TS_LEN);
							probebuf_length += TS_LEN;
						}
					}
				}
				else
				{
					int n = std::min(len, (int) probebuf_size - (int) probebuf_length);
					memcpy(probebuf + probebuf_length, dmxbuf, n);
					probebuf_length += n;
				}
			}
		}
	}

	long b = b_len;
	if (b <= 0)
		return false;

	gettimeofday(&tv, NULL);

	b_total += b;

	long d_tim_ms;

	d_tim_ms = delta_time_ms(&tv, &last_tv);
	if (d_tim_ms <= 0)
		d_tim_ms = 1; // ignore usecs

	bit_s = (((uint64_t) b * 8000ULL) + ((uint64_t) d_tim_ms / 2ULL))
		/ (uint64_t) d_tim_ms;

	d_tim_ms = delta_time_ms(&tv, &first_tv);
	if (d_tim_ms <= 0)
		d_tim_ms = 1; // ignore usecs

	abit_s = ((b_total * 8000ULL) + ((uint64_t) d_tim_ms / 2ULL))
		 / (uint64_t) d_tim_ms;

	last_tv.tv_sec = tv.tv_sec;
	last_tv.tv_usec = tv.tv_usec;
	return true;
}

int CStreamInfo2::ts_close()
{
	abort_probing = true;
	if (probe_thread)
		pthread_join(probe_thread, NULL);

	if (dmx)
	{
		delete dmx;
		dmx = NULL;
	}
	if (dmxbuf)
	{
		delete [] dmxbuf;
		dmxbuf = NULL;
	}
	return 0;
}

void CStreamInfo2::showSNR()
{
	int _h = 2*iheight;
	//int _y = sig_text_y + 4*sheight + 3*OFFSET_INNER_SMALL;
	int _y = y + height - OFFSET_INNER_MID - _h;

	if (signalbox == NULL)
	{
		signalbox = new CSignalBox(width - width/3 - OFFSET_INNER_MID, _y, pip->getWidth(), _h, frontend);
		signalbox->setColorBody(COL_MENUCONTENT_PLUS_0);
		signalbox->setTextColor(COL_MENUCONTENT_TEXT);
		signalbox->doPaintBg(true);
	}
	signalbox->paint(false);
}
