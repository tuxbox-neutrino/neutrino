/*
	nglcd.h -- Neutrino GraphLCD driver

	Copyright (C) 2012-2014 martii

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef ENABLE_GRAPHLCD
#ifndef __glcd_h__
#define __glcd_h__

#include <string>
#include <vector>
#include <time.h>
#include <string>
#include <sys/types.h>
#include <semaphore.h>
#include <zapit/include/zapit/client/zapittypes.h>
#include <neutrinoMessages.h>

#define CLAMP(x)    ((x < 0) ? 0 : ((x > 255) ? 255 : x))
#define SWAP(x,y)	{ x ^= y; y ^= x; x ^= y; }

#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <glcdgraphics/bitmap.h>
#include <glcdgraphics/font.h>
#include <glcddrivers/config.h>
#include <glcddrivers/driver.h>
#include <glcddrivers/drivers.h>
#pragma GCC diagnostic warning "-Wunused-parameter"

class cGLCD
{
	private:
		int fontsize_channel;
		int fontsize_epg;
		int fontsize_time;
		int fontsize_duration;
		int fontsize_start;
		int fontsize_end;
		int fontsize_smalltext;
		int percent_channel;
		int percent_time;
		int percent_duration;
		int percent_start;
		int percent_end;
		int percent_epg;
		int percent_smalltext;
		int percent_bar;
		int percent_logo;
		int power_state;
		std::string Logo;
		std::string Channel;
		std::string Epg;
		std::string Time;
		std::string Duration;
		std::string Start;
		std::string End;
		std::string Smalltext;
		std::string Temperature;
		std::string stagingChannel;
		std::string stagingEpg;
		std::string stagingTime;
		std::string stagingDuration;
		std::string stagingStart;
		std::string stagingEnd;
		std::string stagingSmalltext;
		t_channel_id channel_id;
		int Scale;
		time_t now;
		struct tm *tm;
		int EpgWidth;
		int ChannelWidth;
		int TimeWidth;
		int DurationWidth;
		int StartWidth;
		int EndWidth;
		int SmalltextWidth;
		int scrollEpgSkip;
		int scrollChannelSkip;
		int scrollEpgOffset;
		int scrollChannelOffset;
		bool scrollEpgForward;
		bool scrollChannelForward;
		bool blitFlag;
		bool channelLocked;
		bool timeLocked;
		bool durationLocked;
		bool startLocked;
		bool endLocked;
		bool recLocked;
		bool muteLocked;
		bool tsLocked;
		bool ecmLocked;
		bool timerLocked;
		bool ddLocked;
		bool txtLocked;
		bool subLocked;
		bool camLocked;
		bool doRescan;
		bool doSuspend;
		bool doStandby;
		bool doStandbyTime;
		bool doExit;
		bool doScrollChannel;
		bool doScrollEpg;
		bool doShowVolume;
		bool doShowLcdIcon;
		bool doMirrorOSD;
		bool fonts_initialized;
		bool ismediaplayer;
		int timeout_cnt;
		bool locked_countdown;
		bool time_thread_started;
		pthread_t thrGLCD;
		pthread_t thrTimeThread;
		pthread_mutex_t mutex;
		sem_t sem;
		void updateFonts();
		bool showImage(fb_pixel_t *s,
			uint32_t sw, uint32_t sh,
			uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh,
			bool transp = false, bool maximize = false);
		bool showImage(const std::string & filename,
			uint32_t sw, uint32_t sh,
			uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh,
			bool transp = false, bool maximize = false);
		bool showImage(uint64_t channel_id, std::string ChannelName,
			uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh,
			bool transp = false, bool maximize = false);
		bool getBoundingBox(uint32_t *buffer,
			int width, int height,
			int &bb_x, int &bb_y, int &bb_width, int &bb_height);
		void Exec();
		void CountDown();
		void WakeUp();
		static void	*TimeThread(void *);
		void Run(void);
		static void* Run(void *);
		static void Lock();
		static void Unlock();
	public:
		enum {
			BMP = 0,
			JPG = 1,
			PNG = 2,
		};
		enum {
			ALIGN_NONE = 0,
			ALIGN_LEFT = 1,
			ALIGN_CENTER = 2,
			ALIGN_RIGHT = 3,
		};
		enum {
			REC = 0,
			MUTE = 1,
			TS = 2,
			ECM = 3,
			TIMER = 4,
			DD = 5,
			TXT = 6,
			SUB = 7,
			CAM = 8,
		};
		GLCD::cDriver * lcd;
		GLCD::cFont font_channel;
		GLCD::cFont font_epg;
		GLCD::cFont font_time;
		GLCD::cFont font_duration;
		GLCD::cFont font_start;
		GLCD::cFont font_end;
		GLCD::cFont font_smalltext;
		GLCD::cBitmap * bitmap;
		cGLCD();
		~cGLCD();
		uint32_t ColorConvert3to1(uint32_t red, uint32_t green, uint32_t blue);
		void DeInit();
		void Rescan();
		bool showProgressBarBorder(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t scale, uint32_t color_border, uint32_t color_progress);
		bool imageShow(const std::string & filename, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp = false, bool maximize = false, bool clear = false, bool center_sw = false, bool center_sh = false);
		bool drawText(int x, int y, int xmax, int text_width, const std::string & text, const GLCD::cFont * font, uint32_t color1, uint32_t color2, bool proportional, int skipPixels, int align);
		static cGLCD *getInstance();
		static void lockChannel(std::string txt, std::string epg = "", int scale = 0);
		static void unlockChannel();
		static void lockTime(std::string time);
		static void unlockTime();
		static void lockDuration(std::string time);
		static void unlockDuration();
		static void lockStart(std::string time);
		static void unlockStart();
		static void lockEnd(std::string time);
		static void unlockEnd();
		static void lockIcon(int type = 0);
		static void unlockIcon(int type = 0);
		static void MirrorOSD(bool b = true);
		static void Update();
		static void Suspend();
		static void StandbyMode(bool);
		static void ShowVolume(bool);
		static void ShowLcdIcon(bool);
		static void Resume();
		static void Exit();
		static void Blit();
		static void SetBrightness(unsigned int b);
		static void TogglePower();
		bool dumpBuffer(fb_pixel_t *s, int format, const char *filename);
		void UpdateBrightness();
		int handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data);
};
#endif
#endif
