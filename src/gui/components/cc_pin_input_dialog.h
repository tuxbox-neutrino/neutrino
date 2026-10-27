/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	PIN input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifndef __CC_PIN_INPUT_DIALOG_H__
#define __CC_PIN_INPUT_DIALOG_H__

#include "cc_input_dialog.h"

/**
 * Input dialog for a PIN: digits only, shown as stars, of a fixed length.
 *
 * The last digit saves and closes the dialog, as the PIN prompts always
 * did; OK saves a shorter entry. Back and the timeout leave with the value
 * as it was, without asking about the digits typed - a PIN is not worth a
 * second question. The timeout is 30 seconds, not the menu timeout: a
 * prompt raised while zapping must not stand forever when the menu
 * timeout is off.
 *
 * Up and down leave the dialog by default and are handed on to the screen
 * below, so a locked channel can be zapped past and a locked menu entry
 * skipped without typing anything.
 */
class CCPinInputDialog : public CCTextInputDialog
{
	private:
		size_t cpd_length;
		bool cpd_leave_on_updown;
		bool cpd_watch_lock;
		int cpd_lock_fsk;

		void initPin(size_t length);

	protected:
		int onDialogMsg(neutrino_msg_t msg, neutrino_msg_data_t data);
		int onAfterInsert();

	public:
		CCPinInputDialog(const neutrino_locale_t title,
			std::string *value,
			size_t length,
			CChangeObserver *observ = NULL);
		CCPinInputDialog(const std::string &title,
			std::string *value,
			size_t length,
			CChangeObserver *observ = NULL);
		virtual ~CCPinInputDialog() {}

		///false shows the digits, for an answer the asking side does not hide either
		void setMasked(bool masked);
		///false keeps up and down in the dialog instead of handing them on
		void setLeaveOnUpDown(bool leave);
		/**
		 * Closes the dialog once a program lock event names an age other
		 * than fsk, after the application has seen the event: the
		 * protection that opened the prompt learns the new age from it.
		 */
		void watchLockStatus(int fsk);
};

#endif
