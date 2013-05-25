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

#include <driver/display.h>

CAudioMute::CAudioMute():CComponentsPicture(0, 0, 0, 0, NEUTRINO_ICON_BUTTON_MUTE)
{
	y_old			= -1;
	paint_bg		= false;
	do_paint_mute_icon	= true;
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
			if (do_paint_mute_icon)
			{
				frameBuffer->fbNoCheck(true);
				this->hide(true);
				frameBuffer->fbNoCheck(false);
			}
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1);
			y_old = y;
		}
		if ((g_settings.mode_clock) && (doInit))
			CInfoClock::getInstance()->ClearDisplay();

		frameBuffer->fbNoCheck(true);
		if (newValue) {
			if (do_paint_mute_icon)
				this->paint();
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1, x, y, width, height);
		}
		else {
			if (do_paint_mute_icon)
				this->hide(true);
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1);
		}
		frameBuffer->fbNoCheck(false);
		frameBuffer->blit();

		if (doInit)
			CVolumeHelper::getInstance()->refresh();
	}
	else if (neutrino->getMode() == CNeutrinoApp::mode_audio) {
		if (newValue)
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1, x, y, width, height);
		else
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1);
	}
}

void CAudioMute::enableMuteIcon(bool enable)
{
	CNeutrinoApp *neutrino = CNeutrinoApp::getInstance();
	frameBuffer->fbNoCheck(true);
	if (enable) {
		frameBuffer->doPaintMuteIcon(true);
		do_paint_mute_icon = true;
		if (neutrino->isMuted())
			this->paint();
	}
	else {
		if (neutrino->isMuted())
			this->hide(true);
		frameBuffer->doPaintMuteIcon(false);
		do_paint_mute_icon = false;
	}
	frameBuffer->fbNoCheck(false);
	frameBuffer->blit();
}
