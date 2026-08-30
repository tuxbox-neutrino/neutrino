/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern text input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_input_dialog.h"

#include <algorithm>
#include <driver/neutrinofonts.h>
#include <driver/display.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <gui/widget/menue.h>
#include <gui/widget/msgbox.h>

namespace
{
int getInputDialogWidth()
{
	CFrameBuffer *fb = CFrameBuffer::getInstance();
	const int screen_w = fb->getScreenWidth(true);
	const int preferred_w = std::max(fb->scale2Res(820), screen_w * 62 / 100);

	return std::min(screen_w - 2 * OFFSET_INNER_MID, preferred_w);
}

int getInputDialogMinHeight()
{
	return std::max(CFrameBuffer::getInstance()->scale2Res(260),
			HINTBOX_MIN_HEIGHT + CFrameBuffer::getInstance()->scale2Res(40));
}

int getInputDialogPad()
{
	return std::max(OFFSET_INNER_MID, CFrameBuffer::getInstance()->scale2Res(24));
}

int getInputFieldInset()
{
	return std::max(OFFSET_INNER_SMALL,
			CFrameBuffer::getInstance()->scale2Res(18));
}

int getInputFieldPaddingX()
{
	return std::max(OFFSET_INNER_SMALL,
			CFrameBuffer::getInstance()->scale2Res(12));
}

int getInputFieldPaddingY()
{
	return std::max(OFFSET_INNER_MIN,
			CFrameBuffer::getInstance()->scale2Res(3));
}

int getInputFieldGlyphHeight(Font *font)
{
	if (!font)
		return 0;

	return std::max(0, font->getAscender() + font->getDescender());
}

fb_pixel_t getInputFieldTextColor()
{
	return COL_MENUCONTENTDARK_TEXT_PLUS_1;
}

fb_pixel_t getInputFieldFrameColor()
{
	return COL_FRAME_PLUS_0;
}

fb_pixel_t getInputFieldBodyColor()
{
	return COL_MENUCONTENTDARK_PLUS_0;
}

fb_pixel_t getInputFieldBodyFocusColor()
{
	return COL_MENUCONTENTDARK_PLUS_2;
}

fb_pixel_t getInputFieldPlaceholderColor()
{
	return COL_MENUCONTENTDARK_TEXT_PLUS_2;
}

Font *getInputDialogDynFont(int max_w,
	int max_h,
	const std::string &text,
	Font *fallback,
	int style = CNeutrinoFonts::FONT_STYLE_REGULAR)
{
	if (max_w <= 0 || max_h <= 0)
		return fallback;

	int dyn_w = max_w;
	int dyn_h = max_h;
	Font **dyn_font =
		CNeutrinoFonts::getInstance()->getDynFont(dyn_w, dyn_h, text, style);

	return (dyn_font && *dyn_font) ? *dyn_font : fallback;
}

Font *getInputDialogFieldFont(const int body_w,
	const std::string &reference_text)
{
	return getInputDialogDynFont(body_w,
			CFrameBuffer::getInstance()->scale2Res(36),
			reference_text,
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);
}

int getInputDialogCaretWidth(Font *font)
{
	if (!font)
		return std::max(3, CFrameBuffer::getInstance()->scale2Res(3));

	return std::max(std::max(3, CFrameBuffer::getInstance()->scale2Res(3)),
			font->getRenderWidth("|") / 2);
}

int getInputFieldHeight(Font *font)
{
	const int glyph_h = getInputFieldGlyphHeight(font);
	const int vertical_pad = getInputFieldPaddingY();

	return std::max(CFrameBuffer::getInstance()->scale2Res(40),
			glyph_h + 2 * vertical_pad + 2 * FRAME_WIDTH_MIN);
}
}

CCInputDialogBase::CCInputDialogBase(const int &w,
	const int &h,
	const std::string &caption,
	const std::string &icon_name)
	: CComponentsWindow(CC_CENTERED,
		  CC_CENTERED,
		  w,
		  h,
		  caption,
		  icon_name,
		  NULL,
		  CC_SHADOW_ON,
		  COL_FRAME_PLUS_0,
		  COL_MENUCONTENT_PLUS_1,
		  COL_SHADOW_PLUS_0,
		  HINTBOX_DEFAULT_FRAME_WIDTH)
{
	cc_item_type.id = CC_ITEMTYPE_FRM_INPUT_DIALOG;

	corner_rad = RADIUS_LARGE;
	setWindowHeaderColor(COL_MENUHEAD_PLUS_0);
	setWindowHeaderTextColor(COL_MENUHEAD_TEXT);
	setWindowFooterColor(COL_MENUFOOT_PLUS_0);
	setWindowHeaderButtons(CComponentsHeader::CC_BTN_EXIT);
	enableShadow();
	Refresh();
}

int CCInputDialogBase::getDialogHintHeight(Font *font)
{
	const int font_h = font ? font->getHeight() : 0;

	return std::max(CFrameBuffer::getInstance()->scale2Res(72),
			2 * font_h + CFrameBuffer::getInstance()->scale2Res(16));
}

CCTextInputDialog::CCTextInputDialog(const std::string &title,
	std::string *value,
	CChangeObserver *observ,
	const std::string &icon)
	: CCInputDialogBase(getInputDialogWidth(),
		  getInputDialogMinHeight(),
		  title,
		  icon)
{
	cc_item_type.name = "cc_input_dialog";

	cid_value = value;
	valueString = cid_value ? cid_value : &valueStringTmp;
	cid_observ = observ;
	cid_hint = NULL;
	cid_field = NULL;
	cid_keyboard = NULL;
	cid_field_font_reference.clear();
	cid_password_mode = false;
	cid_enable_keyboard = false;
	cid_allow_empty = true;
	cid_saved = false;
	cid_max_chars = 0;
	cid_filter_mode = CCInputBuffer::FILTER_FREE;
	cid_field_col_text = getInputFieldTextColor();
	cid_field_col_frame = getInputFieldFrameColor();
	cid_field_col_body = getInputFieldBodyColor();
	cid_field_col_body_focus = getInputFieldBodyFocusColor();
	cid_field_col_placeholder = getInputFieldPlaceholderColor();

	initDialogItems();
	applyDialogStyle();
	initFooterButtons();
	layoutDialogItems();
}

void CCInputDialogBase::applyDialogStyle()
{
	col_body_std = COL_MENUCONTENT_PLUS_1;
	ccw_col_footer = COL_MENUFOOT_PLUS_0;

	if (getHeaderObject())
		getHeaderObject()->setColorBody(COL_MENUHEAD_PLUS_0);

	if (getBodyObject())
	{
		getBodyObject()->setColorBody(COL_MENUCONTENT_PLUS_1);
		getBodyObject()->doPaintBg(true);
	}

	if (getFooterObject())
	{
		getFooterObject()->setColorBody(COL_MENUFOOT_PLUS_0);
		getFooterObject()->doPaintBg(true);
		getFooterObject()->enableButtonBg(true);
		getFooterObject()->ButtonsOnTop(true);
	}
}

void CCTextInputDialog::applyFieldStyle()
{
	if (!cid_field)
		return;

	cid_field->setColors(cid_field_col_text,
		cid_field_col_frame,
		cid_field_col_body,
		cid_field_col_body_focus);
	cid_field->setPlaceholderColor(cid_field_col_placeholder);
}

void CCTextInputDialog::initDialogItems()
{
	const int pad = getInputDialogPad();
	const int field_inset = getInputFieldInset();
	const int body_w = getBodyObject()->getWidth() - 2 * pad;
	Font *hint_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];
	Font *input_font = getInputDialogFieldFont(body_w,
			getFieldFontReference());
	const int hint_h = CCInputDialogBase::getDialogHintHeight(hint_font);
	const int field_h = getInputFieldHeight(input_font);
	const int field_x = pad + field_inset;
	const int field_w = std::max(0, body_w - 2 * field_inset);

	cid_hint = new CComponentsText(pad,
		pad,
		body_w,
		hint_h,
		"",
		CTextBox::AUTO_WIDTH |
		CTextBox::AUTO_HIGH,
		hint_font,
		CComponentsText::FONT_STYLE_REGULAR,
		NULL,
		CC_SHADOW_OFF,
		COL_MENUCONTENT_TEXT_PLUS_1);
	cid_hint->doPaintBg(false);
	cid_hint->doPaintTextBoxBg(false);

	cid_field = new CCInputField(field_x,
		pad + hint_h + pad,
		field_w,
		field_h,
		input_font,
		NULL,
		CC_SHADOW_OFF,
		cid_field_col_text,
		cid_field_col_frame,
		cid_field_col_body,
		cid_field_col_body_focus,
		COL_SHADOW_PLUS_0);
	cid_field->setBuffer(&cid_buffer);
	cid_field->setPlaceholder(cid_placeholder);
	cid_field->setPlaceholderColor(cid_field_col_placeholder);
	cid_field->setCaretWidth(getInputDialogCaretWidth(input_font));
	cid_field->setPadding(getInputFieldPaddingX(), getInputFieldPaddingY());

	addWindowItem(cid_hint);
	addWindowItem(cid_field);
}

void CCTextInputDialog::createKeyboard()
{
	if (cid_keyboard || !cid_field)
		return;

	cid_keyboard = new CComponentsKeyboard(cid_field->getXPos(),
		cid_field->getYPos() + cid_field->getHeight(),
		cid_field->getWidth(),
		0);
	cid_keyboard->setBuffer(&cid_buffer);
	cid_keyboard->setKeyColors(cid_field_col_body, cid_field_col_text);

	/* One buffer, two views on it: the keyboard writes glyphs, the field
	 * redraws them. Both signals feed paths that already exist, so a key
	 * from the keyboard and a key from the remote end in exactly the same
	 * place. */
	cid_keyboard->OnAfterKey.connect(
		sigc::mem_fun(*this, &CCTextInputDialog::refreshField));
	cid_keyboard->OnKeyRejected.connect(
		sigc::mem_fun(*this, &CCTextInputDialog::showMaxCharsError));
	cid_keyboard->OnLeaveTop.connect(
		sigc::mem_fun(*this, &CCTextInputDialog::focusFieldFromKeyboard));

	addWindowItem(cid_keyboard);
	initFooterButtons(cid_keyboard->getLayoutName());
	layoutDialogItems();
}

void CCTextInputDialog::layoutDialogItems()
{
	if (!cid_hint || !cid_field)
		return;

	const std::string visible_hint = getVisibleHintText();
	const bool has_hint = !visible_hint.empty();
	const int pad = getInputDialogPad();
	const int field_inset = getInputFieldInset();
	const int gap = std::max(OFFSET_INNER_SMALL,
			CFrameBuffer::getInstance()->scale2Res(12));
	const int header_h = getHeaderObject() ? getHeaderObject()->getHeight() : 0;
	const int footer_h = getFooterObject() ? getFooterObject()->getHeight() : 0;
	const int body_w = getBodyObject()->getWidth() - 2 * pad;
	Font *hint_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];
	Font *input_font = getInputDialogFieldFont(body_w,
			getFieldFontReference());
	const int field_x = pad + field_inset;
	const int field_w = std::max(0, body_w - 2 * field_inset);
	const int hint_h = !has_hint ? 0 :
		CCInputDialogBase::getDialogHintHeight(hint_font);
	const int field_h = getInputFieldHeight(input_font);
	int keyboard_h = 0;
	if (cid_keyboard)
	{
		/* Four rows push the dialog to roughly 600-650 px. On a small
		 * OSD that can leave the safe area, so the keyboard is asked
		 * whether it still fits and falls back to smaller keys. */
		const int screen_h = CFrameBuffer::getInstance()->getScreenHeight(true);
		const int budget = screen_h - header_h - footer_h - pad - hint_h -
			(has_hint ? gap : 0) - field_h - gap - pad -
			2 * fr_thickness;
		/* Set both ways, not latched on: needsCompactMode() answers
		 * for the roomy layout whatever mode the keyboard is in, so
		 * the answer no longer flips once compact has made it fit and
		 * the toggling this used to guard against cannot happen. It
		 * has to be able to switch back - fonts DO change within this
		 * dialog's life, the keyboard follows OnAfterSetupFonts, and a
		 * latch would leave it cramped for the rest of the session
		 * after the user lowered the font sizes again. */
		cid_keyboard->setCompactMode(cid_keyboard->needsCompactMode(budget));
		keyboard_h = cid_keyboard->getPreferredHeight();
	}
	const int body_content_h = pad + hint_h +
		(has_hint ? gap : 0) + field_h +
		(cid_keyboard ? gap + keyboard_h : 0) + pad;
	const int target_h = std::max(getInputDialogMinHeight(),
			header_h + footer_h + body_content_h +
			2 * fr_thickness);

	bool geometry_changed = false;
	if (height != target_h)
	{
		/* A painted window must give its pixels back before it moves:
		 * paintForm() skips paintInit() while is_painted stands, so
		 * frame, body and shadow would keep the old geometry - the
		 * color chooser documents the same trap for its own resize.
		 * hide() restores the saved backdrop; the repaint at the end
		 * of this layout pass re-arms it at the new place. */
		geometry_changed = isPainted();
		if (geometry_changed)
			hide();
		height = target_h;
		setCenterPos(CC_ALONG_X | CC_ALONG_Y);
		Refresh();
		applyDialogStyle();
	}

	setCenterPos(CC_ALONG_X | CC_ALONG_Y);

	const int field_y = has_hint ? (pad + hint_h + gap) : pad;

	cid_hint->setTextFont(hint_font);
	cid_hint->setDimensionsAll(pad, pad, body_w, hint_h);
	cid_hint->setText(visible_hint,
		CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
		hint_font,
		cid_error_text.empty() ?
		COL_MENUCONTENT_TEXT_PLUS_1 :
		COL_RED);
	cid_hint->allowPaint(has_hint);

	cid_field->setFont(input_font);
	applyFieldStyle();
	cid_field->setCaretWidth(getInputDialogCaretWidth(input_font));
	cid_field->setPadding(getInputFieldPaddingX(), getInputFieldPaddingY());
	cid_field->setDimensionsAll(field_x, field_y, field_w, field_h);

	if (cid_keyboard)
	{
		cid_keyboard->setDimensionsAll(field_x,
			field_y + field_h + gap,
			field_w,
			keyboard_h);
		cid_keyboard->refreshLayout();
	}

	/* Only after every child sits at its new place: the first paint of
	 * the resized window saves the backdrop it now really covers. */
	if (geometry_changed)
		paint(CC_SAVE_SCREEN_YES);
}

void CCInputDialogBase::initFooterButtons(const std::string &layout_name)
{
	/* With an on-screen keyboard the footer also names blue and setup:
	 * the legacy keyboard showed caps and the layout name there, and a
	 * key that is not written down might as well not exist. */
	std::vector<button_label_cc> buttons(layout_name.empty() ? 4 : 6);

	buttons[0].button = NEUTRINO_ICON_BUTTON_RED;
	buttons[0].locale = LOCALE_STRINGINPUT_SAVE;
	buttons[0].btn_result = RES_SAVE;
	buttons[0].directKeys.push_back(CRCInput::RC_red);
	buttons[0].directKeys.push_back(CRCInput::RC_ok);

	buttons[1].button = NEUTRINO_ICON_BUTTON_GREEN;
	buttons[1].locale = LOCALE_STRINGINPUT_BACKSPACE;
	buttons[1].btn_result = RES_BACKSPACE;
	buttons[1].directKeys.push_back(CRCInput::RC_green);

	buttons[2].button = NEUTRINO_ICON_BUTTON_YELLOW;
	buttons[2].locale = LOCALE_STRINGINPUT_CLEAR;
	buttons[2].btn_result = RES_CLEAR;
	buttons[2].directKeys.push_back(CRCInput::RC_yellow);

	buttons[3].button = NEUTRINO_ICON_BUTTON_HOME;
	buttons[3].locale = LOCALE_MESSAGEBOX_CANCEL;
	buttons[3].btn_result = RES_CANCEL;
	buttons[3].directKeys.push_back(CRCInput::RC_home);
	buttons[3].directKeys.push_back(CRCInput::RC_back);

	if (!layout_name.empty())
	{
		buttons[4].button = NEUTRINO_ICON_BUTTON_BLUE;
		buttons[4].locale = LOCALE_STRINGINPUT_CAPS;
		buttons[4].btn_result = RES_CAPS;
		buttons[4].directKeys.push_back(CRCInput::RC_blue);

		buttons[5].button = NEUTRINO_ICON_BUTTON_MENU;
		buttons[5].text = layout_name;
		buttons[5].btn_result = RES_LAYOUT;
		buttons[5].directKeys.push_back(CRCInput::RC_setup);
	}

	getFooterObject()->setButtonLabels(buttons,
		CFrameBuffer::getInstance()->scale2Res(20),
		CFrameBuffer::getInstance()->scale2Res(220));
}

int CCInputDialogBase::sendButtonKey(neutrino_msg_t msg)
{
	CComponentsFrmChain *container = getFooterObject()->getButtonChainObject();
	if (!container)
		return -1;

	for (size_t i = 0; i < container->size(); i++)
	{
		CComponentsButton *btn = getFooterObject()->getButtonLabel(i);
		if (btn && btn->hasButtonDirectKey(msg))
			return btn->getButtonResult();
	}

	return -1;
}

bool CCInputDialogBase::applyBufferResult(int btn_res, CCInputBuffer *buffer)
{
	if (!buffer)
		return false;

	switch (btn_res)
	{
		case RES_BACKSPACE:
			return buffer->backspace();
		case RES_CLEAR:
			/* clear() returns void and would report a change even on
			 * an empty buffer - the contract says "changed", so ask
			 * first instead of repainting for nothing. */
			if (buffer->empty())
				return false;
			buffer->clear();
			return true;
		default:
			return false;
	}
}

void CCTextInputDialog::syncDialogState()
{
	cid_buffer.setMaxChars(cid_max_chars);
	cid_buffer.setAllowEmpty(cid_allow_empty);
	cid_buffer.setFilterMode(cid_filter_mode);
	cid_field->setBuffer(&cid_buffer);
	applyFieldStyle();
	cid_field->setPasswordMode(cid_password_mode);
	cid_field->setPlaceholder(cid_placeholder);
	/* Empty field: start on the keyboard, typing is what comes next.
	 * An existing value keeps the field focused, so left/right edit
	 * the text at once - the keyboard would swallow both. */
	bool field_focus = true;
	if (cid_keyboard)
	{
		cid_keyboard->setBuffer(&cid_buffer);
		field_focus = !cid_buffer.getText().empty();
		cid_keyboard->setKeyFocus(!field_focus);
	}
	cid_field->setFieldFocus(field_focus);
	cid_field->setErrorState(false);
	cid_error_text.clear();
	layoutDialogItems();
}

std::string CCTextInputDialog::getFieldFontReference() const
{
	if (!cid_field_font_reference.empty())
		return cid_field_font_reference;

	if (!cid_placeholder.empty())
		return cid_placeholder;

	if (!ccw_caption.empty())
		return ccw_caption;

	return "W";
}

std::string CCTextInputDialog::getVisibleHintText() const
{
	return cid_error_text.empty() ? cid_hint_text : cid_error_text;
}

void CCTextInputDialog::clearInlineError()
{
	if (cid_error_text.empty())
		return;

	cid_error_text.clear();
	/* The frame goes with the text: the remote paths reset it next to
	 * their clear call, the keyboard path arrives only here. */
	cid_field->setErrorState(false);
	layoutDialogItems();
	paint(false);
}

void CCTextInputDialog::showInlineError(const std::string &text)
{
	if (cid_error_text == text)
		return;

	cid_error_text = text;
	layoutDialogItems();
	paint(false);
}

bool CCTextInputDialog::confirmDiscard()
{
	/* The caret blinks from its own timer thread and would keep
	 * painting and restoring its snapshot right through the modal
	 * message box. Disarm it first; when the user stays, the repaint
	 * of the field arms it again. */
	cid_field->cancelCaretBlink();

	const bool discard = ShowMsg(ccw_caption,
			LOCALE_MESSAGEBOX_DISCARD,
			CMsgBox::mbrYes,
			CMsgBox::mbYes | CMsgBox::mbCancel) != CMsgBox::mbrCancel;

	if (!discard)
		cid_field->paint(false);

	return discard;
}

void CCTextInputDialog::showMaxCharsError()
{
	cid_field->setErrorState(true);
	/* insert() refuses for two reasons and does not say which; the
	 * buffer state does. Only a really full buffer is "max chars" -
	 * anything else was the filter. */
	if (cid_max_chars && cid_buffer.size() >= cid_max_chars)
		showInlineError(g_Locale->getText(LOCALE_STRINGINPUT_MAXCHARS_REACHED));
	else
		showInlineError(g_Locale->getText(LOCALE_STRINGINPUT_CHAR_NOT_ALLOWED));
	cid_field->paint(false);
}

void CCTextInputDialog::focusFieldFromKeyboard()
{
	setFieldFocus(true);
}

std::string CCTextInputDialog::getVfdText() const
{
	/* The field masks a password on screen; the VFD line has to do the
	 * same or the secret scrolls over the front display in clear text. */
	if (cid_password_mode)
		return std::string(cid_buffer.size(), '*');

	return cid_buffer.getText();
}

void CCTextInputDialog::refreshField()
{
	/* A glyph made it into the buffer, so a standing rejection notice
	 * no longer describes the state. The remote paths clear it at
	 * every accepted key; the keyboard path arrives here. */
	clearInlineError();
	cid_field->ensureCursorVisible();
	cid_field->paint(false);
	CVFD::getInstance()->showMenuText(1,
		getVfdText().c_str(),
		cid_buffer.getCursor() + 1);
}

void CCTextInputDialog::setFieldFocus(const bool &focused)
{
	if (!cid_keyboard)
		return;

	cid_field->setFieldFocus(focused);
	cid_keyboard->setKeyFocus(!focused);
	cid_field->paint(false);
}

bool CCTextInputDialog::save()
{
	if (!cid_buffer.isAcceptable())
	{
		cid_field->setErrorState(true);
		showInlineError(g_Locale->getText(cid_allow_empty ?
				LOCALE_STRINGINPUT_SAVE_FAILED :
				LOCALE_STRINGINPUT_EMPTY_NOT_ALLOWED));
		cid_field->paint(false);
		return false;
	}

	if (!cid_validator.empty())
	{
		std::string reason;
		if (!cid_validator(cid_buffer.getText(), reason))
		{
			cid_field->setErrorState(true);
			showInlineError(reason.empty() ?
				g_Locale->getText(LOCALE_STRINGINPUT_SAVE_FAILED) :
				reason);
			cid_field->paint(false);
			return false;
		}
	}

	if (cid_value)
		*cid_value = cid_buffer.getText();

	if (cid_observ && cid_value)
		cid_observ->changeNotify(ccw_caption, (void *) cid_value->c_str());

	OnAfterSave();
	cid_saved = true;
	return true;
}

void CCTextInputDialog::setHintText(const std::string &hint)
{
	cid_hint_text = hint;
	if (cid_hint)
		layoutDialogItems();
}

void CCTextInputDialog::setPlaceholder(const std::string &text)
{
	cid_placeholder = text;
	if (cid_field)
	{
		cid_field->setPlaceholder(text);
		layoutDialogItems();
	}
}

void CCTextInputDialog::setFieldFontReference(const std::string &text)
{
	cid_field_font_reference = text;
	if (cid_field)
		layoutDialogItems();
}

void CCTextInputDialog::setMaxChars(size_t max_chars)
{
	cid_max_chars = max_chars;
	cid_buffer.setMaxChars(max_chars);
}

void CCTextInputDialog::setAllowEmpty(bool allow_empty)
{
	cid_allow_empty = allow_empty;
	cid_buffer.setAllowEmpty(allow_empty);
}

void CCTextInputDialog::setFilterMode(CCInputBuffer::input_filter_mode_t mode)
{
	cid_filter_mode = mode;
	cid_buffer.setFilterMode(mode);
}

void CCTextInputDialog::setValidator(const sigc::slot<bool, const std::string &, std::string &> &validator)
{
	cid_validator = validator;
}

void CCTextInputDialog::enablePasswordMode(bool enable)
{
	cid_password_mode = enable;
	if (cid_field)
		cid_field->setPasswordMode(enable);
}

void CCTextInputDialog::enableOnScreenKeyboard(bool enable)
{
	cid_enable_keyboard = enable;

	/* Enabling only records the wish: dialogs are often built while a
	 * menu is assembled, and three keyboards of 56 keys each would be
	 * constructed before the user opens a single field. exec() builds
	 * the keyboard on entry instead. */
	if (cid_enable_keyboard)
		return;

	/* Symmetric on the way out: without this an already created
	 * keyboard stayed in the body and the message loop kept feeding
	 * it, so disabling was a flag with no effect. Focus goes back to
	 * the field first - removeCCItem() deletes the item. */
	if (!cid_keyboard)
		return;

	setFieldFocus(true);
	/* Erase before removing: removeCCItem() deletes the object, and a
	 * deleted keyboard cannot clean its own pixels off the screen. */
	if (cid_keyboard->isOnScreen())
		cid_keyboard->kill();
	getBodyObject()->removeCCItem(cid_keyboard);
	cid_keyboard = NULL;
	layoutDialogItems();
}

std::string &CCTextInputDialog::getValue(void)
{
	if (!cid_value)
	{
		valueStringTmp.clear();
		return valueStringTmp;
	}

	if (!cid_password_mode)
		return *cid_value;

	CCInputBuffer display_buffer;
	display_buffer.setText(*cid_value);
	valueStringTmp.assign(display_buffer.size(), '*');
	return valueStringTmp;
}

int CCTextInputDialog::exec(CMenuTarget *parent, const std::string & /*actionKey*/)
{
	neutrino_msg_t msg = CRCInput::RC_nokey;
	neutrino_msg_data_t data = 0;
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	const std::string original_value = cid_value ? *cid_value : "";

	if (cid_enable_keyboard && !cid_keyboard)
		createKeyboard();

	cid_saved = false;
	cid_buffer.setText(original_value);
	cid_buffer.moveEnd();
	syncDialogState();
	cid_field->ensureCursorVisible();
	paint();

	CVFD::getInstance()->showMenuText(1,
		getVfdText().c_str(),
		cid_buffer.getCursor() + 1);

	uint64_t timeoutEnd =
		CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd =
				CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		bool state_changed = false;

		/* 0. On-screen keyboard, only while it has focus. It refuses
		 * the footer keys itself, so save, backspace, clear and cancel
		 * still reach the layers below. Repaint runs through the
		 * OnAfterKey slot. */
		if (cid_keyboard && cid_keyboard->hasKeyFocus() &&
			cid_keyboard->handleMsg(msg))
			continue;

		/* 1. Cursor navigation (explicit, not button-dispatched) */
		if (msg == CRCInput::RC_left)
		{
			cid_field->setErrorState(false);
			clearInlineError();
			cid_buffer.moveLeft();
			state_changed = true;
		}
		else if (msg == CRCInput::RC_right)
		{
			cid_field->setErrorState(false);
			clearInlineError();
			cid_buffer.moveRight();
			state_changed = true;
		}
		else if ((msg == CRCInput::RC_down || msg == CRCInput::RC_up) &&
			cid_keyboard)
		{
			/* up is the counterpart to OnLeaveTop - without it the
			 * field would be a dead end for that direction */
			cid_field->setErrorState(false);
			clearInlineError();
			setFieldFocus(false);
		}
		else if (msg == CRCInput::RC_page_up)
		{
			cid_field->setErrorState(false);
			clearInlineError();
			cid_buffer.moveHome();
			state_changed = true;
		}
		else if (msg == CRCInput::RC_page_down)
		{
			cid_field->setErrorState(false);
			clearInlineError();
			cid_buffer.moveEnd();
			state_changed = true;
		}
		else if (msg == CRCInput::RC_backspace ||
			msg == CRCInput::RC_rewind)
		{
			/* The hard key twin of the green footer button, kept for
			 * the keyboards and remotes that carry one. Routed through
			 * the same shared method rather than calling backspace()
			 * again: two spellings of one action are how the footer
			 * results drifted apart in the first place. */
			cid_field->setErrorState(false);
			clearInlineError();
			if (applyBufferResult(RES_BACKSPACE, &cid_buffer))
				state_changed = true;
		}
		else if (msg == CRCInput::RC_timeout)
		{
			if (cid_buffer.getText() != original_value &&
				!confirmDiscard())
			{
				/* RC_timeout is above RC_MaxRC, so the renewal at
				 * the loop head never ran - without a fresh
				 * deadline the next getMsgAbsoluteTimeout() fires
				 * immediately and the discard prompt loops. */
				timeoutEnd = CRCInput::calcTimeoutEnd(
						g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
				continue;
			}

			loop = false;
			res = menu_return::RETURN_EXIT_REPAINT;
		}
		else
		{
			/* 2. Action dispatch via footer buttons. listModeKey() is
			 * checked only after this, like every legacy input widget
			 * does: key_favorites is freely bindable, and bound to a
			 * footer color it must not swallow save or cancel. */
			int btn_res = sendButtonKey(msg);
			if (btn_res >= 0)
			{
				switch (btn_res)
				{
					case RES_SAVE:
						if (save())
							loop = false;
						break;
					case RES_CANCEL:
						if (cid_buffer.getText() != original_value &&
							!confirmDiscard())
							break;
						loop = false;
						res = menu_return::RETURN_EXIT_REPAINT;
						break;
					case RES_CAPS:
						if (cid_keyboard)
							cid_keyboard->toggleCaps();
						break;
					case RES_LAYOUT:
						if (cid_keyboard)
						{
							cid_keyboard->toggleLayout();
							/* The rebuilt chain is re-centered
							 * around the new layout name and the
							 * old container drops without its
							 * pixels; a plain repaint would skip
							 * paintInit() on the painted footer.
							 * Kill it first, in its own colour -
							 * a parented kill() would flash the
							 * window body colour instead. */
							if (getFooterObject())
								getFooterObject()->kill(COL_MENUFOOT_PLUS_0, true);
							initFooterButtons(cid_keyboard->getLayoutName());
							if (getFooterObject())
								getFooterObject()->paint(false);
						}
						break;
					default:
						/* Every result this dialog does not name is
						 * the base's to interpret. Naming them here
						 * would put the label in one place and the
						 * effect in another again - and a dialog that
						 * forgot a label would show a footer button
						 * that does nothing, which is the defect this
						 * whole change came from. */
						cid_field->setErrorState(false);
						clearInlineError();
						if (applyBufferResult(btn_res, &cid_buffer))
							state_changed = true;
						break;
				}
			}
			else if (CNeutrinoApp::getInstance()->listModeKey(msg))
			{
				continue;
			}
			else
			{
				/* 3. Text input (unicode / numeric) */
				std::string glyph;
				const char *unicode_value =
					CRCInput::getUnicodeValue(msg);
				if (unicode_value && *unicode_value)
					glyph = unicode_value;
				else if (CRCInput::isNumeric(msg))
					glyph = std::string(1,
							'0' +
							CRCInput::getNumericValue(msg));

				if (!glyph.empty())
				{
					cid_field->setErrorState(false);
					clearInlineError();
					if (cid_buffer.insert(glyph))
						state_changed = true;
					else
						showMaxCharsError();
				}
				/* 4. System messages */
				else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) &
					messages_return::cancel_all)
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}
		}

		if (state_changed)
			refreshField();
	}

	if (!cid_saved && cid_value)
		*cid_value = original_value;

	hide();
	return res;
}

void CCTextInputDialog::hide()
{
	if (cid_field)
		cid_field->hide();
	CComponentsWindow::hide();
}
