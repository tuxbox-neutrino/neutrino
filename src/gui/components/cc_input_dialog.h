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
			///removes the glyph BEFORE the cursor, not the one under it
			RES_BACKSPACE,
			RES_CLEAR,
			RES_CANCEL,
			RES_CAPS,
			RES_LAYOUT,
			///first result free for the buttons a derived dialog adds
			RES_USER = 32
		};

	protected:
		CCInputDialogBase(const int &w,
			const int &h,
			const std::string &caption,
			const std::string &icon_name);

		///applies header, body and footer colours; call again after every Refresh()
		void applyDialogStyle();

		///fills the footer with save/backspace/clear/cancel and their direct keys
		void initFooterButtons(const std::string &layout_name = std::string());

		/**
		 * Buttons initFooterButtons() appends behind its own. Data, not a
		 * virtual call: the footer is first built in the constructor,
		 * where a derived class is not there yet to be asked.
		 */
		std::vector<button_label_cc> idb_extra_buttons;

		///returns the button_result_t a key belongs to, or -1 if no button claims it
		int sendButtonKey(neutrino_msg_t msg);

		/**
		 * Applies a buffer editing button result to the given buffer.
		 *
		 * Lives here, not in the loops: initFooterButtons() writes the
		 * labels, so this writes what they do. A derived dialog that
		 * takes the shared footer must take these results with it, or
		 * its green key says one thing and does another - which is
		 * exactly how RES_BACKSPACE and RES_CLEAR drifted apart between
		 * the two dialogs that existed when this was added.
		 *
		 * Opt-in, not enforced: the base deliberately has no exec(), so
		 * a dialog that builds its own footer stands outside this.
		 *
		 * Returns true when the BUFFER changed - not whether the
		 * field needs a repaint. The two part company as soon as a
		 * caller also drops an error frame, which is model state that
		 * only paint() takes off the screen: such a caller has to ask
		 * both halves and repaint when either says yes.
		 *
		 * Results this does not own - save, cancel, caps, layout -
		 * return false and stay the caller's business.
		 */
		bool applyBufferResult(int btn_res, CCInputBuffer *buffer);

		///height of the hint line above the input area, room for at least two lines
		static int getDialogHintHeight(Font *font, int lines = 2);

		/* Measures and colours every dialog of this family shares, so
		 * that a field looks the same whichever dialog it sits in. */
		static int getInputDialogWidth();
		static int getInputDialogMinHeight();
		static int getInputDialogPad();
		static int getInputFieldInset();
		static int getInputFieldPaddingX();
		static int getInputFieldPaddingY();
		static int getInputFieldHeight(Font *font);
		static int getInputDialogCaretWidth(Font *font);
		///the largest menu font that fits reference_text into body_w
		static Font *getInputDialogFieldFont(const int body_w, const std::string &reference_text);
		static fb_pixel_t getInputFieldTextColor();
		static fb_pixel_t getInputFieldFrameColor();
		static fb_pixel_t getInputFieldBodyColor();
		static fb_pixel_t getInputFieldBodyFocusColor();
		static fb_pixel_t getInputFieldPlaceholderColor();

		/**
		 * The title as a locale, NONEXISTANT_LOCALE for a dialog built
		 * from a string. notifyObserver() needs it: the menu observers
		 * implement the locale overload of changeNotify() only.
		 */
		neutrino_locale_t idb_title_locale;

		///announces a saved value through the overload that fits the title
		void notifyObserver(CChangeObserver *observ, std::string *value);

		/**
		 * Asks whether to discard the edits. Stop a blinking caret before
		 * asking: its timer thread would keep painting through the box.
		 * Returns true when the edits are to go.
		 */
		bool askDiscard();

		///shows the text being edited on the front display and the graphic LCD
		void showFrontDisplay(const std::string &text, size_t cursor);
		///gives the graphic LCD back to the channel display
		void releaseFrontDisplay();

		/**
		 * Hands a message the dialog has no use for to the application
		 * and returns its messages_return flags: cancel_all asks every
		 * menu to close, cancel_info the prompt that is up - a CAM
		 * closing its enquiry sends that one.
		 */
		static int forwardMessage(neutrino_msg_t msg, neutrino_msg_data_t data);
};

class CCTextInputDialog : public CCInputDialogBase
{
	private:
		std::string *cid_value;
		CChangeObserver *cid_observ;
		int cid_exit_result;
		int cid_timeout_sec;
		bool cid_confirm_discard;
		bool cid_close_on_cancel_info;
		CCInputBuffer cid_buffer;
		CComponentsText *cid_hint;
		CCInputField *cid_field;
		CComponentsKeyboard *cid_keyboard;
		std::string cid_hint_text;
		std::string cid_hint_suffix;
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
		///the text being edited to the front display and the graphic LCD, masked like the field
		void showDisplayText();
		void showMaxCharsError();
		void focusFieldFromKeyboard();
		void setFieldFocus(const bool &focused);
		bool confirmDiscard();
		bool save();
		void applyHookResult(int hook, bool &loop, int &res);
		int getTimeoutSec() const;

	protected:
		/**
		 * What a hook asks the message loop to do. Every hook defaults
		 * to HOOK_PASS, so a dialog that overrides none behaves exactly
		 * like this class.
		 */
		enum hook_result_t
		{
			///not taken: the loop goes on as if there were no hook
			HOOK_PASS = 0,
			///taken; nothing on screen changed
			HOOK_DONE,
			///taken; the buffer changed, so the field is repainted
			HOOK_REFRESH,
			///save as the save key does and close if that succeeds
			HOOK_SAVE,
			///close unsaved at once, with the result of setExitResult()
			HOOK_EXIT
		};

		///runs first in exec(): load a value that is not a string into the target
		virtual void onBeforeExec() {}
		///turns the target's value into the text the field starts with
		virtual std::string prepareInitialText(const std::string &raw) { return raw; }
		/**
		 * Sees every message before the dialog does, except those the
		 * on-screen keyboard takes while it has the focus.
		 */
		virtual int onDialogMsg(neutrino_msg_t /*msg*/, neutrino_msg_data_t /*data*/) { return HOOK_PASS; }
		///gets the footer results this class has no meaning for, RES_USER and up
		virtual int onButtonResult(int /*btn_res*/) { return HOOK_PASS; }
		///runs after a glyph typed on the remote went into the buffer
		virtual int onAfterInsert() { return HOOK_PASS; }
		///runs in save() once the text is in the target, before the observer hears of it
		virtual void commitValue() {}

		CCInputBuffer &getBuffer() { return cid_buffer; }
		CCInputField *getField() { return cid_field; }
		bool hasKeyboard() const { return cid_keyboard != NULL; }
		///points the dialog at another string, e.g. one that mirrors a number
		void setValueTarget(std::string *value);
		///the result exec() returns after HOOK_EXIT
		void setExitResult(int res) { cid_exit_result = res; }
		///false: cancel and timeout leave without asking about the edits
		void setConfirmDiscard(bool enable) { cid_confirm_discard = enable; }
		///seconds without a key before the dialog closes; -1 = the menu timeout
		void setTimeoutSec(int sec) { cid_timeout_sec = sec; }
		///true: a request to close the prompt that is up (cancel_info) closes this one
		void setCloseOnCancelInfo(bool enable) { cid_close_on_cancel_info = enable; }
		///the field's colours while it shows something special; resetFieldColors() undoes it
		void setFieldColors(fb_pixel_t text, fb_pixel_t body, fb_pixel_t body_focus);
		void resetFieldColors();
		///replaces the buttons appended behind the shared four
		void setExtraFooterButtons(const std::vector<button_label_cc> &buttons);
		void refreshField();
		void clearInlineError();
		void showInlineError(const std::string &text);
		///a line of the dialog's own below the caller's hint, e.g. a value range
		void setHintSuffix(const std::string &suffix);

	public:
		CCTextInputDialog(const std::string &title,
			std::string *value,
			CChangeObserver *observ = NULL,
			const std::string &icon = NEUTRINO_ICON_EDIT);

		/**
		 * The same dialog with a localized title. A save is then
		 * announced through changeNotify(neutrino_locale_t, ...) with
		 * that locale - the overload the menu observers implement -
		 * where the std::string constructor announces it through the
		 * std::string overload, which none of them does.
		 */
		CCTextInputDialog(const neutrino_locale_t title,
			std::string *value,
			CChangeObserver *observ = NULL,
			const std::string &icon = NEUTRINO_ICON_EDIT);
		virtual ~CCTextInputDialog() {};

		void setHintText(const std::string &hint);
		///one hint text from two lines, either of which may be empty
		static std::string joinHintLines(const std::string &first, const std::string &second);
		void setPlaceholder(const std::string &text);
		void setFieldFontReference(const std::string &text);
		void setMaxChars(size_t max_chars);
		void setAllowEmpty(bool allow_empty = true);
		void setFilterMode(CCInputBuffer::input_filter_mode_t mode);
		///the glyphs FILTER_CHARSET lets through, see CCInputBuffer::setCharset()
		void setCharset(const std::string &glyphs);

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
		 * instead of saving. Red still saves. The dialog starts on
		 * the keyboard when the field is empty; an existing value
		 * keeps the field focused so left/right edit it at once -
		 * down (or up) hands the focus to the keys.
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
