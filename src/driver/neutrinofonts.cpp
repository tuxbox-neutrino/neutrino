/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'

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
	memset(&fontDescr, 0, sizeof(neutrino_font_descr_struct));
}

CNeutrinoFonts::~CNeutrinoFonts()
{
}

CNeutrinoFonts* CNeutrinoFonts::getInstance()
{
	static CNeutrinoFonts* nf = NULL;
	if (!nf)
		nf = new CNeutrinoFonts();
	return nf;
}

void CNeutrinoFonts::SetupNeutrinoFonts()
{
	if (g_fontRenderer != NULL)
		delete g_fontRenderer;
	g_fontRenderer = new FBFontRenderClass(72 * g_settings.screen_xres / 100, 72 * g_settings.screen_yres / 100);

	if (fontDescr.filename != NULL)
		free((void *)fontDescr.filename);
	printf("[neutrino] settings font file %s\n", g_settings.font_file);
	if (access(g_settings.font_file, F_OK)) {
		if (!access(FONTDIR"/neutrino.ttf", F_OK)) {
			fontDescr.filename = strdup(FONTDIR"/neutrino.ttf");
			strcpy(g_settings.font_file, fontDescr.filename);
		} else {
			fprintf( stderr,"[neutrino] font file [%s] not found\n neutrino exit\n",FONTDIR"/neutrino.ttf");
			_exit(0);
		}

	} else
		fontDescr.filename = strdup(g_settings.font_file);
	fontStyle[0] = g_fontRenderer->AddFont(fontDescr.filename);

	if (fontDescr.name != NULL)
		free((void *)fontDescr.name);
	fontDescr.name = strdup(g_fontRenderer->getFamily(fontDescr.filename).c_str());
	printf("[neutrino] font family %s\n", fontDescr.name);
	fontStyle[1] = "Bold Regular";

	g_fontRenderer->AddFont(fontDescr.filename, true);  // make italics
	fontStyle[2] = "Italic";

	for (int i = 0; i < SNeutrinoSettings::FONT_TYPE_COUNT; i++) {
		if (g_Font[i]) delete g_Font[i];
		g_Font[i] = g_fontRenderer->getFont(fontDescr.name, fontStyle[neutrino_font[i].style], CNeutrinoApp::getInstance()->getConfigFile()->getInt32(locale_real_names[neutrino_font[i].name], neutrino_font[i].defaultsize) + neutrino_font[i].size_offset * fontDescr.size_offset);
	}
	g_SignalFont = g_fontRenderer->getFont(fontDescr.name, fontStyle[signal_font.style], signal_font.defaultsize + signal_font.size_offset * fontDescr.size_offset);
}
