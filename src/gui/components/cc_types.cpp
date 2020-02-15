/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Types and base type class for generic GUI-related components.
	Copyright (C) 2012-2018, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#include "cc_types.h"
#include <system/debug.h>


CCTypes::CCTypes()
{
	cc_item_type.id		= CC_ITEMTYPE_GENERIC;
	cc_item_type.name	= "";
}


//returns current item element type, if no available, return -1 as unknown type
int CCTypes::CCTypes::getItemType()
{
	for(int i =0; i< (CC_ITEMTYPES) ;i++){
		if (i == cc_item_type.id)
			return i;
	}

	dprintf(DEBUG_DEBUG, "[CCTypes] %s: unknown item type requested...\n", __func__);

	return -1;
}

//sets item name
void CCTypes::CCTypes::setItemName(const std::string& name)
{
	cc_item_type.name = name;
}

//gets item name
std::string CCTypes::CCTypes::getItemName()
{
	return cc_item_type.name;
}
