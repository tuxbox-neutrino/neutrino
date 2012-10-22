/*
	Neutrino-GUI  -   DBoxII-Project


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

#include <gui/streaminfo2.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <gui/color.h>
#include <gui/widget/icons.h>
#include <gui/customcolor.h>
#include <daemonc/remotecontrol.h>
#include <zapit/zapit.h>
#include <zapit/getservices.h>
#include <video.h>
#include <audio.h>
#include <dmx.h>
#include <zapit/satconfig.h>
#include <string>

extern cVideo * videoDecoder;
extern cAudio * audioDecoder;

extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */

CStreamInfo2::CStreamInfo2 ()
{
	frameBuffer = CFrameBuffer::getInstance ();

	font_head = SNeutrinoSettings::FONT_TYPE_MENU_TITLE;
	font_info = SNeutrinoSettings::FONT_TYPE_MENU;
	font_small = SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL;

	hheight = g_Font[font_head]->getHeight ();
	iheight = g_Font[font_info]->getHeight ();
	sheight = g_Font[font_small]->getHeight ();

	max_width = frameBuffer->getScreenWidth(true);
	max_height = frameBuffer->getScreenHeight(true);

	width =  frameBuffer->getScreenWidth();
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
}

CStreamInfo2::~CStreamInfo2 ()
{
	videoDecoder->Pig(-1, -1, -1, -1);
	ts_close();
}

int CStreamInfo2::exec (CMenuTarget * parent, const std::string &)
{

	if (parent)
		parent->hide ();

	frontend = CFEManager::getInstance()->getLiveFE();
	paint (paint_mode);
	int res = doSignalStrengthLoop ();
	hide ();
	return res;
}

int CStreamInfo2::doSignalStrengthLoop ()
{
#define BAR_WIDTH 150
#define BAR_HEIGHT 12
	int res = menu_return::RETURN_REPAINT;
	sigscale = new CProgressBar(true, BAR_WIDTH, BAR_HEIGHT);
	snrscale = new CProgressBar(true, BAR_WIDTH, BAR_HEIGHT);
	lastsnr = lastsig = -1;

	neutrino_msg_t msg;
	uint64_t maxb, minb, lastb, tmp_rate;
	unsigned int current_pmt_version= pmt_version;
	int cnt = 0;
	char tmp_str[150];
	int delay_counter = 0;
	const int delay = 15;
	int offset = g_Font[font_info]->getRenderWidth(g_Locale->getText (LOCALE_STREAMINFO_BITRATE));
	int sw = g_Font[font_info]->getRenderWidth ("99999.999");
	maxb = minb = lastb = tmp_rate = 0;
	ts_setup ();
	while (1) {
		neutrino_msg_data_t data;

		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS (100);
		g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

		signal.sig = frontend->getSignalStrength() & 0xFFFF;
		signal.snr = frontend->getSignalNoiseRatio() & 0xFFFF;
		signal.ber = frontend->getBitErrorRate();

		int ret = update_rate ();
		if (paint_mode == 0) {
			char currate[150];
			if (cnt < 12)
				cnt++;
			int dheight = g_Font[font_info]->getHeight ();
			int dx1 = x + 10;

			if(delay_counter > delay + 1){
				CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
				if(channel)
					pmt_version = channel->getPmtVersion();
				if(pmt_version != current_pmt_version){
					delay_counter = 0;
				}
			}
			if (ret && (lastb != bit_s)) {
				lastb = bit_s;

				if (maxb < bit_s)
					rate.max_short_average = maxb = bit_s;
				if ((cnt > 10) && ((minb == 0) || (minb > bit_s)))
					rate.min_short_average = minb = bit_s;

				sprintf(tmp_str, "%s:",g_Locale->getText(LOCALE_STREAMINFO_BITRATE));
				g_Font[font_info]->RenderString(dx1 , average_bitrate_pos, offset+10, tmp_str, COL_INFOBAR, 0, true);
				sprintf(currate, "%5llu.%02llu", rate.short_average / 1000ULL, rate.short_average % 1000ULL);
				frameBuffer->paintBoxRel (dx1 + average_bitrate_offset , average_bitrate_pos -dheight, sw, dheight, COL_MENUHEAD_PLUS_0);
				g_Font[font_info]->RenderString (dx1 + average_bitrate_offset , average_bitrate_pos, sw - 10, currate, COL_INFOBAR);
				sprintf(tmp_str, "(%s)",g_Locale->getText(LOCALE_STREAMINFO_AVERAGE_BITRATE));
				g_Font[font_info]->RenderString (dx1 + average_bitrate_offset + sw , average_bitrate_pos, sw *2, tmp_str, COL_INFOBAR);

			}
			showSNR ();
			if(pmt_version != current_pmt_version && delay_counter > delay){
				current_pmt_version = pmt_version;
				paint_techinfo (x + 10, y+ hheight +5);
			}
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

		paint_signal_fe(rate, signal);

		signal.old_sig = signal.sig;
		signal.old_snr = signal.snr;
		signal.old_ber = signal.ber;

		// switch paint mode
		if (msg == CRCInput::RC_red || msg == CRCInput::RC_blue || msg == CRCInput::RC_green || msg == CRCInput::RC_yellow) {
			hide ();
			sigscale->reset();
			snrscale->reset();
			lastsnr = lastsig = -1;
			paint_mode = !paint_mode;
			paint (paint_mode);
			continue;
		}
		else if(msg == CRCInput::RC_setup) {
			res = menu_return::RETURN_EXIT_ALL;
			break;
		}
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
			g_RCInput->postMsg (msg, 0);
			res = menu_return::RETURN_EXIT_ALL;
			break;
		}
		else if (msg == (neutrino_msg_t) g_settings.key_screenshot) {
			CNeutrinoApp::getInstance ()->handleMsg (msg, data);
			continue;
		}

		// -- any key --> abort
		if (msg <= CRCInput::RC_MaxRC)
			break;

		// -- push other events
		if (msg > CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout)
			CNeutrinoApp::getInstance ()->handleMsg (msg, data);
	}
	delete sigscale;
	sigscale = NULL;
	delete snrscale;
	snrscale = NULL;
	ts_close ();
	return res;
}

void CStreamInfo2::hide ()
{
	videoDecoder->Pig(-1, -1, -1, -1);
	frameBuffer->paintBackgroundBoxRel (0, 0, max_width, max_height);
}

void CStreamInfo2::paint_pig (int px, int py, int w, int h)
{
	frameBuffer->paintBackgroundBoxRel (px,py, w, h);
	printf("CStreamInfo2::paint_pig x %d y %d w %d h %d\n", px, py, w, h);
	videoDecoder->Pig(px, py, w, h, frameBuffer->getScreenWidth(true), frameBuffer->getScreenHeight(true));
}

void CStreamInfo2::paint_signal_fe_box(int _x, int _y, int w, int h)
{
	int y1;
	int xd = w/4;

	int tuner = 1 + frontend->getNumber();
	char tname[255];
	snprintf(tname, sizeof(tname), "%s: %d: %s", g_Locale->getText(LOCALE_STREAMINFO_SIGNAL), tuner, frontend->getInfo()->name);

	g_Font[font_small]->RenderString(_x, _y+iheight+15, width-10, tname /*tuner_name.c_str()*/, COL_INFOBAR, 0, true);

	sigBox_x = _x;
	sigBox_y = _y+iheight+15;
	sigBox_w = w;
	sigBox_h = h-iheight*3;
	frameBuffer->paintBoxRel(sigBox_x,sigBox_y,sigBox_w+2,sigBox_h, COL_BLACK);
	sig_text_y = sigBox_y + sigBox_h;
	y1 = sig_text_y + sheight+4;
	int fw = g_Font[font_small]->getWidth();


	frameBuffer->paintBoxRel(_x+xd*0,y1- 12,16,2, COL_RED); //red
	g_Font[font_small]->RenderString(_x+20+xd*0, y1, fw*4, "BER", COL_INFOBAR, 0, true);

	frameBuffer->paintBoxRel(_x+xd*1,y1- 12,16,2,COL_BLUE); //blue
	g_Font[font_small]->RenderString(_x+20+xd*1, y1, fw*4, "SNR", COL_INFOBAR, 0, true);

	frameBuffer->paintBoxRel(_x+8+xd*2,y1- 12,16,2, COL_GREEN); //green
	g_Font[font_small]->RenderString(_x+28+xd*2, y1, fw*4, "SIG", COL_INFOBAR, 0, true);

	frameBuffer->paintBoxRel(_x+xd*3,y1- 12,16,2,COL_YELLOW); // near yellow
	g_Font[font_small]->RenderString(_x+20+xd*3, y1, fw*5, "Bitrate", COL_INFOBAR, 0, true);

	sig_text_ber_x =  _x +      xd * 0;
	sig_text_snr_x =  _x +  5 + xd * 1;
	sig_text_sig_x =  _x +  5 + xd * 2;
	sig_text_rate_x = _x + 10 + xd * 3;

	int maxmin_x; // x-position of min and max
	int fontW = g_Font[font_small]->getWidth();

	if (paint_mode == 0)
		maxmin_x = sig_text_ber_x-(fontW*4);
	else
		maxmin_x = _x + 40 + xd * 3 + (fontW*4);

	g_Font[font_small]->RenderString(maxmin_x, y1 + sheight + 5, fw*3, "max", COL_INFOBAR, 0, true);
	g_Font[font_small]->RenderString(maxmin_x, y1 + (sheight * 2) +5, fw*3, "now", COL_INFOBAR, 0, true);
	g_Font[font_small]->RenderString(maxmin_x, y1 + (sheight * 3) +5, fw*3, "min", COL_INFOBAR, 0, true);


	sigBox_pos = 0;

	signal.old_sig = 1;
	signal.old_snr = 1;
	signal.old_ber = 1;
}

void CStreamInfo2::paint_signal_fe(struct bitrate br, struct feSignal s)
{
	int   x_now = sigBox_pos;
	int   yt = sig_text_y + (sheight *2)+4;
	int   yd;
	static int old_x=0,old_y=0;
	sigBox_pos++;
	sigBox_pos %= sigBox_w;

	frameBuffer->paintVLine(sigBox_x+sigBox_pos, sigBox_y, sigBox_y+sigBox_h, COL_WHITE);
	frameBuffer->paintVLine(sigBox_x+x_now, sigBox_y, sigBox_y+sigBox_h+1, COL_BLACK);

	long value = (long) (bit_s / 1000ULL);

	SignalRenderStr(value,     sig_text_rate_x, yt + sheight);
	SignalRenderStr(br.max_short_average/ 1000ULL, sig_text_rate_x, yt);
	SignalRenderStr(br.min_short_average/ 1000ULL, sig_text_rate_x, yt + (sheight * 2));
	if ( g_RemoteControl->current_PIDs.PIDs.vpid > 0 ){
		yd = y_signal_fe (value, scaling, sigBox_h);// Video + Audio
	} else {
		yd = y_signal_fe (value, 512, sigBox_h); // Audio only
	}
	if ((old_x == 0 && old_y == 0) || sigBox_pos == 1) {
		old_x = sigBox_x+x_now;
		old_y = sigBox_y+sigBox_h-yd;
	} else {
		frameBuffer->paintLine(old_x, old_y, sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_YELLOW); //yellow
		old_x = sigBox_x+x_now;
		old_y = sigBox_y+sigBox_h-yd;
	}

	if (s.ber != s.old_ber) {
		SignalRenderStr(s.ber,     sig_text_ber_x, yt + sheight);
		SignalRenderStr(s.max_ber, sig_text_ber_x, yt);
		SignalRenderStr(s.min_ber, sig_text_ber_x, yt + (sheight * 2));
	}
	yd = y_signal_fe (s.ber, 4000, sigBox_h);
	frameBuffer->paintPixel(sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_RED); //red


	if (s.sig != s.old_sig) {
		SignalRenderStr(s.sig,     sig_text_sig_x, yt + sheight);
		SignalRenderStr(s.max_sig, sig_text_sig_x, yt);
		SignalRenderStr(s.min_sig, sig_text_sig_x, yt + (sheight * 2));
	}
	yd = y_signal_fe (s.sig, 65000, sigBox_h);
	frameBuffer->paintPixel(sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_GREEN); //green


	if (s.snr != s.old_snr) {
		SignalRenderStr(s.snr,     sig_text_snr_x, yt + sheight);
		SignalRenderStr(s.max_snr, sig_text_snr_x, yt);
		SignalRenderStr(s.min_snr, sig_text_snr_x, yt + (sheight * 2));
	}
	yd = y_signal_fe (s.snr, 65000, sigBox_h);
	frameBuffer->paintPixel(sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_BLUE); //blue
}

// -- calc y from max_range and max_y
int CStreamInfo2::y_signal_fe (unsigned long value, unsigned long max_value, int max_y)
{
	long l;

	if (!max_value)
		max_value = 1;

	l = ((long) max_y * (long) value) / (long) max_value;
	if (l > max_y)
		l = max_y;

	return (int) l;
}

void CStreamInfo2::SignalRenderStr(unsigned int value, int _x, int _y)
{
	char str[30];
	int fw = g_Font[font_small]->getWidth();
	fw *=(fw>17)?5:6;
	frameBuffer->paintBoxRel(_x, _y - sheight + 5, fw, sheight -1, COL_MENUHEAD_PLUS_0);
	sprintf(str,"%6u",value);
	g_Font[font_small]->RenderString(_x, _y + 5, fw, str, COL_INFOBAR, 0, true);
}

void CStreamInfo2::paint (int /*mode*/)
{
	const char *head_string;

	width =  frameBuffer->getScreenWidth();
	height = frameBuffer->getScreenHeight();
	x = frameBuffer->getScreenX();
	y = frameBuffer->getScreenY();
	int ypos = y + 5;
	int xpos = x + 10;

	if (paint_mode == 0) {
		// -- tech Infos, PIG, small signal graph
		head_string = g_Locale->getText (LOCALE_STREAMINFO_HEAD);
		CVFD::getInstance ()->setMode (CVFD::MODE_MENU_UTF8, head_string);

		// paint backround, title pig, etc.
		frameBuffer->paintBoxRel (0, 0, max_width, max_height, COL_MENUHEAD_PLUS_0);
		g_Font[font_head]->RenderString (xpos, ypos + hheight + 1, width, head_string, COL_MENUHEAD, 0, true);	// UTF-8
		ypos += hheight;

		paint_pig (width - width/3 - 10, y + 10, width/3, height/3);
		paint_techinfo (xpos, ypos);
		paint_signal_fe_box (width - width/3 - 10, (y + 10 + height/3 + hheight), width/3, height/3 + hheight);
	} else {
		// --  small PIG, small signal graph
		// -- paint backround, title pig, etc.
		frameBuffer->paintBoxRel (0, 0, max_width, max_height, COL_MENUHEAD_PLUS_0);

		// -- paint large signal graph
		paint_signal_fe_box (x, y, width, height-100);

	}
}

void CStreamInfo2::paint_techinfo(int xpos, int ypos)
{
	char buf[100];
	int xres = 0, yres = 0, aspectRatio = 0, framerate = -1;
	// paint labels
	int spaceoffset = 0,i = 0;
	int ypos1 = ypos;
	int box_width = width*2/3 - 10;

	yypos = ypos;
	if(box_h > 0)
		frameBuffer->paintBoxRel (0, ypos, box_width, box_h, COL_MENUHEAD_PLUS_0);

	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(!channel)
		return;

	transponder t;
	CServiceManager::getInstance()->GetTransponder(channel->getTransponderId(), t);

	int array[5]={g_Font[font_info]->getRenderWidth(g_Locale->getText (LOCALE_STREAMINFO_RESOLUTION)),
		      g_Font[font_info]->getRenderWidth(g_Locale->getText (LOCALE_STREAMINFO_ARATIO)),
		      g_Font[font_info]->getRenderWidth(g_Locale->getText (LOCALE_STREAMINFO_FRAMERATE)),
		      g_Font[font_info]->getRenderWidth(g_Locale->getText (LOCALE_STREAMINFO_AUDIOTYPE)),
		      g_Font[font_info]->getRenderWidth(g_Locale->getText (LOCALE_SCANTS_FREQDATA))};

	for(i=0 ; i<5; i++) {
		if(spaceoffset < array[i])
			spaceoffset = array[i];
	}
	average_bitrate_offset = spaceoffset+=4;

	if(channel->getVideoPid() && !(videoDecoder->getBlank())){
		 videoDecoder->getPictureInfo(xres, yres, framerate);
		 aspectRatio = videoDecoder->getAspectRatio();
	}

	//Video RESOLUTION
	ypos += iheight;
	sprintf (buf, "%s:",g_Locale->getText (LOCALE_STREAMINFO_RESOLUTION));
	g_Font[font_info]->RenderString (xpos, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8
	sprintf (buf, "%dx%d", xres, yres);
	g_Font[font_info]->RenderString (xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8

	//audio rate
	ypos += iheight;
	sprintf (buf, "%s:",g_Locale->getText (LOCALE_STREAMINFO_ARATIO));
	g_Font[font_info]->RenderString (xpos, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8
	switch (aspectRatio) {
		case 0:
			sprintf (buf, "N/A");
			break;
		case 1:
			sprintf (buf, "4:3");
			break;
		case 2:
			sprintf (buf, "14:9");
			break;
		case 3:
			sprintf (buf, "16:9");
			break;
		case 4:
			sprintf (buf, "20:9");
			break;
		default:
			strncpy (buf, g_Locale->getText (LOCALE_STREAMINFO_ARATIO_UNKNOWN), sizeof (buf)-1);
	}
	g_Font[font_info]->RenderString (xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8

	//Video FRAMERATE
	ypos += iheight;
	sprintf (buf, "%s:", g_Locale->getText (LOCALE_STREAMINFO_FRAMERATE));
	g_Font[font_info]->RenderString (xpos, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8
	switch (framerate) {
		case 0:
			snprintf (buf,sizeof(buf), "23.976fps");
			break;
		case 1:
			snprintf (buf,sizeof(buf), "24fps");
			break;
		case 2:
			snprintf (buf,sizeof(buf), "25fps");
			break;
		case 3:
			snprintf (buf,sizeof(buf), "29,976fps");
			break;
		case 4:
			snprintf (buf,sizeof(buf), "30fps");
			break;
		case 5:
			snprintf (buf,sizeof(buf), "50fps");
			break;
		case 6:
			snprintf (buf,sizeof(buf), "50,94fps");
			break;
		case 7:
			snprintf (buf,sizeof(buf), "60fps");
			break;
		default:
			strncpy (buf, g_Locale->getText (LOCALE_STREAMINFO_FRAMERATE_UNKNOWN), sizeof (buf)-1);
			break;
	}
	g_Font[font_info]->RenderString (xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8
	// place for average bitrate
	average_bitrate_pos = ypos += iheight;
	//AUDIOTYPE
	ypos += iheight;
	int type, layer, freq, mode, lbitrate;
	audioDecoder->getAudioInfo(type, layer, freq, lbitrate, mode);

	const char *mpegmodes[4] = { "stereo", "joint_st", "dual_ch", "single_ch" };
	const char *ddmodes[8] = { "CH1/CH2", "C", "L/R", "L/C/R", "L/R/S", "L/C/R/S", "L/R/SL/SR", "L/C/R/SL/SR" };

	sprintf (buf, "%s:", g_Locale->getText (LOCALE_STREAMINFO_AUDIOTYPE));
	g_Font[font_info]->RenderString (xpos, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8

	if(type == 0) {
		sprintf (buf, "MPEG %s (%d)", mpegmodes[mode], freq);
	} else {
		sprintf (buf, "DD %s (%d)", ddmodes[mode], freq);
	}
	g_Font[font_info]->RenderString (xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8

	//satellite
	ypos += iheight;
	if(t.type == FE_QPSK)
		sprintf (buf, "%s:",g_Locale->getText (LOCALE_SATSETUP_SATELLITE));//swiped locale
	else if(t.type == FE_QAM)
		sprintf (buf, "%s:",g_Locale->getText (LOCALE_CHANNELLIST_PROVS));

	g_Font[font_info]->RenderString(xpos, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8

	sprintf (buf, "%s",
		CServiceManager::getInstance()->GetSatelliteName(channel->getSatellitePosition()).c_str());
	g_Font[font_info]->RenderString (xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8

	//channel
	ypos += iheight;
	sprintf (buf, "%s:",g_Locale->getText (LOCALE_TIMERLIST_CHANNEL));//swiped locale
	g_Font[font_info]->RenderString(xpos, ypos, box_width, buf , COL_INFOBAR, 0, true); // UTF-8
	sprintf(buf, "%s", channel->getName().c_str());
	g_Font[font_info]->RenderString (xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true);	// UTF-8

	//tsfrequenz
	ypos += iheight;

	scaling = 27000;
	if (t.type == FE_QPSK && t.feparams.dvb_feparams.u.qpsk.fec_inner < FEC_S2_QPSK_1_2)
		scaling = 15000;

	sprintf (buf, "%s",g_Locale->getText (LOCALE_SCANTS_FREQDATA));
	g_Font[font_info]->RenderString(xpos, ypos, box_width, buf , COL_INFOBAR, 0, true); // UTF-8
	g_Font[font_info]->RenderString(xpos+spaceoffset, ypos, box_width, t.description().c_str(), COL_INFOBAR, 0, true); // UTF-8
	
	// paint labels
	int fontW = g_Font[font_small]->getWidth();
	spaceoffset = 7 * fontW;
	//onid
	ypos+= sheight;
	sprintf(buf, "0x%04X (%i)", channel->getOriginalNetworkId(), channel->getOriginalNetworkId());
	g_Font[font_small]->RenderString(xpos, ypos, box_width, "ONid:" , COL_INFOBAR, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8

	//sid
	ypos+= sheight;
	sprintf(buf, "0x%04X (%i)", channel->getServiceId(), channel->getServiceId());
	g_Font[font_small]->RenderString(xpos, ypos, box_width, "Sid:" , COL_INFOBAR, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8

	//tsid
	ypos+= sheight;
	sprintf(buf, "0x%04X (%i)", channel->getTransportStreamId(), channel->getTransportStreamId());
	g_Font[font_small]->RenderString(xpos, ypos, box_width, "TSid:" , COL_INFOBAR, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8

	//pmtpid
	ypos+= sheight;
	pmt_version = channel->getPmtVersion();
	sprintf(buf, "0x%04X (%i) [0x%02X]", channel->getPmtPid(), channel->getPmtPid(), pmt_version);
	g_Font[font_small]->RenderString(xpos, ypos, box_width, "PMTpid:", COL_INFOBAR, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8

	//vpid
	ypos+= sheight;
	if ( g_RemoteControl->current_PIDs.PIDs.vpid > 0 ){
		sprintf(buf, "0x%04X (%i)", g_RemoteControl->current_PIDs.PIDs.vpid, g_RemoteControl->current_PIDs.PIDs.vpid );
	} else {
		sprintf(buf, "%s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
	}
	g_Font[font_small]->RenderString(xpos, ypos, box_width, "Vpid:" , COL_INFOBAR, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8

	//apid
	ypos+= sheight;
	g_Font[font_small]->RenderString(xpos, ypos, box_width, "Apid(s):" , COL_INFOBAR, 0, true); // UTF-8
	if (g_RemoteControl->current_PIDs.APIDs.empty()){
		sprintf(buf, "%s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
	} else {
		unsigned int sw=spaceoffset;
		for (unsigned int li= 0; (li<g_RemoteControl->current_PIDs.APIDs.size()) && (li<10); li++)
		{
			sprintf(buf, "0x%04X (%i)", g_RemoteControl->current_PIDs.APIDs[li].pid, g_RemoteControl->current_PIDs.APIDs[li].pid );
			if (li == g_RemoteControl->current_PIDs.PIDs.selected_apid){
				g_Font[font_small]->RenderString(xpos+sw, ypos, box_width, buf, COL_MENUHEAD, 0, true); // UTF-8
			}
			else{
				g_Font[font_small]->RenderString(xpos+sw, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8
			}
			sw = g_Font[font_small]->getRenderWidth(buf)+sw+10;
			if (((li+1)%3 == 0) &&(g_RemoteControl->current_PIDs.APIDs.size()-1 > li)){ // if we have lots of apids, put "intermediate" line with pids
				ypos+= sheight;
				sw=spaceoffset;
			}
		}
	}

	//vtxtpid
	ypos += sheight;
	if ( g_RemoteControl->current_PIDs.PIDs.vtxtpid == 0 )
        	sprintf(buf, "%s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
	else
        	sprintf(buf, "0x%04X (%i)", g_RemoteControl->current_PIDs.PIDs.vtxtpid, g_RemoteControl->current_PIDs.PIDs.vtxtpid );
	g_Font[font_small]->RenderString(xpos, ypos, box_width, "VTXTpid:" , COL_INFOBAR, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, box_width, buf, COL_INFOBAR, 0, true); // UTF-8
	if(box_h == 0)
		box_h = ypos - ypos1;
	yypos = ypos;
	paintCASystem(xpos,ypos);
}

#define NUM_CAIDS 11
void CStreamInfo2::paintCASystem(int xpos, int ypos)
{
	unsigned short i, ypos1 = 0;
	int box_width = width*2/3-10;
	if(box_h2 > 0)
	{
		frameBuffer->paintBoxRel (0, ypos+(iheight*2), box_width, box_h2, COL_MENUHEAD_PLUS_0);
	}

	std::string casys[NUM_CAIDS]={"Irdeto:","Betacrypt:","Seca:","Viaccess:","Nagra:","Conax: ","Cryptoworks:","Videoguard:","EBU:","XCrypt:","PowerVU:"};
	bool caids[NUM_CAIDS];
	int array[NUM_CAIDS];
	char tmp[100];

	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();
	if(!channel)
		return;

	for(i = 0; i < NUM_CAIDS; i++) {
		array[i] = g_Font[font_info]->getRenderWidth(casys[i].c_str());
		caids[i] = false;
	}

	int spaceoffset = 0;

	for(casys_map_iterator_t it = channel->camap.begin(); it != channel->camap.end(); ++it) {
		int idx = -1;
		switch(((*it) >> 8) & 0xFF) {
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
			default:
				break;
		}
		if(idx >= 0) {
			snprintf(tmp, sizeof(tmp)," 0x%04X", (*it));
			casys[idx] += tmp;
			caids[idx] = true;
			if(spaceoffset < array[idx])
				spaceoffset = array[idx];
		}
	}

	spaceoffset+=4;
	ypos += iheight*2;
	bool cryptsysteme = true;
	for(int ca_id = 0; ca_id < NUM_CAIDS; ca_id++){
		if(caids[ca_id] == true){
			if(cryptsysteme){
				ypos += iheight;
				g_Font[font_info]->RenderString(xpos , ypos, box_width, "Conditional access:" , COL_INFOBAR, 0, false);
				cryptsysteme = false;
			}
			ypos += sheight;
			int width_txt = 0, index = 0;
			const char *tok = " ";
			std::string::size_type last_pos = casys[ca_id].find_first_not_of(tok, 0);
			std::string::size_type pos = casys[ca_id].find_first_of(tok, last_pos);
			while (std::string::npos != pos || std::string::npos != last_pos){
				g_Font[font_small]->RenderString(xpos + width_txt, ypos, box_width, casys[ca_id].substr(last_pos, pos - last_pos).c_str() , COL_INFOBAR, 0, false);
				if(index == 0)
					width_txt = spaceoffset;
				else
					width_txt += g_Font[font_small]->getRenderWidth(casys[ca_id].substr(last_pos, pos - last_pos).c_str())+10;
				index++;
				if (index > 5)
					break;
				last_pos = casys[ca_id].find_first_not_of(tok, pos);
				pos = casys[ca_id].find_first_of(tok, last_pos);
			}
		}
	}
	if(box_h2 == 0)
		box_h2 = ypos - ypos1;
}

/*
 * some definition
 */

static unsigned long timeval_to_ms (const struct timeval *tv)
{
	return (tv->tv_sec * 1000) + ((tv->tv_usec + 500) / 1000);
}

long delta_time_ms (struct timeval *tv, struct timeval *last_tv)
{
	return timeval_to_ms (tv) - timeval_to_ms (last_tv);
}

static cDemux * dmx;

int CStreamInfo2::ts_setup ()
{
#if 0
	if (g_RemoteControl->current_PIDs.PIDs.vpid == 0)
		return -1;
#endif
	unsigned short vpid, apid = 0;

	vpid = g_RemoteControl->current_PIDs.PIDs.vpid;
	if( !g_RemoteControl->current_PIDs.APIDs.empty() )
		apid = g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid;

	short ret = -1;
	if(vpid == 0 && apid == 0)
		return ret;

	dmx = new cDemux(0);
	if(!dmx)
		return ret;
#define TS_LEN			188
#define TS_BUF_SIZE		(TS_LEN * 2048)	/* fix dmx buffer size */

	dmxbuf = new unsigned char[TS_BUF_SIZE];
	if(!dmxbuf){
		delete dmx;
		dmx = NULL;
		return ret;
	}
	dmx->Open(DMX_TP_CHANNEL, NULL, 3 * 3008 * 62);

	if(vpid > 0) {
		dmx->pesFilter(vpid);
		if(apid > 0)
			dmx->addPid(apid);
	} else
		dmx->pesFilter(apid);

	dmx->Start(true);

	gettimeofday (&first_tv, NULL);
	last_tv.tv_sec = first_tv.tv_sec;
	last_tv.tv_usec = first_tv.tv_usec;
	ret = b_total = 0;
	return ret;
}

int CStreamInfo2::update_rate ()
{

	if(!dmx)
		return 0;
	long b = 0;

	int ret = 0;
	int b_len = 0;
	int timeout = 100;


	b_len = dmx->Read(dmxbuf, TS_BUF_SIZE, timeout);
	//printf("ts: read %d\n", b_len);

	b = b_len;
	if (b <= 0)
		return 0;

	gettimeofday (&tv, NULL);

	b_total += b;

	long d_tim_ms;

	d_tim_ms = delta_time_ms (&tv, &last_tv);
	if (d_tim_ms <= 0)
		d_tim_ms = 1;			//  ignore usecs

	bit_s = (((uint64_t) b * 8000ULL) + ((uint64_t) d_tim_ms / 2ULL))
		/ (uint64_t) d_tim_ms;

	d_tim_ms = delta_time_ms (&tv, &first_tv);
	if (d_tim_ms <= 0)
		d_tim_ms = 1;			//  ignore usecs

	abit_s = ((b_total * 8000ULL) + ((uint64_t) d_tim_ms / 2ULL))
		/ (uint64_t) d_tim_ms;

	last_tv.tv_sec = tv.tv_sec;
	last_tv.tv_usec = tv.tv_usec;
	ret = 1;
	return ret;
}

int CStreamInfo2::ts_close ()
{
	if(dmx)
		delete dmx;
	dmx = NULL;
	if(dmxbuf)
		delete [] dmxbuf;
	dmxbuf = NULL;
	return 0;
}

void CStreamInfo2::showSNR ()
{
	char percent[10];
	int barwidth = 150;
	int sig, snr;
	int posx, posy;
	int sw = g_Font[font_info]->getRenderWidth ("100%");
	int pw = g_Font[font_info]->getRenderWidth (" SNR");

	snr = (signal.snr & 0xFFFF) * 100 / 65535;
	sig = (signal.sig & 0xFFFF) * 100 / 65535;

	int mheight = g_Font[font_info]->getHeight();
	if (lastsig != sig) {
		lastsig = sig;
		posy = yypos + (mheight/2)-5;
		posx = x + 10;
		sprintf(percent, "%d%%", sig);
		sigscale->paintProgressBar2(posx - 1, posy, sig);

		posx = posx + barwidth + 3;
		frameBuffer->paintBoxRel(posx, posy -1, sw, mheight-8, COL_MENUHEAD_PLUS_0);
		g_Font[font_info]->RenderString (posx + 2, posy + mheight-5, sw, percent, COL_INFOBAR);
		g_Font[font_info]->RenderString (posx + 2 + sw, posy + mheight-5, pw, "SIG", COL_INFOBAR);
	}
	if (lastsnr != snr) {
		lastsnr = snr;
		posy = yypos + mheight + 5;
		posx = x + 10;
		sprintf(percent, "%d%% SNR", snr);
		snrscale->paintProgressBar2(posx - 1, posy+2, snr);

		posx = posx + barwidth + 3;
		frameBuffer->paintBoxRel(posx, posy - 1, sw, mheight-8, COL_MENUHEAD_PLUS_0, 0, true);
		g_Font[font_info]->RenderString (posx + 2, posy + mheight-5, sw, percent, COL_INFOBAR, 0, true);
		g_Font[font_info]->RenderString (posx + 2 + sw, posy + mheight-5, pw, "SNR", COL_INFOBAR, 0, true);
	}
}
