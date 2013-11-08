/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd

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

#include <OpenThreads/Thread>

class CScreenShot : public OpenThreads::Thread
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
		unsigned char * pixel_data;
		int xres;
		int yres;
		bool get_osd;
		bool get_video;
		bool scale_to_video;
		FILE *fd;

		bool GetData();
		bool OpenFile();
		bool SaveFile();

		bool SavePng();
		bool SaveJpg();
		bool SaveBmp();
		void run();

	public:
		CScreenShot(const std::string fname = "", screenshot_format_t fmt = CScreenShot::FORMAT_JPG);
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
