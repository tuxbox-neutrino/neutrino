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

#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <glcdgraphics/bitmap.h>
#include <glcdgraphics/font.h>
#include <glcddrivers/config.h>
#include <glcddrivers/driver.h>
#include <glcddrivers/drivers.h>
#pragma GCC diagnostic warning "-Wunused-parameter"

class nGLCD
{
	private:
		GLCD::cDriver * lcd;
		GLCD::cFont font_channel;
		GLCD::cFont font_epg;
		GLCD::cFont font_time;
		GLCD::cFont font_time_standby;
		int fontsize_channel;
		int fontsize_epg;
		int fontsize_time;
		int fontsize_time_standby;
		int percent_channel;
		int percent_time;
		int percent_time_standby;
		int percent_epg;
		int percent_bar;
		int percent_logo;
		int percent_space;
		GLCD::cBitmap * bitmap;
		std::string Channel;
		std::string Epg;
		std::string stagingChannel;
		std::string stagingEpg;
		t_channel_id channel_id;
		int Scale;
		time_t now;
		struct tm *tm;
		int EpgWidth;
		int ChannelWidth;
		int scrollEpgSkip;
		int scrollChannelSkip;
		int scrollEpgOffset;
		int scrollChannelOffset;
		bool scrollEpgForward;
		bool scrollChannelForward;
		bool blitFlag;
		bool channelLocked;
		bool doRescan;
		bool doSuspend;
		bool doStandby;
		bool doStandbyTime;
		bool doExit;
		bool doScrollChannel;
		bool doScrollEpg;
		bool doShowVolume;
		bool doMirrorOSD;
		bool fonts_initialized;
		pthread_t thrGLCD;
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
		void LcdAnalogClock(int posx,int posy,int dia);
		void Exec();
		void Run(void);
		static void* Run(void *);
		static void Lock();
		static void Unlock();
	public:
		nGLCD();
		~nGLCD();
		void DeInit();
		void Rescan();
		static nGLCD *getInstance();
		static void lockChannel(std::string txt, std::string epg = "", int scale = 0);
		static void unlockChannel();
		static void MirrorOSD(bool b = true);
		static void Update();
		static void Suspend();
		static void StandbyMode(bool);
		static void ShowVolume(bool);
		static void Resume();
		static void Exit();
		static void Blit();
		static void SetBrightness(unsigned int b);
		void UpdateBrightness();
		int handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data);
};
#endif
#endif
