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

#ifndef __bouqueteditor_globals__
#define __bouqueteditor_globals__

#include <driver/fontrenderer.h>
#include <gui/components/cc.h>

class CFrameBuffer;

class CBEGlobals
{
	public:
		CBEGlobals();
		~CBEGlobals();

		CFrameBuffer *frameBuffer;

		int x;
		int y;
		int width;
		int height;

		int header_height;
		int body_height;
		int item_height;
		int footer_height;
		int info_height;

		CComponentsHeader header;
		CComponentsFooter footer;

		unsigned int items_count;

		Font *item_font;
		Font *info_font;

		int action_icon_width;
		int status_icon_width;
};

#endif
