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

#include <algorithm>
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

fb_pixel_t* CColorGradient::gradientColorToTransparent(fb_pixel_t col, fb_pixel_t *gradientBuf, int bSize, int mode, int intensity)
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

	uint8_t tr_min, tr_max;
	switch (intensity) {
		case light:
			tr_min = 0xE0;
			tr_max = 0x40;
			break;
		case normal:
		default:
			tr_min = 0xFF;
			tr_max = 0x20;
			break;
	}

	/* Swap alpha range based on mode direction */
	if (mode == gradientLight2Dark || mode == gradientLight2Dark2Light)
		std::swap(tr_min, tr_max);

	uint8_t r = (uint8_t)((col & 0x00FF0000) >> 16);
	uint8_t g = (uint8_t)((col & 0x0000FF00) >>  8);
	uint8_t b = (uint8_t) (col & 0x000000FF);

	int bSize1 = ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) ? (bSize + 1) / 2 : bSize;
	float factor = (float)(tr_min - tr_max) / (float)bSize1;

	for (int i = 0; i < bSize1; i++) {
		uint8_t tr = limitChar((int)(factor * (float)i + tr_max) + 1);

		gradientBuf[i] = ((unsigned int)(tr << 24) & 0xFF000000) |
			         ((r  << 16) & 0x00FF0000) |
			         ((g  <<  8) & 0x0000FF00) |
			         ( b         & 0x000000FF);
	}

	/* For symmetric modes, mirror the first half into the second */
	if ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) {
		for (int i = 0; i < bSize / 2; i++)
			gradientBuf[bSize - i - 1] = gradientBuf[i];
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
		col_s   = (noSaturation) ? 0 : s;
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

	/* Scale original saturation once; keep it constant across the gradient */
	float scaled_s = (col_s == 0) ? 0.0f : hsv.s * ((float)col_s / (float)255);

	int bSize1 = ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) ? (bSize + 1) / 2 : bSize;

	int v  = start_v; int v_ = v;
	/* interpolate over bSize1 - 1 steps so the last pixel reaches end_v */
	float factor_v = ((float)end_v - (float)v) / (float)std::max(bSize1 - 1, 1);

	for (int i = 0; i < bSize1; i++) {
		v = v_ + (int)(factor_v * (float)i);
		hsv.v = (float)limitChar(v) / (float)255;
		hsv.s = scaled_s;
		gradientBuf[i] = Hsv2SysColor(&hsv, tr);
	}

	/* For symmetric modes, mirror the first half into the second */
	if ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) {
		for (int i = 0; i < bSize / 2; i++)
			gradientBuf[bSize - i - 1] = gradientBuf[i];
	}

	return gradientBuf;
}

fb_pixel_t* CColorGradient::gradientColorToColor(fb_pixel_t start_col, fb_pixel_t end_col, fb_pixel_t *gradientBuf, int bSize, int mode, int intensity)
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

	if (mode == gradientLight2Dark || mode == gradientLight2Dark2Light)
		std::swap(start_col, end_col);

	uint8_t start_tr = (uint8_t)((start_col & 0xFF000000) >> 24);
	uint8_t start_r  = (uint8_t)((start_col & 0x00FF0000) >> 16);
	uint8_t start_g  = (uint8_t)((start_col & 0x0000FF00) >>  8);
	uint8_t start_b  = (uint8_t) (start_col & 0x000000FF);

	uint8_t end_tr = (uint8_t)((end_col & 0xFF000000) >> 24);
	uint8_t end_r  = (uint8_t)((end_col & 0x00FF0000) >> 16);
	uint8_t end_g  = (uint8_t)((end_col & 0x0000FF00) >>  8);
	uint8_t end_b  = (uint8_t) (end_col & 0x000000FF);

	/* Intensity controls the effective blend range */
	float blend_start = 0.0f;
	float blend_end   = 1.0f;
	if (intensity == light) {
		blend_start = 0.125f;
		blend_end   = 0.875f;
	}

	int bSize1 = ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) ? (bSize + 1) / 2 : bSize;
	/* interpolate over bSize1 - 1 steps so the last pixel reaches end_col
	   (for symmetric modes: the mirror point reaches the middle color) */
	float steps = (float)std::max(bSize1 - 1, 1);

	for (int i = 0; i < bSize1; i++) {
		float t = blend_start + (blend_end - blend_start) * ((float)i / steps);

		uint8_t tr = limitChar((int)((float)start_tr + (float)(end_tr - start_tr) * t));
		uint8_t r  = limitChar((int)((float)start_r  + (float)(end_r  - start_r)  * t));
		uint8_t g  = limitChar((int)((float)start_g  + (float)(end_g  - start_g)  * t));
		uint8_t b  = limitChar((int)((float)start_b  + (float)(end_b  - start_b)  * t));

		gradientBuf[i] = ((unsigned int)(tr << 24) & 0xFF000000) |
			         ((r  << 16) & 0x00FF0000) |
			         ((g  <<  8) & 0x0000FF00) |
			         ( b         & 0x000000FF);
	}

	/* For symmetric modes, mirror the first half into the second */
	if ((mode == gradientDark2Light2Dark) || (mode == gradientLight2Dark2Light)) {
		for (int i = 0; i < bSize / 2; i++)
			gradientBuf[bSize - i - 1] = gradientBuf[i];
	}

	return gradientBuf;
}

//printf("[%s #%d] factor_v: %f, factor_s: %f, v: 0x%02X, s: 0x%02X\n", __func__, __LINE__, factor_v, factor_s, v, s);
