/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	(C) 2008, 2013 by Thilo Graf
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_item_progressbar.h"

#define ITEMW 4
#define POINT 2

#define RED    0xFF0000
#define GREEN  0x00FF00
#define YELLOW 0xFFFF00

CProgressBar::CProgressBar()
{
	initVarProgressbar();
}

CProgressBar::CProgressBar(	const int x_pos, const int y_pos, const int w, const int h,
				fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow,
				const fb_pixel_t active_col, const fb_pixel_t passive_col,
				const bool blinkenlights,
				const int r, const int g, const int b,
				const bool inv)
{
	initVarProgressbar();

	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	width		= w;
	height		= h;

	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

 	pb_blink 	= blinkenlights;
 	pb_invert 	= inv;
	pb_red 		= r;
	pb_green 	= g;
	pb_yellow 	= b;
	pb_active_col	= active_col;
	pb_passive_col 	= passive_col;
}


void CProgressBar::initVarProgressbar()
{
	//CComponentsItem
	initVarItem();
	cc_item_type 	= CC_ITEMTYPE_PROGRESSBAR;

	//CProgressBar
	pb_blink 		= false;
	pb_invert 		= false;
	pb_bl_changed 		= g_settings.progressbar_color;
	pb_last_width 		= -1;
	pb_red 			= 40;
	pb_green 		= 100;
	pb_yellow 		= 70;
	pb_active_col		= COL_INFOBAR_PLUS_7;
	pb_passive_col 		= COL_INFOBAR_PLUS_3;
	pb_value		= 0;
	pb_max_value		= 0;
	pb_paint_zero		= false;

	// init start positions x/y active bar
	pb_x 			= x + fr_thickness;
	pb_y 			= y + fr_thickness;
	pb_active_width 	= 0;
	pb_max_width		= width - 2*fr_thickness;
	pb_height		= 0;
	pb_start_x_passive 	= 0;
	pb_passive_width 	= width;
} 

//calculate bar dimensions
void CProgressBar::initDimensions()
{
	//prevent stupid callers, can give invalid values like "-1"...
	if (pb_value < 0)
		pb_value = 0;
	if (pb_value > pb_max_value)
		pb_max_value = pb_value;

	//assign start positions x/y active bar
	//NOTE: real values are only reqiured, if we paint active/passive bar with own render methodes or not embedded cc-items
	pb_x = (cc_parent ? cc_xr : x) + fr_thickness;
	pb_y = (cc_parent ? cc_yr : y) + fr_thickness;

	// width for active bar with current value
	pb_active_width = max(0, pb_last_width);
	if (pb_max_value)
		pb_active_width = (width - 2*fr_thickness) * pb_value / pb_max_value;

	// max width active/passive bar
	pb_max_width = width - 2*fr_thickness;

	// max height of active/passive bar
	pb_height = height - 2*fr_thickness;

	pb_start_x_passive = pb_x + pb_active_width;

	pb_passive_width = pb_max_width - pb_active_width;

	// background = frame
	if ( col_frame == 0 )
		col_frame = pb_active_col;
}


void CProgressBar::paintSimple()
{
	// progress value
	if (pb_active_width != pb_last_width){ //TODO: use shape cc-item
		frameBuffer->paintBoxRel(pb_x,  pb_y, pb_active_width, pb_height, pb_active_col); // active bar
 		frameBuffer->paintBoxRel(pb_start_x_passive, pb_y, pb_passive_width, pb_height, pb_passive_col); // passive bar
	}
	
	if (pb_paint_zero && pb_value == 0) //TODO: use shape cc-item
		frameBuffer->paintLine(pb_x , pb_y, pb_x+width-3, pb_y+height-3, pb_active_col); // zero line
}

void CProgressBar::paintAdvanced()
{
	int itemw = ITEMW, itemh = ITEMW, pointx = POINT, pointy = POINT;
	if(g_settings.progressbar_color){
		switch ((pb_color_t)g_settings.progressbar_design){
			default:
			case PB_MATRIX: // ::::: matrix
				break;
			case PB_LINES_V: // ||||| vert. lines
				itemh = pb_height;
				pointy = pb_height;
				break;
			case PB_LINES_H: // ===== horiz. lines
				itemw = POINT;
				break;
			case PB_COLOR: // filled color
				itemw = POINT;
				itemh = pb_height;
				pointy = pb_height;
				break;
		}
	}
	
	const int spc = itemh - pointy;			/* space between horizontal lines / points */
	int hcnt = (pb_height + spc) / itemh;		/* how many POINTs is the bar high */
	int yoff = (pb_height + spc - itemh * hcnt) / 2;
	
	//printf("height: %d itemh: %d hcnt: %d yoff: %d spc: %d\n", height, itemh, hcnt, yoff, spc);
	/* red, yellow, green are given in percent */
	int rd = pb_red * pb_max_width / (100 * itemw);		/* how many POINTs red */
	int yw = pb_yellow * pb_max_width / (100 * itemw);	/* how many POINTs yellow */
	int gn = pb_green * pb_max_width / (100 * itemw);	/* how many POINTs green */

	int maxi = pb_active_width / itemw;	/* how many POINTs is the active bar */
	int total = pb_max_width / itemw;	/* total number of POINTs */

	uint32_t rgb;
	fb_pixel_t color;

	if (pb_active_width != pb_last_width) {
		int i, j;
		const int py = pb_y + yoff;
		if (pb_active_width > pb_last_width) {
			int step, off;
			int b = 0;
			uint8_t diff = 0;
			for (i = 0; (i < rd) && (i < maxi); i++) {
				diff = i * 255 / rd;
				if (pb_invert)
					rgb = GREEN + (diff << 16); // adding red
				else
					rgb = RED + (diff << 8); // adding green
				color = make16color(rgb);
				for (j = 0; j < hcnt; j++) //TODO: use shape cc-item
					frameBuffer->paintBoxRel(pb_x + i * itemw, py + j * itemh, pointx, pointy, color);
			}
			step = yw - rd - 1;
			if (step < 1)
				step = 1;
			for (; (i < yw) && (i < maxi); i++) {
				diff = b++ * 255 / step / 2;
				if (pb_invert)
					rgb = YELLOW - (diff << 8); // removing green
				else
					rgb = YELLOW - (diff << 16); // removing red
				color = make16color(rgb);
				for (j = 0; j < hcnt; j++) //TODO: use shape cc-item
					frameBuffer->paintBoxRel(pb_x + i * itemw, py + j * itemh, pointx, pointy, color);
			}
			off = diff;
			b = 0;
			step = gn - yw - 1;
			if (step < 1)
				step = 1;
			for (; (i < gn) && (i < maxi); i++) {
				diff = b++ * 255 / step / 2 + off;
				if (pb_invert)
					rgb = YELLOW - (diff << 8); // removing green
				else
					rgb = YELLOW - (diff << 16); // removing red
				color = make16color(rgb);
				for (j = 0; j < hcnt; j++) //TODO: use shape cc-item
					frameBuffer->paintBoxRel(pb_x + i * itemw, py + j * itemh, pointx, pointy, color);
			}
		}
		for(i = maxi; i < total; i++) {
			for (j = 0; j < hcnt; j++) //TODO: use shape cc-item
				frameBuffer->paintBoxRel(pb_x + i * itemw, py + j * itemh, pointx, pointy, pb_passive_col); //fill passive
		}
	}
}


void CProgressBar::paintProgress(bool do_save_bg)
{
	if(pb_bl_changed != g_settings.progressbar_color) {
		pb_bl_changed = g_settings.progressbar_color;
		reset();
	}

	initDimensions();

	//body
	if (pb_last_width == -1 && col_body != 0) /* first paint */
		paintInit(do_save_bg); 

	//progress
	if (!pb_blink || !g_settings.progressbar_color)
		paintSimple();
	else
		paintAdvanced();

	if (is_painted)
		pb_last_width = pb_active_width;
}


void CProgressBar::paint(bool do_save_bg)
{
  	paintProgress(do_save_bg);
}
