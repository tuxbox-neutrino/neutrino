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

int getInputHintHeight(Font *font)
{
	const int font_h = font ? font->getHeight() : 0;

	return std::max(CFrameBuffer::getInstance()->scale2Res(72),
			2 * font_h + CFrameBuffer::getInstance()->scale2Res(16));
}
}

CCTextInputDialog::CCTextInputDialog(const std::string &title,
	std::string *value,
	CChangeObserver *observ,
	const std::string &icon)
	: CComponentsWindow(CC_CENTERED,
		  CC_CENTERED,
		  getInputDialogWidth(),
		  getInputDialogMinHeight(),
		  title,
		  icon,
		  NULL,
		  CC_SHADOW_ON,
		  COL_FRAME_PLUS_0,
		  COL_MENUCONTENT_PLUS_1,
		  COL_SHADOW_PLUS_0,
		  HINTBOX_DEFAULT_FRAME_WIDTH)
{
	cc_item_type.id = CC_ITEMTYPE_FRM_INPUT_DIALOG;
	cc_item_type.name = "cc_input_dialog";

	cid_value = value;
	valueString = cid_value ? cid_value : &valueStringTmp;
	cid_observ = observ;
	cid_hint = NULL;
	cid_field = NULL;
	cid_field_font_reference.clear();
	cid_password_mode = false;
	cid_allow_empty = true;
	cid_saved = false;
	cid_max_chars = 0;
	cid_filter_mode = CCInputBuffer::FILTER_FREE;
	cid_field_col_text = getInputFieldTextColor();
	cid_field_col_frame = getInputFieldFrameColor();
	cid_field_col_body = getInputFieldBodyColor();
	cid_field_col_body_focus = getInputFieldBodyFocusColor();
	cid_field_col_placeholder = getInputFieldPlaceholderColor();

	corner_rad = RADIUS_LARGE;
	setWindowHeaderColor(COL_MENUHEAD_PLUS_0);
	setWindowHeaderTextColor(COL_MENUHEAD_TEXT);
	setWindowFooterColor(COL_MENUFOOT_PLUS_0);
	setWindowHeaderButtons(CComponentsHeader::CC_BTN_EXIT);
	enableShadow();
	Refresh();

	initDialogItems();
	applyDialogStyle();
	initFooterButtons();
	layoutDialogItems();
}

void CCTextInputDialog::applyDialogStyle()
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
		getFooterObject()->enableButtonShadow(CC_SHADOW_ON,
			OFFSET_SHADOW / 2,
			true);
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
	const int hint_h = getInputHintHeight(hint_font);
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
		getInputHintHeight(hint_font);
	const int field_h = getInputFieldHeight(input_font);
	const int body_content_h = pad + hint_h +
		(has_hint ? gap : 0) + field_h + pad;
	const int target_h = std::max(getInputDialogMinHeight(),
			header_h + footer_h + body_content_h +
			2 * fr_thickness);

	if (height != target_h)
	{
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
}

void CCTextInputDialog::initFooterButtons()
{
	std::vector<button_label_cc> buttons(4);

	buttons[0].button = NEUTRINO_ICON_BUTTON_RED;
	buttons[0].locale = LOCALE_STRINGINPUT_SAVE;
	buttons[0].btn_result = RES_SAVE;
	buttons[0].directKeys.push_back(CRCInput::RC_red);
	buttons[0].directKeys.push_back(CRCInput::RC_ok);

	buttons[1].button = NEUTRINO_ICON_BUTTON_GREEN;
	buttons[1].locale = LOCALE_FILEBROWSER_DELETE;
	buttons[1].btn_result = RES_DELETE;
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

	getFooterObject()->setButtonLabels(buttons,
		CFrameBuffer::getInstance()->scale2Res(20),
		CFrameBuffer::getInstance()->scale2Res(220));
}

int CCTextInputDialog::sendButtonKey(neutrino_msg_t msg)
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

void CCTextInputDialog::syncDialogState()
{
	cid_buffer.setMaxChars(cid_max_chars);
	cid_buffer.setAllowEmpty(cid_allow_empty);
	cid_buffer.setFilterMode(cid_filter_mode);
	cid_field->setBuffer(&cid_buffer);
	applyFieldStyle();
	cid_field->setPasswordMode(cid_password_mode);
	cid_field->setPlaceholder(cid_placeholder);
	cid_field->setFieldFocus(true);
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

bool CCTextInputDialog::confirmDiscard() const
{
	return ShowMsg(ccw_caption,
			LOCALE_MESSAGEBOX_DISCARD,
			CMsgBox::mbrYes,
			CMsgBox::mbYes | CMsgBox::mbCancel) != CMsgBox::mbrCancel;
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

	cid_saved = false;
	cid_buffer.setText(original_value);
	cid_buffer.moveEnd();
	syncDialogState();
	cid_field->ensureCursorVisible();
	paint();

	CVFD::getInstance()->showMenuText(1,
		cid_buffer.getText().c_str(),
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
			cid_field->setErrorState(false);
			clearInlineError();
			if (cid_buffer.backspace())
				state_changed = true;
		}
		else if (msg == CRCInput::RC_timeout)
		{
			if (cid_buffer.getText() != original_value &&
				!confirmDiscard())
				continue;

			loop = false;
			res = menu_return::RETURN_EXIT_REPAINT;
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			continue;
		}
		else
		{
			/* 2. Action dispatch via footer buttons */
			int btn_res = sendButtonKey(msg);
			if (btn_res >= 0)
			{
				switch (btn_res)
				{
					case RES_SAVE:
						if (save())
							loop = false;
						break;
					case RES_DELETE:
						cid_field->setErrorState(false);
						clearInlineError();
						if (cid_buffer.erase())
							state_changed = true;
						break;
					case RES_CLEAR:
						cid_field->setErrorState(false);
						clearInlineError();
						cid_buffer.clear();
						state_changed = true;
						break;
					case RES_CANCEL:
						if (cid_buffer.getText() != original_value &&
							!confirmDiscard())
							break;
						loop = false;
						res = menu_return::RETURN_EXIT_REPAINT;
						break;
				}
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
					{
						cid_field->setErrorState(true);
						showInlineError(g_Locale->getText(
								LOCALE_STRINGINPUT_MAXCHARS_REACHED));
						cid_field->paint(false);
					}
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
		{
			cid_field->ensureCursorVisible();
			cid_field->paint(false);
			CVFD::getInstance()->showMenuText(1,
				cid_buffer.getText().c_str(),
				cid_buffer.getCursor() + 1);
		}
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
