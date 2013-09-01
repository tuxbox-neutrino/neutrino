/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
        Copyright (C) 2003 thegoodguy

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

#include <stdio.h>
#include <stdlib.h>

// this method is recommended for FreeType >2.0.x:
#include <ft2build.h>
#include FT_FREETYPE_H

#include <driver/fontrenderer.h>

#include <system/debug.h>
#include <global.h>

FT_Error FBFontRenderClass::myFTC_Face_Requester(FTC_FaceID  face_id,
        FT_Library  /*library*/,
        FT_Pointer  request_data,
        FT_Face*    aface)
{
	return ((FBFontRenderClass*)request_data)->FTC_Face_Requester(face_id, aface);
}


FBFontRenderClass::FBFontRenderClass(const int xr, const int yr)
{
	dprintf(DEBUG_DEBUG, "[FONT] initializing core...\n");
	if (FT_Init_FreeType(&library))
	{
		dprintf(DEBUG_NORMAL, "[FONT] initializing core failed.\n");
		return;
	}

	font = NULL;

	xres = xr;
	yres = yr;

	int maxbytes= 4 *1024*1024;
	dprintf(DEBUG_INFO, "[FONT] Intializing font cache, using max. %dMB...\n", maxbytes/1024/1024);
	fflush(stdout);
	if (FTC_Manager_New(library, 10, 20, maxbytes, myFTC_Face_Requester, this, &cacheManager))
	{
		dprintf(DEBUG_NORMAL, "[FONT] manager failed!\n");
		return;
	}
	if (!cacheManager)
	{
		dprintf(DEBUG_NORMAL, "[FONT] error.\n");
		return;
	}
	if (FTC_SBitCache_New(cacheManager, &sbitsCache))
	{
		dprintf(DEBUG_NORMAL, "[FONT] sbit failed!\n");
		return;
	}
/*	if (FTC_ImageCache_New(cacheManager, &imageCache))
	{
		printf(" imagecache failed!\n");
	}
*/
	pthread_mutex_init( &render_mutex, NULL );
}

FBFontRenderClass::~FBFontRenderClass()
{
	fontListEntry * g;

	for (fontListEntry * f = font; f; f = g)
	{
		g = f->next;
		delete f;
	}

	FTC_Manager_Done(cacheManager);
	FT_Done_FreeType(library);
}

FT_Error FBFontRenderClass::FTC_Face_Requester(FTC_FaceID face_id, FT_Face* aface)
{
	fontListEntry *lfont=(fontListEntry *)face_id;
	if (!lfont)
		return -1;
	dprintf(DEBUG_DEBUG, "[FONT] FTC_Face_Requester (%s/%s)\n", lfont->family, lfont->style);

	int error;
	if ((error=FT_New_Face(library, lfont->filename, 0, aface)))
	{
		dprintf(DEBUG_NORMAL, "[FONT] FTC_Face_Requester (%s/%s) failed: %i\n", lfont->family, lfont->style, error);
		return error;
	}

	if (strcmp(lfont->style, (*aface)->style_name) != 0)
	{
		FT_Matrix matrix; // Italics

		matrix.xx = 1 * 0x10000;
		matrix.xy = (0x10000 >> 3);
		matrix.yx = 0 * 0x10000;
		matrix.yy = 1 * 0x10000;

		FT_Set_Transform(*aface, &matrix, NULL);
	}
	return 0;
}

FTC_FaceID FBFontRenderClass::getFaceID(const char * const family, const char * const style)
{
	for (fontListEntry *f=font; f; f=f->next)
	{
		if ((!strcmp(f->family, family)) && (!strcmp(f->style, style)))
			return (FTC_FaceID)f;
	}
	if (strncmp(style, "Bold ", 5) == 0)
	{
		for (fontListEntry *f=font; f; f=f->next)
		{
			if ((!strcmp(f->family, family)) && (!strcmp(f->style, &(style[5]))))
				return (FTC_FaceID)f;
		}
	}
	for (fontListEntry *f=font; f; f=f->next)
	{
		if (!strcmp(f->family, family))
		{
			if (f->next) // the first font always seems to be italic, skip if possible
				continue;
			return (FTC_FaceID)f;
		}
	}
	return 0;
}

FT_Error FBFontRenderClass::getGlyphBitmap(FTC_ImageTypeRec *pfont, FT_ULong glyph_index, FTC_SBit *sbit)
{
	return FTC_SBitCache_Lookup(sbitsCache, pfont, glyph_index, sbit, NULL);
}

FT_Error FBFontRenderClass::getGlyphBitmap(FTC_ScalerRec *sc, FT_ULong glyph_index, FTC_SBit *sbit)
{
	return FTC_SBitCache_LookupScaler(sbitsCache, sc, FT_LOAD_DEFAULT, glyph_index, sbit, NULL);
}

const char *FBFontRenderClass::AddFont(const char * const filename, const bool make_italics)
{
	fflush(stdout);
	int error;
	fontListEntry *n=new fontListEntry;

	FT_Face face;
	if ((error=FT_New_Face(library, filename, 0, &face)))
	{
		dprintf(DEBUG_NORMAL, "[FONT] adding font %s, failed: %i\n", filename, error);
		delete n;//Memory leak: n
		return NULL;
	}
	n->filename = strdup(filename);
	n->family   = strdup(face->family_name);
	n->style    = strdup(make_italics ? "Italic" : face->style_name);
	FT_Done_Face(face);
	n->next=font;
	dprintf(DEBUG_DEBUG, "[FONT] adding font %s... family %s, style %s ok\n", filename, n->family, n->style);
	font=n;
	return n->style;
}

FBFontRenderClass::fontListEntry::~fontListEntry()
{
	free(filename);
	free(family);
	free(style);
}

Font *FBFontRenderClass::getFont(const char * const family, const char * const style, int size)
{
	FTC_FaceID id = getFaceID(family, style);
	if (!id) {
		dprintf(DEBUG_DEBUG, "[FONT] getFont: family %s, style %s failed!\n", family, style);
		return 0;
	}
	dprintf(DEBUG_DEBUG, "[FONT] getFont: family %s, style %s ok\n", family, style);
	return new Font(this, id, size, (strcmp(((fontListEntry *)id)->style, style) == 0) ? Font::Regular : Font::Embolden);
}

std::string FBFontRenderClass::getFamily(const char * const filename) const
{
	for (fontListEntry *f=font; f; f=f->next)
	{
		if (!strcmp(f->filename, filename))
			return f->family;
	}

  return "";
}

Font::Font(FBFontRenderClass *render, FTC_FaceID faceid, const int isize, const fontmodifier _stylemodifier)
{
	stylemodifier           = _stylemodifier;

	frameBuffer 		= CFrameBuffer::getInstance();
	renderer 		= render;
	font.face_id 	= faceid;
	font.width  	= isize;
	font.height 	= isize;
	//font.image_type 	= ftc_image_grays;
	//font.image_type 	|= ftc_image_flag_autohinted;
	font.flags = FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT;

	scaler.face_id = font.face_id;
	scaler.width   = isize * 64;
	scaler.height  = isize * 64;
	scaler.pixel   = false;
	scaler.x_res   = render->xres;
	scaler.y_res   = render->yres;

	setSize(isize);
}

FT_Error Font::getGlyphBitmap(FT_ULong glyph_index, FTC_SBit *sbit)
{
	return renderer->getGlyphBitmap(&scaler, glyph_index, sbit);
}

int Font::setSize(int isize)
{
	int temp = font.width;
	font.width = font.height = isize;
	scaler.width  = isize * 64;
	scaler.height = isize * 64;

	FT_Error err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
	if (err != 0)
	{
		dprintf(DEBUG_NORMAL, "%s:FTC_Manager_LookupSize failed (0x%x)\n", __FUNCTION__, err);
		return 0;
	}
	face = size->face;
#if 0
//FIXME test

	ascender = face->ascender;
	descender = face->descender;
	height = face->height;
	lower = -descender+(-(descender>>1))+1;
return 0;
#endif
	// hack begin (this is a hack to get correct font metrics, didn't find any other way which gave correct values)
	FTC_SBit glyph;
	int index;

	index=FT_Get_Char_Index(face, 'M'); // "M" gives us ascender
	getGlyphBitmap(index, &glyph);
	int tM=glyph->top;
	fontwidth = glyph->width;

	index=FT_Get_Char_Index(face, 'g'); // "g" gives us descender
	getGlyphBitmap(index, &glyph);
	int hg=glyph->height;
	int tg=glyph->top;

	ascender=tM;
	descender=tg-hg; //this is a negative value!
	int halflinegap= -(descender>>1); // |descender/2| - we use descender as linegap, half at top, half at bottom
	upper = halflinegap+ascender+3;   // we add 3 at top
	lower = -descender+halflinegap+1; // we add 1 at bottom
	height=upper+lower;               // this is total height == distance of lines
	DigitHeight = ascender+2;
	DigitOffset = -descender+halflinegap;
	// hack end

	//printf("glyph: hM=%d tM=%d hg=%d tg=%d ascender=%d descender=%d height=%d linegap/2=%d upper=%d lower=%d\n",
	//       hM,tM,hg,tg,ascender,descender,height,halflinegap,upper,lower);
	//printf("font metrics: height=%ld\n", (size->metrics.height+32) >> 6);
	return temp;
}

int Font::getWidth(void)
{
	return fontwidth;
}

int Font::getHeight(void)
{
	return height;
}

int Font::getDigitHeight(void)
{
	return DigitHeight;
}

int Font::getDigitOffset(void)
{
	return DigitOffset;
}

int UTF8ToUnicode(const char * &text, const bool utf8_encoded) // returns -1 on error
{
	int unicode_value;
//printf("%c ", (unsigned char)(*text));
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

#define F_MUL 0x7FFF

void Font::paintFontPixel(fb_pixel_t *td, uint8_t fg_trans, uint8_t fg_red, uint8_t fg_green, uint8_t fg_blue, fb_pixel_t bg_col, int faktor, uint8_t index)
{
	int korr_t = ((bg_col & 0xFF000000) >> 24) - fg_trans;
	int korr_r = ((bg_col & 0x00FF0000) >> 16) - fg_red;
	int korr_g = ((bg_col & 0x0000FF00) >>  8) - fg_green;
	int korr_b =  (bg_col & 0x000000FF)        - fg_blue;

	*td =   ((g_settings.contrast_fonts && (index > 128)) ? 0xFF000000 : (((fg_trans + ((korr_t*faktor)/F_MUL)) << 24) & 0xFF000000)) |
		(((fg_red   + ((korr_r*faktor)/F_MUL)) << 16) & 0x00FF0000) |
		(((fg_green + ((korr_g*faktor)/F_MUL)) <<  8) & 0x0000FF00) |
		 ((fg_blue  + ((korr_b*faktor)/F_MUL))        & 0x000000FF);
}

void Font::RenderString(int x, int y, const int width, const char *text, const fb_pixel_t color, const int boxheight, const bool utf8_encoded, const bool useFullBg)
{
/*
	useFullBg (default = false)

	useFullBg = false
	fetch bgcolor from framebuffer, using lower left edge of the font

	useFullBg = true
	fetch bgcolor from framebuffer, using the respective real font position
	- font rendering slower
	- e.g. required for font rendering on images
*/

	if (!frameBuffer->getActive())
		return;

	frameBuffer->checkFbArea(x, y-height, width, height, true);

	pthread_mutex_lock( &renderer->render_mutex );

	FT_Error err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
	if (err != 0)
	{
		dprintf(DEBUG_NORMAL, "%s:FTC_Manager_LookupSize failed (0x%x)\n", __FUNCTION__, err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return;
	}
	face = size->face;

	int use_kerning=FT_HAS_KERNING(face);

	int left=x;
	int step_y=height;

	// ----------------------------------- box upper end (this is NOT a font metric, this is our method for y centering)
	//
	// **  -------------------------------- y=baseline-upper
	// ||
	// |u  --------------------*----------- y=baseline+ascender
	// |p                     * *
	// hp                    *   *
	// ee     *        *    *     *
	// ir      *      *     *******
	// g|       *    *      *     *
	// h|        *  *       *     *
	// t*  -------**--------*-----*-------- y=baseline
	// |l         *
	// |o        *
	// |w  -----**------------------------- y=baseline+descender   // descender is a NEGATIVE value
	// |r
	// **  -------------------------------- y=baseline+lower == YCALLER
	//
	// ----------------------------------- box lower end (this is NOT a font metric, this is our method for y centering)

	// height = ascender + -1*descender + linegap           // descender is negative!

	// now we adjust the given y value (which is the line marked as YCALLER) to be the baseline after adjustment:
	y -= lower;
	// y coordinate now gives font baseline which is used for drawing

	// caution: this only works if we print a single line of text
	// if we have multiple lines, don't use boxheight or specify boxheight==0.
	// if boxheight is !=0, we further adjust y, so that text is y-centered in the box
	if(boxheight)
	{
		if(boxheight>step_y)			// this is a real box (bigger than text)
			y -= ((boxheight-step_y)>>1);
		else if(boxheight<0)			// this normally would be wrong, so we just use it to define a "border"
			y += (boxheight>>1);		// half of border value at lower end, half at upper end
	}

	int lastindex=0; // 0 == missing glyph (never has kerning values)
	FT_Vector kerning;
	int pen1=-1; // "pen" positions for kerning, pen2 is "x"
	static fb_pixel_t old_bgcolor    = 0, old_fgcolor = 0;
	static uint8_t fg_trans          = 0, fg_red = 0, fg_green = 0, fg_blue = 0;
	static bool olduseFullBg         = false;
	static fb_pixel_t colors[256]    = {0};
	static int faktor[256]           = {0};
	static bool fontRecsInit         = false;
	fb_pixel_t bg_color              = 1;
	fb_pixel_t fg_color              = color;

	if (!useFullBg) {
		/* the GXA seems to do it's job asynchonously, so we need to wait until
		   it's ready, otherwise the font will sometimes "be overwritten" with
		   background color or bgcolor will be wrong */
		frameBuffer->waitForIdle("Font::RenderString 1");
		/* fetch bgcolor from framebuffer, using lower left edge of the font... */
		bg_color = *(frameBuffer->getFrameBufferPointer() + x +
				y * frameBuffer->getStride() / sizeof(fb_pixel_t));
	}
	else
		bg_color = 0;

	if ((old_fgcolor != fg_color) || (old_bgcolor != bg_color) || (olduseFullBg != useFullBg) || !fontRecsInit) {
		old_bgcolor  = bg_color;
		old_fgcolor  = fg_color;
		olduseFullBg = useFullBg;
		fontRecsInit = true;

		fg_trans   =  (fg_color & 0xFF000000) >> 24;
		fg_red     =  (fg_color & 0x00FF0000) >> 16;
		fg_green   =  (fg_color & 0x0000FF00) >>  8;
		fg_blue    =   fg_color & 0x000000FF;

		int korr_t=0, korr_r=0, korr_g=0, korr_b=0;
		if (!useFullBg) {
			korr_t = ((bg_color & 0xFF000000) >> 24) - fg_trans;
			korr_r = ((bg_color & 0x00FF0000) >> 16) - fg_red;
			korr_g = ((bg_color & 0x0000FF00) >>  8) - fg_green;
			korr_b =  (bg_color & 0x000000FF)        - fg_blue;
		}

		for (int i = 0; i <= 0xFF; i++) {
			int _faktor = ((0xFF - i) * F_MUL) / 0xFF;

			if (useFullBg)
				faktor[i]   = _faktor;
			else
				colors[i] =  ((g_settings.contrast_fonts && (i > 128)) ? 0xFF000000 : (((fg_trans + ((korr_t*_faktor)/F_MUL)) << 24) & 0xFF000000)) |
					     (((fg_red   + ((korr_r*_faktor)/F_MUL)) << 16) & 0x00FF0000) |
					     (((fg_green + ((korr_g*_faktor)/F_MUL)) <<  8) & 0x0000FF00) |
					      ((fg_blue  + ((korr_b*_faktor)/F_MUL))        & 0x000000FF);
		}
	}

	int spread_by = 0;
	if (stylemodifier == Font::Embolden)
	{
		spread_by = (fontwidth / 6) - 1;
		if (spread_by < 1)
			spread_by = 1;
	}

	for (; *text; text++)
	{
		FTC_SBit glyph;

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;

		if (*text=='\n')
		{
			/* a '\n' in the text is basically an error, it should not have come
			   until here. To find the offenders, we replace it with a paragraph
			   marker */
			unicode_value = 0x00b6; /* &para;  PILCROW SIGN */
			/* this did not work anyway - pen1 overrides x value down below :-)
			   and there are no checks that we don't leave our assigned box
			x  = left;
			y += step_y;
			 */
		}

		int index = FT_Get_Char_Index(face, unicode_value);

		if (!index)
			continue;
		if (getGlyphBitmap(index, &glyph))
		{
			dprintf(DEBUG_NORMAL, "failed to get glyph bitmap.\n");
			continue;
		}

		//kerning
		if (use_kerning)
		{
			FT_Get_Kerning(face, lastindex, index, 0, &kerning);
			//x+=(kerning.x+32)>>6; // kerning!
			x += (kerning.x) >> 6; // kerning!
		}

		// width clip
		if (x + glyph->xadvance + spread_by > left + width)
			break;

		int stride  = frameBuffer->getStride();
		int ap=(x + glyph->left) * sizeof(fb_pixel_t) + stride * (y - glyph->top);
		uint8_t * d = ((uint8_t *)frameBuffer->getFrameBufferPointer()) + ap;
		uint8_t * s = glyph->buffer;
		int w       = glyph->width;
		int h       = glyph->height;
		int pitch   = glyph->pitch;
		if (ap>-1) {
			fb_pixel_t *bg_buf = NULL;
			if (useFullBg) {
				// save background of the char
				bg_buf = new fb_pixel_t[h * (w+spread_by)];
				uint8_t *pos = d;
				fb_pixel_t *bkpos = bg_buf;
				/* the GXA seems to do it's job asynchonously, so we need to wait until
				   it's ready, otherwise the font will sometimes "be overwritten" with
				   background color or bgcolor will be wrong */
				frameBuffer->waitForIdle("Font::RenderString 2");
				for (int j = 0; j < h; j++) {
					fb_pixel_t *dest = (fb_pixel_t*)pos;
					for (int i = 0; i < (w + spread_by); i++)
						*(bkpos++) = *(dest++);
					pos += stride;
				}
			}

			for (int ay = 0; ay < h; ay++) {
				fb_pixel_t * td = (fb_pixel_t *)d;
				int ax;
				for (ax = 0; ax < w + spread_by; ax++) {
					if (stylemodifier != Font::Embolden) {
						/* do not paint the backgroundcolor (*s = 0) */
						if(*s != 0) {
							if (useFullBg)
								paintFontPixel(td, fg_trans, fg_red, fg_green, fg_blue, bg_buf[ax*ay], faktor[*s], *s);
							else
								*td = colors[*s];
						}
					}
					else {
						int lcolor = -1;
						int start = (ax < w) ? 0 : ax - w + 1;
						int end   = (ax < spread_by) ? ax + 1 : spread_by + 1;
						for (int i = start; i < end; i++)
							if (lcolor < *(s - i))
								lcolor = *(s - i);
						/* do not paint the backgroundcolor (lcolor = 0) */
						if(lcolor != 0) {
							if (useFullBg)
								paintFontPixel(td, fg_trans, fg_red, fg_green, fg_blue, bg_buf[ax*ay], faktor[lcolor], (uint8_t)lcolor);
							else
								*td = colors[lcolor];
						}
					}
					td++; s++;
				}
				s += pitch - ax;
				d += stride;
			}
			if (bg_buf != NULL)
				delete[] bg_buf;
			bg_buf = NULL;
		}
		x += glyph->xadvance + 1;
		if (pen1 > x)
			x = pen1;
		pen1 = x;
		lastindex = index;
	}
//printf("RenderStat: %d %d %d \n", renderer->cacheManager->num_nodes, renderer->cacheManager->num_bytes, renderer->cacheManager->max_bytes);
	pthread_mutex_unlock( &renderer->render_mutex );
	frameBuffer->checkFbArea(x, y-height, width, height, false);
	/* x is the rightmost position of the last drawn character */
	frameBuffer->mark(left, y + lower - height, x, y + lower);
}

void Font::RenderString(int x, int y, const int width, const std::string & text, const fb_pixel_t color, const int boxheight, const bool utf8_encoded, const bool useFullBg)
{
	RenderString(x, y, width, text.c_str(), color, boxheight, utf8_encoded, useFullBg);
}

int Font::getRenderWidth(const char *text, const bool utf8_encoded)
{
	pthread_mutex_lock( &renderer->render_mutex );

	FT_Error err = FTC_Manager_LookupSize(renderer->cacheManager, &scaler, &size);
	if (err != 0)
	{
		dprintf(DEBUG_NORMAL, "%s:FTC_Manager_LookupSize failed (0x%x)\n", __FUNCTION__, err);
		pthread_mutex_unlock(&renderer->render_mutex);
		return -1;
	}
	face = size->face;

	int use_kerning=FT_HAS_KERNING(face);
	int x=0;
	int lastindex=0; // 0==missing glyph (never has kerning)
	FT_Vector kerning;
	int pen1=-1; // "pen" positions for kerning, pen2 is "x"
	for (; *text; text++)
	{
		FTC_SBit glyph;

		int unicode_value = UTF8ToUnicode(text, utf8_encoded);

		if (unicode_value == -1)
			break;

		int index=FT_Get_Char_Index(face, unicode_value);

		if (!index)
			continue;
		if (getGlyphBitmap(index, &glyph))
		{
			dprintf(DEBUG_NORMAL, "failed to get glyph bitmap.\n");
			continue;
		}
		//kerning
		if (use_kerning)
		{
			FT_Get_Kerning(face, lastindex, index, 0, &kerning);
			x += (kerning.x) >> 6; // kerning!
		}

		x+=glyph->xadvance+1;
		if(pen1>x)
			x=pen1;
		pen1=x;
		lastindex=index;
	}

	if (stylemodifier == Font::Embolden)
	{
		int spread_by = (fontwidth / 6) - 1;
		if (spread_by < 1)
			spread_by = 1;

		x += spread_by;
	}

	pthread_mutex_unlock( &renderer->render_mutex );

	return x;
}

int Font::getRenderWidth(const std::string & text, const bool utf8_encoded)
{
	return getRenderWidth(text.c_str(), utf8_encoded);
}
