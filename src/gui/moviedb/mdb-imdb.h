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

#ifndef __imdb__
#define __imdb__

#include <system/helpers.h>
#include <zapit/zapit.h>
#include <gui/widget/hintbox.h>

class CIMDB
{
	private:
		CHintBox *hintbox;

		std::string searchIMDbId(const std::string &title);
		std::string wikidataIMDbId(const std::string &title);
		bool fetchDetails(const std::string &url);
		std::map<std::string, std::string> m;

		std::string posterfile;

		void initMap(std::map<std::string, std::string> &my);

	public:
		CIMDB();
		~CIMDB();
		static CIMDB *getInstance();
		void setTitle(std::string epgtitle);
		std::string getEPGText();
		std::string getMovieText();

		std::string search_outfile;
		std::string search_error;
		std::string imdb_outfile;

		int getMovieDetails(const std::string &epgTitle);
		void cleanup();

		std::string getPoster() { return posterfile; }
		bool hasPoster() { return (access(posterfile.c_str(), F_OK) == 0); }
		int getStars();

		bool checkElement(std::string element);
		//FIXME: what if m[element] doesn't exist?
		std::string getIMDbElement(std::string element) { return m[element]; };
};

#endif
