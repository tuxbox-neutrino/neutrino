/*
	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2008 Novell, Inc. Author: Stefan Seyfried
		  (C) 2009-2013 Stefan Seyfried

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

#include <driver/newclock.h>
#include <lcddisplay/lcddisplay.h>
#include <gui/widget/icons.h>

#if defined HAVE_DBOX_HARDWARE || defined HAVE_DREAMBOX_HARDWARE || defined HAVE_IPBOX_HARDWARE
#include <dbox/fp.h>
#endif
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <system/set_threadname.h>
#include <daemonc/remotecontrol.h>
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

/* from edvbstring.cpp */
static bool isUTF8(const std::string &string)
{
	unsigned int len=string.size();

	for (unsigned int i=0; i < len; ++i)
	{
		if (!(string[i]&0x80)) // normal ASCII
			continue;
		if ((string[i] & 0xE0) == 0xC0) // one char following.
		{
			// first, length check:
			if (i+1 >= len)
				return false; // certainly NOT utf-8
			i++;
			if ((string[i]&0xC0) != 0x80)
				return false; // no, not UTF-8.
		} else if ((string[i] & 0xF0) == 0xE0)
		{
			if ((i+1) >= len)
				return false;
			i++;
			if ((string[i]&0xC0) != 0x80)
				return false;
			i++;
			if ((string[i]&0xC0) != 0x80)
				return false;
		}
	}
	return true; // can be UTF8 (or pure ASCII, at least no non-UTF-8 8bit characters)
}

CLCD::CLCD()
{
#ifdef LCD_UPDATE
	m_fileList = NULL;
	m_fileListPos = 0;
	m_fileListHeader = "";
	m_infoBoxText = "";
	m_infoBoxAutoNewline = 0;
	m_progressShowEscape = 0;
	m_progressHeaderGlobal = "";
	m_progressHeaderLocal = "";
	m_progressGlobal = 0;
	m_progressLocal = 0;
#endif // LCD_UPDATE
	muted = false;
	percentOver = 0;
	volume = 0;
	timeout_cnt = 0;
	/* this is a hack: the only place where this is checked is the menu to decide
	 * if display settings are to be applied.
	 * we have nothing to configure anyway, so setting has_lcd = false hides that menu.
	 */
	has_lcd = false;
	clearClock = 0;
	fontRenderer = NULL;
	thread_started = false;
}

CLCD::~CLCD()
{
	if (thread_started) {
		thread_started = false;
		pthread_join(thrTime, NULL);
	}
	delete fontRenderer;
}

CLCD* CLCD::getInstance()
{
	static CLCD* lcdd = NULL;
	if(lcdd == NULL)
	{
		lcdd = new CLCD();
	}
	/* HACK */
	g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] = 2;
	return lcdd;
}

void CLCD::count_down() {
	if (timeout_cnt > 0) {
		timeout_cnt--;
		if (timeout_cnt == 0) {
			setlcdparameter();
		}
	} 
}

void CLCD::wake_up() {
	if (atoi(g_settings.lcd_setting_dim_time) > 0) {
		timeout_cnt = atoi(g_settings.lcd_setting_dim_time);
		setlcdparameter();
	}
}

#ifndef BOXMODEL_DM500
void* CLCD::TimeThread(void *p)
{
	((CLCD *)p)->thread_started = true;
	set_threadname("lcd:time");
	while (((CLCD *)p)->thread_started)
	{
		sleep(1);
		struct stat buf;
		if (stat("/tmp/lcd.locked", &buf) == -1) {
			CLCD::getInstance()->showTime();
			CLCD::getInstance()->count_down();
		} else
			CLCD::getInstance()->wake_up();
	}
	printf("CLCD::TimeThread exit\n");
	return NULL;
}
#else
// there is no LCD on DM500, so let's use the timethread for blinking the LED during record
void* CLCD::TimeThread(void *)
{
	int led = 0;
	int old_led = 0;
	int led_fd = open("/dev/dbox/fp0", O_RDWR);

	if (led_fd < 0)
	{
		perror("CLCD::TimeThread: /dev/dbox/fp0");
		return NULL;
	}
	// printf("CLCD:TimeThread dm500 led_fd: %d\n",led_fd);
	while(1)
	{
		sleep(1);
		if (CNeutrinoApp::getInstance()->recordingstatus)
			led = !led;
		else
			led = (CLCD::getInstance()->mode == MODE_STANDBY);

		if (led != old_led) {
			//printf("CLCD:TimeThread ioctl(led_fd,11, &%d)\n",led);
			ioctl(led_fd, 11, &led);
			old_led = led;
		}
	}
	return NULL;
}
#endif

void CLCD::init(const char * fontfile, const char * fontname,
                const char * fontfile2, const char * fontname2,
                const char * fontfile3, const char * fontname3)
{
	InitNewClock();

	if (!lcdInit(fontfile, fontname, fontfile2, fontname2, fontfile3, fontname3 ))
	{
		printf("[lcdd] LCD-Init failed!\n");
		has_lcd = false;
#ifndef BOXMODEL_DM500
		// on the dm500, we need the timethread for the front LEDs
		return;
#endif
	}

	if (pthread_create (&thrTime, NULL, TimeThread, this) != 0 )
	{
		perror("[lcdd]: pthread_create(TimeThread)");
		return ;
	}
}

enum backgrounds {
	BACKGROUND_SETUP = 0,
	BACKGROUND_POWER = 1,
	BACKGROUND_LCD2  = 2,
	BACKGROUND_LCD3  = 3,
	BACKGROUND_LCD   = 4
//	BACKGROUND_LCD4  = 5
};
const char * const background_name[LCD_NUMBER_OF_BACKGROUNDS] = {
	"setup",
	"power",
	"lcd2",
	"lcd3",
	"lcd"
};
#define NUMBER_OF_PATHS 2
const char * const background_path[NUMBER_OF_PATHS] = {
	LCDDIR_VAR ,
	DATADIR "/lcdd/icons/"
};

bool CLCD::lcdInit(const char * fontfile, const char * fontname,
		   const char * fontfile2, const char * fontname2,
		   const char * fontfile3, const char * fontname3)
{
	fontRenderer = new LcdFontRenderClass(&display);
	const char * style_name = fontRenderer->AddFont(fontfile);
	const char * style_name2;
	const char * style_name3;

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
	fontRenderer->InitFontCache();

	fonts.menu        = fontRenderer->getFont(fontname,  style_name , 12);
	fonts.time        = fontRenderer->getFont(fontname2, style_name2, 14);
	fonts.channelname = fontRenderer->getFont(fontname3, style_name3, 15);
	fonts.menutitle   = fonts.channelname;

	setAutoDimm(g_settings.lcd_setting[SNeutrinoSettings::LCD_AUTODIMM]);

	if (!display.isAvailable())
	{
		printf("[lcdd] exit...(no lcd-support)\n");
		return false;
	}

	for (int i = 0; i < LCD_NUMBER_OF_BACKGROUNDS; i++)
	{
		for (int j = 0; j < NUMBER_OF_PATHS; j++)
		{
			std::string file = background_path[j];
			file += background_name[i];
			file += ".png";
			if (display.load_png(file.c_str()))
				goto found;
		}
		printf("[neutrino/lcd] no valid %s background.\n", background_name[i]);
		return false;
	found:
		display.dump_screen(&(background[i]));
	}

	setMode(MODE_TVRADIO);

	return true;
}

void CLCD::displayUpdate()
{
	struct stat buf;
	if (stat("/tmp/lcd.locked", &buf) == -1)
		display.update();
}

void CLCD::setlcdparameter(int dimm, const int contrast, const int power, const int inverse, const int bias)
{
#if defined HAVE_DBOX_HARDWARE || defined HAVE_DREAMBOX_HARDWARE || defined HAVE_IPBOX_HARDWARE
	if (!display.isAvailable())
		return;
	int fd;
	if (power == 0)
		dimm = 0;

	if ((fd = open("/dev/dbox/fp0", O_RDWR)) == -1)
	{
		perror("[lcdd] open '/dev/dbox/fp0' failed");
	}
	else
	{
		if (ioctl(fd, FP_IOCTL_LCD_DIMM, &dimm) < 0)
		{
			perror("[lcdd] set dimm failed!");
		}

		close(fd);
	}
	
	if ((fd = open("/dev/dbox/lcd0", O_RDWR)) == -1)
	{
		perror("[lcdd] open '/dev/dbox/lcd0' failed");
	}
	else
	{
		if (ioctl(fd, LCD_IOCTL_SRV, &contrast) < 0)
		{
			perror("[lcdd] set contrast failed!");
		}

		if (ioctl(fd, LCD_IOCTL_ON, &power) < 0)
		{
			perror("[lcdd] set power failed!");
		}

		if (ioctl(fd, LCD_IOCTL_REVERSE, &inverse) < 0)
		{
			perror("[lcdd] set invert failed!");
		}

#if 0
		if (g_info.box_Type == CControld::TUXBOX_MAKER_PHILIPS) 
		{
			if (ioctl(fd, LCD_IOCTL_BIAS, &bias) < 0)
			{
				perror("[lcdd] set bias failed!");
			}
		}
#endif
		close(fd);
	}
#endif
}

void CLCD::setlcdparameter(void)
{
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

static std::string removeLeadingSpaces(const std::string & text)
{
	int pos = text.find_first_not_of(" ");

	if (pos != -1)
		return text.substr(pos);

	return text;
}

static std::string splitString(const std::string & text, const int maxwidth, LcdFont *font, bool dumb, bool utf8)
{
	int pos;
	std::string tmp = removeLeadingSpaces(text);

	if (font->getRenderWidth(tmp, utf8) > maxwidth)
	{
		do
		{
			if (dumb)
				tmp = tmp.substr(0, tmp.length()-1);
			else
			{
				pos = tmp.find_last_of("[ .-]+"); // TODO characters might be UTF-encoded!
				if (pos != -1)
					tmp = tmp.substr(0, pos);
				else // does not fit -> fall back to dumb split
					tmp = tmp.substr(0, tmp.length()-1);
			}
		} while (font->getRenderWidth(tmp, utf8) > maxwidth);
	}

	return tmp;
}

/* display "big" and "small" text.
   TODO: right now, "big" is hardcoded as utf-8, small is not (for EPG)
 */
void CLCD::showTextScreen(const std::string & big, const std::string & small, const int showmode, const bool perform_wakeup, const bool centered)
{
	/* the "showmode" variable is a bit map:
		0x01	show "big" string
		0x02	show "small" string
		0x04	show separator line if big and small are present / shown
		0x08	show only one line of "big" string
	 */

	/* draw_fill_rect is braindead: it actually fills _inside_ the described rectangle,
	   so that you have to give it one pixel additionally in every direction ;-(
	   this is where the "-1 to 120" intead of "0 to 119" comes from */
	display.draw_fill_rect(-1, 10, LCD_COLS, 51, CLCDDisplay::PIXEL_OFF);

	bool big_utf8 = false;
	bool small_utf8 = false;
	std::string cname[2];
	std::string event[4];
	int namelines = 0, eventlines = 0, maxnamelines = 2;
	if (showmode & 8)
		maxnamelines = 1;

	if ((showmode & 1) && !big.empty())
	{
		bool dumb = false;
		big_utf8 = isUTF8(big);
		while (true)
		{
			namelines = 0;
			std::string title = big;
			do { // first try "intelligent" splitting
				cname[namelines] = splitString(title, LCD_COLS, fonts.channelname, dumb, big_utf8);
				title = removeLeadingSpaces(title.substr(cname[namelines].length()));
				namelines++;
			} while (!title.empty() && namelines < maxnamelines);
			if (title.empty())
				break;
			dumb = !dumb;	// retry with dumb splitting;
			if (!dumb)	// second retry -> get out;
				break;
		}
	}

	// one nameline => 2 eventlines, 2 namelines => 1 eventline
	int maxeventlines = 4 - (namelines * 3 + 1) / 2;

	if ((showmode & 2) && !small.empty())
	{
		bool dumb = false;
		small_utf8 = isUTF8(small);
		while (true)
		{
			eventlines = 0;
			std::string title = small;
			do { // first try "intelligent" splitting
				event[eventlines] = splitString(title, LCD_COLS, fonts.menu, dumb, small_utf8);
				title = removeLeadingSpaces(title.substr(event[eventlines].length()));
				/* DrDish TV appends a 0x0a to the EPG title. We could strip it in sectionsd...
				   ...instead, strip all control characters at the end of the text for now */
				if (!event[eventlines].empty() && event[eventlines].at(event[eventlines].length() - 1) < ' ')
					event[eventlines].erase(event[eventlines].length() - 1);
				eventlines++;
			} while (!title.empty() && eventlines < maxeventlines);
			if (title.empty())
				break;
			dumb = !dumb;	// retry with dumb splitting;
			if (!dumb)	// second retry -> get out;
				break;
		}
	}

	/* this values were determined empirically */
	int y = 8 + (41 - namelines*14 - eventlines*10)/2;
	int x = 1;
	for (int i = 0; i < namelines; i++) {
		y += 14;
		if (centered)
		{
			int w = fonts.channelname->getRenderWidth(cname[i], big_utf8);
			x = (LCD_COLS - w) / 2;
		}
		fonts.channelname->RenderString(x, y, LCD_COLS + 10, cname[i].c_str(), CLCDDisplay::PIXEL_ON, 0, big_utf8);
	}
	y++;
	if (eventlines > 0 && namelines > 0 && showmode & 4)
	{
		y++;
		display.draw_line(0, y, LCD_COLS - 1, y, CLCDDisplay::PIXEL_ON);
	}
	if (eventlines > 0)
	{
		for (int i = 0; i < eventlines; i++) {
			y += 10;
			if (centered)
			{
				int w = fonts.menu->getRenderWidth(event[i], small_utf8);
				x = (LCD_COLS - w) / 2;
			}
			fonts.menu->RenderString(x, y, LCD_COLS + 10, event[i].c_str(), CLCDDisplay::PIXEL_ON, 0, small_utf8);
		}
	}

	if (perform_wakeup)
		wake_up();

	displayUpdate();
}

void CLCD::showServicename(const std::string name, const bool clear_epg)
{
	/*
	   1 = show servicename
	   2 = show epg title
	   4 = draw separator line between name and EPG
	 */
	int showmode = g_settings.lcd_setting[SNeutrinoSettings::LCD_EPGMODE];
	/* HACK */
	showmode = 7;

	//printf("CLCD::showServicename '%s' epg: '%s'\n", name.c_str(), epg_title.c_str());

	if (!name.empty())
		servicename = name;
	if (clear_epg)
		epg_title.clear();

	if (mode != MODE_TVRADIO)
		return;

	showTextScreen(servicename, epg_title, showmode, true, true);
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
	int showmode = g_settings.lcd_setting[SNeutrinoSettings::LCD_EPGMODE];
	showmode = 7;
	showmode |= 3; // take only the separator line from the config

	movie_playmode = playmode;
	movie_big = big;
	movie_small = small;
	movie_centered = centered;

	if (mode != MODE_MOVIE)
		return;

	showAudioPlayMode(movie_playmode);
	showTextScreen(movie_big, movie_small, showmode, true, movie_centered);
}

void CLCD::setMovieAudio(const bool is_ac3)
{
	movie_is_ac3 = is_ac3;

	if (mode != MODE_MOVIE)
		return;

	showPercentOver(percentOver, true, MODE_MOVIE);
}

void CLCD::showTime(bool)
{
	if (showclock)
	{
		char timestr[21];
		struct timeval tm;
		struct tm * t;

		gettimeofday(&tm, NULL);
		t = localtime(&tm.tv_sec);

		if (mode == MODE_STANDBY)
		{
			display.draw_fill_rect(-1, -1, LCD_COLS, 64, CLCDDisplay::PIXEL_OFF); // clear lcd
			ShowNewClock(&display, t->tm_hour, t->tm_min, t->tm_sec, t->tm_wday, t->tm_mday, t->tm_mon, CNeutrinoApp::getInstance()->recordingstatus);
		}
		else
		{
			if (CNeutrinoApp::getInstance ()->recordingstatus && clearClock == 1)
			{
				strcpy(timestr,"  :  ");
				clearClock = 0;
			}
			else
			{
				strftime(timestr, 20, "%H:%M", t);
				clearClock = 1;
			}

			display.draw_fill_rect (77, 50, LCD_COLS, 64, CLCDDisplay::PIXEL_OFF);

			fonts.time->RenderString(122 - fonts.time->getRenderWidth(timestr), 62, 50, timestr, CLCDDisplay::PIXEL_ON);
		}
		displayUpdate();
	}
}

void CLCD::showRCLock(int duration)
{
	std::string icon = DATADIR "/lcdd/icons/rclock.raw";
	raw_display_t curr_screen;

	// Saving the whole screen is not really nice since the clock is updated
	// every second. Restoring the screen can cause a short travel to the past ;)
	display.dump_screen(&curr_screen);
	display.draw_fill_rect (-1, 10, LCD_COLS, 50, CLCDDisplay::PIXEL_OFF);
	display.paintIcon(icon,44,25,false);
	wake_up();
	displayUpdate();
	sleep(duration);
	display.load_screen(&curr_screen);
	wake_up();
	displayUpdate();
}

void CLCD::showVolume(const char vol, const bool perform_update)
{
	volume = vol;
	if (
	    ((mode == MODE_TVRADIO) && (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME])) ||
	    ((mode == MODE_MOVIE) && (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME])) ||
	    (mode == MODE_SCART) ||
	    (mode == MODE_AUDIO)
	    )
	{
		display.draw_fill_rect (11,53,73,61, CLCDDisplay::PIXEL_OFF);
		//strichlin
		if ((muted) || (volume==0))
		{
			display.draw_line (12,55,72,59, CLCDDisplay::PIXEL_ON);
		}
		else
		{
			int dp = vol*61/100+12;
			display.draw_fill_rect (11,54,dp,60, CLCDDisplay::PIXEL_ON);
		}
		if(mode == MODE_AUDIO)
		{
			display.draw_fill_rect (-1, 51, 10, 62, CLCDDisplay::PIXEL_OFF);
			display.draw_rectangle ( 1, 55,  3, 58, CLCDDisplay::PIXEL_ON, CLCDDisplay::PIXEL_OFF);
			display.draw_line      ( 3, 55,  6, 52, CLCDDisplay::PIXEL_ON);
			display.draw_line      ( 3, 58,  6, 61, CLCDDisplay::PIXEL_ON);
			display.draw_line      ( 6, 54,  6, 59, CLCDDisplay::PIXEL_ON);
		}

		if (perform_update)
		  displayUpdate();
	}
	wake_up();
}

void CLCD::showPercentOver(const unsigned char perc, const bool perform_update, const MODES m)
{
	if (mode != m)
		return;

	int left, top, width, height = 5;
	bool draw = true;
	percentOver = perc;
	if (mode == MODE_TVRADIO || mode == MODE_MOVIE)
	{
		if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 0)
		{
			left = 12; top = 55; width = 60;
		}
		else if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 2)
		{
			left = 12; top = 3; width = 104;
		}
		else if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 3)
		{
			left = 12; top = 3; width = 84;

			if ((g_RemoteControl != NULL && mode == MODE_TVRADIO) || mode == MODE_MOVIE)
			{
				bool is_ac3;
				if (mode == MODE_MOVIE)
					is_ac3 = movie_is_ac3;
				else
				{
					uint count = g_RemoteControl->current_PIDs.APIDs.size();
					if ((g_RemoteControl->current_PIDs.PIDs.selected_apid < count) &&
					    (g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].is_ac3))
						is_ac3 = true;
					else
						is_ac3 = false;
				}

				const char * icon;
				if (is_ac3)
					icon = DATADIR "/lcdd/icons/dd.raw";
				else
					icon = DATADIR "/lcdd/icons/stereo.raw";

				display.paintIcon(icon, 101, 1, false);
			}
		}
		else
			draw = false;

		if (draw)
		{
			display.draw_fill_rect (left-1, top-1, left+width+1, top+height, CLCDDisplay::PIXEL_OFF);
			if (perc == (unsigned char) -1)
			{
				display.draw_line (left, top, left+width, top+height-1, CLCDDisplay::PIXEL_ON);
			}
			else
			{
				int dp;
				if (perc == (unsigned char) -2)
					dp = width+1;
				else
					dp = perc * (width + 1) / 100;
				display.draw_fill_rect (left-1, top-1, left+dp, top+height, CLCDDisplay::PIXEL_ON);

				if (perc == (unsigned char) -2)
				{
					// draw a "+" to show that the event is overdue
					display.draw_line(left+width-2, top+1, left+width-2, top+height-2, CLCDDisplay::PIXEL_OFF);
					display.draw_line(left+width-1, top+(height/2), left+width-3, top+(height/2), CLCDDisplay::PIXEL_OFF);
				}
			}
		}

		if (perform_update)
			displayUpdate();
	}
}

void CLCD::showMenuText(const int position, const char * text, const int highlight, const bool utf_encoded)
{
	/* hack, to not have to patch too much in movieplayer.cpp */
	if (mode == MODE_MOVIE) {
		size_t p;
		AUDIOMODES m = movie_playmode;
		std::string mytext = text;
		if (mytext.find("> ") == 0) {
			mytext = mytext.substr(2);
			m = AUDIO_MODE_PLAY;
		} else if (mytext.find("|| ") == 0) {
			mytext = mytext.substr(3);
			m = AUDIO_MODE_PAUSE;
		} else if ((p = mytext.find("s||> ")) < 3) {
			mytext = mytext.substr(p + 5);
			m = AUDIO_MODE_PLAY;
		} else if ((p = mytext.find("x>> ")) < 3) {
			mytext = mytext.substr(p + 4);
			m = AUDIO_MODE_FF;
		} else if ((p = mytext.find("x<< ")) < 3) {
			mytext = mytext.substr(p + 4);
			m = AUDIO_MODE_REV;
		}
		setMovieInfo(m, "", mytext, false);
		return;
	}

	if (mode != MODE_MENU_UTF8)
		return;

	// reload specified line
	display.draw_fill_rect(-1, 35+14*position, LCD_COLS, 35+14+14*position, CLCDDisplay::PIXEL_OFF);
	fonts.menu->RenderString(0,35+11+14*position, LCD_COLS + 20, text, CLCDDisplay::PIXEL_INV, highlight, utf_encoded);
	wake_up();
	displayUpdate();

}

void CLCD::showAudioTrack(const std::string & artist, const std::string & title, const std::string & album)
{
	if (mode != MODE_AUDIO) 
	{
		return;
	}

	// reload specified line
	display.draw_fill_rect (-1, 10, LCD_COLS, 24, CLCDDisplay::PIXEL_OFF);
	display.draw_fill_rect (-1, 20, LCD_COLS, 37, CLCDDisplay::PIXEL_OFF);
	display.draw_fill_rect (-1, 33, LCD_COLS, 50, CLCDDisplay::PIXEL_OFF);
	fonts.menu->RenderString(0, 22, LCD_COLS + 5, artist.c_str(), CLCDDisplay::PIXEL_ON, 0, isUTF8(artist));
	fonts.menu->RenderString(0, 35, LCD_COLS + 5, album.c_str(),  CLCDDisplay::PIXEL_ON, 0, isUTF8(album));
	fonts.menu->RenderString(0, 48, LCD_COLS + 5, title.c_str(),  CLCDDisplay::PIXEL_ON, 0, isUTF8(title));
	wake_up();
	displayUpdate();

}

void CLCD::showAudioPlayMode(AUDIOMODES m)
{
	display.draw_fill_rect (-1,51,10,62, CLCDDisplay::PIXEL_OFF);
	switch(m)
	{
		case AUDIO_MODE_PLAY:
			{
				int x=3,y=53;
				display.draw_line(x  ,y  ,x  ,y+8, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+1,y+1,x+1,y+7, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+2,y+2,x+2,y+6, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+3,y+3,x+3,y+5, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+4,y+4,x+4,y+4, CLCDDisplay::PIXEL_ON);
				break;
			}
		case AUDIO_MODE_STOP:
			display.draw_fill_rect (1, 53, 8 ,61, CLCDDisplay::PIXEL_ON);
			break;
		case AUDIO_MODE_PAUSE:
			display.draw_line(1,54,1,60, CLCDDisplay::PIXEL_ON);
			display.draw_line(2,54,2,60, CLCDDisplay::PIXEL_ON);
			display.draw_line(6,54,6,60, CLCDDisplay::PIXEL_ON);
			display.draw_line(7,54,7,60, CLCDDisplay::PIXEL_ON);
			break;
		case AUDIO_MODE_FF:
			{
				int x=2,y=55;
				display.draw_line(x   ,y   , x  , y+4, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+1 ,y+1 , x+1, y+3, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+2 ,y+2 , x+2, y+2, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+3 ,y   , x+3, y+4, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+4 ,y+1 , x+4, y+3, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+5 ,y+2 , x+5, y+2, CLCDDisplay::PIXEL_ON);
			}
			break;
		case AUDIO_MODE_REV:
			{
				int x=2,y=55;
				display.draw_line(x   ,y+2 , x  , y+2, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+1 ,y+1 , x+1, y+3, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+2 ,y   , x+2, y+4, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+3 ,y+2 , x+3, y+2, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+4 ,y+1 , x+4, y+3, CLCDDisplay::PIXEL_ON);
				display.draw_line(x+5 ,y   , x+5, y+4, CLCDDisplay::PIXEL_ON);
			}
			break;
	}
	wake_up();
	displayUpdate();
}

void CLCD::showAudioProgress(const char perc, bool isMuted)
{
	if (mode == MODE_AUDIO)
	{
		display.draw_fill_rect (11,53,73,61, CLCDDisplay::PIXEL_OFF);
		int dp = perc * 61 / 100 + 12;
		display.draw_fill_rect (11,54,dp,60, CLCDDisplay::PIXEL_ON);
		if(isMuted)
		{
			if(dp > 12)
			{
				display.draw_line(12, 56, dp-1, 56, CLCDDisplay::PIXEL_OFF);
				display.draw_line(12, 58, dp-1, 58, CLCDDisplay::PIXEL_OFF);
			}
			else
				display.draw_line (12,55,72,59, CLCDDisplay::PIXEL_ON);
		}
		displayUpdate();
	}
}

void CLCD::setMode(const MODES m, const char * const title)
{
	mode = m;
	menutitle = title;
	setlcdparameter();

	switch (m)
	{
	case MODE_TVRADIO:
	case MODE_MOVIE:
		switch (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME])
		{
		case 0:
			display.load_screen(&(background[BACKGROUND_LCD2]));
			showPercentOver(percentOver, false, mode);
			break;
		case 1:
			display.load_screen(&(background[BACKGROUND_LCD]));
			showVolume(volume, false);
			break;
		case 2:
			display.load_screen(&(background[BACKGROUND_LCD3]));
			showVolume(volume, false);
			showPercentOver(percentOver, false, mode);
			break;
		case 3:
			display.load_screen(&(background[BACKGROUND_LCD3]));
			showVolume(volume, false);
			showPercentOver(percentOver, false, mode);
			break;
		default:
			break;
		}
		if (mode == MODE_TVRADIO)
			showServicename(servicename);
		else
		{
			setMovieInfo(movie_playmode, movie_big, movie_small, movie_centered);
			setMovieAudio(movie_is_ac3);
		}
		showclock = true;
		showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		break;
	case MODE_AUDIO:
	{
		display.load_screen(&(background[BACKGROUND_LCD]));
		display.draw_fill_rect(0, 14, LCD_COLS, 48, CLCDDisplay::PIXEL_OFF);
		
		showAudioPlayMode(AUDIO_MODE_STOP);
		showVolume(volume, false);
		showclock = true;
		showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		break;
	}
	case MODE_SCART:
		display.load_screen(&(background[BACKGROUND_LCD]));
		showVolume(volume, false);
		showclock = true;
		showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		break;
	case MODE_MENU_UTF8:
		showclock = false;
		display.load_screen(&(background[BACKGROUND_SETUP]));
		fonts.menutitle->RenderString(0, 28, LCD_COLS + 20, title, CLCDDisplay::PIXEL_ON);
		displayUpdate();
		break;
	case MODE_SHUTDOWN:
		showclock = false;
		display.load_screen(&(background[BACKGROUND_POWER]));
		displayUpdate();
		break;
	case MODE_STANDBY:
		showclock = true;
		showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		                 /* "showTime()" clears the whole lcd in MODE_STANDBY                         */
		break;
#ifdef LCD_UPDATE
	case MODE_FILEBROWSER:
		showclock = true;
		display.draw_fill_rect(-1, -1, LCD_COLS, 64, CLCDDisplay::PIXEL_OFF); // clear lcd
		showFilelist();
		break;
	case MODE_PROGRESSBAR:
		showclock = false;
		display.load_screen(&(background[BACKGROUND_SETUP]));
		showProgressBar();
		break;
	case MODE_PROGRESSBAR2:
		showclock = false;
		display.load_screen(&(background[BACKGROUND_SETUP]));
		showProgressBar2();
		break;
	case MODE_INFOBOX:
		showclock = false;
		showInfoBox();
		break;
#endif // LCD_UPDATE
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

#ifdef HAVE_DBOX_HARDWARE
void CLCD::setAutoDimm(int autodimm)
{
	int fd;
	g_settings.lcd_setting[SNeutrinoSettings::LCD_AUTODIMM] = autodimm;

	if ((fd = open("/dev/dbox/fp0", O_RDWR)) == -1)
	{
		perror("[lcdd] open '/dev/dbox/fp0' failed");
	}
	else
	{
		if( ioctl(fd, FP_IOCTL_LCD_AUTODIMM, &autodimm) < 0 )
		{
			perror("[lcdd] set autodimm failed!");
		}

		close(fd);
	}
}
#else
void CLCD::setAutoDimm(int /*autodimm*/)
{
}
#endif

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
	display.resume();
}

void CLCD::pause()
{
	display.pause();
}

void CLCD::ShowIcon(fp_icon icon, bool show)
{
	fprintf(stderr, "CLCD::ShowIcon(%d, %d)\n", icon, show);
}

void CLCD::Lock()
{
/*
	if(!has_lcd) return;
	creat("/tmp/vfd.locked", 0);
*/
}

void CVFD::Unlock()
{
/*
	if(!has_lcd) return;
	unlink("/tmp/vfd.locked");
*/
}

void CLCD::Clear()
{
	return;
}

#ifdef LCD_UPDATE
/*****************************************************************************************/
// showInfoBox
/*****************************************************************************************/
#define LCD_WIDTH LCD_COLS
#define LCD_HEIGTH 64

#define EPG_INFO_FONT_HEIGHT 9
#define EPG_INFO_SHADOW_WIDTH 1
#define EPG_INFO_LINE_WIDTH 1
#define EPG_INFO_BORDER_WIDTH 2

#define EPG_INFO_WINDOW_POS 4
#define EPG_INFO_LINE_POS 	EPG_INFO_WINDOW_POS + EPG_INFO_SHADOW_WIDTH
#define EPG_INFO_BORDER_POS EPG_INFO_WINDOW_POS + EPG_INFO_SHADOW_WIDTH + EPG_INFO_LINE_WIDTH
#define EPG_INFO_TEXT_POS 	EPG_INFO_WINDOW_POS + EPG_INFO_SHADOW_WIDTH + EPG_INFO_LINE_WIDTH + EPG_INFO_BORDER_WIDTH

#define EPG_INFO_TEXT_WIDTH LCD_WIDTH - (2*EPG_INFO_WINDOW_POS)

// timer 0: OFF, timer>0 time to show in seconds,  timer>=999 endless
void CLCD::showInfoBox(const char * const title, const char * const text ,int autoNewline,int timer)
{
	//printf("[lcdd] Info: \n");
	if(text != NULL)
		m_infoBoxText = text;
	if(title != NULL)
		m_infoBoxTitle = title;
	if(timer != -1)
		m_infoBoxTimer = timer;
	if(autoNewline != -1)
		m_infoBoxAutoNewline = autoNewline;

	//printf("[lcdd] Info: %s,%s,%d,%d\n",m_infoBoxTitle.c_str(),m_infoBoxText.c_str(),m_infoBoxAutoNewline,m_infoBoxTimer);
	if( mode == MODE_INFOBOX &&
	    !m_infoBoxText.empty())
	{
		// paint empty box
		display.draw_fill_rect (EPG_INFO_WINDOW_POS, EPG_INFO_WINDOW_POS, 	LCD_WIDTH-EPG_INFO_WINDOW_POS+1, 	  LCD_HEIGTH-EPG_INFO_WINDOW_POS+1,    CLCDDisplay::PIXEL_OFF);
		display.draw_fill_rect (EPG_INFO_LINE_POS, 	 EPG_INFO_LINE_POS, 	LCD_WIDTH-EPG_INFO_LINE_POS-1, 	  LCD_HEIGTH-EPG_INFO_LINE_POS-1, 	 CLCDDisplay::PIXEL_ON);
		display.draw_fill_rect (EPG_INFO_BORDER_POS, EPG_INFO_BORDER_POS, 	LCD_WIDTH-EPG_INFO_BORDER_POS-3,  LCD_HEIGTH-EPG_INFO_BORDER_POS-3, CLCDDisplay::PIXEL_OFF);

		// paint title
		if(!m_infoBoxTitle.empty())
		{
			int width = fonts.menu->getRenderWidth(m_infoBoxTitle);
			if(width > LCD_COLS - 20)
				width = LCD_COLS - 20;
			int start_pos = (LCD_COLS - width) /2;
			display.draw_fill_rect (start_pos, EPG_INFO_WINDOW_POS-4, 	start_pos+width+5, 	  EPG_INFO_WINDOW_POS+10,    CLCDDisplay::PIXEL_OFF);
			fonts.menu->RenderString(start_pos+4,EPG_INFO_WINDOW_POS+5, width+5, m_infoBoxTitle.c_str(), CLCDDisplay::PIXEL_ON);
		}

		// paint info 
		std::string text_line;
		int line;
		int pos = 0;
		int length = m_infoBoxText.size();
		for(line = 0; line < 5; line++)
		{
			text_line.clear();
			while ( m_infoBoxText[pos] != '\n' &&
					((fonts.menu->getRenderWidth(text_line) < EPG_INFO_TEXT_WIDTH-10) || !m_infoBoxAutoNewline )&& 
					(pos < length)) // UTF-8
			{
				if ( m_infoBoxText[pos] >= ' ' && m_infoBoxText[pos] <= '~' )  // any char between ASCII(32) and ASCII (126)
					text_line += m_infoBoxText[pos];
				pos++;
			} 
			//printf("[lcdd] line %d:'%s'\r\n",line,text_line.c_str());
			fonts.menu->RenderString(EPG_INFO_TEXT_POS+1,EPG_INFO_TEXT_POS+(line*EPG_INFO_FONT_HEIGHT)+EPG_INFO_FONT_HEIGHT+3, EPG_INFO_TEXT_WIDTH, text_line.c_str(), CLCDDisplay::PIXEL_ON);
			if ( m_infoBoxText[pos] == '\n' )
				pos++; // remove new line
		}
		displayUpdate();
	}
}

/*****************************************************************************************/
//showFilelist
/*****************************************************************************************/
#define BAR_POS_X 		114
#define BAR_POS_Y 		 10
#define BAR_POS_WIDTH 	  6
#define BAR_POS_HEIGTH 	 40

void CLCD::showFilelist(int flist_pos,CFileList* flist,const char * const mainDir)
{
	//printf("[lcdd] FileList\n");
	if(flist != NULL)
		m_fileList = flist;
	if(flist_pos != -1)
		m_fileListPos = flist_pos;
	if(mainDir != NULL)
		m_fileListHeader = mainDir;
		
	if (mode == MODE_FILEBROWSER && 
	    m_fileList != NULL &&
	    !m_fileList->empty() )
	{    
		
		printf("[lcdd] FileList:OK\n");
		int size = m_fileList->size();
		
		display.draw_fill_rect(-1, -1, LCD_COLS, 52, CLCDDisplay::PIXEL_OFF); // clear lcd
		
		if(m_fileListPos > size)
			m_fileListPos = size-1;
		
		int width = fonts.menu->getRenderWidth(m_fileListHeader); 
		if(width > LCD_COLS - 10)
			width = LCD_COLS - 10;
		fonts.menu->RenderString((LCD_COLS - width) / 2, 11, width+5, m_fileListHeader.c_str(), CLCDDisplay::PIXEL_ON);
		
		//printf("list%d,%d\r\n",m_fileListPos,(*m_fileList)[m_fileListPos].Marked);
		std::string text;
		int marked;
		if(m_fileListPos >  0)
		{
			if ( (*m_fileList)[m_fileListPos-1].Marked == false )
			{
				text ="";
				marked = CLCDDisplay::PIXEL_ON;
			}
			else
			{
				text ="*";
				marked = CLCDDisplay::PIXEL_INV;
			}
			text += (*m_fileList)[m_fileListPos-1].getFileName();
			fonts.menu->RenderString(1, 12+12, BAR_POS_X+5, text.c_str(), marked);
		}
		if(m_fileListPos <  size)
		{
			if ((*m_fileList)[m_fileListPos-0].Marked == false )
			{
				text ="";
				marked = CLCDDisplay::PIXEL_ON;
			}
			else
			{
				text ="*";
				marked = CLCDDisplay::PIXEL_INV;
			}
			text += (*m_fileList)[m_fileListPos-0].getFileName();
			fonts.time->RenderString(1, 12+12+14, BAR_POS_X+5, text.c_str(), marked);
		}
		if(m_fileListPos <  size-1)
		{
			if ((*m_fileList)[m_fileListPos+1].Marked == false )
			{
				text ="";
				marked = CLCDDisplay::PIXEL_ON;
			}
			else
			{
				text ="*";
				marked = CLCDDisplay::PIXEL_INV;
			}
			text += (*m_fileList)[m_fileListPos+1].getFileName();
			fonts.menu->RenderString(1, 12+12+14+12, BAR_POS_X+5, text.c_str(), marked);
		}
		// paint marker
		int pages = (((size-1)/3 )+1);
		int marker_length = (BAR_POS_HEIGTH-2) / pages;
		if(marker_length <4)
			marker_length=4;// not smaller than 4 pixel
		int marker_offset = ((BAR_POS_HEIGTH-2-marker_length) * m_fileListPos) /size  ;
		//printf("%d,%d,%d\r\n",pages,marker_length,marker_offset);
		
		display.draw_fill_rect (BAR_POS_X,   BAR_POS_Y,   BAR_POS_X+BAR_POS_WIDTH,   BAR_POS_Y+BAR_POS_HEIGTH,   CLCDDisplay::PIXEL_ON);
		display.draw_fill_rect (BAR_POS_X+1, BAR_POS_Y+1, BAR_POS_X+BAR_POS_WIDTH-1, BAR_POS_Y+BAR_POS_HEIGTH-1, CLCDDisplay::PIXEL_OFF);
		display.draw_fill_rect (BAR_POS_X+1, BAR_POS_Y+1+marker_offset, BAR_POS_X+BAR_POS_WIDTH-1, BAR_POS_Y+1+marker_offset+marker_length, CLCDDisplay::PIXEL_ON);
	
		displayUpdate();
	}
}

/*****************************************************************************************/
//showProgressBar
/*****************************************************************************************/
#define PROG_GLOB_POS_X 10
#define PROG_GLOB_POS_Y 30
#define PROG_GLOB_POS_WIDTH 100
#define PROG_GLOB_POS_HEIGTH 20
void CLCD::showProgressBar(int global, const char * const text,int show_escape,int timer)
{
	if(text != NULL)
		m_progressHeaderGlobal = text;
		
	if(timer != -1)
		m_infoBoxTimer = timer;
		
	if(global >= 0)
	{
		if(global > 100)
			m_progressGlobal =100;
		else
			m_progressGlobal = global;
	}
	
	if(show_escape != -1)
		m_progressShowEscape = show_escape;

	if (mode == MODE_PROGRESSBAR)
	{
		//printf("[lcdd] prog:%s,%d,%d\n",m_progressHeaderGlobal.c_str(),m_progressGlobal,m_progressShowEscape);
		// Clear Display
		display.draw_fill_rect (0, 12, LCD_COLS, 64, CLCDDisplay::PIXEL_OFF);
	
		// paint progress header 
		int width = fonts.menu->getRenderWidth(m_progressHeaderGlobal);
		if(width > 100)
			width = 100;
		int start_pos = (LCD_COLS - width) /2;
		fonts.menu->RenderString(start_pos, 12+12, width+10, m_progressHeaderGlobal.c_str(), CLCDDisplay::PIXEL_ON);
	
		// paint global bar 
		int marker_length = (PROG_GLOB_POS_WIDTH * m_progressGlobal)/100;
		
		display.draw_fill_rect (PROG_GLOB_POS_X,   				 PROG_GLOB_POS_Y,   PROG_GLOB_POS_X+PROG_GLOB_POS_WIDTH,   PROG_GLOB_POS_Y+PROG_GLOB_POS_HEIGTH,   CLCDDisplay::PIXEL_ON);
		display.draw_fill_rect (PROG_GLOB_POS_X+1+marker_length, PROG_GLOB_POS_Y+1, PROG_GLOB_POS_X+PROG_GLOB_POS_WIDTH-1, PROG_GLOB_POS_Y+PROG_GLOB_POS_HEIGTH-1, CLCDDisplay::PIXEL_OFF);
	
		// paint foot 
		if(m_progressShowEscape  == true)
		{
			fonts.menu->RenderString(90, 64, 40, "Home", CLCDDisplay::PIXEL_ON);
		}
		displayUpdate();
	}
}

/*****************************************************************************************/
// showProgressBar2
/*****************************************************************************************/
#define PROG2_GLOB_POS_X 10
#define PROG2_GLOB_POS_Y 37
#define PROG2_GLOB_POS_WIDTH 100
#define PROG2_GLOB_POS_HEIGTH 10

#define PROG2_LOCAL_POS_X 10
#define PROG2_LOCAL_POS_Y 24
#define PROG2_LOCAL_POS_WIDTH PROG2_GLOB_POS_WIDTH
#define PROG2_LOCAL_POS_HEIGTH PROG2_GLOB_POS_HEIGTH

void CLCD::showProgressBar2(int local,const char * const text_local ,int global ,const char * const text_global ,int show_escape )
{
	//printf("[lcdd] prog2\n");
	if(text_local != NULL)
		m_progressHeaderLocal = text_local;
		
	if(text_global != NULL)
		m_progressHeaderGlobal = text_global;
		
	if(global >= 0)
	{
		if(global > 100)
			m_progressGlobal =100;
		else
			m_progressGlobal = global;
	}
	
	if(local >= 0)
	{
		if(local > 100)
			m_progressLocal =100;
		else
			m_progressLocal = local;
	}
	
	if(show_escape != -1)
		m_progressShowEscape = show_escape;

	if (mode == MODE_PROGRESSBAR2)
	{
		//printf("[lcdd] prog2:%s,%d,%d\n",m_progressHeaderGlobal.c_str(),m_progressGlobal,m_progressShowEscape);
		// Clear Display
		display.draw_fill_rect (0, 12, LCD_COLS, 64, CLCDDisplay::PIXEL_OFF);
		
	
		// paint  global caption 
		int width = fonts.menu->getRenderWidth(m_progressHeaderGlobal);
		if(width > 100)
			width = 100;
		int start_pos = (LCD_COLS - width) /2;
		fonts.menu->RenderString(start_pos, PROG2_GLOB_POS_Y+20, width+10, m_progressHeaderGlobal.c_str(), CLCDDisplay::PIXEL_ON);
	
		// paint global bar 
		int marker_length = (PROG2_GLOB_POS_WIDTH * m_progressGlobal)/100;
		
		display.draw_fill_rect (PROG2_GLOB_POS_X,   				PROG2_GLOB_POS_Y,   PROG2_GLOB_POS_X+PROG2_GLOB_POS_WIDTH,   PROG2_GLOB_POS_Y+PROG2_GLOB_POS_HEIGTH,   CLCDDisplay::PIXEL_ON);
		display.draw_fill_rect (PROG2_GLOB_POS_X+1+marker_length, PROG2_GLOB_POS_Y+1, PROG2_GLOB_POS_X+PROG2_GLOB_POS_WIDTH-1, PROG2_GLOB_POS_Y+PROG2_GLOB_POS_HEIGTH-1, CLCDDisplay::PIXEL_OFF);
	
		
		// paint  local caption 
		width = fonts.menu->getRenderWidth(m_progressHeaderLocal);
		if(width > 100)
			width = 100;
		start_pos = (LCD_COLS - width) /2;
		fonts.menu->RenderString(start_pos, PROG2_LOCAL_POS_Y -3, width+10, m_progressHeaderLocal.c_str(), CLCDDisplay::PIXEL_ON);
		// paint local bar 
		marker_length = (PROG2_LOCAL_POS_WIDTH * m_progressLocal)/100;
		
		display.draw_fill_rect (PROG2_LOCAL_POS_X,   				PROG2_LOCAL_POS_Y,   PROG2_LOCAL_POS_X+PROG2_LOCAL_POS_WIDTH,   PROG2_LOCAL_POS_Y+PROG2_LOCAL_POS_HEIGTH,   CLCDDisplay::PIXEL_ON);
		display.draw_fill_rect (PROG2_LOCAL_POS_X+1+marker_length,   PROG2_LOCAL_POS_Y+1, PROG2_LOCAL_POS_X+PROG2_LOCAL_POS_WIDTH-1, PROG2_LOCAL_POS_Y+PROG2_LOCAL_POS_HEIGTH-1, CLCDDisplay::PIXEL_OFF);
		
		
		// paint foot 
		if(m_progressShowEscape  == true)
		{
			fonts.menu->RenderString(90, 64, 40, "Home", CLCDDisplay::PIXEL_ON);
		}
		displayUpdate();
	}
}
/*****************************************************************************************/
#endif // LCD_UPDATE
