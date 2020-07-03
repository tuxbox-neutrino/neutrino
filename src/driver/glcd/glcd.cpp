/*
	Neutrino graphlcd daemon thread

	(C) 2012-2014 by martii


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
#include <global.h>
#include <neutrino.h>
#include <algorithm>
#include <system/debug.h>
#include <timerdclient/timerdclient.h>
#include <system/helpers.h>
#include <system/set_threadname.h>
#include <driver/audioplay.h>
#include <gui/movieplayer.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <hardware_caps.h>
#include <eitd/sectionsd.h>
#include <math.h>

#include "glcd.h"
#include "analogclock.h"
#include "digitalclock.h"
#include "simpleclock.h"
#include "weather.h"

#include "zlib.h"
#include "png.h"
#include "jpeglib.h"

#define LCD_ICONSDIR	"/share/lcd/icons/"
#define ICONSEXT	".png"

static const char * kDefaultConfigFile = "/etc/graphlcd.conf";
static cGLCD *cglcd = NULL;

extern CRemoteControl *g_RemoteControl;
extern CPictureViewer * g_PicViewer;

cGLCD::cGLCD()
{
	lcd = NULL;
	Logo = "";
	Channel = "Neutrino";
	Epg = std::string(g_info.hw_caps->boxvendor) + " " + std::string(g_info.hw_caps->boxname);

	sem_init(&sem, 0, 1);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&mutex, &attr);

	channelLocked = false;
	timeLocked = false;
	durationLocked = false;
	startLocked = false;
	endLocked = false;
	recLocked = false;
	muteLocked = false;
	tsLocked = false;
	ecmLocked = false;
	timerLocked = false;
	ddLocked = false;
	txtLocked = false;
	subLocked = false;
	camLocked = false;
	doRescan = false;
	doStandby = false;
	doStandbyTime = false;
	doStandbyWeather = false;
	doShowVolume = false;
	doShowLcdIcon =  false;
	doSuspend = false;
	doExit = false;
	doMirrorOSD = false;
	fontsize_channel = 0;
	fontsize_epg = 0;
	fontsize_time = 0;
	fontsize_duration = 0;
	fontsize_start = 0;
	fontsize_end = 0;
	fontsize_smalltext = 0;
	fonts_initialized = false;
	ismediaplayer = false;
	doScrollChannel = false;
	doScrollEpg = false;
	percent_channel = 0;
	percent_time = 0;
	percent_duration = 0;
	percent_start = 0;
	percent_end = 0;
	percent_smalltext = 0;
	percent_epg = 0;
	percent_bar = 0;
	percent_logo = 0;
	power_state = 1;
	Scale = 0;
	bitmap = NULL;
	blitFlag = true;
	timeout_cnt = 0;
	locked_countdown = false;
	time_thread_started = false;

	cglcd = this;

	if (!g_settings.glcd_enable)
		doSuspend = true;

	if (pthread_create (&thrGLCD, 0, cGLCD::Run, this) != 0 )
		fprintf(stderr, "ERROR: pthread_create(cGLCD::Init)\n");

	if (pthread_create (&thrTimeThread, 0, cGLCD::TimeThread, this) != 0 )
		fprintf(stderr, "ERROR: pthread_create(cGLCD::TimeThread)\n");

	InitAnalogClock();
	InitDigitalClock();
	InitSimpleClock();
	InitWeather();

	Update();
}

void cGLCD::Lock(void)
{
	if (cglcd)
	{
		pthread_mutex_lock(&cglcd->mutex);
	}
}

void cGLCD::Unlock(void)
{
	if (cglcd)
	{
		pthread_mutex_unlock(&cglcd->mutex);
	}
}

cGLCD::~cGLCD()
{
	Suspend();
	cglcd = NULL;
	if (lcd)
	{
		lcd->DeInit();
		delete lcd;
	}
	sem_destroy(&sem);
	pthread_mutex_destroy(&mutex);
}

cGLCD *cGLCD::getInstance()
{
	if (!cglcd)
		cglcd = new cGLCD;
	return cglcd;
}

uint32_t cGLCD::ColorConvert3to1(uint32_t red, uint32_t green, uint32_t blue)
{
	unsigned int color_red_tmp = (static_cast<int>(red) * 2.55) + 1;
	unsigned int color_green_tmp = (static_cast<int>(green) * 2.55) + 1;
	unsigned int color_blue_tmp = (static_cast<int>(blue) * 2.55) + 1;

	uint32_t color = 0xff; color <<= 8;
	color |= color_red_tmp; color <<= 8;
	color |= color_green_tmp; color <<= 8;
	color |= color_blue_tmp;

	return color;
}

void cGLCD::Exec()
{
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	if (!lcd)
		return;

	bitmap->Clear(ColorConvert3to1(t.glcd_color_bg_red, t.glcd_color_bg_green, t.glcd_color_bg_blue));

	if (Channel.compare("Neutrino") == 0)
	{
		if (imageShow(DATADIR "/neutrino/icons/start.jpg", 0, 0, 0, 0, false, true, true, false, false))
		{
			GLCD::cFont font_tmp;

			int fw = font_epg.Width(Epg);
			font_tmp.LoadFT2(t.glcd_font, "UTF-8", fontsize_epg * bitmap->Width() / fw);
			fw = font_tmp.Width(Epg);

			drawText(std::max(2,(bitmap->Width() - fw)/2),
				10 * bitmap->Height()/100, bitmap->Width(), fw, Epg,
				&font_tmp, ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent, true, 0, ALIGN_NONE);

			lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
			lcd->Refresh(true);
		}
		else
		{
			cglcd->bitmap->Clear(ColorConvert3to1(t.glcd_color_bg_red, t.glcd_color_bg_green, t.glcd_color_bg_blue));
			cglcd->lcd->Refresh(true);
		}
		return;
	}

	if (doStandbyTime || doStandbyWeather)
	{
		if (g_settings.glcd_time_in_standby == CLOCK_ANALOG)
		{
			ShowAnalogClock(tm->tm_hour, tm->tm_min, tm->tm_sec, bitmap->Width()/2, bitmap->Height()/2);
		}
		else if (g_settings.glcd_time_in_standby == CLOCK_DIGITAL)
		{
			ShowDigitalClock(tm->tm_hour, tm->tm_min);
		}
		else if (g_settings.glcd_time_in_standby > CLOCK_OFF)
		{
			ShowSimpleClock(strftime("%H:%M", tm), g_settings.glcd_time_in_standby);
		}
		if (g_settings.glcd_standby_weather == 1 && g_settings.glcd_time_in_standby != CLOCK_ANALOG)
		{
			ShowWeather(true);
		}
		lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
		lcd->Refresh(false);
		return;
	}

	if (t.glcd_background != "")
		imageShow(t.glcd_background, 0, 0, 0, 0, false, true, true, false, false);

	if (t.glcd_show_weather)
		ShowWeather(false);

	switch (CAudioPlayer::getInstance()->getState())
	{
		case CBaseDec::REV:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_REW ICONSEXT;
			break;
		case CBaseDec::FF:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_FF ICONSEXT;
			break;
		case CBaseDec::PAUSE:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_PAUSE ICONSEXT;
			break;
		case CBaseDec::PLAY:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_PLAY ICONSEXT;
			break;
		default:
			ismediaplayer = false;
			;
	}

	switch (CMoviePlayerGui::getInstance().getState())
	{
		case CMoviePlayerGui::REW:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_REW ICONSEXT;
			break;
		case CMoviePlayerGui::FF:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_FF ICONSEXT;
			break;
		case CMoviePlayerGui::PAUSE:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_PAUSE ICONSEXT;
			break;
		case CMoviePlayerGui::PLAY:
			ismediaplayer = true;
			Logo = ICONSDIR "/" NEUTRINO_ICON_PLAY ICONSEXT;
			break;
		default:
			ismediaplayer = false;
			;
	}

	switch (CRecordManager::getInstance()->GetRecordMode())
	{
		case CRecordManager::RECMODE_REC_TSHIFT:
			recLocked = true;
			tsLocked = true;
			break;
		case CRecordManager::RECMODE_REC:
			recLocked = true;
			break;
		case CRecordManager::RECMODE_TSHIFT:
			tsLocked = true;
			break;
		default:
			recLocked = false;
			tsLocked = false;
			break;
	}

	int icon_start_width = 0, icon_start_height = 0;
	g_PicViewer->getSize(Logo.c_str(), &icon_start_width, &icon_start_height);

	if (g_settings.glcd_show_logo && percent_logo &&
		showImage(channel_id, Channel, t.glcd_channel_x_position, t.glcd_channel_y_position, bitmap->Width(), percent_logo * bitmap->Height()/100, true, false)) {
		doScrollChannel = false;
		scrollChannelSkip = 0;
	} else if (percent_logo && icon_start_width && icon_start_height &&
		doShowLcdIcon && showImage(Logo, icon_start_width, icon_start_height, t.glcd_channel_x_position, t.glcd_channel_y_position, bitmap->Width(), percent_logo * bitmap->Height()/100, true, false)) {
		doScrollChannel = false;
		scrollChannelSkip = 0;
	} else if (percent_channel) {
		if (ChannelWidth) {
			if (scrollChannelForward) {
				if (ChannelWidth - scrollChannelSkip < bitmap->Width())
					scrollChannelForward = false;
			} else if (scrollChannelSkip <= 0) {
				scrollChannelSkip = 0;
				doScrollChannel = false;
			}

			drawText(t.glcd_channel_x_position + scrollChannelOffset,
				t.glcd_channel_y_position, bitmap->Width(), ChannelWidth, Channel,
				&font_channel, ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent, true, scrollChannelSkip, t.glcd_align_channel);

			if (scrollChannelOffset > 0)
				scrollChannelOffset -= g_settings.glcd_scroll_speed;

			if (scrollChannelOffset < 0)
				scrollChannelOffset = 0;

			if (scrollChannelOffset == 0) {
				if (scrollChannelForward)
					scrollChannelSkip += g_settings.glcd_scroll_speed;
				else
					scrollChannelSkip -= g_settings.glcd_scroll_speed;
			}
		}
	}

	if (percent_epg)
	{
		if (EpgWidth)
		{
			if (scrollEpgForward) {
				if (EpgWidth - scrollEpgSkip < bitmap->Width())
					scrollEpgForward = false;
			} else if (scrollEpgSkip <= 0) {
				scrollEpgSkip = 0;
				doScrollEpg = false;
			}

			drawText(t.glcd_epg_x_position + scrollEpgOffset,
				t.glcd_epg_y_position, bitmap->Width(), EpgWidth, Epg,
				&font_epg, ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent, true, scrollEpgSkip, t.glcd_align_epg);

			if (scrollEpgOffset > 0)
				scrollEpgOffset -= g_settings.glcd_scroll_speed;

			if (scrollEpgOffset < 0)
				scrollEpgOffset = 0;

			if (scrollEpgOffset == 0) {
				if (scrollEpgForward)
					scrollEpgSkip += g_settings.glcd_scroll_speed;
				else
					scrollEpgSkip -= g_settings.glcd_scroll_speed;
			}
		}
	}

	if (percent_bar && t.glcd_show_progressbar)
	{
		showProgressBarBorder(
			t.glcd_bar_x_position,
			t.glcd_bar_y_position,
			t.glcd_bar_width,
			t.glcd_bar_y_position + t.glcd_percent_bar,
			Scale,
			GLCD::cColor::Gray,
			ColorConvert3to1(t.glcd_color_bar_red, t.glcd_color_bar_green, t.glcd_color_bar_blue)
		);
	}

	if (percent_time && t.glcd_show_time)
	{
		Lock();
		Time = strftime("%H:%M", tm);
		TimeWidth = font_time.Width(Time);
		Unlock();

		drawText(t.glcd_time_x_position,
			t.glcd_time_y_position, bitmap->Width() - 1, TimeWidth, Time,
			&font_time, ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent, true, 0, t.glcd_align_time);
	}

	if (percent_duration && t.glcd_show_duration)
	{
		Lock();
		Duration = stagingDuration;
		DurationWidth = font_duration.Width(Duration);
		Unlock();

		drawText(t.glcd_duration_x_position,
			t.glcd_duration_y_position, bitmap->Width() - 1, DurationWidth, Duration,
			&font_duration, ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent, true, 0, t.glcd_align_duration);
	}

	if (percent_start && t.glcd_show_start)
	{
		Lock();
		Start = stagingStart;
		StartWidth = font_start.Width(Start);
		Unlock();

		drawText(t.glcd_start_x_position,
			t.glcd_start_y_position, bitmap->Width() - 1, StartWidth, Start,
			&font_start, ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent, true, 0, t.glcd_align_start);
	}

	if (percent_end && t.glcd_show_end)
	{
		Lock();
		End = stagingEnd;
		EndWidth = font_end.Width(End);
 		Unlock();

		drawText(t.glcd_end_x_position,
			t.glcd_end_y_position, bitmap->Width() - 1, EndWidth, End,
			&font_end, ColorConvert3to1(t.glcd_color_fg_red, t.glcd_color_fg_green, t.glcd_color_fg_blue), GLCD::cColor::Transparent, true, 0, t.glcd_align_end);
	}

	if (percent_smalltext && !doStandby) {
		Lock();
		SmalltextWidth = font_smalltext.Width(Smalltext);
		Unlock();

		if (access("/tmp/ecm.info", F_OK) == 0)
		{
			struct stat buf;
			stat("/tmp/ecm.info", &buf);
			if (buf.st_size > 0)
				ecmLocked = true;
			else
				ecmLocked = false;
		}

		if (recLocked) {
			drawText(t.glcd_rec_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "rec", &font_smalltext, GLCD::cColor::Red,
				GLCD::cColor::Transparent, true, 0, 0);
		} else {
			drawText(t.glcd_rec_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "rec", &font_smalltext, GLCD::cColor::Gray,
				GLCD::cColor::Transparent, true, 0, 0);
		}

		if (muteLocked) {
			drawText(t.glcd_mute_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "mute", &font_smalltext, GLCD::cColor::Green,
				GLCD::cColor::Transparent, true, 0, 0);
		} else {
			drawText(t.glcd_mute_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "mute", &font_smalltext, GLCD::cColor::Gray,
				GLCD::cColor::Transparent, true, 0, 0);
		}

		if (tsLocked) {
			drawText(t.glcd_ts_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "ts", &font_smalltext, GLCD::cColor::Red,
				GLCD::cColor::Transparent, true, 0, 0);
		} else {
			drawText(t.glcd_ts_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "ts", &font_smalltext, GLCD::cColor::Gray,
				GLCD::cColor::Transparent, true, 0, 0);
		}

		if (ecmLocked) {
			drawText(t.glcd_ecm_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "ecm", &font_smalltext, GLCD::cColor::Green,
				GLCD::cColor::Transparent, true, 0, 0);
		} else {
			drawText(t.glcd_ecm_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "ecm", &font_smalltext, GLCD::cColor::Gray,
				GLCD::cColor::Transparent, true, 0, 0);
		}

		if (timerLocked) {
			drawText(t.glcd_timer_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "timer", &font_smalltext, GLCD::cColor::Green,
				GLCD::cColor::Transparent, true, 0, 0);
		} else {
			drawText(t.glcd_timer_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "timer", &font_smalltext, GLCD::cColor::Gray,
				GLCD::cColor::Transparent, true, 0, 0);
		}

		if (ddLocked) {
			drawText(t.glcd_dd_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "dd", &font_smalltext, GLCD::cColor::Green,
				GLCD::cColor::Transparent, true, 0, 0);
		} else {
			drawText(t.glcd_dd_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "dd", &font_smalltext, GLCD::cColor::Gray,
				GLCD::cColor::Transparent, true, 0, 0);
		}

		if (ismediaplayer) {
			if (subLocked) {
				drawText(t.glcd_txt_icon_x_position, t.glcd_smalltext_y_position,
					bitmap->Width() - 1, SmalltextWidth, "sub", &font_smalltext, GLCD::cColor::Green,
					GLCD::cColor::Transparent, true, 0, 0);
			} else {
				drawText(t.glcd_txt_icon_x_position, t.glcd_smalltext_y_position,
					bitmap->Width() - 1, SmalltextWidth, "sub", &font_smalltext, GLCD::cColor::Gray,
					GLCD::cColor::Transparent, true, 0, 0);
			}
		} else {
			if (txtLocked) {
				drawText(t.glcd_txt_icon_x_position, t.glcd_smalltext_y_position,
					bitmap->Width() - 1, SmalltextWidth, "txt", &font_smalltext, GLCD::cColor::Green,
					GLCD::cColor::Transparent, true, 0, 0);
			} else {
				drawText(t.glcd_txt_icon_x_position, t.glcd_smalltext_y_position,
					bitmap->Width() - 1, SmalltextWidth, "txt", &font_smalltext, GLCD::cColor::Gray,
					GLCD::cColor::Transparent, true, 0, 0);
			}
		}

		if (camLocked) {
			drawText(t.glcd_cam_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "cam", &font_smalltext, GLCD::cColor::Green,
				GLCD::cColor::Transparent, true, 0, 0);
		} else {
			drawText(t.glcd_cam_icon_x_position, t.glcd_smalltext_y_position,
				bitmap->Width() - 1, SmalltextWidth, "cam", &font_smalltext, GLCD::cColor::Gray,
				GLCD::cColor::Transparent, true, 0, 0);
		}
	}

	lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
	lcd->Refresh(false);
}

void cGLCD::updateFonts()
{
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	percent_logo = std::min(t.glcd_percent_logo, 100);
	percent_channel = std::min(t.glcd_percent_channel, 100);
	percent_epg = std::min(t.glcd_percent_epg, 100);
	percent_bar = std::min(t.glcd_percent_bar, 100);
	percent_time = std::min(t.glcd_percent_time, 100);
	percent_duration = std::min(t.glcd_percent_duration, 100);
	percent_start = std::min(t.glcd_percent_start, 100);
	percent_end = std::min(t.glcd_percent_end, 100);
	percent_smalltext = std::min(t.glcd_percent_smalltext, 100);

	// calculate height
	int fontsize_channel_new = percent_channel * cglcd->lcd->Height() / 100;
	int fontsize_epg_new = percent_epg * cglcd->lcd->Height() / 100;
	int fontsize_time_new = percent_time * cglcd->lcd->Height() / 100;
	int fontsize_duration_new = percent_duration * cglcd->lcd->Height() / 100;
	int fontsize_start_new = percent_start * cglcd->lcd->Height() / 100;
	int fontsize_end_new = percent_end * cglcd->lcd->Height() / 100;
	int fontsize_smalltext_new = percent_smalltext * cglcd->lcd->Height() / 100;

	if (!fonts_initialized || (fontsize_channel_new != fontsize_channel)) {
		fontsize_channel = fontsize_channel_new;
		if (!font_channel.LoadFT2(t.glcd_font, "UTF-8", fontsize_channel)) {
			t.glcd_font = g_settings.font_file;
			font_channel.LoadFT2(t.glcd_font, "UTF-8", fontsize_channel);
		}
	}
	if (!fonts_initialized || (fontsize_epg_new != fontsize_epg)) {
		fontsize_epg = fontsize_epg_new;
		if (!font_epg.LoadFT2(t.glcd_font, "UTF-8", fontsize_epg)) {
			t.glcd_font = g_settings.font_file;
			font_epg.LoadFT2(t.glcd_font, "UTF-8", fontsize_epg);
		}
	}
	if (!fonts_initialized || (fontsize_time_new != fontsize_time)) {
		fontsize_time = fontsize_time_new;
		if (!font_time.LoadFT2(t.glcd_font, "UTF-8", fontsize_time)) {
			t.glcd_font = g_settings.font_file;
			font_time.LoadFT2(t.glcd_font, "UTF-8", fontsize_time);
		}
	}

	if (!fonts_initialized || (fontsize_duration_new != fontsize_duration)) {
		fontsize_duration = fontsize_duration_new;
		if (!font_duration.LoadFT2(t.glcd_font, "UTF-8", fontsize_duration)) {
			t.glcd_font = g_settings.font_file;
			font_duration.LoadFT2(t.glcd_font, "UTF-8", fontsize_duration);
		}
	}

	if (!fonts_initialized || (fontsize_start_new != fontsize_start)) {
		fontsize_start = fontsize_start_new;
		if (!font_start.LoadFT2(t.glcd_font, "UTF-8", fontsize_start)) {
			t.glcd_font = g_settings.font_file;
			font_start.LoadFT2(t.glcd_font, "UTF-8", fontsize_start);
		}
	}

	if (!fonts_initialized || (fontsize_end_new != fontsize_end)) {
		fontsize_end = fontsize_end_new;
		if (!font_end.LoadFT2(t.glcd_font, "UTF-8", fontsize_end)) {
			t.glcd_font = g_settings.font_file;
			font_end.LoadFT2(t.glcd_font, "UTF-8", fontsize_end);
		}
	}

	if (!fonts_initialized || (fontsize_smalltext_new != fontsize_smalltext)) {
		fontsize_smalltext = fontsize_smalltext_new;
		if (!font_smalltext.LoadFT2(FONTDIR "/oled/terminator.ttf", "UTF-8", fontsize_smalltext)) {
			t.glcd_font = g_settings.font_file;
			font_smalltext.LoadFT2(t.glcd_font, "UTF-8", fontsize_smalltext);
		}
	}

	fonts_initialized = true;
}

bool cGLCD::getBoundingBox(uint32_t *buffer, int width, int height, int &bb_x, int &bb_y, int &bb_w, int &bb_h)
{
	if (!width || !height) {
		bb_x = bb_y = bb_w = bb_h = 0;
		return false;
	}

	int y_min = height;
	uint32_t *b = buffer;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++, b++)
			if (*b) {
				y_min = y;
				goto out1;
			}
out1:
	int y_max = y_min;
	b = buffer + height * width - 1;
	for (int y = height - 1; y_min < y; y--)
		for (int x = 0; x < width; x++, b--)
			if (*b) {
				y_max = y;
				goto out2;
			}
out2:
	int x_min = width;
	for (int x = 0; x < width; x++) {
		b = buffer + x + y_min * width;
		for (int y = y_min; y < y_max; y++, b += width)
			if (*b) {
				x_min = x;
				goto out3;
			}
	}
out3:
	int x_max = x_min;
	for (int x = width - 1; x_min < x; x--) {
		b = buffer + x + y_min * width;
		for (int y = y_min; y < y_max; y++, b += width)
			if (*b) {
				x_max = x;
				goto out4;
			}
	}
out4:
	bb_x = x_min;
	bb_y = y_min;
	bb_w = 1 + x_max - x_min;
	bb_h = 1 + y_max - y_min;

	if (bb_x < 0)
		bb_x = 0;
	if (bb_y < 0)
		bb_y = 0;

	return true;
}

void* cGLCD::Run(void *arg)
{
	cGLCD *me = (cGLCD *)arg;
	me->Run();
	pthread_exit(NULL);
}

void cGLCD::CountDown()
{
	if (timeout_cnt > 0)
	{
		timeout_cnt--;
		if (timeout_cnt == 0)
		{
			UpdateBrightness();
			cglcd->locked_countdown = false;
		}
	}
}

void cGLCD::WakeUp()
{
	int tmp = atoi(g_settings.glcd_brightness_dim_time.c_str());
	if (tmp > 0)
	{
		timeout_cnt = (unsigned int)tmp;
		UpdateBrightness();
		cglcd->locked_countdown = true;
	}
}

void* cGLCD::TimeThread(void *p)
{
	set_threadname("cGLCD:Time");
	((cGLCD *)p)->time_thread_started = true;
	while (((cGLCD *)p)->time_thread_started)
	{
		sleep(1);
		if (!cglcd->doExit && (
			   cglcd->locked_countdown == true
			|| cglcd->channelLocked == true
			|| cglcd->timeLocked == true
			|| cglcd->durationLocked == true
			|| cglcd->startLocked == true
			|| cglcd->endLocked == true
			|| cglcd->recLocked == true
			|| cglcd->muteLocked == true
			|| cglcd->tsLocked == true
			|| cglcd->ecmLocked == true
			|| cglcd->timerLocked == true
			|| cglcd->ddLocked == true
			|| cglcd->txtLocked == true
			|| cglcd->camLocked == true
			|| cglcd->doShowVolume == true
			|| cglcd->doMirrorOSD == true
		))
		{
			cGLCD::getInstance()->CountDown();
		}
	}
	return NULL;
}

void cGLCD::Run(void)
{
	SNeutrinoGlcdTheme &t = g_settings.glcd_theme;

	set_threadname("cGLCD::Run");

	if (GLCD::Config.Load(kDefaultConfigFile) == false)
	{
		fprintf(stderr, "Error loading config file!\n");
		return;
	}
	if ((GLCD::Config.driverConfigs.size() < 1))
	{
		fprintf(stderr, "No driver config found!\n");
		return;
	}

	struct timespec ts;

	CSectionsdClient::CurrentNextInfo info_CurrentNext;
	channel_id = -1;
	info_CurrentNext.current_zeit.startzeit = 0;
	info_CurrentNext.current_zeit.dauer = 0;
	info_CurrentNext.flags = 0;

	fonts_initialized = false;
	bool broken = false;

	do
	{
		if (broken)
		{
#ifdef GLCD_DEBUG
			fprintf(stderr, "No graphlcd display found ... sleeping for 30 seconds\n");
#endif
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 30;
			sem_timedwait(&sem, &ts);
			broken = false;
			if (doExit)
				break;
			if (!g_settings.glcd_enable)
				continue;
		} else
				while ((doSuspend || doStandby || !g_settings.glcd_enable) && !doExit)
					sem_wait(&sem);

		if (doExit)
			break;

		int warmUp = 10;

		if ((g_settings.glcd_selected_config < 0) || (g_settings.glcd_selected_config > GetConfigSize() - 1))
			g_settings.glcd_selected_config = 0;

		lcd = GLCD::CreateDriver(GLCD::Config.driverConfigs[g_settings.glcd_selected_config].id, &GLCD::Config.driverConfigs[g_settings.glcd_selected_config]);
		if (!lcd) {
#ifdef GLCD_DEBUG
			fprintf(stderr, "CreateDriver failed.\n");
#endif
			broken = true;
			continue;
		}
#ifdef GLCD_DEBUG
		fprintf(stderr, "CreateDriver succeeded.\n");
#endif
		if (lcd->Init())
		{
			delete lcd;
			lcd = NULL;
#ifdef GLCD_DEBUG
			fprintf(stderr, "LCD init failed.\n");
#endif
			broken = true;
			continue;
		}
#ifdef GLCD_DEBUG
		fprintf(stderr, "LCD init succeeded.\n");
#endif
		lcd->SetBrightness(0);

		if (!bitmap)
			bitmap = new GLCD::cBitmap(lcd->Width(), lcd->Height(), ColorConvert3to1(t.glcd_color_bg_red, t.glcd_color_bg_green, t.glcd_color_bg_blue));

		UpdateBrightness();
		Update();

		doMirrorOSD = false;

		while ((!doSuspend && !doStandby) && !doExit && g_settings.glcd_enable)
		{
			if (doMirrorOSD && !doStandbyTime && !doStandbyWeather)
			{
				if (blitFlag)
				{
					blitFlag = false;
					bitmap->Clear(GLCD::cColor::Black);
					ts.tv_sec = 0; // don't wait
					static CFrameBuffer* fb = CFrameBuffer::getInstance();
					static int fb_height = fb->getScreenHeight(true);
					static uint32_t *fbp = fb->getFrameBufferPointer();
					int lcd_width = bitmap->Width();
					int lcd_height = bitmap->Height();
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUULTIMO4K || BOXMODEL_VUUNO4KSE
					unsigned int fb_stride = fb->getStride()/4;
					if (!showImage(fbp, fb_stride, fb_height, 0, 0, lcd_width, lcd_height, false, false))
					{
#else
					static int fb_width = fb->getScreenWidth(true);
					if (!showImage(fbp, fb_width, fb_height, 0, 0, lcd_width, lcd_height, false, true))
					{
#endif
						usleep(500000);
					}
					else
					{
						lcd->SetScreen(bitmap->Data(), lcd_width, lcd_height);
						lcd->Refresh(false);
					}
				}
				else
					usleep(100000);
				continue;
			}

			if (g_settings.glcd_mirror_video && !doStandbyTime && !doStandbyWeather)
			{
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUULTIMO4K || BOXMODEL_VUUNO4KSE
				lcd->SetMirrorVideo(true);
#else
				char ws[10];
				snprintf(ws, sizeof(ws), "%d", bitmap->Width());
				const char *bmpShot = "/tmp/glcd-video.bmp";
				my_system(4, "/bin/grab", "-vr", ws, bmpShot);
				int bw = 0, bh = 0;
				g_PicViewer->getSize(bmpShot, &bw, &bh);
				if (bw > 0 && bh > 0)
				{
					int lcd_width = bitmap->Width();
					int lcd_height = bitmap->Height();
					if (!showImage(bmpShot, (uint32_t) bw, (uint32_t) bh, 0, 0, (uint32_t) lcd_width, (uint32_t) lcd_height, false, true))
						usleep(1000000);
					else
					{
						lcd->SetScreen(bitmap->Data(), lcd_width, lcd_height);
						lcd->Refresh(false);
					}
				}
				else
					usleep(1000000);
				continue;
#endif
			}
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUULTIMO4K || BOXMODEL_VUUNO4KSE
			else
				lcd->SetMirrorVideo(false);
#endif

			clock_gettime(CLOCK_REALTIME, &ts);
			tm = localtime(&ts.tv_sec);
			updateFonts();
			Exec();
			clock_gettime(CLOCK_REALTIME, &ts);
			tm = localtime(&ts.tv_sec);
			if (warmUp > 0)
			{
				ts.tv_sec += 1;
				warmUp--;
			}
			else
			{
				ts.tv_sec += 60 - tm->tm_sec;
				ts.tv_nsec = 0;
			}

			if (!doScrollChannel && !doScrollEpg)
				sem_timedwait(&sem, &ts);

			while(!sem_trywait(&sem));

			if(doRescan || doSuspend || doStandby || doExit)
				break;

			if (doShowVolume)
			{
				if (ismediaplayer)
				{
					Channel = "";
					if (Epg.compare(g_Locale->getText(LOCALE_GLCD_VOLUME)))
					{
						Epg = g_Locale->getText(LOCALE_GLCD_VOLUME);
						EpgWidth = font_epg.Width(Epg);
						doScrollEpg = EpgWidth > bitmap->Width();
						scrollEpgSkip = 0;
						scrollEpgForward = true;
						if (doScrollEpg) {
							scrollEpgOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
							EpgWidth += scrollEpgOffset;
						}
						else
							scrollEpgOffset = 0;
					}
					ChannelWidth = 0;
					scrollChannelSkip = 0;
					scrollChannelForward = true;
					Scale = g_settings.current_volume;
					//epg_id = -1;
				} else {
					Epg = "";
					if (Channel.compare(g_Locale->getText(LOCALE_GLCD_VOLUME)))
					{
						Channel = g_Locale->getText(LOCALE_GLCD_VOLUME);
						ChannelWidth = font_channel.Width(Channel);
						doScrollChannel = ChannelWidth > bitmap->Width();
						scrollChannelSkip = 0;
						scrollChannelForward = true;
						if (doScrollChannel) {
							scrollChannelOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
							ChannelWidth += scrollChannelOffset;
						}
						else
							scrollChannelOffset = 0;
					}
					EpgWidth = 0;
					scrollEpgSkip = 0;
					scrollEpgForward = true;
					Scale = g_settings.current_volume;
					channel_id = -1;
				}
			}
			else if (channelLocked)
			{
				Lock();
				if (Epg.compare(stagingEpg))
				{
					Epg = stagingEpg;
					EpgWidth = font_epg.Width(Epg);
					doScrollEpg = EpgWidth > bitmap->Width();
					scrollEpgSkip = 0;
					scrollEpgForward = true;
					if (doScrollEpg)
					{
						scrollEpgOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						EpgWidth += scrollEpgOffset;
					}
					else
						scrollChannelOffset = 0;
				}
				if (Channel.compare(stagingChannel))
				{
					Channel = stagingChannel;
					ChannelWidth = font_channel.Width(Channel);
					doScrollChannel = ChannelWidth > bitmap->Width();
					scrollChannelSkip = 0;
					scrollChannelForward = true;
					if (doScrollChannel)
					{
						scrollChannelOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						ChannelWidth += scrollChannelOffset;
					}
					else
						scrollChannelOffset = 0;
				}
				channel_id = -1;
				Unlock();
			}
			else
			{
				CChannelList *channelList = CNeutrinoApp::getInstance ()->channelList;
				if (!channelList)
					continue;
				t_channel_id new_channel_id = channelList->getActiveChannel_ChannelID();
				if (!new_channel_id)
					continue;

				if ((new_channel_id != channel_id))
				{
					Channel = channelList->getActiveChannelName ();
					ChannelWidth = font_channel.Width(Channel);
					Epg = "";
					EpgWidth = 0;
					Scale = 0;
					doScrollEpg = false;
					doScrollChannel = ChannelWidth > bitmap->Width();
					scrollChannelForward = true;
					scrollChannelSkip = 0;
					if (doScrollChannel) {
						scrollChannelOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						ChannelWidth += scrollChannelOffset;
					}
					else
						scrollChannelOffset = 0;
					warmUp = 10;
					info_CurrentNext.current_name = "";
					info_CurrentNext.current_zeit.dauer = 0;
				}

				CEitManager::getInstance()->getCurrentNextServiceKey(channel_id & 0xFFFFFFFFFFFFULL, info_CurrentNext);
				channel_id = new_channel_id;

				if (info_CurrentNext.current_name.compare(Epg))
				{
					Epg = info_CurrentNext.current_name;
					EpgWidth = font_epg.Width(Epg);
					doScrollEpg = EpgWidth > bitmap->Width();
					scrollEpgForward = true;
					scrollEpgSkip = 0;
					if (doScrollEpg)
					{
						scrollEpgOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						EpgWidth += scrollEpgOffset;
					} else
						scrollEpgOffset = 0;
				}

				if (CSectionsdClient::epgflags::has_current)
				{
					if ((info_CurrentNext.current_zeit.dauer > 0) && (info_CurrentNext.current_zeit.dauer < 86400))
					{
						Scale = (ts.tv_sec - info_CurrentNext.current_zeit.startzeit) * 100 / info_CurrentNext.current_zeit.dauer;
						char tmp_duration[6] = {0};
						int total = info_CurrentNext.current_zeit.dauer / 60;
						int done = (abs(time(NULL) - info_CurrentNext.current_zeit.startzeit) + 30) / 60;
						int todo = total - done;
						if ((time(NULL) < info_CurrentNext.current_zeit.startzeit) && todo >= 0)
						{
							done = 0;
							todo = info_CurrentNext.current_zeit.dauer / 60;
						}
						snprintf(tmp_duration, sizeof(tmp_duration), "%d/%d", done, total);
						Duration = stagingDuration = tmp_duration;
					}
					if (Scale > 100)
						Scale = 100;
					else if (Scale < 0)
						Scale = 0;
					char tmp_start[6] = {0};
					tm = localtime(&info_CurrentNext.current_zeit.startzeit);
					snprintf(tmp_start, sizeof(tmp_start), "%02d:%02d", tm->tm_hour, tm->tm_min);
					Start = stagingStart = tmp_start;
				}

				if (CSectionsdClient::epgflags::has_next)
				{
					char tmp_end[6] = {0};
					tm = localtime(&info_CurrentNext.next_zeit.startzeit);
					snprintf(tmp_end, sizeof(tmp_end), "%02d:%02d", tm->tm_hour, tm->tm_min);
					End = stagingEnd = tmp_end;
				}
			}
		}

		if (!g_settings.glcd_enable || doSuspend || doStandby)
		{
			// for restart, don't blacken screen
			bitmap->Clear(GLCD::cColor::Black);
			lcd->SetBrightness(0);
			lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
			lcd->Refresh(false);
		}
		if (doExit)
		{
			if (imageShow(DATADIR "/neutrino/icons/shutdown.jpg", 0, 0, 0, 0, false, true, true, false, false))
			{
				lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
				lcd->Refresh(false);
				sleep(1);
				lcd->SetBrightness(0);
			}
			else
			{
				bitmap->Clear(GLCD::cColor::Black);
				lcd->SetBrightness(0);
				lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
				lcd->Refresh(false);
			}
			return;
		}
		if (doRescan)
		{
			doRescan = false;
			Update();
		}
		lcd->DeInit();
		delete lcd;
		lcd = NULL;
	} while(!doExit);
}

void cGLCD::Update()
{
	if (cglcd)
	{
		sem_post(&cglcd->sem);
		if (!cglcd->doExit)
			cGLCD::getInstance()->WakeUp();
	}
}

void cGLCD::StandbyMode(bool b)
{
	if (cglcd)
	{
		if (g_settings.glcd_time_in_standby || g_settings.glcd_standby_weather)
		{
			if (g_settings.glcd_time_in_standby)
				cglcd->doStandbyTime = b;
			else
				cglcd->doStandbyTime = false;

			if (g_settings.glcd_standby_weather)
				cglcd->doStandbyWeather = b;
			else
				cglcd->doStandbyWeather = false;

			cglcd->doStandby = false;
		}
		else
			cglcd->doStandby = b;

		if (b)
		{
			cglcd->doScrollChannel = false;
			cglcd->doScrollEpg = false;
		}
		else
		{
			cglcd->doScrollChannel = true;
			cglcd->doScrollEpg = true;
		}
		cglcd->doMirrorOSD = false;
		cglcd->UpdateBrightness();
		cglcd->Update();
	}
}

void cGLCD::ShowVolume(bool b)
{
	if (cglcd)
	{
		cglcd->doShowVolume = b;
		cglcd->Update();
	}
}

void cGLCD::ShowLcdIcon(bool b)
{
	if (cglcd)
	{
		cglcd->Lock();
		cglcd->doShowLcdIcon = b;
		cglcd->Unlock();
		cglcd->Update();
	}
}

void cGLCD::MirrorOSD(bool b)
{
	if (cglcd)
	{
		cglcd->doMirrorOSD = b;
		cglcd->Update();
	}
}

void cGLCD::Exit()
{
	if (cglcd)
	{
		cglcd->doMirrorOSD = false;
		cglcd->doSuspend = false;
		cglcd->time_thread_started = false;
		cglcd->doExit = true;
		cglcd->Update();
		void *res;
		pthread_join(cglcd->thrGLCD, &res);
		pthread_join(cglcd->thrTimeThread, NULL);
		delete cglcd;
		cglcd = NULL;
	}
}

void cGLCD::Respawn()
{
	Exit();
	cGLCD::getInstance();
}

void cGLCD::Rescan()
{
	doRescan = true;
	Update();
}

void cGLCD::Suspend()
{
	if (cglcd)
	{
		cglcd->doSuspend = true;
		cglcd->Update();
	}
}

void cGLCD::Resume()
{
	if (cglcd)
	{
		cglcd->doSuspend = false;
		cglcd->channelLocked = false;
		cglcd->Update();
	}
}

void cGLCD::lockChannel(std::string c, std::string e, int s)
{
	if(cglcd)
	{
		cglcd->Lock();
		cglcd->channelLocked = true;
		cglcd->stagingChannel = c;
		cglcd->stagingEpg = e;
		cglcd->Scale = s;
		cglcd->Unlock();
		cglcd->Update();
	}
}

void cGLCD::unlockChannel(void)
{
	if(cglcd)
	{
		cglcd->channelLocked = false;
		cglcd->Update();
	}
}

void cGLCD::lockTime(std::string t)
{
	if(cglcd)
	{
		cglcd->Lock();
		cglcd->timeLocked = true;
		cglcd->stagingTime = t;
		cglcd->Unlock();
		cglcd->Update();
	}
}

void cGLCD::unlockTime(void)
{
	if(cglcd)
	{
		cglcd->timeLocked = false;
		cglcd->Update();
	}
}

void cGLCD::lockDuration(std::string t)
{
	if(cglcd)
	{
		cglcd->Lock();
		cglcd->durationLocked = true;
		cglcd->stagingDuration = t;
		cglcd->Unlock();
		cglcd->Update();
	}
}

void cGLCD::unlockDuration(void)
{
	if(cglcd)
	{
		cglcd->durationLocked = false;
		cglcd->Update();
	}
}

void cGLCD::lockStart(std::string t)
{
	if(cglcd)
	{
		cglcd->Lock();
		cglcd->startLocked = true;
		cglcd->stagingStart = t;
		cglcd->Unlock();
		cglcd->Update();
	}
}

void cGLCD::unlockStart(void)
{
	if(cglcd)
	{
		cglcd->startLocked = false;
		cglcd->Update();
	}
}

void cGLCD::lockEnd(std::string t)
{
	if(cglcd)
	{
		cglcd->Lock();
		cglcd->endLocked = true;
		cglcd->stagingEnd = t;
		cglcd->Unlock();
		cglcd->Update();
	}
}

void cGLCD::unlockEnd(void)
{
	if(cglcd)
	{
		cglcd->endLocked = false;
		cglcd->Update();
	}
}

void cGLCD::lockIcon(int type)
{
	if(cglcd)
	{
		cglcd->Lock();
		if (type == REC)
			cglcd->recLocked = true;
		else if (type == MUTE)
			cglcd->muteLocked = true;
		else if (type == TS)
			cglcd->tsLocked = true;
		else if (type == ECM)
			cglcd->ecmLocked = true;
		else if (type == TIMER)
			cglcd->timerLocked = true;
		else if (type == DD)
			cglcd->ddLocked = true;
		else if (type == TXT)
			cglcd->txtLocked = true;
		else if (type == SUB)
			cglcd->subLocked = true;
		else if (type == CAM)
			cglcd->camLocked = true;
		cglcd->Unlock();
		cglcd->Update();
	}
}

void cGLCD::unlockIcon(int type)
{
	if(cglcd)
	{
		if (type == REC)
			cglcd->recLocked = false;
		else if (type == MUTE)
			cglcd->muteLocked = false;
		else if (type == TS)
			cglcd->tsLocked = false;
		else if (type == ECM)
			cglcd->ecmLocked = false;
		else if (type == TIMER)
			cglcd->timerLocked = false;
		else if (type == DD)
			cglcd->ddLocked = false;
		else if (type == TXT)
			cglcd->txtLocked = false;
		else if (type == SUB)
			cglcd->subLocked = false;
		else if (type == CAM)
			cglcd->camLocked = false;
		cglcd->Update();
	}
}

bool cGLCD::showProgressBarBorder(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t scale, uint32_t color_border, uint32_t color_progress)
{
	cglcd->bitmap->DrawRectangle(x1, y1, x1 + (x2 - x1 - 1), y2, color_border, false);
	if (scale) {
		cglcd->bitmap->DrawRectangle(x1 + 1, y1 + 1, x1 + (scale * (x2 - x1 - 1) / 100), y2 - 1, color_progress, true);
		return true;
	} else
		return false;
}

bool cGLCD::showImage(fb_pixel_t *s, uint32_t sw, uint32_t sh, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp, bool maximize)
{
	int bb_x, bb_y, bb_w, bb_h;

	if (cglcd->getBoundingBox(s, sw, sh, bb_x, bb_y, bb_w, bb_h) && bb_w && bb_h)
	{
		if (!maximize)
		{
			if (bb_h * dw > bb_w * dh)
			{
				uint32_t dw_new = dh * bb_w / bb_h;
				dx += (dw - dw_new) >> 1;
				dw = dw_new;
			} else {
				uint32_t dh_new = dw * bb_h / bb_w;
				dy += (dh - dh_new) >> 1;
				dh = dh_new;
			}
		}
		for (u_int y = 0; y < dh; y++)
		{
			for (u_int x = 0; x < dw; x++)
			{
				uint32_t pix = *(s + (y * bb_h / dh + bb_y) * sw + x * bb_w / dw + bb_x);
				if (!transp || pix)
					cglcd->bitmap->DrawPixel(x + dx, y + dy, pix);
			}
		}
		return true;
	}
	return false;
}

bool cGLCD::showImage(const std::string & filename, uint32_t sw, uint32_t sh, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp, bool maximize)
{
	bool res = false;
	if (!dw || !dh)
		return res;
	fb_pixel_t *s = g_PicViewer->getImage(filename, sw, sh);
	if (s && sw && sh)
		res = showImage(s, sw, sh, dx, dy, dw, dh, transp, maximize);
	if (s)
		free(s);
	return res;
}

bool cGLCD::showImage(uint64_t cid, std::string cname, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp, bool maximize)
{
	std::string logo;
	int sw, sh;

	if (g_PicViewer->GetLogoName(cid, cname, logo, &sw, &sh))
	{
		return showImage(logo, (uint32_t) sw, (uint32_t) sh, dx, dy, dw, dh, transp, maximize);
	}
	return false;
}

bool cGLCD::imageShow(const std::string & filename, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp, bool maximize, bool clear, bool center_sw, bool center_sh)
{
	bool ret = false;
	int sw, sh;

	g_PicViewer->getSize(filename.c_str(), &sw, &sh);
	if (sw && sh)
	{
		if (clear)
			cglcd->bitmap->Clear(GLCD::cColor::Black);
		if (maximize)
			ret = showImage(filename, (uint32_t) sw, (uint32_t) sh, (uint32_t) dx, (uint32_t) dy, (uint32_t) cglcd->bitmap->Width(), (uint32_t) cglcd->bitmap->Height(), transp, false);
		else
			if (center_sw || center_sh)
			{
				int move_sw = 0;
				int move_sh = 0;
				if (center_sw)
					move_sw = dx - (sw / 2);
				else
					move_sw = dx;
				if (center_sh)
					move_sh = dy - (sh / 2);
				else
					move_sh = dy;
				if (dw > 0 && dh > 0)
					ret = showImage(filename, (uint32_t) sw, (uint32_t) sh, (uint32_t) move_sw, (uint32_t) move_sh, (uint32_t) dw, (uint32_t) dh, transp, false);
				else
					ret = showImage(filename, (uint32_t) sw, (uint32_t) sh, (uint32_t) move_sw, (uint32_t) move_sh, (uint32_t) sw, (uint32_t) sh, transp, false);
			}
			else
				if (dw > 0 && dh > 0)
					ret = showImage(filename, (uint32_t) sw, (uint32_t) sh, (uint32_t) dx, (uint32_t) dy, (uint32_t) dw, (uint32_t) dh, transp, false);
				else
					ret = showImage(filename, (uint32_t) sw, (uint32_t) sh, (uint32_t) dx, (uint32_t) dy, (uint32_t) sw, (uint32_t) sh, transp, false);
	}
	return ret;
}

bool cGLCD::drawText(int x, int y, int xmax, int text_width, const std::string & text, const GLCD::cFont * font, uint32_t color1, uint32_t color2, bool proportional, int skipPixels, int align)
{
	int z = 0;
	int offset = 10; // px

	if (align == ALIGN_NONE)
	{
		z = x;
	}
	else if (align == ALIGN_LEFT)
	{
		z = offset;
	}
	else if (align == ALIGN_CENTER)
	{
		z = std::max(offset, (bitmap->Width() - text_width) / 2);
	}
	else if (align == ALIGN_RIGHT)
	{
		z = std::max(offset, (bitmap->Width() - text_width - offset));
	}

	return bitmap->DrawText(z, y, xmax, text, font, color1, color2, proportional, skipPixels);
}

bool cGLCD::dumpBuffer(fb_pixel_t *s, int format, const char *filename)
{
	int output_bytes = 4;

	int jpg_quality = 90;

	int xres = bitmap->Width();
	int yres = bitmap->Height();

	unsigned char *output = (unsigned char *)s;

	FILE *fd = fopen(filename, "wr");
	if (!fd)
		return false;

	if(cglcd)
		cglcd->Lock();

	if (format == BMP) {
		// write bmp
		unsigned char hdr[14 + 40];
		int i = 0;
#define PUT32(x) hdr[i++] = ((x)&0xFF); hdr[i++] = (((x)>>8)&0xFF); hdr[i++] = (((x)>>16)&0xFF); hdr[i++] = (((x)>>24)&0xFF);
#define PUT16(x) hdr[i++] = ((x)&0xFF); hdr[i++] = (((x)>>8)&0xFF);
#define PUT8(x) hdr[i++] = ((x)&0xFF);
		PUT8('B'); PUT8('M');
		PUT32((((xres * yres) * 3 + 3) &~ 3) + 14 + 40);
		PUT16(0); PUT16(0); PUT32(14 + 40);
		PUT32(40); PUT32(xres); PUT32(yres);
		PUT16(1);
		PUT16(output_bytes*8); // bits
		PUT32(0); PUT32(0); PUT32(0); PUT32(0); PUT32(0); PUT32(0);
#undef PUT32
#undef PUT16
#undef PUT8
		fwrite(hdr, 1, i, fd);

		int y;
		for (y=yres-1; y>=0 ; y-=1)
			fwrite(output + (y * xres * output_bytes), xres * output_bytes, 1, fd);
	 } else if (format == JPG) {
		const int row_stride = xres * output_bytes;
		// write jpg
		if (output_bytes == 3) // swap bgr<->rgb
		{
			int y;
			//#pragma omp parallel for shared(output)
			for (y = 0; y < yres; y++)
			{
				int xres1 = y * xres * 3;
				int xres2 = xres1 + 2;
				int x;
				for (x = 0; x < xres; x++)
				{
					int x2 = x * 3;
					SWAP(output[x2 + xres1], output[x2 + xres2]);
				}
			}
		}
		else // swap bgr<->rgb and eliminate alpha channel jpgs are always saved with 24bit without alpha channel
		{
			int y;
			//#pragma omp parallel for shared(output)
			for (y = 0; y < yres; y++)
			{
				unsigned char *scanline = output + (y * row_stride);
				int x;
				for (x=0; x<xres; x++)
				{
					const int xs = x * 4;
					const int xd = x * 3;
					scanline[xd + 0] = scanline[xs + 2];
					scanline[xd + 1] = scanline[xs + 1];
					scanline[xd + 2] = scanline[xs + 0];
				}
			}
		}

		struct jpeg_compress_struct cinfo;
		struct jpeg_error_mgr jerr;
		JSAMPROW row_pointer[1];
		cinfo.err = jpeg_std_error(&jerr);

		jpeg_create_compress(&cinfo);
		jpeg_stdio_dest(&cinfo, fd);
		cinfo.image_width = xres;
		cinfo.image_height = yres;
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
		cinfo.dct_method = JDCT_IFAST;
		jpeg_set_defaults(&cinfo);
		jpeg_set_quality(&cinfo,jpg_quality, TRUE);
		jpeg_start_compress(&cinfo, TRUE);
		while (cinfo.next_scanline < cinfo.image_height)
		{
			row_pointer[0] = & output[cinfo.next_scanline * row_stride];
			(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
		}
		jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);
	 } else if (format == PNG) {
		// write png
		png_bytep *row_pointers;
		png_structp png_ptr;
		png_infop info_ptr;

		png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, (png_error_ptr)NULL, (png_error_ptr)NULL);
		info_ptr = png_create_info_struct(png_ptr);
		png_init_io(png_ptr, fd);

		row_pointers=(png_bytep*)malloc(sizeof(png_bytep)*yres);

		int y;
		//#pragma omp parallel for shared(output)
		for (y=0; y<yres; y++)
			row_pointers[y]=output+(y*xres*output_bytes);

		png_set_bgr(png_ptr);
		png_set_IHDR(png_ptr, info_ptr, xres, yres, 8, ((output_bytes<4)?PNG_COLOR_TYPE_RGB:PNG_COLOR_TYPE_RGBA) , PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		png_set_compression_level(png_ptr, Z_BEST_SPEED);
		png_write_info(png_ptr, info_ptr);
		png_write_image(png_ptr, row_pointers);
		png_write_end(png_ptr, info_ptr);
		png_destroy_write_struct(&png_ptr, &info_ptr);

		free(row_pointers);
	}

	if(cglcd)
		cglcd->Unlock();

	fclose(fd);
	return true;
}

void cGLCD::UpdateBrightness()
{
	int dim_time = atoi(g_settings.glcd_brightness_dim_time.c_str());
	int dim_brightness = g_settings.glcd_brightness_dim;
	bool timeouted = (dim_time > 0) && (timeout_cnt == 0);

	if (cglcd && cglcd->lcd)
	{
		if (timeouted && !cglcd->doStandbyTime && !cglcd->doStandbyWeather)
			cglcd->lcd->SetBrightness((unsigned int) (dim_brightness));
		else
			cglcd->lcd->SetBrightness((unsigned int) ((cglcd->doStandbyTime || cglcd->doStandbyWeather) ? g_settings.glcd_brightness_standby : g_settings.glcd_brightness));

	}
}

void cGLCD::SetBrightness(unsigned int b)
{
	if (cglcd)
		cglcd->SetBrightness(b);
}

void cGLCD::TogglePower()
{
	if (cglcd)
	{
		cglcd->power_state = 1 - cglcd->power_state;
		if (cglcd->power_state)
			cglcd->Resume();
		else
			cglcd->Suspend();
	}
}

void cGLCD::Blit()
{
	if (cglcd)
		cglcd->blitFlag = true;
}

int cGLCD::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t /* data */)
{
	if (msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG)
	{
		Update();
		return messages_return::handled;
	}

	return messages_return::unhandled;
}

int cGLCD::GetConfigSize()
{
	return (int) GLCD::Config.driverConfigs.size();
}

std::string cGLCD::GetConfigName(int driver)
{
	if ((driver < 0) || (driver > GetConfigSize() - 1))
		driver = 0;
	return GLCD::Config.driverConfigs[driver].name;
}
