/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
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

#include "cc_types.h"
#include <driver/framebuffer.h>
#include <gui/widget/textbox.h>
#include <vector>
#include <string>
#include <driver/pictureviewer/pictureviewer.h>

//#define DEBUG_CC

class CComponents
{
	protected:
		int x, y, height, width, corner_type, shadow_w;
		int corner_rad, fr_thickness;
		CFrameBuffer * frameBuffer;
		std::vector<comp_fbdata_t> v_fbdata;
		fb_pixel_t	col_body, col_shadow, col_frame;
		bool	firstPaint, shadow, is_painted, paint_bg;
		
		void initVarBasic();
		void paintFbItems(bool do_save_bg = true);
		fb_pixel_t* getScreen(int ax, int ay, int dx, int dy);
		comp_screen_data_t saved_screen;

		void clearSavedScreen();
		virtual void clear();
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
///		get colors
		inline virtual fb_pixel_t getColorFrame(){return col_frame;};
		inline virtual fb_pixel_t getColorBody(){return col_body;};
		inline virtual fb_pixel_t getColorShadow(){return col_shadow;};
		
///		set corner types: Possible corner types are defined in CFrameBuffer (see: driver/framebuffer.h).
		inline virtual void setCornerType(const int& type){corner_type = type;};
		inline virtual void setCornerRadius(const int& radius){corner_rad = radius;};
///		get corner types:
		inline virtual int getCornerType(){return corner_type;};
		inline virtual int getCornerRadius(){return corner_rad;};
		
		inline virtual void setFrameThickness(const int& thickness){fr_thickness = thickness;};
		inline virtual void setShadowOnOff(bool has_shadow){shadow = has_shadow;};
		
		virtual void hide();
		virtual bool isPainted(){return is_painted;}
		virtual void doPaintBg(bool do_paint){paint_bg = do_paint;};
};

class CComponentsItem : public CComponents
{
	protected:
		int cc_item_type;
		int cc_item_index;
		
		void hideCCItem(bool no_restore = false);
		void paintInit(bool do_save_bg);
		void initVarItem();

	public:
		enum
		{
			CC_ITEMTYPE_BASE,
			CC_ITEMTYPE_PICTURE,
			CC_ITEMTYPE_TEXT,
			CC_ITEMTYPE_TEXT_INFOBOX,
			CC_ITEMTYPE_SHAPE_SQUARE,
			CC_ITEMTYPE_SHAPE_CIRCLE,
			CC_ITEMTYPE_PIP,
			CC_ITEMTYPE_FRM,
			CC_ITEMTYPE_FRM_HEADER,
			CC_ITEMTYPE_FRM_ICONFORM,
			CC_ITEMTYPE_FRM_WINDOW,

			CC_ITEMTYPES
		};
		CComponentsItem();
		
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES) = 0;
		virtual void hide(bool no_restore = false);
		virtual void kill();
		virtual int getItemType();
		virtual void syncSysColors();
};

class CComponentsPicture : public CComponentsItem
{
	private:
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
		
		void initVarPicture();
		void init(	const int x_pos, const int y_pos, const std::string& image_name, const int alignment, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow);
		
	public:
		CComponentsPicture( 	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& image_name, const int alignment = CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_background = 0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		inline void setPictureOffset(const unsigned char offset){pic_offset = offset;};
		inline void setPicturePaint(bool paint_p){pic_paint = paint_p;};
		inline void setPicturePaintBackground(bool paintBg){pic_paintBg = paintBg;};
		void setPicture(const std::string& picture_name);
		void setPictureAlign(const int alignment);
		
		inline bool isPicPainted(){return pic_painted;};
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		inline void getPictureSize(int *pwidth, int *pheight){*pwidth=pic_width; *pheight=pic_height;};
		void setMaxWidth(const int w_max){pic_max_w = w_max;};
		void setMaxHeight(const int h_max){pic_max_h = h_max;};
};

class CComponentsText : public CComponentsItem
{
	protected:
		CTextBox 	* ct_textbox;
		CBox 		* ct_box;
		Font		* ct_font;

		fb_pixel_t ct_col_text;
		int ct_text_mode; //see textbox.h for possible modes
		const char* ct_text;
		bool ct_text_sent;

		void initVarText();
		void clearCCText();
		void initCCText();
		void paintText(bool do_save_bg = CC_SAVE_SCREEN_YES);
	public:
		CComponentsText();
		CComponentsText(	const int x_pos, const int y_pos, const int w, const int h,
					const char* text = "", const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_text = COL_MENUCONTENT, fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		~CComponentsText();

		void hide(bool no_restore = false);
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		
		virtual inline void setTextFont(Font* font_text){ct_font = font_text;};
		virtual inline void setTextColor(fb_pixel_t color_text){ ct_col_text = color_text;};
		virtual inline void setTextMode(const int mode){ct_text_mode = mode;};//see textbox.h for possible modes
		virtual	inline void setText(const char* ctext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL){ct_text = ctext; ct_text_mode = mode, ct_font = font_text;};
		virtual inline void setText(const std::string& stext, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL){ct_text = stext.c_str(); ct_text_mode = mode, ct_font = font_text;};
		virtual void setText(neutrino_locale_t locale_text, const int mode = ~CTextBox::AUTO_WIDTH, Font* font_text = NULL);
		virtual void removeLineBreaks(std::string& str);
};

#define INFO_BOX_Y_OFFSET	2
class CComponentsInfoBox : public CComponentsText
{
	private:
		int x_text, x_offset;
		CComponentsText * cctext;
		CComponentsPicture * pic;
		std::string pic_default_name;
		
		void paintPicture();
		void initVarInfobox();
		std::string pic_name;
		
	public:
		CComponentsInfoBox();
		CComponentsInfoBox(	const int x_pos, const int y_pos, const int w, const int h,
					const char* info_text = "", const int mode = CTextBox::AUTO_WIDTH, Font* font_text = NULL,
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
	public:
		CComponentsPIP(	const int x_pos, const int y_pos, const int percent = 30, bool has_shadow = CC_SHADOW_OFF);
		~CComponentsPIP();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		void setScreenWidth(int screen_width){screen_w = screen_width;};
		void setScreenHeight(int screen_heigth){screen_h = screen_heigth;};
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
class CComponentsItemBox : public CComponentsItem
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
		void paintText(size_t index, bool newElement);

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


class CComponentsForm : public CComponentsItem
{
	protected:
		std::vector<CComponentsItem*>	v_cc_items;			
		void initVarForm();
		void clearCCItems();
	public:
		
		CComponentsForm();
		CComponentsForm(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		virtual ~CComponentsForm();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void hide(bool no_restore = false);
		virtual void addCCItem(CComponentsItem* cc_Item);
		virtual void insertCCItem(const uint& cc_item_id, CComponentsItem* cc_Item);
		virtual void removeCCItem(const uint& cc_item_id);
		virtual void replaceCCItem(const uint& cc_item_id, CComponentsItem* new_cc_Item);
		virtual void replaceCCItem(CComponentsItem* old_cc_Item, CComponentsItem* new_cc_Item);
		virtual int getCCItemId(CComponentsItem* cc_Item);
		virtual CComponentsItem* getCCItem(const uint& cc_item_id);
		virtual void paintCCItems();
};

class CComponentsIconForm : public CComponentsForm
{
	private:
		std::vector<std::string> v_icons;
		int ccif_offset, ccif_icon_align;
		void initMaxHeight(int *pheight);

	protected:
 		void initVarIconForm();

	public:
		CComponentsIconForm();
		CComponentsIconForm(const int x_pos, const int y_pos, const int w, const int h, const std::vector<std::string> v_icon_names, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void initCCIcons();
		void addIcon(const std::string& icon_name);
		void addIcon(std::vector<std::string> icon_name);
		void removeIcons(){v_icons.clear();};
		void insertIcon(const uint& icon_id, const std::string& icon_name);
		void removeIcon(const uint& icon_id);
		void removeIcon(const std::string& icon_name);
		void removeAllIcons();
		void setIconOffset(const int offset){ccif_offset = offset;};

		enum //alignements
		{
			CC_ICONS_FRM_ALIGN_RIGHT ,
			CC_ICONS_FRM_ALIGN_LEFT
		};
		void setIconAlign(int alignment){ccif_icon_align = alignment;};
		
		int getIconId(const std::string& icon_name);		
};



class CComponentsHeader : public CComponentsForm
{
	private:
		CComponentsPicture * cch_icon_obj;
		CComponentsText * cch_text_obj;
		CComponentsIconForm * cch_btn_obj;
		std::string cch_text;
		const char*  cch_icon_name;
		neutrino_locale_t cch_locale_text;
		fb_pixel_t cch_col_text;
		Font* cch_font;
		int cch_icon_x, cch_items_y, cch_text_x, ccif_width, cch_icon_w, cch_buttons, cch_btn_offset;
		std::vector<std::string> v_cch_btn;
		
		void initCCHeaderIcon();
		void initCCHeaderText();
		void initCCHeaderButtons();
		void initCCHItems();
		void initCCHDefaultButtons();
		void initCCButtonFormSize();
		
	protected:
		void initVarHeader();
		
	public:
		enum
		{
			CC_BTN_HELP = 0x02,
			CC_BTN_INFO = 0x04,
			CC_BTN_MENU = 0x40,
			CC_BTN_EXIT = 0x80
			
		};
		
		enum
		{
			CC_HEADER_ITEM_ICON 	= 0,
			CC_HEADER_ITEM_TEXT 	= 1,
			CC_HEADER_ITEM_BUTTONS	= 2
		};
		CComponentsHeader();
		CComponentsHeader(const int x_pos, const int y_pos, const int w, const int h = 0, const std::string& caption = "header", const char* icon_name = NULL, const int buttons = 0, bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		CComponentsHeader(const int x_pos, const int y_pos, const int w, const int h = 0, neutrino_locale_t caption_locale = NONEXISTANT_LOCALE, const char* icon_name = NULL, const int buttons = 0,bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0, fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

// 		~CComponentsHeader();

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void setHeaderText(const std::string& caption);
		void setHeaderText(neutrino_locale_t caption_locale);
		void setColorHeaderBody(fb_pixel_t text_color){cch_col_text = text_color;};
		void setHeaderButtonOffset(const int offset){cch_btn_offset = offset;};
		void setHeaderIcon(const char* icon_name);
		void addHeaderButton(const std::string& button_name);
		void removeHeaderButtons();
		void setHeaderButtons(const int buttons){cch_buttons = buttons;};
};

class CComponentsWindow : public CComponentsForm
{
	private:
		CComponentsHeader * ccw_head;
		std::string ccw_caption;
		const char* ccw_icon_name;

		void initHeader();
		void initCCWItems();
		
	protected:
		void initVarWindow();
		
	public:
		enum
		{
			CC_WINDOW_ITEM_HEADER 	= 0
		};
		CComponentsWindow();
		~CComponentsWindow();
		
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void refresh(){initCCWItems();};
		void setWindowCaption(const std::string& text){ccw_caption = text;};
		void setWindowIcon(const char* iconname){ccw_icon_name = iconname;};
};

#endif
