/*
	GUI window component classes - Neutrino-GUI

	Copyright(C) 2012, Thilo Graf (dbt)

	This class contains generic components for GUI-related parts.

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

//required typedefs
typedef struct comp_fbdata_t
{
	int x;
	int y;
	int dx;
	int dy;
	fb_pixel_t color;
	int r;
	void * data;
	fb_pixel_t* pixbuf;
	bool is_frame;
	int frame_thickness;
} comp_fbdata_struct_t;

class CComponents
{
	protected:
		int x, y, height, width, sw;
		CFrameBuffer * frameBuffer;
		std::vector<comp_fbdata_t> v_screen_val;

		void paintFbItems(struct comp_fbdata_t * fbdata, const int items_count, bool do_save_bg = true);
		fb_pixel_t* saveScreen(int ax, int ay, int dx, int dy);
		void clear();

	public:
		CComponents(const int x_pos = 0, const int y_pos = 0, const int h = 0, const int w = 0);
		virtual~CComponents();

		virtual void setXPos(const int& xpos){x = xpos;};
		virtual void setYPos(const int& ypos){y = ypos;};
		virtual void setHeight(const int& h){height = h;};
		virtual void setWidth(const int& w){width = w;};
		virtual void restore();
};

class CComponentsDetailLine : public CComponents
{
	private:
		int thickness, y_down, h_mark_top, h_mark_down;
		fb_pixel_t	col_line, col_shadow;

	public:
		CComponentsDetailLine(	const int x_pos,const int y_pos_top, const int y_pos_down,
					const int h_mark_up =16 , const int h_mark_down = 16,
					fb_pixel_t color_line = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		~CComponentsDetailLine();

		void paint(bool do_save_bg = true);
		void hide();
		void setColor(fb_pixel_t color_line, fb_pixel_t color_shadow){col_line = color_line; col_shadow = color_shadow;};
		void setYPosDown(const int& y_pos_down){y_down = y_pos_down;};
		void setHMarkDown(const int& h_mark_down_){h_mark_down = h_mark_down_;};
};

#define INFO_BOX_Y_OFFSET	2
class CComponentsInfoBox : public CComponents
{
	private:
		int rad,fr_thickness;
		bool shadow;
		fb_pixel_t col_frame, col_body, col_shadow;
		bool firstPaint;
		std::vector<comp_fbdata_t> v_infobox_val;

	public:
		CComponentsInfoBox(	const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = true, 
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENTDARK_PLUS_0,fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		void paint(bool do_save_bg = true, bool fullPaint = false);
		void hide();
		void restore(bool clear_);
		void setColor(fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow){col_frame = color_frame; col_body = color_body; col_shadow = color_shadow;};
};

#endif
