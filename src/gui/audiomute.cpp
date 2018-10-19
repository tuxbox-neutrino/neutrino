/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	audioMute - Neutrino-GUI
	Copyright (C) 2013 M. Liebmann (micha-bbg)
	CComponents implementation
	Copyright (C) 2013-2015 Thilo Graf

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
#include <video.h>
#include <driver/display.h>
#include <gui/infoclock.h>
#include <gui/volumebar.h>
#include <gui/audiomute.h>

#include <driver/display.h>

#include <system/helpers.h>

CAudioMute::CAudioMute():CComponentsPicture(0, 0, NEUTRINO_ICON_MUTED)
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
#if HAVE_ARM_HARDWARE
	if (g_settings.hdmi_cec_volume)
		hdmi_cec::getInstance()->toggle_mute();
	else
#endif
	g_Zapit->muteAudio(newValue);

	if( isEvent && ( neutrino->getMode() != NeutrinoModes::mode_scart ) && ( neutrino->getMode() != NeutrinoModes::mode_pic))
	{
		if (doInit)
			CVolumeHelper::getInstance()->refresh();

		CVolumeHelper::getInstance()->getMuteIconDimensions(&x, &y, &width, &height);
		if ((y_old != y)) {
			if (do_paint_mute_icon)
			{
				frameBuffer->fbNoCheck(true);
				this->hide();
				frameBuffer->fbNoCheck(false);
			}
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1);
			y_old = y;
		}

		/* Infoclock should be blocked in all windows and clean the clock
		 * display with ClearDisplay() by itself before paint,
		 * so we don't do this here.
		*/
		if (!CInfoClock::getInstance()->isBlocked()){
			CInfoClock::getInstance()->ClearDisplay();
		}

		frameBuffer->fbNoCheck(true);
		if (newValue) {
			if (do_paint_mute_icon)
				this->paint();
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1, x, y, width, height);
		}
		else {
			if (!CInfoClock::getInstance()->isBlocked()){
				CInfoClock::getInstance()->ClearDisplay();
				this->kill();
			}
			else
				this->hide();
			clearSavedScreen();
			frameBuffer->setFbArea(CFrameBuffer::FB_PAINTAREA_MUTEICON1);
		}
		frameBuffer->fbNoCheck(false);
	}
}

void CAudioMute::enableMuteIcon(bool enable)
{
	CNeutrinoApp *neutrino = CNeutrinoApp::getInstance();
	CVolumeHelper::getInstance()->getMuteIconDimensions(&x, &y, &width, &height);
	frameBuffer->fbNoCheck(true);
	if (enable) {
		frameBuffer->doPaintMuteIcon(true);
		do_paint_mute_icon = true;
		if (neutrino->isMuted())
			this->paint();
	}
	else {
		if (!neutrino->isMuted())
			this->kill();
		frameBuffer->doPaintMuteIcon(false);
		do_paint_mute_icon = false;
	}
	frameBuffer->fbNoCheck(false);
}
