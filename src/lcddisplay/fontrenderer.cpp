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

#include <ft2build.h>
#include FT_FREETYPE_H

#ifdef ENABLE_FRIBIDI
#include <fribidi/fribidi.h>
#endif

FT_Error LcdFontRenderClass::myFTC_Face_Requester(FTC_FaceID  face_id,
                            FT_Library  /*library*/,
                            FT_Pointer  request_data,
                            FT_Face*    aface)
{
	return ((LcdFontRenderClass*)request_data)->FTC_Face_Requester(face_id, aface);
}


LcdFontRenderClass::LcdFontRenderClass(CLCDDisplay * fb)
{
	framebuffer = fb;
	printf("[LCDFONT] initializing core...");
	fflush(stdout);
	if (FT_Init_FreeType(&library))
	{
		printf("failed.\n");
		return;
	}
	printf("\n");
	font=0;
	pthread_mutex_init(&render_mutex, NULL);
}

LcdFontRenderClass::~LcdFontRenderClass()
{
	FTC_Manager_Done(cacheManager);
	FT_Done_FreeType(library);
}

void LcdFontRenderClass::InitFontCache()
{
	printf("[LCDFONT] Intializing font cache...");
	fflush(stdout);
	if (FTC_Manager_New(library, 3, 0, 0, myFTC_Face_Requester, this, &cacheManager))
	{
		printf(" manager failed!\n");
		return;
	}
	if (!cacheManager)
	{
		printf(" error.\n");
		return;
	}
	if (FTC_SBitCache_New(cacheManager, &sbitsCache))
	{
		printf(" sbit failed!\n");
		return;
	}
	if (FTC_ImageCache_New(cacheManager, &imageCache))
	{
		printf(" imagecache failed!\n");
	}
	printf("\n");
}

FT_Error LcdFontRenderClass::FTC_Face_Requester(FTC_FaceID  face_id,
                            FT_Face*    aface)
{
	fontListEntry *f=(fontListEntry *)face_id;
	if (!f)
		return -1;
	printf("[LCDFONT] FTC_Face_Requester (%s/%s)\n", f->family, f->style);

	int error;
	if ((error=FT_New_Face(library, f->filename, 0, aface)))
	{
		printf(" failed: %i\n", error);
		return error;
	}
	return 0;
}                                                                                                                                

FTC_FaceID LcdFontRenderClass::getFaceID(const char *family, const char *style)
{
	for (fontListEntry *f=font; f; f=f->next)
	{
		if ((!strcmp(f->family, family)) && (!strcmp(f->style, style)))
			return (FTC_FaceID)f;
	}
	for (fontListEntry *f=font; f; f=f->next)
	{
		if (!strcmp(f->family, family))
			return (FTC_FaceID)f;
	}
	return 0;
}

FT_Error LcdFontRenderClass::getGlyphBitmap(FTC_ImageType f, FT_ULong glyph_index, FTC_SBit *sbit)
{
	return FTC_SBitCache_Lookup(sbitsCache, f, glyph_index, sbit, NULL);
}

const char * LcdFontRenderClass::AddFont(const char * const filename)
{
	printf("[LCDFONT] adding font %s...", filename);
	fflush(stdout);
	int error;
	fontListEntry *n=new fontListEntry;

	FT_Face face;
	if ((error=FT_New_Face(library, filename, 0, &face)))
	{
		printf(" failed: %i\n", error);
		delete n;
		return NULL;
	}
	n->filename = strdup(filename);
	n->family   = strdup(face->family_name);
	n->style    = strdup(face->style_name);
	FT_Done_Face(face);

	n->next=font;
	printf("OK (%s/%s)\n", n->family, n->style);
	font=n;
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
	FTC_FaceID id=getFaceID(family, style);
	if (!id)
		return 0;
	return new LcdFont(framebuffer, this, id, size);
}

LcdFont::LcdFont(CLCDDisplay * fb, LcdFontRenderClass *render, FTC_FaceID faceid, int isize)
{
	framebuffer=fb;
	renderer=render;
	font.face_id=faceid;
	font.width  = isize;
	font.height = isize;
	font.flags  = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_MONOCHROME;
}

FT_Error LcdFont::getGlyphBitmap(FT_ULong glyph_index, FTC_SBit *sbit)
{
	return renderer->getGlyphBitmap(&font, glyph_index, sbit);
}

extern int UTF8ToUnicode(const char * &text, const bool utf8_encoded); // returns -1 on error
#if 0
{
	int unicode_value;
	
	if (utf8_encoded && ((((unsigned char)(*text)) & 0x80) != 0))
	{
		int remaining_unicode_length;
		if ((((unsigned char)(*text)) & 0xf8) == 0xf0)
		{
			unicode_value = ((unsigned char)(*text)) & 0x07;
			remaining_unicode_length = 3;
		}
		else if ((((unsigned char)(*text)) & 0xf0) == 0xe0)
		{
			unicode_value = ((unsigned char)(*text)) & 0x0f;
			remaining_unicode_length = 2;
		}
		else if ((((unsigned char)(*text)) & 0xe0) == 0xc0)
		{
			unicode_value = ((unsigned char)(*text)) & 0x1f;
			remaining_unicode_length = 1;
		}
		else                     // cf.: http://www.cl.cam.ac.uk/~mgk25/unicode.html#utf-8
			return -1;       // corrupted character or a character with > 4 bytes utf-8 representation
		
		for (int i = 0; i < remaining_unicode_length; i++)
		{
			text++;
			if (((*text) & 0xc0) != 0x80)
			{
				remaining_unicode_length = -1;
				return -1;          // incomplete or corrupted character
			}
			unicode_value <<= 6;
			unicode_value |= ((unsigned char)(*text)) & 0x3f;
		}
		if (remaining_unicode_length == -1)
			return -1;                  // incomplete or corrupted character
	}
	else
		unicode_value = (unsigned char)(*text);

	return unicode_value;
}
#endif

#ifdef ENABLE_FRIBIDI
std::string fribidi_shape_char(const char * text);
#endif

void LcdFont::RenderString(int x, int y, const int width, const char * text, const int color, const int selected, const bool utf8_encoded)
{
	int err;
	pthread_mutex_lock(&renderer->render_mutex);

#ifdef ENABLE_FRIBIDI
	std::string Text = fribidi_shape_char(text);
	text = Text.c_str();
#endif

	FTC_ScalerRec scaler;

	scaler.face_id = font.face_id;
	scaler.width   = font.width;
	scaler.height  = font.height;
	scaler.pixel   = true;

	if ((err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size)) != 0)
	{ 
		printf("FTC_Manager_Lookup_Size failed! (%d)\n",err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return;
	}
	int left=x, step_y=(size->metrics.height >> 6 )*3/4 + 4;

	int pos =0;
	for (; *text; text++)
	{
		pos++;
		FTC_SBit glyph;
		//if ((x + size->metrics.x_ppem > (left+width)) || (*text=='\n'))
		if (x + size->metrics.x_ppem > (left+width))
		{ //width clip
			break;
		}
		if (*text=='\n')
		{
		  x  = left;
		  y += step_y;
		}

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;

		int index = FT_Get_Char_Index(size->face, unicode_value);

		if (!index)
		  continue;
		if (getGlyphBitmap(index, &glyph))
		{
		  printf("failed to get glyph bitmap.\n");
		  continue;
		}
    
		int rx=x+glyph->left;
		int ry=y-glyph->top;
		if(pos==selected)
		{
			framebuffer->draw_fill_rect(x-2,y-glyph->height-2, x+glyph->width+2, y+2, CLCDDisplay::PIXEL_INV );
		}
		
		for (int ay=0; ay<glyph->height; ay++)
		{
			int ax=0;
			int w=glyph->width;
			int xpos = rx;
			for (; ax<w; ax++)
			{
				unsigned char c = glyph->buffer[ay*abs(glyph->pitch)+(ax>>3)];
				if((c>>(7-(ax&7)))&1)
				framebuffer->draw_point(xpos,ry, color);
				xpos ++;
			}
		ry++;
		}

		x+=glyph->xadvance+1;
	}
	pthread_mutex_unlock(&renderer->render_mutex);
}

int LcdFont::getRenderWidth(const char * text, const bool utf8_encoded)
{
	pthread_mutex_lock(&renderer->render_mutex);

#ifdef ENABLE_FRIBIDI
	std::string Text = fribidi_shape_char(text);
	text = Text.c_str();
#endif

	FT_Error err;
	FTC_ScalerRec scaler;
	scaler.face_id = font.face_id;
	scaler.width   = font.width;
	scaler.height  = font.height;
	scaler.pixel   = true;

	err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
	if (err != 0)
	{ 
		printf("FTC_Manager_Lookup_Size failed! (0x%x)\n", err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return -1;
	}
	int x=0;
	for (; *text; text++)
	{
		FTC_SBit glyph;

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;

		int index = FT_Get_Char_Index(size->face, unicode_value);

		if (!index)
			continue;
		if (getGlyphBitmap(index, &glyph))
		{
			printf("failed to get glyph bitmap.\n");
			continue;
		}
    
		x+=glyph->xadvance+1;
	}
	pthread_mutex_unlock(&renderer->render_mutex);
	return x;
}

