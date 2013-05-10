/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	audioMute - Neutrino-GUI
	Copyright (C) 2013 M. Liebmann (micha-bbg)
	CComponents implementation
	Copyright (C) 2013 Thilo Graf

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <global.h>
#include <neutrino.h>
#include <gui/infoclock.h>
#include <gui/volumebar.h>
#include <gui/audiomute.h>

CAudioMute::CAudioMute():CComponentsPicture(0, 0, 0, 0, NEUTRINO_ICON_BUTTON_MUTE)
{
	y_old		= -1;
	CVolumeHelper::getInstance()->refresh();
	CVolumeHelper::getInstance()->getMuteIconDimensions(&x, &y, &width, &height);
}

CAudioMute* CAudioMute::getInstance()
{
	static CAudioMute* Mute = NULL;
	if(!Mute)
		Mute = new CAudioMute();
	return Mute;
}

void CAudioMute::AudioMute(int newValue, bool isEvent)
{
	CNeutrinoApp* neutrino = CNeutrinoApp::getInstance();
	bool doInit = newValue != (int) neutrino->isMuted();

	CVFD::getInstance()->setMuted(newValue);
	neutrino->setCurrentMuted(newValue);
	g_Zapit->muteAudio(newValue);

	if( isEvent && ( neutrino->getMode() != CNeutrinoApp::mode_scart ) && ( neutrino->getMode() != CNeutrinoApp::mode_audio) && ( neutrino->getMode() != CNeutrinoApp::mode_pic))
	{
		CVolumeHelper::getInstance()->getMuteIconDimensions(&x, &y, &width, &height);
		if ((y_old != y)) {
			this->hide();
			y_old = y;
		}
		if ((g_settings.mode_clock) && (doInit))
			CInfoClock::getInstance()->ClearDisplay();

		if (newValue)
			this->paint();
		else
			this->hide();

		if (doInit)
			CVolumeHelper::getInstance()->refresh();
	}
}
