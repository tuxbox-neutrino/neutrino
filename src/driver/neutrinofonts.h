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

#ifndef __neutrinofonts__
#define __neutrinofonts__

#include <src/system/settings.h>

typedef struct neutrino_font_descr {
	std::string	name;
	std::string	filename;
	int		size_offset;
} neutrino_font_descr_struct;

typedef struct font_sizes {
	const neutrino_locale_t name;
	const unsigned int      defaultsize;
	const unsigned int      style;
	const unsigned int      size_offset;
} font_sizes_struct;

typedef struct font_sizes_groups {
	const neutrino_locale_t			groupname;
	const unsigned int			count;
	const SNeutrinoSettings::FONT_TYPES	*const content;
	const char * const			actionkey;
	const neutrino_locale_t hint;
} font_sizes_groups_struct;

class CNeutrinoFonts
{
	private:
		std::string fontStyle[3];
		std::string dynFontStyle[3];

		typedef struct dyn_font_t
		{
			int dx;
			int dy;
			int size;
			int style;
			std::string text;
			Font* font;
			bool useDigitOffset;
		} dyn_font_struct_t;

		typedef std::vector<dyn_font_t> v_dyn_fonts_t;
		v_dyn_fonts_t v_share_fonts;
		v_dyn_fonts_t v_dyn_fonts;
		bool useDigitOffset;

		void InitDynFonts();
		void refreshDynFont(int dx, int dy, std::string text, int style, int index, bool isShare);
		int getFontHeight(Font* fnt);
		int getDynFontSize(int dx, int dy, std::string text, int style);
		Font **getDynFontShare(int &dx, int &dy, std::string text, int style);
		Font **getDynFontWithID(int &dx, int &dy, std::string text, int style, unsigned int f_id);

	public:
		enum {
			FONT_STYLE_REGULAR	= 0,
			FONT_STYLE_BOLD		= 1,
			FONT_STYLE_ITALIC	= 2
		};

		enum {
			FONT_ID_SHARE		= -1
		};
		enum {
			FONT_ID_VOLBAR,
			FONT_ID_INFOCLOCK,

			FONT_ID_MAX
		};
		enum {
			FONTSETUP_NEUTRINO_FONT		= 1,	/* refresh neutrino fonts */
			FONTSETUP_NEUTRINO_FONT_INST	= 2,	/* delete & initialize font renderer class */
			FONTSETUP_DYN_FONT		= 4,	/* refresh dynamic fonts */
			FONTSETUP_DYN_FONT_INST		= 8,	/* delete & initialize font renderer class */

			FONTSETUP_ALL			= FONTSETUP_NEUTRINO_FONT | FONTSETUP_NEUTRINO_FONT_INST | FONTSETUP_DYN_FONT | FONTSETUP_DYN_FONT_INST
		};

		CNeutrinoFonts();
		~CNeutrinoFonts();
		static CNeutrinoFonts* getInstance();

		neutrino_font_descr_struct fontDescr;
		neutrino_font_descr_struct old_fontDescr;

		void SetupNeutrinoFonts(bool initRenderClass = true);
		void SetupDynamicFonts(bool initRenderClass = true);
		void refreshDynFonts();
		Font **getDynFont(int &dx, int &dy, std::string text="", int style=FONT_STYLE_REGULAR, int share=FONT_ID_SHARE);
		void setFontUseDigitHeight(bool set=true) {useDigitOffset = set;}
};


#endif //__neutrinofonts__
