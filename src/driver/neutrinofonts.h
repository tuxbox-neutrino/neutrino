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

#ifndef __neutrinofonts__
#define __neutrinofonts__

typedef struct neutrino_font_descr {
	const char	*name;
	const char	*filename;
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
		const char * fontStyle[3];

	public:
		enum {
			FONT_STYLE_REGULAR	= 0,
			FONT_STYLE_BOLD		= 1,
			FONT_STYLE_ITALIC	= 2
		};

		CNeutrinoFonts();
		~CNeutrinoFonts();
		static CNeutrinoFonts* getInstance();

		neutrino_font_descr_struct fontDescr;

		void SetupNeutrinoFonts();
};


#endif //__neutrinofonts__
