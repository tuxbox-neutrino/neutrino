/*
	$Id: lcdd.cpp 2013/10/12 mohousch Exp $

	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2008 Novell, Inc. Author: Stefan Seyfried
		  (C) 2009 Stefan Seyfried
		  (C) 2022 TangoCash

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

#include <driver/lcdd.h>

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>
#include <system/debug.h>
#include <system/helpers.h>
#include <system/proc_tools.h>

#include <liblcddisplay/lcddisplay.h>
#include <gui/widget/icons.h>

#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <zapit/zapit.h>
#include <eitd/sectionsd.h>

#include <daemonc/remotecontrol.h>

extern CRemoteControl *g_RemoteControl;  /* neutrino.cpp */

/* from edvbstring.cpp */
static bool is_UTF8(const std::string &string)
{
	unsigned int len = string.size();

	for (unsigned int i = 0; i < len; ++i)
	{
		if (!(string[i] & 0x80)) // normal ASCII
			continue;

		if ((string[i] & 0xE0) == 0xC0) // one char following.
		{
			// first, length check:
			if (i + 1 >= len)
				return false; // certainly NOT utf-8

			i++;

			if ((string[i] & 0xC0) != 0x80)
				return false; // no, not UTF-8.
		}
		else if ((string[i] & 0xF0) == 0xE0)
		{
			if ((i + 1) >= len)
				return false;

			i++;

			if ((string[i] & 0xC0) != 0x80)
				return false;

			i++;

			if ((string[i] & 0xC0) != 0x80)
				return false;
		}
	}

	return true; // can be UTF8 (or pure ASCII, at least no non-UTF-8 8bit characters)
}

CLCD::CLCD()
	: configfile('\t')
{
	muted = false;
	percentOver = 0;
	volume = 0;
	timeout_cnt = 0;
	icon_dolby = false;
	has_lcd = true;
	is4digits = false;
	clearClock = 0;
	fheight_s  = 8;
	fheight_t  = 12;
	fheight_r  = 16;
	fheight_b  = 10;
	lcd_width  = g_info.hw_caps->display_xres;
	lcd_height = g_info.hw_caps->display_yres;
}

CLCD::~CLCD()
{
}

CLCD *CLCD::getInstance()
{
	static CLCD *lcdd = NULL;

	if (lcdd == NULL)
	{
		lcdd = new CLCD();
	}

	return lcdd;
}

void CLCD::count_down()
{
	if (timeout_cnt > 0)
	{
		timeout_cnt--;

		if (timeout_cnt == 0)
		{
			setlcdparameter();
		}
	}
}

void CLCD::wake_up()
{
	if (atoi(g_settings.lcd_setting_dim_time.c_str()) > 0)
	{
		timeout_cnt = atoi(g_settings.lcd_setting_dim_time.c_str());
		setlcdparameter();
	}
}

void *CLCD::TimeThread(void *)
{
	while (1)
	{
		sleep(1);
		struct stat buf;

		if (stat("/tmp/lcd.locked", &buf) == -1)
		{
			CLCD::getInstance()->showTime();
			CLCD::getInstance()->count_down();
		}
		else
			CLCD::getInstance()->wake_up();
	}

	return NULL;
}

void CLCD::init(const char *fontfile, const char *fontname, const char *fontfile2, const char *fontname2, const char *fontfile3, const char *fontname3)
{
	if (!lcdInit(fontfile, fontname, fontfile2, fontname2, fontfile3, fontname3))
	{
		dprintf(DEBUG_NORMAL, "CLCD::init: LCD-Init failed!\n");
		has_lcd = false;
	}

	if (pthread_create(&thrTime, NULL, TimeThread, NULL) != 0)
	{
		perror("CLCD::init: pthread_create(TimeThread)");
		return ;
	}
}

bool CLCD::lcdInit(const char *fontfile, const char *fontname, const char *fontfile2, const char *fontname2, const char *fontfile3, const char *fontname3)
{
	fontRenderer = new LcdFontRenderClass(&display);
	const char *style_name = fontRenderer->AddFont(fontfile);
	const char *style_name2;
	const char *style_name3;
	const char *style_name4;

	if (fontfile2 != NULL)
		style_name2 = fontRenderer->AddFont(fontfile2);
	else
	{
		style_name2 = style_name;
		fontname2   = fontname;
	}

	if (fontfile3 != NULL)
		style_name3 = fontRenderer->AddFont(fontfile3);
	else
	{
		style_name3 = style_name;
		fontname3   = fontname;
	}

	style_name4 = style_name;
	fontRenderer->InitFontCache();
	fonts.time        = fontRenderer->getFont(fontname, style_name, fheight_t);
	time_size         = fonts.time->getRenderWidth("88:88") + 4;
	fonts.small       = fontRenderer->getFont(fontname2, style_name2, fheight_s);
	fonts.regular     = fontRenderer->getFont(fontname3, style_name3, fheight_r);
	fonts.big         = fontRenderer->getFont(fontname, style_name4, fheight_b);
	setAutoDimm(g_settings.lcd_setting[SNeutrinoSettings::LCD_AUTODIMM]);

	if (!display.isAvailable())
	{
		dprintf(DEBUG_NORMAL, "CLCD::lcdInit: exit...(no lcd-support)\n");
		return false;
	}

	setMode(MODE_TVRADIO);
	return true;
}

void CLCD::displayUpdate()
{
	struct stat buf;

	if (stat("/tmp/lcd.locked", &buf) == -1)
	{
		display.update();
	}
}

void CLCD::setlcdparameter(int dimm, const int contrast, const int power, const int inverse, const int bias)
{
	if (!has_lcd)
		return;

	if (!display.isAvailable())
		return;

	int fd;

	if (power == 0)
		dimm = 0;

	// dimm
	display.setLCDBrightness(dimm);
	// contrast
	display.setLCDContrast(contrast);

	// power ???

	//reverse
	if (inverse)
		display.setInverted(CLCDDisplay::PIXEL_ON);
	else
		display.setInverted(CLCDDisplay::PIXEL_OFF);
}

void CLCD::setlcdparameter(void)
{
	if (!has_lcd)
		return;

	last_toggle_state_power = g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];
	int dim_time = atoi(g_settings.lcd_setting_dim_time);
	int dim_brightness = g_settings.lcd_setting_dim_brightness;
	bool timeouted = (dim_time > 0) && (timeout_cnt == 0);
	int brightness, power = 0;

	if (timeouted)
		brightness = dim_brightness;
	else
		brightness = g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];

	if (last_toggle_state_power && (!timeouted || dim_brightness > 0))
		power = 1;

	if (mode == MODE_STANDBY)
		brightness = g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS];

	setlcdparameter(brightness,
			g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST],
			power,
			g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE],
			0 /*g_settings.lcd_setting[SNeutrinoSettings::LCD_BIAS]*/);
}

static std::string removeLeadingSpaces(const std::string &text)
{
	int pos = text.find_first_not_of(" ");

	if (pos != -1)
		return text.substr(pos);

	return text;
}

static std::string splitString(const std::string &text, const int maxwidth, LcdFont *font, bool dumb, bool utf8)
{
	int pos;
	std::string tmp = removeLeadingSpaces(text);

	if (font->getRenderWidth(tmp.c_str(), utf8) > maxwidth)
	{
		do
		{
			if (dumb)
				tmp = tmp.substr(0, tmp.length() - 1);
			else
			{
				pos = tmp.find_last_of("[ .-]+"); // TODO characters might be UTF-encoded!

				if (pos != -1)
					tmp = tmp.substr(0, pos);
				else // does not fit -> fall back to dumb split
					tmp = tmp.substr(0, tmp.length() - 1);
			}
		}
		while (font->getRenderWidth(tmp.c_str(), utf8) > maxwidth);
	}

	return tmp;
}

static int getProgress()
{
	int Progress = CLCD::getInstance()->percentOver;
	t_channel_id channel_id = 0;
	uint64_t ID = CNeutrinoApp::getInstance()->getMode();

	if (ID == NeutrinoModes::mode_tv || ID == NeutrinoModes::mode_webtv || ID == NeutrinoModes::mode_radio || ID == NeutrinoModes::mode_webradio)
	{
		CZapitChannel *channel = CZapit::getInstance()->GetCurrentChannel();

		if (channel)
			channel_id = channel->getEpgID();

		CSectionsdClient::CurrentNextInfo CurrentNext;
		CEitManager::getInstance()->getCurrentNextServiceKey(channel_id, CurrentNext);

		if (CurrentNext.flags & CSectionsdClient::epgflags::has_current)
		{
			time_t cur_duration = CurrentNext.current_zeit.dauer;
			time_t cur_start_time = CurrentNext.current_zeit.startzeit;

			if ((cur_duration > 0) && (cur_duration < 86400))
			{
				Progress = 100 * (time(NULL) - cur_start_time) / cur_duration;
			}
		}
	}

	return Progress;
}

void CLCD::showTextScreen(const std::string &big, const std::string &small, const bool perform_wakeup, const bool centered)
{
	if (!has_lcd)
		return;

	bool big_utf8 = false;
	bool small_utf8 = false;
	std::string cname[2];
	std::string event[2];
	int namelines = 0, maxnamelines = 1;

	if (!big.empty())
	{
		bool dumb = false;
		big_utf8 = is_UTF8(big);

		while (true)
		{
			namelines = 0;
			std::string title = big;

			do   // first try "intelligent" splitting
			{
				cname[namelines] = splitString(title, lcd_width, (small.empty()) ? fonts.regular : fonts.big, dumb, big_utf8);
				title = removeLeadingSpaces(title.substr(cname[namelines].length()));
				namelines++;
			}
			while (title.length() > 0 && namelines < maxnamelines);

			if (title.length() == 0)
				break;

			dumb = !dumb;	// retry with dumb splitting;

			if (!dumb)	// second retry -> get out;
				break;
		}
	}

	if (!small.empty())
	{
		bool dumb = false;
		small_utf8 = is_UTF8(small);

		while (true)
		{
			namelines = 0;
			std::string title = small;

			do   // first try "intelligent" splitting
			{
				event[namelines] = splitString(title, lcd_width, fonts.small, dumb, small_utf8);
				title = removeLeadingSpaces(title.substr(event[namelines].length()));
				namelines++;
			}
			while (title.length() > 0 && namelines < maxnamelines);

			if (title.length() == 0)
				break;

			dumb = !dumb;	// retry with dumb splitting;

			if (!dumb)	// second retry -> get out;
				break;
		}
	}

	//
	int t_h = lcd_height - (fheight_t + 2);

	if (!showclock)
		t_h = lcd_height;

	display.draw_fill_rect(-1, 0, lcd_width, t_h, CLCDDisplay::PIXEL_OFF);
	//
	int xb = 1;
	int xs = 1;
	int yb = (t_h - fheight_r) / 2 + fheight_r - 2;
	int ys = 0;

	if (!small.empty())
	{
		yb = 1 + fheight_b - 3;
		ys = yb + fheight_s + 1;
	}

	int wb = (small.empty()) ? fonts.regular->getRenderWidth(cname[0].c_str(), big_utf8) : fonts.big->getRenderWidth(cname[0].c_str(), big_utf8);
	int ws = fonts.time->getRenderWidth(event[0].c_str(), small_utf8);

	if (centered)
	{
		xb = (lcd_width - wb) / 2;
		xs = (lcd_width - ws) / 2;
	}

	if (!small.empty())
	{
		fonts.big->RenderString(xb, yb, lcd_width, cname[0].c_str(), CLCDDisplay::PIXEL_ON, 0, big_utf8);
		fonts.small->RenderString(xs, ys, lcd_width, event[0].c_str(), CLCDDisplay::PIXEL_ON, 0, small_utf8);
	}
	else
		fonts.regular->RenderString(xb, yb, lcd_width, cname[0].c_str(), CLCDDisplay::PIXEL_ON, 0, big_utf8);

	if (perform_wakeup)
		wake_up();

	displayUpdate();
}

void CLCD::showServicename(const std::string name, const bool perform_wakeup)
{
	printf("CLCD::showServicename '%s' epg: '%s'\n", name.c_str(), epg_title.c_str());

	if (!name.empty())
		servicename = name;

	if (mode != MODE_TVRADIO)
		return;

	showTextScreen(servicename, epg_title, perform_wakeup, true);
	return;
}

void CLCD::setEPGTitle(const std::string title)
{
	if (title == epg_title)
	{
		//fprintf(stderr,"CLCD::setEPGTitle: not changed\n");
		return;
	}

	epg_title = title;
	showServicename("", false);
}

void CLCD::setMovieInfo(const AUDIOMODES playmode, const std::string big, const std::string small, const bool centered)
{
	movie_playmode = playmode;
	movie_big = big;
	movie_small = small;
	movie_centered = centered;

	if (mode != MODE_MOVIE)
		return;

	//showAudioPlayMode(movie_playmode);
	showTextScreen(movie_big, "", true, movie_centered);
}

void CLCD::setMovieAudio(const bool is_ac3)
{
	movie_is_ac3 = is_ac3;

	if (mode != MODE_MOVIE)
		return;

	showPercentOver(percentOver, true, MODE_MOVIE);
}

void CLCD::showTime()
{
	if (!has_lcd)
		return;

	if (showclock)
	{
		char timestr[21];
		struct timeval tm;
		struct tm *t;
		invert = !invert;
		gettimeofday(&tm, NULL);
		t = localtime(&tm.tv_sec);

		if (mode == MODE_STANDBY)
		{
			display.clear_screen(); // clear lcd
			//ShowNewClock(&display, t->tm_hour, t->tm_min, t->tm_sec, t->tm_wday, t->tm_mday, t->tm_mon, CNeutrinoApp::getInstance()->recordingstatus);

			if (!invert)
				strftime((char *) &timestr, 20, "%H:%M:%S\n%d.%m.%y", t);
			else
				strftime((char *) &timestr, 20, "%H.%M:%S\n%d.%m.%y", t);

			display.draw_fill_rect(0, 0, lcd_width, lcd_height, CLCDDisplay::PIXEL_OFF);
			fonts.time->RenderString(lcd_width / 4, lcd_height / 2, lcd_width, timestr, CLCDDisplay::PIXEL_ON);
		}
		else
		{
			if (CNeutrinoApp::getInstance()->recordingstatus && clearClock == 1)
			{
				if (!invert)
					strftime((char *) &timestr, 20, "%H:%M", t);
				else
					strftime((char *) &timestr, 20, "%H.%M", t);

				clearClock = 0;
			}
			else
			{
				if (!invert)
					strftime((char *) &timestr, 20, "%H:%M", t);
				else
					strftime((char *) &timestr, 20, "%H.%M", t);

				clearClock = 1;
			}

			display.draw_fill_rect(lcd_width - time_size - 1, lcd_height - 12, lcd_width, lcd_height, clearClock ? CLCDDisplay::PIXEL_OFF : CLCDDisplay::PIXEL_ON);
			fonts.time->RenderString(lcd_width - 4 - fonts.time->getRenderWidth(timestr), lcd_height - 2, time_size, timestr, clearClock ? CLCDDisplay::PIXEL_ON : CLCDDisplay::PIXEL_OFF);
			showPercentOver(getProgress(), true);
		}

		displayUpdate();
	}
}

void CLCD::showRCLock(int duration)
{
	if (!has_lcd)
		return;
}

void CLCD::showVolume(const char vol, const bool perform_update)
{
	if (!has_lcd)
		return;

	volume = vol;
	showTextScreen(servicename, "Volume", true, true);
	showPercentOver(vol, true);
	//showTextScreen(servicename, "", true, true);
	return;
}

void CLCD::showPercentOver(const unsigned char perc, const bool perform_update, const MODES m)
{
	if (!has_lcd)
		return;

	int left, top, width, height = 6;
	percentOver = perc;

	if (!showclock)
		return;

	left = 2;
	top = lcd_height - height - 1 - 2;
	width = lcd_width - left - 4 - time_size;
	display.draw_rectangle(left - 2, top - 2, left + width + 2, top + height + 1, CLCDDisplay::PIXEL_ON, CLCDDisplay::PIXEL_OFF);

	if (perc == (unsigned char) -1)
	{
		display.draw_line(left, top, left + width, top + height - 1, CLCDDisplay::PIXEL_ON);
	}
	else
	{
		int dp;

		if (perc == (unsigned char) -2)
			dp = width + 1;
		else
			dp = perc * (width + 1) / 100;

		display.draw_fill_rect(left - 1, top - 1, left + dp, top + height, CLCDDisplay::PIXEL_ON);

		if (perc == (unsigned char) -2)
		{
			// draw a "+" to show that the event is overdue
			display.draw_line(left + width - 2, top + 1, left + width - 2, top + height - 2, CLCDDisplay::PIXEL_OFF);
			display.draw_line(left + width - 1, top + (height / 2), left + width - 3, top + (height / 2), CLCDDisplay::PIXEL_OFF);
		}
	}

	if (perform_update)
		displayUpdate();
}

void CLCD::showMenuText(const int position, const char *text, const int highlight, const bool utf_encoded)
{
	if (!has_lcd)
		return;

	showTextScreen(text, "", true, true);
}

void CLCD::showAudioTrack(const std::string &artist, const std::string &title, const std::string &album)
{
	if (!has_lcd)
		return;
}

void CLCD::showAudioPlayMode(AUDIOMODES m)
{
	if (!has_lcd)
		return;

	display.draw_fill_rect(-1, 51, 10, 62, CLCDDisplay::PIXEL_OFF);

	switch (m)
	{
		case AUDIO_MODE_PLAY:
		{
			int x = 3, y = 53;
			display.draw_line(x, y, x, y + 8, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 1, y + 1, x + 1, y + 7, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 2, y + 2, x + 2, y + 6, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 3, y + 3, x + 3, y + 5, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 4, y + 4, x + 4, y + 4, CLCDDisplay::PIXEL_ON);
			break;
		}

		case AUDIO_MODE_STOP:
			display.draw_fill_rect(1, 53, 8, 61, CLCDDisplay::PIXEL_ON);
			break;

		case AUDIO_MODE_PAUSE:
			display.draw_line(1, 54, 1, 60, CLCDDisplay::PIXEL_ON);
			display.draw_line(2, 54, 2, 60, CLCDDisplay::PIXEL_ON);
			display.draw_line(6, 54, 6, 60, CLCDDisplay::PIXEL_ON);
			display.draw_line(7, 54, 7, 60, CLCDDisplay::PIXEL_ON);
			break;

		case AUDIO_MODE_FF:
		{
			int x = 2, y = 55;
			display.draw_line(x, y, x, y + 4, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 1, y + 1, x + 1, y + 3, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 2, y + 2, x + 2, y + 2, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 3, y, x + 3, y + 4, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 4, y + 1, x + 4, y + 3, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 5, y + 2, x + 5, y + 2, CLCDDisplay::PIXEL_ON);
		}
		break;

		case AUDIO_MODE_REV:
		{
			int x = 2, y = 55;
			display.draw_line(x, y + 2, x, y + 2, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 1, y + 1, x + 1, y + 3, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 2, y, x + 2, y + 4, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 3, y + 2, x + 3, y + 2, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 4, y + 1, x + 4, y + 3, CLCDDisplay::PIXEL_ON);
			display.draw_line(x + 5, y, x + 5, y + 4, CLCDDisplay::PIXEL_ON);
		}
		break;
	}

	wake_up();
	displayUpdate();
}

void CLCD::showAudioProgress(const char perc, bool isMuted)
{
	if (!has_lcd)
		return;

	if (mode == MODE_AUDIO)
	{
		display.draw_fill_rect(11, 53, 73, 61, CLCDDisplay::PIXEL_OFF);
		int dp = int(perc / 100.0 * 61.0 + 12.0);
		display.draw_fill_rect(11, 54, dp, 60, CLCDDisplay::PIXEL_ON);

		if (isMuted)
		{
			if (dp > 12)
			{
				display.draw_line(12, 56, dp - 1, 56, CLCDDisplay::PIXEL_OFF);
				display.draw_line(12, 58, dp - 1, 58, CLCDDisplay::PIXEL_OFF);
			}
			else
				display.draw_line(12, 55, 72, 59, CLCDDisplay::PIXEL_ON);
		}

		displayUpdate();
	}
}

void CLCD::setMode(const MODES m, const char *const title)
{
	if (!has_lcd)
		return;

	mode = m;
	menutitle = title;
	setlcdparameter();

	switch (m)
	{
		case MODE_TVRADIO:
		case MODE_WEBTV:
			display.clear_screen(); // clear lcd
			showclock = true;
			showServicename(servicename);
			showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
			break;

		case MODE_MOVIE:
			display.clear_screen(); // clear lcd
			showclock = false;
			setMovieInfo(movie_playmode, movie_big, movie_small, movie_centered);
			break;

		case MODE_AUDIO:
		{
			display.clear_screen(); // clear lcd
			showAudioPlayMode(AUDIO_MODE_STOP);
			showVolume(volume, false);
			showclock = true;
			showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
			break;
		}

		case MODE_AVINPUT:
			display.clear_screen(); // clear lcd
			showVolume(volume, false);
			showclock = true;
			showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
			break;

		case MODE_MENU_UTF8:
			showclock = false;
			display.clear_screen(); // clear lcd
			displayUpdate();
			break;

		case MODE_SHUTDOWN:
			showclock = false;
			display.clear_screen(); // clear lcd
			displayUpdate();
			break;

		case MODE_STANDBY:
			showclock = true;
			showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
			/* "showTime()" clears the whole lcd in MODE_STANDBY                         */
			break;
	}

	wake_up();
}


void CLCD::setBrightness(int bright)
{
	g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CLCD::getBrightness()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
}

void CLCD::setBrightnessStandby(int bright)
{
	g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CLCD::getBrightnessStandby()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS];
}

void CLCD::setContrast(int contrast)
{
	g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST] = contrast;
	setlcdparameter();
}

int CLCD::getContrast()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST];
}

void CLCD::setScrollMode(int scroll_repeats)
{
	printf("CLCD::%s scroll_repeats:%d\n", __func__, scroll_repeats);
}

void CLCD::setPower(int power)
{
	g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER] = power;
	setlcdparameter();
}

int CLCD::getPower()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];
}

void CLCD::togglePower(void)
{
	last_toggle_state_power = 1 - last_toggle_state_power;
	setlcdparameter((mode == MODE_STANDBY) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] : g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS],
			g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST],
			last_toggle_state_power,
			g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE],
			0 /*g_settings.lcd_setting[SNeutrinoSettings::LCD_BIAS]*/);
}

void CLCD::setInverse(int inverse)
{
	g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE] = inverse;
	setlcdparameter();
}

int CLCD::getInverse()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE];
}

void CLCD::setAutoDimm(int /*autodimm*/)
{
}

int CLCD::getAutoDimm()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_AUTODIMM];
}

void CLCD::setMuted(bool mu)
{
	muted = mu;
	showVolume(volume);
}

void CLCD::resume()
{
	if (!has_lcd)
		return;

	display.resume();
}

void CLCD::pause()
{
	if (!has_lcd)
		return;

	display.pause();
}

void CLCD::UpdateIcons()
{
}

void CLCD::ShowIcon(vfd_icon icon, bool show)
{
}

void CLCD::ShowIcon(fp_icon i, bool on)
{
	vfd_icon icon = (vfd_icon) i;
	ShowIcon(icon, on);
}

void CLCD::Lock()
{
}

void CLCD::Unlock()
{
}

void CLCD::Clear()
{
	if (!has_lcd)
		return;

	if (mode == MODE_SHUTDOWN)
	{
		display.clear_screen(); // clear lcd
		displayUpdate();
	}

	return;
}

bool CLCD::ShowPng(char *filename)
{
	if (!has_lcd)
		return false;

	return display.load_png(filename);
}

bool CLCD::DumpPng(char *filename)
{
	if (!has_lcd)
		return false;

	return display.dump_png(filename);
}
