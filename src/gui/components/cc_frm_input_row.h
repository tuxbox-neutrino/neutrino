/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern input row form
	Copyright (C) 2026

	License: GPL
*/

#ifndef __CC_FRM_INPUT_ROW_H__
#define __CC_FRM_INPUT_ROW_H__

#include "cc_frm.h"
#include "cc_input_buffer.h"
#include "cc_input_field.h"
#include "cc_item_text.h"

#include <string>

class CComponentsInputRow : public CComponentsForm
{
	private:
		std::string cir_label_text;
		std::string cir_placeholder;
		std::string cir_field_font_reference;
		CComponentsLabel *cir_label;
		CCInputField *cir_field;
		CCInputBuffer cir_buffer;
		bool cir_password_mode;
		fb_pixel_t cir_label_col;
		fb_pixel_t cir_field_col_text;
		fb_pixel_t cir_field_col_frame;
		fb_pixel_t cir_field_col_body;
		fb_pixel_t cir_field_col_body_focus;
		fb_pixel_t cir_field_col_placeholder;

		void initRow();
		void syncFieldStyle();
		std::string getFieldFontReference() const;

	public:
		CComponentsInputRow(const int &x_pos = 0,
			const int &y_pos = 0,
			const int &w = 0,
			const int &h = 0,
			const std::string &label_text = "",
			const std::string &placeholder = "",
			CComponentsForm *parent = NULL,
			int shadow_mode = CC_SHADOW_OFF,
			const fb_pixel_t &color_frame = COL_FRAME_PLUS_0,
			const fb_pixel_t &color_body = COL_MENUCONTENT_PLUS_0,
			const fb_pixel_t &color_shadow = COL_SHADOW_PLUS_0);
		virtual ~CComponentsInputRow() {};

		void setLabelText(const std::string &text);
		void setPlaceholder(const std::string &text);
		void setFieldFontReference(const std::string &text);
		void setText(const std::string &text);
		const std::string &getText() const;
		void setPasswordMode(bool enabled = true);
		bool isPasswordMode() const {return cir_password_mode;}
		void setAllowEmpty(bool allow_empty = true);
		void setMaxChars(size_t max_chars);
		void setFilterMode(CCInputBuffer::input_filter_mode_t mode);
		void setFieldFocus(bool focused = true);
		void setErrorState(bool enabled = true);
		///true while the row's field has its error frame set
		bool getErrorState() const;
		void setLabelColor(const fb_pixel_t &color);
		void setFieldColors(const fb_pixel_t &color_text,
			const fb_pixel_t &color_frame,
			const fb_pixel_t &color_body,
			const fb_pixel_t &color_body_focus,
			const fb_pixel_t &color_placeholder);
		void ensureCursorVisible();
		void refreshLayout();
		int getPreferredHeight() const;

		CCInputBuffer *getBufferObject() {return &cir_buffer;}
		const CCInputBuffer *getBufferObject() const {return &cir_buffer;}
		CComponentsLabel *getLabelObject() {return cir_label;}
		CCInputField *getFieldObject() {return cir_field;}

		virtual void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
		virtual void hide();
};

#endif
