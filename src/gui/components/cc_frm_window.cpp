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
	
	initCCWItems();
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
	ccw_show_footer = true;
	ccw_show_header	= true;
	ccw_align_mode	= CTextBox::NO_AUTO_LINEBREAK;

	setShadowOnOff(true);
}

void CComponentsWindow::setWindowCaption(neutrino_locale_t locale_text, const int& align_mode)
{
	ccw_caption = g_Locale->getText(locale_text);
	ccw_align_mode = align_mode;
}

void CComponentsWindow::initHeader()
{
	if (ccw_head == NULL){
		ccw_head = new CComponentsHeader();
		initHeader();
		//add of header item happens initCCWItems()
	}

	//set header properties //TODO: assigned properties with internal header objekt have no effect!
	if (ccw_head){
		ccw_head->setWidth(width-2*fr_thickness);
// 		ccw_head->setPos(0, 0);
		ccw_head->setIcon(ccw_icon_name);
		ccw_head->setCaption(ccw_caption, ccw_align_mode);
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
		ccw_footer->setWidth(width-2*fr_thickness);
		ccw_footer->setShadowOnOff(shadow);
	}
}

void CComponentsWindow::addWindowItem(CComponentsItem* cc_Item)
{
	if (ccw_body)
		ccw_body->addCCItem(cc_Item);
}

void CComponentsWindow::initCCWItems()
{
#ifdef DEBUG_CC
	printf("[CComponentsWindow]   [%s - %d] init items...\n", __FUNCTION__, __LINE__);
#endif
	//add header if required
	if (ccw_show_header){
		initHeader();
	}else{
		if (ccw_head){
			removeCCItem(ccw_head);
			ccw_head = NULL;
		}
	}

	//add footer if required
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

