/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic for GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'

	License: GPL

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __COMPONENTS__
#define __COMPONENTS__

#include <driver/framebuffer.h>
#include <gui/color.h>
#include <gui/customcolor.h>
#include <vector>
#include <string>

//required typedefs
typedef struct comp_fbdata_t
{
	int fbdata_type;
	int x;
	int y;
	int dx;
	int dy;
	fb_pixel_t color;
	int r;
	int frame_thickness;
	fb_pixel_t* pixbuf;
	void * data;
} comp_fbdata_struct_t;

//fb data object types
typedef enum
{
	CC_FBDATA_TYPE_BGSCREEN,
	CC_FBDATA_TYPE_SHADOW,
	CC_FBDATA_TYPE_BOX,
	CC_FBDATA_TYPE_FRAME,
	CC_FBDATA_TYPE_LINE
}
FBDATA_TYPES;

#define CC_WIDTH_MIN		16
#define CC_HEIGHT_MIN		16
#define CC_SHADOW_ON 		true
#define CC_SHADOW_OFF 		false
#define CC_SAVE_SCREEN_YES 	true
#define CC_SAVE_SCREEN_NO 	false

class CComponents
{
	protected:
		int x, y, height, width, corner_type, shadow_w;
		CFrameBuffer * frameBuffer;
		std::vector<comp_fbdata_t> v_fbdata;
		fb_pixel_t	col_body, col_shadow, col_frame;
		bool	firstPaint, shadow;
		
		void paintFbItems(struct comp_fbdata_t * fbdata, const int items_count, bool do_save_bg = true);
		fb_pixel_t* getScreen(int ax, int ay, int dx, int dy);
		fb_pixel_t* saved_screen;
		void clear();
		
	public:
		CComponents();
		virtual~CComponents();
		
		virtual void setXPos(const int& xpos){x = xpos;};
		virtual void setYPos(const int& ypos){y = ypos;};
		virtual void setHeight(const int& h){height = h;};
		virtual void setWidth(const int& w){width = w;};
		
///		set colors: Possible color values are defined in "gui/color.h" and "gui/customcolor.h"
		virtual void setColorFrame(fb_pixel_t color){col_frame = color;};
		virtual void setColorBody(fb_pixel_t color){col_body = color;};
		virtual void setColorShadow(fb_pixel_t color){col_shadow = color;};
		virtual void setColorAll(fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow){col_frame = color_frame; col_body = color_body; col_shadow = color_shadow;};
		
		virtual void hide();
};

class CComponentsContainer : public CComponents
{
	protected:
		int corner_rad, fr_thickness;
		
	public:
		CComponentsContainer();
				
///		set corner types: Possible corner types are defined in CFrameBuffer (see: driver/framebuffer.h).
		virtual void setCornerType(const int& type){corner_type = type;};
		virtual void setCornerRadius(const int& radius){corner_rad = radius;};
		
		virtual void setFrameThickness(const int& thickness){fr_thickness = thickness;};
		virtual void setShadowOnOff(bool has_shadow){shadow = has_shadow;};
		
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		virtual void hide(bool no_restore = false);
		virtual void paintBackground();
};


#define INFO_BOX_Y_OFFSET	2
class CComponentsInfoBox : public CComponentsContainer
{
	public:
		CComponentsInfoBox(	const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = CC_SHADOW_ON,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENTDARK_PLUS_0,fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
};


class CComponentsDetailLine : public CComponents
{
	private:
		int thickness, y_down, h_mark_top, h_mark_down;
	
	public:
		CComponentsDetailLine(	const int x_pos,const int y_pos_top, const int y_pos_down,
					const int h_mark_up =16 , const int h_mark_down = 16,
					fb_pixel_t color_line = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		~CComponentsDetailLine();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
 		void paintBackground();
		void setColors(fb_pixel_t color_line, fb_pixel_t color_shadow){col_body = color_line; col_shadow = color_shadow;};
		void setYPosDown(const int& y_pos_down){y_down = y_pos_down;};
		void setHMarkDown(const int& h_mark_down_){h_mark_down = h_mark_down_;};
};

#endif
