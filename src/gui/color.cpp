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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/color.h>
#include <stdio.h>


int convertSetupColor2RGB(const unsigned char r, const unsigned char g, const unsigned char b)
{
	unsigned char red =	(int)r * 255 / 100;
	unsigned char green =	(int)g * 255 / 100;
	unsigned char blue =	(int)b * 255 / 100;

	return (red << 16) | (green << 8) | blue;
}

int convertSetupAlpha2Alpha(unsigned char alpha)
{
	if(alpha == 0) return 0xFF;
	else if(alpha >= 100) return 0;
	int a = 100 - alpha;
	int ret = a * 0xFF / 100;
	return ret;
}

void recalcColor(unsigned char &orginal, int fade)
{
	if(fade==100)
	{
		return;
	}
	int color =  orginal * fade / 100;
	if(color>255)
		color=255;
	if(color<0)
		color=0;
	orginal = color;
}

void protectColor( unsigned char &r, unsigned char &g, unsigned char &b, bool protect )
{
	if (!protect)
		return;
	if ((r==0) && (g==0) && (b==0))
	{
		r=1;
		g=1;
		b=1;
	}
}

void fadeColor(unsigned char &r, unsigned char &g, unsigned char &b, int fade, bool protect)
{
	recalcColor(r, fade);
	recalcColor(g, fade);
	recalcColor(b, fade);
	protectColor(r,g,b, protect);
}
