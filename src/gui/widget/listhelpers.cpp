/*
	Helper functions for various list implementations in neutrino
	Copyright (C) 2016 Stefan Seyfried

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
	along with this program; if not, see http://www.gnu.org/licenses/
*/

#include <driver/rcinput.h>
#include "listhelpers.h"

#include <global.h>
#include <gui/bouquetlist.h>

template <class T> int CListHelpers::UpDownKey(T list, neutrino_msg_t msg, int lines, int sel)
{
	int step;
	if (list.empty())
		return -1;

	if (msg >= CRCInput::RC_MaxRC) {
		printf("CListHelpers:%s: invalid key? 0x%lx\n", __func__, msg);
		return -1;
	}
	int key = (int)msg;
	if (key == g_settings.key_pageup)
		step = -lines;
	else if (key == g_settings.key_pagedown)
		step = lines;
	else if (msg == CRCInput::RC_up)
		step = -1;
	else if (msg == CRCInput::RC_down)
		step = 1;
	else {
		printf("CListHelpers:%s: invalid key? 0x%lx\n", __func__, msg);
		return -1;
	}
	int size = (int)list.size(); /* bigger than 0, because we checked for empty() before */
	// printf("CListHelpers:%s: key 0x%04lx lines %d size %d sel %d\n", __func__, msg, lines, size, sel);
	int new_sel = sel + step;
	if (new_sel < 0) {
		if (sel != 0 && step != 1)
			new_sel = 0;
		else
			new_sel = size - 1;
	}
	else if (new_sel > size - 1) {
		if (sel != size - 1)
			new_sel = size - 1;
		else
			new_sel = 0;
	}
	return new_sel;
}

/* all used versions need to be prototyped here, to avoid linker errors */
template int CListHelpers::UpDownKey<std::vector<CBouquet*> >(std::vector<CBouquet*>, neutrino_msg_t, int, int);
template int CListHelpers::UpDownKey<std::vector<CZapitBouquet*> >(std::vector<CZapitBouquet*>, neutrino_msg_t, int, int);
template int CListHelpers::UpDownKey<std::vector<CZapitChannel*> >(std::vector<CZapitChannel*>, neutrino_msg_t, int, int);
