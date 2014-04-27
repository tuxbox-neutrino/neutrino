/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	(C) 2008,2013,2014 by Thilo Graf
	(C) 2009,2010,2013 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
 */

///
/*!
	description of parameters:

	position of progressbar:
	x_pos >	start position on screen x
	y_pos >	start position on screen y
	pb_width >	with of progressbar
	pb_height >	height of progressbar

	definitions of values:
	value >	value, you will display
	max_value >	maximal value that you will display

	appearance:
	activebar_col >	color of inner bar that shows the current value
	passivebar_col >	color of passive bar
	frame_col >	general frame color of progressbar, set 0 for no frame
	shadowbar_col	color > shadow behind progressbar, set 0 for no shadow

	paintZero > optional, if set to true and value = 0, then paints a diagonal line instead of active bar as symbolic for a zero value
*/

#ifndef __CC_PROGRESSBAR_H__
#define __CC_PROGRESSBAR_H__

#include "config.h"
#include "cc_base.h"

#include <string>

class CProgressBar : public CComponentsItem
{
	protected:
		void initVarProgressbar();

	private:
		///colors of active and passive bar area, active means the displayed value, passive the neutral area
		fb_pixel_t pb_active_col, pb_passive_col;

		int pb_last_width;

		///width of progress
		int pb_active_width, pb_passive_width;

		///maximal width,heigth of progress
		int pb_max_width, pb_height;

		///start position of bars
		int pb_start_x_passive;

		///color values
		int pb_red, pb_green, pb_yellow;

		///start position of activ/passiv area
		int pb_x, pb_y;

		bool pb_blink, pb_invert, pb_bl_changed;

		///causes a diagonal line as a sign for 0 value instead of an empty bar
		bool pb_paint_zero;

		///to evaluate values, these will be convert to the graph
		int pb_value, pb_max_value;

		///paint simple version of progressbar with simple color modifications
		void paintSimple();
		///paint version of progressbar with color and advanced display modifications
		void paintAdvanced();
		///painting of activ/passive bars via shape object
		void paintShapes(int &shx, int &shy, int &shw, int &shh, fb_pixel_t &col);

		void initDimensions();

		///paints graph
		void paintProgress(bool do_save_bg = CC_SAVE_SCREEN_NO);

		static inline unsigned int make16color(__u32 rgb){return 0xFF000000 | rgb;};

	public:
		///parameters:
		///x_pos, y_pos, w, h: position and dimension in pixel
		///blinkenlights: true if you want code to follow progressbar_color. needed, no default.
		///w, h: width / height of bar. Can later be set with paintProgressbar.
		///r, g, b: percentage of the bar where red/green/yellow is used, only used if blinkenlights (colored) == true.
		///inv:  false => red on the left side, true: red on right side.
		///active_col, passive_col: sets colors for displayed values, activ_col means the the displayed progress
		///color_frame, color_body, color_shadow: colores of progressbar for frame, body and shadow, Note: color of frame is ineffective on fr_thickness = 0
		CProgressBar(	const int x_pos = 0, const int y_pos = 0,
				const int w = -1, const int h = -1,
				fb_pixel_t color_frame = 0, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0,
				const fb_pixel_t active_col = COL_INFOBAR_PLUS_7, const fb_pixel_t passive_col = COL_INFOBAR_PLUS_3,
				const bool blinkenlights = false,
				const int r = 40, const int g = 100, const int b =70,
				const bool inv = false,
				CComponentsForm *parent = NULL);


		///set up to display available values
		void setValue(const int val){ pb_value = val;};
		void setMaxValue(const int max_val){pb_max_value = max_val;};
		///set up booth values to display at once
		void setValues(const int val, const int max_val){pb_value = val; pb_max_value = max_val;};

		///causes painting a diagonal line if value = 0, Note: ineffective in colored mode
		void setZeroLine(bool paint_zero_line = true){pb_paint_zero = paint_zero_line;};

		///setters for status colors
		void setActiveColor(fb_pixel_t active_color) {pb_active_col = active_color;};
		void setPassiveColor(fb_pixel_t passive_color) {pb_passive_col = passive_color;};
		///set up booth status colors at once
		void setStatusColors(fb_pixel_t active_color, fb_pixel_t passive_color) {pb_passive_col = passive_color; pb_active_col = active_color;};

		///invert: false => red on the left side, true: red on right side.
		void setInvert(bool inverted = true){pb_invert = inverted;};

		///blinkenlights: true (default) if you want code to follow progressbar_color.
		void setBlink(bool blinkenlights = true){pb_blink = blinkenlights;};
		///r, g, b: percentage of the bar where red/green/yellow is used, only used if blinkenlights (colored) == true.
		void setRgb(const int r, const int g, const int b){pb_red =  r; pb_green = g; pb_yellow = b;};

		///x, y, width, height, value, max_value: set most wanted parameters at once
		void setProgress(const int x_pos, const int y_pos,
					const int w, const int h,
					const int val, const int max_val){x=x_pos; y=y_pos; width=w; height=h; pb_value=val; pb_max_value=max_val;};

		///force update on next paint
		void reset() { pb_last_width = -1; }
		void paint(bool do_save_bg = CC_SAVE_SCREEN_NO);

		enum pb_color_t {
			PB_MATRIX = 0,	/* 0 */
			PB_LINES_V,	/* 1 */
			PB_LINES_H,	/* 2 */
			PB_COLOR	/* 3 */
		};

};

#endif /* __CC_PROGRESSBAR_H__ */
