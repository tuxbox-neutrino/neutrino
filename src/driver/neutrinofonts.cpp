/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'

	CNeutrinoFonts class for gui.
	Copyright (C) 2013, M. Liebmann (micha-bbg)

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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <string>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/neutrinofonts.h>
#include <system/settings.h>

#include <configfile.h>

extern font_sizes_groups_struct font_sizes_groups[];
extern font_sizes_struct neutrino_font[];
extern const char * locale_real_names[]; /* #include <system/locals_intern.h> */

const font_sizes_struct signal_font = {LOCALE_FONTSIZE_INFOBAR_SMALL, 14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1};

CNeutrinoFonts::CNeutrinoFonts()
{
	useDigitOffset  = false;

	fontDescr.name		  = "";
	fontDescr.filename	  = "";
	fontDescr.size_offset	  = 0;
	old_fontDescr.name	  = "";
	old_fontDescr.filename	  = "";
	old_fontDescr.size_offset = 0;

	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_COUNT; i++)
		g_Font[i] = NULL;

	g_SignalFont = NULL;

	InitDynFonts();
}

void CNeutrinoFonts::InitDynFonts()
{
	for (int i = 0; i < FONT_ID_MAX; i++) {
		dyn_font_t dyn_font;
		dyn_font.dx		= 0;
		dyn_font.dy		= 0;
		dyn_font.size		= 0;
		dyn_font.style		= 0;
		dyn_font.text		= "";
		dyn_font.font		= NULL;
		dyn_font.useDigitOffset	= false;
		v_dyn_fonts.push_back(dyn_font);
	}
}

CNeutrinoFonts::~CNeutrinoFonts()
{
	if (!v_share_fonts.empty()) {
		for (unsigned int i = 0; i < v_share_fonts.size(); i++)
			delete v_share_fonts[i].font;
		v_share_fonts.clear();
	}

	if (!v_dyn_fonts.empty()) {
		for (unsigned int i = 0; i < v_dyn_fonts.size(); i++)
			delete v_dyn_fonts[i].font;
		v_dyn_fonts.clear();
	}
}

CNeutrinoFonts* CNeutrinoFonts::getInstance()
{
	static CNeutrinoFonts* nf = NULL;
	if (!nf)
		nf = new CNeutrinoFonts();
	return nf;
}

void CNeutrinoFonts::SetupDynamicFonts(bool initRenderClass/*=true*/)
{
	if (initRenderClass) {
		if (g_dynFontRenderer != NULL)
			delete g_dynFontRenderer;
		g_dynFontRenderer = new FBFontRenderClass();

		dynFontStyle[0] = g_dynFontRenderer->AddFont(fontDescr.filename.c_str());

		fontDescr.name = g_dynFontRenderer->getFamily(fontDescr.filename.c_str());
		printf("[neutrino] font family %s\n", fontDescr.name.c_str());
		dynFontStyle[1] = "Bold Regular";

		g_dynFontRenderer->AddFont(fontDescr.filename.c_str(), true);  // make italics
		dynFontStyle[2] = "Italic";
	}
}

void CNeutrinoFonts::SetupNeutrinoFonts(bool initRenderClass/*=true*/)
{
	if (initRenderClass) {
		if (g_fontRenderer != NULL)
			delete g_fontRenderer;
		g_fontRenderer = new FBFontRenderClass(72 * g_settings.screen_xres / 100, 72 * g_settings.screen_yres / 100);

		old_fontDescr.size_offset = fontDescr.size_offset;
		old_fontDescr.filename = fontDescr.filename;
		fontDescr.filename = "";
		printf("[neutrino] settings font file %s\n", g_settings.font_file);
		if (access(g_settings.font_file, F_OK)) {
			if (!access(FONTDIR"/neutrino.ttf", F_OK)) {
				fontDescr.filename = FONTDIR"/neutrino.ttf";
				strcpy(g_settings.font_file, fontDescr.filename.c_str());
			}
			else {
				fprintf( stderr,"[neutrino] font file [%s] not found\n neutrino exit\n",FONTDIR"/neutrino.ttf");
				_exit(0);
			}
		}
		else
			fontDescr.filename = g_settings.font_file;

		fontStyle[0] = g_fontRenderer->AddFont(fontDescr.filename.c_str());

		old_fontDescr.name = fontDescr.name;
		fontDescr.name = "";
		fontDescr.name = g_fontRenderer->getFamily(fontDescr.filename.c_str());
		printf("[neutrino] font family %s\n", fontDescr.name.c_str());
		fontStyle[1] = "Bold Regular";

		g_fontRenderer->AddFont(fontDescr.filename.c_str(), true);  // make italics
		fontStyle[2] = "Italic";
	}

	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_COUNT; i++) {
		if (g_Font[i]) delete g_Font[i];
		g_Font[i] = g_fontRenderer->getFont(fontDescr.name.c_str(), fontStyle[neutrino_font[i].style].c_str(), CNeutrinoApp::getInstance()->getConfigFile()->getInt32(locale_real_names[neutrino_font[i].name], neutrino_font[i].defaultsize) + neutrino_font[i].size_offset * fontDescr.size_offset);
	}
	if (g_SignalFont) delete g_SignalFont;
	g_SignalFont = g_fontRenderer->getFont(fontDescr.name.c_str(), fontStyle[signal_font.style].c_str(), signal_font.defaultsize + signal_font.size_offset * fontDescr.size_offset);
}

void CNeutrinoFonts::refreshDynFonts()
{
	if (!v_share_fonts.empty()) {
		for (unsigned int i = 0; i < v_share_fonts.size(); i++) {
			if (v_share_fonts[i].font != NULL)
				refreshDynFont(v_share_fonts[i].dx, v_share_fonts[i].dy, v_share_fonts[i].text, v_share_fonts[i].style, i, true);
		}
	}

	if (!v_dyn_fonts.empty()) {
		for (unsigned int i = 0; i < v_dyn_fonts.size(); i++) {
			if (v_dyn_fonts[i].font != NULL)
				refreshDynFont(v_dyn_fonts[i].dx, v_dyn_fonts[i].dy, v_dyn_fonts[i].text, v_dyn_fonts[i].style, i, false);
		}
	}

	old_fontDescr.filename    = fontDescr.filename;
	old_fontDescr.name        = fontDescr.name;
	old_fontDescr.size_offset = fontDescr.size_offset;
}

void CNeutrinoFonts::refreshDynFont(int dx, int dy, std::string text, int style, int index, bool isShare)
{
	if ((dx <= 0) && (dy <= 0))
		return;

	dyn_font_t *dyn_font = (isShare) ? &(v_share_fonts[index]) : &(v_dyn_fonts[index]);
	int oldSize = dyn_font->size;
	bool tmp = useDigitOffset;
	useDigitOffset = dyn_font->useDigitOffset;
	int dynSize = getDynFontSize(dx, dy, text, style);
	useDigitOffset = tmp;
//	if ((dyn_font->size == dynSize) && (old_fontDescr.name == fontDescr.name) && (old_fontDescr.filename == fontDescr.filename))
//		return;

	if (dyn_font->font != NULL)
		delete dyn_font->font;
	Font *dynFont	= g_dynFontRenderer->getFont(fontDescr.name.c_str(), dynFontStyle[style].c_str(), dynSize);
	dyn_font->font = dynFont;
	dyn_font->size = dynSize;
	if (dyn_font->size != dynSize)
		printf("##### [%s] change %s_font size old %d to new %d, index: %u\n", __FUNCTION__, (isShare)?"share":"dyn", oldSize, dyn_font->size, index);
	else
		printf("##### [%s] refresh %s_font size %d, index: %u\n", __FUNCTION__, (isShare)?"share":"dyn", dyn_font->size, index);
}

int CNeutrinoFonts::getFontHeight(Font* fnt)
{
	if (useDigitOffset)
		return fnt->getDigitHeight() + (fnt->getDigitOffset() * 18) / 10;
	else
		return fnt->getHeight();
}

int CNeutrinoFonts::getDynFontSize(int dx, int dy, std::string text, int style)
{
	Font *dynFont	= NULL;
	int dynSize	= 8;
	bool dynFlag	= false;

	if (dx == 0) dx = 1280;
	while (1) {
		if (dynFont != NULL)
			delete dynFont;
		dynFont = g_dynFontRenderer->getFont(fontDescr.name.c_str(), dynFontStyle[style].c_str(), dynSize);
		int _width	= 0;
		int _height	= 0;
		// calculate height & width
		_height = getFontHeight(dynFont);
		std::string tmpText = text;
		if (text == "") tmpText = "x";
		_width = dynFont->getRenderWidth(tmpText.c_str());
		if ((_height > dy) || (_width > dx)) {
			if (dynFlag)
				dynSize--;
			else
				printf("##### [%s] Specified size (dx=%d, dy=%d) too small, use minimal font size.\n", __FUNCTION__, dx, dy);
			break;
		}
		else if ((_height < dy) || (_width < dx)) {
			dynFlag = true;
			dynSize++;
		}
		else
			break;
	}

	if (dynFont != NULL)
		delete dynFont;

	return dynSize;
}

/* CNeutrinoFonts::getDynFont usage

 * dx, dy	max. width/height of text field / return: real width/height of text field
		If dx = 0, then the width is calculated automatically

 * text		Text to display
		If text = "", then only the height is calculated

 * style	Font style (FONT_STYLE_REGULAR or FONT_STYLE_BOLD or FONT_STYLE_ITALIC)

 * share	Select font modus
		FONT_ID_SHARE	Font for used by several objects simultaneously
		FONT_ID_xxx	Font for exclusive application
		FONT_ID_yyy		  - "" -
		FONT_ID_zzz		  - "" -

 * return value: Pointer to dynamic font

 example:
   dx = 0;		//dx = 0, width is calculated automatically
   dy = 30;		//max. height
   text = "100";	//max. text to display
			//CNeutrinoFonts::FONT_STYLE_REGULAR = normal font style
			//CNeutrinoFonts::FONT_ID_VOLBAR     = exclusive font for Volume bar (defined in src/driver/neutrinofonts.h)
   Font** font = CNeutrinoFonts::getInstance()->getDynFont(dx, dy, text, CNeutrinoFonts::FONT_STYLE_REGULAR, CNeutrinoFonts::FONT_ID_VOLBAR);
   (*font)->RenderString(...)

*/
Font **CNeutrinoFonts::getDynFont(int &dx, int &dy, std::string text/*=""*/, int style/*=FONT_STYLE_REGULAR*/, int share/*=FONT_ID_SHARE*/)
{
	if (share > FONT_ID_SHARE)
		return getDynFontWithID(dx, dy, text, style, share);
	else
		return getDynFontShare(dx, dy, text, style);
}

Font **CNeutrinoFonts::getDynFontWithID(int &dx, int &dy, std::string text, int style, unsigned int f_id)
{
	if ((dx <= 0) && (dy <= 0))
		return NULL;
	if ((fontDescr.name == "") || (fontDescr.filename == ""))
		SetupNeutrinoFonts();
	if (g_dynFontRenderer == NULL)
		SetupDynamicFonts();

	int dynSize = getDynFontSize(dx, dy, text, style);
	Font *dynFont = NULL;
	Font **ret = NULL;

	if (f_id < v_dyn_fonts.size()) {
		if ((v_dyn_fonts[f_id].size == dynSize) && (v_dyn_fonts[f_id].font != NULL)) {
			dy = v_dyn_fonts[f_id].font->getHeight();
			if (text != "")
				dx = v_dyn_fonts[f_id].font->getRenderWidth(text.c_str());
			return &(v_dyn_fonts[f_id].font);
		}

		dynFont = g_dynFontRenderer->getFont(fontDescr.name.c_str(), dynFontStyle[style].c_str(), dynSize);
		if (v_dyn_fonts[f_id].font != NULL)
			delete v_dyn_fonts[f_id].font;
		v_dyn_fonts[f_id].dx			= dx;
		v_dyn_fonts[f_id].dy			= dy;
		v_dyn_fonts[f_id].size			= dynSize;
		v_dyn_fonts[f_id].style			= style;
		v_dyn_fonts[f_id].text			= text;
		v_dyn_fonts[f_id].font			= dynFont;
		v_dyn_fonts[f_id].useDigitOffset	= useDigitOffset;
		ret = &(v_dyn_fonts[f_id].font);
	}
	else
		return NULL;

	dy = (*ret)->getHeight();
	if (text != "")
		dx = (*ret)->getRenderWidth(text.c_str());
#ifdef DEBUG_NFONTS
	printf("##### [%s] dx: %d, dy: %d, dynSize: %d, dynFont: %p, ret: %p, FontID: %d\n", __FUNCTION__, dx, dy, dynSize, *ret, ret, f_id);
#endif
	return ret;
}

Font **CNeutrinoFonts::getDynFontShare(int &dx, int &dy, std::string text, int style)
{
	if ((dx <= 0) && (dy <= 0))
		return NULL;
	if ((fontDescr.name == "") || (fontDescr.filename == "") || (g_dynFontRenderer == NULL))
		SetupNeutrinoFonts();

	int dynSize = getDynFontSize(dx, dy, text, style);
	Font *dynFont = NULL;

	bool fontAvailable = false;
	unsigned int i;
	Font **ret = NULL;
	if (!v_share_fonts.empty()) {
		for (i = 0; i < v_share_fonts.size(); i++) {
			if ((v_share_fonts[i].size == dynSize) &&
			    (v_share_fonts[i].style == style) &&
			    (v_share_fonts[i].useDigitOffset == useDigitOffset)) {
				fontAvailable = true;
				break;
			}
		}
	}
	if (fontAvailable) {
		if (text.length() > v_share_fonts[i].text.length()) {
			v_share_fonts[i].dx   = dx;
			v_share_fonts[i].dy   = dy;
			v_share_fonts[i].text = text;
		}
		ret = &(v_share_fonts[i].font);
	}
	else {
		dynFont			= g_dynFontRenderer->getFont(fontDescr.name.c_str(), dynFontStyle[style].c_str(), dynSize);
		dyn_font_t dyn_font;
		dyn_font.dx		= dx;
		dyn_font.dy		= dy;
		dyn_font.size		= dynSize;
		dyn_font.style		= style;
		dyn_font.text		= text;
		dyn_font.font		= dynFont;
		dyn_font.useDigitOffset	= useDigitOffset;
		v_share_fonts.push_back(dyn_font);
		ret = &(v_share_fonts[v_share_fonts.size()-1].font);
	}

	dy = (*ret)->getHeight();
	if (text != "")
		dx = (*ret)->getRenderWidth(text.c_str());
#ifdef DEBUG_NFONTS
	printf("##### [%s] dx: %d, dy: %d, dynSize: %d, dynFont: %p, ret: %p, fontAvailable: %d\n", __FUNCTION__, dx, dy, dynSize, *ret, ret, fontAvailable);
#endif
	return ret;
}
