/*
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "icons.h"
#include "progressbar.h"


CProgressBar::CProgressBar()
{
	frameBuffer = CFrameBuffer::getInstance();
	font_pbar = SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL;
	// frame width around active bar
	frame_widht = 2;
}

CProgressBar::~CProgressBar()
{
}

void CProgressBar::paintProgressBar (	const int pos_x,
					const int pos_y,
					const int pb_width,
					const int pb_height,
					const int value,
					const int max_value,
					const fb_pixel_t activebar_col,
					const fb_pixel_t passivebar_col,
					const fb_pixel_t backgroundbar_col,
					const fb_pixel_t shadowbar_col,
					const char *upper_labeltext,
					const uint8_t uppertext_col,
					const char *iconfile,
					bool paintZero)
{
	// set colors
	fb_pixel_t active_col = activebar_col != 0 ? activebar_col : COL_INFOBAR_PLUS_7;
	fb_pixel_t passive_col = passivebar_col != 0 ? passivebar_col : COL_INFOBAR_PLUS_3;

	int c_rad = RADIUS_SMALL;
	// get icon size
	int icon_w = 0, icon_h = 0;
	icon_w = iconfile != NULL ? frameBuffer->getIconWidth(iconfile) : 0;
	icon_h = iconfile != NULL ? frameBuffer->getIconHeight(iconfile) : 0;

	// start positions x/y active bar
	int start_x = icon_w != 0 ? pos_x  + icon_w + 2*frame_widht : pos_x + frame_widht;
	int start_y = pos_y + frame_widht;

	// width active bar for current value
	int active_pb_width;
	if (max_value)
		active_pb_width = (pb_width - icon_w - 2 * frame_widht) * value / max_value;
	else
		active_pb_width = 0;

	// max width active/passive bar
	int pb_max_width = icon_w != 0 ? (pb_width - 2*frame_widht) - icon_w - frame_widht : pb_width - 2*frame_widht;

	// max height progressbar bar, if icon height larger than pb_height then get height from icon
	int pb_max_height = icon_h > pb_height ? icon_h + 2* frame_widht : pb_height;

	// max height of active/passive bar
	int bar_height = pb_max_height - 2*frame_widht;

	int start_x_passive_bar = start_x + active_pb_width;
	int width_passive_bar =  pb_max_width - active_pb_width;

	// shadow
	if (shadowbar_col != 0)
		frameBuffer->paintBoxRel(pos_x+SHADOW_OFFSET,pos_y+SHADOW_OFFSET, pb_width, pb_max_height, shadowbar_col, c_rad); // shadow

	// background = frame
	if (backgroundbar_col != 0) {
		// we must paint background as frame, because of flicker effects at screen on fast changing values
		frameBuffer->paintBoxRel(pos_x,pos_y, 10, pb_max_height, backgroundbar_col, c_rad, CORNER_LEFT);
		frameBuffer->paintBoxRel(pos_x+pb_width-10,pos_y, 10, pb_max_height, backgroundbar_col, c_rad, CORNER_RIGHT);
		frameBuffer->paintBoxRel(pos_x+10,pos_y, pb_width-20, frame_widht, backgroundbar_col);
		frameBuffer->paintBoxRel(pos_x+10,pos_y+pb_max_height-frame_widht, pb_width-20, frame_widht, backgroundbar_col);
	}

	// paint icon if present
	if (iconfile != NULL){
		int icon_y = pos_y + pb_max_height / 2 - icon_h / 2;
		frameBuffer->paintIcon(iconfile, pos_x + frame_widht, icon_y);
	}

	// upper text
	int upper_labeltext_y = start_y - frame_widht;
	if (upper_labeltext != NULL) {
		g_Font[font_pbar]->RenderString(start_x +2,
						upper_labeltext_y,
						pb_width,
						upper_labeltext,
						uppertext_col != 0 ? uppertext_col : COL_INFOBAR,
						pb_height,
						true); // UTF8
	}

	frameBuffer->paintBoxRel(start_x,  start_y, active_pb_width, bar_height, active_col); // active bar
	frameBuffer->paintBoxRel(start_x_passive_bar, start_y, width_passive_bar, bar_height, passive_col); // passive bar
	if (paintZero && value == 0)
		frameBuffer->paintLine(pos_x+2 , pos_y+2, pos_x+pb_width-3, pos_y+pb_height-3, active_col); // zero line
}


void CProgressBar::paintProgressBarDefault(	const int pos_x,
						const int pos_y,
						const int pb_width,
						const int pb_height,
						const int value,
						const int max_value)
{
	paintProgressBar (pos_x, pos_y, pb_width, pb_height, value, max_value, 0, 0, COL_INFOBAR_SHADOW_PLUS_1, 0, "", 0);
}
