/*
	$port: audio_setup.h,v 1.3 2010/12/05 22:32:12 tuxbox-cvs Exp $

	audio setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

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

#ifndef __audio_setup__
#define __audio_setup__

#include <gui/widget/menue.h>

#include <string>

class CAudioSetup : public CMenuTarget
{
	private:
		int width, selected;
		bool is_wizard;
		
		int showAudioSetup();

	public:	
		enum AUDIO_SETUP_MODE
		{
			AUDIO_SETUP_MODE_WIZARD_NO   = 0,
			AUDIO_SETUP_MODE_WIZARD   = 1
		};
		
		CAudioSetup(bool wizard_mode = AUDIO_SETUP_MODE_WIZARD_NO);
		~CAudioSetup();
		
		bool getWizardMode() {return is_wizard;};
		void setWizardMode(bool mode);
		
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CTruVolumeNotifier : public CChangeObserver
{
	private:
		CMenuOptionChooser* toDisable_oj[2];
		CMenuOptionNumberChooser* toDisable_nj;
		
	public:
		CTruVolumeNotifier( CMenuOptionChooser* o1, CMenuOptionChooser* o2, CMenuOptionNumberChooser *n1);
		bool changeNotify(const neutrino_locale_t, void * data);
};


#endif
