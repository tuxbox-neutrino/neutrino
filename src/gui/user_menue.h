/*
	user_menue setup implementation - Neutrino-GUI
	based up implementation by Günther

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Rework
	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

        License: GPL

        This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.

		
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
*/

#ifndef __user_menue__
#define __user_menue__

#include <gui/widget/menue.h>
#include <system/settings.h>
#include <system/setting_helpers.h>

#include <string>

// USERMENU: colorbuttons only
typedef struct user_menu_data_t
{
	neutrino_locale_t caption;
	const neutrino_msg_t key_helper_msg_def;
	const char * key_helper_icon_def;
	const char * menu_icon_def;
	int selected;
} user_menu_data_struct;

#define COL_BUTTONMAX SNeutrinoSettings::BUTTON_MAX
static user_menu_data_t user_menu[COL_BUTTONMAX] =
{
	{LOCALE_USERMENU_TITLE_RED	, CRCInput::RC_red	, NEUTRINO_ICON_BUTTON_RED	, NEUTRINO_ICON_RED,	-1},
	{LOCALE_USERMENU_TITLE_GREEN	, CRCInput::RC_green	, NEUTRINO_ICON_BUTTON_GREEN	, NEUTRINO_ICON_GREEN, 	-1},
	{NONEXISTANT_LOCALE		, CRCInput::RC_yellow	, NEUTRINO_ICON_BUTTON_YELLOW	, NEUTRINO_ICON_YELLOW,	-1},
	{LOCALE_USERMENU_TITLE_BLUE	, CRCInput::RC_blue	, NEUTRINO_ICON_BUTTON_BLUE	, NEUTRINO_ICON_FEATURES, -1}
};

// const neutrino_msg_t col_key_helper_msg_def[COL_BUTTONMAX]={CRCInput::RC_red,CRCInput::RC_green,CRCInput::RC_yellow,CRCInput::RC_blue};
// const char * col_key_helper_icon_def[COL_BUTTONMAX]={NEUTRINO_ICON_BUTTON_RED,NEUTRINO_ICON_BUTTON_GREEN,NEUTRINO_ICON_BUTTON_YELLOW,NEUTRINO_ICON_BUTTON_BLUE};

class CUserMenu : public CChangeObserver
{	
	private:		
		int width;
		bool changeNotify(const neutrino_locale_t OptionName, void *);
		static std::string tmp;

	public:
		CUserMenu();
		~CUserMenu();
		bool showUserMenu(neutrino_msg_t msg);
		static const char *getUserMenuButtonName(int button, bool &active, bool return_title = false);
};


// This is just a quick helper for the usermenu and pluginlist.
class CColorKeyHelper
{
	private:
		int number_key;
		bool color_key_used[COL_BUTTONMAX];
	public:
		CColorKeyHelper() 
		{
			reset();
		};
		
		void reset(void)
		{
			number_key = 1;
			for (int i= 0; i < COL_BUTTONMAX; i++ )
				color_key_used[i] = false;
		};

		/* Returns the next available button, to be used in menu as 'direct' keys. Appropriate
		* definitions are returnd in msp and icon
		* A color button could be requested as prefered button (other buttons are not supported yet).
		* If the appropriate button is already in used, the next number_key button is returned instead
		* (first 1-9 and than 0). */
		bool get(neutrino_msg_t* msg, const char** icon, neutrino_msg_t prefered_key = CRCInput::RC_nokey)
		{
			bool result = false;
			int button = -1;
			if (prefered_key == CRCInput::RC_red)
				button = 0;
			if (prefered_key == CRCInput::RC_green)
				button = 1;
			if (prefered_key == CRCInput::RC_yellow)
				button = 2;
			if (prefered_key == CRCInput::RC_blue)
				button = 3;

			*msg = CRCInput::RC_nokey;
			*icon = "";
			if (button >= 0 && button < COL_BUTTONMAX)
			{ // try to get color button
				if ( color_key_used[button] == false)
				{
					color_key_used[button] = true;
					*msg = user_menu[button].key_helper_msg_def;
					*icon = user_menu[button].key_helper_icon_def;
					result = true;
				}
			}

			if ( result == false && number_key < 10) // no key defined yet, at least try to get a numbered key
			{
				// there is still a available number_key
				*msg = CRCInput::convertDigitToKey(number_key);
				*icon = "";
				if (number_key == 9)
					number_key = 0;
				else if (number_key == 0)
					number_key = 10;
				else
					number_key++;
				result = true;
			}
			return (result);
		};
};

#endif
