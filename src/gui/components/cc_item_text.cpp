/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_item_text.h"
#include <driver/fontrenderer.h>
#include <sstream>
#include <fstream>
#include <errno.h>
#include <system/debug.h>

using namespace std;

//sub class CComponentsText from CComponentsItem
CComponentsText::CComponentsText(	CComponentsForm *parent,
					const int x_pos, const int y_pos, const int w, const int h,
					std::string text,
					const int mode,
					Font* font_text,
					const int& font_style,
					int shadow_mode,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarText(x_pos, y_pos, w, h, text, mode, font_text, font_style, parent, shadow_mode, color_text, color_frame, color_body, color_shadow);
}

CComponentsText::CComponentsText(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text,
					const int mode,
					Font* font_text,
					const int& font_style,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarText(x_pos, y_pos, w, h, text, mode, font_text, font_style, parent, shadow_mode, color_text, color_frame, color_body, color_shadow);
}

CComponentsText::~CComponentsText()
{
	//hide();
	clearCCText();
}


void CComponentsText::initVarText(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text,
					const int mode,
					Font* font_text,
					const int& font_style,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	cc_item_type.id 	= CC_ITEMTYPE_TEXT;
	cc_item_type.name	="cc_text_box";

	ct_font 	= font_text;
	ct_textbox	= NULL;
	ct_text 	= text;
	ct_old_text	= ct_text;
	ct_text_mode	= mode;
	ct_text_style 	= font_style;

	fr_thickness	= 0;
	x 		= x_old 	= x_pos;
	y 		= y_old 	= y_pos;
	width 		= width_old 	= w;
	height		= height_old 	= h;

	ct_text_Hborder	= 0;
	ct_text_Vborder	= 0;

	shadow		= shadow_mode;
	ct_col_text	= color_text;
	ct_old_col_text = ct_col_text;
	col_frame 	= color_frame;
	col_body 	= color_body;
	col_shadow	= color_shadow;

	ct_text_sent	= false;
	ct_paint_textbg = false;
	ct_force_text_paint = false;
	ct_utf8_encoded = true;

	initCCText();
	initParent(parent);
}


void CComponentsText::initCBox()
{
	x = max(0, x);
	y = max(0, y);

	int x_box = x + fr_thickness;
	int y_box = y + fr_thickness;

	if (cc_parent){
		ct_box.iX = cc_xr;
		ct_box.iY = cc_yr;
	}else{
		ct_box.iX = x_box;
		ct_box.iY = y_box;
	}

	ct_box.iWidth	= width - 2*fr_thickness;
	ct_box.iHeight	= height - 2*fr_thickness;
}


void CComponentsText::initCCText()
{
	//set default font, if is no font definied
	if (ct_font == NULL)
		ct_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];

	//define height from font size
	int h_tmp = ct_font->getHeight();
	height 	= max(height, h_tmp-2*fr_thickness);
	if (width == 0)
		width 	= max(width, CTextBox::getMaxLineWidth(ct_text, ct_font)-2*fr_thickness);

	//init textbox
	initCBox();

	if (ct_textbox == NULL)
		ct_textbox = new CTextBox();

	//set text properties
	ct_textbox->setTextFont(ct_font);
	ct_textbox->setTextMode(ct_text_mode);
	ct_textbox->setTextColor(ct_col_text);
	ct_textbox->enableUTF8(ct_utf8_encoded);

	//set text box properties
	ct_textbox->setWindowPos(&ct_box);
	ct_textbox->setWindowMaxDimensions(ct_box.iWidth, ct_box.iHeight);
	ct_textbox->setWindowMinDimensions(ct_box.iWidth, ct_box.iHeight);
	ct_textbox->setTextBorderWidth(ct_text_Hborder, ct_text_Vborder);
	bool enable_bg_paint = ct_paint_textbg && !cc_txt_save_screen;
	ct_textbox->enableBackgroundPaint(enable_bg_paint);
	ct_textbox->setBackGroundColor(col_body);
	ct_textbox->setBackGroundRadius(0/*(corner_type ? corner_rad-fr_thickness : 0), corner_type*/);
	bool enable_save_screen = cc_txt_save_screen && !ct_paint_textbg;
	ct_textbox->enableSaveScreen(enable_save_screen);

	//observe behavior of parent form if available
	bool force_text_paint = ct_force_text_paint;
#if 0 //FIXME.,
	if (cc_parent){
		//if any embedded text item was hided because of hided parent form,
		//we must ensure repaint of text, otherwise text item is not visible
		if (cc_parent->isPainted())
			force_text_paint = true;
	}
#endif
	//send text to CTextBox object, but force text paint text if force_text_paint option is enabled
	//this is managed by CTextBox object itself
	if (cc_allow_paint)
		ct_text_sent = ct_textbox->setText(&ct_text, ct_box.iWidth, force_text_paint);

	//set current text status, needed by textChanged()
	if (ct_text_sent){
		ct_old_text 	= ct_text;
		ct_old_col_text = ct_col_text;
	}

	//ensure clean font rendering on transparency background
	ct_textbox->setTextRenderModeFullBG(!paint_bg);

// 	dprintf(DEBUG_NORMAL, "[CComponentsText]   [%s - %d] ct_text = %s, x = %d , x_old = %d , y = %d , y_old = %d , \nct_box.iWidth = %d , ct_box.iHeight = %d , width = %d , height = %d, corner_rad = %d\n", __func__, __LINE__, ct_text.c_str(), x, x_old, y, y_old, ct_box.iWidth, ct_box.iHeight, width, height, corner_rad);
}

void CComponentsText::clearCCText()
{
	if (ct_textbox)
		delete ct_textbox;
	ct_textbox = NULL;
}

bool CComponentsText::setText(const std::string& stext, const int mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	if (ct_text != stext || ct_text_mode != mode || ct_font != font_text || ct_col_text != color_text || ct_text_style != style  ){
		if (ct_text != stext){
			ct_old_text = ct_text;
			ct_text = stext;
		}
		if (ct_text_mode != mode /*|| mode != ~CTextBox::AUTO_WIDTH*/)
			ct_text_mode = mode;
		if (font_text)
			ct_font = font_text;
		if (color_text != 0)
			setTextColor(color_text);
		if (style != FONT_STYLE_REGULAR)
			ct_text_style = style;

		dprintf(DEBUG_DEBUG, "[CComponentsText]   [%s - %d] ct_text: %s \n", __func__, __LINE__, ct_text.c_str());
		return true;
	}

	return false;
}

bool CComponentsText::setText(neutrino_locale_t locale_text, int mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	string stext = g_Locale->getText(locale_text);
	return setText(stext, mode, font_text, color_text, style);
}

bool CComponentsText::setText(const char* ctext, const int mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	return setText((string)ctext, mode, font_text, color_text, style);
}

bool CComponentsText::setText(const int digit, const int mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	string s_digit = iToString(digit);
	return setText(s_digit, mode, font_text, color_text, style);
}

string CComponentsText::getTextFromFile(const string& path_to_textfile)
{
	string file = path_to_textfile;
	string txt = "";

	ifstream in (file.c_str(), ios::in);
	if (!in){
		printf("[CComponentsText]    [%s - %d] error while open %s -> %s\n", __func__, __LINE__, file.c_str(), strerror(errno));
		return "";
	}
	string line;

	while(getline(in, line)){
		txt += line + '\n';
	}
	in.close();

	return txt;
}

//set text lines directly from a file, returns true on succsess
bool CComponentsText::setTextFromFile(const string& path_to_textfile, const int mode, Font* font_text, const fb_pixel_t& color_text, const int& style)
{
	string txt = getTextFromFile(path_to_textfile);
	
	if (txt.empty())
		return false;
	
	return setText(txt, mode, font_text, color_text, style);
}

void CComponentsText::paintText(const bool &do_save_bg)
{
	if (cc_parent){
		if(!cc_parent->OnAfterPaintBg.empty())
			cc_parent->OnAfterPaintBg.clear();
		//init slot to handle repaint of text if background was repainted
		cc_parent->OnAfterPaintBg.connect(sigc::bind(sigc::mem_fun(*this, &CComponentsText::forceTextPaint), true));
	}

	initCCText();

	if (!is_painted)
		paintInit(do_save_bg);

	if (ct_text_sent && cc_allow_paint){
		ct_textbox->paint();
		is_painted = true;
	}

	ct_text_sent = false;
}

void CComponentsText::paint(const bool &do_save_bg)
{
	OnBeforePaint();
	paintText(do_save_bg);
}

void CComponentsText::hide()
{
	if (ct_textbox)
		ct_textbox->hide();

	ct_old_text.clear();
	CCDraw::hide();
	ct_force_text_paint = true;
}

void CComponentsText::kill(const fb_pixel_t& bg_color, const int& corner_radius, const int& fblayer_type)
{
	if (ct_textbox)
		ct_textbox->hide();

	ct_old_text.clear();
	force_paint_bg = true;
	CCDraw::kill(bg_color, corner_radius, fblayer_type);
	ct_force_text_paint = true;
}

void CComponentsText::setXPos(const int& xpos)
{
	CCDraw::setXPos(xpos);
	initCBox();
}

void CComponentsText::setYPos(const int& ypos)
{
	CCDraw::setYPos(ypos);
	initCBox();
}

void CComponentsText::setHeight(const int& h)
{
	CCDraw::setHeight(h);
	initCBox();
}

void CComponentsText::setWidth(const int& w)
{
	CCDraw::setWidth(w);
	initCBox();
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


//helper, converts int to string
string CComponentsText::iToString(int int_val)
{
	ostringstream i_str;
	i_str << int_val;
	string i_string(i_str.str());
	return i_string;
}

//helper, get lines per textbox page
int CComponentsText::getTextLinesAutoHeight(const int& textMaxHeight, const int& textWidth, const int& mode)
{
	CBox box;
	box.iX      = 0;
	box.iY      = 0;
	box.iWidth  = textWidth;
	box.iHeight = textMaxHeight;

	CTextBox tb(ct_text.c_str(), ct_font, mode, &box);
	int ret = tb.getLinesPerPage();

	return ret;
}

void CComponentsText::setTextColor(const fb_pixel_t& color_text)
{
	if (ct_col_text != color_text){
		//ct_textbox->clearScreenBuffer();

		ct_col_text = color_text;
		if (ct_textbox)
			ct_textbox->setTextColor(ct_col_text);
	}
}

void CComponentsText::setTextFont(Font* font_text)
{
	ct_font = font_text;
}

void CComponentsText::setTextMode(const int mode)
{
	ct_text_mode = mode;
	initCCText();
}

void CComponentsText::setTextBorderWidth(const int Hborder, const int Vborder)
{
	ct_text_Hborder = Hborder;
	ct_text_Vborder = Vborder;
}

void CComponentsText::doPaintTextBoxBg(bool do_paintbox_bg)
{
	ct_paint_textbg = do_paintbox_bg;
}

string  CComponentsText::getText()
{
	return ct_text;
}

Font* CComponentsText::getFont()
{
	return ct_font;
}

void CComponentsText::forceTextPaint(bool force_text_paint)
{
	ct_force_text_paint = force_text_paint;
}

CTextBox* CComponentsText::getCTextBoxObject()
{
	return ct_textbox;
}

void CComponentsText::enableUTF8(bool enable)
{
	ct_utf8_encoded = enable;
}

void CComponentsText::disableUTF8(bool enable)
{
	enableUTF8(enable);
}

bool CComponentsText::clearSavedScreen()
{
	bool ret0 = CCDraw::clearSavedScreen();
	bool ret1 = false;
#if 0
	if (ct_textbox)
		ret1 = ct_textbox->clearScreenBuffer();
#endif
	return max<bool>(ret0, ret1);
}
#if 0
bool CComponentsText::enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_color)
{
	if (CCDraw::enableColBodyGradient(enable_mode, sec_color)){
		if (ct_textbox)
			ct_textbox->clearScreenBuffer();
	}
	return false;
}
#endif

