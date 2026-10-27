/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Structured input dialog
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_structured_input_dialog.h"

#include <driver/display.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <gui/widget/stringinput.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace
{
const char *const DIGITS = "0123456789";

std::vector<std::string> splitOn(const std::string &text, char separator)
{
	std::vector<std::string> parts;
	std::string part;
	std::istringstream in(text);
	while (std::getline(in, part, separator))
		parts.push_back(part);
	return parts;
}

bool allDigits(const std::string &text)
{
	return !text.empty() &&
		text.find_first_not_of(DIGITS) == std::string::npos;
}

///text right-aligned in width cells, filled with zeros; longer text keeps its last cells
std::string zeroPad(const std::string &text, size_t width)
{
	if (text.size() >= width)
		return text.substr(text.size() - width);
	return std::string(width - text.size(), '0') + text;
}

int widestGlyph(Font *font, const std::string &charset)
{
	int widest = 0;
	for (size_t i = 0; font && i < charset.size(); i++)
		widest = std::max(widest, font->getRenderWidth(std::string(1, charset[i])));
	return widest;
}
}

CCStructuredInputDialog::CCStructuredInputDialog(const neutrino_locale_t title,
	format_t format,
	std::string *value,
	CChangeObserver *observ)
	: CCInputDialogBase(getInputDialogWidth(),
		  getInputDialogMinHeight(),
		  title == NONEXISTANT_LOCALE ? "" : g_Locale->getText(title),
		  NEUTRINO_ICON_EDIT)
{
	csd_value = value;
	csd_time = NULL;
	csd_observ = observ;
	idb_title_locale = title;
	init(format);
}

CCStructuredInputDialog::CCStructuredInputDialog(const neutrino_locale_t title,
	time_t *value,
	CChangeObserver *observ)
	: CCInputDialogBase(getInputDialogWidth(),
		  getInputDialogMinHeight(),
		  title == NONEXISTANT_LOCALE ? "" : g_Locale->getText(title),
		  NEUTRINO_ICON_EDIT)
{
	csd_value = NULL;
	csd_time = value;
	csd_observ = observ;
	idb_title_locale = title;
	init(FORMAT_DATETIME);
	if (csd_time)
		csd_time_text = formatTime(*csd_time);
}

CCStructuredInputDialog::~CCStructuredInputDialog()
{
	/* The items belong to the window body and go with it; only the
	 * segment records are this dialog's. */
	for (size_t i = 0; i < csd_segments.size(); i++)
		delete csd_segments[i];
}

void CCStructuredInputDialog::init(format_t format)
{
	cc_item_type.name = "cc_structured_input_dialog";

	csd_format = format;
	csd_cancel = NULL;
	csd_hint = NULL;
	csd_active = 0;
	csd_saved = false;
	csd_built = false;
	csd_decimal_int = 3;
	csd_decimal_frac = 6;
	csd_decimal_max = 999;
	csd_font = NULL;
	valueString = getTarget() ? getTarget() : &valueStringTmp;

	applyDialogStyle();
	initFooterButtons();
}

std::string *CCStructuredInputDialog::getTarget()
{
	return csd_time ? &csd_time_text : csd_value;
}

std::string &CCStructuredInputDialog::getValue(void)
{
	std::string *target = getTarget();
	if (target)
		return *target;

	valueStringTmp.clear();
	return valueStringTmp;
}

void CCStructuredInputDialog::setHintText(const std::string &hint)
{
	csd_hint_text = hint;
	/* The weekday captions come from the same text the old window showed
	 * as its first hint line: "Mo Di Mi Do Fr Sa So". */
	csd_captions = hint.substr(0, hint.find('\n'));
}

void CCStructuredInputDialog::setDecimal(int int_digits, int frac_digits, int max_int)
{
	csd_decimal_int = std::max(1, int_digits);
	csd_decimal_frac = std::max(1, frac_digits);
	csd_decimal_max = std::max(0, max_int);
}

std::string CCStructuredInputDialog::formatTime(time_t value)
{
	struct tm parts;
	char text[40];

	localtime_r(&value, &parts);
	snprintf(text, sizeof(text), "%02d.%02d.%04d %02d:%02d",
		parts.tm_mday, parts.tm_mon + 1, parts.tm_year + 1900,
		parts.tm_hour, parts.tm_min);
	return text;
}

void CCStructuredInputDialog::addSegment(size_t len,
	const std::string &charset,
	bool ranged,
	long long min,
	long long max,
	const std::string &separator,
	const std::string &caption)
{
	segment_t *segment = new segment_t;

	segment->len = len;
	segment->charset = charset;
	segment->ranged = ranged;
	segment->min = min;
	segment->max = max;
	segment->separator = separator;
	segment->caption = caption;
	segment->field = NULL;
	segment->separator_item = NULL;
	segment->caption_item = NULL;
	segment->buffer.setFilterMode(CCInputBuffer::FILTER_CHARSET);
	segment->buffer.setCharset(charset);
	segment->buffer.setMaxChars(len);

	csd_segments.push_back(segment);
}

void CCStructuredInputDialog::clearSegments()
{
	CComponentsForm *body = getBodyObject();

	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		segment_t *segment = csd_segments[i];
		/* removeCCItem() deletes the item it takes out */
		if (segment->field)
			body->removeCCItem(segment->field);
		if (segment->separator_item)
			body->removeCCItem(segment->separator_item);
		if (segment->caption_item)
			body->removeCCItem(segment->caption_item);
		delete segment;
	}
	csd_segments.clear();
	csd_active = 0;
}

void CCStructuredInputDialog::defineSegments(const std::string &value)
{
	clearSegments();

	switch (csd_format)
	{
		case FORMAT_IP:
			for (int i = 0; i < 4; i++)
				addSegment(3, DIGITS, true, 0, 255, i < 3 ? "." : "");
			break;
		case FORMAT_MAC:
			for (int i = 0; i < 6; i++)
				addSegment(2, "0123456789ABCDEF", false, 0, 0, i < 5 ? ":" : "");
			break;
		case FORMAT_DATETIME:
			addSegment(2, DIGITS, true, 1, 31, ".");
			addSegment(2, DIGITS, true, 1, 12, ".");
			addSegment(4, DIGITS, true, 1970, 2099, " ");
			addSegment(2, DIGITS, true, 0, 23, ":");
			addSegment(2, DIGITS, true, 0, 59, "");
			break;
		case FORMAT_TIME:
		{
			/* A film runs longer than 99 hours seldom, but the value
			 * may bring three digits; the field takes what it gets. */
			const std::string hours = value.substr(0, value.find(':'));
			const size_t hours_width = std::max((size_t) 2,
				allDigits(hours) ? hours.size() : (size_t) 2);
			long long highest = 1;
			for (size_t i = 0; i < hours_width; i++)
				highest *= 10;
			addSegment(hours_width, DIGITS, true, 0, highest - 1, ":");
			addSegment(2, DIGITS, true, 0, 59, ":");
			addSegment(2, DIGITS, true, 0, 59, "");
			break;
		}
		case FORMAT_WEEKDAYS:
		{
			std::vector<std::string> names;
			std::istringstream in(csd_captions);
			std::string name;
			while (in >> name)
				names.push_back(name);
			for (size_t i = 0; i < 7; i++)
				addSegment(1, "-X", false, 0, 0, "",
					names.size() == 7 ? names[i] : std::string());
			break;
		}
		case FORMAT_DECIMAL:
			addSegment(csd_decimal_int, DIGITS, true, 0, csd_decimal_max, ".");
			addSegment(csd_decimal_frac, DIGITS, false, 0, 0, "");
			break;
	}
}

void CCStructuredInputDialog::loadValue(const std::string &value)
{
	std::vector<std::string> cells(csd_segments.size());

	switch (csd_format)
	{
		case FORMAT_IP:
		{
			std::vector<std::string> parts = splitOn(value, '.');
			const bool valid = parts.size() == 4 &&
				allDigits(parts[0]) && allDigits(parts[1]) &&
				allDigits(parts[2]) && allDigits(parts[3]);
			/* Anything that is no address - empty, a host name -
			 * opens as zeros; back keeps the value untouched. */
			for (size_t i = 0; i < 4; i++)
				cells[i] = valid ? zeroPad(parts[i], 3) : "000";
			break;
		}
		case FORMAT_MAC:
		{
			std::vector<std::string> parts = splitOn(value, ':');
			bool valid = parts.size() == 6;
			for (size_t i = 0; valid && i < 6; i++)
				valid = parts[i].size() <= 2 && !parts[i].empty() &&
					parts[i].find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
			for (size_t i = 0; i < 6; i++)
			{
				std::string part = valid ? zeroPad(parts[i], 2) : "00";
				std::transform(part.begin(), part.end(), part.begin(), ::toupper);
				cells[i] = part;
			}
			break;
		}
		case FORMAT_DATETIME:
		{
			struct tm parts;
			const time_t when = csd_time ? *csd_time : time(NULL);
			char text[16];
			localtime_r(&when, &parts);
			snprintf(text, sizeof(text), "%02d", parts.tm_mday);
			cells[0] = text;
			snprintf(text, sizeof(text), "%02d", parts.tm_mon + 1);
			cells[1] = text;
			snprintf(text, sizeof(text), "%04d", parts.tm_year + 1900);
			cells[2] = text;
			snprintf(text, sizeof(text), "%02d", parts.tm_hour);
			cells[3] = text;
			snprintf(text, sizeof(text), "%02d", parts.tm_min);
			cells[4] = text;
			break;
		}
		case FORMAT_TIME:
		{
			std::vector<std::string> parts = splitOn(value, ':');
			for (size_t i = 0; i < 3; i++)
				cells[i] = zeroPad(i < parts.size() && allDigits(parts[i]) ?
						parts[i] : "0", csd_segments[i]->len);
			break;
		}
		case FORMAT_WEEKDAYS:
			for (size_t i = 0; i < 7; i++)
				cells[i] = (i < value.size() && (value[i] == 'X' || value[i] == 'x')) ?
					"X" : "-";
			break;
		case FORMAT_DECIMAL:
		{
			const size_t point = value.find('.');
			std::string whole = value.substr(0, point);
			std::string places = point == std::string::npos ? "" : value.substr(point + 1);
			if (!allDigits(whole))
				whole = "0";
			if (!places.empty() && !allDigits(places))
				places.clear();
			cells[0] = zeroPad(whole, csd_segments[0]->len);
			places.resize(csd_segments[1]->len, '0');
			cells[1] = places;
			break;
		}
	}

	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		csd_segments[i]->buffer.setText(cells[i]);
		csd_segments[i]->buffer.setCursor(0);
	}
}

std::string CCStructuredInputDialog::getCells() const
{
	std::string cells;
	for (size_t i = 0; i < csd_segments.size(); i++)
		cells += csd_segments[i]->buffer.getText() + csd_segments[i]->separator;
	return cells;
}

long long CCStructuredInputDialog::segmentNumber(size_t index) const
{
	return strtoll(csd_segments[index]->buffer.getText().c_str(), NULL, 10);
}

bool CCStructuredInputDialog::makeTime(time_t &result) const
{
	struct tm parts;
	const time_t original = csd_time ? *csd_time : time(NULL);

	/* From the original, so its seconds stay: the old window set them
	 * to zero, also on cancel, and a timer then counted as changed. */
	localtime_r(&original, &parts);
	parts.tm_mday = (int) segmentNumber(0);
	parts.tm_mon = (int) segmentNumber(1) - 1;
	parts.tm_year = (int) segmentNumber(2) - 1900;
	parts.tm_hour = (int) segmentNumber(3);
	parts.tm_min = (int) segmentNumber(4);
	parts.tm_isdst = -1;

	struct tm wanted = parts;
	const time_t made = mktime(&parts);
	if (made == (time_t) -1)
		return false;

	/* mktime() turns 31.02. into 03.03. instead of refusing it. Only the
	 * day is compared: an hour that the switch to summer time skips is
	 * moved on by mktime(), and that is the right answer for it. */
	if (parts.tm_mday != wanted.tm_mday || parts.tm_mon != wanted.tm_mon ||
		parts.tm_year != wanted.tm_year)
		return false;

	result = made;
	return true;
}

std::string CCStructuredInputDialog::composeValue() const
{
	char text[64];

	switch (csd_format)
	{
		case FORMAT_IP:
		{
			long long octets[4];
			bool all_zero = true;
			for (size_t i = 0; i < 4; i++)
			{
				octets[i] = segmentNumber(i);
				all_zero = all_zero && octets[i] == 0;
			}
			if (all_zero)
				return std::string();
			snprintf(text, sizeof(text), "%lld.%lld.%lld.%lld",
				octets[0], octets[1], octets[2], octets[3]);
			return text;
		}
		case FORMAT_MAC:
		{
			std::string mac;
			bool all_zero = true;
			for (size_t i = 0; i < 6; i++)
			{
				const std::string part = csd_segments[i]->buffer.getText();
				all_zero = all_zero && part == "00";
				mac += (i ? ":" : "") + part;
			}
			if (all_zero)
				return std::string();
			/* lower case, as the old input and refreshMAC() write it */
			std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);
			return mac;
		}
		case FORMAT_DATETIME:
		{
			time_t made = 0;
			return makeTime(made) ? formatTime(made) : std::string();
		}
		case FORMAT_DECIMAL:
			snprintf(text, sizeof(text), "%lld.", segmentNumber(0));
			return text + csd_segments[1]->buffer.getText();
		case FORMAT_TIME:
		case FORMAT_WEEKDAYS:
		default:
		{
			std::string joined;
			for (size_t i = 0; i < csd_segments.size(); i++)
				joined += csd_segments[i]->buffer.getText() + csd_segments[i]->separator;
			return joined;
		}
	}
}

std::string CCStructuredInputDialog::getRangeText(const segment_t *segment) const
{
	char text[256];
	snprintf(text, sizeof(text), g_Locale->getText(LOCALE_STRINGINPUT_RANGE),
		(int) segment->min, (int) segment->max);
	return text;
}

bool CCStructuredInputDialog::validate(size_t &bad, std::string &reason) const
{
	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		const segment_t *segment = csd_segments[i];
		if (!segment->ranged)
			continue;

		const long long number = segmentNumber(i);
		if (number < segment->min || number > segment->max)
		{
			bad = i;
			reason = getRangeText(segment);
			return false;
		}
	}

	if (csd_format == FORMAT_DATETIME)
	{
		time_t made = 0;
		if (!makeTime(made))
		{
			bad = 0;
			reason = g_Locale->getText(LOCALE_STRINGINPUT_INVALID_DATE);
			return false;
		}
	}

	return true;
}

void CCStructuredInputDialog::buildItems()
{
	const fb_pixel_t text_col = getInputFieldTextColor();
	const fb_pixel_t frame_col = getInputFieldFrameColor();
	const fb_pixel_t body_col = getInputFieldBodyColor();
	const fb_pixel_t focus_col = getInputFieldBodyFocusColor();

	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		segment_t *segment = csd_segments[i];

		segment->field = new CCInputField(0, 0, 0, 0, NULL, NULL, CC_SHADOW_OFF,
			text_col, frame_col, body_col, focus_col, COL_SHADOW_PLUS_0);
		segment->field->setBuffer(&segment->buffer);
		segment->field->setPlaceholderColor(getInputFieldPlaceholderColor());
		segment->field->setPadding(getInputFieldPaddingX(), getInputFieldPaddingY());
		addWindowItem(segment->field);

		if (!segment->separator.empty())
		{
			segment->separator_item = new CComponentsText(0, 0, 0, 0,
				segment->separator, CTextBox::CENTER, NULL,
				CComponentsText::FONT_STYLE_REGULAR, NULL, CC_SHADOW_OFF,
				COL_MENUCONTENT_TEXT);
			segment->separator_item->doPaintBg(false);
			segment->separator_item->doPaintTextBoxBg(false);
			addWindowItem(segment->separator_item);
		}

		if (!segment->caption.empty())
		{
			segment->caption_item = new CComponentsText(0, 0, 0, 0,
				segment->caption, CTextBox::CENTER,
				g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO],
				CComponentsText::FONT_STYLE_REGULAR, NULL, CC_SHADOW_OFF,
				COL_MENUCONTENT_TEXT_PLUS_1);
			segment->caption_item->doPaintBg(false);
			segment->caption_item->doPaintTextBoxBg(false);
			addWindowItem(segment->caption_item);
		}
	}

	if (!csd_hint)
	{
		csd_hint = new CComponentsText(0, 0, 0, 0, "",
			CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO],
			CComponentsText::FONT_STYLE_REGULAR, NULL, CC_SHADOW_OFF,
			COL_MENUCONTENT_TEXT_PLUS_1);
		csd_hint->doPaintBg(false);
		csd_hint->doPaintTextBoxBg(false);
		addWindowItem(csd_hint);
	}
}

void CCStructuredInputDialog::layoutItems()
{
	CFrameBuffer *fb = CFrameBuffer::getInstance();
	const int pad = getInputDialogPad();
	const int gap = std::max(OFFSET_INNER_SMALL, fb->scale2Res(12));
	const int cell_gap = std::max(OFFSET_INNER_MIN, fb->scale2Res(6));
	const int body_w = getBodyObject()->getWidth() - 2 * pad;
	const int field_frame = 2 * (getInputFieldPaddingX() + 2 * FRAME_WIDTH_MIN);
	Font *hint_font = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO];

	/* One font for every segment, the largest that lets the whole mask
	 * fit beside the fields' frames and gaps. */
	std::string mask;
	for (size_t i = 0; i < csd_segments.size(); i++)
		mask += std::string(csd_segments[i]->len, csd_segments[i]->charset == DIGITS ? '0' : 'W') +
			csd_segments[i]->separator;
	const int budget = body_w - (int) csd_segments.size() * (field_frame + 2 * cell_gap + fb->scale2Res(6));
	csd_font = getInputDialogFieldFont(std::max(budget, body_w / 3), mask);
	const int caret_w = getInputDialogCaretWidth(csd_font);
	const int field_h = getInputFieldHeight(csd_font);

	std::vector<int> field_w(csd_segments.size());
	std::vector<int> separator_w(csd_segments.size());
	bool has_captions = false;
	int total_w = 0;
	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		const segment_t *segment = csd_segments[i];
		const int cells_w = (int) segment->len * widestGlyph(csd_font, segment->charset);
		const int caption_w = segment->caption.empty() ? 0 :
			hint_font->getRenderWidth(segment->caption) + 2 * cell_gap;
		/* The caret sits before a cell and needs room behind the last
		 * one too, or the field scrolls its first cell out of sight. */
		field_w[i] = std::max(cells_w + caret_w + field_frame + 2, caption_w);
		separator_w[i] = segment->separator.empty() ? cell_gap :
			csd_font->getRenderWidth(segment->separator) + 2 * cell_gap;
		has_captions = has_captions || !segment->caption.empty();
		total_w += field_w[i] + (i + 1 < csd_segments.size() ? separator_w[i] : 0);
	}

	const std::string hint = csd_error_text.empty() ? csd_hint_text : csd_error_text;
	const int hint_lines = 1 + (int) std::count(csd_hint_text.begin(), csd_hint_text.end(), '\n');
	const bool has_hint = !csd_hint_text.empty() || !csd_error_text.empty();
	const int hint_h = has_hint ? getDialogHintHeight(hint_font, hint_lines) : 0;
	const int caption_h = has_captions ? hint_font->getHeight() : 0;
	const int fields_y = pad + (has_hint ? hint_h + gap : 0) +
		(has_captions ? caption_h + cell_gap : 0);

	const int header_h = getHeaderObject() ? getHeaderObject()->getHeight() : 0;
	const int footer_h = getFooterObject() ? getFooterObject()->getHeight() : 0;
	const int target_h = std::max(getInputDialogMinHeight(),
			header_h + footer_h + fields_y + field_h + pad + 2 * fr_thickness);
	if (height != target_h)
	{
		height = target_h;
		setCenterPos(CC_ALONG_X | CC_ALONG_Y);
		Refresh();
		applyDialogStyle();
	}
	setCenterPos(CC_ALONG_X | CC_ALONG_Y);

	csd_hint->setDimensionsAll(pad, pad, body_w, hint_h);
	csd_hint->setText(hint, CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH, hint_font,
		csd_error_text.empty() ? COL_MENUCONTENT_TEXT_PLUS_1 : COL_RED);
	csd_hint->allowPaint(has_hint);

	int pos_x = pad + std::max(0, (body_w - total_w) / 2);
	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		segment_t *segment = csd_segments[i];

		segment->field->setFont(csd_font);
		segment->field->setCaretWidth(caret_w);
		segment->field->setDimensionsAll(pos_x, fields_y, field_w[i], field_h);

		if (segment->caption_item)
			segment->caption_item->setDimensionsAll(pos_x, fields_y - caption_h - cell_gap,
				field_w[i], caption_h);
		pos_x += field_w[i];

		if (segment->separator_item)
		{
			segment->separator_item->setTextFont(csd_font);
			segment->separator_item->setDimensionsAll(pos_x, fields_y, separator_w[i], field_h);
		}
		pos_x += separator_w[i];
	}
}

void CCStructuredInputDialog::refreshSegment(size_t index)
{
	if (index >= csd_segments.size() || !isPainted())
		return;

	csd_segments[index]->field->ensureCursorVisible();
	csd_segments[index]->field->paint(false);
	showDisplayText();
}

void CCStructuredInputDialog::focusSegment(size_t index, size_t cell)
{
	if (index >= csd_segments.size())
		return;

	if (index != csd_active && csd_active < csd_segments.size())
	{
		CCInputField *old_field = csd_segments[csd_active]->field;
		old_field->setFieldFocus(false);
		if (isPainted())
			old_field->paint(false);
	}

	csd_active = index;
	segment_t *segment = csd_segments[index];
	segment->buffer.setCursor(std::min(cell, segment->len - 1));
	segment->field->setFieldFocus(true);
	refreshSegment(index);
}

void CCStructuredInputDialog::moveCursor(int delta)
{
	segment_t *segment = csd_segments[csd_active];
	const size_t cell = segment->buffer.getCursor();

	if (delta < 0)
	{
		if (cell > 0)
		{
			segment->buffer.setCursor(cell - 1);
			refreshSegment(csd_active);
		}
		else if (csd_active > 0)
			focusSegment(csd_active - 1, csd_segments[csd_active - 1]->len - 1);
	}
	else
	{
		if (cell + 1 < segment->len)
		{
			segment->buffer.setCursor(cell + 1);
			refreshSegment(csd_active);
		}
		else if (csd_active + 1 < csd_segments.size())
			focusSegment(csd_active + 1, 0);
	}
}

void CCStructuredInputDialog::typeGlyph(const std::string &glyph)
{
	segment_t *segment = csd_segments[csd_active];
	const size_t cell = segment->buffer.getCursor();

	if (segment->charset.find(glyph) == std::string::npos ||
		!segment->buffer.replaceAt(cell, glyph))
		return;

	/* On to the next cell, and into the next segment at the end of this
	 * one; the last cell of all stays where it is. */
	if (cell + 1 < segment->len)
	{
		segment->buffer.setCursor(cell + 1);
		refreshSegment(csd_active);
	}
	else if (csd_active + 1 < csd_segments.size())
	{
		refreshSegment(csd_active);
		focusSegment(csd_active + 1, 0);
	}
	else
		refreshSegment(csd_active);
}

void CCStructuredInputDialog::stepGlyph(int delta)
{
	segment_t *segment = csd_segments[csd_active];
	const size_t cell = segment->buffer.getCursor();
	const std::string glyph = segment->buffer.glyphAt(cell);
	const int count = (int) segment->charset.size();
	const size_t found = segment->charset.find(glyph);
	const int position = found == std::string::npos ? 0 : (int) found;
	const int next = ((position + delta) % count + count) % count;

	segment->buffer.replaceAt(cell, std::string(1, segment->charset[next]));
	refreshSegment(csd_active);
}

void CCStructuredInputDialog::backspace()
{
	/* The cell before the cursor goes back to its first value, and the
	 * cursor with it - in fixed cells that is what deleting means. */
	segment_t *segment = csd_segments[csd_active];
	if (segment->buffer.getCursor() > 0 || csd_active > 0)
	{
		moveCursor(-1);
		segment = csd_segments[csd_active];
	}
	segment->buffer.replaceAt(segment->buffer.getCursor(), std::string(1, segment->charset[0]));
	refreshSegment(csd_active);
}

void CCStructuredInputDialog::clearCells()
{
	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		segment_t *segment = csd_segments[i];
		segment->buffer.setText(std::string(segment->len, segment->charset[0]));
		segment->buffer.setCursor(0);
		if (isPainted())
			segment->field->paint(false);
	}
	focusSegment(0, 0);
}

void CCStructuredInputDialog::toggleWeekday(int day)
{
	if (day < 1 || day > (int) csd_segments.size())
		return;

	segment_t *segment = csd_segments[day - 1];
	segment->buffer.replaceAt(0, segment->buffer.glyphAt(0) == "X" ? "-" : "X");
	focusSegment(day - 1, 0);
	refreshSegment(day - 1);
}

void CCStructuredInputDialog::updateHint()
{
	if (!csd_hint)
		return;

	/* The whole window, as the text input dialog does it: the hint is a
	 * transparent child, and a kill() of it alone erased to the colour
	 * of nothing - the picture behind the dialog showed through. */
	layoutItems();
	if (isPainted())
		paint(false);
}

void CCStructuredInputDialog::showError(size_t index, const std::string &reason)
{
	csd_error_text = reason;
	csd_segments[index]->field->setErrorState(true);
	focusSegment(index, 0);
	updateHint();
}

void CCStructuredInputDialog::clearError()
{
	if (csd_error_text.empty())
		return;

	csd_error_text.clear();
	for (size_t i = 0; i < csd_segments.size(); i++)
	{
		if (!csd_segments[i]->field->getErrorState())
			continue;
		csd_segments[i]->field->setErrorState(false);
		if (isPainted())
			csd_segments[i]->field->paint(false);
	}
	updateHint();
}

void CCStructuredInputDialog::showDisplayText()
{
	size_t cursor = 0;
	for (size_t i = 0; i < csd_active && i < csd_segments.size(); i++)
		cursor += csd_segments[i]->len + csd_segments[i]->separator.size();
	if (csd_active < csd_segments.size())
		cursor += csd_segments[csd_active]->buffer.getCursor();

	showFrontDisplay(getCells(), cursor);
}

void CCStructuredInputDialog::stopCarets()
{
	for (size_t i = 0; i < csd_segments.size(); i++)
		if (csd_segments[i]->field)
			csd_segments[i]->field->cancelCaretBlink();
}

bool CCStructuredInputDialog::confirmDiscard()
{
	stopCarets();

	const bool discard = askDiscard();
	if (!discard)
		refreshSegment(csd_active);

	return discard;
}

bool CCStructuredInputDialog::save()
{
	size_t bad = 0;
	std::string reason;
	if (!validate(bad, reason))
	{
		showError(bad, reason);
		return false;
	}

	if (csd_time)
	{
		time_t made = 0;
		makeTime(made);
		*csd_time = made;
		csd_time_text = formatTime(made);
	}
	else if (csd_value)
		setSettingsText(*csd_value, composeValue());

	csd_saved = true;
	return true;
}

int CCStructuredInputDialog::exec(CMenuTarget *parent, const std::string & /*actionKey*/)
{
	CStringInputOpen counted;
	neutrino_msg_t msg = CRCInput::RC_nokey;
	neutrino_msg_data_t data = 0;
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	/* A date starts from its time_t, not from the text a menu row shows:
	 * that text may have been written by someone else meanwhile. */
	if (csd_time)
		csd_time_text = formatTime(*csd_time);

	std::string *target = getTarget();
	const std::string original = target ? *target : std::string();

	/* Built at every open: a time's hours are as wide as its value. */
	defineSegments(original);
	buildItems();
	loadValue(original);
	csd_saved = false;
	csd_error_text.clear();
	for (size_t i = 0; i < csd_segments.size(); i++)
		csd_segments[i]->field->setFieldFocus(i == 0);
	csd_active = 0;
	layoutItems();

	const std::string start_cells = getCells();

	paint();
	showDisplayText();

	uint64_t timeoutEnd =
		CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(
					g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if (msg == CRCInput::RC_left || msg == CRCInput::RC_right)
		{
			clearError();
			moveCursor(msg == CRCInput::RC_left ? -1 : 1);
		}
		else if (msg == CRCInput::RC_up || msg == CRCInput::RC_down)
		{
			clearError();
			stepGlyph(msg == CRCInput::RC_up ? 1 : -1);
		}
		else if (msg == CRCInput::RC_page_up)
			focusSegment(0, 0);
		else if (msg == CRCInput::RC_page_down)
			focusSegment(csd_segments.size() - 1, csd_segments.back()->len - 1);
		else if (msg == CRCInput::RC_backspace || msg == CRCInput::RC_rewind)
		{
			clearError();
			backspace();
		}
		else if (msg == CRCInput::RC_timeout)
		{
			if (getCells() != start_cells && !confirmDiscard())
			{
				timeoutEnd = CRCInput::calcTimeoutEnd(
						g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
				continue;
			}
			loop = false;
			res = menu_return::RETURN_EXIT_REPAINT;
		}
		else
		{
			const int btn_res = sendButtonKey(msg);
			if (btn_res == RES_SAVE)
			{
				if (save())
					loop = false;
			}
			else if (btn_res == RES_CANCEL)
			{
				if (getCells() != start_cells && !confirmDiscard())
					continue;
				loop = false;
				res = menu_return::RETURN_EXIT_REPAINT;
			}
			else if (btn_res == RES_BACKSPACE)
			{
				clearError();
				backspace();
			}
			else if (btn_res == RES_CLEAR)
			{
				clearError();
				clearCells();
			}
			else if (CNeutrinoApp::getInstance()->listModeKey(msg))
				continue;
			else
			{
				std::string glyph;
				const char *unicode_value = CRCInput::getUnicodeValue(msg);
				if (unicode_value && *unicode_value)
					glyph = unicode_value;
				else if (CRCInput::isNumeric(msg))
					glyph = std::string(1, '0' + CRCInput::getNumericValue(msg));

				if (!glyph.empty())
				{
					clearError();
					if (csd_format == FORMAT_WEEKDAYS)
					{
						if (glyph.size() == 1 && glyph[0] >= '1' && glyph[0] <= '7')
							toggleWeekday(glyph[0] - '0');
					}
					else
						typeGlyph(glyph);
				}
				else if (forwardMessage(msg, data))
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}
		}
	}

	hide();
	releaseFrontDisplay();

	/* The target was never written unless saved, so there is nothing to
	 * put back - and nothing written meanwhile gets overwritten. */
	if (csd_saved)
		notifyObserver(csd_observ, target);
	if (csd_cancel)
		*csd_cancel = !csd_saved;

	return res;
}

void CCStructuredInputDialog::hide()
{
	stopCarets();
	CComponentsWindow::hide();
}
