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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc_keyboard_layout.h"
#include "cc_keyboard_keys.h"

CKeyboardLayoutData::CKeyboardLayoutData()
{
	kld_index = 0;
	kld_caps = 0;
	kld_initialized = false;
}

size_t CKeyboardLayoutData::getLayoutCount()
{
	return LAYOUT_COUNT;
}

void CKeyboardLayoutData::initByLocale(const std::string &locale)
{
	if (kld_initialized)
		return;

	kld_initialized = true;
	kld_index = 0;
	for (size_t i = 0; i < LAYOUT_COUNT; i++) {
		if (keyboards[i].locale == locale) {
			kld_index = i;
			return;
		}
	}
}

void CKeyboardLayoutData::initByPreference(const std::string &preferred, const std::string &fallback)
{
	initByLocale(resolveLocale(preferred, fallback));
}

std::string CKeyboardLayoutData::resolveLocale(const std::string &preferred, const std::string &fallback)
{
	if (hasLocale(preferred))
		return preferred;
	if (hasLocale(fallback))
		return fallback;
	return keyboards[0].locale;
}

bool CKeyboardLayoutData::hasLocale(const std::string &locale)
{
	if (locale.empty())
		return false;

	for (size_t i = 0; i < LAYOUT_COUNT; i++) {
		if (keyboards[i].locale == locale)
			return true;
	}
	return false;
}

void CKeyboardLayoutData::nextLayout()
{
	kld_initialized = true;
	kld_index++;
	if (kld_index >= LAYOUT_COUNT)
		kld_index = 0;
}

void CKeyboardLayoutData::toggleCaps()
{
	kld_caps = kld_caps ? 0 : 1;
}

void CKeyboardLayoutData::setCaps(const bool &caps)
{
	kld_caps = caps ? 1 : 0;
}

const std::string &CKeyboardLayoutData::glyphAt(const int &row, const int &column) const
{
	static const std::string none;

	if (row < 0 || row >= KEY_ROWS || column < 0 || column >= KEY_COLUMNS)
		return none;

	return keyboards[kld_index].keys[kld_caps][row][column];
}

const std::string &CKeyboardLayoutData::getLayoutName() const
{
	return keyboards[kld_index].name;
}

const std::string &CKeyboardLayoutData::getLayoutLocale() const
{
	return keyboards[kld_index].locale;
}
