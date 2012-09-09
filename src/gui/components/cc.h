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
#include "textbox.h"
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
	CC_FBDATA_TYPE_LINE,
	CC_FBDATA_TYPE_BACKGROUND,

	CC_FBDATA_TYPES
}FBDATA_TYPES;

typedef struct comp_screen_data_t
{
	int x;
	int y;
	int dx;
	int dy;
	fb_pixel_t* pixbuf;
} comp_screen_data_struct_t;

//align types
enum
{
	CC_ALIGN_RIGHT 		= 0,
	CC_ALIGN_LEFT 		= 1,
	CC_ALIGN_TOP 		= 2,
	CC_ALIGN_BOTTOM 	= 4,
	CC_ALIGN_HOR_CENTER	= 8,
	CC_ALIGN_VER_CENTER	= 16
};

enum
{
	CC_PIC_ICON,
	CC_PIC_IMAGE
};

enum
{
	CC_ITEMBOX_ICON,
	CC_ITEMBOX_PICTURE,
	CC_ITEMBOX_TEXT,
	CC_ITEMBOX_CLOCK
};

typedef struct comp_element_data_t
{
	int		type;
	int		align;
	std::string	element;
	int		x;
	int		y;
	int		width;
	int		height;
	void*		handler1;
	void*		handler2;
}comp_element_data_struct_t;

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
		bool	firstPaint, shadow, is_painted;
		
		void initVarBasic();
		void paintFbItems(struct comp_fbdata_t * fbdata, const int items_count, bool do_save_bg = true);
		fb_pixel_t* getScreen(int ax, int ay, int dx, int dy);
		comp_screen_data_t saved_screen;
		
		void clearSavedScreen();
		void clear();
	public:
		CComponents();
		virtual~CComponents();
		
		inline virtual void setXPos(const int& xpos){x = xpos;};
		inline virtual void setYPos(const int& ypos){y = ypos;};
		inline virtual void setHeight(const int& h){height = h;};
		inline virtual void setWidth(const int& w){width = w;};
		inline virtual void setDimensionsAll(const int& xpos, const int& ypos, const int& w, const int& h){x = xpos; y = ypos; width = w; height = h;};
		
		inline virtual int getXPos(){return x;};
		inline virtual int getYPos(){return y;};
		inline virtual int getHeight(){return height;};
		inline virtual int getWidth(){return width;};
		inline virtual void getDimensions(int* xpos, int* ypos, int* w, int* h){*xpos=x; *ypos=y; *w=width; *h=height;};
		
///		set colors: Possible color values are defined in "gui/color.h" and "gui/customcolor.h"
		inline virtual void setColorFrame(fb_pixel_t color){col_frame = color;};
		inline virtual void setColorBody(fb_pixel_t color){col_body = color;};
		inline virtual void setColorShadow(fb_pixel_t color){col_shadow = color;};
		inline virtual void setColorAll(fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow){col_frame = color_frame; col_body = color_body; col_shadow = color_shadow;};
		
		virtual void hide();
		virtual bool isPainted(){return is_painted;};
};

class CComponentsContainer : public CComponents
{
	protected:
		int corner_rad, fr_thickness;
		void hideContainer(bool no_restore = false);
		void paintInit(bool do_save_bg);
		void initVarContainer();
		
	public:
		CComponentsContainer();
				
///		set corner types: Possible corner types are defined in CFrameBuffer (see: driver/framebuffer.h).
		inline virtual void setCornerType(const int& type){corner_type = type;};
		inline virtual void setCornerRadius(const int& radius){corner_rad = radius;};
		
		inline virtual void setFrameThickness(const int& thickness){fr_thickness = thickness;};
		inline virtual void setShadowOnOff(bool has_shadow){shadow = has_shadow;};
		
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		virtual void hide(bool no_restore = false);
		virtual void kill();

		virtual void syncSysColors();
};

class CComponentsPicture : public CComponentsContainer
{
	private:
		std::string pic_name;
		unsigned char pic_offset;
		bool pic_paint, pic_paintBg, pic_painted, do_paint;
		int pic_align, pic_x, pic_y, pic_width, pic_height;
		int maxWidth, maxHeight, picMode;

		void initVarPicture();
		void init(	const int x_pos, const int y_pos, const std::string& picture_name, const int alignment, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow);

	public:
		CComponentsPicture( 	const int x_pos, const int y_pos,
					const std::string& picture_name, const int alignment = CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_background = 0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		CComponentsPicture( 	const int x_pos, const int y_pos, const int w_max, const int h_max,
					const std::string& picture_name, const int alignment = CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_background = 0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		inline void setPictureOffset(const unsigned char offset){pic_offset = offset;};
		inline void setPicturePaint(bool paint_p){pic_paint = paint_p;};
		inline void setPicturePaintBackground(bool paintBg){pic_paintBg = paintBg;};
		inline void setPicture(const std::string& picture_name);
		void setPictureAlign(const int alignment);
		
		inline bool isPicPainted(){return pic_painted;};
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		inline void getPictureSize(int *pwidth, int *pheight){*pwidth=pic_width; *pheight=pic_height;};

};

#define INFO_BOX_Y_OFFSET	2
class CComponentsInfoBox : public CComponentsContainer
{
	private:
		const char* text;
		int text_mode; //see textbox.h for possible modes
		int x_text, x_offset;
		Font* font;
		CBox * box;
		CTextBox * textbox;
		CComponentsPicture * pic;
		std::string pic_default_name;

		void paintPicture();
		void paintText();
		void initVarInfobox();
		std::string pic_name;
		fb_pixel_t ibox_col_text;
	public:
		CComponentsInfoBox();
		CComponentsInfoBox(	const int x_pos, const int y_pos, const int w, const int h,
					const char* info_text = "", const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		~CComponentsInfoBox();

		inline void setText(const char* info_text, const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL){text = info_text; text_mode = mode, font = font_text;};
		inline void setText(const std::string& info_text, const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL){text = info_text.c_str(); text_mode = mode, font = font_text;};
		void setText(neutrino_locale_t locale_text, const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		inline void setTextMode(const int mode){text_mode = mode;};//see textbox.h for possible modes
		inline void setTextFont(Font* font_text){font = font_text;};
		inline void setTextColor(fb_pixel_t color_text){ ibox_col_text = color_text;};
		inline void setSpaceOffset(const int offset){x_offset = offset;};
		inline void setPicture(const std::string& picture_name){pic_name = picture_name;};
		void removeLineBreaks(std::string& str);

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};


class CComponentsShapeCircle : public CComponentsContainer
{
	private:
		int d;
	public:
		CComponentsShapeCircle(	const int x_pos, const int y_pos, const int diam, bool has_shadow = CC_SHADOW_ON,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
					
		inline void setDiam(const int& diam){d=width=height=diam, corner_rad=d/2;};
		inline int getDiam(){return d;};
};

class CComponentsShapeSquare : public CComponentsContainer
{
	public:
		CComponentsShapeSquare(	const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = CC_SHADOW_ON,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
};

class CComponentsPIP : public CComponentsContainer
{
	private:
		int screen_w, screen_h;
	public:
		CComponentsPIP(	const int x_pos, const int y_pos, const int percent, bool has_shadow = CC_SHADOW_OFF);
		~CComponentsPIP();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
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
		void kill();
		inline void setColors(fb_pixel_t color_line, fb_pixel_t color_shadow){col_body = color_line; col_shadow = color_shadow;};
		void syncSysColors();
		inline void setYPosDown(const int& y_pos_down){y_down = y_pos_down;};
		inline void setHMarkTop(const int& h_mark_top_){h_mark_top = h_mark_top_;};
		inline void setHMarkDown(const int& h_mark_down_){h_mark_down = h_mark_down_;};
};

#define FIRST_ELEMENT_INIT 10000
#define LOGO_MAX_WIDTH width/4
class CComponentsItemBox : public CComponentsContainer
{
	protected:
		int hSpacer;
		int hOffset;
		int vOffset;
		int digit_offset, digit_h;
		bool paintElements;
		bool onlyOneTextElement;
		fb_pixel_t it_col_text;
		Font* font_text;
		int hMax;
		bool has_TextElement;
		size_t firstElementLeft;
		size_t firstElementRight;
		size_t prevElementLeft;
		size_t prevElementRight;
		std::vector<comp_element_data_t> v_element_data;
		bool isCalculated;

		void clearElements();
		void initVarItemBox();
		void calSizeOfElements();
		void calPositionOfElements();
		void paintItemBox(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void calculateElements();
		bool addElement(int align, int type, const std::string& element="", size_t *index=NULL);
		void paintImage(size_t index, bool newElement);

	public:
		CComponentsItemBox();
		virtual ~CComponentsItemBox();

		inline virtual void setTextFont(Font* font){font_text = font;};
		inline virtual void setTextColor(fb_pixel_t color_text){ it_col_text = color_text;};
		
		virtual void refreshElement(size_t index, const std::string& element);
		virtual void paintElement(size_t index, bool newElement= false);
		virtual bool addLogoOrText(int align, const std::string& logo, const std::string& text, size_t *index=NULL);
		virtual void clearTitlebar();
		virtual void addText(const std::string& s_text, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addText(neutrino_locale_t locale_text, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addIcon(const std::string& s_icon_name, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addPicture(const std::string& s_picture_path, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addClock(const int align=CC_ALIGN_RIGHT, size_t *index=NULL);
		virtual int  getHeight();
};

class CComponentsTitleBar : public CComponentsItemBox
{
	private:
		const char* tb_c_text;
		std::string tb_s_text, tb_icon_name;
		neutrino_locale_t tb_locale_text;
		int tb_text_align, tb_icon_align;
		
		void initText();
		void initIcon();
		void initElements();
		void initVarTitleBar();
	
	public:
		CComponentsTitleBar();
		CComponentsTitleBar(	const int x_pos, const int y_pos, const int w, const int h, const char* c_text = NULL, const std::string& s_icon ="",
					fb_pixel_t color_text = COL_MENUHEAD, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0);
		CComponentsTitleBar(	const int x_pos, const int y_pos, const int w, const int h, const std::string& s_text ="", const std::string& s_icon ="",
					fb_pixel_t color_text = COL_MENUHEAD, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0);
		CComponentsTitleBar(	const int x_pos, const int y_pos, const int w, const int h, neutrino_locale_t locale_text = NONEXISTANT_LOCALE, const std::string& s_icon ="",
					fb_pixel_t color_text = COL_MENUHEAD, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0);
					
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);

};


class CComponentsForm : public CComponentsContainer
{
	private:
		CComponentsTitleBar *tb;
		std::string tb_text, tb_icon;
		
		void initVarForm();
		void paintHead();
		
	public:
		CComponentsForm();
		~CComponentsForm();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		void setCaption(const std::string& text);
		void setCaption(neutrino_locale_t locale_text);
		void setIcon(const std::string& icon_name){tb_icon = icon_name;};
};

#endif
