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
#include <system/debug.h>
#include <system/settings.h>

#include <configfile.h>

extern font_sizes_groups_struct font_sizes_groups[];
extern font_sizes_struct neutrino_font[];
extern const char * locale_real_names[]; /* #include <system/locals_intern.h> */

font_sizes_struct fixed_font[SNeutrinoSettings::FONT_TYPE_FIXED_COUNT] =
{
	{NONEXISTANT_LOCALE, 30, CNeutrinoFonts::FONT_STYLE_BOLD,    1},
	{NONEXISTANT_LOCALE, 30, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{NONEXISTANT_LOCALE, 30, CNeutrinoFonts::FONT_STYLE_ITALIC,  1},
	{NONEXISTANT_LOCALE, 20, CNeutrinoFonts::FONT_STYLE_BOLD,    1},
	{NONEXISTANT_LOCALE, 20, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{NONEXISTANT_LOCALE, 20, CNeutrinoFonts::FONT_STYLE_ITALIC,  1},
	{NONEXISTANT_LOCALE, 16, CNeutrinoFonts::FONT_STYLE_BOLD,    1},
	{NONEXISTANT_LOCALE, 16, CNeutrinoFonts::FONT_STYLE_REGULAR, 1},
	{NONEXISTANT_LOCALE, 16, CNeutrinoFonts::FONT_STYLE_ITALIC,  1},
};

const font_sizes_struct signal_font = {NONEXISTANT_LOCALE, 14, CNeutrinoFonts::FONT_STYLE_REGULAR, 1};
const font_sizes_struct shell_font = {NONEXISTANT_LOCALE, 18, CNeutrinoFonts::FONT_STYLE_REGULAR, 1};

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
	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_FIXED_COUNT; i++)
		g_FixedFont[i] = NULL;

	g_SignalFont = NULL;
	g_ShellFont = NULL;

	InitDynFonts();
}

void CNeutrinoFonts::clearDynFontStruct(dyn_font_t* f)
{
	f->dx			= 0;
	f->dy			= 0;
	f->size			= 0;
	f->style		= 0;
	f->text			= "";
	f->font			= NULL;
	f->useDigitOffset	= false;
}

void CNeutrinoFonts::InitDynFonts()
{
	for (int i = 0; i < FONT_ID_MAX; i++) {
		dyn_font_t dyn_font;
		clearDynFontStruct(&dyn_font);
		v_dyn_fonts.push_back(dyn_font);
	}
}

CNeutrinoFonts::~CNeutrinoFonts()
{
	if (!v_share_fonts.empty()) {
		for (unsigned int i = 0; i < v_share_fonts.size(); i++){
			delete v_share_fonts[i].font;
			v_share_fonts[i].font = NULL;
		}
		v_share_fonts.clear();
	}

	if (!v_dyn_fonts.empty()) {
		for (unsigned int i = 0; i < v_dyn_fonts.size(); i++){
			delete v_dyn_fonts[i].font;
			v_dyn_fonts[i].font = NULL;
		}
		v_dyn_fonts.clear();
	}
	if (!vDynSize.empty()) {
		vDynSize.clear();
	}
	deleteDynFontExtAll();
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
		if (!vDynSize.empty()) {
			vDynSize.clear();
		}
		dprintf(DEBUG_NORMAL, "[CNeutrinoFonts] [%s - %d] dynamic font family: %s\n", __func__, __LINE__, fontDescr.name.c_str());
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
		g_fontRenderer = new FBFontRenderClass(72 * g_settings.font_scaling_x / 100, 72 * g_settings.font_scaling_y / 100);

		old_fontDescr.size_offset = fontDescr.size_offset;
		old_fontDescr.filename = fontDescr.filename;
		fontDescr.filename = "";
		dprintf(DEBUG_NORMAL, "[CNeutrinoFonts] [%s - %d] font file: %s\n", __func__, __LINE__, g_settings.font_file.c_str());
		if (access(g_settings.font_file.c_str(), F_OK)) {
			if (!access(FONTDIR"/neutrino.ttf", F_OK)) {
				fontDescr.filename = FONTDIR"/neutrino.ttf";
				g_settings.font_file = fontDescr.filename;
			}
			else {
				fprintf( stderr,"[CNeutrinoFonts] [%s - %d] font file [%s] not found\n neutrino exit\n", __func__, __LINE__, FONTDIR"/neutrino.ttf");
				_exit(0);
			}
		}
		else
			fontDescr.filename = g_settings.font_file;

		fontStyle[0] = g_fontRenderer->AddFont(fontDescr.filename.c_str());

		old_fontDescr.name = fontDescr.name;
		fontDescr.name = "";
		fontDescr.name = g_fontRenderer->getFamily(fontDescr.filename.c_str());
		if (!vDynSize.empty()) {
			vDynSize.clear();
		}
		dprintf(DEBUG_NORMAL, "[CNeutrinoFonts] [%s - %d] standard font family: %s\n", __func__, __LINE__, fontDescr.name.c_str());
		fontStyle[1] = "Bold Regular";

		g_fontRenderer->AddFont(fontDescr.filename.c_str(), true);  // make italics
		fontStyle[2] = "Italic";

		if (g_fixedFontRenderer != NULL)
			delete g_fixedFontRenderer;
		g_fixedFontRenderer = new FBFontRenderClass();

		g_fixedFontRenderer->AddFont(fontDescr.filename.c_str());
		g_fixedFontRenderer->AddFont(fontDescr.filename.c_str(), true); // make italics
	}

	int fontSize;
	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_COUNT; i++)
	{
		if (g_Font[i])
			delete g_Font[i];
		fontSize = CFrameBuffer::getInstance()->scale2Res(CNeutrinoApp::getInstance()->getConfigFile()->getInt32(locale_real_names[neutrino_font[i].name], neutrino_font[i].defaultsize)) + neutrino_font[i].size_offset * fontDescr.size_offset;
		g_Font[i] = g_fontRenderer->getFont(fontDescr.name.c_str(), fontStyle[neutrino_font[i].style].c_str(), fontSize);
	}
	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_FIXED_COUNT; i++)
	{
		if (g_FixedFont[i])
			delete g_FixedFont[i];
		fontSize = CFrameBuffer::getInstance()->scale2Res(fixed_font[i].defaultsize) + fixed_font[i].size_offset * fontDescr.size_offset;
		g_FixedFont[i] = g_fixedFontRenderer->getFont(fontDescr.name.c_str(), fontStyle[fixed_font[i].style].c_str(), fontSize);
	}
	if (g_SignalFont)
		delete g_SignalFont;
	fontSize = CFrameBuffer::getInstance()->scale2Res(signal_font.defaultsize) + signal_font.size_offset * fontDescr.size_offset;
	g_SignalFont = g_fontRenderer->getFont(fontDescr.name.c_str(), fontStyle[signal_font.style].c_str(), fontSize);
}

std::string CNeutrinoFonts::getShellTTF()
{
	const char *shell_ttf[2] = { FONTDIR_VAR "/shell.ttf", FONTDIR "/shell.ttf" };
	for (unsigned int i = 0; i < 2; i++)
	{
		if (access(shell_ttf[i], F_OK) == 0)
			return (std::string)shell_ttf[i];
	}
	return "";
}

void CNeutrinoFonts::SetupShellFont()
{
	if (g_ShellFont)
	{
		delete g_ShellFont;
		g_ShellFont = NULL;
	}

	std::string shell_ttf = getShellTTF();
	if (shell_ttf.empty())
		return;

	if (g_shellFontRenderer != NULL)
		delete g_shellFontRenderer;
	g_shellFontRenderer = new FBFontRenderClass(72 * g_settings.font_scaling_x / 100, 72 * g_settings.font_scaling_y / 100);
	g_shellFontRenderer->AddFont(shell_ttf.c_str());

	std::string shell_font_name = g_shellFontRenderer->getFamily(shell_ttf.c_str());
	int shell_font_size = CFrameBuffer::getInstance()->scale2Res(shell_font.defaultsize)/* + shell_font.size_offset * fontDescr.size_offset*/;
	g_ShellFont = g_shellFontRenderer->getFont(shell_font_name.c_str(), fontStyle[shell_font.style].c_str(), shell_font_size);
	if (g_ShellFont)
		dprintf(DEBUG_NORMAL, "[CNeutrinoFonts] [%s - %d] shell font family: %s (%s)\n", __func__, __LINE__, shell_font_name.c_str(), shell_ttf.c_str());
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
	if (dyn_font->size != dynSize){
		dprintf(DEBUG_NORMAL, "[CNeutrinoFonts] [%s - %d]  change %s_font size old %d to new %d, index: %u\n", __func__, __LINE__, (isShare)?"share":"dyn", oldSize, dyn_font->size, index);
	}else{
		dprintf(DEBUG_NORMAL, "[CNeutrinoFonts] [%s - %d]  refresh %s_font size %d, index: %u\n",__func__, __LINE__, (isShare)?"share":"dyn", dyn_font->size, index);
	}
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
	int dynSize	= dy/1.6;
	if (dx == 0) dx = CFrameBuffer::getInstance()->getScreenWidth(true);

	if (!vDynSize.empty()) {
		for (size_t i = 0; i < vDynSize.size(); i++) {
			if ((vDynSize[i].dy == dy) &&
			    (vDynSize[i].dx == dx) &&
			    (vDynSize[i].style == style) &&
			    (vDynSize[i].text == text)) {
				dynSize = vDynSize[i].dynsize;
				return dynSize;
			}
		}
	}
	Font *dynFont	= NULL;
	bool dynFlag	= false;
	while (1) {
		if (dynFont)
			delete dynFont;
		dynFont = g_dynFontRenderer->getFont(fontDescr.name.c_str(), dynFontStyle[style].c_str(), dynSize);
		// calculate height & width
		int _width 	= 0;
		int _height 	= getFontHeight(dynFont);;

		std::string tmpText = text;
		if (text.empty()) tmpText = "x";
		_width = dynFont->getRenderWidth(tmpText);
		if ((_height > dy) || (_width > dx)) {
			if (dynFlag){
				dynSize--;
			}else{
				if (debug)
					printf("##### [%s] Specified size (dx=%d, dy=%d) too small, use minimal font size.\n", __FUNCTION__, dx, dy);
			}
			break;
		}
		else if ((_height < dy) || (_width < dx)) {
			dynFlag = true;
			dynSize++;
		}
		else
			break;
	}

	if (dynFont){
		delete dynFont;

		if (!vDynSize.empty() && vDynSize.size() > 99) {
			vDynSize.clear();
		}
		if(dynSize){
			dyn_size_t v;
			v.dx		= dx;
			v.dy		= dy;
			v.dynsize	= dynSize;
			v.style		= style;
			v.text		= text;
			vDynSize.push_back(v);
		}
	}

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
	if ((fontDescr.name.empty()) || (fontDescr.filename.empty()))
		SetupNeutrinoFonts();
	if (g_dynFontRenderer == NULL)
		SetupDynamicFonts();

	int dynSize = getDynFontSize(dx, dy, text, style);
	Font *dynFont = NULL;
	Font **ret = NULL;

	if (f_id < v_dyn_fonts.size()) {
		if ((v_dyn_fonts[f_id].size == dynSize) && (v_dyn_fonts[f_id].font != NULL)) {
			dy = v_dyn_fonts[f_id].font->getHeight();
			if (!text.empty())
				dx = v_dyn_fonts[f_id].font->getRenderWidth(text);
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
	if (!text.empty())
		dx = (*ret)->getRenderWidth(text);
#ifdef DEBUG_NFONTS
	printf("##### [%s] dx: %d, dy: %d, dynSize: %d, dynFont: %p, ret: %p, FontID: %d\n", __FUNCTION__, dx, dy, dynSize, *ret, ret, f_id);
#endif
	return ret;
}

void CNeutrinoFonts::initDynFontExt()
{
	for (int i = 0; i < DYNFONTEXT_MAX; i++) {
		dyn_font_t dyn_font;
		clearDynFontStruct(&dyn_font);
		v_dyn_fonts_ext.push_back(dyn_font);
	}
}

void CNeutrinoFonts::deleteDynFontExtAll()
{
	if (!v_dyn_fonts_ext.empty()) {
		for (size_t i = 0; i < v_dyn_fonts_ext.size(); ++i) {
			if (v_dyn_fonts_ext[i].font != NULL){
				delete v_dyn_fonts_ext[i].font;
				v_dyn_fonts_ext[i].font = NULL;
			}
		}
		v_dyn_fonts_ext.clear();
	}
}

Font *CNeutrinoFonts::getDynFontExt(int &dx, int &dy, unsigned int f_id, std::string text/*=""*/, int style/*=FONT_STYLE_REGULAR*/)
{
	if ((dx <= 0) && (dy <= 0))
		return NULL;
	if ((fontDescr.name.empty()) || (fontDescr.filename.empty()))
		SetupNeutrinoFonts();
	if (g_dynFontRenderer == NULL)
		SetupDynamicFonts();
	if (v_dyn_fonts_ext.empty())
		initDynFontExt();

	int dynSize = getDynFontSize(dx, dy, text, style);
	Font *dynFont = NULL;
	Font *ret = NULL;

	if (f_id < v_dyn_fonts_ext.size()) {
		dynFont = g_dynFontRenderer->getFont(fontDescr.name.c_str(), dynFontStyle[style].c_str(), dynSize);
		if (v_dyn_fonts_ext[f_id].font != NULL)
			delete v_dyn_fonts_ext[f_id].font;
		v_dyn_fonts_ext[f_id].dx		= dx;
		v_dyn_fonts_ext[f_id].dy		= dy;
		v_dyn_fonts_ext[f_id].size		= dynSize;
		v_dyn_fonts_ext[f_id].style		= style;
		v_dyn_fonts_ext[f_id].text		= text;
		v_dyn_fonts_ext[f_id].font		= dynFont;
		v_dyn_fonts_ext[f_id].useDigitOffset	= useDigitOffset;
		ret = v_dyn_fonts_ext[f_id].font;
	}
	else
		return NULL;

	dy = ret->getHeight();
	if (!text.empty())
		dx = ret->getRenderWidth(text);
#ifdef DEBUG_NFONTS
	printf("##### [%s] dx: %d, dy: %d, dynSize: %d, dynFont: %p, ret: %p, FontID: %d\n", __FUNCTION__, dx, dy, dynSize, *ret, ret, f_id);
#endif
	return ret;
}

Font **CNeutrinoFonts::getDynFontShare(int &dx, int &dy, std::string text, int style)
{
	if ((dx <= 0) && (dy <= 0))
		return NULL;
	if ((fontDescr.name.empty()) || (fontDescr.filename.empty()) || (g_dynFontRenderer == NULL))
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
	if (!text.empty())
		dx = (*ret)->getRenderWidth(text);
#ifdef DEBUG_NFONTS
	printf("##### [%s] dx: %d, dy: %d, dynSize: %d, dynFont: %p, ret: %p, fontAvailable: %d\n", __FUNCTION__, dx, dy, dynSize, *ret, ret, fontAvailable);
#endif
	return ret;
}
