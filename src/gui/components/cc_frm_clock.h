/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CC_CLOCK__
#define __CC_CLOCK__


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc.h"
#include "cc_frm.h"


//! Sub class of CComponents. Show clock with digits on screen. 
/*!
Usable as simple fixed display or as ticking clock.
*/

class CComponentsFrmClock : public CComponentsForm
{
	private:
		
// 		bool cl_force_segment_paint;
	
	protected:
		///thread
		pthread_t  cl_thread;
		///refresh interval in seconds
		int cl_interval;
		///init function to start clock in own thread
		static void* initClockThread(void *arg);

		///raw time chars
		char cl_timestr[20];

		bool paintClock;
		bool activeClock;

		///font
		int cl_font_type;
		///fontrenderer object
		Font *cl_font;
		///text color
		int cl_col_text;
		///time format
		const char* cl_format_str;
		///time string align, default allign is ver and hor centered
		int cl_align;

		///initialize all attributes and required objects
		void initVarClock();
		
		///initialize clock contents  
		void initCCLockItems();
		///initialize timestring, called in initCCLockItems()
		void initTimeString();
		///initialize of general alignment of timestring segments within form area
		void initSegmentAlign(int* segment_width, int* segment_height);

	public:
		CComponentsFrmClock( 	const int x_pos, const int y_pos, const int w, const int h,
					const char* format_str = "%H:%M", bool activ=true, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_LIGHT_GRAY, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		~CComponentsFrmClock();		

		///set font type for segments
		void setClockFontType(const int& font_type){cl_font_type = font_type;};

		///set text color
		void setTextColor(fb_pixel_t color_text){ cl_col_text = color_text;};

		///set alignment of timestring, possible modes see align types in cc_types.h 
		void setClockAlignment(int align_type){cl_align = align_type;};

		///use string expession: "%H:%M" = 12:22, "%H:%M:%S" = 12:22:12
		void setClockFormat(const char* format_str){cl_format_str = format_str;};

		///start ticking clock thread, returns true on success, if false causes log output
		bool startThread();
		///stop ticking clock thread, returns true on success, if false causes log output
		bool stopThread();

		bool Start();
		bool Stop();

		///returns true, if clock is running in thread
		bool isClockRun() const {return cl_thread == 0 ? false:true;};
		///set refresh interval in seconds, default value=1 (=1 sec)
		void setClockIntervall(const int& seconds){cl_interval = seconds;};

		///show clock on screen
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);

		///reinitialize clock contents
		void refresh() { initCCLockItems(); }
};

#endif
