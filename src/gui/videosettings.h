/*
	$port: video_setup.cpp,v 1.6 2010/08/28 23:06:59 tuxbox-cvs Exp $

	video setup implementation - Neutrino-GUI

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
#ifndef __video_setup__
#define __video_setup__

#include <gui/widget/menue.h>
#include <string>

class CFrameBuffer;
class CVideoSettings : public CMenuWidget, CChangeObserver
{
	private:
		CFrameBuffer 		*frameBuffer;
		CMenuForwarder 		*SyncControlerForwarder;
		CMenuOptionChooser 	*VcrVideoOutSignalOptionChooser;

		int			prev_video_mode;

		int is_wizard;

		int width, selected;
		int showVideoSetup();
		std::vector<CMenuOptionChooser::keyval_ext> videomenu_43mode_options;
		void Init43ModeOptions();

public:
		CVideoSettings(int wizard_mode = SNeutrinoSettings::WIZARD_OFF);
		~CVideoSettings();

		virtual bool changeNotify(const neutrino_locale_t OptionName, void *data);
		//virtual void paint();
		void nextMode();
		void next43Mode();
		void SwitchFormat();

		void setVideoSettings();
		void setupVideoSystem(bool do_ask);

		void setWizardMode(int mode) {is_wizard = mode;};

		int exec(CMenuTarget* parent, const std::string & actionKey);
};
#endif

