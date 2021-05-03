/*
	Movie Database - Tools

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

#ifndef __mdb_tools__
#define __mdb_tools__

class CMDBTools
{
	private:

	public:
		CMDBTools();
		~CMDBTools();
		static CMDBTools *getInstance();

		std::string getFilename(CZapitChannel *channel, uint64_t id);
};

#endif
