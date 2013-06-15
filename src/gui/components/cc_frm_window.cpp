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
#include "cc_frm.h"
#include <driver/screen_max.h>

using namespace std;

//-------------------------------------------------------------------------------------------------------
//sub class CComponentsWindow inherit from CComponentsForm
CComponentsWindow::CComponentsWindow()
{
	initVarWindow();

	initCCWItems();
}

CComponentsWindow::CComponentsWindow(const std::string& caption, const char* iconname)
{
	initVarWindow();
	
	ccw_caption 	= caption;
	ccw_icon_name	= iconname;
	
	initHeader();
	initBody();
}

CComponentsWindow::CComponentsWindow(neutrino_locale_t locale_caption, const char* iconname)
{
	initVarWindow();
	
	ccw_caption 	= g_Locale->getText(locale_caption);
	ccw_icon_name	= iconname;

	initCCWItems();
}

CComponentsWindow::CComponentsWindow(	const int x_pos, const int y_pos, const int w, const int h,
					neutrino_locale_t locale_caption,
					const char* iconname,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	initVarWindow();

	x		= x_pos;
	y		= y_pos;
	width		= w;
	height		= h;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	ccw_caption 	= g_Locale->getText(locale_caption);
	ccw_icon_name	= iconname;

	initCCWItems();
}

CComponentsWindow::CComponentsWindow(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& caption,
					const char* iconname,
					bool has_shadow,
					fb_pixel_t color_frame,
					fb_pixel_t color_body,
					fb_pixel_t color_shadow)
{
	initVarWindow();

	x		= x_pos;
	y		= y_pos;
	width		= w;
	height		= h;
	shadow		= has_shadow;
	col_frame	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	ccw_caption 	= caption;;
	ccw_icon_name	= iconname;

	initCCWItems();
}

CComponentsWindow::~CComponentsWindow()
{
#ifdef DEBUG_CC
	printf("[~CComponentsWindow]   [%s - %d] delete...\n", __FUNCTION__, __LINE__);
#endif
	cleanCCForm();
}

void CComponentsWindow::initVarWindow()
{
	//CComponentsForm
	initVarForm();
	cc_item_type 	= CC_ITEMTYPE_FRM_WINDOW;

	//using current screen settings for default dimensions
	width = frameBuffer->getScreenWidth();
	height = frameBuffer->getScreenHeight();
	x=getScreenStartX(width);
	y=getScreenStartY(height);

	ccw_head 	= NULL;
	ccw_body	= NULL;
	ccw_footer	= NULL;
	ccw_caption 	= "";
	ccw_icon_name	= NULL;
	ccw_buttons	= 0; //no header buttons
	
	setShadowOnOff(true);
}

void CComponentsWindow::setWindowCaption(neutrino_locale_t locale_text)
{
	ccw_caption = g_Locale->getText(locale_text);
}

void CComponentsWindow::initHeader()
{
	if (ccw_head == NULL){
		ccw_head = new CComponentsHeader();
		initHeader();
		//add of header item happens initCCWItems()
	}

	//set header properties
	if (ccw_head){
		ccw_head->setPos(0, 0);
		ccw_head->setWidth(width);
		ccw_head->setIcon(ccw_icon_name);
		ccw_head->setCaption(ccw_caption);
		ccw_head->initCCItems();
		ccw_head->setDefaultButtons(ccw_buttons);
	}
}

void CComponentsWindow::initBody()
{
	if (ccw_body== NULL){
		ccw_body = new CComponentsForm();
		initBody();
		//add of body item happens initCCWItems()
	}

	//set body properties
	if (ccw_body){
		ccw_body->setCornerType(0);
		int fh = ccw_footer->getHeight();
		int hh = ccw_head->getHeight();
		int h_body = height - hh - fh;
		ccw_body->setDimensionsAll(0, CC_APPEND, width, h_body);
		ccw_body->doPaintBg(false);
	}
}

void CComponentsWindow::initFooter()
{
	if (ccw_footer== NULL){

		ccw_footer= new CComponentsFooter();
		initFooter();
		//add of footer item happens initCCWItems()
	}

	//set footer properties
	if (ccw_footer){
		ccw_footer->setPos(0, CC_APPEND);
		ccw_footer->setWidth(width);
		ccw_footer->setShadowOnOff(shadow);
	}
}

void CComponentsWindow::addWindowItem(CComponentsItem* cc_Item)
{
	if (ccw_body)
		ccw_body->addCCItem(cc_Item);
}

int CComponentsWindow::getStartY()
{
	if (ccw_head)
		return ccw_head->getHeight();
	
	return 0;
}

void CComponentsWindow::initCCWItems()
{
#ifdef DEBUG_CC
	printf("[CComponentsWindow]   [%s - %d] init items...\n", __FUNCTION__, __LINE__);
#endif
	initHeader();
	initFooter();
	initBody();
	
	//add header, body and footer items only one time
	if (!isAdded(ccw_head))
		addCCItem(ccw_head);
	if (!isAdded(ccw_body))
		addCCItem(ccw_body);
	if (!isAdded(ccw_footer))
		addCCItem(ccw_footer);
}

void CComponentsWindow::paint(bool do_save_bg)
{
	//prepare items before paint
	initCCWItems();
	
	//paint form contents
	paintForm(do_save_bg);
}
