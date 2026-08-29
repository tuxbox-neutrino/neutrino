/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern input row form
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_frm_input_row.h"

#include <algorithm>
#include <driver/fontrenderer.h>
#include <driver/neutrinofonts.h>

namespace
{
int getInputRowFieldInset()
{
	return std::max(OFFSET_INNER_SMALL,
			CFrameBuffer::getInstance()->scale2Res(18));
}

int getInputRowFieldPaddingX()
{
	return std::max(OFFSET_INNER_SMALL,
			CFrameBuffer::getInstance()->scale2Res(12));
}

int getInputRowFieldPaddingY()
{
	return std::max(OFFSET_INNER_MIN,
			CFrameBuffer::getInstance()->scale2Res(3));
}

int getInputRowGap()
{
	return std::max(OFFSET_INNER_MIN,
			CFrameBuffer::getInstance()->scale2Res(6));
}

int getInputRowGlyphHeight(Font *font)
{
	if (!font)
		return 0;

	return std::max(0, font->getAscender() + font->getDescender());
}

Font *getInputRowDynFont(int max_w,
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

Font *getInputRowLabelFont()
{
	return g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];
}

Font *getInputRowFieldFont(const int field_w, const std::string &reference_text)
{
	return getInputRowDynFont(field_w,
			CFrameBuffer::getInstance()->scale2Res(36),
			reference_text,
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);
}

int getInputRowFieldHeight(Font *font)
{
	const int glyph_h = getInputRowGlyphHeight(font);
	const int vertical_pad = getInputRowFieldPaddingY();

	return std::max(CFrameBuffer::getInstance()->scale2Res(36),
			glyph_h + 2 * vertical_pad + 2 * FRAME_WIDTH_MIN);
}

int getInputRowCaretWidth(Font *font)
{
	if (!font)
		return std::max(3, CFrameBuffer::getInstance()->scale2Res(3));

	return std::max(std::max(3, CFrameBuffer::getInstance()->scale2Res(3)),
			font->getRenderWidth("|") / 2);
}
}

CComponentsInputRow::CComponentsInputRow(const int &x_pos,
	const int &y_pos,
	const int &w,
	const int &h,
	const std::string &label_text,
	const std::string &placeholder,
	CComponentsForm *parent,
	int shadow_mode,
	const fb_pixel_t &color_frame,
	const fb_pixel_t &color_body,
	const fb_pixel_t &color_shadow)
	: CComponentsForm(x_pos,
		  y_pos,
		  w,
		  h,
		  parent,
		  shadow_mode,
		  color_frame,
		  color_body,
		  color_shadow)
{
	cc_item_type.id = CC_ITEMTYPE_FRM_INPUT_ROW;
	cc_item_type.name = "cc_input_row";

	cir_label = NULL;
	cir_field = NULL;
	cir_label_text = label_text;
	cir_placeholder = placeholder;
	cir_field_font_reference.clear();
	cir_password_mode = false;
	cir_label_col = COL_MENUCONTENT_TEXT_PLUS_1;
	cir_field_col_text = COL_MENUCONTENTDARK_TEXT_PLUS_1;
	cir_field_col_frame = COL_FRAME_PLUS_0;
	cir_field_col_body = COL_MENUCONTENTDARK_PLUS_0;
	cir_field_col_body_focus = COL_MENUCONTENTDARK_PLUS_2;
	cir_field_col_placeholder = COL_MENUCONTENTDARK_TEXT_PLUS_2;

	doPaintBg(false);
	enableFrame(false, 0);
	disableShadow();

	initRow();
	refreshLayout();
}

void CComponentsInputRow::initRow()
{
	if (!cir_label)
	{
		cir_label = new CComponentsLabel(this);
		cir_label->doPaintBg(false);
		cir_label->doPaintTextBoxBg(false);
	}

	if (!cir_field)
	{
		cir_field = new CCInputField(0,
			0,
			0,
			0,
			NULL,
			this,
			CC_SHADOW_OFF,
			cir_field_col_text,
			cir_field_col_frame,
			cir_field_col_body,
			cir_field_col_body_focus,
			COL_SHADOW_PLUS_0);
		cir_field->setBuffer(&cir_buffer);
		cir_field->setPlaceholder(cir_placeholder);
		cir_field->setPasswordMode(cir_password_mode);
	}
}

void CComponentsInputRow::syncFieldStyle()
{
	if (!cir_field)
		return;

	cir_field->setColors(cir_field_col_text,
		cir_field_col_frame,
		cir_field_col_body,
		cir_field_col_body_focus);
	cir_field->setPlaceholderColor(cir_field_col_placeholder);
}

std::string CComponentsInputRow::getFieldFontReference() const
{
	if (!cir_field_font_reference.empty())
		return cir_field_font_reference;

	if (!cir_placeholder.empty())
		return cir_placeholder;

	return "AbCdEf0123456789";
}

void CComponentsInputRow::setLabelText(const std::string &text)
{
	cir_label_text = text;
	refreshLayout();
}

void CComponentsInputRow::setPlaceholder(const std::string &text)
{
	cir_placeholder = text;
	if (cir_field)
		cir_field->setPlaceholder(cir_placeholder);
	refreshLayout();
}

void CComponentsInputRow::setFieldFontReference(const std::string &text)
{
	cir_field_font_reference = text;
	refreshLayout();
}

void CComponentsInputRow::setText(const std::string &text)
{
	cir_buffer.setText(text);
	if (cir_field)
		cir_field->ensureCursorVisible();
}

const std::string &CComponentsInputRow::getText() const
{
	return cir_buffer.getText();
}

void CComponentsInputRow::setPasswordMode(bool enabled)
{
	cir_password_mode = enabled;
	if (cir_field)
		cir_field->setPasswordMode(cir_password_mode);
}

void CComponentsInputRow::setAllowEmpty(bool allow_empty)
{
	cir_buffer.setAllowEmpty(allow_empty);
}

void CComponentsInputRow::setMaxChars(size_t max_chars)
{
	cir_buffer.setMaxChars(max_chars);
}

void CComponentsInputRow::setFilterMode(CCInputBuffer::input_filter_mode_t mode)
{
	cir_buffer.setFilterMode(mode);
}

void CComponentsInputRow::setFieldFocus(bool focused)
{
	if (cir_field)
		cir_field->setFieldFocus(focused);
}

void CComponentsInputRow::setErrorState(bool enabled)
{
	if (cir_field)
		cir_field->setErrorState(enabled);
}

bool CComponentsInputRow::getErrorState() const
{
	return cir_field ? cir_field->getErrorState() : false;
}

void CComponentsInputRow::setLabelColor(const fb_pixel_t &color)
{
	cir_label_col = color;
	if (cir_label)
		cir_label->setTextColor(cir_label_col);
}

void CComponentsInputRow::setFieldColors(const fb_pixel_t &color_text,
	const fb_pixel_t &color_frame,
	const fb_pixel_t &color_body,
	const fb_pixel_t &color_body_focus,
	const fb_pixel_t &color_placeholder)
{
	cir_field_col_text = color_text;
	cir_field_col_frame = color_frame;
	cir_field_col_body = color_body;
	cir_field_col_body_focus = color_body_focus;
	cir_field_col_placeholder = color_placeholder;
	syncFieldStyle();
}

void CComponentsInputRow::ensureCursorVisible()
{
	if (cir_field)
		cir_field->ensureCursorVisible();
}

int CComponentsInputRow::getPreferredHeight() const
{
	Font *label_font = getInputRowLabelFont();
	const int field_inset = getInputRowFieldInset();
	const int field_w = std::max(0, width - 2 * field_inset);
	Font *field_font = getInputRowFieldFont(field_w, getFieldFontReference());
	const int label_h = label_font ? label_font->getHeight() : 0;
	const int gap = getInputRowGap();
	const int field_h = getInputRowFieldHeight(field_font);

	return label_h + gap + field_h;
}

void CComponentsInputRow::refreshLayout()
{
	if (!cir_label || !cir_field)
		return;

	Font *label_font = getInputRowLabelFont();
	const int label_h = label_font ? label_font->getHeight() : 0;
	const int gap = getInputRowGap();
	const int field_inset = getInputRowFieldInset();
	const int field_w = std::max(0, width - 2 * field_inset);
	Font *field_font = getInputRowFieldFont(field_w, getFieldFontReference());
	const int field_h = getInputRowFieldHeight(field_font);

	height = label_h + gap + field_h;

	cir_label->setText(cir_label_text,
		CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
		label_font,
		cir_label_col);
	cir_label->setDimensionsAll(0, 0, width, label_h);

	cir_field->setFont(field_font);
	cir_field->setDimensionsAll(field_inset,
		label_h + gap,
		field_w,
		field_h);
	cir_field->setPlaceholder(cir_placeholder);
	cir_field->setPasswordMode(cir_password_mode);
	cir_field->setCaretWidth(getInputRowCaretWidth(field_font));
	cir_field->setPadding(getInputRowFieldPaddingX(),
		getInputRowFieldPaddingY());
	syncFieldStyle();
}

void CComponentsInputRow::paint(const bool &do_save_bg)
{
	refreshLayout();
	paintForm(do_save_bg);
}

void CComponentsInputRow::hide()
{
	if (cir_field)
		cir_field->hide();

	CComponentsForm::hide();
}
