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


#ifndef __CAPTURE_CONTROL__
#define __CAPTURE_CONTROL__


#define __v4l_capture


using namespace std;


//
//  -- Picture Capture  Control
//  --  2003-12  rasc
//

#ifdef __v4l_capture

#include <linux/videodev.h>
#define CAPTURE_DEV "/dev/v4l/video"		// CaptureNr will be appended!

#else

#include <dbox/avia_gt_capture.h>
#define CAPTURE_DEV "/dev/dbox/capture"		// CaptureNr will be appended!

#endif


class CCAPTURE
{
	public:
		CCAPTURE ();
		CCAPTURE (int capture_nr);		// incl. open
		CCAPTURE (int capture_nr, int x, int y, int w, int h); // open + set_coord
		~CCAPTURE ();

		int  captureopen  (int capture_nr);
		void captureclose (void);
		void set_coord (int x, int y, int w, int h);
		void set_xy    (int x, int y);
		void set_size  (int w, int h);
		void set_output_size (int w, int h);
		u_char *readframe (u_char *buf);

	private:
		void _set_window  (int x, int y, int w, int h);

		int	fd;			// io descriptor
		int	cx, cy, cw, ch;		// capture size
		int	out_w, out_h;		// capture output size
		int	stride;

};



#endif


