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

class CComponents
{
	protected:
		int x, y, height, width, corner_type, shadow_w;
		int corner_rad, fr_thickness, fr_thickness_sel;
		CFrameBuffer * frameBuffer;
		std::vector<comp_fbdata_t> v_fbdata;
		fb_pixel_t	col_body, col_shadow, col_frame, col_frame_sel;
		bool	firstPaint, shadow, is_painted, paint_bg;
		
		void initVarBasic();
		void paintFbItems(bool do_save_bg = true);
		virtual fb_pixel_t* getScreen(int ax, int ay, int dx, int dy);
		comp_screen_data_t saved_screen;

		void clearSavedScreen();
		virtual void clear();
	public:
		CComponents();
		virtual~CComponents();

		inline virtual void setXPos(const int& xpos){x = xpos;};
		inline virtual void setYPos(const int& ypos){y = ypos;};
		inline virtual void setPos(const int& xpos, const int& ypos){x = xpos; y = ypos;};
		inline virtual void setHeight(const int& h){height = h;};
		inline virtual void setWidth(const int& w){width = w;};
		inline virtual void setDimensionsAll(const int& xpos, const int& ypos, const int& w, const int& h){x = xpos; y = ypos; width = w; height = h;};
		
		inline virtual int getXPos(){return x;};
		inline virtual int getYPos(){return y;};
		inline virtual int getHeight(){return height;};
		inline virtual int getWidth(){return width;};
		inline virtual void getSize(int* w, int* h){*w=width; *h=height;};
		inline virtual void getDimensions(int* xpos, int* ypos, int* w, int* h){*xpos=x; *ypos=y; *w=width; *h=height;};

		///set colors: Possible color values are defined in "gui/color.h" and "gui/customcolor.h"
		inline virtual void setColorFrame(fb_pixel_t color){col_frame = color;};
		inline virtual void setColorBody(fb_pixel_t color){col_body = color;};
		inline virtual void setColorShadow(fb_pixel_t color){col_shadow = color;};
		inline virtual void setColorAll(fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow){col_frame = color_frame; col_body = color_body; col_shadow = color_shadow;};
		///get colors
		inline virtual fb_pixel_t getColorFrame(){return col_frame;};
		inline virtual fb_pixel_t getColorBody(){return col_body;};
		inline virtual fb_pixel_t getColorShadow(){return col_shadow;};
		
		///set corner types: Possible corner types are defined in CFrameBuffer (see: driver/framebuffer.h).
		inline virtual void setCornerType(const int& type){corner_type = type;};
		inline virtual void setCornerRadius(const int& radius){corner_rad = radius;};
		///get corner types:
		inline virtual int getCornerType(){return corner_type;};
		inline virtual int getCornerRadius(){return corner_rad;};
		
		inline virtual void setFrameThickness(const int& thickness){fr_thickness = thickness;};
		inline virtual void setShadowOnOff(bool has_shadow){shadow = has_shadow;};

		///hide current screen and restore background
		virtual void hide();
		///erase current screen without restore of background, as similar to paintBackgroundBoxRel() from CFrameBuffer
		virtual void kill();
		///returns paint mode, true=item was painted
		virtual bool isPainted(){return is_painted;}
		///allows paint of elemetary item parts (shadow, frame and body), similar as background, set it usually to false, if item used in a form
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

class CComponentsPicture : public CComponentsItem
{
	protected:
		void initVarPicture();
		
		enum
		{
			CC_PIC_IMAGE_MODE_OFF 	= 0, //paint pictures in icon mode, mainly not scaled
			CC_PIC_IMAGE_MODE_ON	= 1, //paint pictures in image mode, paint scaled if required
			CC_PIC_IMAGE_MODE_AUTO	= 2
		};
		
		std::string pic_name;
		unsigned char pic_offset;
		bool pic_paint, pic_paintBg, pic_painted, do_paint;
		int pic_align, pic_x, pic_y, pic_width, pic_height;
		int pic_max_w, pic_max_h, pic_paint_mode;
		
		void init(	const int x_pos, const int y_pos, const std::string& image_name, const int alignment, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow);
		
	public:
		CComponentsPicture( 	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& image_name, const int alignment = CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_background = 0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		virtual inline void setPictureOffset(const unsigned char offset){pic_offset = offset;};
		virtual inline void setPicturePaint(bool paint_p){pic_paint = paint_p;};
		virtual inline void setPicturePaintBackground(bool paintBg){pic_paintBg = paintBg;};
		virtual void setPicture(const std::string& picture_name);
		virtual void setPictureAlign(const int alignment);
		
		virtual inline bool isPicPainted(){return pic_painted;};
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		virtual void hide(bool no_restore = false);
		virtual inline void getPictureSize(int *pwidth, int *pheight){*pwidth=pic_width; *pheight=pic_height;};
		virtual void setMaxWidth(const int w_max){pic_max_w = w_max;};
		virtual void setMaxHeight(const int h_max){pic_max_h = h_max;};
};

class CComponentsText : public CComponentsItem
{
	protected:
		CTextBox 	* ct_textbox;
		CBox 		* ct_box;
		Font		* ct_font;

		fb_pixel_t ct_col_text;
		int ct_text_mode; //see textbox.h for possible modes
		std::string ct_text, ct_old_text;
		bool ct_text_sent, ct_paint_textbg, ct_force_text_paint;

		static std::string iToString(int int_val); //helper to convert int to string

		void initVarText();
		void clearCCText();
		void initCCText();
		void paintText(bool do_save_bg = CC_SAVE_SCREEN_YES);
	public:
		CComponentsText();
		CComponentsText(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text = "", const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		virtual ~CComponentsText();

		//default members to paint a text box and hide painted text
		//hide textbox
		void hide(bool no_restore = false); 
		//paint text box, parameter do_save_bg: default = true, causes fill of backckrond pixel buffer
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES); 

		//send options for text font (size and type), color and mode (allignment)
		virtual inline void setTextFont(Font* font_text){ct_font = font_text;};
		virtual inline void setTextColor(fb_pixel_t color_text){ ct_col_text = color_text;};
		//see textbox.h for possible allignment modes
		virtual inline void setTextMode(const int mode){ct_text_mode = mode;};

		//send option to CTextBox object to paint background box behind text or not
		virtual inline void doPaintTextBoxBg(bool do_paintbox_bg){ ct_paint_textbg = do_paintbox_bg;};

		//sets text mainly with string also possible with overloades members for loacales, const char and text file
		virtual void setText(const std::string& stext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		
		virtual	void setText(const char* ctext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		virtual void setText(neutrino_locale_t locale_text, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		virtual void setText(const int digit, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		virtual bool setTextFromFile(const std::string& path_to_textfile, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);

		//helper to remove linebreak chars from a string if needed
		virtual void removeLineBreaks(std::string& str);
		
		//returns true, if text was changed
		virtual bool textChanged(){return ct_old_text != ct_text;};
		//force paint of text even if text was changed or not
		virtual void forceTextPaint(bool force_text_paint = true){ct_force_text_paint = force_text_paint;};

		//gets the embedded CTextBox object, so it's possible to get access directly to its methods and properties
		virtual CTextBox* getCTextBoxObject() { return ct_textbox; };
};

class CComponentsLabel : public CComponentsText
{
	public:
		CComponentsLabel(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text = "", const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENTINACTIVE, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0)
					:CComponentsText(x_pos, y_pos, w, h, text, mode, font_text, has_shadow, color_text, color_frame, color_body, color_shadow)
		{
			cc_item_type 	= CC_ITEMTYPE_LABEL;
		};
		CComponentsLabel():CComponentsText()
		{
			initVarText();
			cc_item_type 	= CC_ITEMTYPE_LABEL;
			ct_col_text = COL_MENUCONTENTINACTIVE;
		};
};

#define INFO_BOX_Y_OFFSET	2
class CComponentsInfoBox : public CComponentsText
{
	private:
		int x_text, x_offset;
		CComponentsPicture * pic;
		std::string pic_default_name;
		
		void paintPicture();
		void initVarInfobox();
		std::string pic_name;
		
	public:
		CComponentsText * cctext;

		CComponentsInfoBox();
		CComponentsInfoBox(	const int x_pos, const int y_pos, const int w, const int h,
					std::string info_text = "", const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		~CComponentsInfoBox();
		
		inline void setSpaceOffset(const int offset){x_offset = offset;};
		inline void setPicture(const std::string& picture_name){pic_name = picture_name;};
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};


class CComponentsShapeCircle : public CComponentsItem
{
	private:
		int d;
	public:
		CComponentsShapeCircle(	const int x_pos, const int y_pos, const int diam, bool has_shadow = CC_SHADOW_ON,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		inline void setDiam(const int& diam){d=width=height=diam, corner_rad=d/2;};
		inline int getDiam(){return d;};
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

class CComponentsShapeSquare : public CComponentsItem
{
	public:
		CComponentsShapeSquare(	const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = CC_SHADOW_ON,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

class CComponentsPIP : public CComponentsItem
{
	private:
		int screen_w, screen_h;
		std::string pic_name; //alternate picture if is no tv picture available
	public:
		CComponentsPIP(	const int x_pos, const int y_pos, const int percent = 30, bool has_shadow = CC_SHADOW_OFF);
		~CComponentsPIP();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		void setScreenWidth(int screen_width){screen_w = screen_width;};
		void setScreenHeight(int screen_heigth){screen_h = screen_heigth;};
		void setPicture(const std::string& image){pic_name = image;};
};


class CComponentsDetailLine : public CComponents
{
	private:
		int thickness, y_down, h_mark_top, h_mark_down;

		void initVarDline();

	public:
		CComponentsDetailLine();
		CComponentsDetailLine(	const int x_pos,const int y_pos_top, const int y_pos_down,
					const int h_mark_up = CC_HEIGHT_MIN , const int h_mark_down = CC_HEIGHT_MIN,
					fb_pixel_t color_line = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		~CComponentsDetailLine();

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		inline void setColors(fb_pixel_t color_line, fb_pixel_t color_shadow){col_body = color_line; col_shadow = color_shadow;};
		void syncSysColors();
		inline void setYPosDown(const int& y_pos_down){y_down = y_pos_down;};
		inline void setHMarkTop(const int& h_mark_top_){h_mark_top = h_mark_top_;};
		inline void setHMarkDown(const int& h_mark_down_){h_mark_down = h_mark_down_;};
};

#endif
