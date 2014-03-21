/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014 Thilo Graf 'dbt'
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

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_frm_window.h"
#include <system/debug.h>
using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsWindow inherit from CComponentsForm
CComponentsWindow::CComponentsWindow(CComponentsForm *parent)
{
	initVarWindow(0, 0, 800, 600, "", "", parent);
}

CComponentsWindow::CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					neutrino_locale_t locale_caption,
					const string& iconname,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	string s_caption = locale_caption != NONEXISTANT_LOCALE ? g_Locale->getText(locale_caption) : "";
	initVarWindow(x_pos, y_pos, w, h, s_caption, iconname, parent, has_shadow, color_frame, color_body, color_shadow);
}

CComponentsWindow::CComponentsWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const string& caption,
					const string& iconname,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	initVarWindow(x_pos, y_pos, w, h, caption, iconname, parent, has_shadow, color_frame, color_body, color_shadow);
}

CComponentsWindowMax::CComponentsWindowMax(	const string& caption,
						const string& iconname,
						CComponentsForm *parent,
						bool has_shadow,
						fb_pixel_t color_frame,
						fb_pixel_t color_body,
						fb_pixel_t color_shadow)
						:CComponentsWindow(0, 0, 0, 0, caption,
						iconname, parent, has_shadow, color_frame, color_body, color_shadow){};

CComponentsWindowMax::CComponentsWindowMax(	neutrino_locale_t locale_caption,
						const string& iconname,
						CComponentsForm *parent,
						bool has_shadow,
						fb_pixel_t color_frame,
						fb_pixel_t color_body,
						fb_pixel_t color_shadow)
						:CComponentsWindow(0, 0, 0, 0,
						locale_caption != NONEXISTANT_LOCALE ? g_Locale->getText(locale_caption) : "",
						iconname, parent, has_shadow, color_frame, color_body, color_shadow){};

void CComponentsWindow::initVarWindow(	const int& x_pos, const int& y_pos, const int& w, const int& h,
					const string& caption,
					const string& iconname,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	//CComponentsForm
	cc_item_type 	= CC_ITEMTYPE_FRM_WINDOW;

	//using current screen settings for default dimensions,
	//do use full screen (from osd-settings) if default values for width/height = 0
	x = x_pos;
	y = y_pos;
	width = w;
	height = h;
	initWindowSize();
	initWindowPos();

	ccw_caption 	= caption;
	ccw_icon_name	= iconname;

	dprintf(DEBUG_DEBUG, "[CComponentsWindow]   [%s - %d] icon name = %s\n", __func__, __LINE__, ccw_icon_name.c_str());

	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	ccw_head 	= NULL;
	ccw_body	= NULL;
	ccw_footer	= NULL;

	ccw_buttons	= 0; //no header buttons
	ccw_show_footer = true;
	ccw_show_header	= true;
	ccw_align_mode	= CTextBox::NO_AUTO_LINEBREAK;

	initCCWItems();
	initParent(parent);
}

void CComponentsWindow::initWindowSize()
{
	if (cc_parent)
		return;

	if (width == 0)
		width = frameBuffer->getScreenWidth();
	if (height == 0)
		height = frameBuffer->getScreenHeight();
}

void CComponentsWindow::initWindowPos()
{
	if (cc_parent)
		return;

	if (x == 0)
		x = frameBuffer->getScreenX();
	if (y == 0)
		y = frameBuffer->getScreenY();
}

void CComponentsWindow::setWindowCaption(neutrino_locale_t locale_text, const int& align_mode)
{
	ccw_caption = g_Locale->getText(locale_text);
	ccw_align_mode = align_mode;
}

void CComponentsWindow::initHeader()
{
	if (ccw_head == NULL)
		ccw_head = new CComponentsHeader();
	//add of header item happens initCCWItems()
	//set header properties //TODO: assigned properties with internal header objekt have no effect!
	if (ccw_head){
		ccw_head->setWidth(width-2*fr_thickness);
		ccw_head->setPos(0, 0);
		ccw_head->setIcon(ccw_icon_name);
		ccw_head->setCaption(ccw_caption, ccw_align_mode);
		ccw_head->setContextButton(ccw_buttons);
		ccw_head->setCorner(corner_rad, CORNER_TOP);
	}
}

void CComponentsWindow::initFooter()
{
	if (ccw_footer== NULL)
		ccw_footer= new CComponentsFooter();
	//add of footer item happens initCCWItems()
	//set footer properties
	if (ccw_footer){
		ccw_footer->setPos(0, CC_APPEND);
		ccw_footer->setWidth(width-2*fr_thickness);
		ccw_footer->setShadowOnOff(shadow);
		ccw_footer->setCorner(corner_rad, CORNER_BOTTOM);
	}
}

void CComponentsWindow::initBody()
{
	if (ccw_body== NULL)
		ccw_body = new CComponentsForm();
	//add of body item happens initCCWItems()
	//set body properties
	if (ccw_body){
		ccw_body->setCornerType(0);
		int fh = 0;
		int hh = 0;
		if (ccw_footer)
			fh = ccw_footer->getHeight();
		if (ccw_head)
			hh = ccw_head->getHeight();
		int h_body = height - hh - fh - 2*fr_thickness;
		ccw_body->setDimensionsAll(0, CC_APPEND, width-2*fr_thickness, h_body);
		ccw_body->doPaintBg(false);
	}
}

void CComponentsWindow::initCCWItems()
{
	dprintf(DEBUG_DEBUG, "[CComponentsWindow]   [%s - %d] init items...\n", __func__, __LINE__);

	//add/remove header if required
	if (ccw_show_header){
		initHeader();
	}else{
		if (ccw_head){
			removeCCItem(ccw_head);
			ccw_head = NULL;
		}
	}

	//add/remove footer if required
	if (ccw_show_footer){
		initFooter();
	}else{
		if (ccw_footer){
			removeCCItem(ccw_footer);
			ccw_footer = NULL;
		}
	}

	initBody();

	//add header, body and footer items only one time
	if (ccw_head)
		if (!ccw_head->isAdded())
			addCCItem(ccw_head);
	if (!ccw_body->isAdded())
		addCCItem(ccw_body);
	if (ccw_footer)
		if (!ccw_footer->isAdded())
			addCCItem(ccw_footer);
}

void CComponentsWindow::addWindowItem(CComponentsItem* cc_Item)
{
	if (ccw_body)
		ccw_body->addCCItem(cc_Item);
}


void CComponentsWindow::paint(bool do_save_bg)
{
	//prepare items before paint
	initCCWItems();

	//paint form contents
	paintForm(do_save_bg);
}
