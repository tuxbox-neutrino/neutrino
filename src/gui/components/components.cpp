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
	shadow			= CC_SHADOW_OFF;
	shadow_w		= SHADOW_OFFSET;

	firstPaint		= true;
	frameBuffer 		= CFrameBuffer::getInstance();
	v_fbdata.clear();
	bgMode 			= CC_BGMODE_STANDARD;
	saved_screen.pixbuf 	= NULL;
}

//paint framebuffer stuff and fill buffer
void CComponents::paintFbItems(struct comp_fbdata_t * fbdata, const int items_count, bool do_save_bg)
{
	if (firstPaint && do_save_bg)	{
		for(int i=0; i<items_count; i++){
			if (fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN){
				//printf("\n#####[%s - %d] firstPaint: %d, fbdata_type: %d\n \n", __FUNCTION__, __LINE__, firstPaint, fbdata[i].fbdata_type);
				if (bgMode == CC_BGMODE_PERMANENT) {
					saved_screen.x = fbdata[i].x;
					saved_screen.y = fbdata[i].y;
					saved_screen.dx = fbdata[i].dx;
					saved_screen.dy = fbdata[i].dy;
					clearSavedScreen();
					saved_screen.pixbuf = getScreen(saved_screen.x, saved_screen.y, saved_screen.dx, saved_screen.dy);
				}
				else {
					fbdata[i].pixbuf = getScreen(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy);
				}
				firstPaint = false;
				break;
			}
		}
	}
	
	for(int i=0; i< items_count ;i++){
		int fbtype = fbdata[i].fbdata_type;
		
		if (firstPaint){
			
			if (do_save_bg && fbtype == CC_FBDATA_TYPE_LINE)
				fbdata[i].pixbuf = getScreen(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy);
			v_fbdata.push_back(fbdata[i]);
			
			//ensure painting of all line fb items with saved screens
			if (fbtype == CC_FBDATA_TYPE_LINE)
				firstPaint = true;
			else
				firstPaint = false;
		}
		if (fbtype != CC_FBDATA_TYPE_BGSCREEN){
			if (fbtype == CC_FBDATA_TYPE_FRAME && fbdata[i].frame_thickness > 0)
				frameBuffer->paintBoxFrame(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy, fbdata[i].frame_thickness, fbdata[i].color, fbdata[i].r);
			else if (fbtype == CC_FBDATA_TYPE_BACKGROUND)
				frameBuffer->paintBackgroundBoxRel(x, y, fbdata[i].dx, fbdata[i].dy);
			else
				frameBuffer->paintBoxRel(fbdata[i].x, fbdata[i].y, fbdata[i].dx, fbdata[i].dy, fbdata[i].color, fbdata[i].r, corner_type);
		}
	}
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
//abstract sub class CComponentsContainer from CComponents
CComponentsContainer::CComponentsContainer()
{
	//CComponentsContainer
	initVarContainer();
}

// 	 y
// 	x+------f-r-a-m-e-------+
// 	 |			|
//     height	  body		|
// 	 |			|
// 	 +--------width---------+

void CComponentsContainer::initVarContainer()
{
	//CComponents
	initVarBasic();
	
	//ComponentsContainer
	corner_rad	= 0;
	fr_thickness	= 0;
}

void CComponentsContainer::paintInit(bool do_save_bg)
{
	clear();
	
	int sw = shadow ? shadow_w : 0;
	int th = fr_thickness;
	
	comp_fbdata_t fbdata[] =
	{
		{CC_FBDATA_TYPE_BGSCREEN, 	x,	y, 	width+sw, 	height+sw, 	0, 		0, 		0, 	NULL,	NULL},
		{CC_FBDATA_TYPE_SHADOW, 	x+sw,	y+sw, 	width, 		height, 	col_shadow, 	corner_rad, 	0, 	NULL,	NULL},
		{CC_FBDATA_TYPE_FRAME, 		x, 	y, 	width, 		height, 	col_frame, 	corner_rad, 	th, 	NULL,	NULL},
		{CC_FBDATA_TYPE_BOX, 		x+th,	y+th, 	width-2*th, 	height-2*th, 	col_body, 	corner_rad-th,  0, 	NULL,	NULL},
	};
	
	int items_cnt = sizeof(fbdata) / sizeof(fbdata[0]);
	
	paintFbItems(fbdata, items_cnt, do_save_bg);
}

void CComponentsContainer::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
}

//restore last saved screen behind form box,
//Do use parameter 'no restore' to override temporarly the restore funtionality.
//This could help to avoid ugly flicker efffects if it is necessary e.g. on often repaints, without changed contents.
void CComponentsContainer::hideContainer(bool no_restore)
{
	if (bgMode == CC_BGMODE_PERMANENT) {
		if (saved_screen.pixbuf) {
			frameBuffer->RestoreScreen(saved_screen.x, saved_screen.y, saved_screen.dx, saved_screen.dy, saved_screen.pixbuf);
			if (no_restore) {
				delete[] saved_screen.pixbuf;
				saved_screen.pixbuf = NULL;
				firstPaint = true;
			}
		}
	}
	else {
		if (no_restore)
			return;

		for(size_t i =0; i< v_fbdata.size() ;i++) {
			if (v_fbdata[i].pixbuf != NULL && v_fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN)
				frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
			delete[] v_fbdata[i].pixbuf;
		}
		v_fbdata.clear();
		firstPaint = true;
	}
}

void CComponentsContainer::hide(bool no_restore)
{
	hideContainer(no_restore);
}


//hide rendered objects
void CComponentsContainer::kill()
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
}

//synchronize colors for forms
//This is usefull if the system colors are changed during runtime
//so you can ensure correct applied system colors in relevant objects with unchanged instances.
void CComponentsContainer::syncSysColors()
{
	col_body 	= COL_MENUCONTENT_PLUS_0;
	col_shadow 	= COL_MENUCONTENTDARK_PLUS_0;
	col_frame 	= COL_MENUCONTENT_PLUS_6;
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsInfoBox from CComponentsContainer
CComponentsInfoBox::CComponentsInfoBox()
{
	//CComponents, ComponentsContainer
	initVarContainer();
	
	//CComponentsInfoBox
	initVarInfobox();
	text 		= NULL;
	text_mode	= CTextBox::AUTO_WIDTH;
	font		= NULL;
	ibox_col_text	= COL_MENUCONTENT;
}

CComponentsInfoBox::CComponentsInfoBox(const int x_pos, const int y_pos, const int w, const int h,
				       const char* info_text, const int mode, Font* font_text,
				       bool has_shadow,
				       fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponents, ComponentsContainer
	initVarContainer();
	
	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	shadow		= has_shadow;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	
	//CComponentsInfoBox
	initVarInfobox();
	text 		= info_text;
	text_mode	= mode;
	font		= font_text;
	ibox_col_text	= color_text;
}

CComponentsInfoBox::~CComponentsInfoBox()
{
	hide();
	clearSavedScreen();
	delete textbox;
	delete box;
	delete pic;
	clear();
}

void CComponentsInfoBox::initVarInfobox()
{
	//CComponents, ComponentsContainer
	initVarContainer();
	
	//CComponentsInfoBox
	box		= NULL;
	textbox 	= NULL;
	pic 		= NULL;
	pic_name	= "";
	x_text		= x;
	x_offset	= 10;
	
}

void CComponentsInfoBox::setText(neutrino_locale_t locale_text, int mode, Font* font_text)
{
	text = g_Locale->getText(locale_text);
	text_mode = mode;
	font = font_text;
}

void CComponentsInfoBox::paintPicture()
{
	//init and set icon paint position
	if (pic == NULL)
		pic = new CComponentsPicture(x+fr_thickness+x_offset, y+fr_thickness/*+y_offset*/, "");
	pic->setXPos(x+fr_thickness+x_offset);
	pic->setYPos(y+fr_thickness);

	//define icon
	pic->setPicture(pic_name);

	//fit icon into infobox
	pic->setHeight(height-2*fr_thickness);
	pic->setColorBody(col_body);
	
	pic->paint();
}

void CComponentsInfoBox::paintText()
{
	if (box == NULL)
		box = new CBox();
	
	//define text x position
	x_text = x+fr_thickness+x_offset;
	if (pic->isPainted()){
		int pic_w = pic->getWidth();
		x_text += pic_w+x_offset;
	}	

	box->iX = x_text;
	box->iY = y+fr_thickness;

	//text width and height
	box->iWidth = width-2*fr_thickness-(x_text-x);
	box->iHeight = height-2*fr_thickness;

	//init textbox
	if (textbox == NULL) {
		textbox = new CTextBox(text, font, text_mode, box, col_body);
		textbox->setTextBorderWidth(0);
		textbox->enableBackgroundPaint(false);
	}

	//set properties
	textbox->setTextFont(font);
	textbox->movePosition(box->iX, box->iY);
	textbox->setTextColor(ibox_col_text);

	//set text
	string new_text = static_cast <string> (text);
	if (textbox->setText(&new_text))
		textbox->paint();
}

void CComponentsInfoBox::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
	paintPicture();
	if (text)
		paintText();
	text = NULL;
}

void CComponentsInfoBox::removeLineBreaks(std::string& str)
{
	std::string::size_type spos = str.find_first_of("\r\n");
	while (spos != std::string::npos) {
		str.replace(spos, 1, " ");
		spos = str.find_first_of("\r\n");
	}
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsShapeSquare from CComponentsContainer
CComponentsShapeSquare::CComponentsShapeSquare(const int x_pos, const int y_pos, const int w, const int h, bool has_shadow, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//ComponentsContainer
	initVarContainer();
	
	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	firstPaint	= true;
	v_fbdata.clear();
	bgMode 		= CC_BGMODE_PERMANENT;


}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsShapeCircle from CComponentsContainer
CComponentsShapeCircle::CComponentsShapeCircle(	int x_pos, int y_pos, int diam, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsContainer
	initVarContainer();

	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	//width = height	= d = diam;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;
	firstPaint	= true;
	v_fbdata.clear();
	bgMode 		= CC_BGMODE_PERMANENT;
	
	//CComponentsShapeCircle
	width = height	= d = diam;
	
	//CComponentsContainer
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
	int items_cnt = 0;
	
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
	
	items_cnt = sizeof(fbdata) / sizeof(fbdata[0]);
	
	paintFbItems(fbdata, items_cnt, do_save_bg);
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
//sub class CComponentsPIP from CComponentsContainer
CComponentsPIP::CComponentsPIP(	const int x_pos, const int y_pos, const int percent, bool has_shadow)
{
	//CComponents, CComponentsContainer
	initVarContainer();
	
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
	firstPaint	= true;
	v_fbdata.clear();
	bgMode 		= CC_BGMODE_PERMANENT;
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
	hideContainer(no_restore);
	videoDecoder->Pig(-1, -1, -1, -1);
}


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsPicture from CComponentsContainer
CComponentsPicture::CComponentsPicture(	const int x_pos, const int y_pos,
					const std::string& picture_name, const int alignment, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	init(x_pos, y_pos, picture_name, alignment, has_shadow, color_frame, color_background, color_shadow);

	maxWidth	= 0;
	maxHeight	= 0;
	picMode		= CC_PIC_ICON;

	initVarPicture();
}

CComponentsPicture::CComponentsPicture(	const int x_pos, const int y_pos, const int w_max, const int h_max,
					const std::string& picture_name, const int alignment, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	init(x_pos, y_pos, picture_name, alignment, has_shadow, color_frame, color_background, color_shadow);

	maxWidth	= w_max;
	maxHeight	= h_max;
	picMode		= CC_PIC_IMAGE;

	initVarPicture();
}

void CComponentsPicture::init(	int x_pos, int y_pos, const string& picture_name, const int alignment, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsContainer
	initVarContainer();
	
	//CComponentsPicture
	pic_name 	= picture_name;
	pic_align	= alignment;
	pic_offset	= 1;
	pic_paint	= true;
	pic_paintBg	= false;
	pic_painted	= false;
	do_paint	= false;
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
	firstPaint	= true;
	v_fbdata.clear();
	bgMode 		= CC_BGMODE_PERMANENT;
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

	if (picMode == CC_PIC_ICON)
		frameBuffer->getIconSize(pic_name.c_str(), &pic_width, &pic_height);
	else {
		g_PicViewer->getSize(pic_name.c_str(), &pic_width, &pic_height);
		if((pic_width > maxWidth) || (pic_height > maxHeight))
			g_PicViewer->rescaleImageDimensions(&pic_width, &pic_height, maxWidth, maxHeight);
	}
	
#ifdef DEBUG	
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
	
	if (do_paint){
		if (picMode == CC_PIC_ICON)
			pic_painted = frameBuffer->paintIcon(pic_name, pic_x, pic_y, 0, pic_offset, pic_paint, pic_paintBg, col_body);
		else
			pic_painted = g_PicViewer->DisplayImage(pic_name, pic_x, pic_y, pic_width, pic_height);
		do_paint = false;
	}
}

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsItemBox from CComponentsContainer
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
	//CComponents, CComponentsContainer
	initVarContainer();

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
	v_element_data.clear();
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
	return true;
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

void CComponentsItemBox::paintElement(size_t index, bool newElement)
{
	CComponentsPicture* pic = NULL;
	CBox* box = NULL;
	CTextBox* textbox = NULL;
	int pw = 0, ph = 0;
	switch (v_element_data[index].type) {
		case CC_ITEMBOX_ICON:
		case CC_ITEMBOX_PICTURE:
			pic = static_cast<CComponentsPicture*>(v_element_data[index].handler1);
			if ((newElement) || (pic == NULL)) {
				if (pic != NULL) {
					pic->hide();
					delete pic;
				}
				if ((v_element_data[index].type) == CC_ITEMBOX_PICTURE)
					pic = new CComponentsPicture(	v_element_data[index].x, v_element_data[index].y, v_element_data[index].width, 
									v_element_data[index].height, v_element_data[index].element);
				else
					pic = new CComponentsPicture(	v_element_data[index].x, v_element_data[index].y, v_element_data[index].element);
				v_element_data[index].handler1 = (void*)pic;
			}
			pic->getPictureSize(&pw, &ph);
			pic->setHeight(ph);
			pic->setWidth(pw);
			pic->setColorBody(col_body);
			pic->paint();
			break;
		case CC_ITEMBOX_TEXT:
			box = static_cast<CBox*>(v_element_data[index].handler1);
			if ((newElement) || (box == NULL)) {
				if (box != NULL) {
					delete box;
				}
				box = new CBox();
				v_element_data[index].handler1 = (void*)box;
			}
			box->iX      = v_element_data[index].x;
			box->iY      = v_element_data[index].y;
			box->iWidth  = v_element_data[index].width;
			box->iHeight = v_element_data[index].height;
			textbox = static_cast<CTextBox*>(v_element_data[index].handler2);
			if ((newElement) || (textbox == NULL)) {
				if (textbox != NULL) {
					textbox->hide();
					delete textbox;
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
//sub class CComponentsForm from CComponentsItemBox
CComponentsForm::CComponentsForm()
{
	//CComponentsContainer
	initVarContainer();
}

void CComponentsForm::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
}

