/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'

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
#include <gui/widget/icons.h>
#include <driver/fade.h>
/// Basic component class.
/*!
Basic attributes and member functions for component sub classes
*/

class CComponents : public COSDFader
{
	private:
		///pixel buffer handling, returns pixel buffer depends of given parameters
		fb_pixel_t* getScreen(int ax, int ay, int dx, int dy);

	protected:
		///object: framebuffer object, usable in all sub classes
		CFrameBuffer * frameBuffer;
		///container: for frambuffer properties and pixel buffer
		std::vector<comp_fbdata_t> v_fbdata;

		///property: x-position on screen, to alter with setPos() or setDimensionsAll(), see also defines CC_APPEND, CC_CENTERED
		int x;
		///property: y-position on screen, to alter setPos() or setDimensionsAll(), see also defines CC_APPEND, CC_CENTERED
		int y;
		///property: contains real x-position on screen
		int cc_xr;
		///property: contains real y-position on screen
		int cc_yr;
		///property: height-dimension on screen, to alter with setHeight() or setDimensionsAll()
		int height;
		///property: width-dimension on screen, to alter with setWidth() or setDimensionsAll()
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

		 ///property: frame thickness, see also setFrameThickness()
		int fr_thickness;
		///property: frame thickness of selected component, see also setFrameThickness()
		int fr_thickness_sel;

		///status: true=component was painted for 1st time
		bool firstPaint;
		///status: true=component was rendered
		bool is_painted;
		///mode: true=activate rendering of basic elements (frame, shadow and body)
		bool paint_bg;
		///mode:  true=allows painting of item, see also allowPaint()
		bool cc_allow_paint;

		///rendering of framebuffer elements at once,
		///elements are contained in v_fbdata, presumes added frambuffer elements with paintInit(),
		///parameter do_save_bg=true, saves background of element to pixel buffer, this can be restore with hide()
		void paintFbItems(bool do_save_bg = true);

		///check current fbdtata position and dimensions, parameter fbdata is an element of v_fbdata, returns false on error
		bool CheckFbData(const comp_fbdata_t& fbdata, const char* func, const int line);

		///clean up old screen buffer saved in v_fbdata
		virtual void clearFbData();

		///container: contains saved pixel buffer with position and dimensions
		comp_screen_data_t saved_screen; 	
		///cleans saved pixel buffer
		void clearSavedScreen();
		
	public:
		///basic component class constructor.
		CComponents();
		virtual~CComponents();

		///set screen x-position, parameter as int
		virtual void setXPos(const int& xpos);
		///set screen y-position, parameter as int
		virtual void setYPos(const int& ypos);
		///set x and y position at once
		///Note: position of bound components (items) means position related within parent form, not for screen!
		///to set the real screen position, look at setRealPos()
		inline virtual void setPos(const int& xpos, const int& ypos){x = xpos; y = ypos;};

		///sets real x position on screen. Use this, if item is added to a parent form
		virtual void setRealXPos(const int& xr){cc_xr = xr;};
		///sets real y position on screen. Use this, if item is added to a parent form
		virtual void setRealYPos(const int& yr){cc_yr = yr;};
		///sets real x and y position on screen at once. Use this, if item is added to a parent form
		virtual void setRealPos(const int& xr, const int& yr){cc_xr = xr; cc_yr = yr;};
		///get real x-position on screen. Use this, if item contains own render methods and item is bound to a form
		virtual int getRealXPos(){return cc_xr;};
		///get real y-position on screen. Use this, if item contains own render methods and item is bound to a form
		virtual int getRealYPos(){return cc_yr;};
		
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
		///return of frame thickness
		inline virtual int getFrameThickness(){return fr_thickness;};

		///return/set (pass through) width and height of component
		inline virtual void getSize(int* w, int* h){*w=width; *h=height;};
		///return/set (pass through) position and dimensions of component at once
		inline virtual void getDimensions(int* xpos, int* ypos, int* w, int* h){*xpos=x; *ypos=y; *w=width; *h=height;};

		///set frame color
		inline virtual void setColorFrame(fb_pixel_t color){col_frame = color;};
		///set selected frame color
		inline virtual void setColorFrameSel(fb_pixel_t color){col_frame_sel = color;};
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
		///set corner radius and type
		inline virtual void setCorner(const int& radius, const int& type = CORNER_ALL){corner_rad = radius; corner_type = type;};
		///get corner types
		inline virtual int getCornerType(){return corner_type;};
		///get corner radius
		inline virtual int getCornerRadius(){return corner_rad;};

		///set frame thickness
		virtual void setFrameThickness(const int& thickness, const int& thickness_sel = 3);
		///switch shadow on/off
		///Note: it's recommended to use #defines: CC_SHADOW_ON=true or CC_SHADOW_OFF=false as parameter, see also cc_types.h
		inline virtual void setShadowOnOff(bool has_shadow){shadow = has_shadow;};

		///hide current screen and restore background
		virtual void hide();

		///erase or paint over rendered objects without restore of background, it's similar to paintBackgroundBoxRel() known
		///from CFrameBuffer but with possiblity to define color, default color is COL_BACKGROUND (empty background)
		virtual void kill(const fb_pixel_t& bg_color = COL_BACKGROUND, const int& corner_radius = -1);

		///returns paint mode, true=item was painted
		virtual bool isPainted(){return is_painted;}
		///allows paint of elementary item parts (shadow, frame and body), similar as background, set it usually to false, if item used in a form
		virtual void doPaintBg(bool do_paint){paint_bg = do_paint;};

		///allow/disalows paint of item and its contents, but initialize of other properties are not touched
		///this can be understood as a counterpart to isPainted(), but before paint and value of is_painted is modified temporarily till next paint of item //TODO: is this sufficiently?
		virtual void allowPaint(bool allow){cc_allow_paint = allow; is_painted = cc_allow_paint ? false : true;};
		///returns visibility mode
		virtual bool paintAllowed(){return cc_allow_paint;};
};

class CComponentsItem : public CComponents
{
	protected:
		///property: define of item type, see cc_types.h for possible types
		int cc_item_type;
		///property: define of item index, all bound items get an index,
		///default: CC_NO_INDEX as identifer for not embedded item and default index=0 for form as main parent
		///see also getIndex(), setIndex()
		int cc_item_index;
		///property: default enabled
		bool cc_item_enabled;
		///property: default not selected
		bool cc_item_selected;
		///property: page number, this defines current item page location, means: this item is embedded in a parent container on page number n, see also setPageNumber()
		///default value is 0 for page one, any value > 0 causes handling for mutilple pages at parent container
		uint8_t cc_page_number;

		///Pointer to the form object in which this item is embedded.
		///Is typically the type CComponentsForm or derived classes, default intialized with NULL
		CComponentsForm *cc_parent;

		///hides item, arg: no_restore=true causes no restore of background, but clean up pixel buffer if required
		void hideCCItem(bool no_restore = false);
		
		///initialze of basic framebuffer elements with shadow, background and frame.
		///must be called first in all paint() members before paint any item,
		///If backround is not required, it's possible to override this with variable paint_bg=false, use doPaintBg(true/false) to set this!
		///arg do_save_bg=false avoids using of unnecessary pixel memory, eg. if no hide with restore is provided. This is mostly the case  whenever
		///an item will be hide or overpainted with other methods, or it's embedded  (bound)  in a parent form.
		void paintInit(bool do_save_bg);

		///add "this" current item to parent
		void initParent(CComponentsForm* parent);


	public:
		CComponentsItem(CComponentsForm *parent = NULL);

		///sets pointer to the form object in which this item is embedded.
		virtual void setParent(CComponentsForm *parent){cc_parent = parent;};
		///returns pointer to the form object in which this item is embedded.
		virtual CComponentsForm* getParent(){return cc_parent;};
		///property: returns true if item is added to a form
		virtual bool isAdded();

		///abstract: paint item, arg: do_save_bg see paintInit() above
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES) = 0;
		///hides item, arg: no_restore see hideCCItem() above
		virtual void hide(bool no_restore = false);

		///erase or paint over rendered objects without restore of background, it's similar to paintBackgroundBoxRel() known
		///from CFrameBuffer but with possiblity to define color, default color is 0 (empty background)
		///NOTE: Items with parent binding use the parent background color as default! Set parameter 'ignore_parent=true' to ignore parent background color!
		virtual void kill(const fb_pixel_t& bg_color = COL_BACKGROUND, bool ignore_parent = false);

		///get the current item type, see attribute cc_item_type above
		virtual int getItemType();
		///syncronizes item colors with current color settings if required, NOTE: overwrites internal values!
		virtual void syncSysColors();
		
		///set select mode, see also col_frame_sel
		virtual void setSelected(bool selected){cc_item_selected = selected;};
		///set enable mode, see also cc_item_enabled
		virtual void setEnable(bool enabled){cc_item_enabled = enabled;};
		
		///get select mode, see also setSelected() above
		virtual bool isSelected(){return cc_item_selected;};
		///get enable mode, see also setEnable() above
		virtual bool isEnabled(){return cc_item_enabled;};

		///get current index of item, see also attribut cc_item_index
		virtual int getIndex(){return cc_item_index;};
		///set an index to item, see also attribut cc_item_index.
		///To generate an index, use genIndex()
		virtual void setIndex(const int& index){cc_item_index = index;};

		///sets page location of current item, parameter as uint8_t, see: cc_page_number
		virtual void setPageNumber(const uint8_t& on_page_number){cc_page_number = on_page_number;};
		///returns current number of page location of current item, see: cc_page_number
		virtual u_int8_t getPageNumber(){return cc_page_number;};

		///set screen x-position, parameter as uint8_t, percent x value related to current width of parent form or screen
		virtual void setXPosP(const uint8_t& xpos_percent);
		///set screen y-position, parameter as uint8_t, percent y value related to current height of parent form or screen
		virtual void setYPosP(const uint8_t& ypos_percent);
		///set x and y position as percent value related to current parent form or screen dimensions at once
		virtual void setPosP(const uint8_t& xpos_percent, const uint8_t& ypos_percent);

		///do center item on screen or within a parent form, parameter along_mode assigns direction of centering
		virtual void setCenterPos(int along_mode = CC_ALONG_X | CC_ALONG_Y);

		///set item height, parameter as uint8_t, as percent value related to current height of parent form or screen
		virtual void setHeightP(const uint8_t& h_percent);
		///set item width, parameter as uint8_t, as percent value related to current width of parent form or screen
		virtual void setWidthP(const uint8_t& w_percent);
};

#endif
