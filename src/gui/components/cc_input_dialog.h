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
#include "cc_item_text.h"

#include <gui/widget/menue_target.h>

#include <sigc++/signal.h>

class CChangeObserver;

class CCTextInputDialog : public CMenuTarget, public CComponentsWindow
{
	public:
		enum button_result_t
		{
			RES_SAVE = 0,
			RES_DELETE,
			RES_CLEAR,
			RES_CANCEL
		};

	private:
		std::string *cid_value;
		CChangeObserver *cid_observ;
		CCInputBuffer cid_buffer;
		CComponentsText *cid_hint;
		CCInputField *cid_field;
		std::string cid_hint_text;
		std::string cid_error_text;
		std::string cid_placeholder;
		std::string cid_field_font_reference;
		bool cid_password_mode;
		bool cid_allow_empty;
		bool cid_saved;
		size_t cid_max_chars;
		fb_pixel_t cid_field_col_text;
		fb_pixel_t cid_field_col_frame;
		fb_pixel_t cid_field_col_body;
		fb_pixel_t cid_field_col_body_focus;
		fb_pixel_t cid_field_col_placeholder;

		void applyDialogStyle();
		void applyFieldStyle();
		void initDialogItems();
		void layoutDialogItems();
		void initFooterButtons();
		void syncDialogState();
		std::string getFieldFontReference() const;
		std::string getVisibleHintText() const;
		void clearInlineError();
		void showInlineError(const std::string &text);
		int sendButtonKey(neutrino_msg_t msg);
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
		void enablePasswordMode(bool enable = true);

		int exec(CMenuTarget *parent, const std::string &actionKey);
		void hide();
		virtual std::string &getValue(void);

		sigc::signal<void> OnAfterSave;
};

#endif
