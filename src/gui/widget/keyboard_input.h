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


#ifndef __keyboard_input__
#define __keyboard_input__

#include "menue.h"

#include <gui/components/cc_input_dialog.h>
#include <system/localize.h>

#include <string>

/*
 * The keyboard dialog's interface on top of CCTextInputDialog, so every
 * caller and the Lua keyboardinput entry get the cc dialog with its
 * on-screen keyboard.
 *
 * Size limits the number of characters, 0 means no limit. The old dialog
 * took it as a number of character boxes, cut a longer value to what
 * fitted on screen and wrote it back that way even on cancel.
 */
class CKeyboardInput : public CCTextInputDialog
{
	private:
		void init(const std::string &Name, int Size, const std::string &Hint_1, const std::string &Hint_2);

	public:
		CKeyboardInput(const neutrino_locale_t Name, std::string* Value, int Size = 0, CChangeObserver* Observ = NULL, const char * const Icon = NULL, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE);
		CKeyboardInput(const std::string      &Name, std::string* Value, int Size = 0, CChangeObserver* Observ = NULL, const char * const Icon = NULL, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE);
		CKeyboardInput(const std::string      &Name, std::string* Value, int Size = 0, CChangeObserver* Observ = NULL, const char * const Icon = NULL, std::string HintText_1 = "", std::string HintText_2 = "");

		/* the cc window saves and restores its background itself */
		void forceSaveScreen(bool /*enable*/) {}
		/* for secrets: every character shows as '*' - in the field, in a
		   menu row without text of its own and on the front display */
		void setMasked(bool enable) { enablePasswordMode(enable); }
};

#endif
