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





#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <error.h>
#include <stdlib.h>

#include "capture.h"


#include <stdio.h>
#warning  "experimental..."


//
//  -- TV Picture Capture 
//  -- This module is a class to provide a capture abstraction layer
//  --
//  --  2003-12  rasc
//



//
// -- Constructor
//

CCAPTURE::CCAPTURE()
{
    fd = -1;
}


CCAPTURE::CCAPTURE(int capture_nr)
{
    fd = -1;
    fd = captureopen(capture_nr);
}


CCAPTURE::CCAPTURE(int capture_nr, int x, int y, int w, int h)
{
    fd = -1;
    fd = captureopen (capture_nr);
    set_coord (x,y, w,h);
}




CCAPTURE::~CCAPTURE()
{
	captureclose ();		// cleanup anyway!!
}


//
// -- open Capture device
// -- return >=0: ok
//

int CCAPTURE::captureopen (int cap_nr)
{
   char  *capturedevs[] = {
		CAPTURE_DEV "0"		// CAPTURE device 0
		// CAPTURE_DEV "1",	// CAPTURE device 1
		// CAPTURE_DEV "2",	// CAPTURE device 2
		// CAPTURE_DEV "3"	// CAPTURE device 3
   };


    if ( (cap_nr>0) || (cap_nr < (int)(sizeof (capturedevs)/sizeof(char *))) ) {
    
	if (fd < 0) {
		fd = open( capturedevs[cap_nr], O_RDWR );
		if (fd >= 0) {
			cx = cy = cw = ch = 0;
			out_w = out_h = 0;
			stride = 0;
		} else perror (capturedevs[cap_nr]);
		return fd;
	}

    }

    return -1;
}


//
// -- close Capture Device
//

void CCAPTURE::captureclose ()
{
  if (fd >=0 ) {
	close (fd);
	fd = -1;
	cx = cy = cw = ch = 0;
	out_w = out_h = 0;
	stride = 0;
   }
   return;
}




/*
  -- Coordination routines...
 */

void CCAPTURE::set_coord (int x, int y, int w, int h)
{
	if (( x != cx ) || ( y != cy )) {
		cx = x;
		cy = y;
		cw = w;
		ch = h;
		_set_window (cx,cy,cw,ch);
	}
}


void CCAPTURE::set_xy (int x, int y)
{
	if (( x != cx ) || ( y != cy )) {
		cx = x;
		cy = y;
		_set_window (cx,cy,cw,ch);
	}
}



void CCAPTURE::set_size (int w, int h)
{
	if (( w != cw ) || ( h != ch )) {
		cw = w;
		ch = h;
		_set_window (cx,cy,cw,ch);
	}
}




//
// -- set Capture Position
//

void CCAPTURE::_set_window (int x, int y, int w, int h)
{
#ifdef __v4l_capture

	struct v4l2_crop crop;

	crop.c.left = x;	
	crop.c.top = y;
	crop.c.width = w;
	crop.c.height = h;
	if( ioctl(fd, VIDIOC_S_CROP, &crop) < 0) 
		perror ("error VIDIOC_S_CROP");

#else

	capture_set_input_pos(fd, x, y);
	capture_set_input_size(fd, w, h);

#endif
}



void CCAPTURE::set_output_size (int w, int h)
{
#ifdef __v4l_capture

	struct v4l2_format format;

	out_w = w;
	out_h = h;
	format.fmt.pix.width = w;
	format.fmt.pix.height = h;

	if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) 
		perror ("error VIDIOC_S_FMT");

#else

	out_w = w;
	out_h = h;
	capture_set_output_size(fd, w, h);	

#endif
}


//
// -- capture a frame
// -- if buffer == NULL, routine will alloc memory!
// -- destroy will not free this allocated memory!
// -- otherwise buffer has to be large enough!
//

u_char *CCAPTURE::readframe (u_char *buf)
{
#ifdef __v4l_capture

	struct v4l2_format vid;
	int     n;
	u_char  *b;

	if (fd >= 0) {
		// get capture information
		vid.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(fd, VIDIOC_G_FMT, &vid) < 0) 
			perror ("Error getting info with VIDIOC_G_FMT");

		printf ("reported bytesperline: %d, height: %d, sizeimage: %d\n",
				vid.fmt.pix.bytesperline, 
				vid.fmt.pix.height,
				vid.fmt.pix.sizeimage);

		b = (buf != NULL)
			? buf
			: (u_char *) malloc (vid.fmt.pix.bytesperline * 
					     vid.fmt.pix.height);


		n = read (fd, b, vid.fmt.pix.bytesperline*vid.fmt.pix.height);
		if (n < 0)  {
			perror ("Error reading capture buffer");
			if (!buf) free (b);
			return (u_char *)NULL;
		}

		return b;
	}

	return (u_char *) NULL;


#else

	u_char  *b;
	int     stride;
	int	n;

	if (fd >= 0) {
  		stride = capture_start(fd);

		b = (buf != NULL)
			? buf
			: (u_char *) malloc (stride * out_h);

		n = read(fd, b, stride * out_h);
  		capture_stop(fd);
		if (n < 0)  {
			perror ("Error reading capture buffer");
			if (!buf) free (b);
			return (u_char *)NULL;
		}


		return b;
	}

	return (u_char *) NULL;

#endif
}










