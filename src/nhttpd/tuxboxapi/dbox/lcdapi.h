/*      
        nhttpd  -  DBoxII-Project

        Copyright (C) 2001/2002 Dirk Szymanski 'Dirch'
        Copyright (C) 2005 SnowHead

        $Id: lcdapi.h,v 1.1.2.1 2010/02/21 10:10:12 cvs Exp $

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
#ifndef __nhttpd_lcdapi_h__
#define __nhttpd_lcdapi_h__

// tuxbox
#if HAVE_LCD
#include <driver/lcdd.h>
#include <lcddisplay/lcddisplay.h>
#include <lcddisplay/fontrenderer.h>
#endif

class CLCDDisplay;
class LcdFontRenderClass;

//-------------------------------------------------------------------------
class CLCDAPI
{
#if HAVE_LCD
	CLCDDisplay			display;
	LcdFontRenderClass		*fontRenderer;
	LcdFont				*font;
#endif
	const char 			*style_name[3];
public:
	CLCDAPI();
	~CLCDAPI(void);
	void LockDisplay(int lock);
	void DrawText(int px, int py, int psize, int pcolor, int pfont, char *pmsg);
	void DrawLine(int x1, int y1, int x2, int y2, int col);
	void DrawRect(int x1, int y1, int x2, int y2, int coll, int colf);
	bool ShowPng(char *filename);
	void ShowRaw(int xpos, int ypos, int xsize, int ysize, char *screen);
	void Update(void);
	void Clear(void);
};

#endif /* __nhttpd_lcdapi_h__ */
