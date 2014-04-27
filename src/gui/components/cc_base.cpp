/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_base.h"
#include <system/debug.h>
using namespace std;

//abstract basic class CComponents
CComponents::CComponents()
{
	x = saved_screen.x 	= 0;
	y = saved_screen.y 	= 0;
	cc_xr 			= x;
	cc_yr 			= y;
	height 			= saved_screen.dy = CC_HEIGHT_MIN;
	width 			= saved_screen.dx = CC_WIDTH_MIN;

	col_body 		= COL_MENUCONTENT_PLUS_0;
	col_shadow 		= COL_MENUCONTENTDARK_PLUS_0;
	col_frame 		= COL_MENUCONTENT_PLUS_6;
	col_frame_sel 		= COL_MENUCONTENTSELECTED_PLUS_0;
	corner_type 		= CORNER_ALL;
	corner_rad		= 0;
	shadow			= CC_SHADOW_OFF;
	shadow_w		= SHADOW_OFFSET;
	fr_thickness		= 0;
	fr_thickness_sel	= 3;

	firstPaint		= true;
	is_painted		= false;
	paint_bg		= true;
	cc_allow_paint		= true;
	frameBuffer 		= CFrameBuffer::getInstance();
	v_fbdata.clear();
	saved_screen.pixbuf 	= NULL;
}

CComponents::~CComponents()
{
	hide();
	clearSavedScreen();
	clearFbData();
}

void CComponents::clearSavedScreen()
{
	if (saved_screen.pixbuf)
		delete[] saved_screen.pixbuf;
	saved_screen.pixbuf = NULL;
}

bool CComponents::CheckFbData(const comp_fbdata_t& fbdata, const char* func, const int line)
{
	int32_t rows = fbdata.dx / (int32_t)frameBuffer->getScreenWidth(true) - 1 + fbdata.y;
	int32_t rest = fbdata.dx % (int32_t)frameBuffer->getScreenWidth(true);
        int32_t end  = rows * (int32_t)frameBuffer->getScreenWidth(true) + rest;
	if (	(fbdata.x < 0 || fbdata.y < 0) ||
		(end >= (int32_t)frameBuffer->getScreenWidth(true)*(int32_t)frameBuffer->getScreenHeight(true)) 
	   ) {
			dprintf(DEBUG_NORMAL, "[CComponents] ERROR! Position < 0 or > FB end [%s - %d]\n\tx = %d  y = %d\n\tdx = %d  dy = %d\n",
				func, line,
				fbdata.x, fbdata.y,
				fbdata.dx, fbdata.dy);
			return false;
		}
		if (fbdata.dx == 0 || fbdata.dy == 0) {
			dprintf(DEBUG_DEBUG,"[CComponents] INFO! dx and/or dy = 0 [%s - %d]\n\tx = %d  y = %d\n\tdx = %d  dy = %d\n",
				func, line,
				fbdata.x, fbdata.y,
				fbdata.dx, fbdata.dy);
			return false;
		}
	return true;
}

//paint framebuffer stuff and fill buffer
void CComponents::paintFbItems(bool do_save_bg)
{
	//save background before first paint, do_save_bg must be true
	if (firstPaint && do_save_bg){
		for(size_t i=0; i<v_fbdata.size(); i++){
			if (!CheckFbData(v_fbdata[i], __func__, __LINE__)){
				break;
			}

	dprintf(DEBUG_DEBUG, "[CComponents]\n\t[%s - %d] firstPaint->save screen: %d, fbdata_type: %d\n\tx = %d\n\ty = %d\n\tdx = %d\n\tdy = %d\n",
			__func__,
			__LINE__,
			firstPaint,
			v_fbdata[i].fbdata_type,
			v_fbdata[i].x,
			v_fbdata[i].y,
			v_fbdata[i].dx,
			v_fbdata[i].dy);

		saved_screen.x = v_fbdata[i].x;
		saved_screen.y = v_fbdata[i].y;
		saved_screen.dx = v_fbdata[i].dx;
		saved_screen.dy = v_fbdata[i].dy;
		clearSavedScreen();
		saved_screen.pixbuf = getScreen(saved_screen.x, saved_screen.y, saved_screen.dx, saved_screen.dy);
		firstPaint = false;
		break;
		}
	}

	for(size_t i=0; i< v_fbdata.size(); i++){
		// Don't paint on dimension or position error dx or dy are 0
		if (!CheckFbData(v_fbdata[i], __func__, __LINE__)){
			continue;
		}
		int fbtype = v_fbdata[i].fbdata_type;

	dprintf(DEBUG_DEBUG, "[CComponents]\n\t[%s - %d], fbdata_[%d]\n\tx = %d\n\ty = %d\n\tdx = %d\n\tdy = %d\n",
			__func__,
			__LINE__,
			(int)i,
			v_fbdata[i].x,
			v_fbdata[i].y,
			v_fbdata[i].dx,
			v_fbdata[i].dy);

		//some elements can be assembled from lines and must be handled as one unit (see details line),
		//so all individual backgrounds of boxes must be saved and painted in "firstpaint mode"
		if (firstPaint){

			if (do_save_bg && fbtype == CC_FBDATA_TYPE_LINE)
				v_fbdata[i].pixbuf = getScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);

			//ensure painting of all line fb items with saved screens
			if (fbtype == CC_FBDATA_TYPE_LINE)
				firstPaint = true;
			else
				firstPaint = false;
		}
		
		//paint all fb relevant basic parts (frame and body) with all specified properties, paint_bg must be true
		if (fbtype != CC_FBDATA_TYPE_BGSCREEN && paint_bg){
			if (fbtype == CC_FBDATA_TYPE_FRAME) {
				if (v_fbdata[i].frame_thickness > 0 && cc_allow_paint)
					frameBuffer->paintBoxFrame(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].frame_thickness, v_fbdata[i].color, v_fbdata[i].r, corner_type);
			}
			else if (fbtype == CC_FBDATA_TYPE_BACKGROUND)
				frameBuffer->paintBackgroundBoxRel(x, y, v_fbdata[i].dx, v_fbdata[i].dy);
			else if (fbtype == CC_FBDATA_TYPE_SHADOW_BOX) {
				if (shadow) {
					int sw = shadow_w;
					int sw_cur = sw;
					int x_sh = v_fbdata[i].x + v_fbdata[i].dx - sw;
					int y_sh = v_fbdata[i].y + v_fbdata[i].dy - sw;
					if (corner_type && v_fbdata[i].r) {
						//calculate positon of shadow areas
						x_sh += sw - 2*v_fbdata[i].r;
						y_sh += sw - 2*v_fbdata[i].r;
						//calculate current shadow width depends of current corner_rad
						sw_cur = max(2*v_fbdata[i].r, sw);
					}
					if (cc_allow_paint){
						// shadow right
						frameBuffer->paintBoxRel(x_sh, v_fbdata[i].y, sw_cur, v_fbdata[i].dy-sw_cur, v_fbdata[i].color, v_fbdata[i].r, corner_type & CORNER_TOP_RIGHT);
						// shadow bottom
						frameBuffer->paintBoxRel(v_fbdata[i].x, y_sh, v_fbdata[i].dx, sw_cur, v_fbdata[i].color, v_fbdata[i].r, corner_type & CORNER_BOTTOM);
					}
				}
			}
			else
				if(cc_allow_paint)
					frameBuffer->paintBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].color, v_fbdata[i].r, corner_type);
		}
	}

	is_painted = true;
}

//screen area save
inline fb_pixel_t* CComponents::getScreen(int ax, int ay, int dx, int dy)
{
	if (dx * dy == 0)
		return NULL;

	fb_pixel_t* pixbuf = new fb_pixel_t[dx * dy];
	frameBuffer->waitForIdle("CComponents::getScreen()");
	frameBuffer->SaveScreen(ax, ay, dx, dy, pixbuf);
	return pixbuf;
}

//restore screen from buffer
inline void CComponents::hide()
{
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].pixbuf){
			frameBuffer->waitForIdle("CComponents::hide()");
			frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
		}
	}

	clearFbData();
	is_painted = false;
}

//erase rendered objects
void CComponents::kill()
{
	for(size_t i =0; i< v_fbdata.size() ;i++) 
		frameBuffer->paintBackgroundBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);	
	clearFbData();
	firstPaint = true;
	is_painted = false;
}

//clean old screen buffer
void CComponents::clearFbData()
{
	for(size_t i =0; i< v_fbdata.size() ;i++)
		if (v_fbdata[i].pixbuf)
			delete[] v_fbdata[i].pixbuf;
	v_fbdata.clear();
}

inline void CComponents::setXPos(const int& xpos)
{
	x = xpos;
}

inline void CComponents::setYPos(const int& ypos)
{
	y = ypos;
}
