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
#include <math.h>

#include <driver/framebuffer.h>

#ifndef FLT_EPSILON
#define FLT_EPSILON 1E-5
#endif

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
	orginal = (unsigned char)color;
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

uint8_t getBrightnessRGB(fb_pixel_t color)
{
	RgbColor rgb;
	rgb.r  = (uint8_t)((color & 0x00FF0000) >> 16);
	rgb.g  = (uint8_t)((color & 0x0000FF00) >>  8);
	rgb.b  = (uint8_t) (color & 0x000000FF);

	return rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
}

fb_pixel_t changeBrightnessRGBRel(fb_pixel_t color, int br, bool transp)
{
	int br_ = (int)getBrightnessRGB(color);
	br_ += br;
	if (br_ < 0) br_ = 0;
	if (br_ > 255) br_ = 255;
	return changeBrightnessRGB(color, (uint8_t)br_, transp);
}

fb_pixel_t changeBrightnessRGB(fb_pixel_t color, uint8_t br, bool transp)
{
	HsvColor hsv;
	uint8_t tr = SysColor2Hsv(color, &hsv);
	hsv.v = (float)br / (float)255;
	if (!transp)
		tr = 0xFF;
	return Hsv2SysColor(&hsv, tr);
}

fb_pixel_t Hsv2SysColor(HsvColor *hsv, uint8_t tr)
{
	RgbColor rgb;
	Hsv2Rgb(hsv, &rgb);
	return (((tr    << 24) & 0xFF000000) |
		((rgb.r << 16) & 0x00FF0000) |
		((rgb.g <<  8) & 0x0000FF00) |
		((rgb.b      ) & 0x000000FF));
}

uint8_t SysColor2Hsv(fb_pixel_t color, HsvColor *hsv)
{
	uint8_t tr;
	RgbColor rgb;
	tr     = (uint8_t)((color & 0xFF000000) >> 24);
	rgb.r  = (uint8_t)((color & 0x00FF0000) >> 16);
	rgb.g  = (uint8_t)((color & 0x0000FF00) >>  8);
	rgb.b  = (uint8_t) (color & 0x000000FF);
	Rgb2Hsv(&rgb, hsv);
	return tr;
}

void Hsv2Rgb(HsvColor *hsv, RgbColor *rgb)
{
	float f_H = hsv->h;
	float f_S = hsv->s;
	float f_V = hsv->v;
	if (fabsf(f_S) < FLT_EPSILON) {
		rgb->r = (uint8_t)(f_V * 255);
		rgb->g = (uint8_t)(f_V * 255);
		rgb->b = (uint8_t)(f_V * 255);

	} else {
		float f_R;
		float f_G;
		float f_B;
		float hh = f_H;
		if (hh >= 360) hh = 0;
		hh /= 60;
		int i = (int)hh;
		float ff = hh - (float)i;
		float p = f_V * (1 - f_S);
		float q = f_V * (1 - (f_S * ff));
		float t = f_V * (1 - (f_S * (1 - ff)));

		switch (i) {
			case 0:
				f_R = f_V; f_G = t; f_B = p;
				break;
			case 1:
				f_R = q; f_G = f_V; f_B = p;
				break;
			case 2:
				f_R = p; f_G = f_V; f_B = t;
				break;
			case 3:
				f_R = p; f_G = q; f_B = f_V;
				break;
			case 4:
				f_R = t; f_G = p; f_B = f_V;
				break;
			case 5:
			default:
				f_R = f_V; f_G = p; f_B = q;
				break;
		}
		rgb->r = (uint8_t)(f_R * 255);
		rgb->g = (uint8_t)(f_G * 255);
		rgb->b = (uint8_t)(f_B * 255);
	}
}

void Rgb2Hsv(RgbColor *rgb, HsvColor *hsv)
{
	float f_R = (float)rgb->r / (float)255;
	float f_G = (float)rgb->g / (float)255;
	float f_B = (float)rgb->b / (float)255;

	float min = f_R < f_G ? (f_R < f_B ? f_R : f_B) : (f_G < f_B ? f_G : f_B);
	float max = f_R > f_G ? (f_R > f_B ? f_R : f_B) : (f_G > f_B ? f_G : f_B);
	float delta = max - min;

	float f_V = max;
	float f_H = 0;
	float f_S = 0;

	if (fabsf(delta) < FLT_EPSILON) { //gray
		f_S = 0;
		f_H = 0;
	} else {
		f_S = (delta / max);
		if (f_R >= max)
			f_H = (f_G - f_B) / delta;
		else if (f_G >= max)
			f_H = 2 + (f_B - f_R) / delta;
		else
			f_H = 4 + (f_R - f_G) / delta;

		f_H *= 60;
		if (f_H < 0)
			f_H += 360;
	}
	hsv->h = f_H;
	hsv->s = f_S;
	hsv->v = f_V;
}

void getItemColors(fb_pixel_t &t, fb_pixel_t &b, bool selected, bool marked, bool toggle_background, bool toggle_enlighten)
{
	if (selected && marked)
	{
		t = COL_MENUCONTENTSELECTED_TEXT;
		b = COL_MENUCONTENTSELECTED_PLUS_0;
		return;
	}

	if (selected)
	{
		t = COL_MENUCONTENTSELECTED_TEXT;
		b = COL_MENUCONTENTSELECTED_PLUS_0;
		return;
	}

	if (marked)
	{
		t = COL_MENUCONTENT_TEXT_PLUS_1;
		b = COL_MENUCONTENT_PLUS_1;
		return;
	}

	// default
	t = toggle_background ? (toggle_enlighten ? COL_MENUCONTENT_TEXT   : COL_MENUCONTENTDARK_TEXT)   : COL_MENUCONTENT_TEXT;
	b = toggle_background ? (toggle_enlighten ? COL_MENUCONTENT_PLUS_1 : COL_MENUCONTENTDARK_PLUS_0) : COL_MENUCONTENT_PLUS_0;
}
