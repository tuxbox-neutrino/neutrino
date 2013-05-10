/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_misc.h"

using namespace std;

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
extern CPictureViewer * g_PicViewer;
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
