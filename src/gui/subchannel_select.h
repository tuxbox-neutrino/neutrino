/*
	Neutrino-GUI  -   DBoxII-Project
	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/
			
	Rework:
	Outsourced subchannel select modul for Neutrino-HD
	Copyright (C) 2011 T.Graf 'dbt'
	http://www.dbox2-tuning.net

	License: GPL

	This program is free software; you can redistribute it and/or modify it under the terms of the GNU
	General Public License as published by the Free Software Foundation; either version 2 of the License, 
	or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with this program; 
	if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA
		
		
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
*/


#ifndef __subchannel_selector__
#define __subchannel_selector__


#include "widget/menue.h"



class CSubChannelSelectMenu
{
	private: 
		CNVODChangeExec      NVODChanger;

	public:
		CSubChannelSelectMenu();
		~CSubChannelSelectMenu();
		
		int  getNVODMenu(CMenuWidget* menu);

};


#endif

