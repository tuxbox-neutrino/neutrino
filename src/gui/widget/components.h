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

class CComponents
{
	protected:
		int x, y, height, width;
		fb_pixel_t *bg_buf;
		CFrameBuffer * frameBuffer;


	public:
		CComponents(const int x_pos = 0, const int y_pos = 0, const int h = 0, const int w = 0);
		virtual~CComponents();

		virtual void setXPos(const int& xpos){x = xpos;};
		virtual void setYPos(const int& ypos){y = ypos;};
		virtual void setHeight(const int& h){height = h;};
		virtual void setWidth(const int& w){width = w;};
};

class CComponentsDetailLine : public CComponents
{
	private:
		int thickness, y_down, h_mark_top, h_mark_down, offs_up, offs_down;
		fb_pixel_t	col1, col2;

	public:
		CComponentsDetailLine(	const int x_pos,const int y_pos_top, const int y_pos_down,
					const int h_mark_up =16 , const int h_mark_down = 16,
					fb_pixel_t color1 = COL_MENUCONTENT_PLUS_6, fb_pixel_t color2 = COL_MENUCONTENTDARK_PLUS_0);

		void paint();
		void hide();
		void setColor(fb_pixel_t color1, fb_pixel_t color2){col1 = color1; col2 = color2;};
		void setYPosDown(const int& y_pos_down){y_down = y_pos_down;};
		void setHMarkDown(const int& h_mark_down_){h_mark_down = h_mark_down_;};
};

class CComponentsInfoBox : public CComponents
{
	private:
		int width, height, rad;
		fb_pixel_t col_frame, col_body, col_shadow;
		bool shadow, bg_saved;

	public:
		CComponentsInfoBox(	const int x_pos, const int y_pos, const int width_, const int height_, bool shadow_ = true,
					fb_pixel_t color1 = COL_MENUCONTENT_PLUS_6, 
					fb_pixel_t color2 = COL_MENUCONTENTDARK_PLUS_0, 
					fb_pixel_t color3 = COL_MENUCONTENTDARK_PLUS_0);

		void paint(int rad_);
		void hide(bool full = false);
		void setColor(fb_pixel_t color1, fb_pixel_t color2, fb_pixel_t color3){col_frame = color1; col_body = color2; col_shadow = color3;};
};

#endif
