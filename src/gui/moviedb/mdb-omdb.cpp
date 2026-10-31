/*
	Movie Database - OMDb search

	(C) 2026 NI-Team

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.

*/

#include <json/json.h>

#include "mdb-omdb.h"

namespace omdb
{

static const char *base_url = "http://www.omdbapi.com/?r=json&apikey=";

// A member that is not a string reads as empty rather than stopping the box.
static std::string member(const Json::Value &v, const char *name)
{
	const Json::Value &m = v[name];
	return m.isString() ? m.asString() : "";
}

std::string encode(const std::string &s)
{
	static const char hex[] = "0123456789ABCDEF";
	std::string ret;
	for (size_t i = 0; i < s.length(); ++i)
	{
		unsigned char c = (unsigned char) s[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
			|| c == '-' || c == '.' || c == '_' || c == '~')
		{
			ret += (char) c;
		}
		else
		{
			ret += '%';
			ret += hex[c >> 4];
			ret += hex[c & 0x0f];
		}
	}
	return ret;
}

std::string searchTerm(const std::string &title)
{
	std::string ret;
	bool blank = false;
	for (size_t i = 0; i < title.length(); ++i)
	{
		char c = title[i];
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
		{
			blank = true;
			continue;
		}
		if (blank && !ret.empty())
			ret += ' ';
		blank = false;
		ret += c;
	}
	return ret;
}

bool isImdbId(const std::string &id)
{
	if (id.length() < 3 || id.compare(0, 2, "tt") != 0)
		return false;
	for (size_t i = 2; i < id.length(); ++i)
		if (id[i] < '0' || id[i] > '9')
			return false;
	return true;
}

std::string titleUrl(const std::string &apikey, const std::string &title)
{
	return base_url + encode(apikey) + "&plot=full&t=" + encode(searchTerm(title));
}

std::string searchUrl(const std::string &apikey, const std::string &title)
{
	return base_url + encode(apikey) + "&s=" + encode(searchTerm(title));
}

std::string idUrl(const std::string &apikey, const std::string &id)
{
	return base_url + encode(apikey) + "&plot=full&i=" + encode(id);
}

std::string pickId(const std::string &searchAnswer)
{
	Json::CharReaderBuilder builder;
	Json::CharReader *reader = builder.newCharReader();
	Json::Value root;
	JSONCPP_STRING errs;
	const char *begin = searchAnswer.c_str();
	bool parsed = reader->parse(begin, begin + searchAnswer.length(), &root, &errs);
	delete reader;

	if (!parsed || !root.isObject())
		return "";
	if (member(root, "Response") != "True")
		return "";

	const Json::Value &list = root["Search"];
	if (!list.isArray())
		return "";

	std::string first;
	for (Json::ArrayIndex i = 0; i < list.size(); ++i)
	{
		if (!list[i].isObject())
			continue;
		std::string id = member(list[i], "imdbID");
		if (!isImdbId(id))
			continue;
		std::string type = member(list[i], "Type");
		if (type == "movie" || type == "series")
			return id;
		if (first.empty())
			first = id;
	}
	return first;
}

}
