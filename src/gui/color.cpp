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

unsigned char getBrightnessRGB(fb_pixel_t color)
{
	RgbColor rgb;
	rgb.r  = (color & 0x00FF0000) >> 16;
	rgb.g  = (color & 0x0000FF00) >>  8;
	rgb.b  =  color & 0x000000FF;

	return rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
}

fb_pixel_t changeBrightnessRGBRel(fb_pixel_t color, int br)
{
	int br_ = (int)getBrightnessRGB(color);
	br_ += br;
	if (br_ < 0) br_ = 0;
	if (br_ > 255) br_ = 255;
	return changeBrightnessRGB(color, (unsigned char)br_);
}

void changeBrightnessRGBRel2(RgbColor *rgb, int br)
{
	fb_pixel_t color = (((rgb->r << 16) & 0x00FF0000) |
			    ((rgb->g <<  8) & 0x0000FF00) |
			    ((rgb->b      ) & 0x000000FF));
	int br_ = (int)getBrightnessRGB(color);
	br_ += br;
	if (br_ < 0) br_ = 0;
	if (br_ > 255) br_ = 255;

	HsvColor hsv;
	Rgb2Hsv(rgb, &hsv);
	hsv.v = br;
	Hsv2Rgb(&hsv, rgb);
}

fb_pixel_t changeBrightnessRGB(fb_pixel_t color, unsigned char br)
{
	HsvColor hsv;
	RgbColor rgb;

	unsigned char tr;
	tr     = (color & 0xFF000000) >> 24;
	rgb.r  = (color & 0x00FF0000) >> 16;
	rgb.g  = (color & 0x0000FF00) >>  8;
	rgb.b  =  color & 0x000000FF;

	Rgb2Hsv(&rgb, &hsv);
	hsv.v = br;
	Hsv2Rgb(&hsv, &rgb);

	return (((tr    << 24) & 0xFF000000) |
		((rgb.r << 16) & 0x00FF0000) |
		((rgb.g <<  8) & 0x0000FF00) |
		((rgb.b      ) & 0x000000FF));
}

void Hsv2Rgb(HsvColor *hsv, RgbColor *rgb)
{
	unsigned char region, remainder, p, q, t;

	if (hsv->s == 0) {
		rgb->r = hsv->v;
		rgb->g = hsv->v;
		rgb->b = hsv->v;
		return;
	}

	region = hsv->h / 43;
	remainder = (hsv->h - (region * 43)) * 6;

	p = (hsv->v * (255 - hsv->s)) >> 8;
	q = (hsv->v * (255 - ((hsv->s * remainder) >> 8))) >> 8;
	t = (hsv->v * (255 - ((hsv->s * (255 - remainder)) >> 8))) >> 8;

	switch (region) {
		case 0:
			rgb->r = hsv->v; rgb->g = t; rgb->b = p;
			break;
		case 1:
			rgb->r = q; rgb->g = hsv->v; rgb->b = p;
			break;
		case 2:
			rgb->r = p; rgb->g = hsv->v; rgb->b = t;
			break;
		case 3:
			rgb->r = p; rgb->g = q; rgb->b = hsv->v;
			break;
		case 4:
			rgb->r = t; rgb->g = p; rgb->b = hsv->v;
			break;
		default:
			rgb->r = hsv->v; rgb->g = p; rgb->b = q;
			break;
	}

	return;
}

void Rgb2Hsv(RgbColor *rgb, HsvColor *hsv)
{
	unsigned char rgbMin, rgbMax;

	rgbMin = rgb->r < rgb->g ? (rgb->r < rgb->b ? rgb->r : rgb->b) : (rgb->g < rgb->b ? rgb->g : rgb->b);
	rgbMax = rgb->r > rgb->g ? (rgb->r > rgb->b ? rgb->r : rgb->b) : (rgb->g > rgb->b ? rgb->g : rgb->b);

	hsv->v = rgbMax;
	if (hsv->v == 0) {
		hsv->h = 0;
		hsv->s = 0;
		return;
	}

	hsv->s = 255 * long(rgbMax - rgbMin) / hsv->v;
	if (hsv->s == 0) {
		hsv->h = 0;
		return;
	}

	if (rgbMax == rgb->r)
		hsv->h = 0 + 43 * (rgb->g - rgb->b) / (rgbMax - rgbMin);
	else if (rgbMax == rgb->g)
		hsv->h = 85 + 43 * (rgb->b - rgb->r) / (rgbMax - rgbMin);
	else
		hsv->h = 171 + 43 * (rgb->r - rgb->g) / (rgbMax - rgbMin);

	return;
}
