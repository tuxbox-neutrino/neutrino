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

#if HAVE_SH4_HARDWARE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gui/kerneloptions.h>
#else
extern cVideo *videoDecoder;
#endif

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


#if HAVE_SH4_HARDWARE
#define VIDEOMENU_HDMI_CEC_STANDBY_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_CEC_STANDBY_OPTIONS[VIDEOMENU_HDMI_CEC_STANDBY_OPTION_COUNT] =
{
	{ 0	, LOCALE_OPTIONS_OFF				},
	{ 1	, LOCALE_OPTIONS_ON				},
	{ 2	, LOCALE_VIDEOMENU_HDMI_CEC_STANDBY_NOT_TIMER	}
};
#else
#define VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_CEC_MODE_OPTIONS[VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT] =
{
	{ VIDEO_HDMI_CEC_MODE_OFF	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_OFF      },
	{ VIDEO_HDMI_CEC_MODE_TUNER	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_TUNER    },
	{ VIDEO_HDMI_CEC_MODE_RECORDER	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_RECORDER }
};
#endif

int CCECSetup::showMenu()
{
	//menue init
	CMenuWidget *cec = new CMenuWidget(LOCALE_MAINMENU_SETTINGS, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_CEC);
	cec->addIntroItems(LOCALE_VIDEOMENU_HDMI_CEC);

	//cec
#if HAVE_SH4_HARDWARE
	CKernelOptions ko;
	g_settings.hdmi_cec_mode = ko.isEnabled("cec");
	CMenuOptionChooser *cec_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_MODE, &g_settings.hdmi_cec_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	cec1 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_STANDBY, &g_settings.hdmi_cec_standby, VIDEOMENU_HDMI_CEC_STANDBY_OPTIONS, VIDEOMENU_HDMI_CEC_STANDBY_OPTION_COUNT, g_settings.hdmi_cec_mode != 0, this);
	cec2 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_BROADCAST, &g_settings.hdmi_cec_broadcast, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
#else
	CMenuOptionChooser *cec_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_MODE, &g_settings.hdmi_cec_mode, VIDEOMENU_HDMI_CEC_MODE_OPTIONS, VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT, true, this);
	cec_ch->setHint("", LOCALE_MENU_HINT_CEC_MODE);
	cec1 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_VIEW_ON, &g_settings.hdmi_cec_view_on, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	cec1->setHint("", LOCALE_MENU_HINT_CEC_VIEW_ON);
	cec2 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_STANDBY, &g_settings.hdmi_cec_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	cec2->setHint("", LOCALE_MENU_HINT_CEC_STANDBY);
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	cec3 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_VOLUME, &g_settings.hdmi_cec_volume, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	cec3->setHint("", LOCALE_MENU_HINT_CEC_VOLUME);
#endif
#endif

	cec->addItem(cec_ch);
#if !HAVE_SH4_HARDWARE
	cec->addItem(GenericMenuSeparatorLine);
#endif
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

#if HAVE_SH4_HARDWARE
void CCECSetup::setCECSettings(bool b)
{	
	printf("[neutrino CEC Settings] %s init CEC settings...\n", __FUNCTION__);
	if (b) {
		// wakeup
		if (g_settings.hdmi_cec_mode &&
			 ((g_settings.hdmi_cec_standby == 1) ||
			 (g_settings.hdmi_cec_standby == 2 && !CNeutrinoApp::getInstance()->timer_wakeup))) {
			int otp = ::open("/proc/stb/cec/onetouchplay", O_WRONLY);
			if (otp > -1) {
				write(otp, g_settings.hdmi_cec_broadcast ? "f\n" : "0\n", 2);
				close(otp);
			}
		}
	} else {
		if (g_settings.hdmi_cec_mode && g_settings.hdmi_cec_standby) {
			int otp = ::open("/proc/stb/cec/systemstandby", O_WRONLY);
			if (otp > -1) {
				write(otp, g_settings.hdmi_cec_broadcast ? "f\n" : "0\n", 2);
				close(otp);
			}
		}
	}
}
#else
void CCECSetup::setCECSettings()
{
	printf("[neutrino CEC Settings] %s init CEC settings...\n", __FUNCTION__);
	videoDecoder->SetCECAutoStandby(g_settings.hdmi_cec_standby == 1);
	videoDecoder->SetCECAutoView(g_settings.hdmi_cec_view_on == 1);
	videoDecoder->SetCECMode((VIDEO_HDMI_CEC_MODE)g_settings.hdmi_cec_mode);
}
#endif

bool CCECSetup::changeNotify(const neutrino_locale_t OptionName, void * /*data*/)
{
#if HAVE_SH4_HARDWARE
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC_MODE))
	{
		printf("[neutrino CEC Settings] %s set CEC settings...\n", __FUNCTION__);
		cec1->setActive(g_settings.hdmi_cec_mode != 0);
		cec2->setActive(g_settings.hdmi_cec_mode != 0);
		CKernelOptions ko;
		ko.Enable("cec", g_settings.hdmi_cec_mode != 0);
		g_settings.hdmi_cec_mode = ko.isEnabled("cec");
	}
#else

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
#endif

	return false;
}

