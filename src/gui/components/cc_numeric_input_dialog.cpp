/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Numeric input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_numeric_input_dialog.h"

#include <driver/rcinput.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace
{
size_t countDigits(long long value)
{
	unsigned long long rest = value < 0 ? 0ULL - (unsigned long long) value : (unsigned long long) value;
	size_t digits = 1;
	while (rest >= 10)
	{
		rest /= 10;
		digits++;
	}
	return digits;
}

///the largest number with that many digits
long long largestWithDigits(size_t digits)
{
	long long value = 0;
	for (size_t i = 0; i < digits && value < LLONG_MAX / 10; i++)
		value = value * 10 + 9;
	return value;
}
}

CCNumericInputDialog::CCNumericInputDialog(const neutrino_locale_t title,
	std::string *value,
	CChangeObserver *observ,
	const std::string &icon)
	: CCTextInputDialog(title, value, observ, icon)
{
	cnd_int_target = NULL;
	initNumeric();
}

CCNumericInputDialog::CCNumericInputDialog(const std::string &title,
	std::string *value,
	CChangeObserver *observ,
	const std::string &icon)
	: CCTextInputDialog(title, value, observ, icon)
{
	cnd_int_target = NULL;
	initNumeric();
}

CCNumericInputDialog::CCNumericInputDialog(const neutrino_locale_t title,
	int *value,
	CChangeObserver *observ,
	const std::string &icon)
	: CCTextInputDialog(title, (std::string *) NULL, observ, icon)
{
	cnd_int_target = value;
	setValueTarget(&cnd_int_text);
	initNumeric();
	updateValue();
}

CCNumericInputDialog::CCNumericInputDialog(const std::string &title,
	int *value,
	CChangeObserver *observ,
	const std::string &icon)
	: CCTextInputDialog(title, (std::string *) NULL, observ, icon)
{
	cnd_int_target = value;
	setValueTarget(&cnd_int_text);
	initNumeric();
	updateValue();
}

void CCNumericInputDialog::initNumeric()
{
	cc_item_type.name = "cc_numeric_input_dialog";

	cnd_has_range = false;
	cnd_min = INT_MIN;
	cnd_max = INT_MAX;
	cnd_allow_negative = false;
	/* An int holds ten digits; a string target is read with atoi() by
	 * most callers, but one that keeps a longer number must not lose
	 * its digits to a limit nobody asked for. */
	cnd_max_digits = cnd_int_target ? 10 : 18;
	cnd_marked = false;
	cnd_text_mode = false;

	setValidator(sigc::mem_fun(*this, &CCNumericInputDialog::validate));
	applyLimits();
}

void CCNumericInputDialog::applyLimits()
{
	/* In text mode the caller sets the limits of a text input. */
	if (cnd_text_mode)
		return;

	if (cnd_allow_negative)
	{
		setFilterMode(CCInputBuffer::FILTER_CHARSET);
		setCharset("-0123456789");
	}
	else
		setFilterMode(CCInputBuffer::FILTER_NUMERIC);

	setMaxChars(cnd_max_digits + (cnd_allow_negative ? 1 : 0));
}

void CCNumericInputDialog::updateFooter()
{
	std::vector<button_label_cc> extra;

	if (cnd_allow_negative && !cnd_text_mode)
	{
		/* The remote has no minus key, and a sign that can only be
		 * typed on a USB keyboard might as well not be allowed. */
		button_label_cc sign;
		sign.button = NEUTRINO_ICON_BUTTON_BLUE;
		sign.text = "+/-";
		sign.btn_result = RES_USER;
		sign.directKeys.push_back(CRCInput::RC_blue);
		extra.push_back(sign);
	}

	setExtraFooterButtons(extra);
}

void CCNumericInputDialog::setMaxDigits(size_t digits)
{
	const size_t limit = cnd_int_target ? 10 : 18;

	cnd_max_digits = digits ? std::min(digits, limit) : limit;
	applyLimits();
}

void CCNumericInputDialog::setRange(int min_value, int max_value)
{
	if (min_value > max_value)
		std::swap(min_value, max_value);

	cnd_has_range = true;
	cnd_min = min_value;
	cnd_max = max_value;
	cnd_max_digits = std::max(countDigits(cnd_min), countDigits(cnd_max));
	if (cnd_min < 0)
		cnd_allow_negative = true;

	applyLimits();
	updateFooter();
	setHintSuffix(cnd_text_mode ? std::string() : getRangeText());
}

void CCNumericInputDialog::allowNegative(bool allow)
{
	cnd_allow_negative = allow || (cnd_has_range && cnd_min < 0);
	applyLimits();
	updateFooter();
}

void CCNumericInputDialog::updateValue()
{
	if (cnd_int_target)
		cnd_int_text = std::to_string(*cnd_int_target);
}

void CCNumericInputDialog::setTextMode(bool enable, const std::string &charset)
{
	cnd_text_mode = enable;

	if (enable)
	{
		setFilterMode(charset.empty() ?
			CCInputBuffer::FILTER_FREE :
			CCInputBuffer::FILTER_CHARSET);
		setCharset(charset);
		setHintSuffix(std::string());
		setMarked(false);
	}
	else
	{
		applyLimits();
		setHintSuffix(cnd_has_range ? getRangeText() : std::string());
	}

	/* Text needs the keyboard - CRCInput::getUnicodeValue() knows no
	 * lower case letters - and a number has its digits on the remote. */
	enableOnScreenKeyboard(enable);
	updateFooter();
}

bool CCNumericInputDialog::parse(const std::string &text, long long &value) const
{
	if (text.empty())
		return false;

	/* strtoll() would also take leading blanks and a plus sign; the
	 * filter keeps both out of the buffer, so whatever it did not
	 * consume is something that is not a number. */
	errno = 0;
	char *end = NULL;
	const long long parsed = strtoll(text.c_str(), &end, 10);
	if (errno != 0 || end == text.c_str() || *end != '\0')
		return false;

	if (cnd_int_target && (parsed < INT_MIN || parsed > INT_MAX))
		return false;

	value = parsed;
	return true;
}

std::string CCNumericInputDialog::getRangeText() const
{
	char text[256];
	snprintf(text, sizeof(text), g_Locale->getText(LOCALE_STRINGINPUT_RANGE),
		(int) cnd_min, (int) cnd_max);
	return text;
}

bool CCNumericInputDialog::validate(const std::string &text, std::string &reason)
{
	if (cnd_text_mode)
		return true;

	/* An empty number stays allowed where no range says otherwise: the
	 * old string input saved blanks, and its callers read them as 0. */
	if (text.empty() && !cnd_has_range)
		return true;

	long long value = 0;
	if (parse(text, value) &&
		(!cnd_has_range || (value >= cnd_min && value <= cnd_max)))
		return true;

	reason = cnd_has_range ?
		getRangeText() :
		std::string(g_Locale->getText(LOCALE_STRINGINPUT_SAVE_FAILED));
	return false;
}

void CCNumericInputDialog::setMarked(bool marked)
{
	cnd_marked = marked;

	/* Marked looks like a selected menu row: the whole value stands out,
	 * and the first digit replaces it. */
	if (marked)
		setFieldColors(COL_MENUCONTENTSELECTED_TEXT,
			COL_MENUCONTENTSELECTED_PLUS_0,
			COL_MENUCONTENTSELECTED_PLUS_0);
	else
		resetFieldColors();
}

void CCNumericInputDialog::stepValue(int delta)
{
	long long lowest = cnd_has_range ? cnd_min :
		(cnd_allow_negative ? -largestWithDigits(cnd_max_digits) : 0);
	long long highest = cnd_has_range ? cnd_max : largestWithDigits(cnd_max_digits);
	if (cnd_int_target)
	{
		lowest = std::max(lowest, (long long) INT_MIN);
		highest = std::min(highest, (long long) INT_MAX);
	}

	long long value = 0;
	if (!parse(getBuffer().getText(), value))
		/* Nothing to count from: start at the bottom of what is allowed. */
		value = std::max(lowest, std::min(0LL, highest));
	else
		value += delta;

	value = std::max(lowest, std::min(value, highest));

	getBuffer().setText(std::to_string(value));
	getBuffer().moveEnd();
}

void CCNumericInputDialog::toggleSign()
{
	std::string text = getBuffer().getText();
	size_t cursor = getBuffer().getCursor();

	if (!text.empty() && text[0] == '-')
	{
		text.erase(0, 1);
		if (cursor > 0)
			cursor--;
	}
	else
	{
		text.insert(0, 1, '-');
		cursor++;
	}

	getBuffer().setText(text);
	getBuffer().setCursor(cursor);
	setMarked(false);
}

void CCNumericInputDialog::onBeforeExec()
{
	updateValue();
}

std::string CCNumericInputDialog::prepareInitialText(const std::string &raw)
{
	if (cnd_text_mode)
	{
		setMarked(false);
		return raw;
	}

	/* The old string input padded numbers with blanks to its width,
	 * and callers still hand such values in. Leading zeros stay: a
	 * postal code is a number that keeps them. */
	const size_t first = raw.find_first_not_of(' ');
	const std::string text = first == std::string::npos ? std::string() :
		raw.substr(first, raw.find_last_not_of(' ') - first + 1);

	setMarked(!text.empty());
	return text;
}

int CCNumericInputDialog::onDialogMsg(neutrino_msg_t msg, neutrino_msg_data_t /*data*/)
{
	if (cnd_text_mode)
		return HOOK_PASS;

	if (msg == CRCInput::RC_up || msg == CRCInput::RC_down)
	{
		stepValue(msg == CRCInput::RC_up ? 1 : -1);
		return HOOK_REFRESH;
	}

	std::string glyph;
	const char *unicode_value = CRCInput::getUnicodeValue(msg);
	if (unicode_value && *unicode_value)
		glyph = unicode_value;
	else if (CRCInput::isNumeric(msg))
		glyph = std::string(1, '0' + CRCInput::getNumericValue(msg));

	if (glyph == "-")
	{
		if (!cnd_allow_negative)
			return HOOK_PASS;
		toggleSign();
		return HOOK_REFRESH;
	}

	if (!cnd_marked)
		return HOOK_PASS;

	if (glyph.size() == 1 && std::isdigit((unsigned char) glyph[0]))
	{
		getBuffer().clear();
		getBuffer().insert(glyph);
		setMarked(false);
		return HOOK_REFRESH;
	}

	/* Deleting a marked value deletes all of it, as with any marked
	 * text; moving the cursor just ends the marking. */
	if (msg == CRCInput::RC_green || msg == CRCInput::RC_yellow ||
		msg == CRCInput::RC_backspace || msg == CRCInput::RC_rewind)
	{
		getBuffer().clear();
		setMarked(false);
		return HOOK_REFRESH;
	}

	if (msg == CRCInput::RC_left || msg == CRCInput::RC_right ||
		msg == CRCInput::RC_page_up || msg == CRCInput::RC_page_down)
		setMarked(false);

	return HOOK_PASS;
}

int CCNumericInputDialog::onButtonResult(int btn_res)
{
	if (btn_res != RES_USER || cnd_text_mode || !cnd_allow_negative)
		return HOOK_PASS;

	toggleSign();
	return HOOK_REFRESH;
}

void CCNumericInputDialog::commitValue()
{
	if (!cnd_int_target || cnd_text_mode)
		return;

	long long value = 0;
	parse(getBuffer().getText(), value);
	*cnd_int_target = (int) value;
	/* "007" was typed, 7 is stored: the row shows what is stored. */
	cnd_int_text = std::to_string(*cnd_int_target);
}
