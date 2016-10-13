/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2015, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

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

#ifndef __CC_BASE__
#define __CC_BASE__

#include "cc_types.h"
#include "cc_draw.h"

/// Basic component class.
/*!
Basic attributes and member functions for component sub classes
*/

class CComponents : public CCDraw
{
	protected:
	
		///property: tag for component, can contains any value if required, default value is NULL, you can fill with a cast, see also setTag() and getTag()
		void *cc_tag;

	public:
		///basic component class constructor.
		CComponents();
		virtual~CComponents(){};

		///sets tag as void*, see also cc_tag
		virtual void setTag(void* tag){cc_tag = tag;};
		///gets tag as void*, see also cc_tag
		virtual void* getTag(){return cc_tag;};
};

#endif
