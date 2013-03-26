/*
 * $Id$
 *
 * (C) 2008 by dbt <info@dbox2-tuning.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
#ifndef __gui_widget_progressbar_h__
#define __gui_widget_progressbar_h__
#include "config.h"
#include <gui/color.h>
#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <system/settings.h>

#include <string>

class CProgressBar
{
	private:
		CFrameBuffer * frameBuffer;
		int font_pbar;
		int frame_widht;
		int last_width;
		int red, green, yellow;
		bool blink, invert, bl_changed;
		int width, height;
		void realpaint(const int pos_x, const int pos_y,
			       const int value, const int max_value,
			       const fb_pixel_t activebar_col,
			       const fb_pixel_t passivebar_col,
			       const fb_pixel_t backgroundbar_col,
			       const fb_pixel_t shadowbar_col,
			       const char *upper_labeltext,
			       const uint8_t uppertext_col,
			       const char *iconfile,
			       bool paintZero);


	public:
		/* parameters:
		   blinkenligts: true if you want code to follow progressbar_color. needed, no default.
		   w, h: width / height of bar. Can later be set with paintProgressbar.
		         paintProgressBar2 can oly be used if w and h are set.
		   r, g, b: percentage of the bar where red/green/yellow is used.
		         only used if blinkenlights == true.
		   inv:  false => red on the left side, true: red on right side. */
		CProgressBar(const bool blinkenlights,
			     const int w = -1,
			     const int h = -1,
			     const int r = 40,
			     const int g = 100,
			     const int b =70,
			     const bool inv = false);
		~CProgressBar();

/// void paintProgressBar(...)	
/*!
	description of parameters:
	
	position of progressbar:
	pos_x >	start position on screen x
	pos_y >	start position on screen y
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
	
	upper_labeltext >	optional, label text, will be painted  upper/left the progressbar
	uppertext_col >	optional, but necessary with label text, color of label text
	iconfile > optional, name of iconfile
	paintZero > optional, if set to true and value = 0, then paints a diagonal line instead of active bar as symbolic for a zero value
*/
		void paintProgressBar (	const int pos_x,
					const int pos_y,
					const int pb_width,
					const int pb_height,
					const int value,
					const int max_value,
					const fb_pixel_t activebar_col = 0,
					const fb_pixel_t passivebar_col = 0,
					const fb_pixel_t frame_col = 0,
					const fb_pixel_t shadowbar_col = 0,
					const char * upper_labeltext = NULL,
					const uint8_t uppertext_col = 0,
					const char * iconfile = NULL,
					bool paintZero = false);

		void paintProgressBar2 (const int pos_x,
					const int pos_y,
					const int value,
					const int max_value = 100,
					const fb_pixel_t activebar_col = 0,
					const fb_pixel_t passivebar_col = 0,
					const fb_pixel_t frame_col = 0,
					const fb_pixel_t shadowbar_col = 0,
					const char * upper_labeltext = NULL,
					const uint8_t uppertext_col = 0,
					const char * iconfile = NULL,
					bool paintZero = false);

		void paintProgressBarDefault (	const int pos_x,
						const int pos_y,
						const int pb_width,
						const int pb_height,
						const int value,
						const int max_value);

		void reset() { last_width = -1; } /* force update on next paint */
		
		void hide();
};
					
#endif /* __gui_widget_progressbar_h__ */
