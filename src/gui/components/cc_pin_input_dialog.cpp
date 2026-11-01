/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	PIN input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_pin_input_dialog.h"

#include <driver/rcinput.h>
#include <gui/widget/icons.h>

CCPinInputDialog::CCPinInputDialog(const neutrino_locale_t title,
	std::string *value,
	size_t length,
	CChangeObserver *observ)
	: CCTextInputDialog(title, value, observ, NEUTRINO_ICON_LOCK)
{
	initPin(length);
}

CCPinInputDialog::CCPinInputDialog(const std::string &title,
	std::string *value,
	size_t length,
	CChangeObserver *observ)
	: CCTextInputDialog(title, value, observ, NEUTRINO_ICON_LOCK)
{
	initPin(length);
}

void CCPinInputDialog::initPin(size_t length)
{
	cc_item_type.name = "cc_pin_input_dialog";

	cpd_length = length;
	cpd_leave_on_updown = true;
	cpd_watch_lock = false;
	cpd_lock_fsk = 0;
	cpd_expected = NULL;

	setFilterMode(CCInputBuffer::FILTER_NUMERIC);
	/* No length means no limit and no closing on a last digit - the CI
	 * may ask without naming one. */
	setMaxChars(length);
	enablePasswordMode(true);
	setConfirmDiscard(false);
	setTimeoutSec(30);
	/* A CAM that closes its enquiry says so with cancel_info, and the
	 * old PIN window left on it as on cancel_all. */
	setCloseOnCancelInfo(true);
}

void CCPinInputDialog::setMasked(bool masked)
{
	enablePasswordMode(masked);
}

void CCPinInputDialog::setLeaveOnUpDown(bool leave)
{
	cpd_leave_on_updown = leave;
}

void CCPinInputDialog::watchLockStatus(int fsk)
{
	cpd_watch_lock = true;
	cpd_lock_fsk = fsk;
}

void CCPinInputDialog::setExpectedPin(const std::string *pin)
{
	cpd_expected = pin;
	setValidator(sigc::mem_fun(*this, &CCPinInputDialog::checkExpected));
}

bool CCPinInputDialog::checkExpected(const std::string &text, std::string &reason)
{
	/* Empty saves: the callers read it as giving up, as they always
	 * did. */
	if (!cpd_expected || text.empty() || text == *cpd_expected)
		return true;

	reason = g_Locale->getText(LOCALE_PINPROTECTION_WRONG);
	/* Emptied here, as the save that asked is refused: the field it
	 * paints next is ready for the next try, and the last digit of a
	 * wrong PIN does not block the first of the new one. */
	getBuffer().clear();
	return false;
}

int CCPinInputDialog::onDialogMsg(neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if (msg == CRCInput::RC_up || msg == CRCInput::RC_down)
	{
		/* Without a keyboard nothing in this dialog uses the two, and
		 * handing them to the application would move whatever sits
		 * below while the prompt stays up. */
		if (!cpd_leave_on_updown)
			return HOOK_DONE;

		g_RCInput->postMsg(msg, data);
		setExitResult(menu_return::RETURN_EXIT);
		return HOOK_EXIT;
	}

	if (cpd_watch_lock && msg == NeutrinoMessages::EVT_PROGRAMLOCKSTATUS)
	{
		/* Handled all the same, and first: the channel list updates
		 * the age of its zap protection from this event. */
		CNeutrinoApp::getInstance()->handleMsg(msg, data);

		if (data != (neutrino_msg_data_t) cpd_lock_fsk)
		{
			setExitResult(menu_return::RETURN_EXIT);
			return HOOK_EXIT;
		}
		return HOOK_DONE;
	}

	return HOOK_PASS;
}

int CCPinInputDialog::onAfterInsert()
{
	if (cpd_length && getBuffer().size() >= cpd_length)
		return HOOK_SAVE;

	return HOOK_PASS;
}

void CCPinInputDialog::onBeforeExec()
{
	CCTextInputDialog::onBeforeExec();

	/* Digits or their stars only. The field exists from here on. */
	if (getField())
		getField()->enableCapCentering(true);
}
