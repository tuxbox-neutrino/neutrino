#ifndef SILANGUAGES_HPP
#define SILANGUAGES_HPP
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
// $Log: SIlanguage.hpp,v $
// Revision 1.2  2006/04/12 21:23:58  Arzka
// Optimization.
// Removed unnecessary copying of std:map and
// removed few avoidable std::string creation
//
// Revision 1.1  2006/03/26 20:13:49  Arzka
// Added support for selecting EPG language
//
//
//

#include <string>
#include <vector>
#include <map>

#include <sectionsdclient/sectionsdclient.h>

#define LANGUAGEFILE "/var/tuxbox/config/epglanguages.conf"

class SIlanguage {
public:
	static void filter(const std::map<std::string, std::string>& s, int max, std::string& retval);
	static bool loadLanguages();
	static bool saveLanguages();
	static void setLanguages(const std::vector<std::string>& newLanguages);
	static std::vector<std::string> getLanguages();
	static void setMode(CSectionsdClient::SIlanguageMode_t newMode);
	static CSectionsdClient::SIlanguageMode_t getMode();

private:
	SIlanguage();
	~SIlanguage();
	static std::vector<std::string> languages;
	static pthread_mutex_t languages_lock;
	static CSectionsdClient::SIlanguageMode_t mode;
};

#endif
