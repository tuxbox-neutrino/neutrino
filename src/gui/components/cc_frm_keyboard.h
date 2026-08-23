/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	On-screen keyboard component
	Copyright (C) 2026

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.
*/

#ifndef __CC_FRM_KEYBOARD_H__
#define __CC_FRM_KEYBOARD_H__

#include "cc_frm.h"
#include "cc_frm_chain.h"

#include <gui/widget/keyboard_layout.h>

#include <sigc++/signal.h>

#include <string>
#include <vector>

class CCInputBuffer;
class CComponentsButton;
class Font;

/**
 * An on-screen keyboard as a passive cc form.
 *
 * It writes glyphs into a foreign CCInputBuffer and never touches the input
 * field: buffer, field and keyboard are one truth and two views on it. The
 * hosting dialog owns the message loop, the buffer and the repaint of the
 * field; this component only knows which key is focused and how to draw
 * itself.
 *
 * Deliberately no exec(), no timeout and no CMenuTarget - the mirror image of
 * CComponentsInputRow.
 */
class CComponentsKeyboard : public CComponentsForm
{
	private:
		CCInputBuffer *ck_buffer;
		CKeyboardLayoutData ck_layout;
		std::vector<CComponentsFrmChain *> ck_rows;
		std::vector<std::vector<CComponentsButton *> > ck_keys;
		Font *ck_font;
		int ck_row;
		int ck_column;
		bool ck_has_focus;
		bool ck_compact;
		fb_pixel_t ck_col_key_body;
		///true while the keyboard sits on screen: paint() sets it,
		///the dialog clears it via markOffScreen()
		bool ck_on_screen;
		fb_pixel_t ck_col_key_text;

		int getKeyGap() const;
		int getKeyHeight() const;
		int getKeyWidth(const int &w) const;
		std::string getKeyCaption(const int &row, const int &column) const;
		void buildKeys();
		void applyKeyStates();
		void repaintKey(const int &row, const int &column);
		void moveFocus(const int &d_row, const int &d_column);
		void insertFocusedGlyph();

	public:
		CComponentsKeyboard(const int &x_pos = 0,
			const int &y_pos = 0,
			const int &w = 0,
			const int &h = 0,
			CComponentsForm *parent = NULL,
			const int &shadow_mode = CC_SHADOW_OFF,
			const fb_pixel_t &color_frame = COL_FRAME_PLUS_0,
			const fb_pixel_t &color_body = COL_MENUCONTENT_PLUS_0,
			const fb_pixel_t &color_shadow = COL_SHADOW_PLUS_0);
		virtual ~CComponentsKeyboard() {};

		/**
		 * The buffer the keys write into. NOT owned: the dialog keeps
		 * it alive, and the input field reads the same one.
		 */
		void setBuffer(CCInputBuffer *buffer);

		/**
		 * Consumes a key when the keyboard has focus, returns false
		 * otherwise so the caller can go on dispatching.
		 *
		 * Only navigation and OK are consumed. Every color and action
		 * key - blue and setup included - falls through to the dialog
		 * footer, which owns caps and layout switch and keeps its
		 * caption in step.
		 */
		/**Paint and remember that the keyboard reached the screen:
		* the framework's is_painted stays false across this child
		* tree, so repaint guards use this flag instead.*/
		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);

		/**The window's hide() cascades to its children but resets no
		* state - this override remembers that the keyboard left the
		* screen, so the next exec()'s focus sync cannot repaint a key
		* onto a hidden dialog (dialog objects are reused, see
		* proxyserver_setup).*/
		void hide(){CComponentsForm::hide(); ck_on_screen = false;};
		///true while the keyboard is part of the painted dialog
		bool isOnScreen() const {return ck_on_screen;};

		bool handleMsg(const neutrino_msg_t &msg);

		void setKeyFocus(const bool &focused = true);
		bool hasKeyFocus() const {return ck_has_focus;}

		void setLayoutByLocale(const std::string &locale);
		void toggleCaps();
		void toggleLayout();
		std::string getLayoutName() const {return ck_layout.getLayoutName();}

		void setFont(Font *font);
		void setKeyColors(const fb_pixel_t &color_body, const fb_pixel_t &color_text);

		/**
		 * Height this keyboard wants, frame included. Independent of the
		 * width: the row and column counts are fixed by the layout tables,
		 * so only the font decides how tall a key has to be.
		 */
		int getPreferredHeight() const;

		///true when getPreferredHeight() does not fit into max_h
		bool needsCompactMode(const int &max_h) const;

		///smaller keys and gaps; call before the first paint
		void setCompactMode(const bool &compact = true);

		///lays the rows and keys out for the current width
		void refreshLayout();

		///a glyph reached the buffer; the dialog refreshes its field
		sigc::signal<void> OnAfterKey;

		///the buffer refused the glyph (max chars, filter)
		sigc::signal<void> OnKeyRejected;

		///UP was pressed in the top row; the dialog moves focus back to the field
		sigc::signal<void> OnLeaveTop;
};

#endif
