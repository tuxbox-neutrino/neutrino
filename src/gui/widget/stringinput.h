/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/


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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __stringinput__
#define __stringinput__

#include "menue.h"
#include <gui/components/cc_numeric_input_dialog.h>
#include <gui/components/cc_pin_input_dialog.h>

#include <system/localize.h>

#include <string>

/* Counts the input dialogs that are open. Each of them edits a copy and
   writes its target when the value is saved; the text input dialog also
   writes back the value it started with when it is left otherwise, over
   any value put there meanwhile. So code that puts new values into the
   settings asks this first and waits while an input is open, and asks to
   be told when the last one closes: that one posts STRING_INPUTS_CLOSED,
   which the screen the input was opened from hands on before it saves
   anything on its way out. One open on a script's thread counts as well. */
class CStringInputOpen
{
	public:
		CStringInputOpen();
		~CStringInputOpen();
		static bool any();
		/* Asks for STRING_INPUTS_CLOSED once the last open input closes. If the
		   queue will not take it then, the next save of the settings and the
		   main loop take up what waited. */
		static void notifyWhenClosed();
	private:
		CStringInputOpen(const CStringInputOpen &);
		CStringInputOpen &operator=(const CStringInputOpen &);
};

/* The string inputs run on the cc numeric dialog: nearly all of them ask
   for a number kept as a string, and the old constructors stay. Size is
   the most digits. A caller whose valid characters or value are no number
   gets a text input with the on-screen keyboard instead, limited to the
   valid characters and Size of them - the value then reaches the field as
   it is, as a weekday mask "XXXXX--" or a coordinate "52.52" must. */
class CStringInput : public CCNumericInputDialog
{
	private:
		std::string valid_chars;
		int size;
		void init(const std::string &Name, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars);
	protected:
		void onBeforeExec();
	public:
		CStringInput(const neutrino_locale_t Name, std::string* Value, int Size, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, const char * const Valid_Chars= (const char *) "0123456789. ", CChangeObserver* Observ = NULL, const char * const Icon = NULL);
		CStringInput(const std::string &Name, std::string* Value, int Size, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, const char * const Valid_Chars= (const char *) "0123456789. ", CChangeObserver* Observ = NULL, const char * const Icon = NULL);

		///the dialog saves its background itself
		void forceSaveScreen(bool /*enable*/) {}
};

/* The PIN prompts run on the cc PIN dialog: digits only, stars, closing on
   the last digit, up and down handed on to the screen below. The old
   constructors stay, so the menus that ask for a PIN keep their code.
   Valid_Chars is gone with the old window: a PIN is digits. */
class CPINInput : public CCPinInputDialog
{
	private:
		void init(const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2);
	public:
		CPINInput(const neutrino_locale_t Name, std::string *Value, int Size, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, const char * const Valid_Chars = NULL, CChangeObserver* Observ = NULL);
		CPINInput(const std::string &Name, std::string *Value, int Size, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, const char * const Valid_Chars = NULL, CChangeObserver* Observ = NULL);
};

// For CI: Blind says whether the CAM wants the answer hidden
class CEnquiryInput : public CPINInput
{
	public:
		CEnquiryInput(const neutrino_locale_t Name, std::string *Value, int Size, bool Blind, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, const char * const Valid_Chars = NULL, CChangeObserver* Observ = NULL)
		 : CPINInput(Name, Value, Size, Hint_1, Hint_2, Valid_Chars, Observ) { setMasked(Blind); }
		CEnquiryInput(const std::string &Name, std::string *Value, int Size, bool Blind, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, const char * const Valid_Chars = NULL, CChangeObserver* Observ = NULL)
		 : CPINInput(Name, Value, Size, Hint_1, Hint_2, Valid_Chars, Observ) { setMasked(Blind); }
};

/* The youth protection prompt: names the locked channel or the age of the
   locked programme above the caller's hint, and leaves once a program lock
   event names another age. */
class CPLPINInput : public CPINInput
{
	public:
		CPLPINInput(const neutrino_locale_t Name, std::string *Value, int Size, const neutrino_locale_t Hint_2, int FSK);
};

/* Sets a new PIN: asks for it twice, as stars both times, and stores it
   only when both entries match - a PIN typed blind once is a PIN nobody
   knows. getValue() gives the menu row stars as well; a row that is to
   show them must not get the PIN as its option text. */
class CPINChangeWidget : public CMenuTarget
{
	private:
		neutrino_locale_t name;
		std::string *value;
		int size;
		neutrino_locale_t hint;
		CChangeObserver *observ;
	public:
		CPINChangeWidget(const neutrino_locale_t Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const char * const Valid_Chars = NULL, CChangeObserver* Observ = NULL);
		int exec(CMenuTarget* parent, const std::string & actionKey);
		std::string &getValue(void);
};


#endif
