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


#ifndef __LISTHELPERS__
#define __LISTHELPERS__

class CListHelpers
{
	public:
		template <class T> int UpDownKey(T list, neutrino_msg_t k, int lines, int sel);
};
#endif
