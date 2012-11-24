/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic for GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc.h"
#include <driver/pictureviewer/pictureviewer.h>

#include <video.h>

#define DEBUG_CC

extern cVideo * videoDecoder;
extern CPictureViewer * g_PicViewer;

using namespace std;

//abstract basic class CComponents
CComponents::CComponents()
{
	initVarBasic();
}

CComponents::~CComponents()
{
	hide();
	clearSavedScreen();
	clear();
}

void CComponents::clearSavedScreen()
{
	if (saved_screen.pixbuf)
		delete[] saved_screen.pixbuf;
	saved_screen.pixbuf = NULL;
}

void CComponents::initVarBasic()
{
	x = saved_screen.x 	= 0;
	y = saved_screen.y 	= 0;
	height 			= saved_screen.dy = CC_HEIGHT_MIN;
	width 			= saved_screen.dx = CC_WIDTH_MIN;

	col_body 		= COL_MENUCONTENT_PLUS_0;
	col_shadow 		= COL_MENUCONTENTDARK_PLUS_0;
	col_frame 		= COL_MENUCONTENT_PLUS_6;
	corner_type 		= CORNER_ALL;
	corner_rad		= 0;
	shadow			= CC_SHADOW_OFF;
	shadow_w		= SHADOW_OFFSET;
	fr_thickness		= 0;
	
	firstPaint		= true;
	is_painted		= false;
	paint_bg		= true;
	frameBuffer 		= CFrameBuffer::getInstance();
	v_fbdata.clear();
	saved_screen.pixbuf 	= NULL;
}

//paint framebuffer stuff and fill buffer
void CComponents::paintFbItems(bool do_save_bg)
{
	if (firstPaint && do_save_bg)	{
		for(size_t i=0; i<v_fbdata.size(); i++){
			if (v_fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN){
				//printf("\n#####[%s - %d] firstPaint: %d, fbdata_type: %d\n \n", __FUNCTION__, __LINE__, firstPaint, fbdata[i].fbdata_type);
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
	}

	for(size_t i=0; i< v_fbdata.size() ;i++){
		int fbtype = v_fbdata[i].fbdata_type;

		if (firstPaint){

			if (do_save_bg && fbtype == CC_FBDATA_TYPE_LINE)
				v_fbdata[i].pixbuf = getScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);

			//ensure painting of all line fb items with saved screens
			if (fbtype == CC_FBDATA_TYPE_LINE)
				firstPaint = true;
			else
				firstPaint = false;
		}
		if (fbtype != CC_FBDATA_TYPE_BGSCREEN){
			if (fbtype == CC_FBDATA_TYPE_FRAME && v_fbdata[i].frame_thickness > 0)
				frameBuffer->paintBoxFrame(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].frame_thickness, v_fbdata[i].color, v_fbdata[i].r);
			else if (fbtype == CC_FBDATA_TYPE_BACKGROUND)
				frameBuffer->paintBackgroundBoxRel(x, y, v_fbdata[i].dx, v_fbdata[i].dy);
			else
				frameBuffer->paintBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].color, v_fbdata[i].r, corner_type);
		}
	}

	is_painted = true;
}

//screen area save
inline fb_pixel_t* CComponents::getScreen(int ax, int ay, int dx, int dy)
{
	fb_pixel_t* pixbuf = new fb_pixel_t[dx * dy];
	frameBuffer->SaveScreen(ax, ay, dx, dy, pixbuf);
	return pixbuf;
}

//restore screen from buffer
inline void CComponents::hide()
{
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].pixbuf != NULL){
			frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
			delete[] v_fbdata[i].pixbuf;
			v_fbdata[i].pixbuf = NULL;
		}
	}
	v_fbdata.clear();
	is_painted = false;
}

//clean old screen buffer
inline void CComponents::clear()
{
	for(size_t i =0; i< v_fbdata.size() ;i++)
		if (v_fbdata[i].pixbuf != NULL)
			delete[] v_fbdata[i].pixbuf;
	v_fbdata.clear();
}


//-------------------------------------------------------------------------------------------------------
//abstract sub class CComponentsItem from CComponents
CComponentsItem::CComponentsItem()
{
	//CComponentsItem
	initVarItem();
}

// 	 y
// 	x+------f-r-a-m-e-------+
// 	 |			|
//     height	  body		|
// 	 |			|
// 	 +--------width---------+

void CComponentsItem::initVarItem()
{
	//CComponents
	initVarBasic();
}

// Paint container background in cc-items with shadow, background and frame.
// This member must be called first in all paint() members before paint other items into the container.
// If backround is not required, it's possible to override this with variable paint_bg=false, use doPaintBg(true/false) to set this!
void CComponentsItem::paintInit(bool do_save_bg)
{
	clear();

	if(!paint_bg)
		return;

	int sw = shadow ? shadow_w : 0;
	int th = fr_thickness;

	comp_fbdata_t fbdata[] =
	{
		{CC_FBDATA_TYPE_BGSCREEN, 	x,	y, 	width+sw, 	height+sw, 	0, 		0, 		0, 	NULL,	NULL},
		{CC_FBDATA_TYPE_SHADOW, 	x+sw,	y+sw, 	width, 		height, 	col_shadow, 	corner_rad, 	0, 	NULL,	NULL},
		{CC_FBDATA_TYPE_FRAME, 		x, 	y, 	width, 		height, 	col_frame, 	corner_rad, 	th, 	NULL,	NULL},
		{CC_FBDATA_TYPE_BOX, 		x+th,	y+th, 	width-2*th, 	height-2*th, 	col_body, 	corner_rad-th,  0, 	NULL,	NULL},
	};

	for(size_t i =0; i< (sizeof(fbdata) / sizeof(fbdata[0])) ;i++)
		v_fbdata.push_back(fbdata[i]);

	paintFbItems(do_save_bg);
}

//restore last saved screen behind form box,
//Do use parameter 'no restore' to override temporarly the restore funtionality.
//This could help to avoid ugly flicker efffects if it is necessary e.g. on often repaints, without changed contents.
void CComponentsItem::hideCCItem(bool no_restore)
{
	is_painted = false;

	if (saved_screen.pixbuf) {
		frameBuffer->RestoreScreen(saved_screen.x, saved_screen.y, saved_screen.dx, saved_screen.dy, saved_screen.pixbuf);
		if (no_restore) {
			delete[] saved_screen.pixbuf;
			saved_screen.pixbuf = NULL;
			firstPaint = true;
		}
	}
}

void CComponentsItem::hide(bool no_restore)
{
	hideCCItem(no_restore);
}


//hide rendered objects
void CComponentsItem::kill()
{
	//save current colors
	fb_pixel_t c_tmp1, c_tmp2, c_tmp3;
	c_tmp1 = col_body;
	c_tmp2 = col_shadow;
	c_tmp3 = col_frame;

	//set background color
	col_body = col_frame = col_shadow = COL_BACKGROUND;

	//paint with background and restore last used colors
	paint(CC_SAVE_SCREEN_NO);
	col_body = c_tmp1;
	col_shadow = c_tmp2;
	col_frame = c_tmp3;
	firstPaint = true;
	is_painted = false;
}

//synchronize colors for forms
//This is usefull if the system colors are changed during runtime
//so you can ensure correct applied system colors in relevant objects with unchanged instances.
void CComponentsItem::syncSysColors()
{
	col_body 	= COL_MENUCONTENT_PLUS_0;
	col_shadow 	= COL_MENUCONTENTDARK_PLUS_0;
	col_frame 	= COL_MENUCONTENT_PLUS_6;
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsText from CComponentsItem
CComponentsText::CComponentsText()
{
	//CComponentsText
	initVarText();
}

CComponentsText::CComponentsText(	const int x_pos, const int y_pos, const int w, const int h,
					const char* text, const int mode, Font* font_text,
					bool has_shadow,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsText
	initVarText();

	//CComponents
	x 		= x_pos,
	y 		= y_pos,
	width		= w;
	height		= h;
	
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	shadow		= has_shadow;
	
	ct_font 	= font_text;
	ct_text 	= text;
	ct_text_mode	= mode;
	ct_col_text	= color_text;
}



CComponentsText::~CComponentsText()
{
	hide();
	clearSavedScreen();
	clearCCText();
	clear();
}


void CComponentsText::initVarText()
{
	//CComponents, CComponentsItem
	initVarItem();

	//CComponentsText
	ct_font 	= NULL;
	ct_box		= NULL;
	ct_textbox	= NULL;
	ct_text 	= NULL;
	ct_text_mode	= CTextBox::AUTO_WIDTH;
	ct_col_text	= COL_MENUCONTENT;
	ct_text_sent	= false;
}


void CComponentsText::initCCText()
{
	//set default font, if is no font definied
	if (ct_font == NULL)
		ct_font = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];

	//define height from font size
	height 	= max(height, 	ct_font->getHeight());

	//text box dimensions
	if (ct_box){
		//ensure that we have a new instance
		delete ct_box;
		ct_box = NULL;
	}
	ct_box = new CBox();
	ct_box->iX 	= x+fr_thickness;
	ct_box->iY 	= y+fr_thickness;
	ct_box->iWidth 	= width-2*fr_thickness;
	ct_box->iHeight = height-2*fr_thickness;

	//init textbox
	if (ct_textbox == NULL)
		ct_textbox = new CTextBox();

	//set text box properties
	ct_textbox->setTextFont(ct_font);
	ct_textbox->setTextMode(ct_text_mode);
	ct_textbox->setWindowPos(ct_box);
	ct_textbox->setTextBorderWidth(0);
	ct_textbox->enableBackgroundPaint(false);
	ct_textbox->setBackGroundRadius(corner_rad-fr_thickness, corner_type);
	ct_textbox->setTextColor(ct_col_text);
	ct_textbox->setWindowMaxDimensions(ct_box->iWidth, ct_box->iHeight);
	ct_textbox->setWindowMinDimensions(ct_box->iWidth, ct_box->iHeight);

	//set text
	string new_text = static_cast <string> (ct_text);
	ct_text_sent = ct_textbox->setText(&new_text, ct_box->iWidth);
}

void CComponentsText::clearCCText()
{
	if (ct_box)
		delete ct_box;
	ct_box = NULL;

	if (ct_textbox)
		delete ct_textbox;
	ct_textbox = NULL;
}

void CComponentsText::setText(neutrino_locale_t locale_text, int mode, Font* font_text)
{
	ct_text = g_Locale->getText(locale_text);
	ct_text_mode = mode;
	ct_font = font_text;
}

void CComponentsText::paintText(bool do_save_bg)
{
	paintInit(do_save_bg);
	initCCText();
	if (ct_text_sent)
		ct_textbox->paint();
	ct_text_sent = false;
}

void CComponentsText::paint(bool do_save_bg)
{

	paintText(do_save_bg);
}

void CComponentsText::hide(bool no_restore)
{
	if (ct_textbox)
		ct_textbox->hide();
	hideCCItem(no_restore);
}

//small helper to remove excessiv linbreaks
void CComponentsText::removeLineBreaks(std::string& str)
{
	std::string::size_type spos = str.find_first_of("\r\n");
	while (spos != std::string::npos) {
		str.replace(spos, 1, " ");
		spos = str.find_first_of("\r\n");
	}
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsInfoBox from CComponentsItem
CComponentsInfoBox::CComponentsInfoBox()
{
	//CComponentsInfoBox
	initVarInfobox();
}

CComponentsInfoBox::CComponentsInfoBox(const int x_pos, const int y_pos, const int w, const int h,
				       const char* info_text, const int mode, Font* font_text,
				       bool has_shadow,
				       fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsInfoBox
	initVarInfobox();
	
	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	shadow		= has_shadow;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	ct_text 	= info_text;
	ct_text_mode	= mode;
	ct_font		= font_text;
	ct_col_text	= color_text;
}

CComponentsInfoBox::~CComponentsInfoBox()
{
	hide();
	clearSavedScreen();
	clearCCText();
	delete pic;
	delete cctext;
	clear();
}

void CComponentsInfoBox::initVarInfobox()
{
	//CComponents, CComponentsItem,  CComponentsText
	initVarText();

	//CComponentsInfoBox
	pic 		= NULL;
	cctext		= NULL;
	pic_name	= "";
	x_offset	= 10;
	x_text		= x+fr_thickness+x_offset;;

}

void CComponentsInfoBox::paintPicture()
{
	//init and set icon paint position
	if (pic == NULL)
		pic = new CComponentsPicture(x+fr_thickness+x_offset, y+fr_thickness/*+y_offset*/, 0, 0, "");
	pic->setXPos(x+fr_thickness+x_offset);
	pic->setYPos(y+fr_thickness);
	
	//define icon
	pic->setPicture(pic_name);
	
	//fit icon into infobox
	pic->setHeight(height-2*fr_thickness);
	pic->setColorBody(col_body);
	
	pic->paint(CC_SAVE_SCREEN_NO);	
}

void CComponentsInfoBox::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
	paintPicture();

	//define text x position
	x_text = x+fr_thickness+x_offset;
	
	//set text to the left border if picture is not painted
	if (pic->isPicPainted()){
		int pic_w = pic->getWidth();
		x_text += pic_w+x_offset;
	}

	//set text and paint text lines
 	if (ct_text){
		if (cctext == NULL)
			cctext = new CComponentsText();
		cctext->setText(ct_text, ct_text_mode, ct_font);
		cctext->setDimensionsAll(x_text, y+fr_thickness, width-(x_text-x+x_offset+fr_thickness), height-2*fr_thickness);
 		cctext->paint(CC_SAVE_SCREEN_NO);
	}
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsShapeSquare from CComponentsItem
CComponentsShapeSquare::CComponentsShapeSquare(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsItem
	initVarItem();

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
}

void CComponentsShapeSquare::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsShapeCircle from CComponentsItem
CComponentsShapeCircle::CComponentsShapeCircle(	int x_pos, int y_pos, int diam, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsItem
	initVarItem();

	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	//width = height	= d = diam;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	//CComponentsShapeCircle
	width = height	= d = diam;

	//CComponentsItem
	corner_rad	= d/2;
}

// 	 y
// 	x+	 -	 +
//
//
//
// 	 |----d-i-a-m----|
//
//
//
// 	 +	 -	 +

void CComponentsShapeCircle::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsDetailLine from CComponents
CComponentsDetailLine::CComponentsDetailLine()
{
	initVarDline();

	//CComponents
	x 		= 0;
	y 		= 0;
	col_shadow	= COL_MENUCONTENTDARK_PLUS_0;
	col_body	= COL_MENUCONTENT_PLUS_6;

	//CComponentsDetailLine
	y_down 		= 0;
	h_mark_top 	= CC_HEIGHT_MIN;
	h_mark_down 	= CC_HEIGHT_MIN;
}

CComponentsDetailLine::CComponentsDetailLine(const int x_pos, const int y_pos_top, const int y_pos_down, const int h_mark_top_, const int h_mark_down_, fb_pixel_t color_line, fb_pixel_t color_shadow)
{
	initVarDline();

	//CComponents
	x 		= x_pos;
	y 		= y_pos_top;
	col_shadow	= color_shadow;
	col_body	= color_line;

	//CComponentsDetailLine
	y_down 		= y_pos_down;
	h_mark_top 	= h_mark_top_;
	h_mark_down 	= h_mark_down_;
}

void CComponentsDetailLine::initVarDline()
{
	//CComponents
	initVarBasic();

	shadow_w	= 1;

	//CComponentsDetailLine
	thickness 	= 4;
}

CComponentsDetailLine::~CComponentsDetailLine()
{
	hide(); //restore background
	clear();
}

//		y_top (=y)
//	xpos	+--|h_mark_up
//		|
//		|
//		|
//		|
//		|
//		|
//		|
//		|
//		|
//		+--|h_mark_down
//		y_down


//paint details line with current parameters
void CComponentsDetailLine::paint(bool do_save_bg)
{
	clear();

	int y_mark_top = y-h_mark_top/2+thickness/2;
	int y_mark_down = y_down-h_mark_down/2+thickness/2;

	int sw = shadow_w;

	comp_fbdata_t fbdata[] =
	{
		/* vertical item mark | */
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw, 	y_mark_top, 		thickness, 		h_mark_top, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-sw,		y_mark_top, 		sw, 			h_mark_top, 		col_shadow, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw,	y_mark_top+h_mark_top, 	thickness+sw, 		sw	, 		col_shadow, 	0, 0, NULL, NULL},

		/* horizontal item line - */
		{CC_FBDATA_TYPE_LINE, x, 			y,			width-thickness-sw,	thickness, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+thickness,		y+thickness,		width-2*thickness-sw,	sw, 			col_shadow, 	0, 0, NULL, NULL},

		/* vertical connect line [ */
		{CC_FBDATA_TYPE_LINE, x,			y+thickness, 		thickness, 		y_down-y-thickness, 	col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+thickness,		y+thickness+sw,		sw, 			y_down-y-thickness-sw,	col_shadow, 	0, 0, NULL, NULL},

		/* horizontal info line - */
		{CC_FBDATA_TYPE_LINE, x,			y_down, 		width-thickness-sw, 	thickness, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x,			y_down+thickness, 	width-thickness-sw,	sw, 			col_shadow, 	0, 0, NULL, NULL},

		/* vertical info mark | */
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw,	y_mark_down, 		thickness, 		h_mark_down, 		col_body, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-sw,		y_mark_down, 		sw, 			h_mark_down, 		col_shadow, 	0, 0, NULL, NULL},
		{CC_FBDATA_TYPE_LINE, x+width-thickness-sw,	y_mark_down+h_mark_down,thickness+sw, 		sw,	 		col_shadow, 	0, 0, NULL, NULL},
	};

	for(size_t i =0; i< (sizeof(fbdata) / sizeof(fbdata[0])) ;i++)
		v_fbdata.push_back(fbdata[i]);

	paintFbItems(do_save_bg);
}

//remove painted fb items from screen
void CComponentsDetailLine::kill()
{
	//save current colors
	fb_pixel_t c_tmp1, c_tmp2;
	c_tmp1 = col_body;
	c_tmp2 = col_shadow;

	//set background color
	col_body = col_shadow = COL_BACKGROUND;

	//paint with background and restore, set last used colors
	paint(CC_SAVE_SCREEN_NO);
	col_body = c_tmp1;
	col_shadow = c_tmp2;
	firstPaint = true;
}

//synchronize colors for details line
//This is usefull if the system colors are changed during runtime
//so you can ensure correct applied system colors in relevant objects with unchanged instances.
void CComponentsDetailLine::syncSysColors()
{
	col_body 	= COL_MENUCONTENT_PLUS_6;
	col_shadow 	= COL_MENUCONTENTDARK_PLUS_0;
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsPIP from CComponentsItem
CComponentsPIP::CComponentsPIP(	const int x_pos, const int y_pos, const int percent, bool has_shadow)
{
	//CComponents, CComponentsItem
	initVarItem();

	//CComponentsPIP
	screen_w = frameBuffer->getScreenWidth(true);
	screen_h = frameBuffer->getScreenHeight(true);

	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	width 		= percent*screen_w/100;
	height	 	= percent*screen_h/100;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= COL_BACKGROUND;
	col_body	= COL_BACKGROUND;
	col_shadow	= COL_MENUCONTENTDARK_PLUS_0;
}

CComponentsPIP::~CComponentsPIP()
{
	hide();
	clearSavedScreen();
	clear();
	videoDecoder->Pig(-1, -1, -1, -1);
}

void CComponentsPIP::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
	videoDecoder->Pig(x+fr_thickness, y+fr_thickness, width-2*fr_thickness, height-2*fr_thickness, screen_w, screen_h);
}


void CComponentsPIP::hide(bool no_restore)
{
	hideCCItem(no_restore);
	videoDecoder->Pig(-1, -1, -1, -1);
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsPicture from CComponentsItem
CComponentsPicture::CComponentsPicture(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& image_name, const int alignment, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	init(x_pos, y_pos, image_name, alignment, has_shadow, color_frame, color_background, color_shadow);

	width	= w;
	height	= h;
	pic_paint_mode 	= CC_PIC_IMAGE_MODE_AUTO,

	initVarPicture();
}

void CComponentsPicture::init(	int x_pos, int y_pos, const string& image_name, const int alignment, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsItem
	initVarItem();

	//CComponentsPicture
	pic_name 	= image_name;
	pic_align	= alignment;
	pic_offset	= 1;
	pic_paint	= true;
	pic_paintBg	= false;
	pic_painted	= false;
	do_paint	= false;
	pic_max_w	= 0;
	pic_max_h	= 0;
	if (pic_name.empty())
		pic_width = pic_height = 0;

	//CComponents
	x = pic_x	= x_pos;
	y = pic_y	= y_pos;
	height		= 0;
	width 		= 0;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_background;
	col_shadow	= color_shadow;
}

void CComponentsPicture::setPicture(const std::string& picture_name)
{
	pic_name = picture_name;
	initVarPicture();
}


void CComponentsPicture::setPictureAlign(const int alignment)
{
	pic_align	= alignment;
	initVarPicture();
}


void CComponentsPicture::initVarPicture()
{
	pic_width = pic_height = 0;
	pic_painted = false;
	do_paint = false;

	if (pic_max_w == 0)
		pic_max_w = width-2*fr_thickness;

	if (pic_max_h == 0)
		pic_max_h = height-2*fr_thickness;

	//set the image mode depends of name syntax, icon names contains no path,
	//so we can detect the required image mode
	if (pic_paint_mode == CC_PIC_IMAGE_MODE_AUTO){
		if (!access(pic_name.c_str(), F_OK ))
			pic_paint_mode = CC_PIC_IMAGE_MODE_ON;
		else
			pic_paint_mode = CC_PIC_IMAGE_MODE_OFF;
	}

	if (pic_paint_mode == CC_PIC_IMAGE_MODE_OFF){
		frameBuffer->getIconSize(pic_name.c_str(), &pic_width, &pic_height);
#if 0
		pic_width = max(pic_width, pic_max_w);
		pic_height = max(pic_height, pic_max_h);
#endif
	}
	
	if (pic_paint_mode == CC_PIC_IMAGE_MODE_ON) {
		g_PicViewer->getSize(pic_name.c_str(), &pic_width, &pic_height);
		if((pic_width > pic_max_w) || (pic_height > pic_max_h))
			g_PicViewer->rescaleImageDimensions(&pic_width, &pic_height, pic_max_w, pic_max_h);
	}

#ifdef DEBUG_CC
	if (pic_width == 0 || pic_height == 0)
		printf("CComponentsPicture: %s file: %s, no icon dimensions found! width = %d, height = %d\n", __FUNCTION__, pic_name.c_str(),  pic_width, pic_height);
#endif

	pic_x += fr_thickness;
	pic_y += fr_thickness;

	if (pic_height>0 && pic_width>0){
		if (pic_align & CC_ALIGN_LEFT)
			pic_x = x+fr_thickness;
		if (pic_align & CC_ALIGN_RIGHT)
			pic_x = x+width-pic_width-fr_thickness;
		if (pic_align & CC_ALIGN_TOP)
			pic_y = y+fr_thickness;
		if (pic_align & CC_ALIGN_BOTTOM)
			pic_y = y+height-pic_height-fr_thickness;
		if (pic_align & CC_ALIGN_HOR_CENTER)
			pic_x = x+width/2-pic_width/2;
		if (pic_align & CC_ALIGN_VER_CENTER)
			pic_y = y+height/2-pic_height/2;

		do_paint = true;
	}

	int sw = (shadow ? shadow_w :0);
	width = max(pic_width, width)  + sw ;
	height = max(pic_height, height)  + sw ;
}

void CComponentsPicture::paint(bool do_save_bg)
{
	initVarPicture();
	paintInit(do_save_bg);
	pic_painted = false;

	if (do_paint){
		if (pic_paint_mode == CC_PIC_IMAGE_MODE_OFF)
			pic_painted = frameBuffer->paintIcon(pic_name, pic_x, pic_y, 0 /*pic_max_h*/, pic_offset, pic_paint, pic_paintBg, col_body);
		else if (pic_paint_mode == CC_PIC_IMAGE_MODE_ON)
			pic_painted = g_PicViewer->DisplayImage(pic_name, pic_x, pic_y, pic_width, pic_height);
		do_paint = false;
	}
}

void CComponentsPicture::hide(bool no_restore)
{
	hideCCItem(no_restore);
	pic_painted = false;
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsItemBox from CComponentsItem
CComponentsItemBox::CComponentsItemBox()
{
	//CComponentsItemBox
	initVarItemBox();
}

CComponentsItemBox::~CComponentsItemBox()
{
	hide();
	clearElements();
	clearSavedScreen();
	clear();
}

void CComponentsItemBox::initVarItemBox()
{
	//CComponents, CComponentsItem
	initVarItem();

	//CComponentsItemBox
	it_col_text 		= COL_MENUCONTENT;
	hSpacer 		= 2;
	hOffset 		= 4;
	vOffset 		= 1;
	digit_h			= 0;
	digit_offset		= 0;
	font_text		= NULL;
	paintElements 		= true;
	hMax 			= 0;
	has_TextElement		= false;
	firstElementLeft 	= FIRST_ELEMENT_INIT;
	firstElementRight	= FIRST_ELEMENT_INIT;
	prevElementLeft 	= 0;
	prevElementRight 	= 0;
	onlyOneTextElement	= false;
	isCalculated		= false;
	v_element_data.clear();
}

int CComponentsItemBox::getHeight()
{
	if (!isCalculated)
		calculateElements();
	return height;
}

void CComponentsItemBox::clearElements()
{
	for(size_t i = 0; i < v_element_data.size(); i++) {
		switch (v_element_data[i].type) {
			case CC_ITEMBOX_ICON:
			case CC_ITEMBOX_PICTURE:
				if (v_element_data[i].handler1 != NULL)
					delete static_cast<CComponentsPicture*>(v_element_data[i].handler1);
				break;
			case CC_ITEMBOX_TEXT:
				if (v_element_data[i].handler1 != NULL)
					delete static_cast<CBox*>(v_element_data[i].handler1);
				if (v_element_data[i].handler2 != NULL)
					delete static_cast<CTextBox*>(v_element_data[i].handler2);
				break;
			default:
				break;
		}
	}
	isCalculated = false;
	v_element_data.clear();
}

bool CComponentsItemBox::addLogoOrText(int align, const std::string& logo, const std::string& text, size_t *index)
{
	comp_element_data_t data;
	int dx=0, dy=0;

	data.align 	= align;
	data.x 		= x;
	data.y 		= y;
	data.width 	= 0;
	data.height 	= 0;
	data.handler1 	= NULL;
	data.handler2 	= NULL;

	g_PicViewer->getSize(logo.c_str(), &dx, &dy);
	if ((dx != 0) && (dy != 0)) {
		// logo OK
		data.type 	= CC_ITEMBOX_PICTURE;
		data.element 	= logo;
	}
	else {
		// no logo
		if ((text == "") || ((onlyOneTextElement) && (has_TextElement)))
			return false;
		else
			has_TextElement = true;
		if (font_text != NULL)
			data.height = font_text->getHeight();
		data.type 	= CC_ITEMBOX_TEXT;
		data.element 	= text;
	}

	v_element_data.push_back(data);
	if (index != NULL)
		*index = v_element_data.size()-1;
	isCalculated = false;
	return true;
}

void CComponentsItemBox::addText(const std::string& s_text, const int align, size_t *index)
{
	addElement(align, CC_ITEMBOX_TEXT, s_text, index);
}

void CComponentsItemBox::addText(neutrino_locale_t locale_text, const int align, size_t *index)
{
	addElement(align, CC_ITEMBOX_TEXT, g_Locale->getText(locale_text), index);
}

void CComponentsItemBox::addIcon(const std::string& s_icon_name, const int align, size_t *index)
{
	addElement(align, CC_ITEMBOX_ICON, s_icon_name, index);
}

void CComponentsItemBox::addPicture(const std::string& s_picture_path, const int align, size_t *index)
{
	addElement(align, CC_ITEMBOX_PICTURE, s_picture_path, index);
}

void CComponentsItemBox::addClock(const int align, size_t *index)
{
	addElement(align, CC_ITEMBOX_CLOCK, "", index);
}

bool CComponentsItemBox::addElement(int align, int type, const std::string& element, size_t *index)
{
	comp_element_data_t data;
	int dx=0, dy=0;

	switch (type)
	{
		case CC_ITEMBOX_ICON:
			frameBuffer->getIconSize(element.c_str(), &dx, &dy);
			if ((dx == 0) || (dy == 0))
				return false;
			break;
		case CC_ITEMBOX_TEXT:
			if ((element == "") || ((onlyOneTextElement) && (has_TextElement)))
				return false;
			else
				has_TextElement = true;
			break;
		default:
			break;
	}

	data.type 	= type;
	data.align 	= align;
	data.element 	= element;
	data.x 		= x;
	data.y 		= y;
	data.width 	= 0;
	data.height 	= 0;
	data.handler1 	= NULL;
	data.handler2 	= NULL;

	v_element_data.push_back(data);
	if (index != NULL)
		*index = v_element_data.size()-1;
	isCalculated = false;
	return true;
}

void CComponentsItemBox::refreshElement(size_t index, const std::string& element)
{
	CComponentsPicture* pic = NULL;
	switch (v_element_data[index].type) {
		case CC_ITEMBOX_PICTURE:
			pic = static_cast<CComponentsPicture*>(v_element_data[index].handler1);
			if (pic != NULL) {
				pic->hide();
				delete pic;
			}
			v_element_data[index].element	= element;
			v_element_data[index].x 	= x;
			v_element_data[index].y 	= y;
			v_element_data[index].width 	= 0;
			v_element_data[index].height 	= 0;
			v_element_data[index].handler1 	= NULL;
			v_element_data[index].handler2 	= NULL;
			break;
		default:
			break;
	}
	calculateElements();
}

//paint image into item box
void CComponentsItemBox::paintImage(size_t index, bool newElement)
{
	CComponentsPicture* pic = NULL;
	pic = static_cast<CComponentsPicture*>(v_element_data[index].handler1);

	int pw = 0, ph = 0;

	if ((newElement) || (pic == NULL)) {
		if (pic != NULL) {
			pic->hide();
			delete pic;
			pic = NULL;
		}
		if ((v_element_data[index].type) == CC_ITEMBOX_PICTURE)
			pic = new CComponentsPicture(	v_element_data[index].x, v_element_data[index].y, v_element_data[index].width,
							v_element_data[index].height, v_element_data[index].element);
		else
			pic = new CComponentsPicture(	v_element_data[index].x, v_element_data[index].y, 0, 0, v_element_data[index].element);
		v_element_data[index].handler1 = (void*)pic;
	}

	pic->getPictureSize(&pw, &ph);
	pic->setHeight(ph);
	pic->setWidth(pw);
	pic->setColorBody(col_body);
	pic->paint();
}

//paint text into item box
void CComponentsItemBox::paintText(size_t index, bool newElement)
{
	//prepare textbox dimension instances
	CBox* box = NULL;
	box = static_cast<CBox*>(v_element_data[index].handler1);

	if ((newElement) || (box == NULL)) {
		if (box != NULL) {
			delete box;
			box = NULL;
		}
		box = new CBox();
		v_element_data[index].handler1 = (void*)box;
	}

	box->iX      = v_element_data[index].x;
	box->iY      = v_element_data[index].y;
	box->iWidth  = v_element_data[index].width;
	box->iHeight = v_element_data[index].height;


	//prepare text
	CTextBox* textbox = NULL;
	textbox = static_cast<CTextBox*>(v_element_data[index].handler2);

	if ((newElement) || (textbox == NULL)) {
		if (textbox != NULL) {
			textbox->hide();
			delete textbox;
			textbox = NULL;
		}
		textbox = new CTextBox(v_element_data[index].element.c_str(), font_text, CTextBox::AUTO_WIDTH|CTextBox::AUTO_HIGH, box, col_body);
		v_element_data[index].handler2 = (void*)textbox;
	}

	textbox->setTextBorderWidth(0);
	textbox->enableBackgroundPaint(false);
	textbox->setTextFont(font_text);
	textbox->movePosition(box->iX, box->iY);
	textbox->setTextColor(it_col_text);

	if (textbox->setText(&v_element_data[index].element))
		textbox->paint();
}


//paint available elements at one task
void CComponentsItemBox::paintElement(size_t index, bool newElement)
{
	switch (v_element_data[index].type) {
		case CC_ITEMBOX_ICON:
		case CC_ITEMBOX_PICTURE:
			paintImage(index,newElement);
			break;
		case CC_ITEMBOX_TEXT:
			paintText(index,newElement);
			break;
		case CC_ITEMBOX_CLOCK:
			font_text->RenderString(v_element_data[index].x, v_element_data[index].y, v_element_data[index].width,
						v_element_data[index].element.c_str(), it_col_text);
			break;
		default:
			break;
	}
}

void CComponentsItemBox::calSizeOfElements()
{
	size_t i;

	// Set element size
	for (i = 0; i < v_element_data.size(); i++) {
		switch (v_element_data[i].type)
		{
			case CC_ITEMBOX_ICON:
				frameBuffer->getIconSize(v_element_data[i].element.c_str(), &v_element_data[i].width, &v_element_data[i].height);
				break;
			case CC_ITEMBOX_PICTURE:
				g_PicViewer->getSize(v_element_data[i].element.c_str(), &v_element_data[i].width, &v_element_data[i].height);
				break;
			case CC_ITEMBOX_TEXT:
				if (font_text != NULL)
					v_element_data[i].height = font_text->getHeight();
				break;
			case CC_ITEMBOX_CLOCK: {
				if (!g_Sectionsd->getIsTimeSet())
					break;
				if (font_text != NULL) {
					char timestr[10] = {0};
					time_t now = time(NULL);
					struct tm *tm = localtime(&now);
					strftime(timestr, sizeof(timestr)-1, "%H:%M", tm);

					digit_h = font_text->getDigitHeight();
					digit_offset = font_text->getDigitOffset();
					v_element_data[i].height = digit_h + (int)((float)digit_offset*1.5);
//					v_element_data[i].width = font_text->getRenderWidth(widest_number)*4 + font->getRenderWidth(":");
					v_element_data[i].width = font_text->getRenderWidth(timestr);
					v_element_data[i].element = timestr;
				}
			}
				break;
			default:
				break;
		}
	}

	// Calculate largest height without CC_ITEMBOX_PICTURE
	firstElementLeft 	= FIRST_ELEMENT_INIT;
	firstElementRight	= FIRST_ELEMENT_INIT;
	for (i = 0; i < v_element_data.size(); i++) {
		if ((firstElementLeft == FIRST_ELEMENT_INIT) && (v_element_data[i].align == CC_ALIGN_LEFT))
			firstElementLeft = i;
		if ((firstElementRight == FIRST_ELEMENT_INIT) && (v_element_data[i].align == CC_ALIGN_RIGHT))
			firstElementRight = i;
		if (v_element_data[i].type != CC_ITEMBOX_PICTURE)
			hMax = max(v_element_data[i].height, hMax);
	}
}

void CComponentsItemBox::calPositionOfElements()
{
	size_t i;

	// Calculate y-positions
	height = hMax + 2*vOffset;
	for (i = 0; i < v_element_data.size(); i++) {
		v_element_data[i].y = y + (height - v_element_data[i].height) / 2;
		if (v_element_data[i].type == CC_ITEMBOX_CLOCK)
			v_element_data[i].y += v_element_data[i].height + digit_offset/4;
	}

	// Calculate x-positions
	for (i = 0; i < v_element_data.size(); i++) {
		if (firstElementLeft == i){
			prevElementLeft = i;
			v_element_data[i].x = x + hOffset + corner_rad/2;
		}
		else if (firstElementRight == i){
			prevElementRight = i;
			v_element_data[i].x = x + width - v_element_data[i].width - hOffset - corner_rad/2;
		}
		else {
			if (v_element_data[i].align == CC_ALIGN_LEFT) {
				// left elements
				v_element_data[i].x = v_element_data[prevElementLeft].x + v_element_data[prevElementLeft].width + hSpacer;
				prevElementLeft = i;
			}
			else {
				// right elements
				v_element_data[i].x = v_element_data[prevElementRight].x - v_element_data[i].width - hSpacer;
				prevElementRight = i;
			}
		}
	}
}

void CComponentsItemBox::calculateElements()
{
	if (v_element_data.empty())
		return;

	size_t i;

	calSizeOfElements();

	// hMax correction if no text element.
	if (!has_TextElement)
		hMax = max(font_text->getHeight(), hMax);

	// Calculate logo
	for (i = 0; i < v_element_data.size(); i++) {
		if (v_element_data[i].type == CC_ITEMBOX_PICTURE) {
			if ((v_element_data[i].width > LOGO_MAX_WIDTH) || (v_element_data[i].height > hMax))
				g_PicViewer->rescaleImageDimensions(&v_element_data[i].width, &v_element_data[i].height, LOGO_MAX_WIDTH, hMax);
		}
	}

	// Calculate text width
	int allWidth = 0;
	for (i = 0; i < v_element_data.size(); i++) {
		if (v_element_data[i].type != CC_ITEMBOX_TEXT)
			allWidth += v_element_data[i].width + hSpacer;
	}
	for (i = 0; i < v_element_data.size(); i++) {
		if (v_element_data[i].type == CC_ITEMBOX_TEXT) {
			v_element_data[i].width = width - (allWidth + 2*hSpacer);
			// If text is too long, number of rows = 2
			if (font_text->getRenderWidth(v_element_data[i].element) > v_element_data[i].width) {
				v_element_data[i].height = font_text->getHeight() * 2;
				hMax = max(v_element_data[i].height, hMax);
			}
			break;
		}
	}

	calPositionOfElements();
	isCalculated = true;
}

void CComponentsItemBox::paintItemBox(bool do_save_bg)
{
	// paint background
	paintInit(do_save_bg);

	if ((v_element_data.empty()) || (!paintElements))
		return;

	// paint elements
	for (size_t i = 0; i < v_element_data.size(); i++)
		paintElement(i);
}

void CComponentsItemBox::clearTitlebar()
{
	clearElements();
	paintElements = false;
	paint(false);
	paintElements = true;
}



//-------------------------------------------------------------------------------------------------------
//sub class CComponentsTitleBar from CComponentsItemBox
CComponentsTitleBar::CComponentsTitleBar()
{
	//CComponentsTitleBar
	initVarTitleBar();
}

void CComponentsTitleBar::initVarTitleBar()
{
	//CComponentsItemBox
	initVarItemBox();
	onlyOneTextElement	= true;

	font_text	= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	it_col_text 	= COL_MENUHEAD;

	//CComponents
	x		= 0;
	y 		= 0;
	height		= font_text->getHeight() + 2*hSpacer;
	width 		= frameBuffer->getScreenWidth(true);;
	col_body	= COL_MENUHEAD_PLUS_0;
	corner_type 	= CORNER_TOP;
	corner_rad	= RADIUS_LARGE;

	//CComponentsTitleBar
	tb_text_align	= CC_ALIGN_LEFT;
	tb_icon_align	= CC_ALIGN_LEFT;
	tb_c_text	= NULL;
	tb_s_text	= "";
	tb_locale_text	= NONEXISTANT_LOCALE;
	tb_icon_name	= "";
}

CComponentsTitleBar::CComponentsTitleBar(const int x_pos, const int y_pos, const int w, const int h, const char* c_text, const std::string& s_icon,
					fb_pixel_t color_text, fb_pixel_t color_body)
{
	//CComponentsItemBox
	initVarTitleBar();
	it_col_text 	= color_text;

	//CComponents
	x		= x_pos;
	y 		= y_pos;
	height		= h;
	width 		= w;
	col_body	= color_body;

	//CComponentsTitleBar
	tb_c_text	= c_text;
	tb_icon_name	= s_icon;

	initElements();
}

CComponentsTitleBar::CComponentsTitleBar(const int x_pos, const int y_pos, const int w, const int h, const string& s_text, const std::string& s_icon,
					fb_pixel_t color_text, fb_pixel_t color_body)
{
	//CComponentsItemBox
	initVarTitleBar();
	it_col_text 	= color_text;

	//CComponents
	x		= x_pos;
	y 		= y_pos;
	height		= h;
	width 		= w;
	col_body	= color_body;

	//CComponentsTitleBar
	tb_s_text	= s_text;
	tb_icon_name	= s_icon;

	initElements();
}

CComponentsTitleBar::CComponentsTitleBar(const int x_pos, const int y_pos, const int w, const int h, neutrino_locale_t locale_text, const std::string& s_icon,
					fb_pixel_t color_text, fb_pixel_t color_body)
{
	//CComponentsItemBox
	initVarTitleBar();
	it_col_text 	= color_text;

	//CComponents
	x		= x_pos;
	y 		= y_pos;
	height		= h;
	width 		= w;
	col_body	= color_body;

	//CComponentsTitleBar
	tb_locale_text	= locale_text;
	tb_s_text	= g_Locale->getText(tb_locale_text);
	tb_icon_name	= s_icon;

	initElements();
}

///basic init methodes for constructors ***************************************
void CComponentsTitleBar::initIcon()
{
	if (!tb_icon_name.empty())
		addElement	(tb_icon_align, CC_ITEMBOX_ICON, tb_icon_name);
}

void CComponentsTitleBar::initText()
{
	if (tb_c_text){
		addElement	(tb_text_align, CC_ITEMBOX_TEXT, tb_c_text);
		return;
	}
	else if (!tb_s_text.empty()){
		addElement	(tb_text_align, CC_ITEMBOX_TEXT, tb_s_text);
		return;
	}
}

void CComponentsTitleBar::initElements()
{
	initIcon();
	initText();
}
///*****************************************************************************

void CComponentsTitleBar::paint(bool do_save_bg)
{
	calculateElements();
	paintItemBox(do_save_bg);
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsForm from CComponentsItem
CComponentsForm::CComponentsForm()
{
	//CComponentsForm
	initVarForm();
}

CComponentsForm::CComponentsForm(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsForm
	initVarForm();

	//CComponents
	x		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;

	shadow		= has_shadow;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
}

CComponentsForm::~CComponentsForm()
{
#ifdef DEBUG_CC
	printf("[CComponents] calling %s...\n", __FUNCTION__);
#endif
	hide();
	clearSavedScreen();
	clearCCItems();
	clear();
}

void CComponentsForm::clearCCItems()
{
	if (v_cc_items.empty())
		return;
	
	for(size_t i=0; i<v_cc_items.size(); i++) {
		if (v_cc_items[i])
			delete v_cc_items[i];
		v_cc_items[i] = NULL;
	}
	v_cc_items.clear();
}


void CComponentsForm::initVarForm()
{
	//CComponentsItem
	initVarItem();
	
	//simple default dimensions
	x 		= 0;
	y 		= 0;
	width 		= 150;
	height	 	= 150;
	shadow		= CC_SHADOW_OFF;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= COL_MENUCONTENT_PLUS_6;
	col_body	= COL_MENUCONTENT_PLUS_0;
	col_shadow	= COL_MENUCONTENTDARK_PLUS_0;
	corner_rad	= RADIUS_LARGE;
	corner_type 	= CORNER_ALL;
	
	//CComponentsForm
	v_cc_items.clear();
	
}

void CComponentsForm::addCCItem(CComponentsItem* cc_Item)
{
	v_cc_items.push_back(cc_Item);
}

int CComponentsForm::getCCItemId(CComponentsItem* cc_Item)
{
	for (size_t i= 0; i< v_cc_items.size(); i++)
		if (v_cc_items[i] == cc_Item)
			return i;
	return -1;
}

CComponentsItem* CComponentsForm::getCCItem(const uint& cc_item_id)
{
	if (v_cc_items[cc_item_id])
		return v_cc_items[cc_item_id];
	return NULL;
}

void CComponentsForm::insertCCItem(const uint& cc_item_id, CComponentsItem* cc_Item)
{
#ifdef DEBUG_CC
	if (cc_Item == NULL){
		printf("CComponentsForm: %s parameter: cc_Item = %d...\n", __FUNCTION__, (int)cc_Item);
		return;
	}
#endif
	v_cc_items.insert(v_cc_items.begin()+cc_item_id, cc_Item);
}

void CComponentsForm::removeCCItem(const uint& cc_item_id)
{
	if (v_cc_items[cc_item_id]) {
		delete v_cc_items[cc_item_id];
		v_cc_items[cc_item_id] = NULL;
		v_cc_items.erase(v_cc_items.begin()+cc_item_id);
	}
}

void CComponentsForm::paint(bool do_save_bg)
{
	//paint body
	paintInit(do_save_bg);
	
	//paint items
	paintCCItems();	
}

void CComponentsForm::paintCCItems()
{	
	size_t items_count = v_cc_items.size();
	int x_frm_left 		= x+fr_thickness; //left form border
	int y_frm_top 		= y+fr_thickness; //top form border
	int x_frm_right		= x+width-fr_thickness; //right form border
	int y_frm_bottom	= y+height-fr_thickness; //bottom form border
		
	for(size_t i=0; i<items_count; i++) {
		//cache original item position and dimensions
		int x_item, y_item, w_item, h_item;
		v_cc_items[i]->getDimensions(&x_item, &y_item, &w_item, &h_item);
		

		int xy_ref = 0+fr_thickness; //allowed minimal x and y start position
		if (x_item < xy_ref){
#ifdef DEBUG_CC
			printf("[CComponentsForm] %s: item %d position is out of form dimensions\ndefinied x=%d\nallowed x>=%d\n", __FUNCTION__, i, x_item, xy_ref);
#endif
			x_item = xy_ref;
		}
		if (y_item < xy_ref){
#ifdef DEBUG_CC
			printf("[CComponentsForm] %s: item %d position is out of form dimensions\ndefinied y=%d\nallowed y>=%d\n", __FUNCTION__, i, y_item, xy_ref);
#endif
			y_item = xy_ref;
		}
		
		//set adapted position onto form
		v_cc_items[i]->setXPos(x_frm_left+x_item);
		v_cc_items[i]->setYPos(y_frm_top+y_item);

		//watch horizontal x dimensions of items
		int x_item_right = v_cc_items[i]->getXPos()+w_item; //right item border
		if (x_item_right > x_frm_right){
			v_cc_items[i]->setWidth(w_item-(x_item_right-x_frm_right));
#ifdef DEBUG_CC
			printf("[CComponentsForm] %s: item %d too large, definied width=%d, possible width=%d \n", __FUNCTION__, i, w_item, v_cc_items[i]->getWidth());
#endif
		}

		//watch vertical y dimensions
		int y_item_bottom = v_cc_items[i]->getYPos()+h_item; //bottom item border
		if (y_item_bottom > y_frm_bottom){
			v_cc_items[i]->setHeight(h_item-(y_item_bottom-y_frm_bottom));
#ifdef DEBUG_CC
			printf("[CComponentsForm] %s: item %d too large, definied height=%d, possible height=%d \n", __FUNCTION__, i, h_item, v_cc_items[i]->getHeight());
#endif
		}
		
		//paint element without saved screen!
		v_cc_items[i]->paint(CC_SAVE_SCREEN_NO);
		
		//restore dimensions and position
		v_cc_items[i]->setDimensionsAll(x_item, y_item, w_item, h_item);
	}
}

void CComponentsForm::hide(bool no_restore)
{
	//hide body
	hideCCItem(no_restore);
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsHeader inherit from CComponentsForm
CComponentsHeader::CComponentsHeader()
{
	//CComponentsHeader
	initVarHeader();
}

CComponentsHeader::CComponentsHeader(	const int x_pos, const int y_pos, const int w, const int h, const std::string& caption, const char* icon_name, const int buttons, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsHeader
	initVarHeader();

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h > 0 ? h : g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	cch_text	= caption;
	cch_icon_name	= icon_name;
	cch_buttons	= buttons;
}

CComponentsHeader::CComponentsHeader(	const int x_pos, const int y_pos, const int w, const int h, neutrino_locale_t caption_locale, const char* icon_name, const int buttons, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponentsHeader
	initVarHeader();
	
	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	cch_locale_text = caption_locale;
	cch_icon_name	= icon_name;
	cch_buttons	= buttons;
}

#if 0 
CComponentsHeader::~CComponentsHeader()
{
	hide();
	clearSavedScreen();
	clearCCItems();
	clear();
}
#endif

void CComponentsHeader::initVarHeader()
{
	//CComponentsHeader
	cch_font 		= g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	cch_icon_obj		= NULL;
	cch_text_obj		= NULL;
	cch_icon_name		= NULL;
	cch_btn_obj		= NULL;
	cch_text		= "header";
	cch_locale_text 	= NONEXISTANT_LOCALE;
	cch_col_text		= COL_MENUHEAD;
	cch_items_y 		= 0;
	cch_icon_x 		= 0;
	cch_icon_w		= 5;
	cch_text_x		= 0;
	ccif_width 		= 0;
	cch_buttons		= 0;
	cch_btn_offset		= 8;
	v_cch_btn.clear();	
	
	//CComponentsForm
	initVarForm();
	height 			= cch_font->getHeight();
	
	col_body 		= COL_MENUHEAD_PLUS_0;
	corner_rad		= RADIUS_LARGE,
	corner_type		= CORNER_TOP;
}

void CComponentsHeader::setHeaderText(const std::string& caption)
{
	cch_text	= caption;
}

void CComponentsHeader::setHeaderText(neutrino_locale_t caption_locale)
{
	cch_text	= g_Locale->getText(caption_locale);
}

void CComponentsHeader::setHeaderIcon(const char* icon_name)
{
	cch_icon_name 	= icon_name;
}

void CComponentsHeader::initCCHeaderIcon()
{
	if (cch_icon_name == NULL) {
// 		cch_icon_w = 5;
		return;
	}

	if (cch_icon_name)
		cch_icon_obj = new CComponentsPicture(cch_icon_x, cch_items_y, 0, 0, cch_icon_name);
	cch_icon_obj->setWidth(height-2*fr_thickness);
	cch_icon_obj->setHeight(height);
	cch_icon_obj->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
	cch_icon_obj->doPaintBg(false);

	//corner of icon item
	cch_icon_obj->setCornerRadius(corner_rad-fr_thickness);
	int cc_icon_corner_type = corner_type;
	if (corner_type == CORNER_TOP_LEFT || corner_type == CORNER_TOP)
		cc_icon_corner_type = CORNER_TOP_LEFT;
	else
		cc_icon_corner_type = CORNER_LEFT;
	cch_icon_obj->setCornerType(cc_icon_corner_type);

	//set width of icon object
	cch_icon_w = cch_icon_obj->getWidth();
}

void CComponentsHeader::addHeaderButton(const std::string& button_name)
{
	v_cch_btn.push_back(button_name);
}

void CComponentsHeader::removeHeaderButtons()
{
	v_cch_btn.clear();
	if (cch_btn_obj)
		cch_btn_obj->removeAllIcons();
}

void CComponentsHeader::initCCHDefaultButtons()
{
	if (cch_buttons & CC_BTN_EXIT)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_HOME);
	if (cch_buttons & CC_BTN_HELP)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_HELP);
	if (cch_buttons & CC_BTN_INFO)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_INFO);
	if (cch_buttons & CC_BTN_MENU)
		v_cch_btn.push_back(NEUTRINO_ICON_BUTTON_MENU);
}

void CComponentsHeader::initCCHeaderButtons()
{
	initCCHDefaultButtons();
	
	//exit if no button defined
	if (v_cch_btn.empty())
		return;

	// calculate minimal width of icon form
	size_t btncnt = v_cch_btn.size();
	ccif_width = 0;
	for(size_t i=0; i<btncnt; i++){
		int bw, bh;
		frameBuffer->getIconSize(v_cch_btn[i].c_str(), &bw, &bh);
		ccif_width += (bw + cch_btn_offset);
	}

	cch_btn_obj = new CComponentsIconForm();
	cch_btn_obj->setDimensionsAll(0+width-ccif_width, 0, ccif_width-cch_btn_offset, height);
	cch_btn_obj->doPaintBg(false);
	cch_btn_obj->setIconOffset(cch_btn_offset);
	cch_btn_obj->setIconAlign(CComponentsIconForm::CC_ICONS_FRM_ALIGN_RIGHT);
	cch_btn_obj->removeAllIcons();
	cch_btn_obj->addIcon(v_cch_btn);
}

void CComponentsHeader::initCCHeaderText()
{
	cch_text_x = cch_icon_x+cch_icon_w;
	cch_text_obj = new CComponentsText(cch_text_x, cch_items_y, width-cch_icon_w-fr_thickness, height-2*fr_thickness, cch_text.c_str());
	cch_text_obj->setTextFont(cch_font);
	cch_text_obj->setTextColor(cch_col_text);
	cch_text_obj->doPaintBg(false);
	
	//corner of text item
	cch_text_obj->setCornerRadius(corner_rad-fr_thickness);
	cch_text_obj->setCornerType(corner_type);		
}

void CComponentsHeader::initCCHItems()
{
	//clean up first possible old item objects, includes delete and clean up vector
	clearCCItems();
	
	//init icon
	initCCHeaderIcon();

	//init text
	initCCHeaderText();

	//init buttons
	initCCHeaderButtons();

	//add elements
	if (cch_icon_obj)
		addCCItem(cch_icon_obj); //icon
	if (cch_text_obj)
		addCCItem(cch_text_obj); //text
	if (cch_btn_obj)
		addCCItem(cch_btn_obj); //buttons

}

void CComponentsHeader::paint(bool do_save_bg)
{
	//paint body
	paintInit(do_save_bg);

	//init default header ccitems
	initCCHItems();

	//paint
	paintCCItems();
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsIconForm inherit from CComponentsForm
CComponentsIconForm::CComponentsIconForm()
{
	initVarIconForm();
}

CComponentsIconForm::CComponentsIconForm(const int x_pos, const int y_pos, const int w, const int h, const std::vector<std::string> v_icon_names, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarIconForm();

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height 		= h;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
		
	v_icons		= v_icon_names;
}

void CComponentsIconForm::initVarIconForm()
{
	//CComponentsForm
	initVarForm();

	//set default width and height to 0, this causes a dynamic adaptation of width and height of form
	width 		= 0;
	height 		= 0;

	v_icons.clear();
	ccif_offset 	= 2;
	ccif_icon_align = CC_ICONS_FRM_ALIGN_LEFT;
}

void CComponentsIconForm::addIcon(const std::string& icon_name)
{
	v_icons.push_back(icon_name);
}

void CComponentsIconForm::addIcon(std::vector<std::string> icon_name)
{
	for (size_t i= 0; i< icon_name.size(); i++)
		v_icons.push_back(icon_name[i]);
}

void CComponentsIconForm::insertIcon(const uint& icon_id, const std::string& icon_name)
{
	v_icons.insert(v_icons.begin()+icon_id, icon_name);
}

void CComponentsIconForm::removeIcon(const uint& icon_id)
{
	v_icons.erase(v_icons.begin()+icon_id);
}

void CComponentsIconForm::removeIcon(const std::string& icon_name)
{
	int id = getIconId(icon_name);
	removeIcon(id);
}

int CComponentsIconForm::getIconId(const std::string& icon_name)
{
	for (size_t i= 0; i< v_icons.size(); i++)
		if (v_icons[i] == icon_name)
			return i;
	return -1;
}

//For existing instances it's recommended
//to remove old items before add new icons, otherwise icons will be appended.
void CComponentsIconForm::removeAllIcons()
{
	if (!v_icons.empty())
		v_icons.clear();
	clearCCItems();
}

//get maximal form height depends of biggest icon height, but don't touch defined form height
void CComponentsIconForm::initMaxHeight(int *pheight)
{
	for (size_t i= 0; i< v_icons.size(); i++){
		int dummy, htmp;
		frameBuffer->getIconSize(v_icons[i].c_str(), &dummy, &htmp);
		*pheight = max(htmp, height)/*+2*fr_thickness*/;
	}
}

void CComponentsIconForm::initCCIcons()
{
	//clean up first possible old item objects, includes delete and clean up vector and icons
	clearCCItems();

	int ccp_y = 0;
	int ccp_h = 0;
	int ccp_w = 0;
	//calculate start pos of first icon
	int ccp_x = 0 + fr_thickness; //CC_ICONS_FRM_ALIGN_LEFT;
	
	if (ccif_icon_align == CC_ICONS_FRM_ALIGN_RIGHT)
		ccp_x += (width - fr_thickness);
	
	//get width of first icon
	frameBuffer->getIconSize(v_icons[0].c_str(), &ccp_w, &ccp_h);
		
	//get maximal form height
 	int h = 0;
	initMaxHeight(&h);

	//set xpos of first icon with right alignment, icon must positionized on the right border reduced with icon width
	if (ccif_icon_align == CC_ICONS_FRM_ALIGN_RIGHT)
		ccp_x -= ccp_w;

	//init and add item objects
	size_t i_cnt = 	v_icons.size();	//icon count
	
	for (size_t i= 0; i< i_cnt; i++){
		//create new cc-picture item object
		CComponentsPicture *ccp = NULL;
		ccp = new CComponentsPicture(ccp_x, ccp_y, ccp_w, h, v_icons[i]);
		ccp->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
 		ccp->doPaintBg(false);
		//add item to form
		addCCItem(ccp);

		//reset current width for next object
		ccp_w = 0;
		//get next icon size if available
 		size_t next_i = i+1;
		if (next_i != i_cnt)
			frameBuffer->getIconSize(v_icons[next_i].c_str(), &ccp_w, &ccp_h);

		//set next icon position
		int tmp_offset = ( i<i_cnt ? ccif_offset : 0 );

		if (ccif_icon_align == CC_ICONS_FRM_ALIGN_LEFT)
			ccp_x += (ccp->getWidth() + tmp_offset);
		if (ccif_icon_align == CC_ICONS_FRM_ALIGN_RIGHT)
			ccp_x -= (ccp_w + tmp_offset);
	}

	//calculate width and height of form
	int w_tmp = 0;
	int h_tmp = 0;
	for (size_t i= 0; i< i_cnt; i++){
		w_tmp += v_cc_items[i]->getWidth()+ccif_offset+fr_thickness;
		h_tmp = max(h_tmp, v_cc_items[i]->getHeight()+2*fr_thickness);

	}
	width = max(w_tmp, width);
	height = max(h_tmp, height);
}

void CComponentsIconForm::paint(bool do_save_bg)
{
	//init and add icons
	initCCIcons();

	//paint body
	paintInit(do_save_bg);

	//paint
	paintCCItems();
}
