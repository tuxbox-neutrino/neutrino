/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implementation of CComponent Window class.
	Copyright (C) 2014-2017 Thilo Graf 'dbt'

	Copyright (C) 2009-2013 Stefan Seyfried

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

#include "progresswindow.h"

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/display.h>
using namespace sigc;
using namespace std;

CProgressWindow::CProgressWindow(CComponentsForm *parent,
				 const int &dx,
				 const int &dy,
				 signal<void, size_t, size_t, string> *statusSignal,
				 signal<void, size_t, size_t, string> *localSignal,
				 signal<void, size_t, size_t, string> *globalSignal)
				: CComponentsWindow(0, 0, dx, dy, string(), NEUTRINO_ICON_INFO, parent, CC_SHADOW_ON)
{
	Init(statusSignal, localSignal, globalSignal);
}

CProgressWindow::CProgressWindow(const neutrino_locale_t title,
				 const int &dx,
				 const int &dy,
				 signal<void, size_t, size_t, string> *statusSignal,
				 signal<void, size_t, size_t, string> *localSignal,
				 signal<void, size_t, size_t, string> *globalSignal)
				: CComponentsWindow(0, 0, dx, dy, g_Locale->getText(title), NEUTRINO_ICON_INFO, NULL, CC_SHADOW_ON)
{
	Init(statusSignal, localSignal, globalSignal);
}

CProgressWindow::CProgressWindow(const std::string &title,
				 const int &dx,
				 const int &dy,
				 signal<void, size_t, size_t, string> *statusSignal,
				 signal<void, size_t, size_t, string> *localSignal,
				 signal<void, size_t, size_t, string> *globalSignal)
				: CComponentsWindow(0, 0, dx, dy, title, NEUTRINO_ICON_INFO, NULL, CC_SHADOW_ON)
{
	Init(statusSignal, localSignal, globalSignal);
}

void CProgressWindow::Init(	signal<void, size_t, size_t, string> *statusSignal,
				signal<void, size_t, size_t, string> *localSignal,
				signal<void, size_t, size_t, string> *globalSignal)
{
	if (statusSignal)
		*statusSignal->connect(mem_fun(*this, &CProgressWindow::showStatus));
	if (localSignal)
		*localSignal->connect(mem_fun(*this, &CProgressWindow::showLocalStatus));
	if (globalSignal)
		*globalSignal->connect(mem_fun(*this, &CProgressWindow::showGlobalStatus));

	global_progress = local_progress = 0;

	showFooter(false);

	//create status text object
	status_txt = new CComponentsLabel();
	status_txt->setDimensionsAll(OFFSET_INNER_MID, OFFSET_INNER_MID, width-2*OFFSET_INNER_MID, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
	status_txt->setColorBody(col_body);
	status_txt->doPaintTextBoxBg(true);
	status_txt->doPaintBg(false);
	addWindowItem(status_txt);

	cur_statusText = string();

	//create local_bar object
	local_bar = getProgressItem();

	//create global_bar object
	global_bar = getProgressItem();

	//set window height
	h_height = ccw_head->getHeight();
	ccw_body->setHeight(ccw_body->back()->getYPos()+ ccw_body->back()->getHeight()+ OFFSET_INNER_MID);
	height = max(height, ccw_body->getHeight() + h_height);

	//set position on screen
	setCenterPos();
}

CProgressBar* CProgressWindow::getProgressItem()
{
	CProgressBar *pBar = new CProgressBar();
	pBar->allowPaint(false);
	int y_tmp = 0;
	for(size_t i = ccw_body->size()-1; i< ccw_body->size(); i++){
		y_tmp += ccw_body->getCCItem(i)->getYPos() + ccw_body->getCCItem(i)->getHeight();
		y_tmp += OFFSET_INNER_MID;
	}
	pBar->setDimensionsAll(OFFSET_INNER_MID, y_tmp, width-2*OFFSET_INNER_MID, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight());
	pBar->setColorBody(col_body);
	pBar->setActiveColor(COL_PROGRESSBAR_ACTIVE_PLUS_0);
	pBar->setFrameThickness(FRAME_WIDTH_MIN);
	pBar->setColorFrame(COL_PROGRESSBAR_ACTIVE_PLUS_0);
	pBar->setType(CProgressBar::PB_TIMESCALE);
	addWindowItem(pBar);

	return pBar;
}

void CProgressWindow::initStatus(const unsigned int prog, const unsigned int max, const string &statusText, CProgressBar *pBar)
{
	pBar->allowPaint(true);
	pBar->setValues(prog, (int)max);
	if (!statusText.empty() && (cur_statusText != statusText)){
		showStatusMessageUTF(statusText);
		cur_statusText = statusText;
	}
	pBar->paint(false);
	frameBuffer->blit();
}

void CProgressWindow::showStatus(const unsigned int prog, const unsigned int max, const string &statusText)
{
	showLocalStatus(prog, max, statusText);
}

void CProgressWindow::showGlobalStatus(const unsigned int prog, const unsigned int max, const string &statusText)
{
	if (!local_bar->isPainted())
		showLocalStatus(0, 0, statusText); // ensure first paint of local bar on painted global bar at same time
	if (global_progress == prog && global_bar->isPainted())
		return;
	global_progress = prog;
	initStatus(prog, max, statusText, global_bar);

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,NULL,global_progress);
#endif // VFD_UPDATE
}

void CProgressWindow::showLocalStatus(const unsigned int prog, const unsigned int max, const string &statusText)
{
	if (local_progress == prog && local_bar->isPainted())
		return;
	local_progress = prog;
	initStatus(prog, max, statusText, local_bar);

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(local_progress);
#else
	CVFD::getInstance()->showPercentOver((uint8_t)local_progress);
#endif // VFD_UPDATE
}

void CProgressWindow::showStatusMessageUTF(const string & text)
{
	string txt = text;
	int w_txt = status_txt->getWidth();
	int h_txt = status_txt->getHeight();
	status_txt->setText(txt, CTextBox::CENTER, *CNeutrinoFonts::getInstance()->getDynFont(w_txt, h_txt, txt, CNeutrinoFonts::FONT_STYLE_BOLD), COL_MENUCONTENT_TEXT);

	status_txt->paint(false);

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,text.c_str()); // set local text in VFD
#endif // VFD_UPDATE
}


unsigned int CProgressWindow::getGlobalStatus(void)
{
	return global_progress;
}

void CProgressWindow::hide()
{
	CComponentsWindow::hide();
}

int CProgressWindow::exec(CMenuTarget* parent, const string & /*actionKey*/)
{
	if(parent)
	{
		parent->hide();
	}
	paint();

	return menu_return::RETURN_REPAINT;
}

void CProgressWindow::paint(const bool &do_save_bg)
{
	fitItems();
	CComponentsWindow::paint(do_save_bg);
}

void CProgressWindow::setTitle(const neutrino_locale_t title)
{
	setWindowCaption(title);

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,NULL,-1,g_Locale->getText(ccw_caption)); // set global text in VFD
#endif // VFD_UPDATE
}

void CProgressWindow::setTitle(const string & title)
{
	setWindowCaption(title);

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,NULL,-1,g_Locale->getText(ccw_caption)); // set global text in VFD
#endif // VFD_UPDATE
}

//if header is disabled we need new position for body items
void CProgressWindow::fitItems()
{
	if (ccw_show_header)
		return;

	for(size_t i=0; i<ccw_body->size() ;i++){
		int y_item = ccw_body->getCCItem(i)->getYPos() + h_height - OFFSET_INNER_MID;
		ccw_body->getCCItem(i)->setYPos(y_item);
	}
}