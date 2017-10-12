/*
	neutrino bouquet editor - globals

	Copyright (C) 2017 Sven Hoefer

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include <driver/screen_max.h>

#include "bouqueteditor_globals.h"


CBEGlobals::CBEGlobals()
{
	frameBuffer = CFrameBuffer::getInstance();

	item_font = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST];
	info_font = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR];

	width  = frameBuffer->getScreenWidthRel();
	height = frameBuffer->getScreenHeightRel();

	header_height = header.getHeight();
	item_height = item_font->getHeight();
	footer_height = footer.getHeight();
	info_height = 2*info_font->getHeight() + 2*OFFSET_INNER_SMALL;

	items_count = (height - header_height - footer_height - OFFSET_INTER - info_height - 2*OFFSET_SHADOW) / item_height;
	body_height = items_count*item_height;
	height = header_height + body_height + footer_height + OFFSET_INTER + info_height + 2*OFFSET_SHADOW; // recalc height

        x = getScreenStartX(width);
        y = getScreenStartY(height);

	timeout = g_settings.timing[SNeutrinoSettings::TIMING_MENU];
}

CBEGlobals::~CBEGlobals()
{
}
