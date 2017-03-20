/*
        Copyright (C) 2015 CoolStream International Ltd

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

#ifndef __MOVIE_CUT__
#define __MOVIE_CUT__

#include <driver/movieinfo.h>
#include <gui/components/cc.h>

class CFrameBuffer;
class CMovieCut
{
	private:
		CProgressBar *timescale;
		CFrameBuffer * frameBuffer;
		int x;
		int y;
		int percent;

		void reset_atime(const char * path, time_t tt);
		uint32_t getHeaderDurationMS(MI_MOVIE_INFO * minfo);
		off64_t getSecondSize(MI_MOVIE_INFO * minfo);
		void WriteHeader(const char * path, uint32_t duration);
		int check_pes_start (unsigned char *packet);
		int find_gop(unsigned char *buf, int r);
		off64_t fake_read(int fd, unsigned char *buf, size_t size, off64_t fsize);
		int read_psi(const char * spart, unsigned char * buf);
		void save_info(MI_MOVIE_INFO * minfo, char * dpart, off64_t spos, off64_t secsize);
		void findNewName(const char * fname, char * dpart,size_t dpart_len);
		static int compare_book(const void *x, const void *y);
		int getInput();

		void paintProgress(bool refresh);

	public:
		CMovieCut();
		~CMovieCut();
		bool truncateMovie(MI_MOVIE_INFO * minfo);
		bool cutMovie(MI_MOVIE_INFO * minfo);
		bool copyMovie(MI_MOVIE_INFO * minfo, bool onefile);
		//int handleMsg(const neutrino_msg_t _msg, neutrino_msg_data_t data);
};

#endif
