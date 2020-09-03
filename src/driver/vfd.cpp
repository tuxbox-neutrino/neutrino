/*
	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/


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

#include <driver/vfd.h>

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>
#include <driver/record.h>

#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h> 

#include <daemonc/remotecontrol.h>
#include <system/helpers.h>
#include <zapit/debug.h>

#include <cs_api.h>
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

CVFD::CVFD()
{
#ifdef VFD_UPDATE
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
#endif // VFD_UPDATE

	has_lcd = true;
	has_led_segment = false;
	fd = open("/dev/display", O_RDONLY);
	if(fd < 0) {
		perror("/dev/display");
		has_lcd = false;
		has_led_segment = false;
	}

#ifdef BOXMODEL_CST_HD2
	if (fd >= 0) {
		int ret = ioctl(fd, IOC_FP_GET_DISPLAY_CAPS, &caps);
		if (ret < 0) {
			perror("IOC_FP_GET_DISPLAY_CAPS");
			printf("VFD: please update driver!\n");
			support_text	= true;
			support_numbers	= true;
		} else {
			switch (caps.display_type) {
			case FP_DISPLAY_TYPE_NONE:
				has_lcd		= false;
				has_led_segment	= false;
				break;
			case FP_DISPLAY_TYPE_LED_SEGMENT:
				has_lcd		= false;
				has_led_segment	= true;
				break;
			default:
				has_lcd		= true;
				has_led_segment	= false;
				break;
			}
			support_text    = (caps.display_type != FP_DISPLAY_TYPE_LED_SEGMENT &&
				           caps.text_support != FP_DISPLAY_TEXT_NONE);
			support_numbers = caps.number_support;
		}
	}
#else
	support_text	= true;
	support_numbers	= true;
#endif

	text.clear();
	clearClock = 0;
	mode = MODE_TVRADIO;
	switch_name_time_cnt = 0;
	timeout_cnt = 0;
	service_number = -1;
}

CVFD::~CVFD()
{
	if(fd > 0){
		close(fd);
		fd = -1;
	}
}

CVFD* CVFD::getInstance()
{
	static CVFD* lcdd = NULL;
	if(lcdd == NULL) {
		lcdd = new CVFD();
	}
	return lcdd;
}

void CVFD::count_down() {
	if (timeout_cnt > 0) {
		timeout_cnt--;
		if (timeout_cnt == 0 ) {
			if (g_settings.lcd_setting_dim_brightness > -1) {
				// save lcd brightness, setBrightness() changes global setting
				int b = g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
				setBrightness(g_settings.lcd_setting_dim_brightness);
				g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = b;
			} else {
				setPower(0);
			}
		}
	}
	if (g_settings.lcd_info_line && switch_name_time_cnt > 0) {
	  switch_name_time_cnt--;
		if (switch_name_time_cnt == 0) {
			if (g_settings.lcd_setting_dim_brightness > -1) {
				CVFD::getInstance()->showTime(true);
			}
		}
	}
}

void CVFD::wake_up() {
	if(fd < 0) return;

	if (atoi(g_settings.lcd_setting_dim_time.c_str()) > 0) {
		timeout_cnt = atoi(g_settings.lcd_setting_dim_time.c_str());
		g_settings.lcd_setting_dim_brightness > -1 ?
			setBrightness(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS]) : setPower(1);
	}
	else
		setPower(1);
	if(g_settings.lcd_info_line){
		switch_name_time_cnt = g_settings.handling_infobar[SNeutrinoSettings::HANDLING_INFOBAR] + 10;
	}

}

void* CVFD::TimeThread(void *)
{
	while(1) {
		sleep(1);
		struct stat buf;
                if (stat("/tmp/vfd.locked", &buf) == -1) {
                        CVFD::getInstance()->showTime();
                        CVFD::getInstance()->count_down();
                } else
                        CVFD::getInstance()->wake_up();
	}
	return NULL;
}

void CVFD::init(const char * /*fontfile*/, const char * /*fontname*/)
{
	//InitNewClock(); /FIXME

	brightness = -1;
	setMode(MODE_TVRADIO);

	if (pthread_create (&thrTime, NULL, TimeThread, NULL) != 0 ) {
		perror("[lcdd]: pthread_create(TimeThread)");
		return ;
	}
}

void CVFD::setlcdparameter(int dimm, const int power)
{
	if(fd < 0) return;

	if(dimm < 0)
		dimm = 0;
	else if(dimm > 15)
		dimm = 15;

	if(!power)
		dimm = 0;

	if(brightness == dimm)
		return;

	brightness = dimm;

printf("CVFD::setlcdparameter dimm %d power %d\n", dimm, power);
	int ret = ioctl(fd, IOC_FP_SET_BRIGHT, dimm);
	if(ret < 0)
		perror("IOC_FP_SET_BRIGHT");
}

void CVFD::setlcdparameter(void)
{
	if(fd < 0) return;
	last_toggle_state_power = g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];
	setlcdparameter((mode == MODE_STANDBY) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] : (mode == MODE_SHUTDOWN) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] : g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS],
			last_toggle_state_power);
}

void CVFD::setled(int led1, int led2)
{
	int ret = -1;

	if(led1 != -1){
		ret = ioctl(fd, IOC_FP_LED_CTRL, led1);
		if(ret < 0)
			perror("IOC_FP_LED_CTRL");
	}
	if(led2 != -1){
		ret = ioctl(fd, IOC_FP_LED_CTRL, led2);
		if(ret < 0)
			perror("IOC_FP_LED_CTRL");
	}
}

void CVFD::setBacklight(bool on_off)
{
	if(cs_get_revision() != 9)
		return;

	int led = on_off ? FP_LED_3_ON : FP_LED_3_OFF;
	if (ioctl(fd, IOC_FP_LED_CTRL, led) < 0)
		perror("FP_LED_3");
}

void CVFD::setled(bool on_off)
{
	if(g_settings.led_rec_mode == 0)
		return;

	int led1 = -1, led2 = -1;
	if(on_off){//on
		switch(g_settings.led_rec_mode) {
			case 1:
				led1 = FP_LED_1_ON; led2 = FP_LED_2_ON;
				break;
			case 2:
				led1 = FP_LED_1_ON;
				break;
			case 3:
				led2 = FP_LED_2_ON;
				break;
			default:
				break;
	      }
	}
	else {//off
		switch(g_settings.led_rec_mode) {
			case 1:
				led1 = FP_LED_1_OFF; led2 = FP_LED_2_OFF;
				break;
			case 2:
				led1 = FP_LED_1_OFF;
				break;
			case 3:
				led2 = FP_LED_2_OFF;
				break;
			default:
				led1 = FP_LED_1_OFF; led2 = FP_LED_2_OFF;
				break;
	      }
	}

	setled(led1, led2);
}

void CVFD::setled(void)
{
	if(fd < 0) return;

	int led1 = -1, led2 = -1;
	int select = 0;

	if(mode == MODE_MENU_UTF8 || mode == MODE_TVRADIO  )
		  select = g_settings.led_tv_mode;
	else if(mode == MODE_STANDBY)
		  select = g_settings.led_standby_mode;

	switch(select){
		case 0:
			led1 = FP_LED_1_OFF; led2 = FP_LED_2_OFF;
			break;
		case 1:
			led1 = FP_LED_1_ON; led2 = FP_LED_2_ON;
			break;
		case 2:
			led1 = FP_LED_1_ON; led2 = FP_LED_2_OFF;
			break;
		case 3:
			led1 = FP_LED_1_OFF; led2 = FP_LED_2_ON;
			break;
		default:
			break;
	}
	setled(led1, led2);
}

void CVFD::showServicename(const std::string & name, int number) // UTF-8
{
	if(fd < 0) return;

printf("CVFD::showServicename: %s\n", name.c_str());
	servicename = name;
	service_number = number;

	if (mode != MODE_TVRADIO)
		return;

	if (support_text)
		ShowText(name.c_str());
	else
		ShowNumber(service_number);
	wake_up();
}

void CVFD::showTime(bool force)
{
	//unsigned int system_rev = cs_get_revision();
	static int recstatus = 0;
#if 0
	if(!has_lcd)
		return;
#endif
	if(fd >= 0 && mode == MODE_SHUTDOWN) {
		ShowIcon(FP_ICON_CAM1, false);
		return;
	}
	if (fd >= 0 && showclock) {
		if (mode == MODE_STANDBY || ( g_settings.lcd_info_line && (MODE_TVRADIO == mode))) {
			char timestr[21];
			time_t now = time(NULL);
			struct tm t;
			static int hour = 0, minute = 0;

			localtime_r(&now, &t);
			if(force || ( switch_name_time_cnt == 0 && ((hour != t.tm_hour) || (minute != t.tm_min))) ) {
				hour = t.tm_hour;
				minute = t.tm_min;
				if (support_text) {
					strftime(timestr, 20, "%H:%M", &t);
					ShowText(timestr);
				} else if (support_numbers && has_led_segment) {
					ShowNumber((t.tm_hour*100) + t.tm_min);
#ifdef BOXMODEL_CST_HD2
					ioctl(fd, IOC_FP_SET_COLON, 0x01);
#endif
				}
			}
		}
	}

	int tmp_recstatus = CNeutrinoApp::getInstance()->recordingstatus;
	if (tmp_recstatus) {
		if(clearClock) {
			clearClock = 0;
			if(has_lcd)
				ShowIcon(FP_ICON_CAM1, false);
			setled(false);//off
		} else {
			clearClock = 1;
			if(has_lcd)
				ShowIcon(FP_ICON_CAM1, true);
			setled(true);//on
		}
	} else if(clearClock || (recstatus != tmp_recstatus)) { // in case icon ON after record stopped
		clearClock = 0;
		if(has_lcd)
			ShowIcon(FP_ICON_CAM1, false);

		setled();
	}

	recstatus = tmp_recstatus;
}

void CVFD::showRCLock(int duration)
{
	if (!has_lcd || !g_settings.lcd_notify_rclock)
	{
		sleep(duration);
		return;
	}

	std::string _text = text;
	ShowText(g_Locale->getText(LOCALE_RCLOCK_LOCKED));
	sleep(duration);
	ShowText(_text.c_str());
}

void CVFD::showVolume(const char vol, const bool force_update)
{
	static int oldpp = 0;
	if(!has_lcd) return;

	ShowIcon(FP_ICON_MUTE, muted);

	if(!force_update && vol == volume)
		return;
	volume = vol;

	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 2 /* off */)
		return;

	bool allowed_mode = (mode == MODE_TVRADIO || mode == MODE_AUDIO || mode == MODE_MENU_UTF8);
	if (!allowed_mode)
		return;

	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 1) {
		wake_up();
		ShowIcon(FP_ICON_FRAME, true);
		int pp = (vol * 8 + 50) / 100;
		if(pp > 8) pp = 8;

		if(force_update || oldpp != pp) {
printf("CVFD::showVolume: %d, bar %d\n", (int) vol, pp);
			int i;
			int j = 0x00000200;
			for(i = 0; i < pp; i++) {
				ShowIcon((fp_icon) j, true);
				j /= 2;
			}
			for(;i < 8; i++) {
				ShowIcon((fp_icon) j, false);
				j /= 2;
			}
			oldpp = pp;
		}
	}
}

void CVFD::showPercentOver(const unsigned char perc, const bool /*perform_update*/, const MODES origin)
{

	static int ppold = 0;
	if(!has_lcd) return;

	percentOver = perc;

	if (mode == MODE_AUDIO && origin != MODE_AUDIO) // exclusive access for audio mode
		return;

	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 2 /* off */)
		return;

	bool allowed_mode = (mode == MODE_TVRADIO || mode == MODE_AUDIO || mode == MODE_MENU_UTF8);
	if (!allowed_mode)
		return;

	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] == 0) {
		ShowIcon(FP_ICON_FRAME, true);
		int pp;
		if(perc == 255)
			pp = 0;
		else
			pp = (perc * 8 + 50) / 100;
		if(pp > 8) pp = 8;

		if(pp != ppold) {
//printf("CVFD::showPercentOver: %d, bar %d\n", (int) perc, pp);
			int i;
			int j = 0x00000200;
			for(i = 0; i < pp; i++) {
				ShowIcon((fp_icon) j, true);
				j /= 2;
			}
			for(;i < 8; i++) {
				ShowIcon((fp_icon) j, false);
				j /= 2;
			}
			ppold = pp;
		}
	}
}

void CVFD::showMenuText(const int /*position*/, const char * ptext, const int /*highlight*/, const bool /*utf_encoded*/)
{
	if(fd < 0) return;
	if (mode != MODE_MENU_UTF8)
		return;

	ShowText(ptext);
	wake_up();
}

void CVFD::showAudioTrack(const std::string & /*artist*/, const std::string & title, const std::string & /*album*/)
{
	if(fd < 0) return;
	if (mode != MODE_AUDIO)
		return;
printf("CVFD::showAudioTrack: %s\n", title.c_str());
	ShowText(title.c_str());
	wake_up();

#ifdef HAVE_LCD
	fonts.menu->RenderString(0,22, 125, artist.c_str() , CLCDDisplay::PIXEL_ON);
	fonts.menu->RenderString(0,35, 125, album.c_str() , CLCDDisplay::PIXEL_ON);
	fonts.menu->RenderString(0,48, 125, title.c_str() , CLCDDisplay::PIXEL_ON);
#endif
}

void CVFD::showAudioPlayMode(AUDIOMODES m)
{
	if(fd < 0) return;
	if (mode != MODE_AUDIO)
		return;

	switch(m) {
		case AUDIO_MODE_PLAY:
			ShowIcon(FP_ICON_PLAY, true);
			ShowIcon(FP_ICON_PAUSE, false);
			break;
		case AUDIO_MODE_STOP:
			ShowIcon(FP_ICON_PLAY, false);
			ShowIcon(FP_ICON_PAUSE, false);
			break;
		case AUDIO_MODE_PAUSE:
			ShowIcon(FP_ICON_PLAY, false);
			ShowIcon(FP_ICON_PAUSE, true);
			break;
		case AUDIO_MODE_FF:
			break;
		case AUDIO_MODE_REV:
			break;
	}
	wake_up();
}

void CVFD::showAudioProgress(const unsigned char perc)
{
	if(fd < 0) return;
	if (mode != MODE_AUDIO)
		return;

	showPercentOver(perc, true, MODE_AUDIO);
}

void CVFD::setMode(const MODES m, const char * const title)
{
	if(fd < 0) return;

	// Clear colon in display if it is still there
#ifdef BOXMODEL_CST_HD2
	if (support_numbers && has_led_segment)
		ioctl(fd, IOC_FP_SET_COLON, 0x00);
#endif

	if(mode == MODE_AUDIO)
		ShowIcon(FP_ICON_MP3, false);
#if 0
	else if(mode == MODE_STANDBY) {
		ShowIcon(FP_ICON_COL1, false);
		ShowIcon(FP_ICON_COL2, false);
	}
#endif

	if(strlen(title))
		ShowText(title);
	mode = m;
	setlcdparameter();

	switch (m) {
	case MODE_TVRADIO:
		switch (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME])
		{
		case 0:
			showPercentOver(percentOver, false);
			break;
		case 1:
			showVolume(volume, false);
			break;
#if 0
		case 2:
			showVolume(volume, false);
			showPercentOver(percentOver, false);
			break;
#endif
		}
		showServicename(servicename);
		showclock = true;
		if(g_settings.lcd_info_line)
			switch_name_time_cnt = g_settings.handling_infobar[SNeutrinoSettings::HANDLING_INFOBAR] + 10;
		break;
	case MODE_AUDIO:
	{
		ShowIcon(FP_ICON_MP3, true);
		showAudioPlayMode(AUDIO_MODE_STOP);
		showVolume(volume, false);
		showclock = true;
		//showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		break;
	}
	case MODE_SCART:
		showVolume(volume, false);
		showclock = true;
		//showTime();      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		break;
	case MODE_MENU_UTF8:
		showclock = false;
		//fonts.menutitle->RenderString(0,28, 140, title, CLCDDisplay::PIXEL_ON);
		break;
	case MODE_SHUTDOWN:
		showclock = false;
		Clear();
		break;
	case MODE_STANDBY:
#if 0
		ShowIcon(FP_ICON_COL1, true);
		ShowIcon(FP_ICON_COL2, true);
#endif
		showclock = true;
		showTime(true);      /* "showclock = true;" implies that "showTime();" does a "displayUpdate();" */
		                 /* "showTime()" clears the whole lcd in MODE_STANDBY                         */
		break;
#ifdef VFD_UPDATE
        case MODE_FILEBROWSER:
                showclock = true;
                display.draw_fill_rect(-1, -1, 120, 64, CLCDDisplay::PIXEL_OFF); // clear lcd
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
#endif // VFD_UPDATE
	}
	wake_up();
	setled();
}

void CVFD::setBrightness(int bright)
{
	if(!has_lcd && !has_led_segment) return;

	g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CVFD::getBrightness()
{
	//FIXME for old neutrino.conf
	if(g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] > 15)
		g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS] = 15;

	return g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS];
}

void CVFD::setBrightnessStandby(int bright)
{
	if(!has_lcd && !has_led_segment) return;

	g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CVFD::getBrightnessStandby()
{
	//FIXME for old neutrino.conf
	if(g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] > 15)
		g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] = 15;
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS];
}

void CVFD::setBrightnessDeepStandby(int bright)
{
	if(!has_lcd && !has_led_segment) return;

	g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] = bright;
	setlcdparameter();
}

int CVFD::getBrightnessDeepStandby()
{
	//FIXME for old neutrino.conf
	if(g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] > 15)
		g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] = 15;
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS];
}

void CVFD::setPower(int /*power*/)
{
	if(!has_lcd) return;
// old
	//g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER] = power;
	//setlcdparameter();
}

int CVFD::getPower()
{
	return g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER];
}

void CVFD::togglePower(void)
{
	if(fd < 0) return;

	last_toggle_state_power = 1 - last_toggle_state_power;
	setlcdparameter((mode == MODE_STANDBY) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_STANDBY_BRIGHTNESS] : (mode == MODE_SHUTDOWN) ? g_settings.lcd_setting[SNeutrinoSettings::LCD_DEEPSTANDBY_BRIGHTNESS] : g_settings.lcd_setting[SNeutrinoSettings::LCD_BRIGHTNESS],
			last_toggle_state_power);
}

void CVFD::setMuted(bool mu)
{
	if(!has_lcd) return;
	muted = mu;
	showVolume(volume);
}

void CVFD::resume()
{
	if(!has_lcd) return;
}

void CVFD::pause()
{
	if(!has_lcd) return;
}

void CVFD::Lock()
{
	if(fd < 0) return;
	creat("/tmp/vfd.locked", 0);
}

void CVFD::Unlock()
{
	if(fd < 0) return;
	unlink("/tmp/vfd.locked");
}

void CVFD::Clear()
{
	if(fd < 0) return;
	int ret = ioctl(fd, IOC_FP_CLEAR_ALL, 0);
	if(ret < 0)
		perror("IOC_FP_SET_TEXT");
	else
		text.clear();
}

void CVFD::ShowIcon(fp_icon icon, bool show)
{
	if(!has_lcd || fd < 0) return;
//printf("CVFD::ShowIcon %s %x\n", show ? "show" : "hide", (int) icon);
	int ret = ioctl(fd, show ? IOC_FP_SET_ICON : IOC_FP_CLEAR_ICON, icon);
	if(ret < 0)
		perror(show ? "IOC_FP_SET_ICON" : "IOC_FP_CLEAR_ICON");
}

void CVFD::ShowText(const char * str)
{
	if (fd < 0 || !support_text)
		return;

	char flags[2] = { FP_FLAG_ALIGN_LEFT, 0 };
	if (! str) {
		printf("CVFD::ShowText: str is NULL!\n");
		return;
	}

	if (g_settings.lcd_scroll && ((int)strlen(str) > g_info.hw_caps->display_xres))
		flags[0] |= FP_FLAG_SCROLL_ON | FP_FLAG_SCROLL_SIO | FP_FLAG_SCROLL_DELAY;

	std::string txt = std::string(flags) + str;
	txt = trim(txt);
	printf("CVFD::ShowText: [0x%02x][%s]\n", flags[0], txt.c_str() + 1);

	size_t len = txt.length();
	if (txt == text || len > 255)
		return;

	text = txt;
	int ret = ioctl(fd, IOC_FP_SET_TEXT, len > 1 ? txt.c_str() : NULL);
	if(ret < 0) {
		support_text = false;
		perror("IOC_FP_SET_TEXT");
	}
}

void CVFD::ShowNumber(int number)
{
	if (fd < 0 || (!support_text && !support_numbers))
		return;

	if (number < 0)
		return;
	
#ifdef BOXMODEL_CST_HD2
	int ret = ioctl(fd, IOC_FP_SET_NUMBER, number);
	if(ret < 0) {
		support_numbers = false;
		perror("IOC_FP_SET_NUMBER");
	}
#endif
}

#ifdef VFD_UPDATE

/*****************************************************************************************/
// showInfoBox
/*****************************************************************************************/
#define LCD_WIDTH 120
#define LCD_HEIGTH 64

#define EPG_INFO_FONT_HEIGHT 9
#define EPG_INFO_SHADOW_WIDTH 1
#define EPG_INFO_LINE_WIDTH 1
#define EPG_INFO_BORDER_WIDTH 2

#define EPG_INFO_WINDOW_POS 4
#define EPG_INFO_LINE_POS       EPG_INFO_WINDOW_POS + EPG_INFO_SHADOW_WIDTH
#define EPG_INFO_BORDER_POS EPG_INFO_WINDOW_POS + EPG_INFO_SHADOW_WIDTH + EPG_INFO_LINE_WIDTH
#define EPG_INFO_TEXT_POS       EPG_INFO_WINDOW_POS + EPG_INFO_SHADOW_WIDTH + EPG_INFO_LINE_WIDTH + EPG_INFO_BORDER_WIDTH

#define EPG_INFO_TEXT_WIDTH LCD_WIDTH - (2*EPG_INFO_WINDOW_POS)

// timer 0: OFF, timer>0 time to show in seconds,  timer>=999 endless
void CVFD::showInfoBox(const char * const title, const char * const text ,int autoNewline,int timer)
{
#ifdef HAVE_LCD
	if(!has_lcd) return;
        //printf("[lcdd] Info: \n");
        if(text != NULL)
                m_infoBoxText = text;
        if(text != NULL)
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
                display.draw_fill_rect (EPG_INFO_WINDOW_POS, EPG_INFO_WINDOW_POS,       LCD_WIDTH-EPG_INFO_WINDOW_POS+1,          LCD_HEIGTH-EPG_INFO_WINDOW_POS+1,    CLCDDisplay::PIXEL_OFF);
                display.draw_fill_rect (EPG_INFO_LINE_POS,       EPG_INFO_LINE_POS,     LCD_WIDTH-EPG_INFO_LINE_POS-1,    LCD_HEIGTH-EPG_INFO_LINE_POS-1,        CLCDDisplay::PIXEL_ON);
                display.draw_fill_rect (EPG_INFO_BORDER_POS, EPG_INFO_BORDER_POS,       LCD_WIDTH-EPG_INFO_BORDER_POS-3,  LCD_HEIGTH-EPG_INFO_BORDER_POS-3, CLCDDisplay::PIXEL_OFF);

                // paint title
                if(!m_infoBoxTitle.empty())
                {
                        int width = fonts.menu->getRenderWidth(m_infoBoxTitle);
                        if(width > 100)
                                width = 100;
                        int start_pos = (120-width) /2;
                        display.draw_fill_rect (start_pos, EPG_INFO_WINDOW_POS-4,       start_pos+width+5,        EPG_INFO_WINDOW_POS+10,    CLCDDisplay::PIXEL_OFF);
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
#endif
}

/*****************************************************************************************/
//showFilelist
/*****************************************************************************************/
#define BAR_POS_X               114
#define BAR_POS_Y                10
#define BAR_POS_WIDTH     6
#define BAR_POS_HEIGTH   40

void CVFD::showFilelist(int flist_pos,CFileList* flist,const char * const mainDir)
{
#ifdef HAVE_LCD
	if(!has_lcd) return;
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

                display.draw_fill_rect(-1, -1, 120, 52, CLCDDisplay::PIXEL_OFF); // clear lcd

                if(m_fileListPos > size)
                        m_fileListPos = size-1;

                int width = fonts.menu->getRenderWidth(m_fileListHeader);
                if(width>110)
                        width=110;
                fonts.menu->RenderString((120-width)/2, 11, width+5, m_fileListHeader.c_str(), CLCDDisplay::PIXEL_ON);

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
#endif
}

/*****************************************************************************************/
//showProgressBar
/*****************************************************************************************/
#define PROG_GLOB_POS_X 10
#define PROG_GLOB_POS_Y 30
#define PROG_GLOB_POS_WIDTH 100
#define PROG_GLOB_POS_HEIGTH 20
void CVFD::showProgressBar(int global, const char * const text,int show_escape,int timer)
{
#ifdef HAVE_LCD
	if(!has_lcd) return;
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
                display.draw_fill_rect (0,12,120,64, CLCDDisplay::PIXEL_OFF);

                // paint progress header
                int width = fonts.menu->getRenderWidth(m_progressHeaderGlobal);
                if(width > 100)
                        width = 100;
                int start_pos = (120-width) /2;
                fonts.menu->RenderString(start_pos, 12+12, width+10, m_progressHeaderGlobal.c_str(), CLCDDisplay::PIXEL_ON);

                // paint global bar
                int marker_length = (PROG_GLOB_POS_WIDTH * m_progressGlobal)/100;

                display.draw_fill_rect (PROG_GLOB_POS_X,                                 PROG_GLOB_POS_Y,   PROG_GLOB_POS_X+PROG_GLOB_POS_WIDTH,   PROG_GLOB_POS_Y+PROG_GLOB_POS_HEIGTH,   CLCDDisplay::PIXEL_ON);
                display.draw_fill_rect (PROG_GLOB_POS_X+1+marker_length, PROG_GLOB_POS_Y+1, PROG_GLOB_POS_X+PROG_GLOB_POS_WIDTH-1, PROG_GLOB_POS_Y+PROG_GLOB_POS_HEIGTH-1, CLCDDisplay::PIXEL_OFF);

                // paint foot
                if(m_progressShowEscape  == true)
                {
                        fonts.menu->RenderString(90, 64, 40, "Home", CLCDDisplay::PIXEL_ON);
                }
                displayUpdate();
        }
#endif
}

/*****************************************************************************************/
// showProgressBar2
/*****************************************************************************************/
#define PROG2_GLOB_POS_X 10
#define PROG2_GLOB_POS_Y 24
#define PROG2_GLOB_POS_WIDTH 100
#define PROG2_GLOB_POS_HEIGTH 10

#define PROG2_LOCAL_POS_X 10
#define PROG2_LOCAL_POS_Y 37
#define PROG2_LOCAL_POS_WIDTH PROG2_GLOB_POS_WIDTH
#define PROG2_LOCAL_POS_HEIGTH PROG2_GLOB_POS_HEIGTH

void CVFD::showProgressBar2(int local,const char * const text_local ,int global ,const char * const text_global ,int show_escape )
{
#ifdef HAVE_LCD
	if(!has_lcd) return;
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
                display.draw_fill_rect (0,12,120,64, CLCDDisplay::PIXEL_OFF);

                // paint  global header
                int width = fonts.menu->getRenderWidth(m_progressHeaderGlobal);
                if(width > 100)
                        width = 100;
                int start_pos = (120-width) /2;
                fonts.menu->RenderString(start_pos, PROG2_GLOB_POS_Y-3, width+10, m_progressHeaderGlobal.c_str(), CLCDDisplay::PIXEL_ON);

                // paint global bar
                int marker_length = (PROG2_GLOB_POS_WIDTH * m_progressGlobal)/100;

                display.draw_fill_rect (PROG2_GLOB_POS_X,                               PROG2_GLOB_POS_Y,   PROG2_GLOB_POS_X+PROG2_GLOB_POS_WIDTH,   PROG2_GLOB_POS_Y+PROG2_GLOB_POS_HEIGTH,   CLCDDisplay::PIXEL_ON);
                display.draw_fill_rect (PROG2_GLOB_POS_X+1+marker_length, PROG2_GLOB_POS_Y+1, PROG2_GLOB_POS_X+PROG2_GLOB_POS_WIDTH-1, PROG2_GLOB_POS_Y+PROG2_GLOB_POS_HEIGTH-1, CLCDDisplay::PIXEL_OFF);

                // paint  local header
                width = fonts.menu->getRenderWidth(m_progressHeaderLocal);
                if(width > 100)
                        width = 100;
                start_pos = (120-width) /2;
                fonts.menu->RenderString(start_pos, PROG2_LOCAL_POS_Y + PROG2_LOCAL_POS_HEIGTH +10 , width+10, m_progressHeaderLocal.c_str(), CLCDDisplay::PIXEL_ON);
                // paint local bar
                marker_length = (PROG2_LOCAL_POS_WIDTH * m_progressLocal)/100;

                display.draw_fill_rect (PROG2_LOCAL_POS_X,                              PROG2_LOCAL_POS_Y,   PROG2_LOCAL_POS_X+PROG2_LOCAL_POS_WIDTH,   PROG2_LOCAL_POS_Y+PROG2_LOCAL_POS_HEIGTH,   CLCDDisplay::PIXEL_ON);
                display.draw_fill_rect (PROG2_LOCAL_POS_X+1+marker_length,   PROG2_LOCAL_POS_Y+1, PROG2_LOCAL_POS_X+PROG2_LOCAL_POS_WIDTH-1, PROG2_LOCAL_POS_Y+PROG2_LOCAL_POS_HEIGTH-1, CLCDDisplay::PIXEL_OFF);
                // paint foot
                if(m_progressShowEscape  == true)
                {
                        fonts.menu->RenderString(90, 64, 40, "Home", CLCDDisplay::PIXEL_ON);
                }
                displayUpdate();
        }
#endif
}
/*****************************************************************************************/
#endif // VFD_UPDATE


