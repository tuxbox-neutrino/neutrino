/*
	Neutrino-GUI  -   DBoxII-Project

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

#ifndef __controldtypes__
#define __controldtypes__

class CControld
{
public:

	//BoxType  /* cf. driver/include/tuxbox/tuxbox_info.h */
	typedef enum tuxbox_maker
	{
		TUXBOX_MAKER_UNKNOWN			= 0,
		TUXBOX_MAKER_NOKIA			= 1,
		TUXBOX_MAKER_PHILIPS			= 2,
		TUXBOX_MAKER_SAGEM			= 3,
		TUXBOX_MAKER_DREAM_MM			= 4,
		TUXBOX_MAKER_TECHNOTREND		= 5
	} tuxbox_maker_t;

	enum volume_type
        {  
		TYPE_OST=0,
		TYPE_AVS=1,
		TYPE_LIRC=2,
		TYPE_UNKNOWN
	};

        const static int no_video_formats = 5;

	typedef enum video_format 
	{ 
		FORMAT_CVBS	= 0,
		FORMAT_RGB	= 1,
		FORMAT_SVIDEO	= 2,
		FORMAT_YUV_VBS	= 3,
		FORMAT_YUV_CVBS	= 4
	} t_video_format;

};
#endif
