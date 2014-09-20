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
					bool has_shadow,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarText(x_pos, y_pos, w, h, text, mode, font_text, parent, has_shadow, color_text, color_frame, color_body, color_shadow);
}

CComponentsText::CComponentsText(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text,
					const int mode,
					Font* font_text,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	initVarText(x_pos, y_pos, w, h, text, mode, font_text, parent, has_shadow, color_text, color_frame, color_body, color_shadow);
}

CComponentsText::~CComponentsText()
{
	hide();
	clearCCText();
}


void CComponentsText::initVarText(	const int x_pos, const int y_pos, const int w, const int h,
					std::string text,
					const int mode,
					Font* font_text,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_text, fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)
{
	cc_item_type 	= CC_ITEMBOX_TEXT;
	ct_font 	= font_text;
	ct_textbox	= NULL;
	ct_text 	= text;
	ct_old_text	= ct_text;
	ct_text_mode	= mode;

	iX = x 		= x_pos;
	iY = y 		= y_pos;
	iWidth=width 	= w;
	iHeight=height 	= h;

	/* we need a minimal borderwith of 1px because the edge-smoothing
	(or fontrenderer?) otherwise will paint single pixels outside the
	defined area. e.g. 'j' is leaving such residues */
	ct_text_Hborder	= 1;
	ct_text_Vborder	= 0;

	shadow		= has_shadow;
	ct_col_text	= color_text;
	ct_old_col_text = 0;
	col_frame 	= color_frame;
	col_body 	= color_body;
	col_shadow	= color_shadow;
	
	ct_text_sent	= false;
	ct_paint_textbg = false;
	ct_force_text_paint = false;

	initCCText();
	initParent(parent);
}


void CComponentsText::initCCText()
{
	//set default font, if is no font definied
	if (ct_font == NULL)
		ct_font = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];

	//define height from font size
	height 	= max(height, ct_font->getHeight());

	//init CBox dimensions
	iWidth 	= width-2*fr_thickness;
	iHeight	= height-2*fr_thickness;
	iX = x + fr_thickness;
	iY = y + fr_thickness;

	//using of real x/y values to paint textbox if this text object is bound in a parent form
	if (cc_parent){
		int th_parent_fr = cc_parent->getFrameThickness();
		iX = cc_xr + (x <= th_parent_fr ? th_parent_fr : 0);
		iY = cc_yr + (y <= th_parent_fr ? th_parent_fr : 0);
	}

	//init textbox
	if (ct_textbox == NULL)
		ct_textbox = new CTextBox();

	//set text box properties
	ct_textbox->setTextFont(ct_font);
	ct_textbox->setTextMode(ct_text_mode);
	ct_textbox->setWindowPos(this);
	ct_textbox->setTextBorderWidth(ct_text_Hborder, ct_text_Vborder);
	ct_textbox->enableBackgroundPaint(ct_paint_textbg);
	ct_textbox->setBackGroundColor(col_body);
	ct_textbox->setBackGroundRadius(corner_rad-fr_thickness, corner_type);
	ct_textbox->setTextColor(ct_col_text);
	ct_textbox->setWindowMaxDimensions(iWidth, iHeight);
	ct_textbox->setWindowMinDimensions(iWidth, iHeight);

	//observe behavior of parent form if available
	bool force_text_paint = ct_force_text_paint;
	if (cc_parent){
		//if any embedded text item was hided because of hided parent form,
		//we must ensure repaint of text, otherwise text item is not visible
		if (cc_parent->isPainted())
			force_text_paint = true;
	}

	//send text to CTextBox object, but force text paint text if force_text_paint option is enabled
	//this is managed by CTextBox object itself
	ct_text_sent = ct_textbox->setText(&ct_text, this->iWidth, force_text_paint);
	
	//set current text status, needed by textChanged()
	if (ct_text_sent){
		ct_old_text 	= ct_text;
		ct_old_col_text = ct_col_text;
	}

	//ensure clean font rendering on transparency background
	ct_textbox->setTextRenderModeFullBG(!paint_bg);

	dprintf(DEBUG_DEBUG, "[CComponentsText]   [%s - %d] init text: %s [x %d, y %d, w %d, h %d]\n", __func__, __LINE__, ct_text.c_str(), this->iX, this->iY, this->iWidth, this->iHeight);
}

void CComponentsText::clearCCText()
{
	if (ct_textbox)
		delete ct_textbox;
	ct_textbox = NULL;
}

void CComponentsText::setText(const std::string& stext, const int mode, Font* font_text, const fb_pixel_t& color_text)
{
	ct_old_text = ct_text;
	ct_text = stext;
	ct_text_mode = mode;
	ct_font = font_text;
	if (color_text != 0)
		ct_col_text = color_text;

	dprintf(DEBUG_DEBUG, "[CComponentsText]   [%s - %d] ct_text: %s \n", __func__, __LINE__, ct_text.c_str());
}

void CComponentsText::setText(neutrino_locale_t locale_text, int mode, Font* font_text, const fb_pixel_t& color_text)
{
	string stext = g_Locale->getText(locale_text);
	setText(stext, mode, font_text, color_text);
}

void CComponentsText::setText(const char* ctext, const int mode, Font* font_text, const fb_pixel_t& color_text)
{
 	setText((string)ctext, mode, font_text, color_text);
}

void CComponentsText::setText(const int digit, const int mode, Font* font_text, const fb_pixel_t& color_text)
{
	string s_digit = iToString(digit);
	setText(s_digit, mode, font_text, color_text);
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
bool CComponentsText::setTextFromFile(const string& path_to_textfile, const int mode, Font* font_text, const fb_pixel_t& color_text)
{
	string txt = getTextFromFile(path_to_textfile);
	
	if (txt.empty())
		return false;
	
	setText(txt, mode, font_text, color_text);
	
	return true;
}

void CComponentsText::paintText(bool do_save_bg)
{
	paintInit(do_save_bg);
	initCCText();
	if (ct_text_sent && cc_allow_paint)
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
	ct_old_text = "";
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
