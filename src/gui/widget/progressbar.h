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
	
	public:
		CProgressBar();
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
	
		void paintProgressBarDefault (	const int pos_x,
						const int pos_y,
						const int pb_width,
						const int pb_height,
						const int value,
						const int max_value);
						
};
					
#endif /* __gui_widget_progressbar_h__ */
