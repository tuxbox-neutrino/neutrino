/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Numeric input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifndef __CC_NUMERIC_INPUT_DIALOG_H__
#define __CC_NUMERIC_INPUT_DIALOG_H__

#include "cc_input_dialog.h"

/**
 * Input dialog for a whole number, typed on the digit keys of the remote.
 *
 * There is no on-screen keyboard: the digits are on the remote. The value
 * opens marked, so the first digit replaces it as a whole - typing a new
 * number is what this dialog is opened for. After that, digits append,
 * green deletes backwards, left and right move the cursor. Up and down
 * count by one.
 *
 * A range, once set, is named below the hint and checked on save. A value
 * outside it is refused with that line in red; it is never clamped behind
 * the user's back.
 *
 * The target is a string, as the old string input edited, or an int. For
 * an int the dialog edits a text of its own, and getValue() returns that
 * same string object for the dialog's whole life: menu rows keep a pointer
 * to it.
 */
class CCNumericInputDialog : public CCTextInputDialog
{
	private:
		int *cnd_int_target;
		std::string cnd_int_text;
		bool cnd_has_range;
		long long cnd_min;
		long long cnd_max;
		bool cnd_allow_negative;
		size_t cnd_max_digits;
		bool cnd_marked;
		bool cnd_text_mode;

		void initNumeric();
		void applyLimits();
		void updateFooter();
		bool parse(const std::string &text, long long &value) const;
		bool validate(const std::string &text, std::string &reason);
		std::string getRangeText() const;
		void setMarked(bool marked);
		void stepValue(int delta);
		void toggleSign();

	protected:
		void onBeforeExec();
		std::string prepareInitialText(const std::string &raw);
		int onDialogMsg(neutrino_msg_t msg, neutrino_msg_data_t data);
		int onButtonResult(int btn_res);
		void commitValue();

	public:
		CCNumericInputDialog(const neutrino_locale_t title,
			std::string *value,
			CChangeObserver *observ = NULL,
			const std::string &icon = NEUTRINO_ICON_EDIT);
		CCNumericInputDialog(const std::string &title,
			std::string *value,
			CChangeObserver *observ = NULL,
			const std::string &icon = NEUTRINO_ICON_EDIT);
		CCNumericInputDialog(const neutrino_locale_t title,
			int *value,
			CChangeObserver *observ = NULL,
			const std::string &icon = NEUTRINO_ICON_EDIT);
		CCNumericInputDialog(const std::string &title,
			int *value,
			CChangeObserver *observ = NULL,
			const std::string &icon = NEUTRINO_ICON_EDIT);
		virtual ~CCNumericInputDialog() {}

		///at most this many digits, the sign not counted; as many as the target holds by default
		void setMaxDigits(size_t digits);
		///the values save() accepts; also sets the digits, and a negative minimum allows a sign
		void setRange(int min_value, int max_value);
		///blue and the minus key put a minus in front or take it away
		void allowNegative(bool allow = true);
		///reads the int target again, for a change made behind the dialog's back
		void updateValue();
		/**
		 * Makes the dialog a plain text input with the on-screen keyboard
		 * and FILTER_CHARSET instead, for a value that is not a number
		 * after all. For callers that can only tell at exec() time, which
		 * is why switching back is possible too.
		 */
		void setTextMode(bool enable, const std::string &charset = std::string());
};

#endif
