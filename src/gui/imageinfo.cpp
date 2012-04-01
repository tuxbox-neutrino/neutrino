/*
	Neutrino-GUI  -   DBoxII-Project


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

#include <gui/imageinfo.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>

#include <daemonc/remotecontrol.h>

#include <system/flashtool.h>
#include <video.h>

#include "svn_version.h"
#define SVN_REV "SVN Rev.:"
#define GIT_REV "GIT Build:"
extern cVideo * videoDecoder;

extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

CImageInfo::CImageInfo()
{
	Init();
}

static const neutrino_locale_t info_items[8] =
{
	LOCALE_IMAGEINFO_IMAGE,
	LOCALE_IMAGEINFO_DATE,
	LOCALE_IMAGEINFO_VERSION,
	LOCALE_IMAGEINFO_CREATOR,
	LOCALE_IMAGEINFO_HOMEPAGE,
	LOCALE_IMAGEINFO_DOKUMENTATION,
	LOCALE_IMAGEINFO_FORUM,
	LOCALE_IMAGEINFO_LICENSE
};

void CImageInfo::Init(void)
{
	frameBuffer = CFrameBuffer::getInstance();

	font_head   = SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME;;
	font_small  = SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL;
	font_info   = SNeutrinoSettings::FONT_TYPE_MENU;

	hheight     = g_Font[font_head]->getHeight();
	iheight     = g_Font[font_info]->getHeight();
	sheight     = g_Font[font_small]->getHeight();

	max_width = frameBuffer->getScreenWidth(true);
	max_height =  frameBuffer->getScreenHeight(true);

	width = frameBuffer->getScreenWidth() - 10;
	height = frameBuffer->getScreenHeight() - 10;
	x=getScreenStartX( width );
	y=getScreenStartY( height );
	
	// calculate max width of used LOCALES
	offset = 0;
	
	for (int i = 0; i < 8; i++) {
		int tmpoffset = g_Font[font_info]->getRenderWidth(g_Locale->getText (info_items[i]));
		if (tmpoffset > offset) {
			offset = tmpoffset;
		}
	}
#ifdef SVNVERSION
	int off_tmp = g_Font[font_info]->getRenderWidth(SVN_REV);
#else
	int off_tmp = g_Font[font_info]->getRenderWidth(GIT_REV);
#endif
	if(off_tmp > offset)
		offset  = off_tmp;

	offset = offset + 15;
}

CImageInfo::~CImageInfo()
{
	videoDecoder->Pig(-1, -1, -1, -1);
}

int CImageInfo::exec(CMenuTarget* parent, const std::string &)
{
	int res = menu_return::RETURN_REPAINT;
	if (parent)
		parent->hide();

	width = frameBuffer->getScreenWidth() - 10;
	height = frameBuffer->getScreenHeight() - 10;
	x=getScreenStartX( width );
	y=getScreenStartY( height );

	paint();

	//paint_pig( width-170, y, 215, 170);
	paint_pig (width - width/3 - 10, y + 10, width/3, height/3);
	frameBuffer->blit();

	neutrino_msg_t msg;

	while (1)
	{
		neutrino_msg_data_t data;
		uint64_t timeoutEnd = CRCInput::calcTimeoutEnd_MS(100);
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if(msg == CRCInput::RC_setup) {
			res = menu_return::RETURN_EXIT_ALL;
			break;
		}
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
			g_RCInput->postMsg (msg, 0);
			res = menu_return::RETURN_EXIT_ALL;
			break;
		}
		else if (msg <= CRCInput::RC_MaxRC)
		{
			break;
		}

		if ( msg >  CRCInput::RC_MaxRC && msg != CRCInput::RC_timeout)
		{
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}
		frameBuffer->blit();
	}

	hide();

	return res;
}

void CImageInfo::hide()
{
	//frameBuffer->paintBackgroundBoxRel(0,0, max_width,max_height);
	frameBuffer->paintBackground();
	videoDecoder->Pig(-1, -1, -1, -1);
}

void CImageInfo::paint_pig(int px, int py, int w, int h)
{
	//frameBuffer->paintBoxRel(px,py,w,h, COL_BACKGROUND);
	frameBuffer->paintBackgroundBoxRel(px,py,w,h);
	videoDecoder->Pig(px, py, w, h, frameBuffer->getScreenWidth(true), frameBuffer->getScreenHeight(true));
}

void CImageInfo::paintLine(int xpos, int font, const char* text)
{
	char buf[100];
	snprintf((char*) buf,sizeof(buf), "%s", text);
	//g_Font[font]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true);
	g_Font[font]->RenderString(xpos, ypos, width-10, buf, COL_INFOBAR, 0, true);
}

void CImageInfo::paint()
{
	const char * head_string;
	char imagedate[18] = "";
	int  xpos = x+10;

	ypos = y+5;

	head_string = g_Locale->getText(LOCALE_IMAGEINFO_HEAD);
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, head_string);

	//frameBuffer->paintBoxRel(0, 0, max_width, max_height, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintBoxRel(0, 0, max_width, max_height, COL_INFOBAR_PLUS_0);
	g_Font[font_head]->RenderString(xpos, ypos+ hheight+1, width, head_string, COL_MENUHEAD, 0, true);

	ypos += hheight;
	ypos += (iheight >>1);

	CConfigFile config('\t');
	config.loadConfig("/.version");

	const char * imagename = config.getString("imagename", "Neutrino-HD").c_str();
	const char * homepage  = config.getString("homepage",  "n/a").c_str();
	const char * creator   = config.getString("creator",   "n/a").c_str();
	const char * version   = config.getString("version",   "no version").c_str();
	const char * docs      = config.getString("docs",      "http://wiki.neutrino-hd.de").c_str();
	const char * forum     = config.getString("forum",     "http://forum.tuxbox.org").c_str();
#ifdef SVNVERSION
	const char * builddate     = config.getString("builddate",     SVNVERSION).c_str();
#else
	const char * builddate     = config.getString("builddate",     BUILT_DATE).c_str();
#endif
	static CFlashVersionInfo versionInfo(version);
	const char * releaseCycle = versionInfo.getReleaseCycle();
	snprintf((char*) imagedate,sizeof(imagedate), "%s  %s", versionInfo.getDate(), versionInfo.getTime());

	ypos += iheight;
	paintLine(xpos    , font_info, g_Locale->getText(LOCALE_IMAGEINFO_IMAGE));
	paintLine(xpos+offset, font_info, imagename);

	ypos += iheight;
	paintLine(xpos    , font_info, g_Locale->getText(LOCALE_IMAGEINFO_DATE));
	paintLine(xpos+offset, font_info, imagedate);

	ypos += iheight;
	paintLine(xpos    , font_info, g_Locale->getText(LOCALE_IMAGEINFO_VERSION));
	paintLine(xpos+offset, font_info, releaseCycle);

	ypos += iheight;
#ifdef SVNVERSION
	paintLine(xpos    , font_info, SVN_REV);
#else
	paintLine(xpos    , font_info, GIT_REV);
#endif
	paintLine(xpos+offset, font_info, builddate );
	
	ypos += iheight;
	paintLine(xpos    , font_info, g_Locale->getText(LOCALE_IMAGEINFO_CREATOR));
	paintLine(xpos+offset, font_info, creator);

	ypos += iheight+15;
	paintLine(xpos    , font_info, g_Locale->getText(LOCALE_IMAGEINFO_HOMEPAGE));
	paintLine(xpos+offset, font_info, homepage);

	ypos += iheight;
	paintLine(xpos    , font_info, g_Locale->getText(LOCALE_IMAGEINFO_DOKUMENTATION));
	paintLine(xpos+offset, font_info, docs);

	ypos += iheight;
	paintLine(xpos    , font_info, g_Locale->getText(LOCALE_IMAGEINFO_FORUM));
	paintLine(xpos+offset, font_info, forum);

	ypos += iheight+15;
	paintLine(xpos, font_info,g_Locale->getText(LOCALE_IMAGEINFO_LICENSE));
	paintLine(xpos+offset, font_small, "This program is free software; you can redistribute it and/or modify");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "it under the terms of the GNU General Public License as published by");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "the Free Software Foundation; either version 2 of the License, or");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "(at your option) any later version.");

	ypos+= sheight+10;
	paintLine(xpos+offset, font_small, "This program is distributed in the hope that it will be useful,");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "but WITHOUT ANY WARRANTY; without even the implied warranty of");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "See the GNU General Public License for more details.");

	ypos+= sheight+10;
	paintLine(xpos+offset, font_small, "You should have received a copy of the GNU General Public License");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "along with this program; if not, write to the Free Software");

	ypos+= sheight;
	paintLine(xpos+offset, font_small, "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.");
}
