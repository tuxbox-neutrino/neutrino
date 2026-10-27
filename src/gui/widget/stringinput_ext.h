/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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


#ifndef __stringinput_ext__
#define __stringinput_ext__

#include "menue.h"
#include <gui/components/cc_numeric_input_dialog.h>
#include <gui/components/cc_structured_input_dialog.h>

#include <system/localize.h>

#include <string>

/* The address inputs run on the cc structured dialog: four segments of
   0-255, or six hex pairs. The old constructors stay; an address of zeros
   still saves as "". */
class CIPInput : public CCStructuredInputDialog
{
	public:
		CIPInput(const neutrino_locale_t Name, std::string *Value, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, CChangeObserver* Observ = NULL);
};

//----------------------------------------------------------------------------------------------------

/* The date and time inputs run on the cc structured dialog. The date keeps
   the text its menu row shows in getValue() - the timer list writes into
   it - and reads the time_t again at every open. */
class CDateInput : public CCStructuredInputDialog
{
	public:
		CDateInput(const neutrino_locale_t Name, time_t* Time, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, CChangeObserver* Observ = NULL);
};

//----------------------------------------------------------------------------------------------------

class CMACInput : public CCStructuredInputDialog
{
	public:
		CMACInput(const neutrino_locale_t Name, std::string *Value, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, CChangeObserver* Observ = NULL);
};

//----------------------------------------------------------------------------------------------------

///*cancel is true after exec() unless the time was saved
class CTimeInput : public CCStructuredInputDialog
{
	public:
		CTimeInput(const neutrino_locale_t Name, std::string *Value, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, CChangeObserver* Observ = NULL, bool* cancel=NULL);
};

//----------------------------------------------------------------------------------------------------

/* The int inputs run on the cc numeric dialog; the old constructor stays.
   Size is the most digits, updateValue() reads the int again, and
   getValue() keeps returning one string for the menu rows that hold it. */
class CIntInput : public CCNumericInputDialog
{
	public:
		CIntInput(const neutrino_locale_t Name, int *Value, const unsigned int Size, const neutrino_locale_t Hint_1 = NONEXISTANT_LOCALE, const neutrino_locale_t Hint_2 = NONEXISTANT_LOCALE, CChangeObserver* Observ = NULL);
};

#endif
