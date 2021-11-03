/*
	cec settings menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "cec_setup.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>
#include <neutrino_menue.h>

#include <gui/widget/icons.h>

#include <driver/screen_max.h>

#include <system/debug.h>

#include <cs_api.h>
#include <hardware/video.h>

extern cVideo *videoDecoder;

CCECSetup::CCECSetup()
{
	width = 40;
	cec1 = NULL;
	cec2 = NULL;
	cec3 = NULL;
}

CCECSetup::~CCECSetup()
{

}

int CCECSetup::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	printf("[neutrino] init cec setup...\n");
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	res = showMenu();

	return res;
}

#define VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_CEC_MODE_OPTIONS[VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT] =
{
	{ VIDEO_HDMI_CEC_MODE_OFF	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_OFF      },
	{ VIDEO_HDMI_CEC_MODE_TUNER	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_TUNER    },
	{ VIDEO_HDMI_CEC_MODE_RECORDER	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_RECORDER }
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#define VIDEOMENU_HDMI_CEC_VOL_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_CEC_VOL_OPTIONS[VIDEOMENU_HDMI_CEC_VOL_OPTION_COUNT] =
{
	{ VIDEO_HDMI_CEC_VOL_OFF		, LOCALE_VIDEOMENU_HDMI_CEC_VOL_OFF },
	{ VIDEO_HDMI_CEC_VOL_AUDIOSYSTEM, LOCALE_VIDEOMENU_HDMI_CEC_VOL_AUDIOSYSTEM },
	{ VIDEO_HDMI_CEC_VOL_TV			, LOCALE_VIDEOMENU_HDMI_CEC_VOL_TV }
};
#endif

int CCECSetup::showMenu()
{
	//menue init
	CMenuWidget *cec = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_CEC);
	cec->addIntroItems(LOCALE_VIDEOMENU_HDMI_CEC);

	//cec
	CMenuOptionChooser *cec_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_MODE, &g_settings.hdmi_cec_mode, VIDEOMENU_HDMI_CEC_MODE_OPTIONS, VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT, true, this);
	cec_ch->setHint("", LOCALE_MENU_HINT_CEC_MODE);
	cec1 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_VIEW_ON, &g_settings.hdmi_cec_view_on, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	cec1->setHint("", LOCALE_MENU_HINT_CEC_VIEW_ON);
	cec2 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_STANDBY, &g_settings.hdmi_cec_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	cec2->setHint("", LOCALE_MENU_HINT_CEC_STANDBY);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	cec3 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_VOLUME, &g_settings.hdmi_cec_volume, VIDEOMENU_HDMI_CEC_VOL_OPTIONS, VIDEOMENU_HDMI_CEC_VOL_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	cec3->setHint("", LOCALE_MENU_HINT_CEC_VOLUME);
#endif

	cec->addItem(cec_ch);
	cec->addItem(GenericMenuSeparatorLine);
	//-------------------------------------------------------
	cec->addItem(cec1);
	cec->addItem(cec2);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	cec->addItem(cec3);
#endif

	int res = cec->exec(NULL, "");
	delete cec;

	return res;
}

void CCECSetup::setCECSettings()
{
	printf("[neutrino CEC Settings] %s init CEC settings...\n", __FUNCTION__);
	videoDecoder->SetCECAutoStandby(g_settings.hdmi_cec_standby == 1);
	videoDecoder->SetCECAutoView(g_settings.hdmi_cec_view_on == 1);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	videoDecoder->SetAudioDestination(g_settings.hdmi_cec_volume);
#endif
	videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE)g_settings.hdmi_cec_mode);
}

bool CCECSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC_MODE))
	{
		printf("[neutrino CEC Settings] %s set CEC settings...\n", __FUNCTION__);
		cec1->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
		cec2->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		cec3->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
#endif
		videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE)g_settings.hdmi_cec_mode);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC_STANDBY))
	{
		videoDecoder->SetCECAutoStandby(g_settings.hdmi_cec_standby == 1);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC_VIEW_ON))
	{
		videoDecoder->SetCECAutoView(g_settings.hdmi_cec_view_on == 1);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC_VOLUME))
	{
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
		if (g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF)
		{
			videoDecoder->SetAudioDestination(g_settings.hdmi_cec_volume);
		}
#endif
	}

	return false;
}

