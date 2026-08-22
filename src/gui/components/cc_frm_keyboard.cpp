/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	On-screen keyboard component
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_frm_keyboard.h"
#include "cc_frm_button.h"
#include "cc_input_buffer.h"

#include <algorithm>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

CComponentsKeyboard::CComponentsKeyboard(const int &x_pos,
	const int &y_pos,
	const int &w,
	const int &h,
	CComponentsForm *parent,
	const int &shadow_mode,
	const fb_pixel_t &color_frame,
	const fb_pixel_t &color_body,
	const fb_pixel_t &color_shadow)
	: CComponentsForm(x_pos, y_pos, w, h, parent, shadow_mode, color_frame, color_body, color_shadow)
{
	cc_item_type.id = CC_ITEMTYPE_FRM_KEYBOARD;
	cc_item_type.name = "cc_frm_keyboard";

	ck_buffer = NULL;
	ck_font = NULL;
	ck_row = 0;
	ck_column = 0;
	ck_has_focus = false;
	ck_compact = false;
	ck_col_key_body = COL_MENUCONTENT_PLUS_3;
	ck_col_key_text = COL_MENUCONTENT_TEXT;

	ck_layout.initByLocale(g_settings.language);

	doPaintBg(true);
	buildKeys();
}

int CComponentsKeyboard::getKeyGap() const
{
	const int gap = CFrameBuffer::getInstance()->scale2Res(ck_compact ? 2 : 4);

	return std::max(1, gap);
}

int CComponentsKeyboard::getKeyHeight() const
{
	Font *font = ck_font ? ck_font : g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	const int font_h = font ? font->getHeight() : 0;
	const int min_h = CFrameBuffer::getInstance()->scale2Res(ck_compact ? 26 : 36);

	return std::max(min_h, font_h + 2 * getKeyGap());
}

int CComponentsKeyboard::getKeyWidth(const int &w) const
{
	const int columns = CKeyboardLayoutData::getColumnCount();
	const int gap = getKeyGap();
	const int inner = w - 2 * fr_thickness - 2 * gap - (columns - 1) * gap;

	return std::max(1, inner / columns);
}

int CComponentsKeyboard::getPreferredHeight() const
{
	const int rows = CKeyboardLayoutData::getRowCount();
	const int gap = getKeyGap();

	return 2 * fr_thickness + 2 * gap + rows * getKeyHeight() + (rows - 1) * gap;
}

bool CComponentsKeyboard::needsCompactMode(const int &max_h) const
{
	return getPreferredHeight() > max_h;
}

void CComponentsKeyboard::setCompactMode(const bool &compact)
{
	if (ck_compact == compact)
		return;

	ck_compact = compact;
	refreshLayout();
}

std::string CComponentsKeyboard::getKeyCaption(const int &row, const int &column) const
{
	const std::string glyph = ck_layout.glyphAt(row, column);

	/* A blank glyph is the space bar. Rendered as-is it would be an empty
	 * key the user cannot tell from an unassigned one, so it carries a
	 * caption while still inserting a plain space. */
	if (glyph == " ")
		return g_Locale->getText(LOCALE_STRINGINPUT_SPACE);

	return glyph;
}

void CComponentsKeyboard::buildKeys()
{
	const int rows = CKeyboardLayoutData::getRowCount();
	const int columns = CKeyboardLayoutData::getColumnCount();

	ck_rows.clear();
	ck_keys.clear();
	ck_rows.reserve(rows);
	ck_keys.reserve(rows);

	for (int r = 0; r < rows; r++)
	{
		CComponentsFrmChain *row = new CComponentsFrmChain(0, 0, width, getKeyHeight(),
			NULL, CC_DIR_X, this);
		/* The row body is opaque and carries the container colour on
		 * purpose: CComponentsItem::kill() erases a child to
		 * cc_parent->getColorBody(), so a single key can be repainted
		 * on focus changes without leaving a hole. */
		row->doPaintBg(true);
		row->setColorBody(col_body_std);
		row->setFrameThickness(0);
		ck_rows.push_back(row);

		std::vector<CComponentsButton *> keys;
		keys.reserve(columns);
		for (int c = 0; c < columns; c++)
		{
			/* Built at the final key width, not at a placeholder:
			 * CComponentsButton::initCaption() shrinks the button
			 * and drops to a dynamic font when the caption does not
			 * fit, and that font survives a later resize. */
			CComponentsButton *key = new CComponentsButton(0, 0,
				getKeyWidth(width), getKeyHeight(),
				getKeyCaption(r, c), "", row);
			/* Keys stay opaque as well. With doPaintBg(false) an item
			 * without frame or shadow paints no fb layer at all, so
			 * paintFbItems() never sets is_painted and the erase guard
			 * "if (isPainted()) hide();" in paintCCItems() never fires -
			 * the key would never be cleaned up. */
			key->doPaintBg(true);
			key->setColorBody(ck_col_key_body);
			key->setButtonTextColor(ck_col_key_text);
			key->setCorner(RADIUS_SMALL, CORNER_ALL);
			keys.push_back(key);
		}
		ck_keys.push_back(keys);
	}

	refreshLayout();
}

void CComponentsKeyboard::refreshLayout()
{
	if (ck_rows.empty())
		return;

	const int rows = CKeyboardLayoutData::getRowCount();
	const int columns = CKeyboardLayoutData::getColumnCount();
	const int gap = getKeyGap();
	const int key_w = getKeyWidth(width);
	const int key_h = getKeyHeight();
	const int row_w = columns * key_w + (columns - 1) * gap;
	const int row_x = std::max(fr_thickness, (width - row_w) / 2);
	int row_y = fr_thickness + gap;

	for (int r = 0; r < rows; r++)
	{
		ck_rows[r]->setDimensionsAll(row_x, row_y, row_w, key_h);
		ck_rows[r]->setColorBody(col_body_std);

		int key_x = 0;
		for (int c = 0; c < columns; c++)
		{
			ck_keys[r][c]->setDimensionsAll(key_x, 0, key_w, key_h);
			/* Drop a dynamic font picked for a narrower key, so the
			 * caption is measured against the width it now has. */
			ck_keys[r][c]->resetButtonFont();
			if (ck_font)
				ck_keys[r][c]->setButtonFont(ck_font);
			ck_keys[r][c]->setCaption(getKeyCaption(r, c));
			key_x += key_w + gap;
		}

		row_y += key_h + gap;
	}

	applyKeyStates();
}

void CComponentsKeyboard::applyKeyStates()
{
	const int rows = CKeyboardLayoutData::getRowCount();
	const int columns = CKeyboardLayoutData::getColumnCount();

	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < columns; c++)
		{
			const bool selected = ck_has_focus && r == ck_row && c == ck_column;
			/* Every colour is passed explicitly: the defaults of
			 * setSelected() would put the global menu colours and a
			 * 3px frame on every key and so undo setKeyColors() on
			 * each focus move. Focus shows as a frame; the body keeps
			 * the configured key colour in both states. */
			ck_keys[r][c]->setSelected(selected,
				COL_MENUCONTENTSELECTED_PLUS_0,
				ck_col_key_body,
				ck_col_key_body,
				ck_col_key_body,
				0,
				2);
		}
	}
}

void CComponentsKeyboard::repaintKey(const int &row, const int &column)
{
	if (row < 0 || row >= (int) ck_keys.size())
		return;
	if (column < 0 || column >= (int) ck_keys[row].size())
		return;
	if (!isPainted())
		return;

	/* Only the two keys that changed, not the whole keyboard: the key is
	 * opaque on an opaque row, so kill() erases it to the row colour and
	 * the following paint() runs the full first-paint path again. */
	ck_keys[row][column]->kill();
	ck_keys[row][column]->paint(CC_SAVE_SCREEN_NO);
}

void CComponentsKeyboard::setBuffer(CCInputBuffer *buffer)
{
	ck_buffer = buffer;
}

void CComponentsKeyboard::setFont(Font *font)
{
	ck_font = font;
	refreshLayout();
}

void CComponentsKeyboard::setKeyColors(const fb_pixel_t &color_body, const fb_pixel_t &color_text)
{
	ck_col_key_body = color_body;
	ck_col_key_text = color_text;

	for (size_t r = 0; r < ck_keys.size(); r++)
	{
		for (size_t c = 0; c < ck_keys[r].size(); c++)
		{
			ck_keys[r][c]->setColorBody(ck_col_key_body);
			ck_keys[r][c]->setButtonTextColor(ck_col_key_text);
		}
	}
}

void CComponentsKeyboard::setKeyFocus(const bool &focused)
{
	if (ck_has_focus == focused)
		return;

	ck_has_focus = focused;
	applyKeyStates();
	repaintKey(ck_row, ck_column);
}

void CComponentsKeyboard::setLayoutByLocale(const std::string &locale)
{
	ck_layout.initByLocale(locale);
	refreshLayout();
}

void CComponentsKeyboard::toggleCaps()
{
	ck_layout.toggleCaps();
	refreshLayout();

	/* Every caption changed, so this is a structural change and the whole
	 * container is repainted - unlike a focus move, which touches two keys. */
	if (isPainted())
	{
		kill();
		paint(CC_SAVE_SCREEN_NO);
	}
}

void CComponentsKeyboard::toggleLayout()
{
	ck_layout.nextLayout();
	refreshLayout();

	if (isPainted())
	{
		kill();
		paint(CC_SAVE_SCREEN_NO);
	}
}

void CComponentsKeyboard::moveFocus(const int &d_row, const int &d_column)
{
	const int rows = CKeyboardLayoutData::getRowCount();
	const int columns = CKeyboardLayoutData::getColumnCount();
	const int old_row = ck_row;
	const int old_column = ck_column;

	ck_row = std::min(std::max(0, ck_row + d_row), rows - 1);
	ck_column = std::min(std::max(0, ck_column + d_column), columns - 1);

	if (ck_row == old_row && ck_column == old_column)
		return;

	applyKeyStates();
	repaintKey(old_row, old_column);
	repaintKey(ck_row, ck_column);
}

void CComponentsKeyboard::insertFocusedGlyph()
{
	if (!ck_buffer)
		return;

	const std::string glyph = ck_layout.glyphAt(ck_row, ck_column);
	if (glyph.empty())
		return;

	/* insert() already answers max chars and the filter mode, so the
	 * rejection travels the same path the typed input uses. */
	if (ck_buffer->insert(glyph))
		OnAfterKey();
	else
		OnKeyRejected();
}

bool CComponentsKeyboard::handleMsg(const neutrino_msg_t &msg)
{
	if (!ck_has_focus)
		return false;

	/* Red, green, yellow, home and back drive the dialog footer. They must
	 * pass through even while the keyboard has focus, or save, delete,
	 * clear and cancel become unreachable from here. Blue and setup are
	 * not taken by the footer and switch caps and layout. */
	switch (msg)
	{
		case CRCInput::RC_left:
			moveFocus(0, -1);
			return true;
		case CRCInput::RC_right:
			moveFocus(0, 1);
			return true;
		case CRCInput::RC_up:
			if (ck_row == 0)
			{
				OnLeaveTop();
				return true;
			}
			moveFocus(-1, 0);
			return true;
		case CRCInput::RC_down:
			moveFocus(1, 0);
			return true;
		case CRCInput::RC_ok:
			insertFocusedGlyph();
			return true;
		case CRCInput::RC_blue:
			toggleCaps();
			return true;
		case CRCInput::RC_setup:
			toggleLayout();
			return true;
		default:
			break;
	}

	return false;
}
