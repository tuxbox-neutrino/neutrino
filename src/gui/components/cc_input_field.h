/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern input field
	Copyright (C) 2026

	License: GPL
*/

#ifndef __CC_INPUT_FIELD_H__
#define __CC_INPUT_FIELD_H__

#include "cc_item.h"

#include <string>

class CCInputBuffer;
class CComponentsShapeSquare;
class Font;

class CCInputField : public CComponentsItem
{
	private:
		CCInputBuffer *if_buffer;
		CComponentsShapeSquare *if_caret;
		Font *if_font;
		std::string if_placeholder;
		int if_viewport_start;
		int if_padding_x;
		int if_padding_y;
		int if_caret_width;
		bool if_password_mode;
		bool if_focused;
		bool if_error_state;
		fb_pixel_t if_col_text;
		fb_pixel_t if_col_placeholder;
		fb_pixel_t if_col_error;
		fb_pixel_t if_col_frame;
		fb_pixel_t if_col_body;
		fb_pixel_t if_col_body_focus;

		void initCaret(const int &x_pos, const int &caret_h);
		void syncFieldColors();
		int getContentWidth() const;
		int getContentX() const;
		int getTextY() const;
		int getCaretY(const int &caret_h) const;
		std::string getDisplayGlyph(size_t index) const;
		std::string getDisplayRange(size_t first, size_t last) const;

	public:
		CCInputField(const int &x_pos = 0,
			const int &y_pos = 0,
			const int &w = 0,
			const int &h = 0,
			Font *font_text = NULL,
			CComponentsForm *parent = NULL,
			const int &shadow_mode = CC_SHADOW_OFF,
			const fb_pixel_t &color_text = COL_MENUCONTENT_TEXT,
			const fb_pixel_t &color_frame = COL_FRAME_PLUS_0,
			const fb_pixel_t &color_body = COL_MENUCONTENT_PLUS_2,
			const fb_pixel_t &color_body_focus = COL_MENUCONTENTSELECTED_PLUS_0,
			const fb_pixel_t &color_shadow = COL_SHADOW_PLUS_0);
		virtual ~CCInputField();

		void setBuffer(CCInputBuffer *buffer);
		void setPlaceholder(const std::string &text);
		void setPasswordMode(bool enabled = true);
		void setFieldFocus(bool focused = true);
		void setErrorState(bool enabled = true);
		///true while the error frame is set - paint() reads this
		bool getErrorState() const {return if_error_state;}
		void setFont(Font *font);
		void setColors(const fb_pixel_t &color_text,
			const fb_pixel_t &color_frame,
			const fb_pixel_t &color_body,
			const fb_pixel_t &color_body_focus);
		void setPlaceholderColor(const fb_pixel_t &color);
		void setCaretWidth(const int &caret_width);

		/**
		 * Stops the caret's blink timer. The owner calls this before a
		 * modal prompt paints over the field: the timer thread would
		 * keep flipping the caret and restoring its stale snapshot
		 * right through the box. The next paint() arms it again.
		 */
		void cancelCaretBlink(bool keep_on_screen = false);
		void setPadding(const int &x_pad, const int &y_pad);
		void ensureCursorVisible();

		virtual void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
		virtual void hide();
};

#endif
