/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd
	Copyright (C) 2017 M. Liebmann (micha-bbg)

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __screenshot_h_
#define __screenshot_h_

#ifdef SCREENSHOT
  #if BOXMODEL_VUPLUS_ALL
    #define SCREENSHOT_EXTERNAL 1
  #else
    #define SCREENSHOT_INTERNAL 1
  #endif
#endif

#include <pthread.h>

class CScreenShot
{
	public:
		typedef enum {
			FORMAT_PNG,
			FORMAT_JPG,
			FORMAT_BMP
		} screenshot_format_t;

	private:
		screenshot_format_t format;
		std::string filename;
		int xres;
		int yres;
		bool extra_osd;
		bool get_osd;
		bool get_video;
		bool scale_to_video;

#if SCREENSHOT_INTERNAL
		unsigned char * pixel_data;
		FILE *fd;
		pthread_t  scs_thread;
		pthread_mutex_t thread_mutex;
		pthread_mutex_t getData_mutex;

		bool GetData();
		bool OpenFile();
		bool SaveFile();

		bool SavePng();
		bool SaveJpg();
		bool SaveBmp();

		bool startThread();
		static void* initThread(void *arg);
		void runThread();
		static void cleanupThread(void *arg);

#ifdef BOXMODEL_CST_HD2
		bool mergeOsdScreen(uint32_t dx, uint32_t dy, fb_pixel_t* osdData);
#endif
#endif // SCREENSHOT_INTERNAL

	public:
		CScreenShot(const std::string &fname = "", screenshot_format_t fmt = CScreenShot::FORMAT_JPG);
		~CScreenShot();

		void MakeFileName(const t_channel_id channel_id);
		void SetSize(int w, int h) { xres = w; yres = h; }
		void EnableVideo(bool enable) { get_video = enable; }
		void EnableOSD(bool enable) { get_osd = enable; }
		void ScaleToVideo(bool enable) { scale_to_video = enable; }
		bool Start();
		bool StartSync();
};

#endif
