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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>

#include <gui/widget/icons.h>

#include "keyboard_input.h"

static std::string localeText(const neutrino_locale_t locale)
{
	return locale == NONEXISTANT_LOCALE ? "" : g_Locale->getText(locale);
}

/* A localized name goes to the dialog as a locale: that is what its
   observer gets announced, the overload the menu observers implement. */
CKeyboardInput::CKeyboardInput(const neutrino_locale_t Name, std::string* Value, int Size, CChangeObserver* Observ, const char * const Icon, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2)
				:CCTextInputDialog(Name, Value, Observ, Icon ? Icon : NEUTRINO_ICON_EDIT)
{
	init(localeText(Name), Size, localeText(Hint_1), localeText(Hint_2));
}

CKeyboardInput::CKeyboardInput(const std::string &Name, std::string* Value, int Size, CChangeObserver* Observ, const char * const Icon, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2)
				:CCTextInputDialog(Name, Value, Observ, Icon ? Icon : NEUTRINO_ICON_EDIT)
{
	init(Name, Size, localeText(Hint_1), localeText(Hint_2));
}

CKeyboardInput::CKeyboardInput(const std::string &Name, std::string* Value, int Size, CChangeObserver* Observ, const char * const Icon, std::string HintText_1, std::string HintText_2)
				:CCTextInputDialog(Name, Value, Observ, Icon ? Icon : NEUTRINO_ICON_EDIT)
{
	init(Name, Size, HintText_1, HintText_2);
}

void CKeyboardInput::init(const std::string &Name, int Size, const std::string &Hint_1, const std::string &Hint_2)
{
	setMaxChars(Size > 0 ? Size : 0);
	setHintText(joinHintLines(Hint_1, Hint_2));
	setPlaceholder(Name);
	/* The keyboard is what this dialog is for, and without it there is no
	   lower case and no umlaut: CRCInput::getUnicodeValue() knows upper
	   case letters only. */
	enableOnScreenKeyboard(true);
}
