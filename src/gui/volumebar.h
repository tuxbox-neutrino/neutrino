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

#include <gui/components/cc_frm.h>  //CComponentsForm
#include <gui/components/cc_item_progressbar.h> //CProgressBar

class CVolumeBar : public CComponentsForm
{
	private:
		CProgressBar *vb_pb;
		CComponentsPicture *vb_icon;
		CComponentsLabel *vb_digit;
		int vb_digit_mode;
		Font* vb_font;
		int VolumeFont;
		int sy, sw, sh;
		int mute_ax, mute_ay, mute_dx, mute_dy, mute_ay_old;
		int h_spacer, v_spacer;

		//clock
		int clock_y, clock_width, clock_height;

		//volume value
		char *vb_vol;

		//scale dimensions
		int vb_pbx, vb_pby, vb_pbw, vb_pbh;
		int icon_x, pb_x, digit_x;
		int icon_w, pb_w, digit_w;

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

	public:

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
		int h_spacer, v_spacer;
		int vol_ay, vol_height;
		CFrameBuffer *frameBuffer;

		void Init();
		void initVolBarHeight();
		void initMuteIcon();
		void initInfoClock();

	public:

		CVolumeHelper();
		static CVolumeHelper* getInstance();

		void getSpacer(int *h, int *v) { *h = h_spacer; *v = v_spacer; }
		void getDimensions(int *_x, int *_y, int *_sw, int *_sh) { *_x = x; *_y = y; *_sw = sw; *_sh = sh; }
		void getMuteIconDimensions(int *_x, int *_y, int *w, int *h) { *_x = mute_ax; *_y = mute_ay+mute_corrY; *w = mute_dx; *h = mute_dy; }
		void getInfoClockDimensions(int *_x, int *_y, int *w, int *h, int *d_h, int *d_o) { *_x = clock_ax; *_y = clock_ay; *w = clock_dx; *h = clock_dy, *d_h = digit_h, *d_o = digit_offset; }
		void getVolBarDimensions(int *_y, int *_dy) { *_y = vol_ay; *_dy = vol_height; }
		void setMuteIconCorrY(int corr) { mute_corrY = corr; }
		void refresh();
};

#endif
