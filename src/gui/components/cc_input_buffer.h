/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Modern input buffer
	Copyright (C) 2026

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.
*/

#ifndef __CC_INPUT_BUFFER_H__
#define __CC_INPUT_BUFFER_H__

#include <stddef.h>
#include <string>
#include <vector>

class CCInputBuffer
{
	public:
		enum input_filter_mode_t
		{
			FILTER_FREE = 0,
			FILTER_NUMERIC,
			FILTER_ASCII_VISIBLE,
			FILTER_HEX,
			///the glyphs given to setCharset(); an empty set takes any glyph
			FILTER_CHARSET
		};

	private:
		std::vector<std::string> ib_glyphs;
		mutable std::string ib_value_cache;
		mutable bool ib_value_dirty;
		size_t ib_cursor;
		size_t ib_max_chars;
		bool ib_allow_empty;
		input_filter_mode_t ib_filter_mode;
		std::vector<std::string> ib_charset;
		std::vector<std::string> ib_kept_glyphs;

		static std::string utf8ToGlyph(const char *&text);
		bool isAllowedGlyph(const std::string &glyph) const;
		void markDirty();
		void rebuildValueCache() const;

	public:
		CCInputBuffer();

		void setText(const std::string &text);
		const std::string &getText() const;
		std::string getRange(size_t first, size_t last) const;
		std::string glyphAt(size_t index) const;

		size_t size() const;
		bool empty() const;

		size_t getCursor() const;
		void setCursor(size_t index);
		void moveLeft();
		void moveRight();
		void moveHome();
		void moveEnd();

		bool insert(const std::string &glyph);
		bool backspace();
		bool erase();
		bool replaceAt(size_t index, const std::string &glyph);
		void clear();

		void setMaxChars(size_t max_chars);
		size_t getMaxChars() const;
		void setAllowEmpty(bool allow_empty);
		bool allowEmpty() const;
		bool isAcceptable() const;
		void setFilterMode(input_filter_mode_t mode);
		input_filter_mode_t getFilterMode() const;

		/**
		 * The glyphs FILTER_CHARSET accepts, given as one UTF-8 string.
		 *
		 * setText() keeps every glyph of the value it is handed, also
		 * one outside the set, and such a glyph may be typed again
		 * afterwards: the set says what can be entered, it must not
		 * change a value nobody edited. A coordinate "52.52" stays
		 * intact even when the set only names the digits.
		 */
		void setCharset(const std::string &glyphs);
};

#endif
