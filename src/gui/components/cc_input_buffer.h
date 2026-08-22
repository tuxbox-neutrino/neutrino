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
			FILTER_HEX
		};

	private:
		std::vector<std::string> ib_glyphs;
		mutable std::string ib_value_cache;
		mutable bool ib_value_dirty;
		size_t ib_cursor;
		size_t ib_max_chars;
		bool ib_allow_empty;
		input_filter_mode_t ib_filter_mode;

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
};

#endif
