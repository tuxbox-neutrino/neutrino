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
/* allow to trick the compiler into overriding template definitions */
template<typename T> struct _id { typedef T type; };

class CListHelpers
{
	public:
		template <typename T> int UpDownKey(T list, neutrino_msg_t k, int lines, int sel) {
			return _UpDownKey(list, k, lines, sel, _id<T>());
		}
	private:
		/* stackoverflow.com/questions/3052579 */
		template <typename T> int _UpDownKey(T list, neutrino_msg_t k, int lines, int sel, _id<T>);
		int _UpDownKey(int list, neutrino_msg_t k, int lines, int sel, _id<int>);
};
#endif
