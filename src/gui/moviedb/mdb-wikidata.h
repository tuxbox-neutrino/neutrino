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

#ifndef __mdb_wikidata__
#define __mdb_wikidata__

#include <string>
#include <vector>

/*
   OMDb knows its titles mostly in English, so a German EPG title such as
   "SpongeBob Schwammkopf" finds nothing there. Wikidata keeps the titles of a
   work in many languages beside its IMDb ID (property P345), and answers
   without a key. It is asked for the IMDb ID only; the details still come
   from OMDb.

   Wikimedia refuses generic User-Agents: whoever asks must send one that
   names the program and a way to reach its makers.
*/
namespace wikidata
{
	// The title with what the search would read as syntax taken out:
	// quotes, wildcards, a minus or bang before a word, colons.
	std::string searchTerm(const std::string &title);

	// A search for works with an IMDb ID, people and episodes left out.
	std::string searchUrl(const std::string &title, unsigned int limit = 3);
	// The items the search found ("Q" and digits), best first.
	std::vector<std::string> pickItems(const std::string &searchAnswer);

	// The IMDb ID statements of one item.
	std::string claimsUrl(const std::string &item);
	// The title ID ("tt...") of those statements: a preferred one before a
	// normal one, a deprecated one never; empty if there is none.
	std::string pickImdbId(const std::string &claimsAnswer);
}

#endif
