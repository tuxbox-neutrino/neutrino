/*
 * $Id: configfile.cpp,v 1.19 2003/11/23 19:16:03 obi Exp $
 *
 * configuration object for the d-box 2 linux project
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

#include "configfile.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

CConfigFile::CConfigFile(const char p_delimiter, const bool p_saveDefaults)
{
	modifiedFlag = false;
	unknownKeyQueryedFlag = false;
	delimiter = p_delimiter;
	saveDefaults = p_saveDefaults;
}

void CConfigFile::clear()
{
	configData.clear();
}

//
// public file operation methods
//
bool CConfigFile::loadConfig(const char * const filename, char _delimiter)
{
	std::ifstream configFile(filename);

	if (configFile != NULL)
	{
		std::string s;
		clear();
		modifiedFlag = false;

		for (int linenr = 1; ; linenr++)
		{
			getline(configFile, s);
			if (configFile.fail())
				break;

			std::string::size_type i = s.find(_delimiter);
			if (i != std::string::npos)
			{
				std::string::size_type j = s.find('#');
				if (j == std::string::npos)
					j = s.length();
				configData[s.substr(0, i)] = s.substr(i + 1, j - (i + 1));
			}
		}
		configFile.close();
		return true;
	}
	else
	{
		std::cerr << "[ConfigFile] Unable to open file " << filename << " for reading." << std::endl;
		return false;
	}
}

bool CConfigFile::loadConfig(const std::string & filename, char _delimiter)
{
	return loadConfig(filename.c_str(), _delimiter);
}

bool CConfigFile::saveConfig(const char * const filename, char _delimiter)
{
	std::string tmpname = std::string(filename) + ".tmp";
	unlink(tmpname.c_str());
	std::fstream configFile(tmpname.c_str(), std::ios::out);

	if (configFile != NULL)
	{
		std::cout << "[ConfigFile] saving " << filename << std::endl;
		for (ConfigDataMap::const_iterator it = configData.begin(); it != configData.end(); ++it)
		{
			configFile << it->first << _delimiter << it->second << std::endl;
		}

		configFile.sync();
		configFile.close();

		chmod(tmpname.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		/* TODO: check available space? */
		rename(tmpname.c_str(), filename);

		modifiedFlag = false;
		return true;
	}
	else
	{
		std::cerr << "[ConfigFile] Unable to open file " << tmpname << " for writing: "
				<< strerror(errno) << std::endl;
		return false;
	}
}

bool CConfigFile::saveConfig(const std::string & filename, char _delimiter)
{
	return saveConfig(filename.c_str(), _delimiter);
}



//
// private "store" methods
// 
void CConfigFile::storeBool(const std::string & key, const bool val)
{
	if (val == true)
		configData[key] = std::string("true");
	else
		configData[key] = std::string("false");
}

void CConfigFile::storeInt32(const std::string & key, const int32_t val)
{
	std::stringstream s;
	s << val;
	s >> configData[key];
}

void CConfigFile::storeInt64(const std::string & key, const int64_t val)
{
	std::stringstream s;
	s << val;
	s >> configData[key];
}

void CConfigFile::storeString(const std::string & key, const std::string & val)
{
	configData[key] = val;
}



//
// public "get" methods
//
bool CConfigFile::getBool(const char * const key, const bool defaultVal)
{
	return getBool(std::string(key), defaultVal);
}

bool CConfigFile::getBool(const std::string & key, const bool defaultVal)
{
	if (configData.find(key) == configData.end())
	{
		if (saveDefaults) {
			unknownKeyQueryedFlag = true;
			storeBool(key, defaultVal);
		}
		else {
			return defaultVal;
		}
	}

	return !((configData[key] == "false") || (configData[key] == "0"));
}

int32_t CConfigFile::getInt32(const char * const key, const int32_t defaultVal)
{
	return getInt32(std::string(key), defaultVal);
}

int32_t CConfigFile::getInt32(const std::string & key, const int32_t defaultVal)
{
	if (configData.find(key) == configData.end())
	{
		if (saveDefaults) {
			unknownKeyQueryedFlag = true;
			storeInt32(key, defaultVal);
		}
		else {
			return defaultVal;
		}
	}

	return atoi(configData[key].c_str());
}

int64_t CConfigFile::getInt64(const char * const key, const int64_t defaultVal)
{
	return getInt64(std::string(key), defaultVal);
}

int64_t CConfigFile::getInt64(const std::string & key, const int64_t defaultVal)
{
	if (configData.find(key) == configData.end())
	{
		if (saveDefaults) {
			unknownKeyQueryedFlag = true;
			storeInt64(key, defaultVal);
		}
		else {
			return defaultVal;
		}
	}

	return atoll(configData[key].c_str());
}

std::string CConfigFile::getString(const char * const key, const std::string & defaultVal)
{
	return getString(std::string(key), defaultVal);
}

std::string CConfigFile::getString(const std::string & key, const std::string & defaultVal)
{
	if (configData.find(key) == configData.end())
	{
		if (saveDefaults) {
			unknownKeyQueryedFlag = true;
			storeString(key, defaultVal);
		}
		else {
			return defaultVal;
		}
	}

	return configData[key];
}

std::vector <int32_t> CConfigFile::getInt32Vector(const std::string & key)
{
	std::string val = configData[key];
	std::vector <int32_t> vec;
	uint16_t length = 0;
	uint16_t pos = 0;
	uint16_t i;

	for (i = 0; i < val.length(); i++)
	{
		if (val[i] == delimiter)
		{
			vec.push_back(atoi(val.substr(pos, length).c_str()));
			pos = i + 1;
			length = 0;
		}
		else
		{
			length++;
		}
	}

	if (length == 0)
		unknownKeyQueryedFlag = true;
	else
		vec.push_back(atoi(val.substr(pos, length).c_str()));

	return vec;
}

std::vector <std::string> CConfigFile::getStringVector(const std::string & key)
{
	std::string val = configData[key];
	std::vector <std::string> vec;
	uint16_t length = 0;
	uint16_t pos = 0;
	uint16_t i;

	for (i = 0; i < val.length(); i++)
	{
		if (val[i] == delimiter)
		{
			vec.push_back(val.substr(pos, length));
			pos = i + 1;
			length = 0;
		}
		else
		{
			length++;
		}
	}

	if (length == 0)
		unknownKeyQueryedFlag = true;
	else
		vec.push_back(val.substr(pos, length));

	return vec;
}



//
// public "set" methods
//
void CConfigFile::setBool(const char * const key, const bool val)
{
	setBool(std::string(key), val);
}

void CConfigFile::setBool(const std::string & key, const bool val)
{
	bool tmpUnknownKeyQueryedFlag = unknownKeyQueryedFlag;
	unknownKeyQueryedFlag = false;
	bool oldVal = getBool(key);

	if ((oldVal != val) || (unknownKeyQueryedFlag))
	{
		modifiedFlag = true;
		storeBool(key, val);
	}

	unknownKeyQueryedFlag = tmpUnknownKeyQueryedFlag;
}

void CConfigFile::setInt32(const char * const key, int32_t val)
{
	setInt32(std::string(key), val);
}

void CConfigFile::setInt32(const std::string & key, int32_t val)
{
	bool tmpUnknownKeyQueryedFlag = unknownKeyQueryedFlag;
	unknownKeyQueryedFlag = false;
	int32_t oldVal = getInt32(key);

	if ((oldVal != val) || (unknownKeyQueryedFlag))
	{
		modifiedFlag = true;
		storeInt32(key, val);
	}

	unknownKeyQueryedFlag = tmpUnknownKeyQueryedFlag;
}

void CConfigFile::setInt64(const char * const key, const int64_t val)
{
	setInt64(std::string(key), val);
}

void CConfigFile::setInt64(const std::string & key, const int64_t val)
{
	bool tmpUnknownKeyQueryedFlag = unknownKeyQueryedFlag;
	unknownKeyQueryedFlag = false;
	int64_t oldVal = getInt64(key);

	if ((oldVal != val) || (unknownKeyQueryedFlag))
	{
		modifiedFlag = true;
		storeInt64(key, val);
	}

	unknownKeyQueryedFlag = tmpUnknownKeyQueryedFlag;
}

void CConfigFile::setString(const char * const key, const std::string & val)
{
	setString(std::string(key), val);
}

void CConfigFile::setString(const std::string & key, const std::string & val)
{
	bool tmpUnknownKeyQueryedFlag = unknownKeyQueryedFlag;
	unknownKeyQueryedFlag = false;
	std::string oldVal = getString(key);
	
	if ((oldVal != val) || (unknownKeyQueryedFlag))
	{
		modifiedFlag = true;
		storeString(key, val);
	}

	unknownKeyQueryedFlag = tmpUnknownKeyQueryedFlag;
}

void CConfigFile::setInt32Vector(const std::string & key, const std::vector<int32_t> &vec)
{
	bool tmpUnknownKeyQueryedFlag = unknownKeyQueryedFlag;
	unknownKeyQueryedFlag = false;
	std::string oldVal = getString(key);
	std::stringstream s;

	for (std::vector<int32_t>::const_iterator it = vec.begin(); ; )
	{
		if (it == vec.end())
			break;
		s << (*it);
		++it;
		if (it == vec.end())
			break;
		s << delimiter;
	}
	if (oldVal != s.str() || unknownKeyQueryedFlag)
	{
		modifiedFlag = true;
		configData[key] = s.str();
	}
	unknownKeyQueryedFlag = tmpUnknownKeyQueryedFlag;
}

void CConfigFile::setStringVector(const std::string & key, const std::vector<std::string> &vec)
{
	bool tmpUnknownKeyQueryedFlag = unknownKeyQueryedFlag;
	unknownKeyQueryedFlag = false;
	std::string oldVal = getString(key);
	std::string newVal = "";

	for (std::vector<std::string>::const_iterator it = vec.begin(); ; )
	{
		if (it == vec.end())
			break;
		newVal += *it;
		++it;
		if (it == vec.end())
			break;
		newVal += delimiter;
	}
	if (oldVal != newVal || unknownKeyQueryedFlag)
	{
		modifiedFlag = true;
		configData[key] = newVal;
	}
	unknownKeyQueryedFlag = tmpUnknownKeyQueryedFlag;
}
