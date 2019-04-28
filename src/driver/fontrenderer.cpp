/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
        Copyright (C) 2003 thegoodguy
        Copyright (C) 2013-2017 M. Liebmann (micha-bbg)
	Copyright (C) 2009-2013,2017-2018 Stefan Seyfried

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

#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>

#include <system/debug.h>
#include <global.h>

#ifdef ENABLE_FRIBIDI
#include <fribidi/fribidi.h>
#endif

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
		delete f; f = NULL;
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

	maxdigitwidth = 0;

	scaler.face_id = font.face_id;
	scaler.width   = isize * 64;
	scaler.height  = isize * 64;
	scaler.pixel   = false;
	scaler.x_res   = render->xres;
	scaler.y_res   = render->yres;

	setSize(isize);
	fg_red = 0, fg_green = 0, fg_blue = 0;
	memset((void*)colors, '\0', sizeof(colors));
	useFullBG = false;
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

	//hack: Use additional percentage height offset, font types could have different heights, static values seems not really senseful.
	upper = halflinegap+ascender+hg/7;   // we add 1/7 of glyph height at top
	lower = -descender+halflinegap-hg/10;  // we add 1/10 of glyph height at bottom

	height=upper+lower;               // this is total height == distance of lines

	DigitHeight = ascender+2; //Is this static value really ok?
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

int Font::getMaxDigitWidth(void)
{
	if (maxdigitwidth < 1) {
		char b[2];
		b[1] = 0;
		for (char c = '0'; c <= '9'; c++) {
			*b = c;
			int w = getRenderWidth(b);
			if (w > maxdigitwidth)
				maxdigitwidth = w;
		}
	}
	return maxdigitwidth;
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

#ifdef ENABLE_FRIBIDI
static std::string fribidi_shape_char(const char * text)
{
	if(text && *text)
	{
		int len = strlen(text);
		char * rtl_text = NULL;
		int rtl_len = 0;

		fribidi_set_mirroring(true);
		fribidi_set_reorder_nsm(false);

		// init to utf-8
		FriBidiCharSet fribidi_charset = FRIBIDI_CHAR_SET_UTF8;

		// tell bidi that we need bidirectional
		FriBidiCharType fribidi_chartype = FRIBIDI_TYPE_L;

		// our buffer
		FriBidiChar *logical = (FriBidiChar *)alloca(sizeof(FriBidiChar)*(len + 1));
		FriBidiChar *visual = (FriBidiChar *)alloca(sizeof(FriBidiChar)*(len + 1));

		// convert from the selected charset to Unicode
		rtl_len = fribidi_charset_to_unicode(fribidi_charset, const_cast<char *>(text), len, logical);
		//printf("len: %d rtl_len: %d\n", len, rtl_len);

		// logical to visual
		if (fribidi_log2vis(logical, rtl_len, &fribidi_chartype, visual, NULL, NULL, NULL))
		{
			// removes bidirectional marks
			FriBidiStrIndex __attribute__ ((unused)) idx = fribidi_remove_bidi_marks(visual, rtl_len, NULL, NULL, NULL);

			rtl_text = (char *)alloca(sizeof(char)*(rtl_len * 4 + 1));
			fribidi_unicode_to_charset(fribidi_charset, visual, rtl_len, rtl_text);

			return std::string(rtl_text);
		}
	}

	return std::string(text);
}
#endif

void Font::paintFontPixel(fb_pixel_t *td, uint8_t src)
{
#define DST_BLUE  0x80
#define DST_GREEN 0x80
#define DST_RED   0x80
#define DST_TRANS 0x80
	if (useFullBG) {
		uint8_t *dst = (uint8_t *)td;
		if (*td == (fb_pixel_t)0) {
			*dst = DST_BLUE  + ((fg_blue  - DST_BLUE)  * src) / 256;
			dst++;
			*dst = DST_GREEN + ((fg_green - DST_GREEN) * src) / 256;
			dst++;
			*dst = DST_RED   + ((fg_red   - DST_RED)   * src) / 256;
			dst++;
			*dst = (uint8_t)int_min(255, DST_TRANS + src);
		}
		else {
			*dst = *dst + ((fg_blue  - *dst) * src) / 256;
			dst++;
			*dst = *dst + ((fg_green - *dst) * src) / 256;
			dst++;
			*dst = *dst + ((fg_red   - *dst) * src) / 256;
			dst++;
			*dst = (uint8_t)int_min(0xFF, *dst + src);
		}
	}
	else
		*td = colors[src];
}

void Font::RenderString(int x, int y, const int width, const char *text, const fb_pixel_t color, const int boxheight, const unsigned int flags, fb_pixel_t *buffer, int _stride)
{
	if (!text){
		dprintf(DEBUG_DEBUG,"\033[33m[Font] [%s - %d] ERROR! parameter text = [%p]\033[0m\n", __func__, __LINE__, text);
		return;
	}

	bool render_to_fb = (buffer == NULL);
	if (render_to_fb && !frameBuffer->getActive())
		return;

	fb_pixel_t *buff;
	int stride;
	if (buffer) {
		buff = buffer;
		stride = _stride;
	} else {
		buff = frameBuffer->getFrameBufferPointer();
		stride = frameBuffer->getStride();
	}

	const bool utf8_encoded = flags & IS_UTF8;
#if HAVE_TRIPLEDRAGON
	/* the TD Framebufffer is ARGB; the others are BGRA. The fullbg code does not handle that
	 * the quick workaround is to just disable the fullbg flag */
	useFullBG               = false;
#else
	useFullBG               = flags & FULLBG;
#endif
/*
	useFullBg = false
	 fetch bgcolor from framebuffer, using lower left edge of the font
	  - default render mode
	  - font rendering faster

	useFullBg = true
	 fetch bgcolor from framebuffer, using the respective real fontpixel position
	  - better quality at font rendering on images or background with color gradient
	  - font rendering slower
*/

	if (render_to_fb)
		frameBuffer->checkFbArea(x, y-height, width, height, true);

	pthread_mutex_lock( &renderer->render_mutex );

#ifdef ENABLE_FRIBIDI
	std::string Text = fribidi_shape_char(text);
	text = Text.c_str();
#endif

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
	int pen1 = -1; // "pen" positions for kerning, pen2 is "x"

	fg_red     = (color & 0x00FF0000) >> 16;
	fg_green   = (color & 0x0000FF00) >>  8;
	fg_blue    = color  & 0x000000FF;
	fb_pixel_t bg_color = 0;

	if (y < 0)
		y = 0;

	/* the GXA seems to do it's job asynchonously, so we need to wait until
	   it's ready, otherwise the font will sometimes "be overwritten" with
	   background color or bgcolor will be wrong */
	if (render_to_fb)
		frameBuffer->waitForIdle("Font::RenderString 1");
	if (!useFullBG) {
		/* fetch bgcolor from framebuffer, using lower left edge of the font... */
		bg_color = *(buff + x + y * stride / sizeof(fb_pixel_t));

		if (bg_color == (fb_pixel_t)0)
			bg_color = 0x80808080;

		uint8_t bg_trans = (bg_color & 0xFF000000) >> 24;
		uint8_t bg_red   = (bg_color & 0x00FF0000) >> 16;
		uint8_t bg_green = (bg_color & 0x0000FF00) >>  8;
		uint8_t bg_blue  =  bg_color & 0x000000FF;
		for (int i = 0; i < 256; i++) {
			colors[i] = (((int_min(0xFF, bg_trans + i))           << 24) & 0xFF000000) |
				    (((bg_red  +((fg_red  -bg_red)  * i)/256) << 16) & 0x00FF0000) |
				    (((bg_green+((fg_green-bg_green)* i)/256) <<  8) & 0x0000FF00) |
				     ((bg_blue +((fg_blue -bg_blue) * i)/256)        & 0x000000FF);
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

#if 0
		// width clip
		if (x + glyph->xadvance + spread_by > left + width)
			break;
#endif
		int xoff    = x + glyph->left;
		int ap      = xoff * sizeof(fb_pixel_t) + stride * (y - glyph->top);
		uint8_t * d = ((uint8_t *)buff) + ap;
		uint8_t * s = glyph->buffer;
		int w       = glyph->width;
		int h       = glyph->height;
		int pitch   = glyph->pitch;
		if (ap>-1) {
			for (int ay = 0; ay < h; ay++) {
				fb_pixel_t * td = (fb_pixel_t *)d;
				int ax;
				for (ax = 0; ax < w + spread_by; ax++) {
					/* width clip */
					if (xoff  + ax >= left + width)
						break;
					if (stylemodifier != Font::Embolden) {
						/* do not paint the backgroundcolor (*s = 0) */
						if (*s != 0)
							paintFontPixel(td, *s);
					}
					else {
						int lcolor = -1;
						int start = (ax < w) ? 0 : ax - w + 1;
						int end   = (ax < spread_by) ? ax + 1 : spread_by + 1;
						for (int i = start; i < end; i++)
							if (lcolor < *(s - i))
								lcolor = *(s - i);
						/* do not paint the backgroundcolor (lcolor = 0) */
						if (lcolor != 0)
							paintFontPixel(td, (uint8_t)lcolor);
					}
					td++; s++;
				}
				s += pitch - ax;
				d += stride;
			}
		}
		x += glyph->xadvance + 1;
		if (pen1 > x)
			x = pen1;
		pen1 = x;
		lastindex = index;
	}
	//printf("RenderStat: %d %d %d \n", renderer->cacheManager->num_nodes, renderer->cacheManager->num_bytes, renderer->cacheManager->max_bytes);
	pthread_mutex_unlock( &renderer->render_mutex );
	if (render_to_fb) {
		frameBuffer->checkFbArea(x, y-height, width, height, false);
		/* x is the rightmost position of the last drawn character */
		frameBuffer->mark(left, y + lower - height, x, y + lower);
	}
}

void Font::RenderString(int x, int y, const int width, const std::string & text, const fb_pixel_t color, const int boxheight, const unsigned int flags, fb_pixel_t *buffer, int stride)
{
	RenderString(x, y, width, text.c_str(), color, boxheight, flags, buffer, stride);
}

int Font::getRenderWidth(const char *text, const bool utf8_encoded)
{
	pthread_mutex_lock( &renderer->render_mutex );

#ifdef ENABLE_FRIBIDI
	std::string Text = fribidi_shape_char(text);
	text = Text.c_str();
#endif

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
