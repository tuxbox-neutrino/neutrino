/*
	$Id: progresswindow.cpp,v 1.16 2007/02/25 21:32:58 guenther Exp $

	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009-2013 Stefan Seyfried

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "progresswindow.h"

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <gui/color.h>

CProgressWindow::CProgressWindow()
{
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	int fw      = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
	width       = w_max (50*fw, 0);
	height      = h_max(hheight+5*mheight, 20);

	global_progress = local_progress = 101;
	statusText = "";
	globalstatusX = 0;
	globalstatusY = 0;
	localstatusY = 0;
	statusTextY = 0;

	x = frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - width ) >> 1 );
	y = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - height) >>1 );

	caption = NONEXISTANT_LOCALE;
}

void CProgressWindow::setTitle(const neutrino_locale_t title)
{
	caption = title;

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,NULL,-1,g_Locale->getText(caption)); // set global text in VFD
#endif // VFD_UPDATE
}


void CProgressWindow::showGlobalStatus(const unsigned int prog)
{
	if (global_progress == prog)
		return;

	global_progress = prog;

	int pos = x + 10;

	if(global_progress != 0)
	{
		if (global_progress > 100)
			global_progress = 100;

		pos += (width - 20) * global_progress / 100;
		//vordergrund
		frameBuffer->paintBox(x+10, globalstatusY,pos, globalstatusY+10, COL_MENUCONTENT_PLUS_7);
	}
	//hintergrund
	frameBuffer->paintBox(pos, globalstatusY, x+width-10, globalstatusY+10, COL_MENUCONTENT_PLUS_2);
	frameBuffer->blit();
#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(-1,NULL,global_progress);
#endif // VFD_UPDATE
}

void CProgressWindow::showLocalStatus(const unsigned int prog)
{
	if (local_progress == prog)
		return;

	local_progress = prog;

	int pos = x + 10;

	if (local_progress != 0)
	{
		if (local_progress > 100)
			local_progress = 100;

		pos += (width - 20) * local_progress / 100;
		//vordergrund
		frameBuffer->paintBox(x+10, localstatusY,pos, localstatusY+10, COL_MENUCONTENT_PLUS_7);
	}
	//hintergrund
	frameBuffer->paintBox(pos, localstatusY, x+width-10, localstatusY+10, COL_MENUCONTENT_PLUS_2);
	frameBuffer->blit();

#ifdef VFD_UPDATE
	CVFD::getInstance()->showProgressBar2(local_progress);
#else
	CVFD::getInstance()->showPercentOver(local_progress);
#endif // VFD_UPDATE
}

void CProgressWindow::showStatusMessageUTF(const std::string & text)
{
	statusText = text;
	frameBuffer->paintBox(x, statusTextY-mheight, x+width, statusTextY,  COL_MENUCONTENT_PLUS_0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+10, statusTextY, width-20, text, COL_MENUCONTENT_TEXT, 0, true); // UTF-8
	frameBuffer->blit();

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
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
	frameBuffer->blit();
}

void CProgressWindow::paint()
{
	int ypos=y;
	frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	if (caption != NONEXISTANT_LOCALE)
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+10, ypos+ hheight, width - 10, g_Locale->getText(caption), COL_MENUHEAD_TEXT, 0, true); // UTF-8
	frameBuffer->paintBoxRel(x, ypos+ hheight, width, height- hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	ypos+= hheight + (mheight >>1);
	statusTextY = ypos+mheight;
	showStatusMessageUTF(statusText);

	ypos+= mheight;
	localstatusY = ypos+ mheight-20;
	showLocalStatus(0);
	ypos+= mheight+10;

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width- 10, g_Locale->getText(LOCALE_FLASHUPDATE_GLOBALPROGRESS), COL_MENUCONTENT_TEXT, 0, true); // UTF-8
	ypos+= mheight;

	globalstatusY = ypos+ mheight-20;
	//ypos+= mheight >>1;
	showGlobalStatus(global_progress);
}

int CProgressWindow::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	if(parent)
	{
		parent->hide();
	}
	paint();
	frameBuffer->blit();

	return menu_return::RETURN_REPAINT;
}
