/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern text input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifndef __CC_INPUT_DIALOG_H__
#define __CC_INPUT_DIALOG_H__

#include "cc_frm_window.h"
#include "cc_input_buffer.h"
#include "cc_input_field.h"
#include "cc_frm_keyboard.h"
#include "cc_item_text.h"

#include <gui/widget/menue_target.h>

#include <sigc++/signal.h>

class CChangeObserver;

/**
 * Shared skeleton of the modern input dialogs: window chrome, the four
 * footer buttons and the key dispatch that turns a keypress into one of
 * their results.
 *
 * Everything a dialog with input fields needs before it has any fields.
 * Derived classes add their body items and their own message loop; the
 * base has neither, and deliberately no exec().
 */
class CCInputDialogBase : public CMenuTarget, public CComponentsWindow
{
	public:
		///result values carried by the shared footer buttons
		enum button_result_t
		{
			RES_SAVE = 0,
			RES_DELETE,
			RES_CLEAR,
			RES_CANCEL
		};

	protected:
		CCInputDialogBase(const int &w,
			const int &h,
			const std::string &caption,
			const std::string &icon_name);

		///applies header, body and footer colours; call again after every Refresh()
		void applyDialogStyle();

		///fills the footer with save/delete/clear/cancel and their direct keys
		void initFooterButtons();

		///returns the button_result_t a key belongs to, or -1 if no button claims it
		int sendButtonKey(neutrino_msg_t msg);

		///height of the hint line above the input area
		static int getDialogHintHeight(Font *font);
};

class CCTextInputDialog : public CCInputDialogBase
{
	private:
		std::string *cid_value;
		CChangeObserver *cid_observ;
		CCInputBuffer cid_buffer;
		CComponentsText *cid_hint;
		CCInputField *cid_field;
		CComponentsKeyboard *cid_keyboard;
		std::string cid_hint_text;
		std::string cid_error_text;
		std::string cid_placeholder;
		std::string cid_field_font_reference;
		bool cid_password_mode;
		bool cid_enable_keyboard;
		bool cid_allow_empty;
		bool cid_saved;
		size_t cid_max_chars;
		CCInputBuffer::input_filter_mode_t cid_filter_mode;
		sigc::slot<bool, const std::string &, std::string &> cid_validator;
		fb_pixel_t cid_field_col_text;
		fb_pixel_t cid_field_col_frame;
		fb_pixel_t cid_field_col_body;
		fb_pixel_t cid_field_col_body_focus;
		fb_pixel_t cid_field_col_placeholder;

		void applyFieldStyle();
		void initDialogItems();
		void createKeyboard();
		void layoutDialogItems();
		void syncDialogState();
		std::string getFieldFontReference() const;
		std::string getVisibleHintText() const;
		std::string getVfdText() const;
		void refreshField();
		void showMaxCharsError();
		void focusFieldFromKeyboard();
		void setFieldFocus(const bool &focused);
		void clearInlineError();
		void showInlineError(const std::string &text);
		bool confirmDiscard() const;
		bool save();

	public:
		CCTextInputDialog(const std::string &title,
			std::string *value,
			CChangeObserver *observ = NULL,
			const std::string &icon = NEUTRINO_ICON_EDIT);
		virtual ~CCTextInputDialog() {};

		void setHintText(const std::string &hint);
		void setPlaceholder(const std::string &text);
		void setFieldFontReference(const std::string &text);
		void setMaxChars(size_t max_chars);
		void setAllowEmpty(bool allow_empty = true);
		void setFilterMode(CCInputBuffer::input_filter_mode_t mode);

		/**
		 * Rejects a value before it is written back. The slot gets the
		 * current text and fills the second argument with the reason;
		 * returning false keeps the dialog open and shows that reason
		 * on the hint line.
		 *
		 * NOTE: this is for validation, not for decisions. A prompt
		 * such as "overwrite the existing entry?" belongs into the
		 * caller after exec(), not in here: ShowMsg() out of save()
		 * paints over the open dialog whose saved background predates
		 * the message box.
		 */
		void setValidator(const sigc::slot<bool, const std::string &, std::string &> &validator);
		void enablePasswordMode(bool enable = true);

		/**
		 * Shows a keyboard below the input field.
		 *
		 * Not cosmetic: CRCInput::getUnicodeValue() only knows upper
		 * case letters, so without it lower case and umlauts cannot be
		 * entered at all. Call before exec().
		 *
		 * While the keyboard has focus, OK inserts the focused glyph
		 * instead of saving. Red still saves.
		 */
		void enableOnScreenKeyboard(bool enable = true);

		int exec(CMenuTarget *parent, const std::string &actionKey);
		void hide();
		virtual std::string &getValue(void);

		///true when the dialog left through save(), not through cancel, back or timeout
		bool wasSaved() const {return cid_saved;}

		sigc::signal<void> OnAfterSave;
};

#endif
