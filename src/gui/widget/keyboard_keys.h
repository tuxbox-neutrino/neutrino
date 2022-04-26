/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2014 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


static std::string keys_english[2][KEY_ROWS][KEY_COLUMNS] =
{
	{
		{ "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-",  "=",  "§"  },
		{ "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[",  "]",  "{", "}"  },
		{ "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "\'", "\\", ":", "\"" },
		{ "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "<",  ">",  "?", " "  }
	},
	{
		{ "~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_",  "+",  "§", },
		{ "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{",  "}",  "{", "}"  },
		{ "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "\'", "\\", ":", "\"" },
		{ "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "<",  ">",  "?", " "  }
	}
};

static std::string keys_deutsch[2][KEY_ROWS][KEY_COLUMNS] =
{
	{
		{ "°", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "ß", "´", "@" },
		{ "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "ü", "+", "~", "/" },
		{ "a", "s", "d", "f", "g", "h", "j", "k", "l", "ö", "ä", "#", "[", "]" },
		{ "y", "x", "c", "v", "b", "n", "m", ",", ".", "-", "|", "<", ">", " " }
	},
	{
		{ "^", "!", "\"","§", "$", "%", "&", "/", "(", ")", "=", "?", "`", "€" },
		{ "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü", "*", "\\","/" },
		{ "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ö", "Ä", "\'","{", "}" },
		{ "Y", "X", "C", "V", "B", "N", "M", ";", ":", "_", "²", "³", "µ", " " }
	}
};

struct keyboard_layout keyboards[] =
{
	{ "English", "english", keys_english },
	{ "Deutsch", "deutsch", keys_deutsch }
};
#define LAYOUT_COUNT (sizeof(keyboards)/sizeof(struct keyboard_layout))
