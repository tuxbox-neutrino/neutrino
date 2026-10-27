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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/stringinput_ext.h>

#include <global.h>
#include <neutrino.h>

static std::string localeText(const neutrino_locale_t locale)
{
	return locale == NONEXISTANT_LOCALE ? "" : g_Locale->getText(locale);
}

CIPInput::CIPInput(const neutrino_locale_t Name, std::string *Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CCStructuredInputDialog(Name, FORMAT_IP, Value, Observ)
{
	setHintText(CCTextInputDialog::joinHintLines(localeText(Hint_1), localeText(Hint_2)));
}

CDateInput::CDateInput(const neutrino_locale_t Name, time_t* Time, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CCStructuredInputDialog(Name, Time, Observ)
{
	setHintText(CCTextInputDialog::joinHintLines(localeText(Hint_1), localeText(Hint_2)));
}

CMACInput::CMACInput(const neutrino_locale_t Name, std::string *Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CCStructuredInputDialog(Name, FORMAT_MAC, Value, Observ)
{
	setHintText(CCTextInputDialog::joinHintLines(localeText(Hint_1), localeText(Hint_2)));
}

CTimeInput::CTimeInput(const neutrino_locale_t Name, std::string* Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ, bool* Cancel)
	: CCStructuredInputDialog(Name, FORMAT_TIME, Value, Observ)
{
	setHintText(CCTextInputDialog::joinHintLines(localeText(Hint_1), localeText(Hint_2)));
	setCancelFlag(Cancel);
}

CIntInput::CIntInput(const neutrino_locale_t Name, int *Value, const unsigned int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CCNumericInputDialog(Name, Value, Observ)
{
	setMaxDigits(Size);
	setHintText(joinHintLines(localeText(Hint_1), localeText(Hint_2)));
	setPlaceholder(localeText(Name));
}
