/*
	Neutrino-GUI  -   DBoxII-Project


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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __audio_selector__
#define __audio_selector__

//
// -- Audio Channel Selector Menue
// -- 2005-08-31 rasc
//

#include "widget/menue.h"



using namespace std;

class CAudioSelectMenuHandler : public CMenuTarget
{
	private:
		int width;
		
	public:
		CAudioSelectMenuHandler();
		~CAudioSelectMenuHandler();
		
		int  exec( CMenuTarget* parent,  const std::string &actionkey);
		int  doMenu();

};


#endif

