/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern input field
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_input_field.h"
#include "cc_input_buffer.h"
#include "cc_item_shapes.h"

#include <algorithm>
#include <driver/fontrenderer.h>

namespace
{
int getFontGlyphHeight(Font *font)
{
	if (!font)
		return 0;

	return std::max(0, font->getAscender() + font->getDescender());
}

/* The block of a cell caret: the cell in the text colour, its glyph in
 * the body colour on top. The blink timer calls paint() and hide() in
 * turn, and hide() puts back what paint() saved first - the field's own
 * rendering of the same glyph - so the cell flips between plain and
 * inverted. */
class CCInputCellCaret : public CComponentsShapeSquare
{
	private:
		Font *icc_font;
		std::string icc_glyph;
		int icc_text_y;
		fb_pixel_t icc_col_glyph;

	public:
		CCInputCellCaret(const int &x_pos, const int &y_pos, const int &w, const int &h,
			Font *font, const std::string &glyph, const int &text_y,
			const fb_pixel_t &color_block, const fb_pixel_t &color_glyph)
			: CComponentsShapeSquare(x_pos, y_pos, w, h, NULL, CC_SHADOW_OFF,
				color_block, color_block)
		{
			icc_font = font;
			icc_glyph = glyph;
			icc_text_y = text_y;
			icc_col_glyph = color_glyph;
		}

		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES)
		{
			CComponentsShapeSquare::paint(do_save_bg);
			if (icc_font)
				icc_font->RenderString(x, icc_text_y, width, icc_glyph,
					icc_col_glyph);
		}
};
}

CCInputField::CCInputField(const int &x_pos,
	const int &y_pos,
	const int &w,
	const int &h,
	Font *font_text,
	CComponentsForm *parent,
	const int &shadow_mode,
	const fb_pixel_t &color_text,
	const fb_pixel_t &color_frame,
	const fb_pixel_t &color_body,
	const fb_pixel_t &color_body_focus,
	const fb_pixel_t &color_shadow)
	: CComponentsItem(parent)
{
	cc_item_type.id = CC_ITEMTYPE_INPUT_FIELD;
	cc_item_type.name = "cc_input_field";

	x = x_old = x_pos;
	y = y_old = y_pos;
	width = width_old = w;
	height = height_old = h;
	shadow = shadow_mode;
	col_shadow = color_shadow;
	if_buffer = NULL;
	if_caret = NULL;
	if_font = font_text;
	if_viewport_start = 0;
	if_padding_x = OFFSET_INNER_SMALL;
	if_padding_y = OFFSET_INNER_SMALL;
	if_caret_width = std::max(2, FRAME_WIDTH_MIN * 2);
	if_password_mode = false;
	if_focused = false;
	if_caret_unfocused = false;
	if_cell_caret = false;
	if_cap_centering = false;
	if_error_state = false;
	if_col_text = color_text;
	if_col_placeholder = COL_MENUCONTENTINACTIVE_TEXT;
	if_col_error = COL_RED;
	if_col_frame = color_frame;
	if_col_body = color_body;
	if_col_body_focus = color_body_focus;

	corner_rad = RADIUS_SMALL;
	corner_type = CORNER_ALL;
	enableFrame(true, FRAME_WIDTH_MIN);
	doPaintBg(true);
	setColorBody(if_col_body, if_col_body_focus, if_col_body);
}

CCInputField::~CCInputField()
{
	cancelCaretBlink(false);
}

void CCInputField::cancelCaretBlink(bool keep_on_screen)
{
	if (!if_caret)
		return;

	if_caret->cancelBlink(keep_on_screen);
	delete if_caret;
	if_caret = NULL;
}

void CCInputField::enableUnfocusedCaret(bool enable)
{
	if_caret_unfocused = enable;
}

void CCInputField::enableCellCaret(bool enable)
{
	if_cell_caret = enable;
}

void CCInputField::enableCapCentering(bool enable)
{
	if_cap_centering = enable;
}

void CCInputField::initCaret(const int &x_pos, const int &caret_h,
	const std::string &cell_glyph, const int &cell_w, const int &text_y)
{
	cancelCaretBlink(false);

	if ((!if_focused && !if_caret_unfocused) || caret_h <= 0)
		return;

	if (if_cell_caret && !cell_glyph.empty() && cell_w > 0)
	{
		if_caret = new CCInputCellCaret(x_pos,
			getCaretY(caret_h),
			cell_w,
			caret_h,
			if_font,
			cell_glyph,
			text_y,
			if_col_text,
			if_focused ? if_col_body_focus : if_col_body);
		return;
	}

	if_caret = new CComponentsShapeSquare(x_pos,
		getCaretY(caret_h),
		if_caret_width,
		caret_h,
		NULL,
		CC_SHADOW_OFF,
		if_col_text,
		if_col_text);
}

void CCInputField::syncFieldColors()
{
	cc_item_selected = if_focused;
	setFrameThickness((if_focused || if_error_state) ? 2 * FRAME_WIDTH_MIN :
		FRAME_WIDTH_MIN);
	setColorFrame(if_error_state ? if_col_error : if_col_frame);
	setColorBody(if_col_body, if_col_body_focus, if_col_body);
}

int CCInputField::getContentWidth() const
{
	return std::max(0, width - 2 * (getFrameThickness() + if_padding_x));
}

int CCInputField::getContentX() const
{
	return getRealXPos() + getFrameThickness() + if_padding_x;
}

int CCInputField::getTextY(bool cap_band) const
{
	const int inner_y = getRealYPos() + getFrameThickness();
	const int inner_h = std::max(0, height - 2 * getFrameThickness());

	if (!if_font)
		return inner_y + inner_h;

	/* The baseline is placed here, not by RenderString(): it puts the
	 * baseline above the y it gets by the lower part of its line box,
	 * which reserves half a line gap below the descender and so lifts
	 * the glyphs off the middle - by half a descender more for digits,
	 * which have none. Centred is the band the glyphs fill: down to the
	 * descender for text, the cap height alone for digits and hex. */
	const int ascender = if_font->getAscender();
	const int band = cap_band ? ascender : ascender + if_font->getDescender();
	const int baseline = inner_y + (inner_h - band) / 2 + ascender;

	/* RenderString() without a boxheight draws on y minus this. */
	return baseline + if_font->getLower();
}

int CCInputField::getCaretY(const int &caret_h) const
{
	const int inner_h = std::max(0, height - 2 * getFrameThickness());
	return getRealYPos() + getFrameThickness() +
		std::max(0, (inner_h - caret_h) / 2);
}

std::string CCInputField::getDisplayGlyph(size_t index) const
{
	if (!if_buffer)
		return std::string();

	if (if_password_mode && index < if_buffer->size())
		return "*";

	return if_buffer->glyphAt(index);
}

std::string CCInputField::getDisplayRange(size_t first, size_t last) const
{
	if (!if_buffer || first >= last)
		return std::string();

	last = std::min(last, if_buffer->size());

	std::string out;
	for (size_t i = first; i < last; i++)
		out += getDisplayGlyph(i);

	return out;
}

void CCInputField::setBuffer(CCInputBuffer *buffer)
{
	if_buffer = buffer;
	if_viewport_start = 0;
}

void CCInputField::setPlaceholder(const std::string &text)
{
	if_placeholder = text;
}

void CCInputField::setPasswordMode(bool enabled)
{
	if_password_mode = enabled;
}

void CCInputField::setFieldFocus(bool focused)
{
	if_focused = focused;
}

void CCInputField::setErrorState(bool enabled)
{
	if_error_state = enabled;
}

void CCInputField::setFont(Font *font)
{
	if_font = font;
}

void CCInputField::setColors(const fb_pixel_t &color_text,
	const fb_pixel_t &color_frame,
	const fb_pixel_t &color_body,
	const fb_pixel_t &color_body_focus)
{
	if_col_text = color_text;
	if_col_frame = color_frame;
	if_col_body = color_body;
	if_col_body_focus = color_body_focus;
}

void CCInputField::setPlaceholderColor(const fb_pixel_t &color)
{
	if_col_placeholder = color;
}

void CCInputField::setCaretWidth(const int &caret_width)
{
	if_caret_width = std::max(2, caret_width);
}

void CCInputField::setPadding(const int &x_pad, const int &y_pad)
{
	if_padding_x = std::max(0, x_pad);
	if_padding_y = std::max(0, y_pad);
}

void CCInputField::ensureCursorVisible()
{
	if (!if_buffer)
		return;

	if (!if_font)
		if_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];

	const size_t cursor = std::min(if_buffer->getCursor(), if_buffer->size());
	const int content_w = getContentWidth();

	if_viewport_start = std::min(if_viewport_start, (int) if_buffer->size());
	if ((size_t) if_viewport_start > cursor)
		if_viewport_start = cursor;

	if (content_w <= if_caret_width)
	{
		if_viewport_start = cursor;
		return;
	}

	while ((size_t) if_viewport_start < cursor)
	{
		const std::string visible = getDisplayRange(if_viewport_start, cursor);
		const int visible_w = if_font->getRenderWidth(visible);
		if (visible_w + if_caret_width <= content_w)
			break;
		if_viewport_start++;
	}

	/* And back to the left: deleting walks the cursor down onto the
	 * viewport edge, and a start that only ever grows would leave the
	 * field empty while the buffer still holds text. Pull the window
	 * left again as long as one more glyph fits. */
	while (if_viewport_start > 0)
	{
		const std::string wider = getDisplayRange(if_viewport_start - 1, cursor);
		if (if_font->getRenderWidth(wider) + if_caret_width > content_w)
			break;
		if_viewport_start--;
	}
}

void CCInputField::paint(const bool &do_save_bg)
{
	if (!if_font)
		if_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];

	cancelCaretBlink(false);
	syncFieldColors();
	ensureCursorVisible();
	paintInit(do_save_bg);

	const int content_x = getContentX();
	const int content_w = getContentWidth();
	const int text_y = getTextY(if_cap_centering);
	const int inner_h = std::max(0, height - 2 * getFrameThickness());
	const int glyph_h = getFontGlyphHeight(if_font);
	const int caret_target_h = glyph_h > 0 ?
		glyph_h + CFrameBuffer::getInstance()->scale2Res(1) : inner_h;
	const int caret_h = std::min(inner_h, std::max(0, caret_target_h));
	int draw_x = content_x;
	int caret_x = content_x;
	bool caret_set = false;
	std::string cell_glyph;
	int cell_w = 0;

	if (!if_buffer || if_buffer->empty())
	{
		if (!if_placeholder.empty())
		{
			/* The placeholder is a name, text of its own. */
			if_font->RenderString(content_x,
				getTextY(false),
				content_w,
				if_placeholder,
				if_col_placeholder);
		}
	}
	else
	{
		const size_t cursor = std::min(if_buffer->getCursor(), if_buffer->size());
		const int content_end = content_x + content_w;

		for (size_t i = if_viewport_start; i < if_buffer->size(); i++)
		{
			if (!caret_set && cursor == i)
			{
				caret_x = draw_x;
				caret_set = true;
			}

			const std::string glyph = getDisplayGlyph(i);
			const int glyph_w = if_font->getRenderWidth(glyph);

			if (draw_x + glyph_w > content_end)
				break;

			if (cursor == i)
			{
				cell_glyph = glyph;
				cell_w = glyph_w;
			}

			if_font->RenderString(draw_x,
				text_y,
				content_end - draw_x,
				glyph,
				if_col_text);
			draw_x += glyph_w;

			if (!caret_set && cursor == (i + 1))
			{
				caret_x = draw_x;
				caret_set = true;
			}
		}

		if (!caret_set)
			caret_x = std::min(draw_x,
					std::max(content_x, content_end - if_caret_width));
	}

	if (if_focused || if_caret_unfocused)
	{
		initCaret(caret_x, caret_h, cell_glyph, cell_w, text_y);
		if (if_caret)
		{
			if (if_focused)
				if_caret->paintBlink(500);
			else
				/* One static frame over the saved backdrop - the
				 * same call cancelBlink(keep_on_screen) uses, so
				 * the next cancelCaretBlink() restores the pixels
				 * underneath whether a timer ran or not. */
				if_caret->paint1();
		}
	}

	if (if_error_state)
	{
		CFrameBuffer::getInstance()->paintBoxFrame(getRealXPos(),
			getRealYPos(),
			width,
			height,
			std::max(2, FRAME_WIDTH_MIN * 2),
			if_col_error,
			corner_rad,
			corner_type);
	}
}

void CCInputField::hide()
{
	cancelCaretBlink(false);
	CCDraw::hide();
}
