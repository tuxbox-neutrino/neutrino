/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2015, Thilo Graf 'dbt'

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

#ifndef __CC_TXT_SCREEN__
#define __CC_TXT_SCREEN__


//! Sub class for CTextBox using CComponent classes.
/*!
This class contains flags or helpers to control CTextBox screen and paint handling and mostly used by
CComponentsText and its derivatives and should be implemented as heredity.
At the moment used in classes with text handlers. e.g. buttons, headers, ext text ...
*/
class CCTextScreen
{
	protected:
		///allows to save bg screen behind caption within CTextBox object, default = false
		bool cc_txt_save_screen;

	public:
		CCTextScreen(){cc_txt_save_screen = false;};
};

#endif
