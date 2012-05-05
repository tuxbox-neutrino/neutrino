/*
	$port: video_setup.h,v 1.4 2009/11/22 15:36:52 tuxbox-cvs Exp $

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "videosettings.h"

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include "gui/widget/hintbox.h"
#include "gui/widget/messagebox.h"

#include <driver/screen_max.h>

#include <daemonc/remotecontrol.h>

#include <system/debug.h>

#include <cs_api.h>
#include <video.h>

extern cVideo *videoDecoder;
extern int prev_video_mode;
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

CVideoSettings::CVideoSettings(bool wizard_mode)
{
	frameBuffer = CFrameBuffer::getInstance();
	
	is_wizard = wizard_mode;
	
	SyncControlerForwarder = NULL;
	VcrVideoOutSignalOptionChooser = NULL;

	width = w_max (35, 20);
	selected = -1;
	
	prev_video_mode = g_settings.video_Mode;
	
	setupVideoSystem(false);
}

CVideoSettings::~CVideoSettings()
{

}

int CVideoSettings::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	printf("[neutrino VideoSettings] %s: init video setup (Mode: %d)...\n",__FUNCTION__ , is_wizard);
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	res = showVideoSetup();
	
	return res;
}

#define VIDEOMENU_43MODE_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_43MODE_OPTIONS[VIDEOMENU_43MODE_OPTION_COUNT] =
{
	{ DISPLAY_AR_MODE_PANSCAN, LOCALE_VIDEOMENU_PANSCAN },
	{ DISPLAY_AR_MODE_PANSCAN2, LOCALE_VIDEOMENU_PANSCAN2 },
	{ DISPLAY_AR_MODE_LETTERBOX, LOCALE_VIDEOMENU_LETTERBOX },
	{ DISPLAY_AR_MODE_NONE, LOCALE_VIDEOMENU_FULLSCREEN }
	//{ 2, LOCALE_VIDEOMENU_AUTO } // whatever is this auto mode, it seems its totally broken
};

#define VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT 8
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_SCART   }, /* composite + RGB (for both SCART and Cinch) */
	{ ANALOG_SD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_CINCH   }, /* composite + RGB (for both SCART and Cinch) */
	{ ANALOG_SD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_SCART }, /* YPbPr SCART (with wrongly connected Cinch) */
	{ ANALOG_SD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_CINCH },  /* YPbPr Cinch (with wrongly connected SCART) */
	{ ANALOG_HD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_SCART   },
	{ ANALOG_HD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_CINCH   },
	{ ANALOG_HD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_SCART },
	{ ANALOG_HD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_CINCH }
};

#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_SCART   }, /* composite + RGB */
	{ ANALOG_SD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_SCART }, /* YPbPr SCART */
	{ ANALOG_HD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_SCART   },
	{ ANALOG_HD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_SCART },
};

// #define VIDEOMENU_VCRSIGNAL_OPTION_COUNT 2
// const CMenuOptionChooser::keyval VIDEOMENU_VCRSIGNAL_OPTIONS[VIDEOMENU_VCRSIGNAL_OPTION_COUNT] =
// {
// 	{ 2, LOCALE_VIDEOMENU_VCRSIGNAL_SVIDEO    },
// 	{ 0, LOCALE_VIDEOMENU_VCRSIGNAL_COMPOSITE }
// };


#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_CINCH   }, /* composite + RGB (for both SCART and Cinch) */
	{ ANALOG_SD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_CINCH },  /* YPbPr Cinch (with wrongly connected SCART) */
	{ ANALOG_HD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_CINCH   },
	{ ANALOG_HD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_CINCH }
};

/* numbers corresponding to video.cpp from zapit */
CMenuOptionChooser::keyval_ext VIDEOMENU_VIDEOMODE_OPTIONS[VIDEOMENU_VIDEOMODE_OPTION_COUNT] =
{
	{ VIDEO_STD_SECAM,   NONEXISTANT_LOCALE, "SECAM"	},
	{ VIDEO_STD_PAL,     NONEXISTANT_LOCALE, "PAL"		},
	{ VIDEO_STD_576P,    NONEXISTANT_LOCALE, "576p"		},
	{ VIDEO_STD_720P50,  NONEXISTANT_LOCALE, "720p 50Hz"	},
	{ VIDEO_STD_1080I50, NONEXISTANT_LOCALE, "1080i 50Hz"	},
	{ VIDEO_STD_1080P24, NONEXISTANT_LOCALE, "1080p 24Hz"	},
	{ VIDEO_STD_1080P25, NONEXISTANT_LOCALE, "1080p 25Hz"	},

	{ VIDEO_STD_NTSC,    NONEXISTANT_LOCALE, "NTSC"		},
	{ VIDEO_STD_480P,    NONEXISTANT_LOCALE, "480p"		},
	{ VIDEO_STD_720P60,  NONEXISTANT_LOCALE, "720p 60Hz"	},
	{ VIDEO_STD_1080I60, NONEXISTANT_LOCALE, "1080i 60Hz"	},
	{ VIDEO_STD_AUTO,    NONEXISTANT_LOCALE, "Auto"         }
};

#define VIDEOMENU_VIDEOFORMAT_OPTION_COUNT 3//2
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOFORMAT_OPTIONS[VIDEOMENU_VIDEOFORMAT_OPTION_COUNT] =
{
	{ DISPLAY_AR_4_3, LOCALE_VIDEOMENU_VIDEOFORMAT_43         },
	{ DISPLAY_AR_16_9, LOCALE_VIDEOMENU_VIDEOFORMAT_169        },
	{ DISPLAY_AR_14_9, LOCALE_VIDEOMENU_VIDEOFORMAT_149        }
};

#define VIDEOMENU_DBDR_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_DBDR_OPTIONS[VIDEOMENU_DBDR_OPTION_COUNT] =
{
	{ 0, LOCALE_VIDEOMENU_DBDR_NONE },
	{ 1, LOCALE_VIDEOMENU_DBDR_DEBLOCK },
	{ 2, LOCALE_VIDEOMENU_DBDR_BOTH }
};

int CVideoSettings::showVideoSetup()
{
	//init
	CMenuWidget * videosetup = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width);
	videosetup->setSelected(selected);
	videosetup->setWizardMode(is_wizard);

	//analog options
	unsigned int system_rev = cs_get_revision();
	CMenuOptionChooser * vs_analg_ch = NULL; 
	CMenuOptionChooser * vs_scart_ch = NULL;
	CMenuOptionChooser * vs_chinch_ch = NULL;
	if (system_rev == 0x06) 
	{
		vs_analg_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_ANALOG_MODE, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_HD1_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT, true, this);
	} 
	else if (system_rev > 0x06) 
	{
		if(system_rev != 10)
			vs_scart_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_SCART, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT, true, this);

		vs_chinch_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_CINCH, &g_settings.analog_mode2, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT, true, this);
	}

	//4:3 mode
	CMenuOptionChooser * vs_43mode_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_43MODE, &g_settings.video_43mode, VIDEOMENU_43MODE_OPTIONS, VIDEOMENU_43MODE_OPTION_COUNT, true, this);

	//display format
	CMenuOptionChooser * vs_dispformat_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_VIDEOFORMAT, &g_settings.video_Format, VIDEOMENU_VIDEOFORMAT_OPTIONS, VIDEOMENU_VIDEOFORMAT_OPTION_COUNT, true, this);

	//video system
	CMenuOptionChooser * vs_videomodes_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_VIDEOMODE, &g_settings.video_Mode, VIDEOMENU_VIDEOMODE_OPTIONS, VIDEOMENU_VIDEOMODE_OPTION_COUNT, true, this, CRCInput::RC_nokey, "", true);

	//dbdr options
	CMenuOptionChooser * vs_dbdropt_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_DBDR, &g_settings.video_dbdr, VIDEOMENU_DBDR_OPTIONS, VIDEOMENU_DBDR_OPTION_COUNT, true, this);

	//video system modes submenue
	CMenuWidget videomodes(LOCALE_MAINSETTINGS_VIDEO, NEUTRINO_ICON_SETTINGS);
	videomodes.addIntroItems(LOCALE_VIDEOMENU_ENABLED_MODES);

	CAutoModeNotifier * anotify = new CAutoModeNotifier();
	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
		videomodes.addItem(new CMenuOptionChooser(VIDEOMENU_VIDEOMODE_OPTIONS[i].valname, &g_settings.enabled_video_modes[i], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, anotify));
	//anotify->changeNotify(NONEXISTANT_LOCALE, 0);

	CMenuForwarder * vs_videomodes_fw = new CMenuForwarder(LOCALE_VIDEOMENU_ENABLED_MODES, true, NULL, &videomodes, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED );

	//---------------------------------------
	videosetup->addIntroItems(LOCALE_MAINSETTINGS_VIDEO, LOCALE_VIDEOMENU_TV_SCART);
	//---------------------------------------
	//videosetup->addItem(vs_scart_sep);	  //separator scart
	if (vs_analg_ch != NULL)
		videosetup->addItem(vs_analg_ch); //analog option
	if (vs_scart_ch != NULL)
		videosetup->addItem(vs_scart_ch); //scart
	if (vs_chinch_ch != NULL)
		videosetup->addItem(vs_chinch_ch);//chinch
	videosetup->addItem(GenericMenuSeparatorLine);
	//---------------------------------------
	videosetup->addItem(vs_43mode_ch);	  //4:3 mode
	videosetup->addItem(vs_dispformat_ch);	  //display format
	videosetup->addItem(vs_videomodes_ch);	  //video system
	videosetup->addItem(vs_dbdropt_ch);	  //dbdr options
	videosetup->addItem(vs_videomodes_fw);	  //video modes submenue

	int res = videosetup->exec(NULL, "");
	videosetup->hide();
	selected = videosetup->getSelected();
	delete videosetup;
	return res;
}


void CVideoSettings::setVideoSettings()
{
	printf("[neutrino VideoSettings] %s init video settings...\n", __FUNCTION__);
	unsigned int system_rev = cs_get_revision();
#if 0
	//FIXME focus: ?? this is different for different boxes
	videoDecoder->SetVideoMode((analog_mode_t) g_settings.analog_mode1);
	videoDecoder->SetVideoMode((analog_mode_t) g_settings.analog_mode2);
#endif
	if (system_rev == 0x06) {
		changeNotify(LOCALE_VIDEOMENU_ANALOG_MODE, NULL);
	} else {
		changeNotify(LOCALE_VIDEOMENU_SCART, NULL);
		changeNotify(LOCALE_VIDEOMENU_CINCH, NULL);
	}

	//setupVideoSystem(false/*don't ask*/);// focus: CVideoSettings constructor do this already ?

#if 0
	videoDecoder->setAspectRatio(-1, g_settings.video_43mode);
	videoDecoder->setAspectRatio(g_settings.video_Format, -1);
#endif
	videoDecoder->setAspectRatio(g_settings.video_Format, g_settings.video_43mode);

	videoDecoder->SetDBDR(g_settings.video_dbdr);
	CAutoModeNotifier * anotify = new CAutoModeNotifier();
	anotify->changeNotify(NONEXISTANT_LOCALE, 0);
	delete anotify;
}

void CVideoSettings::setupVideoSystem(bool do_ask)
{
	printf("[neutrino VideoSettings] %s setup videosystem...\n", __FUNCTION__);
	videoDecoder->SetVideoSystem(g_settings.video_Mode); //FIXME 
	
	if (do_ask)
	{
		if (prev_video_mode != g_settings.video_Mode) 
		{
			frameBuffer->paintBackground();
			if (ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_VIDEO_MODE_OK), CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_INFO) != CMessageBox::mbrYes) 
			{
				g_settings.video_Mode = prev_video_mode;
				videoDecoder->SetVideoSystem(g_settings.video_Mode);
			} 
		}
		else
			prev_video_mode = g_settings.video_Mode;
	}
}

bool CVideoSettings::changeNotify(const neutrino_locale_t OptionName, void *data)
{
	int val = 0;
	if(data)
		val = * (int *) data;

	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_ANALOG_MODE))
	{
		videoDecoder->SetVideoMode((analog_mode_t) g_settings.analog_mode1);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_SCART))
	{
		videoDecoder->SetVideoMode((analog_mode_t) g_settings.analog_mode1);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_CINCH))
	{
		videoDecoder->SetVideoMode((analog_mode_t) g_settings.analog_mode2);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_DBDR))
	{
		videoDecoder->SetDBDR(g_settings.video_dbdr);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_VCRSIGNAL))
	{
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_VIDEOFORMAT) ||
			ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_43MODE))
	{
		//if(g_settings.video_Format != 1 && g_settings.video_Format != 3)
		if (g_settings.video_Format != 1 && g_settings.video_Format != 3 && g_settings.video_Format != 2)
			g_settings.video_Format = 3;
		videoDecoder->setAspectRatio(g_settings.video_Format, g_settings.video_43mode);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_VIDEOMODE))
	{
		setupVideoSystem(true/*ask*/);
	}
#if 1
        else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_CONTRAST))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_CONTRAST, val);
	}
        else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_BRIGHTNESS))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_BRIGHTNESS, val);
	}
        else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_SHARPNESS))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_SHARPNESS, val);
	}
        else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_SATURATION))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_SATURATION, val);
	}
        else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HUE))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_HUE, val);
	}
#endif
	return true;
}



void CVideoSettings::next43Mode(void)
{
	printf("[neutrino VideoSettings] %s setting 43Mode...\n", __FUNCTION__);
	neutrino_locale_t text;
	int curmode = 0;

	for (int i = 0; i < VIDEOMENU_43MODE_OPTION_COUNT; i++) {
		if (VIDEOMENU_43MODE_OPTIONS[i].key == g_settings.video_43mode) {
			curmode = i;
			break;
		}
	}
	curmode++;
	if (curmode >= VIDEOMENU_43MODE_OPTION_COUNT)
		curmode = 0;

	text =  VIDEOMENU_43MODE_OPTIONS[curmode].value;
	g_settings.video_43mode = VIDEOMENU_43MODE_OPTIONS[curmode].key;
	videoDecoder->setAspectRatio(-1, g_settings.video_43mode);
	ShowHintUTF(LOCALE_VIDEOMENU_43MODE, g_Locale->getText(text), 450, 2);
	
}

void CVideoSettings::SwitchFormat()
{
	printf("[neutrino VideoSettings] %s setting videoformat...\n", __FUNCTION__);
	neutrino_locale_t text;
	int curmode = 0;

	for (int i = 0; i < VIDEOMENU_VIDEOFORMAT_OPTION_COUNT; i++) {
		if (VIDEOMENU_VIDEOFORMAT_OPTIONS[i].key == g_settings.video_Format) {
			curmode = i;
			break;
		}
	}
	curmode++;
	if (curmode >= VIDEOMENU_VIDEOFORMAT_OPTION_COUNT)
		curmode = 0;

	text =  VIDEOMENU_VIDEOFORMAT_OPTIONS[curmode].value;
	g_settings.video_Format = VIDEOMENU_VIDEOFORMAT_OPTIONS[curmode].key;

	videoDecoder->setAspectRatio(g_settings.video_Format, -1);
	ShowHintUTF(LOCALE_VIDEOMENU_VIDEOFORMAT, g_Locale->getText(text), 450, 2);
}

void CVideoSettings::nextMode(void)
{
	printf("[neutrino VideoSettings] %s setting video Mode...\n", __FUNCTION__);
	const char * text;
	int curmode = 0;
	int i;
	bool disp_cur = 1;

	for (i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++) {
		if (VIDEOMENU_VIDEOMODE_OPTIONS[i].key == g_settings.video_Mode) {
			curmode = i;
			break;
		}
	}
	text =  VIDEOMENU_VIDEOMODE_OPTIONS[curmode].valname;

	while(1) {
		CVFD::getInstance()->ShowText(text);
		int res = ShowHintUTF(LOCALE_VIDEOMENU_VIDEOMODE, text, 450, 2);

		if(disp_cur && res != messages_return::handled)
			break;

		disp_cur = 0;

		if(res == messages_return::handled) {
			i = 0;
			while (true) {
				curmode++;
				if (curmode >= VIDEOMENU_VIDEOMODE_OPTION_COUNT)
					curmode = 0;
				if (g_settings.enabled_video_modes[curmode])
					break;
				i++;
				if (i >= VIDEOMENU_VIDEOMODE_OPTION_COUNT) {
					CVFD::getInstance()->showServicename(g_RemoteControl->getCurrentChannelName());
					return;
				}
			}

			text =  VIDEOMENU_VIDEOMODE_OPTIONS[curmode].valname;
		}
		else if(res == messages_return::cancel_info) {
			g_settings.video_Mode = VIDEOMENU_VIDEOMODE_OPTIONS[curmode].key;
			//CVFD::getInstance()->ShowText(text);
			videoDecoder->SetVideoSystem(g_settings.video_Mode);
			//return;
			disp_cur = 1;
		}
		else
			break;
	}
	CVFD::getInstance()->showServicename(g_RemoteControl->getCurrentChannelName());
	//ShowHintUTF(LOCALE_VIDEOMENU_VIDEOMODE, text, 450, 2);
}

//sets menu mode to "wizard" or "default"
void CVideoSettings::setWizardMode(bool mode)
{
	printf("[neutrino VideoSettings] %s set video settings menu to mode %d...\n", __FUNCTION__, mode);
	is_wizard = mode;
}

