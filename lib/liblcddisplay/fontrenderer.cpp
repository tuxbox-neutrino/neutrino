/*
        $Header$

	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2003 thegoodguy
		baseroutines by tmbinc


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

#include <config.h>

#include "fontrenderer.h"

#include <stdio.h>
#include <string.h>

#include <system/debug.h>

#include <ft2build.h>
#include FT_FREETYPE_H

/* tested with freetype 2.3.9, and 2.1.4 */
#if FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 3
#define FT_NEW_CACHE_API
#endif

// fribidi
#if 0// defined (ENABLE_FRIBIDI)
#include <fribidi/fribidi.h>
#endif

FT_Error LcdFontRenderClass::myFTC_Face_Requester(FTC_FaceID  face_id, FT_Library library, FT_Pointer request_data, FT_Face *aface)
{
	return ((LcdFontRenderClass *)request_data)->FTC_Face_Requester(face_id, aface);
}


LcdFontRenderClass::LcdFontRenderClass(CLCDDisplay *fb)
{
	framebuffer = fb;

	dprintf(DEBUG_NORMAL, "LcdFontRenderClass::LcdFontRenderClass: initializing core...\n");

	fflush(stdout);
	if (FT_Init_FreeType(&library))
	{
		dprintf(DEBUG_NORMAL, "LcdFontRenderClass::LcdFontRenderClass: failed.\n");
		return;
	}
	dprintf(DEBUG_NORMAL, "\n");
	font = 0;
	pthread_mutex_init(&render_mutex, NULL);
}

LcdFontRenderClass::~LcdFontRenderClass()
{
	FTC_Manager_Done(cacheManager);
	FT_Done_FreeType(library);
}

void LcdFontRenderClass::InitFontCache()
{
	dprintf(DEBUG_NORMAL, "LcdFontRenderClass::InitFontCache: Intializing font cache...\n");

	fflush(stdout);
	if (FTC_Manager_New(library, 3, 0, 0, myFTC_Face_Requester, this, &cacheManager))
	{
		dprintf(DEBUG_NORMAL, "LcdFontRenderClass::InitFontCache: manager failed!\n");
		return;
	}
	if (!cacheManager)
	{
		dprintf(DEBUG_NORMAL, "LcdFontRenderClass::InitFontCache: error.\n");
		return;
	}
	if (FTC_SBitCache_New(cacheManager, &sbitsCache))
	{
		dprintf(DEBUG_NORMAL, "LcdFontRenderClass::InitFontCache: sbit failed!\n");
		return;
	}
	if (FTC_ImageCache_New(cacheManager, &imageCache))
	{
		dprintf(DEBUG_NORMAL, "LcdFontRenderClass::InitFontCache: imagecache failed!\n");
	}
	printf("\n");
}

FT_Error LcdFontRenderClass::FTC_Face_Requester(FTC_FaceID  face_id, FT_Face    *aface)
{
	fontListEntry *font = (fontListEntry *)face_id;
	if (!font)
		return -1;

	dprintf(DEBUG_NORMAL, "LcdFontRenderClass::FTC_Face_Requester: FTC_Face_Requester (%s/%s)\n", font->family, font->style);

	int error;
	if ((error = FT_New_Face(library, font->filename, 0, aface)))
	{
		dprintf(DEBUG_NORMAL, "LcdFontRenderClass::FTC_Face_Requester: failed: %i\n", error);
		return error;
	}
	return 0;
}

FTC_FaceID LcdFontRenderClass::getFaceID(const char *family, const char *style)
{
	for (fontListEntry *f = font; f; f = f->next)
	{
		if ((!strcmp(f->family, family)) && (!strcmp(f->style, style)))
			return (FTC_FaceID)f;
	}
	for (fontListEntry *f = font; f; f = f->next)
	{
		if (!strcmp(f->family, family))
			return (FTC_FaceID)f;
	}
	return 0;
}

#ifdef FT_NEW_CACHE_API
FT_Error LcdFontRenderClass::getGlyphBitmap(FTC_ImageType font, FT_ULong glyph_index, FTC_SBit *sbit)
{
	return FTC_SBitCache_Lookup(sbitsCache, font, glyph_index, sbit, NULL);
}
#else
FT_Error LcdFontRenderClass::getGlyphBitmap(FTC_Image_Desc *font, FT_ULong glyph_index, FTC_SBit *sbit)
{
	return FTC_SBit_Cache_Lookup(sbitsCache, font, glyph_index, sbit);
}
#endif

const char *LcdFontRenderClass::AddFont(const char *const filename)
{
	dprintf(DEBUG_NORMAL, "LcdFontRenderClass::AddFont: adding font %s...\n", filename);

	fflush(stdout);
	int error;
	fontListEntry *n;

	FT_Face face;
	if ((error = FT_New_Face(library, filename, 0, &face)))
	{
		dprintf(DEBUG_NORMAL, "LcdFontRenderClass::AddFont: failed: %i\n", error);
		return NULL;
	}

	n = new fontListEntry;

	n->filename = strdup(filename);
	n->family   = strdup(face->family_name);
	n->style    = strdup(face->style_name);
	FT_Done_Face(face);

	n->next = font;
	dprintf(DEBUG_NORMAL, "LcdFontRenderClass::AddFont: OK (%s/%s)\n", n->family, n->style);
	font = n;

	return n->style;
}

LcdFontRenderClass::fontListEntry::~fontListEntry()
{
	free(filename);
	free(family);
	free(style);
}

LcdFont *LcdFontRenderClass::getFont(const char *family, const char *style, int size)
{
	FTC_FaceID id = getFaceID(family, style);
	if (!id)
		return 0;
	return new LcdFont(framebuffer, this, id, size);
}

LcdFont::LcdFont(CLCDDisplay *fb, LcdFontRenderClass *render, FTC_FaceID faceid, int isize)
{
	framebuffer = fb;
	renderer = render;
#ifdef FT_NEW_CACHE_API
	font.face_id = faceid;
	font.width  = isize;
	font.height = isize;
	font.flags  = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_MONOCHROME;
#else
	font.font.face_id = faceid;
	font.font.pix_width  = isize;
	font.font.pix_height = isize;
	font.image_type = ftc_image_mono;
	font.image_type |= ftc_image_flag_autohinted;
#endif
}

FT_Error LcdFont::getGlyphBitmap(FT_ULong glyph_index, FTC_SBit *sbit)
{
	return renderer->getGlyphBitmap(&font, glyph_index, sbit);
}

extern int UTF8ToUnicode(const char *&text, const bool utf8_encoded);	//defined in src/driver/fontrenderer.cpp
#if 0 //defined (ENABLE_FRIBIDI)
std::string fribidiShapeChar(const char *text, const bool utf8_encoded);
#endif

void LcdFont::RenderString(int x, int y, const int width, const char *text, const int color, const int selected, const bool utf8_encoded)
{
	int err;
	pthread_mutex_lock(&renderer->render_mutex);

// fribidi
#if 0// defined (ENABLE_FRIBIDI)
	std::string Text = fribidiShapeChar(text, utf8_encoded);
	text = Text.c_str();
#endif

#ifdef FT_NEW_CACHE_API
	FTC_ScalerRec scaler;

	scaler.face_id = font.face_id;
	scaler.width   = font.width;
	scaler.height  = font.height;
	scaler.pixel   = true;

	if ((err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size)) != 0)
#else
	if ((err = FTC_Manager_Lookup_Size(renderer->cacheManager, &font.font, &face, &size)) != 0)
#endif
	{
		dprintf(DEBUG_NORMAL, "LcdFont::RenderString: FTC_Manager_Lookup_Size failed! (%d)\n", err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return;
	}
	int left = x, step_y = (size->metrics.height >> 6) * 3 / 4 + 4;

	int pos = 0;
	for (; *text; text++)
	{
		pos++;
		FTC_SBit glyph;
		//if ((x + size->metrics.x_ppem > (left+width)) || (*text=='\n'))
		if (x + size->metrics.x_ppem > (left + width))
		{
			//width clip
			break;
		}
		if (*text == '\n')
		{
			x  = left;
			y += step_y;
		}

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;

#ifdef FT_NEW_CACHE_API
		int index = FT_Get_Char_Index(size->face, unicode_value);
#else
		int index = FT_Get_Char_Index(face, unicode_value);
#endif

		if (!index)
			continue;
		if (getGlyphBitmap(index, &glyph))
		{
			dprintf(DEBUG_NORMAL, "LcdFont::RenderString: failed to get glyph bitmap.\n");
			continue;
		}

		int rx = x + glyph->left;
		int ry = y - glyph->top;
		if (pos == selected)
		{
			framebuffer->draw_fill_rect(x - 2, y - glyph->height - 2, x + glyph->width + 2, y + 2, CLCDDisplay::PIXEL_INV);
		}

		for (int ay = 0; ay < glyph->height; ay++)
		{
			int ax = 0;
			int w = glyph->width;
			int xpos = rx;
			for (; ax < w; ax++)
			{
				unsigned char c = glyph->buffer[ay * abs(glyph->pitch) + (ax >> 3)];
				if ((c >> (7 - (ax & 7))) & 1)
					framebuffer->draw_point(xpos, ry, color);
				xpos ++;
			}
			ry++;
		}

		x += glyph->xadvance + 1;
	}
	pthread_mutex_unlock(&renderer->render_mutex);
}

int LcdFont::getRenderWidth(const char *text, const bool utf8_encoded)
{
	pthread_mutex_lock(&renderer->render_mutex);

// fribidi
#if 0// defined (ENABLE_FRIBIDI)
	std::string Text = fribidiShapeChar(text, utf8_encoded);
	text = Text.c_str();
#endif

	FT_Error err;
#ifdef FT_NEW_CACHE_API
	FTC_ScalerRec scaler;
	scaler.face_id = font.face_id;
	scaler.width   = font.width;
	scaler.height  = font.height;
	scaler.pixel   = true;

	err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
#else
	err = FTC_Manager_Lookup_Size(renderer->cacheManager, &font.font, &face, &size);
#endif
	if (err != 0)
	{
		dprintf(DEBUG_NORMAL, "LcdFont::getRenderWidth: FTC_Manager_Lookup_Size failed! (0x%x)\n", err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return -1;
	}
	int x = 0;
	for (; *text; text++)
	{
		FTC_SBit glyph;

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;

#ifdef FT_NEW_CACHE_API
		int index = FT_Get_Char_Index(size->face, unicode_value);
#else
		int index = FT_Get_Char_Index(face, unicode_value);
#endif

		if (!index)
			continue;
		if (getGlyphBitmap(index, &glyph))
		{
			dprintf(DEBUG_NORMAL, "LcdFont::getRenderWidth: failed to get glyph bitmap.\n");
			continue;
		}

		x += glyph->xadvance + 1;
	}
	pthread_mutex_unlock(&renderer->render_mutex);
	return x;
}
