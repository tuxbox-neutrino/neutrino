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
#include <gui/widget/icons.h>

/* Caption fallbacks for the space key, in order, used when its icon is
 * not usable: U+2423 OPEN BOX is the classic space-bar symbol, and the
 * ASCII rule stands in where the font does not carry it.
 *
 * One character wide, both of them, and that is the point: the fit below
 * sizes the whole page to its widest caption, so a three-character rule
 * would shrink all 56 keys to accommodate the one key that lost its
 * icon - worst exactly where room is already short. */
static const char KEY_SPACE_GLYPH[] = "␣";
static const char KEY_SPACE_ASCII[] = "_";

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
	ck_on_screen = false;
	ck_col_key_text = COL_MENUCONTENT_TEXT;
	ck_widest_caption.clear();
	ck_widest_for_space.clear();

	ck_layout.initByLocale(g_settings.language);

	doPaintBg(true);
	buildKeys();

	CNeutrinoApp::getInstance()->OnAfterSetupFonts.connect(sigc::mem_fun(this, &CComponentsKeyboard::onAfterSetupFonts));
}

void CComponentsKeyboard::onAfterSetupFonts()
{
	/* A font handed in through setFont() is a raw pointer into g_Font[],
	 * and SetupFonts() has just deleted every one of them - keeping it
	 * would hand a dangling font to getKeyHeight() and to every key. Drop
	 * it, like CComponentsButton::resetButtonFont() drops its own.
	 *
	 * Deliberately no layout pass here. Key height, the form's own height
	 * and compact mode belong together, and only the owning dialog can
	 * recompute all three - re-laying out just the rows would grow them
	 * past a height nobody updated, and paintCCItems() silently skips a
	 * child that no longer fits. The dialog re-lays out on every exec(),
	 * which is the next time this keyboard can be seen. */
	ck_font = NULL;
	/* The reference caption was picked by measuring the old fonts. */
	ck_widest_caption.clear();
}

int CComponentsKeyboard::getKeyGap(const bool &compact) const
{
	const int gap = CFrameBuffer::getInstance()->scale2Res(compact ? 2 : 4);

	return std::max(1, gap);
}

int CComponentsKeyboard::getKeyHeight(const bool &compact) const
{
	Font *font = ck_font ? ck_font : g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	int font_h = font ? font->getHeight() : 0;

	if (!ck_font && !compact)
	{
		/* The button text font counts as well, and it is the one a user
		 * raises in the OSD font sizes to read the keys. A cell measured
		 * only against the menu font would ignore that setting - the fit
		 * below never goes under it, so the caption would end up taller
		 * than the key that has to hold it.
		 *
		 * Not in compact mode: that mode exists because the dialog is
		 * already short of room, and growing the cells there is how the
		 * keyboard would ask for a height nobody can give it. There the
		 * fit adapts to the cell instead, which is the more readable of
		 * the two answers when space has run out. */
		Font *button_font = g_Font[SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT];
		if (button_font)
			font_h = std::max(font_h, button_font->getHeight());
	}

	const int min_h = CFrameBuffer::getInstance()->scale2Res(compact ? 26 : 36);

	return std::max(min_h, font_h + 2 * getKeyGap(compact));
}

int CComponentsKeyboard::getKeyWidth(const int &w) const
{
	const int columns = CKeyboardLayoutData::getColumnCount();
	const int gap = getKeyGap(ck_compact);
	const int inner = w - 2 * fr_thickness - 2 * gap - (columns - 1) * gap;

	return std::max(1, inner / columns);
}

int CComponentsKeyboard::getPreferredHeight(const bool &compact) const
{
	const int rows = CKeyboardLayoutData::getRowCount();
	const int gap = getKeyGap(compact);

	return 2 * fr_thickness + 2 * gap + rows * getKeyHeight(compact) + (rows - 1) * gap;
}

bool CComponentsKeyboard::needsCompactMode(const int &max_h) const
{
	/* Measured against the roomy layout, never against the current one.
	 * Asking "does what I show right now fit" flips its own answer as
	 * soon as compact mode has made it fit, which is why the caller used
	 * to latch the mode on for good - and a keyboard that can never go
	 * back stays cramped for the rest of the session after one font
	 * change. This question has one answer per font size. */
	return getPreferredHeight(false) > max_h;
}

void CComponentsKeyboard::setCompactMode(const bool &compact)
{
	if (ck_compact == compact)
		return;

	ck_compact = compact;
	refreshLayout();
}

std::string CComponentsKeyboard::getSpaceCaption(Font *key_font) const
{
	/* The caption the space key falls back to when its icon is unusable.
	 * Not the localized word: that word is what made this key unreadable
	 * in the first place, so it is no fallback at all. The open-box glyph
	 * is the proper symbol, but RenderString() silently skips glyphs a
	 * font does not carry (getRenderWidth() == 0 spots exactly that) and
	 * the shipped OSD font is one of those - hence the ASCII rule as the
	 * last resort, which renders anywhere and still reads as a space bar. */
	if (key_font && key_font->getRenderWidth(KEY_SPACE_GLYPH) > 0)
		return KEY_SPACE_GLYPH;

	return KEY_SPACE_ASCII;
}

std::string CComponentsKeyboard::getWidestCaption(const std::string &space_caption) const
{
	/* Cached per page: this runs from refreshLayout(), which the dialog
	 * calls on every inline error toggle while typing, and measuring 56
	 * captions each time is a real cost on a box. The page only changes
	 * through toggleCaps(), toggleLayout() and setLayoutByLocale(), which
	 * drop the cache. */
	if (!ck_widest_caption.empty() && ck_widest_for_space == space_caption)
		return ck_widest_caption;

	/* Measured with a fixed font: only the RELATIVE widths matter for
	 * picking the reference, and those barely change with size. */
	Font *font = g_Font[SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT];
	if (!font)
		return "M";

	const int rows = CKeyboardLayoutData::getRowCount();
	const int columns = CKeyboardLayoutData::getColumnCount();
	/* The space key's fallback caption counts too when it is in use: it
	 * is several characters wide, so fitting only to the single glyphs
	 * would leave it the one caption that does not fit - and it would
	 * then take the per-key shrink path this fit exists to avoid. */
	std::string widest = space_caption;
	int widest_w = space_caption.empty() ? 0 : font->getRenderWidth(space_caption);

	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < columns; c++)
		{
			const std::string glyph = ck_layout.glyphAt(r, c);
			if (glyph == " ")
				continue;

			const int w = font->getRenderWidth(glyph);
			if (w > widest_w)
			{
				widest_w = w;
				widest = glyph;
			}
		}
	}

	ck_widest_caption = widest.empty() ? "M" : widest;
	ck_widest_for_space = space_caption;

	return ck_widest_caption;
}

Font *CComponentsKeyboard::getFittedKeyFont(const int &key_w,
	const int &key_h,
	const int &gap,
	const std::string &space_caption) const
{
	Font *fallback = g_Font[SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT];
	const int max_w = key_w - 2 * OFFSET_INNER_MIN;
	const int max_h = key_h - 2 * gap;
	const std::string reference = getWidestCaption(space_caption);

	if (max_w <= 0 || max_h <= 0 || reference.empty())
		return fallback;

	int dyn_w = max_w;
	int dyn_h = max_h;
	Font **dyn_font = CNeutrinoFonts::getInstance()->getDynFont(dyn_w, dyn_h, reference);
	Font *fitted = (dyn_font && *dyn_font) ? *dyn_font : fallback;

	/* Never below the configured button text size. That font is what the
	 * keys used before and it is user settable (OSD font sizes), so
	 * someone who raised it did so to be able to read them - answering
	 * that with a smaller fitted font would take away the very setting
	 * they reached for. The fit is here to enlarge, not to cap.
	 *
	 * Compared by height, not by getSize(): the two fonts come from
	 * different render classes - the dynamic one at 72 dpi, g_Font[] at
	 * 72 * font_scaling - so their nominal sizes mean different pixel
	 * counts as soon as the user scaled fonts at all. */
	if (!ck_compact && fallback && fitted && fitted->getHeight() < fallback->getHeight())
		return fallback;

	return fitted;
}

bool CComponentsKeyboard::applyKeyFace(const int &row,
	const int &column,
	const std::string &space_caption,
	const bool &space_icon_ok)
{
	CComponentsButton *key = ck_keys[row][column];
	const std::string glyph = ck_layout.glyphAt(row, column);
	std::string icon;
	std::string caption;

	if (glyph == " ")
	{
		if (space_icon_ok)
			/* Caption stays empty: the button then centres the icon alone. */
			icon = NEUTRINO_ICON_KEY_SPACE;
		else
			caption = space_caption;
	}
	else
		caption = glyph;

	/* Touch only what actually changed. Both setters re-initialize the
	 * whole button, and refreshLayout() runs on every inline error toggle
	 * while typing - re-facing 56 keys that did not change would measure
	 * and re-render every caption for nothing. */
	bool reinited = false;
	if (key->getButtonIcon() != icon)
	{
		key->setButtonIcon(icon);
		reinited = true;
	}
	if (key->getCaptionString() != caption)
	{
		key->setCaption(caption);
		reinited = true;
	}

	return reinited;
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
		CComponentsFrmChain *row = new CComponentsFrmChain(0, 0, width, getKeyHeight(ck_compact),
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
			 * fit, and that font survives a later resize. The caption
			 * itself stays empty here - refreshLayout() below faces
			 * every key anyway, and a caption set now would only feed
			 * that shrink path once for nothing. */
			CComponentsButton *key = new CComponentsButton(0, 0,
				getKeyWidth(width), getKeyHeight(ck_compact),
				"", "", row);
			/* Keys stay opaque as well: the filled selection pair
			 * needs a painted body, and repaintKey()'s kill() then
			 * erases the key to the row colour before repainting. */
			key->doPaintBg(true);
			key->setColorBody(ck_col_key_body);
			key->setButtonTextColor(ck_col_key_text,
				COL_MENUCONTENTSELECTED_TEXT);
			key->setCorner(RADIUS_SMALL, CORNER_ALL);
			/* The locale caption of the space key never fits a 14
			 * column grid cell; without a fixed width every paint
			 * would shrink the key and shift the row. */
			key->enableFixedWidth();
			keys.push_back(key);
		}
		ck_keys.push_back(keys);
	}

	refreshLayout();
}

void CComponentsKeyboard::refreshLayout()
{
	if (ck_rows.empty() || ck_keys.empty())
		return;

	const int rows = CKeyboardLayoutData::getRowCount();
	const int columns = CKeyboardLayoutData::getColumnCount();
	const int gap = getKeyGap(ck_compact);
	const int key_w = getKeyWidth(width);
	const int key_h = getKeyHeight(ck_compact);
	const int row_w = columns * key_w + (columns - 1) * gap;
	const int row_x = std::max(fr_thickness, (width - row_w) / 2);
	int row_y = fr_thickness + gap;

	/* The space icon is probed once per pass, not per key: getIconSize()
	 * actually decodes (and caches) the image, so a present-but-broken
	 * file takes the caption fallback too. Probed before the font is
	 * sized, because the fallback caption is one of the captions that
	 * has to fit. */
	int icon_w = 0, icon_h = 0;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_KEY_SPACE, &icon_w, &icon_h);
	const bool space_icon_ok = icon_w > 0 && icon_h > 0;
	/* Probed with the font that will render it: an externally set key
	 * font may carry a different glyph set than the button text font,
	 * and picking U+2423 for a font that lacks it draws nothing. */
	const std::string space_caption = space_icon_ok ? std::string() :
		getSpaceCaption(ck_font ? ck_font : g_Font[SNeutrinoSettings::FONT_TYPE_BUTTON_TEXT]);

	/* One shared font fitted into the key cell, sized against the widest
	 * caption of the active page: the FONT_TYPE_BUTTON_TEXT default is a
	 * footer font that ignores the cell and left the labels needlessly
	 * small. An externally set font keeps its old meaning and wins; it
	 * also feeds getKeyHeight(), so the fitted font must never land in
	 * ck_font or the sizing would become circular. */
	Font *key_font = ck_font ?
		ck_font : getFittedKeyFont(key_w, key_h, gap, space_caption);

	for (int r = 0; r < rows; r++)
	{
		ck_rows[r]->setDimensionsAll(row_x, row_y, row_w, key_h);
		ck_rows[r]->setColorBody(col_body_std);

		int key_x = 0;
		for (int c = 0; c < columns; c++)
		{
			/* setDimensionsAll() moves the button but not the label
			 * and icon inside it, and the two calls below re-init
			 * only when the font or the face changed. A cell that
			 * changes size while both stay the same - the compact
			 * switch does exactly that - would keep its children at
			 * the old offsets, so that case re-inits explicitly. */
			const bool resized = ck_keys[r][c]->getWidth() != key_w ||
				ck_keys[r][c]->getHeight() != key_h;
			ck_keys[r][c]->setDimensionsAll(key_x, 0, key_w, key_h);
			/* setButtonFont() re-initializes the whole button, so it
			 * is called only when the font really changed - otherwise
			 * every pass would re-measure all 56 captions and undo
			 * what applyKeyFace() saves below. */
			bool reinited = false;
			if (!key_font)
				ck_keys[r][c]->resetButtonFont();
			else if (ck_keys[r][c]->getButtonFont() != key_font)
			{
				ck_keys[r][c]->setButtonFont(key_font);
				reinited = true;
			}
			reinited |= applyKeyFace(r, c, space_caption, space_icon_ok);
			if (resized && !reinited)
				ck_keys[r][c]->Refresh();
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
			/* Focus shows as the theme's selected pair - a filled
			 * highlight like everywhere else in the menus. A frame
			 * alone has no guaranteed contrast: the key body is the
			 * field body colour, and on dark themes the selection
			 * frame colour sits right next to it, which left the
			 * focus invisible. Every colour is passed explicitly:
			 * the defaults of setSelected() would put the global
			 * menu colours and a 3px frame on every key and so undo
			 * setKeyColors() on each focus move. */
			const fb_pixel_t body = selected ?
				COL_MENUCONTENTSELECTED_PLUS_0 : ck_col_key_body;
			ck_keys[r][c]->setSelected(selected,
				COL_MENUCONTENTSELECTED_PLUS_0,
				ck_col_key_body,
				body,
				ck_col_key_body,
				0,
				0);
			/* setSelected() hard-wires the SECONDARY body color, and
			 * paintInit() picks exactly that one for a selected child
			 * whose parent row has no focus - which after buildKeys()
			 * is every row but the last, since setFocus(true) clears
			 * the flag of all siblings. Pin all three body states. */
			ck_keys[r][c]->setColorBody(body,
				body,
				body);
			ck_keys[r][c]->setButtonTextColor(ck_col_key_text,
				COL_MENUCONTENTSELECTED_TEXT);
		}
	}
}

void CComponentsKeyboard::repaintKey(const int &row, const int &column)
{
	if (row < 0 || row >= (int) ck_keys.size())
		return;
	if (column < 0 || column >= (int) ck_keys[row].size())
		return;
	/* Own bookkeeping instead of isPainted(): after every parent
	 * paint, paintCCItems() restores each child's visibility via
	 * allowPaint(true), and CCDraw::allowPaint() clears is_painted as
	 * a side effect - so the whole child tree of a painted dialog
	 * always reads as unpainted. The flag keeps focus changes from
	 * drawing at unset positions: paint() sets it, hide() clears
	 * it - the window's hide() cascades down to here. */
	if (!ck_on_screen)
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
			ck_keys[r][c]->setColorBody(ck_col_key_body,
				ck_col_key_body,
				ck_col_key_body);
			ck_keys[r][c]->setButtonTextColor(ck_col_key_text,
				COL_MENUCONTENTSELECTED_TEXT);
		}
	}

	/* Re-pin the per-state colors: without this the single-argument
	 * defaults above would survive only because the current caller
	 * happens to run a layout pass right after - an ordering nobody
	 * guarantees. */
	applyKeyStates();
}

void CComponentsKeyboard::paint(const bool &do_save_bg)
{
	CComponentsForm::paint(do_save_bg);
	ck_on_screen = true;
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
	/* initByLocale() refuses to run twice so that it cannot undo a
	 * manual layout switch - but this here IS an explicit switch. A
	 * fresh state object gets past that guard without a second API;
	 * caps starts over, which is what a layout change means. */
	ck_layout = CKeyboardLayoutData();
	ck_layout.initByLocale(locale);
	ck_widest_caption.clear();
	refreshLayout();

	/* Same structural-change rule as toggleLayout(): every caption
	 * changed, so a painted keyboard must redraw or the old glyphs
	 * stay visible while OK already inserts from the new table. */
	if (ck_on_screen)
	{
		kill();
		paint(CC_SAVE_SCREEN_NO);
	}
}

void CComponentsKeyboard::toggleCaps()
{
	ck_layout.toggleCaps();
	ck_widest_caption.clear();
	refreshLayout();

	/* Every caption changed, so this is a structural change and the whole
	 * container is repainted - unlike a focus move, which touches two keys. */
	if (ck_on_screen)
	{
		kill();
		paint(CC_SAVE_SCREEN_NO);
	}
}

void CComponentsKeyboard::toggleLayout()
{
	ck_layout.nextLayout();
	ck_widest_caption.clear();
	refreshLayout();

	if (ck_on_screen)
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

	/* Every color and action key falls through to the dialog footer -
	 * including blue and setup: the footer owns caps and layout switch
	 * now, so its caption stays true no matter which side has focus. */
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
		default:
			break;
	}

	return false;
}
