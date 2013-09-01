/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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


#ifndef __color__
#define __color__

#include <driver/framebuffer.h>

#define COL_MAXFREE			254-8*12 - 1
#define COL_NEUTRINO_TEXT		254-8*12	// 24 values
#define COL_COLORED_EVENTS_CHANNELLIST	254-8*9
#define COL_COLORED_EVENTS_INFOBAR	254-8*8
#define COL_INFOBAR_SHADOW		254-8*7
#define COL_INFOBAR			254-8*6
#define COL_MENUHEAD			254-8*5
#define COL_MENUCONTENT			254-8*4
#define COL_MENUCONTENTDARK		254-8*3
#define COL_MENUCONTENTSELECTED		254-8*2
#define COL_MENUCONTENTINACTIVE		254-8*1

#define COL_BACKGROUND 			255

#ifdef FB_USE_PALETTE
#define COL_INFOBAR_SHADOW_PLUS_0       (COL_INFOBAR_SHADOW + 0)
#define COL_INFOBAR_SHADOW_PLUS_1       (COL_INFOBAR_SHADOW + 1)
#define COL_INFOBAR_PLUS_0              (COL_INFOBAR + 0)
#define COL_INFOBAR_PLUS_1              (COL_INFOBAR + 1)
#define COL_INFOBAR_PLUS_3              (COL_INFOBAR + 3)
#define COL_INFOBAR_PLUS_7              (COL_INFOBAR + 7)
#define COL_MENUHEAD_PLUS_0             (COL_MENUHEAD + 0)
#define COL_MENUCONTENT_PLUS_0          (COL_MENUCONTENT + 0)
#define COL_MENUCONTENT_PLUS_1          (COL_MENUCONTENT + 1)
#define COL_MENUCONTENT_PLUS_2          (COL_MENUCONTENT + 2)
#define COL_MENUCONTENT_PLUS_3          (COL_MENUCONTENT + 3)
#define COL_MENUCONTENT_PLUS_4          (COL_MENUCONTENT + 4)
#define COL_MENUCONTENT_PLUS_5          (COL_MENUCONTENT + 5)
#define COL_MENUCONTENT_PLUS_6          (COL_MENUCONTENT + 6)
#define COL_MENUCONTENT_PLUS_7          (COL_MENUCONTENT + 7)
#define COL_MENUCONTENTDARK_PLUS_0      (COL_MENUCONTENTDARK + 0)
#define COL_MENUCONTENTDARK_PLUS_2      (COL_MENUCONTENTDARK + 2)
#define COL_MENUCONTENTSELECTED_PLUS_0  (COL_MENUCONTENTSELECTED + 0)
#define COL_MENUCONTENTSELECTED_PLUS_2  (COL_MENUCONTENTSELECTED + 2)
#define COL_MENUCONTENTINACTIVE_PLUS_0  (COL_MENUCONTENTINACTIVE + 0)
#define COL_BACKGROUND_PLUS_0           (COL_BACKGROUND + 0)
#else
#define COL_INFOBAR_SHADOW_PLUS_0       (CFrameBuffer::getInstance()->realcolor[(COL_INFOBAR_SHADOW + 0)])
#define COL_INFOBAR_SHADOW_PLUS_1       (CFrameBuffer::getInstance()->realcolor[(COL_INFOBAR_SHADOW + 1)])
#define COL_INFOBAR_PLUS_0              (CFrameBuffer::getInstance()->realcolor[(COL_INFOBAR + 0)])
#define COL_INFOBAR_PLUS_1              (CFrameBuffer::getInstance()->realcolor[(COL_INFOBAR + 1)])
#define COL_INFOBAR_PLUS_3              (CFrameBuffer::getInstance()->realcolor[(COL_INFOBAR + 3)])
#define COL_INFOBAR_PLUS_7              (CFrameBuffer::getInstance()->realcolor[(COL_INFOBAR + 7)])
#define COL_MENUHEAD_PLUS_0             (CFrameBuffer::getInstance()->realcolor[(COL_MENUHEAD + 0)])
#define COL_MENUCONTENT_PLUS_0          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 0)])
#define COL_MENUCONTENT_PLUS_1          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 1)])
#define COL_MENUCONTENT_PLUS_2          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 2)])
#define COL_MENUCONTENT_PLUS_3          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 3)])
#define COL_MENUCONTENT_PLUS_4          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 4)])
#define COL_MENUCONTENT_PLUS_5          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 5)])
#define COL_MENUCONTENT_PLUS_6          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 6)])
#define COL_MENUCONTENT_PLUS_7          (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENT + 7)])
#define COL_MENUCONTENTDARK_PLUS_0      (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENTDARK + 0)])
#define COL_MENUCONTENTDARK_PLUS_2      (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENTDARK + 2)])
#define COL_MENUCONTENTSELECTED_PLUS_0  (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENTSELECTED + 0)])
#define COL_MENUCONTENTSELECTED_PLUS_2  (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENTSELECTED + 2)])
#define COL_MENUCONTENTINACTIVE_PLUS_0  (CFrameBuffer::getInstance()->realcolor[(COL_MENUCONTENTINACTIVE + 0)])
#define COL_BACKGROUND_PLUS_0           (CFrameBuffer::getInstance()->realcolor[(COL_BACKGROUND + 0)])

// text colors
#define COL_COLORED_EVENTS_TEXT			(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 0)])
#define COL_INFOBAR_TEXT			(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 1)])
#define COL_INFOBAR_SHADOW_TEXT			(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 2)])
#define COL_MENUHEAD_TEXT			(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 3)])
#define COL_MENUCONTENT_TEXT			(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 4)])
#define COL_MENUCONTENT_TEXT_PLUS_1		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 5)])
#define COL_MENUCONTENT_TEXT_PLUS_2		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 6)])
#define COL_MENUCONTENT_TEXT_PLUS_3		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 7)])
#define COL_MENUCONTENTDARK_TEXT		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 8)])
#define COL_MENUCONTENTDARK_TEXT_PLUS_1		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 9)])
#define COL_MENUCONTENTDARK_TEXT_PLUS_2		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 10)])
#define COL_MENUCONTENTSELECTED_TEXT		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 11)])
#define COL_MENUCONTENTSELECTED_TEXT_PLUS_1	(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 12)])
#define COL_MENUCONTENTSELECTED_TEXT_PLUS_2	(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 13)])
#define COL_MENUCONTENTINACTIVE_TEXT		(CFrameBuffer::getInstance()->realcolor[(COL_NEUTRINO_TEXT + 14)])
#endif


int convertSetupColor2RGB(unsigned char r, unsigned char g, unsigned char b);
int convertSetupAlpha2Alpha(unsigned char alpha);

void fadeColor(unsigned char &r, unsigned char &g, unsigned char &b, int fade, bool protect=true);

typedef struct {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} RgbColor;

typedef struct {
	unsigned char h;
	unsigned char s;
	unsigned char v;
} HsvColor;

unsigned char getBrightnessRGB(fb_pixel_t color);

fb_pixel_t changeBrightnessRGBRel(fb_pixel_t color, int br);
fb_pixel_t changeBrightnessRGB(fb_pixel_t color, unsigned char br);
void changeBrightnessRGBRel2(RgbColor *rgb, int br);

void Hsv2Rgb(HsvColor *hsv, RgbColor *rgb);
void Rgb2Hsv(RgbColor *rgb, HsvColor *hsv);

#endif
