/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009-2012 Stefan Seyfried

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

#include <gui/widget/stringinput.h>

#include <driver/rcinput.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <system/debug.h>

#include <global.h>
#include <neutrinoMessages.h>

#include <atomic>
#include <neutrino.h>

// Atomic, because an input may also be open on a script's thread.
static std::atomic<int> string_inputs_open(0);
static std::atomic<bool> string_inputs_notify(false);

CStringInputOpen::CStringInputOpen()
{
	string_inputs_open++;
}

CStringInputOpen::~CStringInputOpen()
{
	if (--string_inputs_open > 0)
		return;
	if (string_inputs_notify.exchange(false) && g_RCInput != NULL &&
	    !g_RCInput->postMsg(NeutrinoMessages::STRING_INPUTS_CLOSED, 0))
		string_inputs_notify = true;
}

void CStringInputOpen::notifyWhenClosed()
{
	string_inputs_notify = true;
}

bool CStringInputOpen::any()
{
	return string_inputs_open.load() > 0;
}

static std::string localeText(const neutrino_locale_t locale)
{
	return locale == NONEXISTANT_LOCALE ? "" : g_Locale->getText(locale);
}

/* A point is no digit: with it in the valid characters the value may be a
   decimal, and the numeric dialog takes whole numbers only. */
static const char *const NUMBER_CHARS = "0123456789 ";

CStringInput::CStringInput(const neutrino_locale_t Name, std::string* Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
	: CCNumericInputDialog(Name, Value, Observ, Icon ? Icon : NEUTRINO_ICON_EDIT)
{
	init(localeText(Name), Size, Hint_1, Hint_2, Valid_Chars);
}

CStringInput::CStringInput(const std::string &Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
	: CCNumericInputDialog(Name, Value, Observ, Icon ? Icon : NEUTRINO_ICON_EDIT)
{
	init(Name, Size, Hint_1, Hint_2, Valid_Chars);
}

void CStringInput::init(const std::string &Name, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars)
{
	valid_chars = Valid_Chars ? Valid_Chars : "";
	size = Size > 0 ? Size : 0;
	setHintText(joinHintLines(localeText(Hint_1), localeText(Hint_2)));
	setPlaceholder(Name);
	setMaxDigits(size);
}

void CStringInput::onBeforeExec()
{
	CCNumericInputDialog::onBeforeExec();

	/* Decided at every open, not once: the value may have changed since
	   the input was built, and only both together say "a number". */
	const bool number_chars = !valid_chars.empty() &&
		valid_chars.find_first_not_of(NUMBER_CHARS) == std::string::npos;
	const std::string &value = valueString ? *valueString : valueStringTmp;
	const bool number_value = value.find_first_not_of("0123456789 ") == std::string::npos;

	if (number_chars && number_value)
	{
		setTextMode(false);
		setMaxDigits(size);
		return;
	}

	if (number_chars)
		dprintf(DEBUG_NORMAL, "[CStringInput] %s: \"%s\" is no number, opened as text\n",
			ccw_caption.c_str(), value.c_str());
	setTextMode(true, valid_chars);
	setMaxChars(size);
}

CPINInput::CPINInput(const neutrino_locale_t Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const /*Valid_Chars*/, CChangeObserver* Observ)
	: CCPinInputDialog(Name, Value, Size > 0 ? Size : 0, Observ)
{
	init(Hint_1, Hint_2);
}

CPINInput::CPINInput(const std::string &Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const /*Valid_Chars*/, CChangeObserver* Observ)
	: CCPinInputDialog(Name, Value, Size > 0 ? Size : 0, Observ)
{
	init(Hint_1, Hint_2);
}

void CPINInput::init(const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2)
{
	setHintText(joinHintLines(localeText(Hint_1), localeText(Hint_2)));
}

/* The age hint was written for the old window and never shown: its
   constructor dropped Hint_1 and nothing called the text for it. */
static std::string lockHint(int fsk)
{
	if (fsk >= 0x100)
		return g_Locale->getText(LOCALE_PARENTALLOCK_LOCKEDCHANNEL);

	char hint[128];
	snprintf(hint, sizeof(hint), g_Locale->getText(LOCALE_PARENTALLOCK_LOCKEDPROGRAM), fsk);
	return hint;
}

CPLPINInput::CPLPINInput(const neutrino_locale_t Name, std::string *Value, int Size, const neutrino_locale_t Hint_2, int FSK)
	: CPINInput(Name, Value, Size)
{
	setHintText(joinHintLines(lockHint(FSK), localeText(Hint_2)));
	watchLockStatus(FSK);
}

CPINChangeWidget::CPINChangeWidget(const neutrino_locale_t Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const char * const /*Valid_Chars*/, CChangeObserver* Observ)
{
	name = Name;
	value = Value;
	size = Size > 0 ? Size : 0;
	hint = Hint_1;
	observ = Observ;
}

int CPINChangeWidget::exec(CMenuTarget* parent, const std::string &)
{
	if (parent)
		parent->hide();

	while (true)
	{
		std::string entered;
		CCPinInputDialog enter(name, &entered, size);
		enter.setHintText(localeText(hint));
		/* Nothing below to hand them to: this is a menu of its own. */
		enter.setLeaveOnUpDown(false);
		int res = enter.exec(NULL, "");
		if (!enter.wasSaved())
			return res == menu_return::RETURN_EXIT_ALL ? res : menu_return::RETURN_REPAINT;

		/* A title of its own: the last digit closes the first prompt at
		   once, and a second prompt that looks the same reads as the
		   first one starting over. */
		std::string repeated;
		CCPinInputDialog repeat(LOCALE_PINPROTECTION_REPEAT_HEAD, &repeated, size);
		repeat.setHintText(localeText(LOCALE_PINPROTECTION_REPEAT));
		repeat.setLeaveOnUpDown(false);
		res = repeat.exec(NULL, "");
		if (!repeat.wasSaved())
			return res == menu_return::RETURN_EXIT_ALL ? res : menu_return::RETURN_REPAINT;

		if (entered == repeated)
		{
			if (value)
			{
				setSettingsText(*value, entered);
				if (observ)
					observ->changeNotify(name, (void *) value->c_str());
			}
			ShowHint(LOCALE_MESSAGEBOX_INFO, LOCALE_PINPROTECTION_CHANGED);
			return menu_return::RETURN_REPAINT;
		}

		ShowMsg(LOCALE_MESSAGEBOX_ERROR, LOCALE_PINPROTECTION_MISMATCH,
			CMsgBox::mbrOk, CMsgBox::mbOk, NEUTRINO_ICON_ERROR,
			MSGBOX_MIN_WIDTH, DEFAULT_TIMEOUT);
	}
}

std::string &CPINChangeWidget::getValue(void)
{
	valueStringTmp.assign(value ? value->size() : 0, '*');
	return valueStringTmp;
}
