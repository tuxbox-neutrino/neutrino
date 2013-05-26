/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'

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

#ifndef __COMPONENTS__
#define __COMPONENTS__

#include "cc_types.h"
#include <gui/widget/textbox.h>
#include <vector>
#include <string>
#include <driver/pictureviewer/pictureviewer.h>

//#define DEBUG_CC

/// Basic component class.
/*!
Basic attributes and member functions for component sub classes
*/

class CComponents
{
	private:
		///pixel buffer handling, returns pixel buffer depends of given parameters
		fb_pixel_t* getScreen(int ax, int ay, int dx, int dy);
		
	protected:
		///object: framebuffer object, usable in all sub classes
		CFrameBuffer * frameBuffer;
		///container: for frambuffer properties and pixel buffer
		std::vector<comp_fbdata_t> v_fbdata;

		///property: x-position on screen
		int x;
		///property: y-position on screen
		int y;
		///property: height-dimension on screen
		int height;
		///property: width-dimension on screen
		int width;
		///property: has corners with definied type, types are defined in /driver/frambuffer.h, without effect, if corner_radius=0
		int corner_type;
		///property: defined radius of corner, without effect, if corner_type=0
		int corner_rad;
		
		///property: color of body
		fb_pixel_t col_body;
		///property: color of shadow
		fb_pixel_t col_shadow;
		///property: color of frame
		fb_pixel_t col_frame;
		///property: color of frame if component is selected, Note: fr_thickness_sel must be set
		fb_pixel_t col_frame_sel;

		///property: true=component has shadow
		bool shadow;
		///property: width of shadow
		int shadow_w;

		 ///property: frame thickness
		int fr_thickness;
		///property: frame thickness of selected component
		int fr_thickness_sel;

		///status: true=component was painted for 1st time
		bool firstPaint;
		///status: true=component was rendered
		bool is_painted;
		///mode: true=activate rendering of basic elements (frame, shadow and body)
		bool paint_bg;

		///initialize of basic attributes, no parameters required
		void initVarBasic();
		///rendering of framebuffer elements at once,
		///elements are contained in v_fbdata, presumes added frambuffer elements with paintInit(),
		///parameter do_save_bg=true, saves background of element to pixel buffer, this can be restore with hide()
		void paintFbItems(bool do_save_bg = true);

		///clean up old screen buffer saved in v_fbdata
		virtual void clear();

		///container: contains saved pixel buffer with position and dimensions
		comp_screen_data_t saved_screen; 	
		///cleans saved pixel buffer
		void clearSavedScreen();
		
	public:
		///basic component class constructor.
		CComponents();
		virtual~CComponents();

		///set screen x-position
		inline virtual void setXPos(const int& xpos){x = xpos;};
		///set screen y-position,
		inline virtual void setYPos(const int& ypos){y = ypos;};
		///set x and y position
		///Note: position of bound components (items) means position related within parent form, not for screen!
		///to set the real screen position, setRealPos(), to find in CComponentsItem sub classes
		inline virtual void setPos(const int& xpos, const int& ypos){x = xpos; y = ypos;};
		///set height of component on screen
		inline virtual void setHeight(const int& h){height = h;};
		///set width of component on screen
		inline virtual void setWidth(const int& w){width = w;};
		///set all positions and dimensions of component at once
		inline virtual void setDimensionsAll(const int& xpos, const int& ypos, const int& w, const int& h){x = xpos; y = ypos; width = w; height = h;};

		///return screen x-position of component
		///Note: position of bound components (items) means position related within parent form, not for screen!
		///to get the real screen position, use getRealXPos(), to find in CComponentsItem sub classes
		inline virtual int getXPos(){return x;};
		///return screen y-position of component
		///Note: position of bound components (items) means position related within parent form, not for screen!
		///to get the real screen position, use getRealYPos(), to find in CComponentsItem sub classes
		inline virtual int getYPos(){return y;};
		///return height of component
		inline virtual int getHeight(){return height;};
		///return width of component
		inline virtual int getWidth(){return width;};

		///return/set (pass through) width and height of component
		inline virtual void getSize(int* w, int* h){*w=width; *h=height;};
		///return/set (pass through) position and dimensions of component at once
		inline virtual void getDimensions(int* xpos, int* ypos, int* w, int* h){*xpos=x; *ypos=y; *w=width; *h=height;};

		///set frame color
		inline virtual void setColorFrame(fb_pixel_t color){col_frame = color;};
		///set body color
		inline virtual void setColorBody(fb_pixel_t color){col_body = color;};
		///set shadow color
		inline virtual void setColorShadow(fb_pixel_t color){col_shadow = color;};
		///set all basic framebuffer element colors at once
		///Note: Possible color values are defined in "gui/color.h" and "gui/customcolor.h"
		inline virtual void setColorAll(fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow){col_frame = color_frame; col_body = color_body; col_shadow = color_shadow;};

		///get frame color
		inline virtual fb_pixel_t getColorFrame(){return col_frame;};
		///get body color
		inline virtual fb_pixel_t getColorBody(){return col_body;};
		///get shadow color
		inline virtual fb_pixel_t getColorShadow(){return col_shadow;};
		
		///set corner types
		///Possible corner types are defined in CFrameBuffer (see: driver/framebuffer.h)
		///Note: default values are given from settings
		inline virtual void setCornerType(const int& type){corner_type = type;};
		///set corner radius
		inline virtual void setCornerRadius(const int& radius){corner_rad = radius;};
		///get corner types
		inline virtual int getCornerType(){return corner_type;};
		///get corner radius
		inline virtual int getCornerRadius(){return corner_rad;};

		///set frame thickness
		inline virtual void setFrameThickness(const int& thickness){fr_thickness = thickness;};
		///switch shadow on/off
		///Note: it's recommended to use #defines: CC_SHADOW_ON=true or CC_SHADOW_OFF=false as parameter, see also cc_types.h
		inline virtual void setShadowOnOff(bool has_shadow){shadow = has_shadow;};

		///hide current screen and restore background
		virtual void hide();
		///erase current screen without restore of background, it's similar to paintBackgroundBoxRel() from CFrameBuffer
		virtual void kill();
		///returns paint mode, true=item was painted
		virtual bool isPainted(){return is_painted;}
		///allows paint of elementary item parts (shadow, frame and body), similar as background, set it usually to false, if item used in a form
		virtual void doPaintBg(bool do_paint){paint_bg = do_paint;};

};

class CComponentsItem : public CComponents
{
	protected:
		int cc_item_type;
		int cc_item_index;
		bool cc_item_enabled, cc_item_selected;

		///Pointer to the form object in which this item is embedded.
		///Is typically the type CComponentsForm or derived classes, default intialized with NULL
		CComponents *cc_parent;

		///contains real position and dimensions on screen,
		int cc_item_xr, cc_item_yr;
		
		void hideCCItem(bool no_restore = false);
		void paintInit(bool do_save_bg);
		void initVarItem();

	public:
		CComponentsItem();

		///sets pointer to the form object in which this item is embedded.
		virtual void setParent(CComponents *parent){cc_parent = parent;};

		///sets real position on screen. Use this, if item contains own render methods and item is added to a form
		virtual void setRealPos(const int& xr, const int& yr){cc_item_xr = xr; cc_item_yr = yr;};
		virtual int getRealXPos(){return cc_item_xr;};
		virtual int getRealYPos(){return cc_item_yr;};
		
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES) = 0;
		virtual void hide(bool no_restore = false);
		virtual int getItemType();
		virtual void syncSysColors();
		
		///setters for item select stats
		virtual void setSelected(bool selected){cc_item_selected = selected;};
		virtual void setEnable(bool enabled){cc_item_enabled = enabled;};
		///getters for item enable stats
		virtual bool isSelected(){return cc_item_selected;};
		virtual bool isEnabled(){return cc_item_enabled;};
};

#endif
