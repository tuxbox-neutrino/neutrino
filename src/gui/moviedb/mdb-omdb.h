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

#ifndef __mdb_omdb__
#define __mdb_omdb__

#include <string>

/*
   What the IMDb lookup asks OMDb, kept apart from the screen so it can be
   tested without one. The lookup used to find the IMDb ID by scraping a
   Google result page; Google has answered clients without JavaScript with a
   page that holds no results since January 2025, so the ID was always empty.
   OMDb finds it by itself: first by title, then by its search.
*/
namespace omdb
{
	// Percent-encodes everything but the unreserved characters of RFC 3986.
	std::string encode(const std::string &s);

	// The title with the blanks at either end removed and inner runs of
	// blanks folded to one.
	std::string searchTerm(const std::string &title);

	// An IMDb ID: "tt" followed by digits and nothing else.
	bool isImdbId(const std::string &id);

	// The details of the best match for a title.
	std::string titleUrl(const std::string &apikey, const std::string &title);
	// A list of matches for a title.
	std::string searchUrl(const std::string &apikey, const std::string &title);
	// The details of one IMDb ID.
	std::string idUrl(const std::string &apikey, const std::string &id);

	// The IMDb ID to look up from the answer to searchUrl(): the first
	// movie or series, else the first entry at all, empty if there is none.
	std::string pickId(const std::string &searchAnswer);
}

#endif
