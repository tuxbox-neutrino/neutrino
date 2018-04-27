/*
	imdb

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

#include <global.h>
#include <driver/screen_max.h>
#include <system/httptool.h>
#include <system/helpers.h>
#include <system/helpers-json.h>
#include <eitd/sectionsd.h>
#include <unistd.h>
#include <json/json.h>

#include "imdb.h"


CIMDB::CIMDB()
{
	search_url	= "http://www.google.de/search?q=";
	search_outfile	= "/tmp/google.out";
	search_error	= "IMDb: Google download failed";
	imdb_url	= "http://www.omdbapi.com/?plot=full&r=json&i=";
	imdb_outfile	= "/tmp/imdb.json";
	omdb_apikey	= "&apikey=";
	omdb_apikey	+= g_settings.imdb_api_key;
	posterfile	= "/tmp/imdb.jpg";

	acc = 0;
}

CIMDB::~CIMDB()
{
	cleanup();
}

CIMDB* CIMDB::getInstance()
{
	static CIMDB* imdb = NULL;
	if(!imdb)
		imdb = new CIMDB();
	return imdb;
}

std::string CIMDB::utf82url(std::string s)
{
	std::stringstream ss;
	for (size_t i = 0; i < s.length(); ++i)
	{
		if (unsigned(s[i]) <= ' ') {
			ss << '+';
		}
		else if (unsigned(s[i]) <= '\x27') {
			ss << "%" << std::hex << unsigned(s[i]);
		}
		else {
			ss << s[i];
		}
	}
	return ss.str();
}

std::string CIMDB::parseString(std::string search1, std::string search2, std::string str)
{
	std::string ret, search;
	size_t pos_wildcard, pos_search1, pos_search2;
	pos_wildcard = pos_search1 = pos_search2 = std::string::npos;

	if((pos_wildcard = search1.find('*')) != std::string::npos)
	{
		search = search1.substr(0, pos_wildcard);
		//std::cout << "wildcard detected" << '\t' << "= " << search << "[*]" << search1.substr(pos_wildcard+1) << std::endl;
	}
	else
		search = search1;

	//std::cout << "search1" << "\t\t\t" << "= " << '"' << search << '"' << std::endl;
	if((pos_search1 = str.find(search)) != std::string::npos)
	{
		//std::cout << "search1 found" << "\t\t" << "= " << '"' << search << '"' << " at pos "<< (int)(pos_search1) << " => " << str << std::endl;

		pos_search1 += search.length();

		if(pos_wildcard != std::string::npos)
		{
			size_t pos_wildcard_ext;
			std::string wildcard_ext = search1.substr(pos_wildcard+1);

			//std::cout << "wildcard_ext" << "\t\t" << "= " << '"' << wildcard_ext << '"' << std::endl;
			if((pos_wildcard_ext = str.find(wildcard_ext,pos_wildcard+1)) != std::string::npos)
			{
				//std::cout << "wildcard_ext found" << "\t" << "= " << '"' << wildcard_ext << '"' << " at pos "<< (int)(pos_wildcard_ext) << " => " << str << std::endl;
				pos_search1 = pos_wildcard_ext + wildcard_ext.length();
			}
			else
			{
				//std::cout << "wildcard_ext not found in line " << acc << " - exit" << std::endl;
				return("");
			}
		}
	}
	else
	{
		//std::cout << "search1 not found in line " << acc << " - exit" << std::endl;
		return("");
	}

	if(pos_search1 != std::string::npos)
	{
		//std::cout << "search2 " << "\t\t" << "= " << '"' << search2 << '"' << std::endl;

		if(search2 == "\n")
		{
			ret = str.substr(pos_search1, str.length() - pos_search1);
			return(ret);
		}

		if((pos_search2 = str.find(search2, pos_search1)) != std::string::npos)
		{
			if(search2.empty())
				pos_search2 = str.length();

			//std::cout << "search2" << "\t\t\t" << "= " << '"' << search2 << '"' << " found at "<< (int)(pos_search2) << " => " << str << std::endl;
			ret = str.substr(pos_search1, pos_search2 - pos_search1);
		}
		//else
			//std::cout << "search2 not found in line " << acc << " - exit" << std::endl;

	}

	return(ret);
}

std::string CIMDB::parseFile(std::string search1, std::string search2, const char* file, std::string firstline, int line_offset)
{

	acc = 0;
	std::ifstream fh;
	std::string str, ret;
	size_t pos_firstline;
	pos_firstline = std::string::npos;

	if(firstline.empty())
		pos_firstline = 0;

	fh.open(file, std::ios::in);
	if(fh.is_open())
	{
		int line = 0;
		while (!fh.eof())
		{
			getline(fh, str);
			acc++;

			if(pos_firstline == std::string::npos)
			{
				if((pos_firstline = str.find(firstline)) != std::string::npos)
				{
					//std::cout << "firstline found " << str << std::endl;
				}
				continue;
			}

			if(line_offset /*&& pos_firstline != std::string::npos*/)
			{
				if(line+1 != line_offset)
				{
					line++;
					continue;
				}
			}

			ret = parseString(search1,search2,str);

			if(!ret.empty())
				break;
		}
		fh.close();
	}

	return(ret);
}

std::string CIMDB::googleIMDb(std::string s)
{
	CHTTPTool httpTool;
	std::string ret = search_error;
	std::string search_string("title+");
	char* search_char = (char*) s.c_str();

	m.clear();
	unlink(search_outfile.c_str());
	unlink(imdb_outfile.c_str());
	unlink(posterfile.c_str());

	while (*search_char != 0)
	{
		if (*search_char == ' ')
		{
			search_string += '+';
		}
		else
		{
			search_string += *search_char;
		}
		search_char++;
	}

	std::string url = search_url + utf82url(search_string) + "%20site:www.imdb.com";

	if (httpTool.downloadFile(url, search_outfile.c_str()))
	{
		ret = parseFile("http://www.imdb.com/title/", ">", search_outfile.c_str());

		if(ret.empty())
			ret = parseFile("http://www.imdb.de/title/", ">", search_outfile.c_str());

		std::string delimiters = "/&;";
		size_t next = ret.find_first_of(delimiters, 0);
		ret = ret.substr(0, next);
	}

	return ret;
}

void CIMDB::initMap( std::map<std::string, std::string>& my )
{
	std::string errMsg = "";
	Json::Value root;

	std::ostringstream ss;
	std::ifstream fh(imdb_outfile.c_str(),std::ifstream::in);
	ss << fh.rdbuf();
	std::string filedata = ss.str();

	bool parsedSuccess = parseJsonFromString(filedata, &root, &errMsg);

	if(!parsedSuccess)
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

int CIMDB::getIMDb(const std::string& epgTitle)
{
	CHTTPTool httpTool;
	int ret = 0;

	std::string imdb_id = googleIMDb(epgTitle);

	if(((imdb_id.find(search_error)) != std::string::npos))
		return ret;

	std::string url = imdb_url + imdb_id + omdb_apikey;

	if (httpTool.downloadFile(url, imdb_outfile.c_str()))
	{
		initMap(m);

		//std::cout << "m now contains " << m.size() << " elements.\n";

		if(m.empty() || m["Response"]!="True")
			return 0;

		//for (std::map<std::string,std::string>::iterator it=m.begin(); it!=m.end(); ++it)
		//	std::cout << it->first << " => " << it->second << '\n';

		//download Poster
		if(m["Poster"] != "N/A")
		{
			// if possible load bigger image
			std::string origURL ("300");
			std::string replURL ("600");

			if (m["Poster"].compare(m["Poster"].size()-7,3,origURL) == 0){
				//std::cout << "########## " << m["Poster"] << " contains " << origURL << '\n';
				m["Poster"].replace(m["Poster"].size()-7,3,replURL);
				//std::cout << "########## New string: " << m["Poster"] << '\n';
			}

			if (httpTool.downloadFile(m["Poster"], posterfile.c_str()))
				return 2;
			else {
				if (access(posterfile.c_str(), F_OK) == 0)
					unlink(posterfile.c_str());
				return 1;
			}
		}
		ret=2;
	}

	return ret;
}

bool CIMDB::checkIMDbElement(std::string element)
{
	if (m[element].empty() || m[element].compare("N/A") == 0)
		return false;
	else
		return true;
}

void CIMDB::getIMDbData(std::string& txt)
{
	if (m["imdbID"].empty() || m["Response"] != "True")
	{
		txt = g_Locale->getText(LOCALE_IMDB_DATA_FAILED);
		return;
	}

	txt += g_Locale->getString(LOCALE_IMDB_DATA_VOTES) + ": " + m["imdbVotes"] + "\n";
	if (checkIMDbElement("Metascore"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_METASCORE) + ": " + m["Metascore"] + "/100\n";
	txt += g_Locale->getString(LOCALE_IMDB_DATA_TITLE) + ": " + m["Title"] + "\n";
	if (checkIMDbElement("Released"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_RELEASED) + ": " + m["Country"] + ", " + m["Released"] + "\n";
	if (checkIMDbElement("Runtime"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_RUNTIME) + ": " + m["Runtime"] + "\n";
	if (checkIMDbElement("Genre"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_GENRE) + ": " + m["Genre"] + "\n";
	if (checkIMDbElement("Awards"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_AWARDS) + ": " + m["Awards"] + "\n";
	if (checkIMDbElement("Director"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_DIRECTOR) + ": " + m["Director"] + "\n";
	if (checkIMDbElement("Writer"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_WRITER) + ": " + m["Writer"] + "\n";
	if (checkIMDbElement("Production"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_PRODUCTION) + ": " + m["Production"] + "\n";
	if (checkIMDbElement("Website"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_WEBSITE) + ": " + m["Website"] + "\n";
	if (checkIMDbElement("BoxOffice"))
		txt += g_Locale->getString(LOCALE_IMDB_DATA_BOXOFFICE) + ": " + m["BoxOffice"] + "\n";
	if (checkIMDbElement("Actors"))
	{
		txt += "\n";
		txt += g_Locale->getString(LOCALE_IMDB_DATA_ACTORS) + ": " + m["Actors"] + "\n";
	}
	if (checkIMDbElement("Plot"))
	{
		txt += "\n";
		txt += g_Locale->getString(LOCALE_IMDB_DATA_PLOT) + ": " + m["Plot"];
	}
}

std::string CIMDB::getFilename(CZapitChannel * channel, uint64_t id)
{
	char		fname[512]; // UTF-8
	char		buf[256];
	unsigned int	pos = 0;

	if(check_dir(g_settings.network_nfs_recordingdir.c_str()))
		return ("");

	snprintf(fname, sizeof(fname), "%s/", g_settings.network_nfs_recordingdir.c_str());
	pos = strlen(fname);

	// %C == channel, %T == title, %I == info1, %d == date, %t == time_t
	std::string FilenameTemplate = g_settings.recording_filename_template;
	if (FilenameTemplate.empty())
		FilenameTemplate = "%C_%T_%d_%t";

	StringReplace(FilenameTemplate,"%d","");
	StringReplace(FilenameTemplate,"%t","");
	StringReplace(FilenameTemplate,"__","_");

	std::string channel_name = channel->getName();
	if (!(channel_name.empty())) {
		strcpy(buf, UTF8_TO_FILESYSTEM_ENCODING(channel_name.c_str()));
		ZapitTools::replace_char(buf);
		StringReplace(FilenameTemplate,"%C",buf);
	}
	else
		StringReplace(FilenameTemplate,"%C","no_channel");

	CShortEPGData epgdata;
	if(CEitManager::getInstance()->getEPGidShort(id, &epgdata)) {
		if (!(epgdata.title.empty())) {
			strcpy(buf, epgdata.title.c_str());
			ZapitTools::replace_char(buf);
			StringReplace(FilenameTemplate,"%T",buf);
		}
		else
			StringReplace(FilenameTemplate,"%T","no_title");

		if (!(epgdata.info1.empty())) {
			strcpy(buf, epgdata.info1.c_str());
			ZapitTools::replace_char(buf);
			StringReplace(FilenameTemplate,"%I",buf);
		}
		else
			StringReplace(FilenameTemplate,"%I","no_info");
	}

	strcpy(&(fname[pos]), UTF8_TO_FILESYSTEM_ENCODING(FilenameTemplate.c_str()));

	pos = strlen(fname);

	strcpy(&(fname[pos]), ".jpg");

	return (fname);
}

void CIMDB::StringReplace(std::string &str, const std::string search, const std::string rstr)
{
	std::string::size_type ptr = 0;
	std::string::size_type pos = 0;
	while((ptr = str.find(search,pos)) != std::string::npos){
		str.replace(ptr,search.length(),rstr);
		pos = ptr + rstr.length();
	}
}

void CIMDB::cleanup()
{
	if (access(search_outfile.c_str(), F_OK) == 0)
		unlink(search_outfile.c_str());
	if (access(posterfile.c_str(), F_OK) == 0)
		unlink(posterfile.c_str());
}

bool CIMDB::gotPoster()
{
    return (access(posterfile.c_str(), F_OK) == 0);
}
