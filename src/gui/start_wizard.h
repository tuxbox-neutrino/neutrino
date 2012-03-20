/*
	Neutrino-GUI  -  Tuxbox-Project
	Copyright (C) 2001 Steffen Hehn 'McClean'
	http://www.tuxbox.org
		
	Startup wizard
	based upon an implementation created by 
	Copyright (C) 2009 CoolStream International Ltd.
      
	Reworked by dbt (Thilo Graf)
	Copyright (C) 2012 dbt
	http://www.dbox2-tuning.net

	License: GPL

	This software is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
	
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
*/

#ifndef __start_wizard__
#define __start_wizard__

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>


class CStartUpWizard : public CMenuTarget
{
	private:		
		void showBackgroundLogo();
		void killBackgroundLogo();
	public:			
		CStartUpWizard();
		~CStartUpWizard();
				
		int 	exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
