/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Keyboard layout tables without a keyboard
	Copyright (C) 2026

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.
*/

#ifndef __KEYBOARD_LAYOUT_H__
#define __KEYBOARD_LAYOUT_H__

#include <string>

#define KEY_ROWS 4
#define KEY_COLUMNS 14

struct keyboard_layout
{
	std::string name;
	std::string locale;
	std::string(*keys)[KEY_ROWS][KEY_COLUMNS];
};

/**
 * The keyboard layouts without a keyboard: which layout is active, whether
 * caps is on, and what glyph sits at a given row and column.
 *
 * Holds no widget and paints nothing, so both the legacy CKeyboardInput and
 * the cc on-screen keyboard can share one set of tables and one notion of
 * what "the current layout" means.
 */
class CKeyboardLayoutData
{
	private:
		size_t kld_index;
		int kld_caps;
		bool kld_initialized;

	public:
		CKeyboardLayoutData();

		/**
		 * Picks the layout whose locale matches, or the first one.
		 * Does nothing once a layout has been chosen, so it cannot
		 * undo a manual switch.
		 */
		void initByLocale(const std::string &locale);

		/**
		 * Picks the preferred locale when it names a known layout,
		 * the fallback otherwise. The preferred value comes from the
		 * user pinning a layout by hand, so an empty or stale entry
		 * must not beat the fallback's locale match.
		 */
		void initByPreference(const std::string &preferred, const std::string &fallback);

		/**
		 * The locale initByPreference() would pick: the preferred one
		 * when it names a known layout, else the fallback when it
		 * does, else the first table's locale. One place decides, so
		 * a caller can ask "would this change anything" before paying
		 * for a rebuild.
		 */
		static std::string resolveLocale(const std::string &preferred, const std::string &fallback);

		///true when a layout with this locale exists in the tables
		static bool hasLocale(const std::string &locale);

		///advances to the next layout, wrapping around
		void nextLayout();

		void toggleCaps();
		void setCaps(const bool &caps);
		bool hasCaps() const {return kld_caps != 0;}

		///glyph at row/column of the active table, empty when unassigned or out of range
		const std::string &glyphAt(const int &row, const int &column) const;

		const std::string &getLayoutName() const;
		const std::string &getLayoutLocale() const;

		static size_t getLayoutCount();
		static int getRowCount() {return KEY_ROWS;}
		static int getColumnCount() {return KEY_COLUMNS;}
};

#endif /* __KEYBOARD_LAYOUT_H__ */
