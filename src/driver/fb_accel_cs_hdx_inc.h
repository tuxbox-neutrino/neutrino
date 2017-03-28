/*
	Framebuffer acceleration hardware abstraction functions.
	The common functions for coolstream hd1/hd2 graphic chips
	are represented in this class.

	(C) 2017 M. Liebmann
	(C) 2017 Thilo Graf 'dbt'
	Derived from old neutrino-hd framebuffer code

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
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FB_ACCEL_CS_HDX_INC_H
#define FB_ACCEL_CS_HDX_INC_H

#include <driver/fb_generic.h>
#include <driver/fb_accel.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <OpenThreads/Mutex>

#include <gui/color.h>
#include <gui/osd_helpers.h>
#include <system/debug.h>
#include <cs_api.h>
#include <video_cs.h>
#include <helpers_cs.h>
#include <cnxtfb.h>

extern cVideo * videoDecoder;

#endif // FB_ACCEL_CS_HDX_INC_H
