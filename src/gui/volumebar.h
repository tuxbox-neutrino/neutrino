/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Volumebar class for gui.
	Copyright (C) 2013, Thilo Graf 'dbt'
	Copyright (C) 2013, M. Liebmann (micha-bbg)

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

#ifndef __VOLUMEBAR_H__
#define __VOLUMEBAR_H__

#include <global.h>
#include <system/settings.h>
#include <gui/components/cc.h>

class CVolumeBar : public CComponentsForm
{
	private:
		CProgressBar *vb_pb;
		CComponentsPicture *vb_icon;
		CComponentsLabel *vb_digit;
		int vb_digit_mode;
		int VolumeFont;
		int sy, sw, sh;
		int mute_ax, mute_ay, mute_dx, mute_dy, mute_ay_old;
		int h_spacer, v_spacer;
		int vb_item_offset;

		//clock
		int clock_y, clock_width, clock_height;

		//volume value
		char *vb_vol;

		//scale dimensions
		int vb_pbx, vb_pby, vb_pbw, vb_pbh;

		//icon dimensions
		int vb_icon_x, vb_icon_w/*, vb_icon_h*/;

		//digit dimensions
		int vb_digit_x, vb_digit_w/*, vb_digit_h*/;

		void initVarVolumeBar();
		void initVolumeBarPosition();
		void initVolumeBarSize();

		void initVolumeBarIcon();
		void initVolumeBarScale();
		void initVolumeBarDigitValue();
		void initVolumeBarDigit();
		void initVolumeBarItems();

		void paintVolScale();
		void paintVolumeBarDigit();

//		inline int cornerRad() { return (g_settings.rounded_corners) ? height/2 : 0; }
		inline int cornerRad() { return (g_settings.rounded_corners) ? CORNER_RADIUS_SMALL : 0; }

	public:

		enum
		{
			VOLUMEBAR_POS_TOP_RIGHT		= 0,
			VOLUMEBAR_POS_TOP_LEFT		= 1,
			VOLUMEBAR_POS_BOTTOM_LEFT	= 2,
			VOLUMEBAR_POS_BOTTOM_RIGHT	= 3,
			VOLUMEBAR_POS_TOP_CENTER	= 4,
			VOLUMEBAR_POS_BOTTOM_CENTER	= 5,
			VOLUMEBAR_POS_HIGHER_CENTER	= 6 
		};

		CVolumeBar(/*int current_volume*/);
// 		~CVolumeBar(); inherited from CComponentsForm

		void repaintVolScale();
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};


class CVolumeHelper
{
	private:
		int x, y, sw, sh;
		int mute_ax, mute_ay, mute_dx, mute_dy, mute_corrY;
		int clock_ax, clock_ay, clock_dx, clock_dy, digit_h, digit_offset;
		int time_ax, time_dx;
		int icon_width, icon_height, digit_width;
		int h_spacer, v_spacer;
		int vol_ay, vol_height;
		Font** clock_font;
		CFrameBuffer *frameBuffer;

		void Init(Font** font=NULL);
		void initVolBarSize();
		void initMuteIcon();
		void initInfoClock(Font** font);

	public:

		CVolumeHelper();
		static CVolumeHelper* getInstance();

		Font** vb_font;

		void getSpacer(int *h, int *v) { *h = h_spacer; *v = v_spacer; }
		int  getVolIconHeight() {return icon_height;}
		void getDimensions(int *_x, int *_y, int *_sw, int *_sh, int *_iw, int *_dw) { *_x = x; *_y = y; *_sw = sw; *_sh = sh; *_iw = icon_width; *_dw = digit_width; }
		void getMuteIconDimensions(int *_x, int *_y, int *w, int *h) { *_x = mute_ax; *_y = mute_ay+mute_corrY; *w = mute_dx; *h = mute_dy; }
		int  getInfoClockX();
		void getInfoClockDimensions(int *_x, int *_y, int *w, int *h) { *_x = getInfoClockX(); *_y = clock_ay; *w = clock_dx; *h = clock_dy; }
		void getTimeDimensions(int *_x, int *_y, int *w, int *h) { *_x = time_ax; *_y = clock_ay; *w = time_dx; *h = clock_dy; }
		void getVolBarDimensions(int *_y, int *_dy) { *_y = vol_ay; *_dy = vol_height; }
		void setMuteIconCorrY(int corr) { mute_corrY = corr; }
		void refresh(Font** font=NULL);
};

#endif
