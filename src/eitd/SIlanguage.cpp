/*
 * SIlanguage.cpp, Class for filtering preferred language
 *
 * Copyright (C) 2001 arzka (dbox2@oh3mqu.pp.hyper.fi)
 * Copyright (C) 2006 houdini, mws
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <vector>
#include <map>
#include <pthread.h>
#include <fstream>

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#include <OpenThreads/ScopedLock>

#include <sectionsdclient/sectionsdclient.h>

#include "SIlanguage.hpp"

static OpenThreads::Mutex languages_lock;
static std::vector<std::string> languages;

static OpenThreads::Mutex langIndexMutex;
std::vector<std::string> langIndex;

static const std::string languageOFF = "OFF";

unsigned int getLangIndex(const std::string &lang)
{
	unsigned int ix = 0;
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(langIndexMutex);
	if (!langIndex.size())
		langIndex.push_back(languageOFF);
	for (std::vector<std::string>::iterator it = langIndex.begin(); it != langIndex.end(); ++it, ++ix)
		if (*it == lang)
			return ix;
	langIndex.push_back(lang);
	return ix;
}


/*
ALL = Show all available languages
FIRST_FIRST = Show first found language from list. If not found show first of available language
FIRST_ALL = Show first found language from list. If not found show all available languages.
ALL_FIRST = Show all wanted languages if possible. If not found show first of av ailable language
ALL_ALL = Show all wanted languages if possible. If not found show all available languages.
*/
//CSectionsdClient::SIlanguageMode_t SIlanguage::mode = CSectionsdClient::ALL;
CSectionsdClient::SIlanguageMode_t SIlanguage::mode = CSectionsdClient::FIRST_ALL;

void SIlanguage::filter(const std::list<SILangData>& s, SILangData::SILangDataIndex textIndex, int max, std::string& retval)
{
	// languages cannot get iterated through
	// if another thread is updating it simultaneously
	int count = max;

	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(languages_lock);
	if (mode != CSectionsdClient::ALL) {
		for (std::vector<std::string>::const_iterator it = languages.begin() ;
				it != languages.end() ; ++it) {
			std::list<SILangData>::const_iterator text;
			unsigned int lang = getLangIndex(*it);
			for (text = s.begin(); text != s.end() && text->lang != lang; ++text);
			if (text != s.end()) {
				if (text->text[textIndex].empty())
					continue;
				if (count != max) {
					retval.append(" \n");
				}
				retval.append(text->text[textIndex]);
				if (--count == 0) break;
				if (mode == CSectionsdClient::FIRST_FIRST ||
						mode == CSectionsdClient::FIRST_ALL) {
					break;
				}
			}
		}
	}

	if (retval.length() == 0) {
		// return all available languages
		if (s.begin() != s.end()) {
			for (std::list<SILangData>::const_iterator it = s.begin() ;
					it != s.end() ; ++it) {
				if (it->text[textIndex].empty())
					continue;
				if (it != s.begin()) {
					retval.append(" \n");
				}
				retval.append(it->text[textIndex]);
				if (--max == 0) break;
				if (mode == CSectionsdClient::FIRST_FIRST ||
						mode == CSectionsdClient::ALL_FIRST) {
					break;
				}
			}
		}
	}
}


bool SIlanguage::loadLanguages()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(languages_lock);
	std::ifstream file(LANGUAGEFILE);
	std::string word;
	CSectionsdClient::SIlanguageMode_t tmpMode = CSectionsdClient::FIRST_ALL;
	std::vector<std::string> tmpLang;

	if (!(file >> word)) goto error;
	if (word == "FIRST_FIRST") {
		tmpMode = CSectionsdClient::FIRST_FIRST;
	} else if (word == "FIRST_ALL") {
		tmpMode = CSectionsdClient::FIRST_ALL;
	} else if (word == "ALL_FIRST") {
		tmpMode = CSectionsdClient::ALL_FIRST;
	} else if (word == "ALL_ALL") {
		tmpMode = CSectionsdClient::ALL_ALL;
	} else if (word == "OFF") {
		tmpMode = CSectionsdClient::LANGUAGE_MODE_OFF;
	}

	while (!file.eof()) {
		if ((file >> word)) {
			tmpLang.push_back(word);
		}
	}

	if (tmpLang.empty()) tmpLang.push_back("OFF");
	languages = tmpLang;
	mode = tmpMode;
	return true;

error:
	tmpLang.push_back("OFF");
	languages = tmpLang;
	mode = tmpMode;
	return false;
}

bool SIlanguage::saveLanguages()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(languages_lock);
	std::ofstream file(LANGUAGEFILE);
	switch(mode) {
	case CSectionsdClient::ALL:
		file << "ALL\n";
		break;
	case CSectionsdClient::FIRST_FIRST:
		file << "FIRST_FIRST\n";
		break;
	case CSectionsdClient::FIRST_ALL:
		file << "FIRST_ALL\n";
		break;
	case CSectionsdClient::ALL_FIRST:
		file << "ALL_FIRST\n";
		break;
	case CSectionsdClient::ALL_ALL:
		file << "ALL_ALL\n";
		break;
	case CSectionsdClient::LANGUAGE_MODE_OFF:
		file << "OFF\n";
		break;
	}

	for (std::vector<std::string>::iterator it = languages.begin() ;
			it != languages.end() ; ++it) {
		file << *it;
		file << "\n";
		if (file.fail()) goto error;
	}

	file.close();
	if (file.fail()) goto error;

	return true;

error:
	return false;
}

void SIlanguage::setLanguages(const std::vector<std::string>& newLanguages)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(languages_lock);
	languages = newLanguages;
}

std::vector<std::string> SIlanguage::getLanguages()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(languages_lock);
	std::vector<std::string> retval(languages);  // Store it before unlock
	return retval;
}

void SIlanguage::setMode(CSectionsdClient::SIlanguageMode_t newMode)
{
	mode = newMode;
}

CSectionsdClient::SIlanguageMode_t SIlanguage::getMode()
{
	return mode;
}

