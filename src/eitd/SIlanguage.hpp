//
// $Id: SIlanguage.hpp,v 1.2 2006/04/12 21:23:58 Arzka Exp $
//
// Class for filtering preferred language
//
//    Copyright (C) 2001 arzka (dbox2@oh3mqu.pp.hyper.fi)
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef SILANGUAGES_HPP
#define SILANGUAGES_HPP

#include <string>
#include <vector>
#include <list>

#include <sectionsdclient/sectionsdclient.h>

#define LANGUAGEFILE CONFIGDIR "/epglanguages.conf"

class SILangData {
public:
	enum SILangDataIndex { langName = 0, langText, langExtendedText, langMax };
	unsigned int lang;
	std::string text[langMax];
};

class SIlanguage {
public:
	static void filter(const std::list<SILangData>& s, SILangData::SILangDataIndex textIndex, int max, std::string& retval);
	static bool loadLanguages();
	static bool saveLanguages();
	static void setLanguages(const std::vector<std::string>& newLanguages);
	static std::vector<std::string> getLanguages();
	static void setMode(CSectionsdClient::SIlanguageMode_t newMode);
	static CSectionsdClient::SIlanguageMode_t getMode();

private:
	SIlanguage();
	~SIlanguage();
	static CSectionsdClient::SIlanguageMode_t mode;
};

extern std::vector<std::string> langIndex;
unsigned int getLangIndex(const std::string &lang);
#endif
