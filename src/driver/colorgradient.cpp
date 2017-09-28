/*
	color gradient - Neutrino-GUI
	Copyright (C) 2014 M. Liebmann (micha-bbg)

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
#include <driver/colorgradient.h>
#include <gui/color.h>
#include <system/debug.h>

#pragma GCC diagnostic error "-Wconversion"

CColorGradient::CColorGradient()
{
	frameBuffer = CFrameBuffer::getInstance();
}

CColorGradient::~CColorGradient()
{
}

#if 0
CColorGradient* CColorGradient::getInstance()
{
	static CColorGradient* GradientInstance = NULL;
	if (!GradientInstance)
		GradientInstance = new CColorGradient();
	return GradientInstance;
}
#endif


uint8_t CColorGradient::limitChar(int c)
{
	uint8_t ret;
	if (c < 0) ret = 0;
	else if (c > 0xFF) ret = 0xFF;
	else ret = (uint8_t)c;
	return ret;
}

fb_pixel_t* CColorGradient::gradientColorToTransparent(fb_pixel_t col, fb_pixel_t *gradientBuf, int bSize, int /*mode*/, int /*intensity*/)
{
	if (bSize < 1)
		return gradientBuf;
	unsigned long _bSize = (unsigned long) bSize;
	if (gradientBuf == NULL) {
		gradientBuf = (fb_pixel_t*) malloc(_bSize * sizeof(fb_pixel_t));
		if (gradientBuf == NULL) {
			dprintf(DEBUG_NORMAL, "[%s:%d] malloc error\n", __func__, __LINE__);
			return NULL;
		}
	}
	memset((void*)gradientBuf, '\0', _bSize * sizeof(fb_pixel_t));

	int start_box = 0;
	int end_box = bSize;
	uint8_t tr_min = 0xFF;
	uint8_t tr_max = 0x20;
	float factor = (float)(tr_min - tr_max) / (float)(end_box - start_box);

	for (int i = start_box; i < end_box; i++) {

		uint8_t tr = limitChar((int)(factor * (float)i + tr_max) + 1);
		uint8_t r  = (uint8_t)((col & 0x00FF0000) >> 16);
		uint8_t g  = (uint8_t)((col & 0x0000FF00) >>  8);
		uint8_t b  = (uint8_t) (col & 0x000000FF);

		gradientBuf[i] = ((unsigned int)(tr << 24) & 0xFF000000) |
			         ((r  << 16) & 0x00FF0000) |
			         ((g  <<  8) & 0x0000FF00) |
			         ( b         & 0x000000FF);
	}
	return gradientBuf;
}

fb_pixel_t* CColorGradient::gradientOneColor(fb_pixel_t col, fb_pixel_t *gradientBuf, int bSize, int mode, int intensity, uint8_t v_min, uint8_t v_max, uint8_t s)
{
	if (bSize < 1)
		return gradientBuf;
	unsigned long _bSize = (unsigned long) bSize;
	if (gradientBuf == NULL) {
		gradientBuf = (fb_pixel_t*) malloc(_bSize * sizeof(fb_pixel_t));
		if (gradientBuf == NULL) {
			dprintf(DEBUG_NORMAL, "[%s:%d] malloc error\n", __func__, __LINE__);
			return NULL;
		}
	}
	memset((void*)gradientBuf, '\0', _bSize * sizeof(fb_pixel_t));

	HsvColor hsv;
	uint8_t min_v=0, max_v=0, col_s=0;
	uint8_t start_v=0 , end_v=0;

	uint8_t tr = SysColor2Hsv(col, &hsv);
	bool noSaturation = (hsv.s <= (float)0.05);

	if (intensity == extended) {
		min_v   = v_min;
		max_v   = v_max;
		col_s   = s;
	}
	else {
		switch (intensity) {
			case light:
				min_v   = 0x40;
				max_v   = 0xE0;
				col_s   = (noSaturation) ? 0 : 0xC0;
				break;
			case normal:
				min_v   = 0x00;
				max_v   = 0xFF;
				col_s   = (noSaturation) ? 0 : 0xC0;
				break;
		}
	}

	switch (mode) {
		case gradientDark2Light:
		case gradientDark2Light2Dark:
			start_v = min_v;
			end_v   = max_v;
			break;
		case gradientLight2Dark:
		case gradientLight2Dark2Light:
			start_v = max_v;
			end_v   = min_v;
			break;
		default:{
			free(gradientBuf);
			return 0;
		}
	}

	int bSize1 = ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) ? bSize/2 : bSize;

	int v  = start_v; int v_ = v;
	float factor_v = ((float)end_v - (float)v) / (float)bSize1;

	for (int i = 0; i < bSize1; i++) {
		v = v_ + (int)(factor_v * (float)i);
		hsv.v = (float)limitChar(v) / (float)255;
		hsv.s = (float)col_s / (float)255;
		gradientBuf[i] = Hsv2SysColor(&hsv, tr);
	}

	if ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) {
		bSize1 = bSize - bSize1;
		for (int i = 0; i < bSize1; i++) {
			v = v_ + (int)(factor_v * (float)i);
			hsv.v = (float)limitChar(v) / (float)255;
			hsv.s = (float)col_s / (float)255;
			gradientBuf[bSize-i-1] = Hsv2SysColor(&hsv, tr);
		}
	}

	return gradientBuf;
}

fb_pixel_t* CColorGradient::gradientColorToColor(fb_pixel_t start_col,fb_pixel_t end_col, fb_pixel_t *gradientBuf, int bSize, int mode, int /*intensity*/)
{
	unsigned long _bSize = (unsigned long) bSize;
	if (gradientBuf == NULL) {
		gradientBuf = (fb_pixel_t*) malloc(_bSize * sizeof(fb_pixel_t));
		if (gradientBuf == NULL) {
			dprintf(DEBUG_NORMAL, "[%s:%d] malloc error\n", __func__, __LINE__);
			return NULL;
		}
	}
	memset((void*)gradientBuf, '\0', _bSize * sizeof(fb_pixel_t));

	int start_box = 0;
	int end_box = bSize;

	std::swap(start_col,end_col);
	if (mode == gradientDark2Light){
		std::swap(start_col,end_col);
	}

	uint8_t start_tr = (uint8_t)((start_col & 0xFF000000) >> 24);
	uint8_t start_r  = (uint8_t)((start_col & 0x00FF0000) >> 16);
	uint8_t start_g  = (uint8_t)((start_col & 0x0000FF00) >>  8);
	uint8_t start_b  = (uint8_t) (start_col & 0x000000FF);

	uint8_t end_tr = (uint8_t)((end_col & 0xFF000000) >> 24);
	uint8_t end_r  = (uint8_t)((end_col & 0x00FF0000) >> 16);
	uint8_t end_g  = (uint8_t)((end_col & 0x0000FF00) >>  8);
	uint8_t end_b  = (uint8_t) (end_col & 0x000000FF);

	float steps = (float) bSize;

	float trStep = (float)(end_tr - start_tr) / steps;
	float rStep = (float)(end_r - start_r) / steps;
	float gStep = (float)(end_g - start_g) / steps;
	float bStep = (float)(end_b - start_b) / steps;

	for (int i = start_box; i < end_box; i++) {

		uint8_t tr = limitChar((int)((float)start_tr + trStep*(float)i));
		uint8_t r  = limitChar((int)((float)start_r + rStep*(float)i));
		uint8_t g  = limitChar((int)((float)start_g + gStep*(float)i));
		uint8_t b  = limitChar((int)((float)start_b + bStep*(float)i));

		gradientBuf[i] = ((unsigned int)(tr << 24) & 0xFF000000) |
			         ((r  << 16) & 0x00FF0000) |
			         ((g  <<  8) & 0x0000FF00) |
			         ( b         & 0x000000FF);
	}
	return gradientBuf;
}

//printf("[%s #%d] factor_v: %f, factor_s: %f, v: 0x%02X, s: 0x%02X\n", __func__, __LINE__, factor_v, factor_s, v, s);
