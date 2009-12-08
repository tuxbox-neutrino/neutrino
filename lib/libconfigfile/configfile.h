/*
 * $Id: configfile.h,v 1.11 2003/09/25 18:52:27 thegoodguy Exp $
 *
 * Copyright (C) 2001, 2002 Andreas Oberritter <obi@tuxbox.org>,
 *                          thegoodguy  <thegoodguy@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef __configfile_h__
#define __configfile_h__


#include <stdint.h>

#include <map>
#include <string>
#include <vector>


class CConfigFile
{
 private:

	typedef std::map<std::string, std::string> ConfigDataMap;

	ConfigDataMap configData;
	char delimiter;
	bool saveDefaults;
	bool modifiedFlag;
	bool unknownKeyQueryedFlag;

	void storeBool(const std::string & key, const bool val);
	void storeInt32(const std::string & key, const int32_t val);
	void storeInt64(const std::string & key, const int64_t val);
	void storeString(const std::string & key, const std::string & val);

 public:
	CConfigFile(const char p_delimiter, const bool p_saveDefaults = true);

	const bool loadConfig(const char * const filename);
	const bool loadConfig(const std::string & filename);

	const bool saveConfig(const char * const filename);
	const bool saveConfig(const std::string & filename);

	void clear();

	//
	// strings
	//
	std::string getString(const char * const key, const std::string & defaultVal = "");
	std::string getString(const std::string & key, const std::string & defaultVal = "");
	void setString(const char * const key, const std::string & val);
	void setString(const std::string & key, const std::string & val);

	//
	// 32, 64 bit int
	//
	int32_t getInt32(const char * const key, const int32_t defaultVal = 0);
	int32_t getInt32(const std::string & key, const int32_t defaultVal = 0);
	void setInt32(const char * const key, const int32_t val);
	void setInt32(const std::string & key, const int32_t val);

	int64_t getInt64(const char * const key, const int64_t defaultVal = 0);
	int64_t getInt64(const std::string & key, const int64_t defaultVal = 0);
	void setInt64(const char * const key, const int64_t val);
	void setInt64(const std::string & key, const int64_t val);

	//
	// boolean
	//
	bool getBool(const char * const key, const bool defaultVal = false);
	bool getBool(const std::string & key, const bool defaultVal = false);
	void setBool(const char * const key, const bool val);
	void setBool(const std::string & key, const bool val);

	//
	// vectors
	//
	std::vector <std::string> getStringVector(const std::string & key);
	void setStringVector(const std::string & key, const std::vector<std::string> vec);

	std::vector <int32_t> getInt32Vector(const std::string & key);
	void setInt32Vector(const std::string & key, const std::vector<int32_t> vec);

	//
	// flags
	//
	const bool getModifiedFlag() const { return modifiedFlag; }
	void setModifiedFlag(const bool val) { modifiedFlag = val; }

	const bool getUnknownKeyQueryedFlag() const { return unknownKeyQueryedFlag; }
	void setUnknownKeyQueryedFlag(const bool val) { unknownKeyQueryedFlag = val; }

};

#endif /* __configfile_h__ */
