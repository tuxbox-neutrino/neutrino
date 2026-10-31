/*
	Movie Database - OMDb/IMDb

	(C) 2009-2016 NG-Team
	(C) 2016 NI-Team

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>
#include <iostream>

#include <unistd.h>

#include <global.h>
#include <driver/screen_max.h>
#include <system/httptool.h>
#include <system/helpers.h>
#include <system/helpers-json.h>
#include <eitd/sectionsd.h>
#include <json/json.h>

#include "mdb-imdb.h"
#include "mdb-omdb.h"

CIMDB *CIMDB::getInstance()
{
	static CIMDB *imdb = NULL;
	if (!imdb)
		imdb = new CIMDB();
	return imdb;
}

CIMDB::CIMDB()
{
	hintbox = NULL;
	search_outfile	= "/tmp/omdb-search.json";
	search_error	= "IMDb: OMDb search failed";
	imdb_outfile	= "/tmp/imdb.json";
	posterfile	= "/tmp/imdb.jpg";
}

CIMDB::~CIMDB()
{
	cleanup();
}

void CIMDB::setTitle(std::string epgtitle)
{
	hintbox = new CHintBox(LOCALE_MESSAGEBOX_INFO, LOCALE_OMDB_READ_DATA);
	hintbox->paint();

	getMovieDetails(epgtitle);

	if (hintbox)
	{
		hintbox->hide();
		delete hintbox;
		hintbox = NULL;
	}
}

std::string CIMDB::searchIMDbId(const std::string &title)
{
	CHTTPTool httpTool;
	std::string ret;

	if (!httpTool.downloadFile(omdb::searchUrl(g_settings.omdb_api_key, title), search_outfile.c_str()))
		return ret;

	std::ostringstream ss;
	std::ifstream fh(search_outfile.c_str(), std::ifstream::in);
	ss << fh.rdbuf();
	ret = omdb::pickId(ss.str());

	return ret;
}

bool CIMDB::fetchDetails(const std::string &url)
{
	CHTTPTool httpTool;

	m.clear();
	unlink(imdb_outfile.c_str());

	if (!httpTool.downloadFile(url, imdb_outfile.c_str()))
		return false;

	initMap(m);

	return m["Response"] == "True" && omdb::isImdbId(m["imdbID"]);
}

void CIMDB::initMap(std::map<std::string, std::string> &my)
{
	std::string errMsg = "";
	Json::Value root;

	std::ostringstream ss;
	std::ifstream fh(imdb_outfile.c_str(), std::ifstream::in);
	ss << fh.rdbuf();
	std::string filedata = ss.str();

	bool parsedSuccess = parseJsonFromString(filedata, &root, &errMsg);

	if (!parsedSuccess)
	{
		std::cout << "Failed to parse JSON\n";
		std::cout << errMsg << std::endl;

		my["Response"] = "False"; // we fake a false response
		return;
	}

	/*
	   we grab only what we need to avoid bad surprises
	   when api is changed
	*/

	my["Actors"]		= root.get("Actors", "").asString();
	my["Awards"]		= root.get("Awards", "").asString();
	my["BoxOffice"]		= root.get("BoxOffice", "").asString();
	my["Country"]		= root.get("Country", "").asString();
	my["Director"]		= root.get("Director", "").asString();
	my["Genre"]		= root.get("Genre", "").asString();
	my["imdbID"]		= root.get("imdbID", "").asString();
	my["imdbRating"]	= root.get("imdbRating", "N/A").asString();
	my["imdbVotes"]		= root.get("imdbVotes", "").asString();
	my["Metascore"]		= root.get("Metascore", "N/A").asString();
	my["Plot"]		= root.get("Plot", "").asString();
	my["Poster"]		= root.get("Poster", "N/A").asString();
	my["Production"]	= root.get("Production", "").asString();
	my["Released"]		= root.get("Released", "").asString();
	my["Response"]		= root.get("Response", "False").asString();
	my["Runtime"]		= root.get("Runtime", "").asString();
	my["Title"]		= root.get("Title", "").asString();
	my["Website"]		= root.get("Website", "").asString();
	my["Writer"]		= root.get("Writer", "").asString();
	my["Year"]		= root.get("Year", "").asString();

	// currently unused
	//my["Rated"]		= root.get("Rated", "").asString();
	//my["Type"]		= root.get("Type", "").asString();
}

int CIMDB::getMovieDetails(const std::string &epgTitle)
{
	CHTTPTool httpTool;

	m.clear();
	unlink(search_outfile.c_str());
	unlink(imdb_outfile.c_str());
	unlink(posterfile.c_str());

	std::string title = omdb::searchTerm(epgTitle);
	if (title.empty())
		return 0;

	// The key is read here and not kept, so a key entered after the first lookup is the one used.
	// The best match for the title first; if OMDb knows no film of exactly
	// that name, the first hit of its search.
	if (!fetchDetails(omdb::titleUrl(g_settings.omdb_api_key, title)))
	{
		std::string imdb_id = searchIMDbId(title);
		if (imdb_id.empty() || !fetchDetails(omdb::idUrl(g_settings.omdb_api_key, imdb_id)))
		{
			m.clear();
			m["Response"] = "False";
			return 0;
		}
	}

	//download Poster
	if (m["Poster"] != "N/A")
	{
		// if possible load bigger image
		std::string origURL("300");
		std::string replURL("600");

		if (m["Poster"].size() >= 7 && m["Poster"].compare(m["Poster"].size() - 7, 3, origURL) == 0)
			m["Poster"].replace(m["Poster"].size() - 7, 3, replURL);

		if (httpTool.downloadFile(m["Poster"], posterfile.c_str()))
			return 2;

		if (access(posterfile.c_str(), F_OK) == 0)
			unlink(posterfile.c_str());
		return 1;
	}

	return 2;
}

bool CIMDB::checkElement(std::string element)
{
	if (m[element].empty() || m[element].compare("N/A") == 0)
		return false;
	else
		return true;
}

std::string CIMDB::getEPGText()
{
	if (m["imdbID"].empty() || m["Response"] != "True")
		return g_Locale->getText(LOCALE_IMDB_DATA_FAILED);

	std::string epgtext("");

	if (checkElement("imdbRating"))
	{
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_RATING) + ": " + m["imdbRating"] + "/10; ";
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_VOTES) + ": " + m["imdbVotes"] + "\n";
	}
	if (checkElement("Metascore"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_METASCORE) + ": " + m["Metascore"] + "/100\n";
	if (checkElement("Title"))
	{
		epgtext += "\n";
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_TITLE) + ": " + m["Title"] + "\n";
	}
	if (checkElement("Released"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_RELEASED) + ": " + m["Country"] + ", " + m["Released"] + "\n";
	if (checkElement("Runtime"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_RUNTIME) + ": " + m["Runtime"] + "\n";
	if (checkElement("Genre"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_GENRE) + ": " + m["Genre"] + "\n";
	if (checkElement("Awards"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_AWARDS) + ": " + m["Awards"] + "\n";
	if (checkElement("Director"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_DIRECTOR) + ": " + m["Director"] + "\n";
	if (checkElement("Writer"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_WRITER) + ": " + m["Writer"] + "\n";
	if (checkElement("Production"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_PRODUCTION) + ": " + m["Production"] + "\n";
	if (checkElement("Website"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_WEBSITE) + ": " + m["Website"] + "\n";
	if (checkElement("BoxOffice"))
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_BOXOFFICE) + ": " + m["BoxOffice"] + "\n";
	if (checkElement("Actors"))
	{
		epgtext += "\n";
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_ACTORS) + ": " + m["Actors"] + "\n";
	}
	if (checkElement("Plot"))
	{
		epgtext += "\n";
		epgtext += g_Locale->getString(LOCALE_IMDB_DATA_PLOT) + ": " + m["Plot"];
	}

	return epgtext;
}

std::string CIMDB::getMovieText()
{
	std::string movietext("");

	if (checkElement("Plot"))
	{
		movietext = m["Plot"] + "\n";
		if (checkElement("Title"))
		{
			movietext += "\n";
			movietext += g_Locale->getString(LOCALE_IMDB_DATA_TITLE) + ": ";
			movietext += m["Title"];
		}
		if (checkElement("Country"))
		{
			movietext += "\n";
			movietext += g_Locale->getString(LOCALE_IMDB_DATA_RELEASED) + ": ";
			movietext += m["Country"];
			if (checkElement("Released"))
				movietext += ", " + m["Released"];
		}
		if (checkElement("Actors"))
		{
			movietext += "\n";
			movietext += g_Locale->getString(LOCALE_IMDB_DATA_ACTORS) + ": ";
			movietext += m["Actors"];
		}
	}

	return movietext;
}

int CIMDB::getStars()
{
	std::string imdbRating = "";

	if (checkElement("imdbRating"))
		imdbRating = getIMDbElement("imdbRating");

	if (imdbRating.empty() || imdbRating == "N/A")
		return 0;

	return (int)(atof(imdbRating.c_str()) * 10);
}

void CIMDB::cleanup()
{
	if (access(search_outfile.c_str(), F_OK) == 0)
		unlink(search_outfile.c_str());
	if (access(posterfile.c_str(), F_OK) == 0)
		unlink(posterfile.c_str());
}
