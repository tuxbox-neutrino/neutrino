/*
	Movie Database - IMDb ID from Wikidata

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

#include <cstdio>

#include <json/json.h>

#include "mdb-omdb.h"
#include "mdb-wikidata.h"

namespace wikidata
{

static const char *api_url = "https://www.wikidata.org/w/api.php?format=json";

static bool parse(const std::string &answer, Json::Value &root)
{
	Json::CharReaderBuilder builder;
	Json::CharReader *reader = builder.newCharReader();
	JSONCPP_STRING errs;
	const char *begin = answer.c_str();
	bool parsed = reader->parse(begin, begin + answer.length(), &root, &errs);
	delete reader;
	return parsed && root.isObject();
}

// A member that is not a string reads as empty rather than stopping the box.
static std::string member(const Json::Value &v, const char *name)
{
	if (!v.isObject())
		return "";
	const Json::Value &m = v[name];
	return m.isString() ? m.asString() : "";
}

static bool isItem(const std::string &id)
{
	if (id.length() < 2 || id[0] != 'Q')
		return false;
	for (size_t i = 1; i < id.length(); ++i)
		if (id[i] < '0' || id[i] > '9')
			return false;
	return true;
}

std::string searchTerm(const std::string &title)
{
	std::string s = omdb::searchTerm(title);
	std::string ret;
	for (size_t i = 0; i < s.length(); ++i)
	{
		char c = s[i];
		bool word_start = (i == 0 || s[i - 1] == ' ');
		if (c == '"' || c == '*' || c == '?' || c == '~' || c == '\\' || c == ':'
			|| ((c == '-' || c == '!') && word_start))
			c = ' ';
		ret += c;
	}
	return omdb::searchTerm(ret);
}

std::string searchUrl(const std::string &title, unsigned int limit)
{
	char lim[16];
	snprintf(lim, sizeof(lim), "%u", limit);

	// P31=Q5: a human, who has an IMDb ID of his own ("nm...").
	// P31=Q21191270: a single episode of a series.
	std::string query = searchTerm(title)
		+ " haswbstatement:P345 -haswbstatement:P31=Q5 -haswbstatement:P31=Q21191270";

	return std::string(api_url) + "&action=query&list=search&srnamespace=0&srprop="
		+ "&srlimit=" + lim + "&srsearch=" + omdb::encode(query);
}

std::vector<std::string> pickItems(const std::string &searchAnswer)
{
	std::vector<std::string> ret;
	Json::Value root;
	if (!parse(searchAnswer, root))
		return ret;

	const Json::Value &query = root["query"];
	if (!query.isObject())
		return ret;
	const Json::Value &list = query["search"];
	if (!list.isArray())
		return ret;

	for (Json::ArrayIndex i = 0; i < list.size(); ++i)
	{
		std::string item = member(list[i], "title");
		if (isItem(item))
			ret.push_back(item);
	}
	return ret;
}

std::string claimsUrl(const std::string &item)
{
	return std::string(api_url) + "&action=wbgetclaims&property=P345&entity=" + omdb::encode(item);
}

std::string pickImdbId(const std::string &claimsAnswer)
{
	Json::Value root;
	if (!parse(claimsAnswer, root))
		return "";

	const Json::Value &claims = root["claims"];
	if (!claims.isObject())
		return "";
	const Json::Value &list = claims["P345"];
	if (!list.isArray())
		return "";

	std::string normal;
	for (Json::ArrayIndex i = 0; i < list.size(); ++i)
	{
		const Json::Value &claim = list[i];
		if (!claim.isObject())
			continue;
		std::string rank = member(claim, "rank");
		if (rank != "preferred" && rank != "normal")
			continue;
		const Json::Value &snak = claim["mainsnak"];
		if (!snak.isObject() || member(snak, "snaktype") != "value")
			continue;
		std::string id = member(snak["datavalue"], "value");
		if (!omdb::isImdbId(id))
			continue;
		if (rank == "preferred")
			return id;
		if (normal.empty())
			normal = id;
	}
	return normal;
}

}
