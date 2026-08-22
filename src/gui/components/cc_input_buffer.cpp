/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern input buffer
	Copyright (C) 2026

	License: GPL
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc_input_buffer.h"

#include <algorithm>
#include <cctype>

CCInputBuffer::CCInputBuffer()
{
	ib_value_dirty = true;
	ib_cursor = 0;
	ib_max_chars = 0;
	ib_allow_empty = true;
	ib_filter_mode = FILTER_FREE;
}

std::string CCInputBuffer::utf8ToGlyph(const char *&text)
{
	std::string glyph;

	if (!text || !*text)
		return glyph;

	glyph = *text;
	if ((((unsigned char)(*text)) & 0x80) != 0)
	{
		int remaining_unicode_length = 0;
		if ((((unsigned char)(*text)) & 0xf8) == 0xf0)
			remaining_unicode_length = 3;
		else if ((((unsigned char)(*text)) & 0xf0) == 0xe0)
			remaining_unicode_length = 2;
		else if ((((unsigned char)(*text)) & 0xe0) == 0xc0)
			remaining_unicode_length = 1;

		for (int i = 0; i < remaining_unicode_length; i++)
		{
			text++;
			if (((*text) & 0xc0) != 0x80)
			{
				/* Not a continuation byte - the sequence ends
				 * early. Step back so this function always
				 * leaves the pointer ON the last consumed
				 * byte: the caller advances by one itself, and
				 * without the step back that advance would
				 * jump over this byte - past the terminator
				 * when a truncated lead byte ends the string,
				 * and while (*cursor) would then read beyond
				 * the buffer. */
				text--;
				break;
			}
			glyph += *text;
		}
	}

	return glyph;
}

bool CCInputBuffer::isAllowedGlyph(const std::string &glyph) const
{
	if (glyph.empty())
		return false;

	switch (ib_filter_mode)
	{
		case FILTER_NUMERIC:
			return glyph.size() == 1 &&
				std::isdigit((unsigned char) glyph[0]) != 0;
		case FILTER_ASCII_VISIBLE:
			return glyph.size() == 1 &&
				glyph[0] >= 0x20 &&
				glyph[0] <= 0x7e;
		case FILTER_HEX:
			return glyph.size() == 1 &&
				std::isxdigit((unsigned char) glyph[0]) != 0;
		case FILTER_FREE:
		default:
			return true;
	}
}

void CCInputBuffer::markDirty()
{
	ib_value_dirty = true;
}

void CCInputBuffer::rebuildValueCache() const
{
	if (!ib_value_dirty)
		return;

	ib_value_cache.clear();
	for (size_t i = 0; i < ib_glyphs.size(); i++)
		ib_value_cache += ib_glyphs[i];

	ib_value_dirty = false;
}

void CCInputBuffer::setText(const std::string &text)
{
	ib_glyphs.clear();

	const char *cursor = text.c_str();
	while (*cursor)
	{
		std::string glyph = utf8ToGlyph(cursor);
		if (isAllowedGlyph(glyph))
		{
			if (ib_max_chars && ib_glyphs.size() >= ib_max_chars)
				break;
			ib_glyphs.push_back(glyph);
		}
		cursor++;
	}

	ib_cursor = ib_glyphs.size();
	markDirty();
}

const std::string &CCInputBuffer::getText() const
{
	rebuildValueCache();
	return ib_value_cache;
}

std::string CCInputBuffer::getRange(size_t first, size_t last) const
{
	if (first >= ib_glyphs.size() || first >= last)
		return std::string();

	last = std::min(last, ib_glyphs.size());

	std::string out;
	for (size_t i = first; i < last; i++)
		out += ib_glyphs[i];

	return out;
}

std::string CCInputBuffer::glyphAt(size_t index) const
{
	if (index >= ib_glyphs.size())
		return std::string();

	return ib_glyphs[index];
}

size_t CCInputBuffer::size() const
{
	return ib_glyphs.size();
}

bool CCInputBuffer::empty() const
{
	return ib_glyphs.empty();
}

size_t CCInputBuffer::getCursor() const
{
	return ib_cursor;
}

void CCInputBuffer::setCursor(size_t index)
{
	ib_cursor = std::min(index, ib_glyphs.size());
}

void CCInputBuffer::moveLeft()
{
	if (ib_cursor > 0)
		ib_cursor--;
}

void CCInputBuffer::moveRight()
{
	if (ib_cursor < ib_glyphs.size())
		ib_cursor++;
}

void CCInputBuffer::moveHome()
{
	ib_cursor = 0;
}

void CCInputBuffer::moveEnd()
{
	ib_cursor = ib_glyphs.size();
}

bool CCInputBuffer::insert(const std::string &glyph)
{
	if (!isAllowedGlyph(glyph))
		return false;

	if (ib_max_chars && ib_glyphs.size() >= ib_max_chars)
		return false;

	const size_t insert_pos = std::min(ib_cursor, ib_glyphs.size());
	ib_glyphs.insert(ib_glyphs.begin() + insert_pos, glyph);
	ib_cursor = insert_pos + 1;
	markDirty();
	return true;
}

bool CCInputBuffer::backspace()
{
	if (ib_cursor == 0 || ib_glyphs.empty())
		return false;

	ib_glyphs.erase(ib_glyphs.begin() + (ib_cursor - 1));
	ib_cursor--;
	markDirty();
	return true;
}

bool CCInputBuffer::erase()
{
	if (ib_cursor >= ib_glyphs.size())
		return false;

	ib_glyphs.erase(ib_glyphs.begin() + ib_cursor);
	markDirty();
	return true;
}

bool CCInputBuffer::replaceAt(size_t index, const std::string &glyph)
{
	if (index >= ib_glyphs.size())
		return false;

	if (!isAllowedGlyph(glyph))
		return false;

	ib_glyphs[index] = glyph;
	markDirty();
	return true;
}

void CCInputBuffer::clear()
{
	ib_glyphs.clear();
	ib_cursor = 0;
	markDirty();
}

void CCInputBuffer::setMaxChars(size_t max_chars)
{
	ib_max_chars = max_chars;

	if (ib_max_chars && ib_glyphs.size() > ib_max_chars)
		ib_glyphs.resize(ib_max_chars);

	ib_cursor = std::min(ib_cursor, ib_glyphs.size());
	markDirty();
}

size_t CCInputBuffer::getMaxChars() const
{
	return ib_max_chars;
}

void CCInputBuffer::setAllowEmpty(bool allow_empty)
{
	ib_allow_empty = allow_empty;
}

bool CCInputBuffer::allowEmpty() const
{
	return ib_allow_empty;
}

bool CCInputBuffer::isAcceptable() const
{
	if (!ib_allow_empty && ib_glyphs.empty())
		return false;

	return true;
}

void CCInputBuffer::setFilterMode(input_filter_mode_t mode)
{
	ib_filter_mode = mode;
}

CCInputBuffer::input_filter_mode_t CCInputBuffer::getFilterMode() const
{
	return ib_filter_mode;
}
