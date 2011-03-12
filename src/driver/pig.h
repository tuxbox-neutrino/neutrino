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


#ifndef __PIG_CONTROL__
#define __PIG_CONTROL__
#include <config.h>
using namespace std;


//
//  -- Picture in Graphics  Control
//  --  2002-11  rasc
//  --  2003-06  rasc   V4L API
//



#if 1
#include <linux/videodev.h>

#define PIG_DEV "/dev/v4l/video"		// PigNr will be appended!
#else
#include <dbox/avia_gt_pig.h>
#define PIG_DEV "/dev/dbox/pig"
#endif


class CPIG
{
	public:
		CPIG ();
		CPIG (int pig_nr);		// incl. open
		CPIG (int pig_nr, int x, int y, int w, int h); // open + set_coord
		~CPIG ();

		int  pigopen  (int pig_nr);
		void pigclose (void);
		void set_coord (int x, int y, int w, int h);
		void set_xy    (int x, int y);
		void set_size  (int w, int h);
//		void set_source(int x, int y);
//		void set_stackpos  (int pos);
		void show (void);
		void show (int x, int y, int w, int h);
		void hide (void);

		enum PigStatus { CLOSED, HIDE, SHOW };
		PigStatus getStatus(void);

	private:
		void _set_window  (int x, int y, int w, int h);

		int	fd;			// io descriptor
		int	px, py, pw, ph;		// pig frame
		int	stackpos;		// Order (Framebuffer, PIGs)
		PigStatus  status;		// on display?


};


#endif


