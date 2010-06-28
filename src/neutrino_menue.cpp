/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
							 and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>

#include <sys/socket.h>

#include <iostream>
#include <fstream>
#include <string>


#include "global.h"
#include "neutrino.h"

#include <daemonc/remotecontrol.h>

#include <driver/encoding.h>
#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/stream2file.h>
#include <driver/vcrcontrol.h>
#include <driver/shutdown_count.h>

#include <gui/epgplus.h>
#include <gui/streaminfo2.h>

#include "gui/widget/colorchooser.h"
#include "gui/widget/menue.h"
#include "gui/widget/messagebox.h"
#include "gui/widget/hintbox.h"
#include "gui/widget/icons.h"
#include "gui/widget/lcdcontroler.h"
#include "gui/widget/vfdcontroler.h"
#include "gui/widget/rgbcsynccontroler.h"
#include "gui/widget/keychooser.h"
#include "gui/widget/stringinput.h"
#include "gui/widget/stringinput_ext.h"
#include "gui/widget/mountchooser.h"

#include "gui/color.h"
#include "gui/customcolor.h"

#include "gui/bedit/bouqueteditor_bouquets.h"
#include "gui/bouquetlist.h"
#include "gui/eventlist.h"
#include "gui/channellist.h"
#include "gui/screensetup.h"
#include "gui/pluginlist.h"
#include "gui/plugins.h"
#include "gui/infoviewer.h"
#include "gui/epgview.h"
#include "gui/epg_menu.h"
#include "gui/update.h"
#include "gui/scan.h"
#include "gui/favorites.h"
#include "gui/sleeptimer.h"
#include "gui/rc_lock.h"
#include "gui/timerlist.h"
#include "gui/alphasetup.h"
#include "gui/audioplayer.h"
#include "gui/imageinfo.h"
#include "gui/movieplayer.h"
#include "gui/nfs.h"
#include "gui/pictureviewer.h"
#include "gui/motorcontrol.h"
#include "gui/filebrowser.h"
#include "gui/upnpbrowser.h"

#include <system/setting_helpers.h>
#include <system/settings.h>
#include <system/debug.h>
#include <system/flashtool.h>
#include <system/fsmounter.h>

#include <timerdclient/timerdmsg.h>
#include <video_cs.h>
#include <audio_cs.h>
#include <zapit/frontend_c.h>
#include <xmlinterface.h>
#include <zapit/getservices.h>

#include <string.h>
#include "gui/infoclock.h"
#include "mymenu.h"
#include "gui/dboxinfo.h"
#include "gui/hdd_menu.h"
#include "gui/audio_select.h"
#include "gui/cam_menu.h"

#include <zapit/getservices.h>
#include <zapit/satconfig.h>
#include <zapit/fastscan.h>

//#define TEST_MENU

extern CFrontend * frontend;
extern CAudioPlayerGui * audioPlayer;
extern CMoviePlayerGui* moviePlayerGui;
extern CPlugins       * g_PluginList;
extern bool has_hdd;
extern CZapitClient::SatelliteList satList;
extern Zapit_config zapitCfg;
extern char zapit_lat[20];
extern char zapit_long[20];
extern char current_timezone[50];
extern cVideo *videoDecoder;
extern cAudio *audioDecoder;
extern int prev_video_mode;
extern bool parentallocked;
extern const char * locale_real_names[];
extern CFontSizeNotifier fontsizenotifier;
extern CFanControlNotifier * funNotifier;
extern CRemoteControl * g_RemoteControl;
extern CCAMMenuHandler * g_CamHandler;
extern bool autoshift;

//extern int sectionsd_scanning;

static CTimingSettingsNotifier timingsettingsnotifier;

int safe_mkdir(char * path);

#define OPTIONS_OFF0_ON1_OPTION_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_OFF0_ON1_OPTIONS[OPTIONS_OFF0_ON1_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON  }
};

#define OPTIONS_OFF1_ON0_OPTION_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_OFF1_ON0_OPTIONS[OPTIONS_OFF1_ON0_OPTION_COUNT] =
{
	{ 1, LOCALE_OPTIONS_OFF },
	{ 0, LOCALE_OPTIONS_ON  }
};

#define VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT 8
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_MODE_SD_RGB_SCART   }, /* composite + RGB (for both SCART and Cinch) */
	{ ANALOG_SD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_MODE_SD_RGB_CINCH   }, /* composite + RGB (for both SCART and Cinch) */
	{ ANALOG_SD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_MODE_SD_YPRPB_SCART }, /* YPbPr SCART (with wrongly connected Cinch) */
	{ ANALOG_SD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_MODE_SD_YPRPB_CINCH },  /* YPbPr Cinch (with wrongly connected SCART) */
	{ ANALOG_HD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_MODE_HD_RGB_SCART   },
	{ ANALOG_HD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_MODE_HD_RGB_CINCH   },
	{ ANALOG_HD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_MODE_HD_YPRPB_SCART },
	{ ANALOG_HD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_MODE_HD_YPRPB_CINCH }
};

#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_MODE_SD_RGB_SCART   }, /* composite + RGB */
	{ ANALOG_SD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_MODE_SD_YPRPB_SCART }, /* YPbPr SCART */
	{ ANALOG_HD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_MODE_HD_RGB_SCART   },
	{ ANALOG_HD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_MODE_HD_YPRPB_SCART },
};

#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_MODE_SD_RGB_CINCH   }, /* composite + RGB (for both SCART and Cinch) */
	{ ANALOG_SD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_MODE_SD_YPRPB_CINCH },  /* YPbPr Cinch (with wrongly connected SCART) */
	{ ANALOG_HD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_MODE_HD_RGB_CINCH   },
	{ ANALOG_HD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_MODE_HD_YPRPB_CINCH }
};

#define VIDEOMENU_VCRSIGNAL_OPTION_COUNT 2
const CMenuOptionChooser::keyval VIDEOMENU_VCRSIGNAL_OPTIONS[VIDEOMENU_VCRSIGNAL_OPTION_COUNT] =
{
	{ 2, LOCALE_VIDEOMENU_VCRSIGNAL_SVIDEO    },
	{ 0, LOCALE_VIDEOMENU_VCRSIGNAL_COMPOSITE }
};

#define VIDEOMENU_VIDEOFORMAT_OPTION_COUNT 3//2
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOFORMAT_OPTIONS[VIDEOMENU_VIDEOFORMAT_OPTION_COUNT] =
{
	{ DISPLAY_AR_4_3, LOCALE_VIDEOMENU_VIDEOFORMAT_43         },
	{ DISPLAY_AR_16_9, LOCALE_VIDEOMENU_VIDEOFORMAT_169        },
	{ DISPLAY_AR_14_9, LOCALE_VIDEOMENU_VIDEOFORMAT_149        }
};

#define VIDEOMENU_43MODE_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_43MODE_OPTIONS[VIDEOMENU_43MODE_OPTION_COUNT] =
{
	{ DISPLAY_AR_MODE_PANSCAN, LOCALE_VIDEOMENU_PANSCAN },
	{ DISPLAY_AR_MODE_PANSCAN2, LOCALE_VIDEOMENU_PANSCAN2 },
	{ DISPLAY_AR_MODE_LETTERBOX, LOCALE_VIDEOMENU_LETTERBOX },
	{ DISPLAY_AR_MODE_NONE, LOCALE_VIDEOMENU_FULLSCREEN }
	//{ 2, LOCALE_VIDEOMENU_AUTO } // whatever is this auto mode, it seems its totally broken
};

#define VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_CEC_MODE_OPTIONS[VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT] =
{
	{ VIDEO_HDMI_CEC_MODE_OFF	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_OFF      },
	{ VIDEO_HDMI_CEC_MODE_TUNER	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_TUNER    },
	{ VIDEO_HDMI_CEC_MODE_RECORDER	, LOCALE_VIDEOMENU_HDMI_CEC_MODE_RECORDER },
};

/* numbers corresponding to video.cpp from zapit */
//#define VIDEOMENU_VIDEOMODE_OPTION_COUNT 11
const CMenuOptionChooser::keyval_ext VIDEOMENU_VIDEOMODE_OPTIONS[VIDEOMENU_VIDEOMODE_OPTION_COUNT] =
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

#define VIDEOMENU_DBDR_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_DBDR_OPTIONS[VIDEOMENU_DBDR_OPTION_COUNT] =
{
	{ 0, LOCALE_VIDEOMENU_DBDR_NONE },
	{ 1, LOCALE_VIDEOMENU_DBDR_DEBLOCK },
	{ 2, LOCALE_VIDEOMENU_DBDR_BOTH }
};

#include "videosettings.h"
#include <cs_api.h>

CVideoSettings::CVideoSettings() : CMenuWidget(LOCALE_VIDEOMENU_HEAD, NEUTRINO_ICON_VIDEO) /*, RGBCSyncControler(LOCALE_VIDEOMENU_RGB_CENTERING, &g_settings.video_csync)*/
{
	addItem(GenericMenuSeparator);
	addItem(GenericMenuBack);

	addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_VIDEOMENU_TV_SCART));

	unsigned int system_rev = cs_get_revision();
	if (system_rev == 0x06) {
		addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_ANALOG_MODE, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_HD1_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT, true, this));
	} else if (system_rev > 0x06) {
		addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_SCART, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT, true, this));
		addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_CINCH, &g_settings.analog_mode2, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT, true, this));
	}

	addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_VIDEOFORMAT, &g_settings.video_Format, VIDEOMENU_VIDEOFORMAT_OPTIONS, VIDEOMENU_VIDEOFORMAT_OPTION_COUNT, true, this));
	addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_43MODE, &g_settings.video_43mode, VIDEOMENU_43MODE_OPTIONS, VIDEOMENU_43MODE_OPTION_COUNT, true, this));
	addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_VIDEOMODE, &g_settings.video_Mode, VIDEOMENU_VIDEOMODE_OPTIONS, VIDEOMENU_VIDEOMODE_OPTION_COUNT, true, this, CRCInput::RC_nokey, "", true));
	addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_DBDR, &g_settings.video_dbdr, VIDEOMENU_DBDR_OPTIONS, VIDEOMENU_DBDR_OPTION_COUNT, true, this));
	addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_MODE, &g_settings.hdmi_cec_mode, VIDEOMENU_HDMI_CEC_MODE_OPTIONS, VIDEOMENU_HDMI_CEC_MODE_OPTION_COUNT, true, this));

	cec1 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_VIEW_ON, &g_settings.hdmi_cec_view_on, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);
	cec2 = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_CEC_STANDBY, &g_settings.hdmi_cec_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF, this);

	addItem(cec1);
	addItem(cec2);

	CMenuWidget* menu = new CMenuWidget(LOCALE_VIDEOMENU_ENABLED_MODES, NEUTRINO_ICON_SETTINGS);
	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
		menu->addItem(new CMenuOptionChooser(VIDEOMENU_VIDEOMODE_OPTIONS[i].valname, &g_settings.enabled_video_modes[i], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	addItem(new CMenuForwarder(LOCALE_VIDEOMENU_ENABLED_MODES, true, NULL, menu));
#if 0
	addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_VIDEOMENU_VCR_SCART));
	addItem(new CMenuOptionChooser(LOCALE_VIDEOMENU_VCRSWITCH, &g_settings.vcr_AutoSwitch, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_VIDEOMENU_OSD));
	addItem(new CMenuForwarder(LOCALE_VIDEOMENU_SCREENSETUP, true, NULL, &ScreenSetup, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
#endif
	changeNotify(LOCALE_VIDEOMENU_VIDEOFORMAT, NULL);
	if (system_rev == 0x06) {
		changeNotify(LOCALE_VIDEOMENU_ANALOG_MODE, NULL);
	} else {
		changeNotify(LOCALE_VIDEOMENU_SCART, NULL);
		changeNotify(LOCALE_VIDEOMENU_CINCH, NULL);
	}
}

void CVideoSettings::nextMode(void)
{
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
		CVFD::getInstance()->ShowText((char *)text);
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
			//CVFD::getInstance()->ShowText((char *)text);
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

void CVideoSettings::next43Mode(void)
{
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

void CVideoSettings::SwitchFormat(void)
{
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

bool CVideoSettings::changeNotify(const neutrino_locale_t OptionName, void *)
{
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
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_CEC_MODE))
	{
		cec1->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
		cec2->setActive(g_settings.hdmi_cec_mode != VIDEO_HDMI_CEC_MODE_OFF);
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
		videoDecoder->SetVideoSystem(g_settings.video_Mode);

		if (prev_video_mode != g_settings.video_Mode) {
			frameBuffer->paintBackground();
			if (ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_VIDEOMODE_OK), CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_INFO) != CMessageBox::mbrYes) {
				g_settings.video_Mode = prev_video_mode;
				videoDecoder->SetVideoSystem(g_settings.video_Mode);
			} else
				prev_video_mode = g_settings.video_Mode;
		}
	}

	return true;
}

void CVideoSettings::paint()
{
	CMenuWidget::paint();
}

#ifdef TEST_MENU
class CTestMenu : public CMenuTarget
{
public:
	int exec(CMenuTarget* parent,  const std::string &actionkey);
};

#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

extern int cs_test_card(int unit, char * str);
int CTestMenu::exec(CMenuTarget* parent, const std::string &actionKey)
{
	if (parent)
		parent->hide();

	printf("CTestMenu::exec: %s\n", actionKey.c_str());
	if (actionKey == "vfd") {
		CVFD::getInstance()->Clear();
		int icon = 0x00040000;
		while (icon > 0x2) {
			CVFD::getInstance()->ShowIcon((vfd_icon) icon, true);
			icon /= 2;
		}
		for (int i = 0x01000001; i <= 0x0C000001; i+= 0x01000000) {
			CVFD::getInstance()->ShowIcon((vfd_icon) i, true);
		}
		CVFD::getInstance()->ShowIcon((vfd_icon) 0x09000002, true);
		CVFD::getInstance()->ShowIcon((vfd_icon) 0x0B000002, true);
		char text[255];
		char buf[XML_UTF8_ENCODE_MAX];
		int ch = 0x2588;
		int len = XmlUtf8Encode(ch, buf);

		for (int i = 0; i < 12; i++) {
			memcpy(&text[i*len], buf, len);
		}
		text[12*len] = 0;

		CVFD::getInstance()->ShowText(text);
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "VFD test, Press OK to return", CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		CVFD::getInstance()->Clear();
	}
	else if (actionKey == "network") {
		int fd, ret;
		struct ifreq ifr;
		char * ip = NULL, str[255];
		struct sockaddr_in *addrp=NULL;

		fd = socket(AF_INET, SOCK_DGRAM, 0);

		ifr.ifr_addr.sa_family = AF_INET;
		strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

		ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
		if (ret < 0)
			perror("SIOCGIFHWADDR");

		ret = ioctl(fd, SIOCGIFADDR, &ifr );
		if (ret < 0)
			perror("SIOCGIFADDR");
		else {
			addrp = (struct sockaddr_in *)&(ifr.ifr_addr);
			ip = inet_ntoa(addrp->sin_addr);
		}

		sprintf(str, "MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\nIP: %s",
			(unsigned char)ifr.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr.ifr_hwaddr.sa_data[5], ip == NULL ? "Unknown" : ip);

		close(fd);
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
	}
	else if (actionKey == "card0") {
		char str[255];
		int ret = cs_test_card(0, str);
		switch(ret) {
			case 0:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 ATR read failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			case -2:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 reset failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 open failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
		}
	}
	else if (actionKey == "card1") {
		char str[255];
		int ret = cs_test_card(1, str);
		switch(ret) {
			case 0:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 ATR read failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			case -2:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 reset failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 open failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
		}
	}
	else if (actionKey == "hdd") {
		char buffer[255];
		FILE *f = fopen("/proc/mounts", "r");
		bool mounted = false;
		if (f != NULL) {
			while (fgets (buffer, 255, f) != NULL) {
				if (strstr(buffer, "/dev/sda1")) {
					mounted = true;
					break;
				}
			}
			fclose(f);
		}
		sprintf(buffer, "HDD: /dev/sda1 is %s", mounted ? "mounted" : "NOT mounted");
		printf("%s\n", buffer);
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, buffer, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
	}
	else if (actionKey == "buttons") {
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;
		CHintBox * khintBox = NULL;
		CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, "Press button, or press EXIT to return");
		hintBox->paint();
		while (1) {
			g_RCInput->getMsg(&msg, &data, 100);
			if (msg == CRCInput::RC_home)
				break;

			if (msg != CRCInput::RC_timeout && msg <= CRCInput::RC_MaxRC) {
				char keyname[50];
				sprintf(keyname, "Button [%s] pressed (EXIT to return)", g_RCInput->getKeyName(msg).c_str());
				if (khintBox) {
					delete khintBox;
				}
				khintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, keyname);
				hintBox->hide();
				khintBox->paint();
			}
		}
		if (khintBox)
			delete khintBox;
		delete hintBox;
	}
	else if (actionKey == "22kon" || actionKey == "22koff") {
		CScanTs * scanTs = new CScanTs();

		int freq = (actionKey == "22kon") ? 12000*1000: 11000*1000;

		sprintf(get_set.TP_freq, "%d", freq);
#if 0 // not needed ?
		switch (frontend->getInfo()->type) {
		case FE_QPSK:
			sprintf(get_set.TP_rate, "%d", tmpI->second.feparams.u.qpsk.symbol_rate);
			get_set.TP_fec = tmpI->second.feparams.u.qpsk.fec_inner;
			get_set.TP_pol = tmpI->second.polarization;
			break;
		case FE_QAM:
			sprintf(get_set.TP_rate, "%d", tmpI->second.feparams.u.qam.symbol_rate);
			get_set.TP_fec = tmpI->second.feparams.u.qam.fec_inner;
			get_set.TP_mod = tmpI->second.feparams.u.qam.modulation;
			break;
		}
#endif
		scanTs->exec(NULL, "test");
		delete scanTs;
	}
	else if (actionKey == "scan") {
		CScanTs * scanTs = new CScanTs();

		int freq = 12538000;
		sprintf(get_set.TP_freq, "%d", freq);
		switch (frontend->getInfo()->type) {
		case FE_QPSK:
			sprintf(get_set.TP_rate, "%d", 41250*1000);
			get_set.TP_fec = 1;
			get_set.TP_pol = 1;
			break;
		case FE_QAM:
#if 0
			sprintf(get_set.TP_rate, "%d", tmpI->second.feparams.u.qam.symbol_rate);
			get_set.TP_fec = tmpI->second.feparams.u.qam.fec_inner;
			get_set.TP_mod = tmpI->second.feparams.u.qam.modulation;
#endif
			break;
		case FE_OFDM:
		case FE_ATSC:
			break;
		}
		scanTs->exec(NULL, "manual");
		delete scanTs;
	}

	return menu_return::RETURN_REPAINT;
}

CMenuWidget * TestMenu;
#endif

CVideoSettings * videoSettings;
//CMenuOptionStringChooser* tzSelect;
/**************************************************************************************
*          CNeutrinoApp -  init main menu                                             *
**************************************************************************************/
void CNeutrinoApp::InitMainMenu(CMenuWidget &mainMenu, CMenuWidget &mainSettings,  CMenuWidget &audioSettings, CMenuWidget &parentallockSettings,
				CMenuWidget &networkSettings, CMenuWidget &recordingSettings, CMenuWidget &colorSettings, CMenuWidget &lcdSettings,
				CMenuWidget &keySettings, CMenuWidget &languageSettings, CMenuWidget &miscSettings,
				CMenuWidget &service, CMenuWidget &/*fontSettings*/, CMenuWidget &audiopl_picSettings, CMenuWidget &/*streamingSettings*/, CMenuWidget &moviePlayer)
{

#ifdef TEST_MENU
	char rev[255];
	sprintf(rev, "Test menu, System revision %d %s\n", system_rev, system_rev == 0 ? "WARNING - INVALID" : "");

	TestMenu = new CMenuWidget(rev /*"Test menu"*/);
	CTestMenu * testHandler = new CTestMenu();
	TestMenu->addItem(new CMenuForwarderNonLocalized("VFD", true, NULL, testHandler, "vfd"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Network", true, NULL, testHandler, "network"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Smartcard 1", true, NULL, testHandler, "card0"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Smartcard 2", true, NULL, testHandler, "card1"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("HDD", true, NULL, testHandler, "hdd"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Buttons", true, NULL, testHandler, "buttons"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Scan 12538000", true, NULL, testHandler, "scan"));
	//TestMenu->addItem(new CMenuForwarderNonLocalized("22 Khz ON", true, NULL, testHandler, "22kon"));
	//TestMenu->addItem(new CMenuForwarderNonLocalized("22 Khz OFF", true, NULL, testHandler, "22koff"));
#endif
	videoSettings = new CVideoSettings();

	int shortcut = 1;

	dprintf(DEBUG_DEBUG, "init mainmenue\n");
	mainMenu.addItem(GenericMenuSeparator);

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_TVMODE, true, NULL, this, "tv", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED), true);
	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_RADIOMODE, true, NULL, this, "radio", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	//mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SCARTMODE, true, NULL, this, "scart", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	if (g_PluginList->hasPlugin(CPlugins::P_TYPE_GAME))
		mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_GAMES, true, NULL, new CPluginList(LOCALE_MAINMENU_GAMES,CPlugins::P_TYPE_GAME), "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	audioPlayer = new CAudioPlayerGui();
	moviePlayerGui = new CMoviePlayerGui();

	addMenueIntroItems(moviePlayer);
	moviePlayer.addItem(new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, moviePlayerGui, "tsmoviebrowser", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	moviePlayer.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, moviePlayerGui, "fileplayback", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

#if 1
	CMenuWidget *media = new CMenuWidget(LOCALE_MAINMENU_MEDIA);
	addMenueIntroItems(*media);
	audioPlayer = new CAudioPlayerGui();
	media->addItem(new CMenuForwarder(LOCALE_MAINMENU_AUDIOPLAYER, true, NULL, audioPlayer, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	audioPlayer = new CAudioPlayerGui(true);
	media->addItem(new CMenuForwarder(LOCALE_INETRADIO_NAME, true, NULL, audioPlayer, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
	media->addItem(new CMenuForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, &moviePlayer, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	media->addItem(new CMenuForwarder(LOCALE_MAINMENU_PICTUREVIEWER, true, NULL, new CPictureViewerGui(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	media->addItem(new CMenuForwarder(LOCALE_UPNPBROWSER_HEAD, true, NULL, new CUpnpBrowserGui(), NULL, CRCInput::convertDigitToKey(0)));
	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_MEDIA, true, NULL, media, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
#else
	mainMenu.addItem(GenericMenuSeparatorLine);
	mainMenu.addItem(new CMenuForwarder(LOCALE_UPNPBROWSER_HEAD, true, NULL, new CUpnpBrowserGui(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_AUDIOPLAYER, true, NULL, audioPlayer, NULL, CRCInput::convertDigitToKey(shortcut++)));

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_MOVIEPLAYER, true, NULL, &moviePlayer, NULL, CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_PICTUREVIEWER, true, NULL, new CPictureViewerGui(), NULL, CRCInput::convertDigitToKey(shortcut++)));
#endif

#if 0
	moviePlayer.addItem(GenericMenuSeparator);
	moviePlayer.addItem(GenericMenuBack);
	moviePlayer.addItem(GenericMenuSeparatorLine);

	//moviePlayer.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_PESPLAYBACK, true, NULL, moviePlayerGui, "pesplayback"));
	//moviePlayer.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK_PC, true, NULL, moviePlayerGui, "tsplayback_pc"));
	moviePlayer.addItem(new CLockedMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "tsmoviebrowser"));
	moviePlayer.addItem(new CLockedMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "tsplayback", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

	moviePlayer.addItem(new CLockedMenuForwarder(LOCALE_MOVIEPLAYER_BOOKMARK, g_settings.parentallock_pincode, false, true, NULL, moviePlayerGui, "bookmarkplayback"));
	moviePlayer.addItem(GenericMenuSeparator);
	moviePlayer.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_FILEPLAYBACK, true, NULL, moviePlayerGui, "fileplayback", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	moviePlayer.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_DVDPLAYBACK, true, NULL, moviePlayerGui, "dvdplayback", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	moviePlayer.addItem(new CMenuForwarder(LOCALE_MOVIEPLAYER_VCDPLAYBACK, true, NULL, moviePlayerGui, "vcdplayback", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
	moviePlayer.addItem(GenericMenuSeparatorLine);
	moviePlayer.addItem(new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, &streamingSettings, NULL, CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL));
	moviePlayer.addItem(new CMenuForwarder(LOCALE_NFSMENU_HEAD, true, NULL, new CNFSSmallMenu(), NULL, CRCInput::RC_setup, NEUTRINO_ICON_BUTTON_DBOX_SMALL));
#endif
	if (g_PluginList->hasPlugin(CPlugins::P_TYPE_SCRIPT))
		mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SCRIPTS, true, NULL, new CPluginList(LOCALE_MAINMENU_SCRIPTS,CPlugins::P_TYPE_SCRIPT), "",
						    CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(GenericMenuSeparatorLine);

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, &mainSettings, NULL,
					    CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(new CLockedMenuForwarder(LOCALE_MAINMENU_SERVICE, g_settings.parentallock_pincode, false, true, NULL, &service, NULL,
			 CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(GenericMenuSeparatorLine);

	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SLEEPTIMER, true, NULL, new CSleepTimerWidget, NULL,
					    CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_REBOOT, true, NULL, this, "reboot",
					    CRCInput::convertDigitToKey(shortcut++)));

	unsigned int system_rev = cs_get_revision();
	if(system_rev >= 8)
		mainMenu.addItem(new CMenuForwarder(LOCALE_MAINMENU_SHUTDOWN, true, NULL, this, "shutdown", CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER));

	mainMenu.addItem( new CMenuSeparator(CMenuSeparator::LINE) );
	mainMenu.addItem( new CMenuForwarder(LOCALE_DBOXINFO, true, NULL, new CDBoxInfoWidget, NULL, CRCInput::convertDigitToKey(shortcut++)));
	mainMenu.addItem( new CMenuForwarder(LOCALE_CAM_SETTINGS, true, NULL, g_CamHandler, NULL, CRCInput::convertDigitToKey(0)));

	//settings menu
	int sett_count =1;
	addMenueIntroItems(mainSettings);

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	CDataResetNotifier * resetNotifier = new CDataResetNotifier();
	CMenuWidget * mset = new CMenuWidget(LOCALE_MAINSETTINGS_MANAGE, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*mset);

	mset->addItem(new CMenuForwarder(LOCALE_RESET_SETTINGS    , true, NULL, resetNotifier, "settings"));
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_LOADCONFIG, true, NULL, this, "loadconfig"));
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVECONFIG, true, NULL, this, "saveconfig"));

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_MANAGE, true, NULL, mset));

	//mainSettings.addItem(new CMenuForwarder(LOCALE_RESET_SETTINGS    , true, NULL, resetNotifier, "settings"));

	mainSettings.addItem(GenericMenuSeparatorLine);
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_VIDEO     , true, NULL, videoSettings    , NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_AUDIO     , true, NULL, &audioSettings    , NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CLockedMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, g_settings.parentallock_pincode, true, true, NULL, &parentallockSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));

#if 0
	if (g_settings.parentallock_prompt)
		mainSettings.addItem(new CLockedMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, g_settings.parentallock_pincode, true, true, NULL, &parentallockSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));
	else
		mainSettings.addItem(new CMenuForwarder(LOCALE_PARENTALLOCK_PARENTALLOCK, true, NULL, &parentallockSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));
#endif
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_NETWORK   , true, NULL, &networkSettings  , NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_RECORDING , true, NULL, &recordingSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));
	//mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_STREAMING , true, NULL, &streamingSettings, NULL, CRCInput::convertDigitToKey(sett_count++)));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_LANGUAGE  , true, NULL, &languageSettings , NULL, CRCInput::convertDigitToKey(sett_count++)));

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_OSD    , true, NULL, &colorSettings    , NULL, CRCInput::convertDigitToKey(sett_count++)));

	if (CVFD::getInstance()->has_lcd)
		mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_LCD       , true, NULL, &lcdSettings      , NULL, CRCInput::convertDigitToKey(sett_count++)));

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_KEYBINDING, true, NULL, &keySettings      , NULL, CRCInput::RC_blue  , NEUTRINO_ICON_BUTTON_BLUE  ));
	mainSettings.addItem(new CMenuForwarder(LOCALE_AUDIOPLAYERPICSETTINGS_GENERAL , true, NULL, &audiopl_picSettings   , NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_MISC      , true, NULL, &miscSettings     , NULL, CRCInput::RC_green , NEUTRINO_ICON_BUTTON_GREEN ));

	mainSettings.addItem(new CMenuForwarder(LOCALE_HDD_SETTINGS, true, NULL, new CHDDMenuHandler()));
	//mainSettings.addItem(new CMenuForwarder(LOCALE_CAM_SETTINGS, true, NULL, g_CamHandler));

#if 0
	CMenuWidget * mset = new CMenuWidget(LOCALE_MAINSETTINGS_MANAGE, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*mset);

	mainSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE));
	mset->addItem(new CMenuForwarder(LOCALE_RESET_SETTINGS    , true, NULL, resetNotifier, "settings"));
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_LOADCONFIG, true, NULL, this, "loadconfig"));
	mset->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVECONFIG, true, NULL, this, "saveconfig"));

	mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_MANAGE, true, NULL, mset));
#endif
#ifdef TEST_MENU
	mainMenu.addItem(new CMenuForwarderNonLocalized("Test menu", true, NULL, TestMenu));
#endif
#if 0
	g_RFmod = new RFmod();
	if (g_RFmod->rfmodfd >=0) {
		g_RFmod->init();
		CMenuWidget * rfmenu  = new CMenuWidget(LOCALE_RFMOD, NEUTRINO_ICON_SETTINGS);
		CRfNotifier * rfnot = new CRfNotifier();
		CRfExec * rfexec = new CRfExec();

		rfmenu->addItem( GenericMenuBack );
		rfmenu->addItem( new CMenuForwarder(LOCALE_RECORDINGMENU_SETUPNOW, true, NULL, rfexec, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
		rfmenu->addItem( GenericMenuSeparatorLine );

		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_CARRIER, &g_settings.rf_subcarrier, RF_CARRIER_OPTIONS, RF_CARRIER_OPTION_COUNT, true, rfnot));
		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_ENABLE, &g_settings.rf_soundenable, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, rfnot));
		rfmenu->addItem( new CMenuOptionNumberChooser(LOCALE_RF_CHANNEL, &g_settings.rf_channel, true, 21, 69) );
		rfmenu->addItem( new CMenuOptionNumberChooser(LOCALE_RF_FINETUNE, &g_settings.rf_finetune, true, 0, 64) );

		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_STANDBY, &g_settings.rf_standby, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, rfnot));
		rf_dummy = 0;
		rfmenu->addItem( new CMenuOptionChooser(LOCALE_RF_TEST, &rf_dummy, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, rfnot));
		mainSettings.addItem(new CMenuForwarder(LOCALE_RFMOD, true, NULL, rfmenu));
	}
#endif
}

#define SCANTS_BOUQUET_OPTION_COUNT 3
const CMenuOptionChooser::keyval SCANTS_BOUQUET_OPTIONS[SCANTS_BOUQUET_OPTION_COUNT] =
{
	{ CZapitClient::BM_DELETEBOUQUETS        , LOCALE_SCANTS_BOUQUET_ERASE     },
	/*{ CZapitClient::BM_CREATEBOUQUETS        , LOCALE_SCANTS_BOUQUET_CREATE    },*/
	{ CZapitClient::BM_DONTTOUCHBOUQUETS     , LOCALE_SCANTS_BOUQUET_LEAVE     },
	{ CZapitClient::BM_UPDATEBOUQUETS        , LOCALE_SCANTS_BOUQUET_UPDATE    }
	/*{ CZapitClient::BM_CREATESATELLITEBOUQUET, LOCALE_SCANTS_BOUQUET_SATELLITE }*/
};

#define SCANTS_ZAPIT_SCANTYPE_COUNT 4
const CMenuOptionChooser::keyval SCANTS_ZAPIT_SCANTYPE[SCANTS_ZAPIT_SCANTYPE_COUNT] =
{
	{  CZapitClient::ST_TVRADIO	, LOCALE_ZAPIT_SCANTYPE_TVRADIO     },
	{  CZapitClient::ST_TV		, LOCALE_ZAPIT_SCANTYPE_TV    },
	{  CZapitClient::ST_RADIO	, LOCALE_ZAPIT_SCANTYPE_RADIO     },
	{  CZapitClient::ST_ALL		, LOCALE_ZAPIT_SCANTYPE_ALL }
};

#define SATSETUP_DISEQC_OPTION_COUNT 6
const CMenuOptionChooser::keyval SATSETUP_DISEQC_OPTIONS[SATSETUP_DISEQC_OPTION_COUNT] =
{
	{ NO_DISEQC,		LOCALE_SATSETUP_NODISEQC },
	{ MINI_DISEQC,		LOCALE_SATSETUP_MINIDISEQC },
	{ DISEQC_1_0,		LOCALE_SATSETUP_DISEQC10 },
	{ DISEQC_1_1,		LOCALE_SATSETUP_DISEQC11 },
	/*{ DISEQC_1_2,	LOCALE_SATSETUP_DISEQC12 },*/
	{ DISEQC_ADVANCED,	LOCALE_SATSETUP_DISEQ_ADVANCED },
	{ SMATV_REMOTE_TUNING,	LOCALE_SATSETUP_SMATVREMOTE }
};

#define SATSETUP_SCANTP_FEC_COUNT 21
#define CABLESETUP_SCANTP_FEC_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_FEC[SATSETUP_SCANTP_FEC_COUNT] =
{
	{ FEC_AUTO, LOCALE_SCANTP_FEC_AUTO },
	{ FEC_S2_AUTO, LOCALE_SCANTP_FEC_AUTO_S2 },

	{ FEC_1_2, LOCALE_SCANTP_FEC_1_2 },
	{ FEC_2_3, LOCALE_SCANTP_FEC_2_3 },
	{ FEC_3_4, LOCALE_SCANTP_FEC_3_4 },
	{ FEC_5_6, LOCALE_SCANTP_FEC_5_6 },
	{ FEC_7_8, LOCALE_SCANTP_FEC_7_8 },

	{ FEC_S2_QPSK_1_2, LOCALE_FEC_S2_QPSK_1_2 },
	{ FEC_S2_QPSK_2_3, LOCALE_FEC_S2_QPSK_2_3 },
	{ FEC_S2_QPSK_3_4, LOCALE_FEC_S2_QPSK_3_4 },
	{ FEC_S2_QPSK_5_6, LOCALE_FEC_S2_QPSK_5_6 },
	//{ FEC_S2_QPSK_7_8, LOCALE_FEC_S2_QPSK_7_8 },
	{ FEC_S2_QPSK_8_9, LOCALE_FEC_S2_QPSK_8_9 },
	{ FEC_S2_QPSK_3_5, LOCALE_FEC_S2_QPSK_3_5 },
	{ FEC_S2_QPSK_4_5, LOCALE_FEC_S2_QPSK_4_5 },
	{ FEC_S2_QPSK_9_10, LOCALE_FEC_S2_QPSK_9_10 },

	//{ FEC_S2_8PSK_1_2, LOCALE_FEC_S2_8PSK_1_2 },
	{ FEC_S2_8PSK_2_3, LOCALE_FEC_S2_8PSK_2_3 },
	{ FEC_S2_8PSK_3_4, LOCALE_FEC_S2_8PSK_3_4 },
	{ FEC_S2_8PSK_3_5, LOCALE_FEC_S2_8PSK_3_5 },
	{ FEC_S2_8PSK_5_6, LOCALE_FEC_S2_8PSK_5_6 },
	//{ FEC_S2_8PSK_7_8, LOCALE_FEC_S2_8PSK_7_8 },
	{ FEC_S2_8PSK_8_9, LOCALE_FEC_S2_8PSK_8_9 },
	//{ FEC_S2_8PSK_4_5, LOCALE_FEC_S2_8PSK_4_5 },
	{ FEC_S2_8PSK_9_10, LOCALE_FEC_S2_8PSK_9_10 }
};

#define SATSETUP_SCANTP_MOD_COUNT 5
const CMenuOptionChooser::keyval SATSETUP_SCANTP_MOD[SATSETUP_SCANTP_MOD_COUNT] =
{
	{ 1, LOCALE_SCANTP_MOD_16 },
	{ 2, LOCALE_SCANTP_MOD_32 },
	{ 3, LOCALE_SCANTP_MOD_64 },
	{ 4, LOCALE_SCANTP_MOD_128},
	{ 5, LOCALE_SCANTP_MOD_256}
};
#define SATSETUP_SCANTP_POL_COUNT 2
const CMenuOptionChooser::keyval SATSETUP_SCANTP_POL[SATSETUP_SCANTP_POL_COUNT] =
{
	{ 0, LOCALE_EXTRA_POL_H },
	{ 1, LOCALE_EXTRA_POL_V }
};

#if 0
/*Cable*/
#define CABLESETUP_SCANTP_MOD_COUNT 7
const CMenuOptionChooser::keyval CABLESETUP_SCANTP_MOD[CABLESETUP_SCANTP_MOD_COUNT] =
{
	{0, LOCALE_SCANTP_MOD_QPSK     } ,
	{1, LOCALE_SCANTP_MOD_QAM_16   } ,
	{2, LOCALE_SCANTP_MOD_QAM_32   } ,
	{3, LOCALE_SCANTP_MOD_QAM_64   } ,
	{4, LOCALE_SCANTP_MOD_QAM_128  } ,
	{5, LOCALE_SCANTP_MOD_QAM_256  } ,
	{6, LOCALE_SCANTP_MOD_QAM_AUTO }
};
#endif

#define SECTIONSD_SCAN_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval SECTIONSD_SCAN_OPTIONS[SECTIONSD_SCAN_OPTIONS_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON  },
	{ 2, LOCALE_OPTIONS_ON_WITHOUT_MESSAGES  }
};
#define DISEQC_ORDER_OPTION_COUNT 2
const CMenuOptionChooser::keyval DISEQC_ORDER_OPTIONS[DISEQC_ORDER_OPTION_COUNT] =
{
	{ COMMITED_FIRST, LOCALE_SATSETUP_DISEQC_COM_UNCOM },
	{ UNCOMMITED_FIRST, LOCALE_SATSETUP_DISEQC_UNCOM_COM  }
};

class CTPSelectHandler : public CMenuTarget
{
public:
	int exec(CMenuTarget* parent,  const std::string &actionkey);
};

extern CZapitChannel *channel;
extern std::map<transponder_id_t, transponder> select_transponders;
int CTPSelectHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
{
	transponder_list_t::iterator tI;
	sat_iterator_t sit;
	t_satellite_position position = 0;
	std::map<int, transponder> tmplist;
	std::map<int, transponder>::iterator tmpI;
	int i;
	char cnt[5];
	int select = -1;
	static int old_selected = 0;
	static t_satellite_position old_position = 0;

	if (parent)
		parent->hide();

	for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		if (!strcmp(sit->second.name.c_str(), CNeutrinoApp::getInstance()->getScanSettings().satNameNoDiseqc)) {
			position = sit->first;
			break;
		}
	}
	if (old_position != position) {
		old_selected = 0;
		old_position = position;
	}

	CMenuWidget* menu = new CMenuWidget(LOCALE_SCANTS_SELECT_TP, NEUTRINO_ICON_SETTINGS);
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
	i = 0;
	for (tI = select_transponders.begin(); tI != select_transponders.end(); tI++) {
		t_satellite_position satpos = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
		if (GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
			satpos = -satpos;
		if (satpos != position)
			continue;

		char buf[128];
		sprintf(cnt, "%d", i);
		char * f, *s, *m;
		switch (frontend->getInfo()->type) {
		case FE_QPSK:
			frontend->getDelSys(tI->second.feparams.u.qpsk.fec_inner, dvbs_get_modulation(tI->second.feparams.u.qpsk.fec_inner),  f, s, m);
			snprintf(buf, sizeof(buf), "%d %c %d %s %s %s ", tI->second.feparams.frequency/1000, tI->second.polarization ? 'V' : 'H', tI->second.feparams.u.qpsk.symbol_rate/1000, f, s, m);
			break;
		case FE_QAM:
			frontend->getDelSys(tI->second.feparams.u.qam.fec_inner, tI->second.feparams.u.qam.modulation, f, s, m);
			snprintf(buf, sizeof(buf), "%d %d %s %s %s ", tI->second.feparams.frequency/1000, tI->second.feparams.u.qam.symbol_rate/1000, f, s, m);
			break;
		case FE_OFDM:
		case FE_ATSC:
			break;
		}
		if(!old_selected && channel && channel->getSatellitePosition() == position) {
			if(channel->getFreqId() == GET_FREQ_FROM_TPID(tI->first)) {
				old_selected = i;
			}
		}

		//menu->addItem(new CMenuForwarderNonLocalized(buf, true, NULL, selector, cnt), old_selected == i);
		menu->addItem(new CMenuForwarderNonLocalized(buf, true, NULL, selector, cnt), false);
		tmplist.insert(std::pair <int, transponder>(i, tI->second));
		i++;
	}
	if (i == 0) {
		char text[255];
		sprintf(text, "No transponders found for %s\n", CNeutrinoApp::getInstance()->getScanSettings().satNameNoDiseqc);
		ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, text, 450, 2);
		return menu_return::RETURN_REPAINT;
	}
	menu->setSelected(old_selected);
	int retval = menu->exec(NULL, "");
	delete menu;
	delete selector;
	if (select >= 0) {
		old_selected = select;

		tmpI = tmplist.find(select);
		printf("CTPSelectHandler::exec: selected TP: freq %d pol %d SR %d\n", tmpI->second.feparams.frequency,
		       tmpI->second.polarization, tmpI->second.feparams.u.qpsk.symbol_rate);
		sprintf(get_set.TP_freq, "%d", tmpI->second.feparams.frequency);
		switch (frontend->getInfo()->type) {
		case FE_QPSK:
			sprintf(get_set.TP_rate, "%d", tmpI->second.feparams.u.qpsk.symbol_rate);
			get_set.TP_fec = tmpI->second.feparams.u.qpsk.fec_inner;
			get_set.TP_pol = tmpI->second.polarization;
			break;
		case FE_QAM:
			sprintf(get_set.TP_rate, "%d", tmpI->second.feparams.u.qam.symbol_rate);
			get_set.TP_fec = tmpI->second.feparams.u.qam.fec_inner;
			get_set.TP_mod = tmpI->second.feparams.u.qam.modulation;
			break;
		case FE_OFDM:
		case FE_ATSC:
			break;
		}

	}
	if (retval == menu_return::RETURN_EXIT_ALL)
		return menu_return::RETURN_EXIT_ALL;

	return menu_return::RETURN_REPAINT;
}

extern int scan_pids;

#define FAST_SCAN_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval FAST_SCAN_OPTIONS[FAST_SCAN_OPTIONS_COUNT] =
{
	{ FAST_SCAN_SD, LOCALE_SATSETUP_FASTSCAN_SD },
        { FAST_SCAN_HD, LOCALE_SATSETUP_FASTSCAN_HD  }
        /*{ FAST_SCAN_ALL, LOCALE_SATSETUP_FASTSCAN_ALL  }*/
};

#define FAST_SCAN_PROV_OPTIONS_COUNT 3
const CMenuOptionChooser::keyval FAST_SCAN_PROV_OPTIONS[FAST_SCAN_PROV_OPTIONS_COUNT] =
{
	{ OPERATOR_CD, LOCALE_SATSETUP_FASTSCAN_PROV_CD },
        { OPERATOR_TVV, LOCALE_SATSETUP_FASTSCAN_PROV_TVV  },
        { OPERATOR_TELESAT, LOCALE_SATSETUP_FASTSCAN_PROV_TELESAT  }
};

void CNeutrinoApp::InitScanSettings(CMenuWidget &settings)
{
	dprintf(DEBUG_DEBUG, "init scansettings\n");
	int sfound = 0;
	int dmode = scanSettings.diseqcMode;
	int shortcut = 1;
	static int all_usals = 1;

	CTPSelectHandler * tpSelect = new CTPSelectHandler();

	CSatelliteSetupNotifier * satNotify = new CSatelliteSetupNotifier();

	CMenuOptionChooser* ojScantype = new CMenuOptionChooser(LOCALE_ZAPIT_SCANTYPE, (int *)&scanSettings.scanType, SCANTS_ZAPIT_SCANTYPE, SCANTS_ZAPIT_SCANTYPE_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);
	CMenuOptionChooser* ojBouquets = new CMenuOptionChooser(LOCALE_SCANTS_BOUQUET, (int *)&scanSettings.bouquetMode, SCANTS_BOUQUET_OPTIONS, SCANTS_BOUQUET_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++), "", true);

	CMenuOptionChooser* useNit = new CMenuOptionChooser(LOCALE_SATSETUP_USE_NIT, (int *)&scanSettings.scan_mode, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
	CMenuOptionChooser* scanPids = new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SCANPIDS,  &scan_pids, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);

	// Please lets keep shortcuts which used for a long time - unchanged. -- focus
	//CMenuOptionChooser* ftaFlag = new CMenuOptionChooser(LOCALE_SATSETUP_USE_FTA_FLAG, (int *)&scanSettings.scan_fta_flag, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
	CMenuOptionChooser* ftaFlag = new CMenuOptionChooser(LOCALE_SATSETUP_USE_FTA_FLAG, (int *)&scanSettings.scan_fta_flag, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(0));

	CMenuWidget* satSetup = new CMenuWidget(LOCALE_SATSETUP_SAT_SETUP, NEUTRINO_ICON_SETTINGS);
	satSetup->addItem(GenericMenuSeparator);
	satSetup->addItem(GenericMenuBack);

	satSetup->addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savescansettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	satSetup->addItem(GenericMenuSeparatorLine);

	CMenuWidget* satfindMenu = new CMenuWidget(LOCALE_MOTORCONTROL_HEAD, NEUTRINO_ICON_SETTINGS);

	addMenueIntroItems(*satfindMenu);

	CMenuOptionStringChooser *satSelect = NULL;
	CMenuWidget* satOnOff = NULL;
	sat_iterator_t sit;

	//t_satellite_position currentSatellitePosition = frontend->getCurrentSatellitePosition();
	if (g_info.delivery_system == DVB_S) {
		satSelect = new CMenuOptionStringChooser(LOCALE_SATSETUP_SATELLITE, scanSettings.satNameNoDiseqc, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
		satOnOff = new CMenuWidget(LOCALE_SATSETUP_SATELLITE, NEUTRINO_ICON_SETTINGS);
		addMenueIntroItems(*satOnOff);

		for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			/* printf("Adding sat menu for %s position %d\n", sit->second.name.c_str(), sit->first); */

			satSelect->addOption(sit->second.name.c_str());
			if (strcmp(scanSettings.satNameNoDiseqc,sit->second.name.c_str()) == 0) {
				sfound = 1;
			}

			CMenuWidget* tempsat = new CMenuWidget(sit->second.name.c_str(), NEUTRINO_ICON_SETTINGS);
			addMenueIntroItems(*tempsat);

			CMenuOptionChooser * inuse = new CMenuOptionChooser(sit->second.name.c_str(),  &sit->second.use_in_scan, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
			CMenuOptionNumberChooser * diseqc = new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQC_INPUT, &sit->second.diseqc, ((dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED)), -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
			CMenuOptionNumberChooser * comm = new CMenuOptionNumberChooser(LOCALE_SATSETUP_COMM_INPUT, &sit->second.commited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
			CMenuOptionNumberChooser * uncomm = new CMenuOptionNumberChooser(LOCALE_SATSETUP_UNCOMM_INPUT, &sit->second.uncommited, dmode == DISEQC_ADVANCED, -1, 15, NULL, 1, -1, LOCALE_OPTIONS_OFF);
			//CMenuOptionNumberChooser * motor = new CMenuOptionNumberChooser(LOCALE_SATSETUP_MOTOR_POS, &sit->second.motor_position, dmode == DISEQC_ADVANCED, 0, 64, NULL, 0, 0, LOCALE_OPTIONS_OFF);
			CMenuOptionNumberChooser * motor = new CMenuOptionNumberChooser(LOCALE_SATSETUP_MOTOR_POS, &sit->second.motor_position, true, 0, 64, NULL, 0, 0, LOCALE_OPTIONS_OFF);
			//CMenuOptionChooser * usals = new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  &sit->second.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, dmode == DISEQC_ADVANCED);
			CMenuOptionChooser * usals = new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  &sit->second.use_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
			if(!sit->second.use_usals)
				all_usals = 0;

			satNotify->addItem(1, diseqc);
			satNotify->addItem(0, comm);
			satNotify->addItem(0, uncomm);
			//satNotify->addItem(0, motor); //FIXME testing motor with not DISEQC_ADVANCED
			//satNotify->addItem(0, usals);

			CIntInput* lofL = new CIntInput(LOCALE_SATSETUP_LOFL, (int&) sit->second.lnbOffsetLow, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
			CIntInput* lofH = new CIntInput(LOCALE_SATSETUP_LOFH, (int&) sit->second.lnbOffsetHigh, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);
			CIntInput* lofS = new CIntInput(LOCALE_SATSETUP_LOFS, (int&) sit->second.lnbSwitch, 5, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE);

			satOnOff->addItem(inuse);
			tempsat->addItem(diseqc);
			tempsat->addItem(comm);
			tempsat->addItem(uncomm);
			tempsat->addItem(motor);
			tempsat->addItem(usals);
			tempsat->addItem(new CMenuForwarder(LOCALE_SATSETUP_LOFL, true, lofL->getValue(), lofL));
			tempsat->addItem(new CMenuForwarder(LOCALE_SATSETUP_LOFH, true, lofH->getValue(), lofH));
			tempsat->addItem(new CMenuForwarder(LOCALE_SATSETUP_LOFS, true, lofS->getValue(), lofS));
			satSetup->addItem(new CMenuForwarderNonLocalized(sit->second.name.c_str(), true, NULL, tempsat));
		}
	} else if (g_info.delivery_system == DVB_C) {
		satSelect = new CMenuOptionStringChooser(LOCALE_CABLESETUP_PROVIDER, (char*)scanSettings.satNameNoDiseqc, true);
		for (sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
			printf("Adding cable menu for %s position %d\n", sit->second.name.c_str(), sit->first);
			satSelect->addOption(sit->second.name.c_str());
			if (strcmp(scanSettings.satNameNoDiseqc,sit->second.name.c_str()) == 0) {
				sfound = 1;
			}
			dprintf(DEBUG_DEBUG, "got scanprovider (cable): %s\n", sit->second.name.c_str());
		}
	}
	satfindMenu->addItem(satSelect);
	satfindMenu->addItem(new CMenuForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, tpSelect, "test"));

	int freq_length = (g_info.delivery_system == DVB_S) ? 8 : 6;

	CStringInput*		freq = new CStringInput(LOCALE_EXTRA_FREQ, (char *) scanSettings.TP_freq, freq_length, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CStringInput*		rate = new CStringInput(LOCALE_EXTRA_RATE, (char *) scanSettings.TP_rate, 8, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder *	Freq = new CMenuForwarder(LOCALE_EXTRA_FREQ, true, scanSettings.TP_freq, freq, "", CRCInput::convertDigitToKey(1));
	CMenuForwarder *	Rate = new CMenuForwarder(LOCALE_EXTRA_RATE, true, scanSettings.TP_rate, rate, "", CRCInput::convertDigitToKey(2));

	int fec_count = (g_info.delivery_system == DVB_S) ? SATSETUP_SCANTP_FEC_COUNT : CABLESETUP_SCANTP_FEC_COUNT;

	CMenuOptionChooser*	fec = new CMenuOptionChooser(LOCALE_EXTRA_FEC, (int *)&scanSettings.TP_fec, SATSETUP_SCANTP_FEC, fec_count, true, NULL, CRCInput::convertDigitToKey(3), "", true);
	CMenuOptionChooser*	mod_pol = NULL;

	if (g_info.delivery_system == DVB_S)
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_POL, (int *)&scanSettings.TP_pol, SATSETUP_SCANTP_POL, SATSETUP_SCANTP_POL_COUNT, true, NULL, CRCInput::convertDigitToKey(4));
	else if (g_info.delivery_system == DVB_C)
		mod_pol = new CMenuOptionChooser(LOCALE_EXTRA_MOD, (int *)&scanSettings.TP_mod, SATSETUP_SCANTP_MOD, SATSETUP_SCANTP_MOD_COUNT, true, NULL, CRCInput::convertDigitToKey(4));

	satfindMenu->addItem(Freq);
	satfindMenu->addItem(Rate);
	satfindMenu->addItem(fec);
	satfindMenu->addItem(mod_pol);

	CMenuWidget* motorMenu = NULL;
	if (g_info.delivery_system == DVB_S) {
		satfindMenu->addItem(new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, true, NULL, new CMotorControl(), "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

		motorMenu = new CMenuWidget(LOCALE_SATSETUP_EXTENDED_MOTOR, NEUTRINO_ICON_SETTINGS);
		motorMenu->addItem(GenericMenuSeparator);
		motorMenu->addItem(GenericMenuBack);
		motorMenu->addItem(new CMenuForwarder(LOCALE_SATSETUP_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
		//motorMenu->addItem(new CMenuForwarder(LOCALE_SATSETUP_MOTORCONTROL   , true, NULL, new CMotorControl()));
		motorMenu->addItem(new CMenuForwarder(LOCALE_MOTORCONTROL_HEAD, true, NULL, satfindMenu, "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
		motorMenu->addItem(GenericMenuSeparatorLine);

		motorMenu->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_ROTATION_SPEED, (int *)&zapitCfg.motorRotationSpeed, true, 0, 64, NULL) );
		motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_HVOLTAGE,  (int *)&zapitCfg.highVoltage, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		//motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_USE_GOTOXX,  (int *)&zapitCfg.useGotoXX, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

		CStringInput * toff;
		sprintf(zapit_lat, "%02.6f", zapitCfg.gotoXXLatitude);
		sprintf(zapit_long, "%02.6f", zapitCfg.gotoXXLongitude);

		motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_LADIR,  (int *)&zapitCfg.gotoXXLaDirection, OPTIONS_SOUTH0_NORTH1_OPTIONS, OPTIONS_SOUTH0_NORTH1_OPTION_COUNT, true));
		toff = new CStringInput(LOCALE_EXTRA_LAT, (char *) zapit_lat, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
		motorMenu->addItem(new CMenuForwarder(LOCALE_EXTRA_LAT, true, zapit_lat, toff));

		motorMenu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_LODIR,  (int *)&zapitCfg.gotoXXLoDirection, OPTIONS_EAST0_WEST1_OPTIONS, OPTIONS_EAST0_WEST1_OPTION_COUNT, true));
		toff = new CStringInput(LOCALE_EXTRA_LONG, (char *) zapit_long, 10, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789.");
		motorMenu->addItem(new CMenuForwarder(LOCALE_EXTRA_LONG, true, zapit_long, toff));
		motorMenu->addItem(new CMenuOptionNumberChooser(LOCALE_SATSETUP_USALS_REPEAT, (int *)&zapitCfg.repeatUsals, true, 0, 10, NULL, 0, 0, LOCALE_OPTIONS_OFF) );
		CAllUsalsNotifier * usalsNotify = new CAllUsalsNotifier();
		CMenuOptionChooser * allusals = new CMenuOptionChooser(LOCALE_SATSETUP_USE_USALS,  &all_usals, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, usalsNotify);
		motorMenu->addItem(allusals);
	}

	if (!sfound && satellitePositions.size()) {
		sit = satellitePositions.begin();
		strcpy(scanSettings.satNameNoDiseqc, sit->second.name.c_str());
	}

	CMenuWidget* manualScan = new CMenuWidget(LOCALE_SATSETUP_MANUAL_SCAN, NEUTRINO_ICON_SETTINGS);

	CScanTs * scanTs = new CScanTs();

	addMenueIntroItems(*manualScan);

	manualScan->addItem(satSelect);
	manualScan->addItem(new CMenuForwarder(LOCALE_SCANTS_SELECT_TP, true, NULL, tpSelect, "test"));
	manualScan->addItem(Freq);
	manualScan->addItem(Rate);
	manualScan->addItem(fec);
	manualScan->addItem(mod_pol);
	manualScan->addItem(useNit);
	manualScan->addItem(ftaFlag);
	manualScan->addItem(GenericMenuSeparatorLine);
	manualScan->addItem(new CMenuForwarder(LOCALE_SCANTS_TEST, true, NULL, scanTs, "test", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	manualScan->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "manual", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	CMenuWidget* autoScan = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*autoScan);
	autoScan->addItem(satSelect);
	autoScan->addItem(useNit);
	autoScan->addItem(scanPids);
	autoScan->addItem(ftaFlag);
	autoScan->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "auto", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	CMenuOptionChooser* ojDiseqc = NULL;
	CMenuOptionNumberChooser * ojDiseqcRepeats = NULL;
	CMenuForwarder *fsatSetup = NULL;
	CMenuForwarder *fmotorMenu = NULL;
	CMenuForwarder *fautoScanAll = NULL;

	if (g_info.delivery_system == DVB_S) {
		ojDiseqc	= new CMenuOptionChooser(LOCALE_SATSETUP_DISEQC, (int *)&scanSettings.diseqcMode, SATSETUP_DISEQC_OPTIONS, SATSETUP_DISEQC_OPTION_COUNT, true, satNotify, CRCInput::convertDigitToKey(shortcut++), "", true);
		ojDiseqcRepeats	= new CMenuOptionNumberChooser(LOCALE_SATSETUP_DISEQCREPEAT, (int *)&scanSettings.diseqcRepeat, (dmode != NO_DISEQC) && (dmode != DISEQC_ADVANCED), 0, 2, NULL);

		satNotify->addItem(1, ojDiseqcRepeats);

		fsatSetup	= new CMenuForwarder(LOCALE_SATSETUP_SAT_SETUP, true, NULL, satSetup, "", CRCInput::convertDigitToKey(shortcut++));
		//fmotorMenu	= new CMenuForwarder(LOCALE_SATSETUP_EXTENDED_MOTOR, (dmode == DISEQC_ADVANCED), NULL, motorMenu, "", CRCInput::convertDigitToKey(shortcut++));
		//satNotify->addItem(0, fmotorMenu); //FIXME testing motor with not DISEQC_ADVANCED
		fmotorMenu	= new CMenuForwarder(LOCALE_SATSETUP_EXTENDED_MOTOR, true, NULL, motorMenu, "", CRCInput::convertDigitToKey(shortcut++));

		CMenuWidget* autoScanAll = new CMenuWidget(LOCALE_SATSETUP_AUTO_SCAN_ALL, NEUTRINO_ICON_SETTINGS);
		fautoScanAll	= new CMenuForwarder(LOCALE_SATSETUP_AUTO_SCAN_ALL, (dmode != NO_DISEQC), NULL, autoScanAll, "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE);
		satNotify->addItem(2, fautoScanAll);


		addMenueIntroItems(*autoScanAll);
		autoScanAll->addItem(new CMenuForwarder(LOCALE_SATSETUP_SATELLITE, true, NULL, satOnOff, "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
		autoScanAll->addItem(useNit);
		autoScanAll->addItem(scanPids);
		autoScanAll->addItem(ftaFlag);
		autoScanAll->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "all", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
	}

	settings.addItem(GenericMenuSeparator);
	settings.addItem(GenericMenuBack);
	settings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	settings.addItem(GenericMenuSeparatorLine);

	settings.addItem(ojScantype);
	settings.addItem(ojBouquets);

	if (g_info.delivery_system == DVB_S) {
		settings.addItem(ojDiseqc);
		settings.addItem(ojDiseqcRepeats);
		settings.addItem(fsatSetup);
		settings.addItem(fmotorMenu);
	}
	settings.addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 6, 100) );

	settings.addItem(new CMenuForwarder(LOCALE_SATSETUP_MANUAL_SCAN, true, NULL, manualScan, "", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	settings.addItem(new CMenuForwarder(LOCALE_SATSETUP_AUTO_SCAN, true, NULL, autoScan, "", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));

	if (g_info.delivery_system == DVB_S) {
		settings.addItem(fautoScanAll);

		CMenuWidget* fastScanMenu = new CMenuWidget(LOCALE_SATSETUP_FASTSCAN_HEAD, NEUTRINO_ICON_SETTINGS);
		addMenueIntroItems(*fastScanMenu);

		CMenuOptionChooser* fastProv = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_PROV, (int *)&scanSettings.fast_op, FAST_SCAN_PROV_OPTIONS, FAST_SCAN_PROV_OPTIONS_COUNT, true, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED, true);
		CMenuOptionChooser* fastType = new CMenuOptionChooser(LOCALE_SATSETUP_FASTSCAN_TYPE, (int *)&scanSettings.fast_type, FAST_SCAN_OPTIONS, FAST_SCAN_OPTIONS_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN, true);

		fastScanMenu->addItem(fastProv);
		fastScanMenu->addItem(fastType);
		fastScanMenu->addItem(new CMenuForwarder(LOCALE_SCANTS_STARTNOW, true, NULL, scanTs, "fast", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
		settings.addItem(new CMenuForwarder(LOCALE_SATSETUP_FASTSCAN_HEAD, true, NULL, fastScanMenu, "", CRCInput::convertDigitToKey(0)));
	}
}

#define FLASHUPDATE_UPDATEMODE_OPTION_COUNT 2
const CMenuOptionChooser::keyval FLASHUPDATE_UPDATEMODE_OPTIONS[FLASHUPDATE_UPDATEMODE_OPTION_COUNT] =
{
	{ 0, LOCALE_FLASHUPDATE_UPDATEMODE_MANUAL   },
	{ 1, LOCALE_FLASHUPDATE_UPDATEMODE_INTERNET }
};

void getZapitConfig(Zapit_config *Cfg);
void CNeutrinoApp::InitServiceSettings(CMenuWidget &service, CMenuWidget &_scanSettings)
{
	dprintf(DEBUG_DEBUG, "init serviceSettings\n");
	getZapitConfig(&zapitCfg);

#if 0
	CZapitDestExec * zexec = new CZapitDestExec();

	CMenuWidget* zapit_menu = new CMenuWidget(LOCALE_EXTRA_ZAPIT_MENU, NEUTRINO_ICON_SETTINGS);
	zapit_menu->addItem( GenericMenuBack );
	//zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVESETTINGS, true, "", new CZapitDestExec, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_SAVESETTINGS, true, "", this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	zapit_menu->addItem( GenericMenuSeparatorLine );

	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_MAKE_BOUQUET, (int *)&zapitCfg.makeRemainingChannelsBouquet, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SAVE_LAST, (int *)&zapitCfg.saveLastChannel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_WRITE_NAMES, (int *)&zapitCfg.writeChannelsNames, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SORTNAMES,  (int *)&zapitCfg.sortNames, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

#if 0
	zapit_menu->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_ZAPIT_TIMEOUT, (int *)&zapitCfg.feTimeout, true, 0, 100) );
#else
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_FASTZAP,  (int *)&zapitCfg.fastZap, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
#endif
	zapit_menu->addItem( new CMenuOptionChooser(LOCALE_ZAPIT_SCANSDT, (int *)&zapitCfg.scanSDT, SECTIONSD_SCAN_OPTIONS, SECTIONSD_SCAN_OPTIONS_COUNT, true));

	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_SCANPIDS,  (int *)&zapitCfg.scanPids, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	zapit_menu->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAPIT_HVOLTAGE,  (int *)&zapitCfg.highVoltage, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	zapit_menu->addItem( GenericMenuSeparatorLine );
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_DELETE, true, "", zexec /*new CZapitDestExec*/, "delete"));
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_BACKUP, true, "", zexec /*new CZapitDestExec*/, "backup"));
	zapit_menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_RESTORE, true, "", zexec /*new CZapitDestExec*/, "restore"));
#endif

	addMenueIntroItems(service);
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_SCANTS    , true, NULL, &_scanSettings, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) );
	// service.addItem(new CMenuForwarder(LOCALE_EXTRA_ZAPIT_MENU      , true, NULL, zapit_menu, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_RELOAD    , true, NULL, this, "reloadchannels", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	service.addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_NAME    , true, NULL, new CBEBouquetWidget(), NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE ));

	CDataResetNotifier * resetNotifier = new CDataResetNotifier();
	service.addItem(new CMenuForwarder(LOCALE_RESET_CHANNELS    , true, NULL, resetNotifier, "channels", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN ));
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_GETPLUGINS, true, NULL, this, "reloadplugins"));

	service.addItem(GenericMenuSeparatorLine);
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_IMAGEINFO,  true, NULL, new CImageInfo(), NULL, CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL ), false);
	service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_RESTART   , true, NULL, this, "restart", CRCInput::RC_standby, NEUTRINO_ICON_BUTTON_POWER));
	//softupdate
	//if(softupdate)
	{
		dprintf(DEBUG_DEBUG, "init soft-update-stuff\n");
		CMenuWidget* updateSettings = new CMenuWidget(LOCALE_SERVICEMENU_UPDATE, NEUTRINO_ICON_UPDATE);
		addMenueIntroItems(*updateSettings);

		// expert-functions to read/write mtd
		CMenuWidget* mtdexpert = new CMenuWidget(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, NEUTRINO_ICON_UPDATE);
		addMenueIntroItems(*mtdexpert);
		CFlashExpert* fe = new CFlashExpert();

		//mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_READFLASH    , true, NULL, fe, "readflash"    ));
		//if(softupdate)
		//	mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_WRITEFLASH   , true, NULL, fe, "writeflash"   ));
		//mtdexpert->addItem(GenericMenuSeparatorLine);
		mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_READFLASHMTD , true, NULL, fe, "readflashmtd" ));
		if (softupdate)
			mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_WRITEFLASHMTD, true, NULL, fe, "writeflashmtd"));
		mtdexpert->addItem(GenericMenuSeparatorLine);

		CStringInputSMS * updateSettings_url_file = new CStringInputSMS(LOCALE_FLASHUPDATE_URL_FILE, g_settings.softupdate_url_file, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-. ");
		mtdexpert->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_URL_FILE, true, g_settings.softupdate_url_file, updateSettings_url_file));

		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_EXPERTFUNCTIONS, true, NULL, mtdexpert, "", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

		updateSettings->addItem(GenericMenuSeparatorLine);
		CMenuOptionChooser *oj = new CMenuOptionChooser(LOCALE_FLASHUPDATE_UPDATEMODE, &g_settings.softupdate_mode, FLASHUPDATE_UPDATEMODE_OPTIONS, FLASHUPDATE_UPDATEMODE_OPTION_COUNT, true, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);
		updateSettings->addItem( oj );
		updateSettings->addItem( new CMenuForwarder(LOCALE_EXTRA_UPDATE_DIR, true, g_settings.update_dir , this, "update_dir", CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW) );

#if 0 // image info and update list show current version, probably ehough
		/* show current version */
		updateSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FLASHUPDATE_CURRENTVERSION_SEP));

		/* get current version SBBBYYYYMMTTHHMM -- formatsting */
		CConfigFile _configfile('\t');

		const char * versionString = (_configfile.loadConfig("/.version")) ? (_configfile.getString( "version", "????????????????").c_str()) : "????????????????";

		dprintf(DEBUG_INFO, "current flash-version: %s\n", versionString);

		static CFlashVersionInfo versionInfo(versionString);

		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTVERSIONDATE    , false, versionInfo.getDate()));
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTVERSIONTIME    , false, versionInfo.getTime()));
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTRELEASECYCLE   , false, versionInfo.getReleaseCycle()));
		/* versionInfo.getType() returns const char * which is never deallocated */
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CURRENTVERSIONSNAPSHOT, false, versionInfo.getType()));
#endif
		updateSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FLASHUPDATE_PROXYSERVER_SEP));

		CStringInputSMS * updateSettings_proxy = new CStringInputSMS(LOCALE_FLASHUPDATE_PROXYSERVER, g_settings.softupdate_proxyserver, 23, LOCALE_FLASHUPDATE_PROXYSERVER_HINT1, LOCALE_FLASHUPDATE_PROXYSERVER_HINT2, "abcdefghijklmnopqrstuvwxyz0123456789-.: ");
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_PROXYSERVER, true, g_settings.softupdate_proxyserver, updateSettings_proxy));

		CStringInputSMS * updateSettings_proxyuser = new CStringInputSMS(LOCALE_FLASHUPDATE_PROXYUSERNAME, g_settings.softupdate_proxyusername, 23, LOCALE_FLASHUPDATE_PROXYUSERNAME_HINT1, LOCALE_FLASHUPDATE_PROXYUSERNAME_HINT2, "abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-. ");
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_PROXYUSERNAME, true, g_settings.softupdate_proxyusername, updateSettings_proxyuser));

		CStringInputSMS * updateSettings_proxypass = new CStringInputSMS(LOCALE_FLASHUPDATE_PROXYPASSWORD, g_settings.softupdate_proxypassword, 20, LOCALE_FLASHUPDATE_PROXYPASSWORD_HINT1, LOCALE_FLASHUPDATE_PROXYPASSWORD_HINT2, "abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-. ");
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_PROXYPASSWORD, true, g_settings.softupdate_proxypassword, updateSettings_proxypass));

		updateSettings->addItem(GenericMenuSeparatorLine);
		updateSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_CHECKUPDATE, true, NULL, new CFlashUpdate(), "", CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

		service.addItem(new CMenuForwarder(LOCALE_SERVICEMENU_UPDATE, true, NULL, updateSettings));
	}
}

#define MESSAGEBOX_NO_YES_OPTION_COUNT 2
const CMenuOptionChooser::keyval MESSAGEBOX_NO_YES_OPTIONS[MESSAGEBOX_NO_YES_OPTION_COUNT] =
{
	{ 0, LOCALE_MESSAGEBOX_NO  },
	{ 1, LOCALE_MESSAGEBOX_YES }
};

#define PICTUREVIEWER_SCALING_OPTION_COUNT 3
const CMenuOptionChooser::keyval PICTUREVIEWER_SCALING_OPTIONS[PICTUREVIEWER_SCALING_OPTION_COUNT] =
{
	{ CPictureViewer::SIMPLE, LOCALE_PICTUREVIEWER_RESIZE_SIMPLE        },
	{ CPictureViewer::COLOR , LOCALE_PICTUREVIEWER_RESIZE_COLOR_AVERAGE },
	{ CPictureViewer::NONE  , LOCALE_PICTUREVIEWER_RESIZE_NONE          }
};

#define AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT 2
const CMenuOptionChooser::keyval AUDIOPLAYER_DISPLAY_ORDER_OPTIONS[AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT] =
{
	{ CAudioPlayerGui::ARTIST_TITLE, LOCALE_AUDIOPLAYER_ARTIST_TITLE },
	{ CAudioPlayerGui::TITLE_ARTIST, LOCALE_AUDIOPLAYER_TITLE_ARTIST }
};

void CNeutrinoApp::InitAudioplPicSettings(CMenuWidget &audioplPicSettings)
{
	audioplPicSettings.addItem(GenericMenuSeparator);
	audioplPicSettings.addItem(GenericMenuBack);

	audioplPicSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_PICTUREVIEWER_HEAD));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_PICTUREVIEWER_SCALING  , &g_settings.picviewer_scaling     , PICTUREVIEWER_SCALING_OPTIONS  , PICTUREVIEWER_SCALING_OPTION_COUNT  , true ));
	CStringInput * pic_timeout= new CStringInput(LOCALE_PICTUREVIEWER_SLIDE_TIME, g_settings.picviewer_slide_time, 2, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789 ");
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_PICTUREVIEWER_SLIDE_TIME, true, g_settings.picviewer_slide_time, pic_timeout));
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_PICTUREVIEWER_DEFDIR, true, g_settings.network_nfs_picturedir, this, "picturedir"));

	audioplPicSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOPLAYER_NAME));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_DISPLAY_ORDER, &g_settings.audioplayer_display     , AUDIOPLAYER_DISPLAY_ORDER_OPTIONS, AUDIOPLAYER_DISPLAY_ORDER_OPTION_COUNT, true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_FOLLOW       , &g_settings.audioplayer_follow      , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SELECT_TITLE_BY_NAME       , &g_settings.audioplayer_select_title_by_name      , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_REPEAT_ON       , &g_settings.audioplayer_repeat_on      , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SHOW_PLAYLIST, &g_settings.audioplayer_show_playlist, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true ));

	CStringInput * audio_screensaver= new CStringInput(LOCALE_AUDIOPLAYER_SCREENSAVER_TIMEOUT, g_settings.audioplayer_screensaver, 2, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789 ");
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_AUDIOPLAYER_SCREENSAVER_TIMEOUT, true, g_settings.audioplayer_screensaver, audio_screensaver));

	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_HIGHPRIO     , &g_settings.audioplayer_highprio    , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));

	if (CVFD::getInstance()->has_lcd) //FIXME
		audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_SPECTRUM     , &g_settings.spectrum    , MESSAGEBOX_NO_YES_OPTIONS      , MESSAGEBOX_NO_YES_OPTION_COUNT      , true ));
	audioplPicSettings.addItem(new CMenuForwarder(LOCALE_AUDIOPLAYER_DEFDIR, true, g_settings.network_nfs_audioplayerdir, this, "audioplayerdir"));
	audioplPicSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOPLAYER_ENABLE_SC_METADATA, &g_settings.audioplayer_enable_sc_metadata, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true ));

}

#define MISCSETTINGS_FB_DESTINATION_OPTION_COUNT 3
const CMenuOptionChooser::keyval MISCSETTINGS_FB_DESTINATION_OPTIONS[MISCSETTINGS_FB_DESTINATION_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_NULL   },
	{ 1, LOCALE_OPTIONS_SERIAL },
	{ 2, LOCALE_OPTIONS_FB     }
};

#define MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTION_COUNT 2
const CMenuOptionChooser::keyval MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTIONS[MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTION_COUNT] =
{
	{ 0, LOCALE_FILESYSTEM_IS_UTF8_OPTION_ISO8859_1 },
	{ 1, LOCALE_FILESYSTEM_IS_UTF8_OPTION_UTF8      }
};

#define INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT 5
const CMenuOptionChooser::keyval  INFOBAR_SUBCHAN_DISP_POS_OPTIONS[INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_SETTINGS_POS_TOP_RIGHT },
	{ 1 , LOCALE_SETTINGS_POS_TOP_LEFT },
	{ 2 , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
	{ 3 , LOCALE_SETTINGS_POS_BOTTOM_RIGHT },
	{ 4 , LOCALE_INFOVIEWER_SUBCHAN_INFOBAR }
};

#define CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval  CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS[CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_CHANNELLIST_EPGTEXT_ALIGN_LEFT },
	{ 1 , LOCALE_CHANNELLIST_EPGTEXT_ALIGN_RIGHT }
};

#define CPU_FREQ_OPTION_COUNT 13
const CMenuOptionChooser::keyval_ext CPU_FREQ_OPTIONS[CPU_FREQ_OPTION_COUNT] =
{
	{ 0, LOCALE_CPU_FREQ_DEFAULT, NULL },
	{ 50, NONEXISTANT_LOCALE,  "50 Mhz"},
	{ 100, NONEXISTANT_LOCALE, "100 Mhz"},
	{ 150, NONEXISTANT_LOCALE, "150 Mhz"},
	{ 200, NONEXISTANT_LOCALE, "200 Mhz"},
	{ 250, NONEXISTANT_LOCALE, "250 Mhz"},
	{ 300, NONEXISTANT_LOCALE, "300 Mhz"},
	{ 350, NONEXISTANT_LOCALE, "350 Mhz"},
	{ 400, NONEXISTANT_LOCALE, "400 Mhz"},
	{ 450, NONEXISTANT_LOCALE, "450 Mhz"},
	{ 500, NONEXISTANT_LOCALE, "500 Mhz"},
	{ 550, NONEXISTANT_LOCALE, "550 Mhz"},
	{ 600, NONEXISTANT_LOCALE, "600 Mhz"},
};

#define LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT 4
const CMenuOptionChooser::keyval  LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS[LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT]=
{
   { 0 , LOCALE_MISCSETTINGS_INFOBAR_DISP_0 },
   { 1 , LOCALE_MISCSETTINGS_INFOBAR_DISP_1 },
   { 2 , LOCALE_MISCSETTINGS_INFOBAR_DISP_2 },
   { 3 , LOCALE_MISCSETTINGS_INFOBAR_DISP_3 }
};

#define VOLUMEBAR_DISP_POS_OPTIONS_COUNT 6
const CMenuOptionChooser::keyval  VOLUMEBAR_DISP_POS_OPTIONS[VOLUMEBAR_DISP_POS_OPTIONS_COUNT]=
{
	{ 0 , LOCALE_SETTINGS_POS_TOP_RIGHT },
	{ 1 , LOCALE_SETTINGS_POS_TOP_LEFT },
	{ 2 , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
	{ 3 , LOCALE_SETTINGS_POS_BOTTOM_RIGHT },
	{ 4 , LOCALE_SETTINGS_POS_DEFAULT_CENTER },
	{ 5 , LOCALE_SETTINGS_POS_HIGHER_CENTER }
};

#define MENU_DISP_POS_OPTIONS_COUNT 5
const CMenuOptionChooser::keyval  MENU_DISP_POS_OPTIONS[MENU_DISP_POS_OPTIONS_COUNT]=
{
        { 0 , LOCALE_SETTINGS_POS_DEFAULT_CENTER },
        { 1 , LOCALE_SETTINGS_POS_TOP_LEFT },
        { 2 , LOCALE_SETTINGS_POS_TOP_RIGHT },
        { 3 , LOCALE_SETTINGS_POS_BOTTOM_LEFT },
        { 4 , LOCALE_SETTINGS_POS_BOTTOM_RIGHT }
};

#define MENU_CORNERSETTINGS_TYPE_OPTION_COUNT 2
const CMenuOptionChooser::keyval MENU_CORNERSETTINGS_TYPE_OPTIONS[MENU_CORNERSETTINGS_TYPE_OPTION_COUNT] =
{
	{ 0, LOCALE_EXTRA_ROUNDED_CORNERS_OFF },
	{ 1, LOCALE_EXTRA_ROUNDED_CORNERS_ON  }
};

void CNeutrinoApp::InitMiscSettings(CMenuWidget &miscSettings)
{
	dprintf(DEBUG_DEBUG, "init miscsettings\n");
	addMenueIntroItems(miscSettings);
//	miscSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_SAVESETTINGSNOW, true, NULL, this, "savesettings", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	CMenuWidget *miscSettingsGeneral = new CMenuWidget(LOCALE_MISCSETTINGS_GENERAL, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*miscSettingsGeneral);
#if 0
	CMenuOptionChooser *m1 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL_RCDELAY, &g_settings.shutdown_real_rcdelay, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, !g_settings.shutdown_real);
	CMiscNotifier* miscNotifier = new CMiscNotifier( m1 );
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL, &g_settings.shutdown_real, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, miscNotifier));
#endif
	CStringInput * miscSettings_shutdown_count = new CStringInput(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, g_settings.shutdown_count, 3, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT1, LOCALE_MISCSETTINGS_SHUTDOWN_COUNT_HINT2, "0123456789 ");

	miscSettingsGeneral->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_SHUTDOWN_COUNT, true, g_settings.shutdown_count, miscSettings_shutdown_count));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_STARTSTANDBY, &g_settings.power_standby, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_CACHE_TXT,  (int *)&g_settings.cacheTXT, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	//miscSettings.addItem(new CMenuForwarder(LOCALE_EXTRA_KEY_PLUGIN, true, g_settings.onekey_plugin,this,"onekeyplugin"));
	
	//don't show rotor and fan speed settings on cable box
	funNotifier = new CFanControlNotifier();
	if (g_info.delivery_system == DVB_S) {
		miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ROTORSWAP, &g_settings.rotor_swap, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		miscSettingsGeneral->addItem(new CMenuOptionNumberChooser(LOCALE_FAN_SPEED, &g_settings.fan_speed, true, 1, 14, funNotifier, 0, 0, LOCALE_OPTIONS_OFF) );
		funNotifier->changeNotify(NONEXISTANT_LOCALE, (void*) &g_settings.fan_speed);
	}

#if 0
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_SCRAMBLED_MESSAGE, &g_settings.scrambled_message, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_INFOVIEWER_SUBCHAN_DISP_POS, &g_settings.infobar_subchan_disp_pos, INFOBAR_SUBCHAN_DISP_POS_OPTIONS, INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_VOLUME_POS, &g_settings.volume_pos, VOLUMEBAR_DISP_POS_OPTIONS, VOLUMEBAR_DISP_POS_OPTIONS_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_SETTINGS_MENU_POS, &g_settings.menu_pos, MENU_DISP_POS_OPTIONS, MENU_DISP_POS_OPTIONS_COUNT, true));
	miscSettingsGeneral->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ROUNDED_CORNERS, &g_settings.rounded_corners, MENU_CORNERSETTINGS_TYPE_OPTIONS, MENU_CORNERSETTINGS_TYPE_OPTION_COUNT, true));
#endif
	miscSettings.addItem( new CMenuForwarder(LOCALE_MISCSETTINGS_GENERAL, true, NULL, miscSettingsGeneral, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) );

	//channellist
	CMenuWidget *miscSettingsChannellist = new CMenuWidget(LOCALE_MISCSETTINGS_CHANNELLIST, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*miscSettingsChannellist);
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_CHANNELLIST_EPGTEXT_ALIGN, &g_settings.channellist_epgtext_align_right, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS, CHANNELLIST_EPGTEXT_ALIGN_RIGHT_OPTIONS_COUNT, true));
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_CHANNELLIST_EXTENDED, &g_settings.channellist_extended, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_CHANNELLIST_MAKE_HDLIST, &g_settings.make_hd_list, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsChannellist->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAP_CYCLE, &g_settings.zap_cycle, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettings.addItem( new CMenuForwarder(LOCALE_MISCSETTINGS_CHANNELLIST, true, NULL, miscSettingsChannellist, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW) );

	//EPGSettings
	CMenuWidget *miscSettingsEPGSettings = new CMenuWidget(LOCALE_MISCSETTINGS_EPG_HEAD, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*miscSettingsEPGSettings);
	CSectionsdConfigNotifier* sectionsdConfigNotifier = new CSectionsdConfigNotifier;
	miscSettingsEPGSettings->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_EPG_SAVE, &g_settings.epg_save, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	CStringInput * miscSettings_epg_cache = new CStringInput(LOCALE_MISCSETTINGS_EPG_CACHE, &g_settings.epg_cache, 2,LOCALE_MISCSETTINGS_EPG_CACHE_HINT1, LOCALE_MISCSETTINGS_EPG_CACHE_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_CACHE, true, g_settings.epg_cache, miscSettings_epg_cache));
	CStringInput * miscSettings_epg_cache_e = new CStringInput(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, &g_settings.epg_extendedcache, 3,LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT1, LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_EXTENDEDCACHE, true, g_settings.epg_extendedcache, miscSettings_epg_cache_e));
	CStringInput * miscSettings_epg_old_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, &g_settings.epg_old_events, 2,LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_OLD_EVENTS_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_OLD_EVENTS, true, g_settings.epg_old_events, miscSettings_epg_old_events));
	CStringInput * miscSettings_epg_max_events = new CStringInput(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, &g_settings.epg_max_events, 5,LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT1, LOCALE_MISCSETTINGS_EPG_MAX_EVENTS_HINT2 , "0123456789 ", sectionsdConfigNotifier);
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_MAX_EVENTS, true, g_settings.epg_max_events, miscSettings_epg_max_events));
	miscSettingsEPGSettings->addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_DIR, true, g_settings.epg_dir, this, "epgdir"));
	miscSettings.addItem( new CMenuForwarder(LOCALE_MISCSETTINGS_EPG_HEAD, true, NULL, miscSettingsEPGSettings, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN) );
	//end of EPGSettings

#if 0
	miscSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MISCSETTINGS_DRIVER_BOOT));
	if (access("/var/tuxbox/emlog", F_OK) == 0)
		g_settings.emlog = 1;
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_USELOG, &g_settings.emlog, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, new CLogChangeNotifier));
#endif

	//filebrowsersettings
	CMenuWidget *miscSettingsFilebrowser = new CMenuWidget(LOCALE_FILEBROWSER_HEAD, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*miscSettingsFilebrowser);
	miscSettingsFilebrowser->addItem(new CMenuOptionChooser(LOCALE_FILESYSTEM_IS_UTF8            , &g_settings.filesystem_is_utf8            , MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTIONS, MISCSETTINGS_FILESYSTEM_IS_UTF8_OPTION_COUNT, true ));
	miscSettingsFilebrowser->addItem(new CMenuOptionChooser(LOCALE_FILEBROWSER_SHOWRIGHTS        , &g_settings.filebrowser_showrights        , MESSAGEBOX_NO_YES_OPTIONS              , MESSAGEBOX_NO_YES_OPTION_COUNT              , true ));
	miscSettingsFilebrowser->addItem(new CMenuOptionChooser(LOCALE_FILEBROWSER_DENYDIRECTORYLEAVE, &g_settings.filebrowser_denydirectoryleave, MESSAGEBOX_NO_YES_OPTIONS              , MESSAGEBOX_NO_YES_OPTION_COUNT              , true ));
	miscSettings.addItem( new CMenuForwarder(LOCALE_FILEBROWSER_HEAD, true, NULL, miscSettingsFilebrowser, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE) );

#if 0
	//infobar
	CMenuWidget *miscSettingsInfobar = new CMenuWidget(LOCALE_MISCSETTINGS_INFOBAR, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*miscSettingsInfobar);
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_PROGRESSBAR_COLOR, &g_settings.progressbar_color, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY, &g_settings.casystem_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_DISP_LOG, &g_settings.infobar_show_channellogo, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_VIRTUAL_ZAP_MODE, &g_settings.virtual_zap_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SAT_DISPLAY, &g_settings.infobar_sat_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettings.addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR, true, NULL, miscSettingsInfobar, NULL, CRCInput::RC_1));
#endif
#if 0
	CCpuFreqNotifier * cpuNotifier = new CCpuFreqNotifier();
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_CPU_FREQ_NORMAL, &g_settings.cpufreq, CPU_FREQ_OPTIONS, CPU_FREQ_OPTION_COUNT, true, cpuNotifier));
	miscSettings.addItem(new CMenuOptionChooser(LOCALE_CPU_FREQ_STANDBY, &g_settings.standby_cpufreq, CPU_FREQ_OPTIONS, CPU_FREQ_OPTION_COUNT, true));
#endif
}

void sectionsd_set_languages(const std::vector<std::string>& newLanguages);

class CLangSelectNotifier : public CChangeObserver
{
public:
        bool changeNotify(const neutrino_locale_t, void * data);
};

bool CLangSelectNotifier::changeNotify(const neutrino_locale_t, void *)
{
	std::vector<std::string> languages;
	bool found = false;
	std::map<std::string, std::string>::const_iterator it;

	for(int i = 0; i < 3; i++) {
		if(strlen(g_settings.pref_lang[i])) {
			printf("setLanguages: %d: %s\n", i, g_settings.pref_lang[i]);
			std::string temp(g_settings.pref_lang[i]);
			for(it = iso639.begin(); it != iso639.end(); it++) {
				if(temp == it->second) {
					languages.push_back(it->first);
					printf("setLanguages: adding %s\n", it->first.c_str());
					found = true;
				}
			}
		}
	}
	if(found)
		sectionsd_set_languages(languages);

	return true;
}

void CNeutrinoApp::InitLanguageSettings(CMenuWidget &languageSettings)
{
	addMenueIntroItems(languageSettings);

	struct dirent **namelist;
	int n;
	//		printf("scanning locale dir now....(perhaps)\n");

	char *pfad[] = {(char *) DATADIR "/neutrino/locale",(char *) "/var/tuxbox/config/locale"};
	CMenuWidget * localeMenu = new CMenuWidget(LOCALE_LANGUAGESETUP_OSD, NEUTRINO_ICON_LANGUAGE);
	addMenueIntroItems(*localeMenu);

	for (int p = 0; p < 2; p++) {
		n = scandir(pfad[p], &namelist, 0, alphasort);
		if (n < 0) {
			perror("loading locales: scandir");
		} else {
			for (int count=0; count<n; count++) {
				char * locale = strdup(namelist[count]->d_name);
				char * pos = strstr(locale, ".locale");
				if (pos != NULL) {
					char iname[50];
					*pos = '\0';
					sprintf(iname, "%s", locale);
					CMenuOptionLanguageChooser* oj = new CMenuOptionLanguageChooser((char*)locale, this, iname);
					oj->addOption(locale);
					localeMenu->addItem( oj );
				} else
					free(locale);
				free(namelist[count]);
			}
			free(namelist);
		}
	}
	languageSettings.addItem(new CMenuForwarder(LOCALE_LANGUAGESETUP_OSD, true, NULL, localeMenu, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	xmlDocPtr parser = parseXmlFile("/etc/timezone.xml");

	CMenuOptionStringChooser* tzSelect;
	if (parser != NULL) {
		tzSelect = new CMenuOptionStringChooser(LOCALE_MAINSETTINGS_TIMEZONE, g_settings.timezone, true, new CTZChangeNotifier(), CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN, true);
		xmlNodePtr search = xmlDocGetRootElement(parser)->xmlChildrenNode;
		bool found = false;
		while (search) {
			if (!strcmp(xmlGetName(search), "zone")) {
				std::string name = xmlGetAttribute(search, "name");
				std::string zone = xmlGetAttribute(search, "zone");
				//printf("Timezone: %s -> %s\n", name.c_str(), zone.c_str());
				tzSelect->addOption(name.c_str());
				found = true;
			}
			search = search->xmlNextNode;
		}
		if (found)
			languageSettings.addItem(tzSelect);
		else {
			delete tzSelect;
			tzSelect = NULL;
		}
		xmlFreeDoc(parser);
	}

	CMenuWidget * prefMenu = new CMenuWidget(LOCALE_AUDIOMENU_PREF_LANGUAGES, NEUTRINO_ICON_LANGUAGE);

	prefMenu->addItem(GenericMenuSeparator);
	prefMenu->addItem(GenericMenuBack);
	prefMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_PREF_LANG_HEAD));

	prefMenu->addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_AUTO_LANG, &g_settings.auto_lang, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL));

	CLangSelectNotifier * langNotifier = new CLangSelectNotifier();
	for(int i = 0; i < 3; i++) {
		CMenuOptionStringChooser * langSelect = new CMenuOptionStringChooser(LOCALE_AUDIOMENU_PREF_LANG, g_settings.pref_lang[i], true, langNotifier, CRCInput::convertDigitToKey(i+1), "", true);
		std::map<std::string, std::string>::const_iterator it;
		for(it = iso639rev.begin(); it != iso639rev.end(); it++) {
			langSelect->addOption(it->first.c_str());
		}
		prefMenu->addItem(langSelect);
	}

	prefMenu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_PREF_SUBS_HEAD));
	prefMenu->addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_AUTO_SUBS, &g_settings.auto_subs, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL));
	for(int i = 0; i < 3; i++) {
		CMenuOptionStringChooser * langSelect = new CMenuOptionStringChooser(LOCALE_AUDIOMENU_PREF_SUBS, g_settings.pref_subs[i], true, NULL, CRCInput::convertDigitToKey(i+4), "", true);
		std::map<std::string, std::string>::const_iterator it;
		for(it = iso639rev.begin(); it != iso639rev.end(); it++) {
			langSelect->addOption(it->first.c_str());
		}
		prefMenu->addItem(langSelect);
	}

	languageSettings.addItem(new CMenuForwarder(LOCALE_AUDIOMENU_PREF_LANGUAGES, true, NULL, prefMenu, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	langNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
}

#define AUDIOMENU_ANALOGOUT_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_ANALOGOUT_OPTIONS[AUDIOMENU_ANALOGOUT_OPTION_COUNT] =
{
	{ 0, LOCALE_AUDIOMENU_STEREO    },
	{ 1, LOCALE_AUDIOMENU_MONOLEFT  },
	{ 2, LOCALE_AUDIOMENU_MONORIGHT }
};

#define AUDIOMENU_SRS_OPTION_COUNT 2
const CMenuOptionChooser::keyval AUDIOMENU_SRS_OPTIONS[AUDIOMENU_SRS_OPTION_COUNT] =
{
	{ 0 , LOCALE_SRS_ALGO_LIGHT },
	{ 1 , LOCALE_SRS_ALGO_NORMAL }
};

#define AUDIOMENU_AVSYNC_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_AVSYNC_OPTIONS[AUDIOMENU_AVSYNC_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON  },
	{ 2, LOCALE_AUDIOMENU_AVSYNC_AM }
};

#define AUDIOMENU_CLOCKREC_OPTION_COUNT 3
const CMenuOptionChooser::keyval AUDIOMENU_CLOCKREC_OPTIONS[AUDIOMENU_CLOCKREC_OPTION_COUNT] =
{
	{ 0, LOCALE_OPTIONS_OFF },
	{ 1, LOCALE_OPTIONS_ON  },
};

void CNeutrinoApp::InitAudioSettings(CMenuWidget &audioSettings, CAudioSetupNotifier* audioSetupNotifier)
{
	addMenueIntroItems(audioSettings);

	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_ANALOG_MODE, &g_settings.audio_AnalogMode, AUDIOMENU_ANALOGOUT_OPTIONS, AUDIOMENU_ANALOGOUT_OPTION_COUNT, true, audioSetupNotifier));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_HDMI_DD, &g_settings.hdmi_dd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, audioSetupNotifier));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_SPDIF_DD, &g_settings.spdif_dd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, audioSetupNotifier));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_AVSYNC, &g_settings.avsync, AUDIOMENU_AVSYNC_OPTIONS, AUDIOMENU_AVSYNC_OPTION_COUNT, true, audioSetupNotifier));
#if 0
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_CLOCKREC, &g_settings.clockrec, AUDIOMENU_CLOCKREC_OPTIONS, AUDIOMENU_CLOCKREC_OPTION_COUNT, true, audioSetupNotifier));
#endif
	audioSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_SRS_IQ));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_SRS_IQ, &g_settings.srs_enable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, audioSetupNotifier));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_SRS_ALGO, &g_settings.srs_algo, AUDIOMENU_SRS_OPTIONS, AUDIOMENU_SRS_OPTION_COUNT, true, audioSetupNotifier));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_SRS_NMGR, &g_settings.srs_nmgr_enable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, audioSetupNotifier));
	audioSettings.addItem(new CMenuOptionNumberChooser(LOCALE_SRS_VOLUME, &g_settings.srs_ref_volume, true, 1, 100, audioSetupNotifier));

	//audioSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_AUDIOMENU_PREF_LANG_HEAD));
	audioSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_DOLBYDIGITAL, &g_settings.audio_DolbyDigital, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, audioSetupNotifier));
#if 0
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_AUTO_LANG, &g_settings.auto_lang, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL));
	audioSettings.addItem(new CMenuOptionChooser(LOCALE_AUDIOMENU_AUTO_SUBS, &g_settings.auto_subs, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL));

	CLangSelectNotifier * langNotifier = new CLangSelectNotifier();
	for(int i = 0; i < 3; i++) {
		CMenuOptionStringChooser * langSelect = new CMenuOptionStringChooser(LOCALE_AUDIOMENU_PREF_LANG, g_settings.pref_lang[i], true, langNotifier, CRCInput::convertDigitToKey(i+1), "", true);
		std::map<std::string, std::string>::const_iterator it;
		for(it = iso639rev.begin(); it != iso639rev.end(); it++) {
			langSelect->addOption(it->first.c_str());
		}
		audioSettings.addItem(langSelect);
	}
	langNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
#endif
#if 0
	CStringInput * audio_PCMOffset = new CStringInput(LOCALE_AUDIOMENU_PCMOFFSET, g_settings.audio_PCMOffset, 2, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789 ", audioSetupNotifier);
	CMenuForwarder *mf = new CMenuForwarder(LOCALE_AUDIOMENU_PCMOFFSET, true, g_settings.audio_PCMOffset, audio_PCMOffset );
	audioSettings.addItem(mf);
#endif
}

#if 1
#define PARENTALLOCK_PROMPT_OPTION_COUNT 3
#else
#define PARENTALLOCK_PROMPT_OPTION_COUNT 4
#endif
const CMenuOptionChooser::keyval PARENTALLOCK_PROMPT_OPTIONS[PARENTALLOCK_PROMPT_OPTION_COUNT] =
{
	{ PARENTALLOCK_PROMPT_NEVER         , LOCALE_PARENTALLOCK_NEVER          },
#if 0
	{ PARENTALLOCK_PROMPT_ONSTART       , LOCALE_PARENTALLOCK_ONSTART        },
#endif
	{ PARENTALLOCK_PROMPT_CHANGETOLOCKED, LOCALE_PARENTALLOCK_CHANGETOLOCKED },
	{ PARENTALLOCK_PROMPT_ONSIGNAL      , LOCALE_PARENTALLOCK_ONSIGNAL       }
};

#define PARENTALLOCK_LOCKAGE_OPTION_COUNT 3
const CMenuOptionChooser::keyval PARENTALLOCK_LOCKAGE_OPTIONS[PARENTALLOCK_LOCKAGE_OPTION_COUNT] =
{
	{ 12, LOCALE_PARENTALLOCK_LOCKAGE12 },
	{ 16, LOCALE_PARENTALLOCK_LOCKAGE16 },
	{ 18, LOCALE_PARENTALLOCK_LOCKAGE18 }
};

void CNeutrinoApp::InitParentalLockSettings(CMenuWidget &parentallockSettings)
{
	addMenueIntroItems(parentallockSettings);

	parentallockSettings.addItem(new CMenuOptionChooser(LOCALE_PARENTALLOCK_PROMPT , &g_settings.parentallock_prompt , PARENTALLOCK_PROMPT_OPTIONS , PARENTALLOCK_PROMPT_OPTION_COUNT , !parentallocked));

	parentallockSettings.addItem(new CMenuOptionChooser(LOCALE_PARENTALLOCK_LOCKAGE, &g_settings.parentallock_lockage, PARENTALLOCK_LOCKAGE_OPTIONS, PARENTALLOCK_LOCKAGE_OPTION_COUNT, !parentallocked));

	CPINChangeWidget * pinChangeWidget = new CPINChangeWidget(LOCALE_PARENTALLOCK_CHANGEPIN, g_settings.parentallock_pincode, 4, LOCALE_PARENTALLOCK_CHANGEPIN_HINT1);
	parentallockSettings.addItem( new CMenuForwarder(LOCALE_PARENTALLOCK_CHANGEPIN, true, g_settings.parentallock_pincode, pinChangeWidget));
}

void CNeutrinoApp::InitNetworkSettings(CMenuWidget &networkSettings)
{
	CIPInput * networkSettings_NetworkIP  = new CIPInput(LOCALE_NETWORKMENU_IPADDRESS , networkConfig.address   , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, MyIPChanger);
	CIPInput * networkSettings_NetMask    = new CIPInput(LOCALE_NETWORKMENU_NETMASK   , networkConfig.netmask   , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CIPInput * networkSettings_Broadcast  = new CIPInput(LOCALE_NETWORKMENU_BROADCAST , networkConfig.broadcast , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CIPInput * networkSettings_Gateway    = new CIPInput(LOCALE_NETWORKMENU_GATEWAY   , networkConfig.gateway   , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CIPInput * networkSettings_NameServer = new CIPInput(LOCALE_NETWORKMENU_NAMESERVER, networkConfig.nameserver, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);

	CSectionsdConfigNotifier* sectionsdConfigNotifier = new CSectionsdConfigNotifier;
	CStringInputSMS * networkSettings_NtpServer = new CStringInputSMS(LOCALE_NETWORKMENU_NTPSERVER, &g_settings.network_ntpserver, 30, LOCALE_NETWORKMENU_NTPSERVER_HINT1, LOCALE_NETWORKMENU_NTPSERVER_HINT2, "abcdefghijklmnopqrstuvwxyz0123456789-. ", sectionsdConfigNotifier);

	CStringInput * networkSettings_NtpRefresh = new CStringInput(LOCALE_NETWORKMENU_NTPREFRESH, &g_settings.network_ntprefresh, 3,LOCALE_NETWORKMENU_NTPREFRESH_HINT1, LOCALE_NETWORKMENU_NTPREFRESH_HINT2 , "0123456789 ", sectionsdConfigNotifier);

	CStringInputSMS * networkSettings_Hostname = new CStringInputSMS(LOCALE_NETWORKMENU_HOSTNAME, &networkConfig.hostname, 30, LOCALE_NETWORKMENU_NTPSERVER_HINT1, LOCALE_NETWORKMENU_NTPSERVER_HINT2, "abcdefghijklmnopqrstuvwxyz0123456789-. ");

	CMenuForwarder *m0 = new CMenuForwarder(LOCALE_NETWORKMENU_SETUPNOW, true, NULL, this, "network", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	CMenuForwarder *m1 = new CMenuForwarder(LOCALE_NETWORKMENU_IPADDRESS , networkConfig.inet_static, networkConfig.address   , networkSettings_NetworkIP );
	CMenuForwarder *m2 = new CMenuForwarder(LOCALE_NETWORKMENU_NETMASK   , networkConfig.inet_static, networkConfig.netmask   , networkSettings_NetMask   );
	CMenuForwarder *m3 = new CMenuForwarder(LOCALE_NETWORKMENU_BROADCAST , networkConfig.inet_static, networkConfig.broadcast , networkSettings_Broadcast );
	CMenuForwarder *m4 = new CMenuForwarder(LOCALE_NETWORKMENU_GATEWAY   , networkConfig.inet_static, networkConfig.gateway   , networkSettings_Gateway   );
	CMenuForwarder *m5 = new CMenuForwarder(LOCALE_NETWORKMENU_NAMESERVER, networkConfig.inet_static, networkConfig.nameserver, networkSettings_NameServer);
	CMenuForwarder *m6 = new CMenuForwarder(LOCALE_NETWORKMENU_NTPSERVER, true , g_settings.network_ntpserver, networkSettings_NtpServer );
	CMenuForwarder *m7 = new CMenuForwarder(LOCALE_NETWORKMENU_NTPREFRESH, true , g_settings.network_ntprefresh, networkSettings_NtpRefresh );

	CMenuForwarder  *m8 = new CMenuForwarder(LOCALE_NETWORKMENU_HOSTNAME, !networkConfig.inet_static, networkConfig.hostname, networkSettings_Hostname);

	CDHCPNotifier* dhcpNotifier = new CDHCPNotifier(m1,m2,m3,m4,m5, m8);

	network_automatic_start = networkConfig.automatic_start ? 1 : 0;
	CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_NETWORKMENU_SETUPONSTARTUP, &network_automatic_start, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	addMenueIntroItems(networkSettings);

	networkSettings.addItem( oj );
	networkSettings.addItem(new CMenuForwarder(LOCALE_NETWORKMENU_TEST, true, NULL, this, "networktest"));
	networkSettings.addItem(new CMenuForwarder(LOCALE_NETWORKMENU_SHOW, true, NULL, this, "networkshow", CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL));
	networkSettings.addItem( m0 );

	networkSettings.addItem(GenericMenuSeparatorLine);


	network_dhcp = networkConfig.inet_static ? 0 : 1;
	oj = new CMenuOptionChooser(LOCALE_NETWORKMENU_DHCP, &network_dhcp, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, dhcpNotifier);
	networkSettings.addItem(oj);
	networkSettings.addItem(m8);
	networkSettings.addItem(GenericMenuSeparatorLine);

	networkSettings.addItem( m1);
	networkSettings.addItem( m2);
	networkSettings.addItem( m3);

	networkSettings.addItem(GenericMenuSeparatorLine);
	networkSettings.addItem( m4);
	networkSettings.addItem( m5);
	networkSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_NETWORKMENU_NTPTITLE));
	networkSettings.addItem(new CMenuOptionChooser(LOCALE_NETWORKMENU_NTPENABLE, &g_settings.network_ntpenable, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, sectionsdConfigNotifier));
	networkSettings.addItem( m6);
	networkSettings.addItem( m7);

	networkSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_NETWORKMENU_MOUNT));
	networkSettings.addItem(new CMenuForwarder(LOCALE_NFS_MOUNT , true, NULL, new CNFSMountGui(), NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	networkSettings.addItem(new CMenuForwarder(LOCALE_NFS_UMOUNT, true, NULL, new CNFSUmountGui(), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
}

#define RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT 4
const CMenuOptionChooser::keyval RECORDINGMENU_RECORDING_TYPE_OPTIONS[RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT] =
{
	{ CNeutrinoApp::RECORDING_OFF   , LOCALE_RECORDINGMENU_OFF    },
	{ CNeutrinoApp::RECORDING_SERVER, LOCALE_RECORDINGMENU_SERVER },
	{ CNeutrinoApp::RECORDING_VCR   , LOCALE_RECORDINGMENU_VCR    },
	{ CNeutrinoApp::RECORDING_FILE  , LOCALE_RECORDINGMENU_FILE   }
};

#define CHOOSE_DIRECT_REC_DIR_COUNT 3
const CMenuOptionChooser::keyval CHOOSE_DIRECT_REC_DIR[RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT] =
{
	{0, LOCALE_OPTIONS_OFF},
	{1, LOCALE_NFS_TYPE_NFS},
	{2, LOCALE_NFS_LOCALDIR}
};
void CNeutrinoApp::InitRecordingSettings(CMenuWidget &recordingSettings)
{
#if 0
	CIPInput * recordingSettings_server_ip = new CIPInput(LOCALE_RECORDINGMENU_SERVER_IP, g_settings.recording_server_ip, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CStringInput * recordingSettings_server_port = new CStringInput(LOCALE_RECORDINGMENU_SERVER_PORT, g_settings.recording_server_port, 6, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, "0123456789 ");

	CMenuForwarder * mf1 = new CMenuForwarder(LOCALE_RECORDINGMENU_SERVER_IP, (g_settings.recording_type == RECORDING_SERVER), g_settings.recording_server_ip, recordingSettings_server_ip);
	CMenuForwarder * mf2 = new CMenuForwarder(LOCALE_RECORDINGMENU_SERVER_PORT, (g_settings.recording_type == RECORDING_SERVER), g_settings.recording_server_port, recordingSettings_server_port);

	CMACInput * recordingSettings_server_mac = new CMACInput(LOCALE_RECORDINGMENU_SERVER_MAC,  g_settings.recording_server_mac, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CMenuForwarder * mf3 = new CMenuForwarder(LOCALE_RECORDINGMENU_SERVER_MAC, ((g_settings.recording_type == RECORDING_SERVER) && g_settings.recording_server_wakeup==1), g_settings.recording_server_mac, recordingSettings_server_mac);

	CRecordingNotifier2 * RecordingNotifier2 = new CRecordingNotifier2(mf3);

	CMenuOptionChooser * oj2 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_SERVER_WAKEUP, &g_settings.recording_server_wakeup, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, (g_settings.recording_type == RECORDING_SERVER), RecordingNotifier2);

	CMenuOptionChooser* oj3 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_STOPPLAYBACK, &g_settings.recording_stopplayback, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, (g_settings.recording_type == RECORDING_SERVER || g_settings.recording_type == RECORDING_FILE));

	CMenuOptionChooser* oj4 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_STOPSECTIONSD, &g_settings.recording_stopsectionsd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, (g_settings.recording_type == RECORDING_SERVER || g_settings.recording_type == RECORDING_FILE));
	CMenuOptionChooser* oj4b = new CMenuOptionChooser(LOCALE_RECORDINGMENU_ZAP_ON_ANNOUNCE, &g_settings.recording_zap_on_announce, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	CMenuOptionChooser* oj5 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_NO_SCART, &g_settings.recording_vcr_no_scart, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, (g_settings.recording_type == RECORDING_VCR));

	CMenuOptionChooser* oj12 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_RECORD_IN_SPTS_MODE, &g_settings.recording_in_spts_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT,(g_settings.recording_type == RECORDING_SERVER || g_settings.recording_type == RECORDING_FILE) );
#endif
	int pre,post;
	g_Timerd->getRecordingSafety(pre,post);
	sprintf(g_settings.record_safety_time_before, "%02d", pre/60);
	sprintf(g_settings.record_safety_time_after, "%02d", post/60);

	CRecordingSafetyNotifier *RecordingSafetyNotifier = new CRecordingSafetyNotifier;
	CStringInput * timerBefore = new CStringInput(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE, g_settings.record_safety_time_before, 2, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE_HINT_1, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE_HINT_2,"0123456789 ", RecordingSafetyNotifier);
	CMenuForwarder *fTimerBefore = new CMenuForwarder(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_BEFORE, true, g_settings.record_safety_time_before, timerBefore);

	CStringInput * timerAfter = new CStringInput(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER, g_settings.record_safety_time_after, 2, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER_HINT_1, LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER_HINT_2,"0123456789 ", RecordingSafetyNotifier);
	CMenuForwarder *fTimerAfter = new CMenuForwarder(LOCALE_TIMERSETTINGS_RECORD_SAFETY_TIME_AFTER, true, g_settings.record_safety_time_after, timerAfter);

	// default recording audio pids
	//CMenuWidget * apidMenu = new CMenuWidget(LOCALE_RECORDINGMENU_APIDS, NEUTRINO_ICON_AUDIO);
	//CMenuForwarder* fApidMenu = new CMenuForwarder(LOCALE_RECORDINGMENU_APIDS ,true, NULL, apidMenu);
	g_settings.recording_audio_pids_std = ( g_settings.recording_audio_pids_default & TIMERD_APIDS_STD ) ? 1 : 0 ;
	g_settings.recording_audio_pids_alt = ( g_settings.recording_audio_pids_default & TIMERD_APIDS_ALT ) ? 1 : 0 ;
	g_settings.recording_audio_pids_ac3 = ( g_settings.recording_audio_pids_default & TIMERD_APIDS_AC3 ) ? 1 : 0 ;

	CRecAPIDSettingsNotifier * an = new CRecAPIDSettingsNotifier;
	CMenuOptionChooser* aoj1 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_STD, &g_settings.recording_audio_pids_std, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, an);
	CMenuOptionChooser* aoj2 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_ALT, &g_settings.recording_audio_pids_alt, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, an);
	CMenuOptionChooser* aoj3 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_APIDS_AC3, &g_settings.recording_audio_pids_ac3, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true, an);
#if 0
	apidRecordingSettings->addItem(GenericMenuSeparator);
	apidRecordingSettings->addItem(GenericMenuBack);
	apidRecordingSettings->addItem(GenericMenuSeparatorLine);
	apidRecordingSettings->addItem(aoj1);
	apidRecordingSettings->addItem(aoj2);
	apidRecordingSettings->addItem(aoj3);
#endif
	CMenuForwarder* fRecDir = new CMenuForwarder(LOCALE_RECORDINGMENU_DEFDIR, true, g_settings.network_nfs_recordingdir, this, "recordingdir");
	CMenuForwarder* fTsDir = new CMenuForwarder(LOCALE_RECORDINGMENU_TSDIR, true, g_settings.timeshiftdir, this, "timeshiftdir");
	CMenuOptionChooser* zapAnnounce = new CMenuOptionChooser(LOCALE_RECORDINGMENU_ZAP_ON_ANNOUNCE, &g_settings.recording_zap_on_announce, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	recordingSettings.addItem(GenericMenuSeparator);
	recordingSettings.addItem(GenericMenuBack);
	recordingSettings.addItem(new CMenuForwarder(LOCALE_RECORDINGMENU_SETUPNOW, true, NULL, this, "recording", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	recordingSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_TIMERSETTINGS_SEPARATOR));
	recordingSettings.addItem(fTimerBefore);
	recordingSettings.addItem(fTimerAfter);
	recordingSettings.addItem(zapAnnounce);
	recordingSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_RECORDINGMENU_APIDS));
	recordingSettings.addItem(aoj1);
	recordingSettings.addItem(aoj2);
	recordingSettings.addItem(aoj3);
	recordingSettings.addItem(GenericMenuSeparatorLine);
	recordingSettings.addItem(fRecDir);
	recordingSettings.addItem(fTsDir);
	recordingSettings.addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_RECORD_TIME, &g_settings.record_hours, true, 1, 24, NULL) );
	if (1) { //has_hdd) {
		recordingSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_TIMESHIFT_PAUSE, &g_settings.timeshift_pause, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		recordingSettings.addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_AUTO_TIMESHIFT, &g_settings.auto_timeshift, true, 0, 300, NULL));
		recordingSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_AUTO_DELETE, &g_settings.auto_delete, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		recordingSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_TEMP_TIMESHIFT, &g_settings.temp_timeshift, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	}
#if 0
	// for direct recording
	CMenuWidget *directRecordingSettings = new CMenuWidget(LOCALE_RECORDINGMENU_FILESETTINGS, NEUTRINO_ICON_RECORDING);

	CMenuForwarder* mf7 = new CMenuForwarder(LOCALE_RECORDINGMENU_FILESETTINGS,(g_settings.recording_type == RECORDING_FILE),NULL,directRecordingSettings, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN);


	CStringInput * recordingSettings_splitsize = new CStringInput(LOCALE_RECORDINGMENU_SPLITSIZE, g_settings.recording_splitsize, 6, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, "0123456789 ");
	CMenuForwarder* mf9 = new CMenuForwarder(LOCALE_RECORDINGMENU_SPLITSIZE, true, g_settings.recording_splitsize,recordingSettings_splitsize);

	CMenuOptionChooser* oj6 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_USE_O_SYNC, &g_settings.recording_use_o_sync, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	CMenuOptionChooser* oj7 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_USE_FDATASYNC, &g_settings.recording_use_fdatasync, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	CMenuOptionChooser* oj8 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_STREAM_VTXT_PID, &g_settings.recording_stream_vtxt_pid, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	CMenuOptionChooser* oj9 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_STREAM_PMT_PID, &g_settings.recording_stream_pmt_pid, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	CStringInput * recordingSettings_ringbuffers = new CStringInput(LOCALE_RECORDINGMENU_RINGBUFFERS, g_settings.recording_ringbuffers, 2, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, "0123456789 ");
	CMenuForwarder* mf10 = new CMenuForwarder(LOCALE_RECORDINGMENU_RINGBUFFERS, true, g_settings.recording_ringbuffers,recordingSettings_ringbuffers);
	//CMenuOptionChooser* oj10 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_CHOOSE_DIRECT_REC_DIR, &g_settings.recording_choose_direct_rec_dir, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	CMenuOptionChooser* oj10 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_CHOOSE_DIRECT_REC_DIR, &g_settings.recording_choose_direct_rec_dir, CHOOSE_DIRECT_REC_DIR, CHOOSE_DIRECT_REC_DIR_COUNT, true);

	CMenuOptionChooser* oj11 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_EPG_FOR_FILENAME, &g_settings.recording_epg_for_filename, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	CMenuOptionChooser* oj13 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_SAVE_IN_CHANNELDIR, &g_settings.recording_save_in_channeldir, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	CRecordingNotifier *RecordingNotifier = new CRecordingNotifier(mf1,mf2,oj2,mf3,oj3,oj4,oj5,mf7,oj12);

	CMenuOptionChooser* oj1 = new CMenuOptionChooser(LOCALE_RECORDINGMENU_RECORDING_TYPE, &g_settings.recording_type, RECORDINGMENU_RECORDING_TYPE_OPTIONS, RECORDINGMENU_RECORDING_TYPE_OPTION_COUNT, true, RecordingNotifier);

	recordingSettings.addItem(GenericMenuSeparator);
	recordingSettings.addItem(GenericMenuBack);
	//recordingSettings.addItem(new CMenuForwarder(LOCALE_SETTINGS_HELP, true, NULL, this, "help_recording", CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP_SMALL));
	recordingSettings.addItem(new CMenuForwarder(LOCALE_RECORDINGMENU_SETUPNOW, true, NULL, this, "recording", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	recordingSettings.addItem(GenericMenuSeparatorLine);
	recordingSettings.addItem( oj1);
	recordingSettings.addItem(GenericMenuSeparatorLine);
	recordingSettings.addItem( mf1);
	recordingSettings.addItem( mf2);
	recordingSettings.addItem( oj2);
	recordingSettings.addItem( mf3);
	recordingSettings.addItem( oj3);
	recordingSettings.addItem( oj4);
	recordingSettings.addItem(GenericMenuSeparatorLine);
	recordingSettings.addItem( oj5);
	recordingSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_TIMERSETTINGS_SEPARATOR));
	recordingSettings.addItem( mf5);
	recordingSettings.addItem( mf6);
	recordingSettings.addItem( oj4b);
	recordingSettings.addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_RECORD_TIME, &g_settings.record_hours, true, 1, 24) );
	recordingSettings.addItem(GenericMenuSeparatorLine);
#if 0
	recordingSettings.addItem(oj12);
#endif
	recordingSettings.addItem( mf13);
	recordingSettings.addItem( mf7);

	directRecordingSettings->addItem(GenericMenuSeparator);
	directRecordingSettings->addItem(GenericMenuBack);
	directRecordingSettings->addItem(GenericMenuSeparatorLine);
	directRecordingSettings->addItem(mf8);
	directRecordingSettings->addItem(mf9);
	directRecordingSettings->addItem(mf10);
	directRecordingSettings->addItem(oj6);
	directRecordingSettings->addItem(oj7);
	directRecordingSettings->addItem(oj8);
	directRecordingSettings->addItem(oj9);
	directRecordingSettings->addItem(oj10);
	directRecordingSettings->addItem(oj11);
	directRecordingSettings->addItem(oj13);
	recordingstatus = 0;
	if (has_hdd) {
		directRecordingSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_TIMESHIFT_PAUSE, &g_settings.timeshift_pause, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		directRecordingSettings->addItem(new CMenuOptionNumberChooser(LOCALE_EXTRA_AUTO_TIMESHIFT, &g_settings.auto_timeshift, true, 0, 300));
		directRecordingSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_AUTO_DELETE, &g_settings.auto_delete, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
		directRecordingSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_TEMP_TIMESHIFT, &g_settings.temp_timeshift, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	}
#endif
}

#define STREAMINGMENU_STREAMING_TRANSCODE_VIDEO_CODEC_OPTION_COUNT 2
const CMenuOptionChooser::keyval STREAMINGMENU_STREAMING_TRANSCODE_VIDEO_CODEC_OPTIONS[STREAMINGMENU_STREAMING_TRANSCODE_VIDEO_CODEC_OPTION_COUNT] =
{
	{ 0, LOCALE_STREAMINGMENU_MPEG1 },
	{ 1, LOCALE_STREAMINGMENU_MPEG2 }
};

#define STREAMINGMENU_STREAMING_RESOLUTION_OPTION_COUNT 4
const CMenuOptionChooser::keyval STREAMINGMENU_STREAMING_RESOLUTION_OPTIONS[STREAMINGMENU_STREAMING_RESOLUTION_OPTION_COUNT] =
{
	{ 0, LOCALE_STREAMINGMENU_352X288 },
	{ 1, LOCALE_STREAMINGMENU_352X576 },
	{ 2, LOCALE_STREAMINGMENU_480X576 },
	{ 3, LOCALE_STREAMINGMENU_704X576 }
};

#define STREAMINGMENU_STREAMING_TYPE_OPTION_COUNT 2
const CMenuOptionChooser::keyval STREAMINGMENU_STREAMING_TYPE_OPTIONS[STREAMINGMENU_STREAMING_TYPE_OPTION_COUNT] =
{
	{ 0, LOCALE_STREAMINGMENU_OFF },
	{ 1, LOCALE_STREAMINGMENU_ON  }
};

void CNeutrinoApp::InitStreamingSettings(CMenuWidget &streamingSettings)
{
	addMenueIntroItems(streamingSettings);

	CIPInput * streamingSettings_server_ip = new CIPInput(LOCALE_STREAMINGMENU_SERVER_IP, g_settings.streaming_server_ip, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CStringInput * streamingSettings_server_port = new CStringInput(LOCALE_STREAMINGMENU_SERVER_PORT, g_settings.streaming_server_port, 6, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2,"0123456789 ");
	CStringInputSMS * cddriveInput = new CStringInputSMS(LOCALE_STREAMINGMENU_STREAMING_SERVER_CDDRIVE, g_settings.streaming_server_cddrive, 20, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-:\\ ");
	CStringInput * streamingSettings_videorate = new CStringInput(LOCALE_STREAMINGMENU_STREAMING_VIDEORATE, g_settings.streaming_videorate, 5, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2,"0123456789 ");
	CStringInput * streamingSettings_audiorate = new CStringInput(LOCALE_STREAMINGMENU_STREAMING_AUDIORATE, g_settings.streaming_audiorate, 5, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2,"0123456789 ");
	CStringInputSMS * startdirInput = new CStringInputSMS(LOCALE_STREAMINGMENU_STREAMING_SERVER_STARTDIR, g_settings.streaming_server_startdir, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"abcdefghijklmnopqrstuvwxyz0123456789!""�$%&/()=?-:\\ ");

	CMenuForwarder* mf1 = new CMenuForwarder(LOCALE_STREAMINGMENU_SERVER_IP                , (g_settings.streaming_type==1), g_settings.streaming_server_ip      , streamingSettings_server_ip);
	CMenuForwarder* mf2 = new CMenuForwarder(LOCALE_STREAMINGMENU_SERVER_PORT              , (g_settings.streaming_type==1), g_settings.streaming_server_port    , streamingSettings_server_port);
	CMenuForwarder* mf3 = new CMenuForwarder(LOCALE_STREAMINGMENU_STREAMING_SERVER_CDDRIVE , (g_settings.streaming_type==1), g_settings.streaming_server_cddrive , cddriveInput);
	CMenuForwarder* mf4 = new CMenuForwarder(LOCALE_STREAMINGMENU_STREAMING_VIDEORATE      , (g_settings.streaming_type==1), g_settings.streaming_videorate      , streamingSettings_videorate);
	CMenuForwarder* mf5 = new CMenuForwarder(LOCALE_STREAMINGMENU_STREAMING_AUDIORATE      , (g_settings.streaming_type==1), g_settings.streaming_audiorate      , streamingSettings_audiorate);
	CMenuForwarder* mf6 = new CMenuForwarder(LOCALE_STREAMINGMENU_STREAMING_SERVER_STARTDIR, (g_settings.streaming_type==1), g_settings.streaming_server_startdir, startdirInput);
	CMenuForwarder* mf7 = new CMenuForwarder(LOCALE_MOVIEPLAYER_DEFDIR, true, g_settings.network_nfs_moviedir,this,"moviedir");
	CMenuForwarder* mf8 = new CMenuForwarder(LOCALE_MOVIEPLAYER_DEFPLUGIN, true, g_settings.movieplayer_plugin,this,"movieplugin");
	CMenuOptionChooser* oj1 = new CMenuOptionChooser(LOCALE_STREAMINGMENU_STREAMING_TRANSCODE_AUDIO      , &g_settings.streaming_transcode_audio      , MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	CMenuOptionChooser* oj2 = new CMenuOptionChooser(LOCALE_STREAMINGMENU_STREAMING_FORCE_AVI_RAWAUDIO   , &g_settings.streaming_force_avi_rawaudio   , MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	CMenuOptionChooser* oj3 = new CMenuOptionChooser(LOCALE_STREAMINGMENU_STREAMING_FORCE_TRANSCODE_VIDEO, &g_settings.streaming_force_transcode_video, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

// not yet supported by VLC
	CMenuOptionChooser* oj4 = new CMenuOptionChooser(LOCALE_STREAMINGMENU_STREAMING_TRANSCODE_VIDEO_CODEC, &g_settings.streaming_transcode_video_codec, STREAMINGMENU_STREAMING_TRANSCODE_VIDEO_CODEC_OPTIONS, STREAMINGMENU_STREAMING_TRANSCODE_VIDEO_CODEC_OPTION_COUNT, true);

	CMenuOptionChooser* oj5 = new CMenuOptionChooser(LOCALE_STREAMINGMENU_STREAMING_RESOLUTION           , &g_settings.streaming_resolution           , STREAMINGMENU_STREAMING_RESOLUTION_OPTIONS, STREAMINGMENU_STREAMING_RESOLUTION_OPTION_COUNT, true);

	CStreamingNotifier *StreamingNotifier = new CStreamingNotifier(mf1,mf2,mf3,mf4,mf5,mf6,oj1,oj2,oj3,oj4,oj5);

	streamingSettings.addItem(new CMenuOptionChooser(LOCALE_STREAMINGMENU_STREAMING_TYPE                 , &g_settings.streaming_type                 , STREAMINGMENU_STREAMING_TYPE_OPTIONS, STREAMINGMENU_STREAMING_TYPE_OPTION_COUNT, true, StreamingNotifier));
	streamingSettings.addItem(GenericMenuSeparatorLine);
	streamingSettings.addItem( mf1);                          //Server IP
	streamingSettings.addItem( mf2);                          //Server Port
	streamingSettings.addItem( mf3);                          //CD-Drive
	streamingSettings.addItem( mf6);                          //Startdir
	streamingSettings.addItem(GenericMenuSeparatorLine);
	streamingSettings.addItem( mf4);                          //Video-Rate
	streamingSettings.addItem( oj3);
	streamingSettings.addItem( oj4);
	streamingSettings.addItem( oj5);
	streamingSettings.addItem(GenericMenuSeparatorLine);
	streamingSettings.addItem( mf5);                          //Audiorate
	streamingSettings.addItem( oj1);
	streamingSettings.addItem( oj2);
	streamingSettings.addItem(GenericMenuSeparatorLine);
	streamingSettings.addItem( mf7);                          //default dir
	streamingSettings.addItem( mf8);				//default movieplugin
}


class CMenuNumberInput : public CMenuForwarder, CMenuTarget, CChangeObserver
{
private:
	CChangeObserver * observer;
	CConfigFile     * configfile;
	int32_t           defaultvalue;
	char              value[11];

protected:

	virtual const char * getOption(void)
	{
		sprintf(value, "%u", configfile->getInt32(locale_real_names[text], defaultvalue));
		return value;
	}

	virtual bool changeNotify(const neutrino_locale_t OptionName, void * Data)
	{
		configfile->setInt32(locale_real_names[text], atoi(value));
		return observer->changeNotify(OptionName, Data);
	}


public:
	CMenuNumberInput(const neutrino_locale_t Text, const int32_t DefaultValue, CChangeObserver * const _observer, CConfigFile * const _configfile) : CMenuForwarder(Text, true, NULL, this)
	{
		observer     = _observer;
		configfile   = _configfile;
		defaultvalue = DefaultValue;
	}

	int exec(CMenuTarget * parent, const std::string & _actionKey)
	{
		CStringInput input(text, (char *)getOption(), 3, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, "0123456789 ", this);
		return input.exec(parent, _actionKey);
	}
};

const SNeutrinoSettings::FONT_TYPES channellist_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR,
	SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER,
	SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP
};

const SNeutrinoSettings::FONT_TYPES eventlist_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL,
	SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME,
};

const SNeutrinoSettings::FONT_TYPES infobar_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO,
	SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL
};

const SNeutrinoSettings::FONT_TYPES epg_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_EPG_TITLE,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO1,
	SNeutrinoSettings::FONT_TYPE_EPG_INFO2,
	SNeutrinoSettings::FONT_TYPE_EPG_DATE
};

const SNeutrinoSettings::FONT_TYPES gamelist_font_sizes[2] =
{
	SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMLARGE,
	SNeutrinoSettings::FONT_TYPE_GAMELIST_ITEMSMALL
};

const SNeutrinoSettings::FONT_TYPES other_font_sizes[4] =
{
	SNeutrinoSettings::FONT_TYPE_MENU,
	SNeutrinoSettings::FONT_TYPE_MENU_TITLE,
	SNeutrinoSettings::FONT_TYPE_MENU_INFO,
	SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM
};

font_sizes_groups font_sizes_groups[6] =
{
	{LOCALE_FONTMENU_MENU       , 4, other_font_sizes      , "fontsize.doth"},
	{LOCALE_FONTMENU_CHANNELLIST, 4, channellist_font_sizes, "fontsize.dcha"},
	{LOCALE_FONTMENU_EVENTLIST  , 4, eventlist_font_sizes  , "fontsize.deve"},
	{LOCALE_FONTMENU_EPG        , 4, epg_font_sizes        , "fontsize.depg"},
	{LOCALE_FONTMENU_INFOBAR    , 4, infobar_font_sizes    , "fontsize.dinf"},
	{LOCALE_FONTMENU_GAMELIST   , 2, gamelist_font_sizes   , "fontsize.dgam"}
};

#define FONT_STYLE_REGULAR 0
#define FONT_STYLE_BOLD    1
#define FONT_STYLE_ITALIC  2

font_sizes_struct neutrino_font[FONT_TYPE_COUNT] =
{
	{LOCALE_FONTSIZE_MENU               ,  20, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_TITLE         ,  30, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_MENU_INFO          ,  16, FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_EPG_TITLE          ,  25, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EPG_INFO1          ,  17, FONT_STYLE_ITALIC , 2},
	{LOCALE_FONTSIZE_EPG_INFO2          ,  17, FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EPG_DATE           ,  15, FONT_STYLE_REGULAR, 2},
	{LOCALE_FONTSIZE_EVENTLIST_TITLE    ,  30, FONT_STYLE_REGULAR, 0},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMLARGE,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_EVENTLIST_ITEMSMALL,  14, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_EVENTLIST_DATETIME ,  16, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_GAMELIST_ITEMLARGE ,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_GAMELIST_ITEMSMALL ,  16, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST        ,  20, FONT_STYLE_BOLD   , 1},
	{LOCALE_FONTSIZE_CHANNELLIST_DESCR  ,  20, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_CHANNELLIST_NUMBER ,  14, FONT_STYLE_BOLD   , 2},
	{LOCALE_FONTSIZE_CHANNEL_NUM_ZAP    ,  40, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_NUMBER     ,  50, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_CHANNAME   ,  30, FONT_STYLE_BOLD   , 0},
	{LOCALE_FONTSIZE_INFOBAR_INFO       ,  20, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_INFOBAR_SMALL      ,  14, FONT_STYLE_REGULAR, 1},
	{LOCALE_FONTSIZE_FILEBROWSER_ITEM   ,  16, FONT_STYLE_BOLD   , 1}
};

void CNeutrinoApp::AddFontSettingItem(CMenuWidget &fontSettings, const SNeutrinoSettings::FONT_TYPES number_of_fontsize_entry)
{
	fontSettings.addItem(new CMenuNumberInput(neutrino_font[number_of_fontsize_entry].name, neutrino_font[number_of_fontsize_entry].defaultsize, &fontsizenotifier, &configfile));
}

void CNeutrinoApp::InitFontSettings(CMenuWidget &fontSettings)
{
	addMenueIntroItems(fontSettings);
	fontSettings.addItem( new CMenuForwarder(LOCALE_COLORMENU_FONT, true, NULL, this, "select_font"));
	fontSettings.addItem( new CMenuForwarder(LOCALE_COLORMENU_FONT_TTX, true, NULL, this, "ttx_font"));

	fontSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_FONTMENU_SIZES));

	//fontSettings.addItem( new CMenuForwarder(LOCALE_EPGPLUS_SELECT_FONT_NAME, true, NULL, this, "select_font"));
#if 0
	AddFontSettingItem(fontSettings, SNeutrinoSettings::FONT_TYPE_MENU);
	AddFontSettingItem(fontSettings, SNeutrinoSettings::FONT_TYPE_MENU_TITLE);
	AddFontSettingItem(fontSettings, SNeutrinoSettings::FONT_TYPE_MENU_INFO);
	fontSettings.addItem(GenericMenuSeparatorLine);
#endif

	for (int i = 0; i < 6; i++)
	{
		CMenuWidget * fontSettingsSubMenu = new CMenuWidget(font_sizes_groups[i].groupname, NEUTRINO_ICON_COLORS);
		addMenueIntroItems(*fontSettingsSubMenu);
		for (unsigned int j = 0; j < font_sizes_groups[i].count; j++)
		{
			AddFontSettingItem(*fontSettingsSubMenu, font_sizes_groups[i].content[j]);
		}
		fontSettingsSubMenu->addItem(GenericMenuSeparatorLine);
		fontSettingsSubMenu->addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, font_sizes_groups[i].actionkey));

		fontSettings.addItem(new CMenuForwarder(font_sizes_groups[i].groupname, true, NULL, fontSettingsSubMenu));
	}

	//AddFontSettingItem(fontSettings, SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM);
	//fontSettings.addItem(GenericMenuSeparatorLine);
	//fontSettings.addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, font_sizes_groups[5].actionkey));
}

#define OSD_PRESET_OPTIONS_COUNT 2
const CMenuOptionChooser::keyval OSD_PRESET_OPTIONS[OSD_PRESET_OPTIONS_COUNT] =
{
	{ 0, LOCALE_COLORMENU_SD_PRESET },
	{ 1, LOCALE_COLORMENU_HD_PRESET }
};

void CNeutrinoApp::InitColorSettings(CMenuWidget &colorSettings, CMenuWidget &fontSettings )
{
	CScreenSetup  * ScreenSetup = new CScreenSetup();
	addMenueIntroItems(colorSettings);

	CMenuWidget *colorSettings_menuColors = new CMenuWidget(LOCALE_COLORMENUSETUP_HEAD, NEUTRINO_ICON_SETTINGS);

	InitColorSettingsMenuColors(*colorSettings_menuColors);
	colorSettings.addItem( new CMenuForwarder(LOCALE_COLORMENU_MENUCOLORS, true, NULL, colorSettings_menuColors, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));

	colorSettings.addItem( new CMenuForwarder(LOCALE_FONTMENU_HEAD, true, NULL, &fontSettings, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));

	CMenuWidget *colorSettings_timing = new CMenuWidget(LOCALE_COLORMENU_TIMING, NEUTRINO_ICON_SETTINGS);
	InitColorSettingsTiming(*colorSettings_timing);

	//colorSettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_VIDEOMENU_OSD));
	colorSettings.addItem(new CMenuForwarder(LOCALE_COLORMENU_TIMING, true, NULL, colorSettings_timing, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	colorSettings.addItem(new CMenuForwarder(LOCALE_VIDEOMENU_SCREENSETUP, true, NULL, ScreenSetup, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

	CScreenPresetNotifier * presetNotify = new CScreenPresetNotifier();
	colorSettings.addItem(new CMenuOptionChooser(LOCALE_COLORMENU_OSD_PRESET, &g_settings.screen_preset, OSD_PRESET_OPTIONS, OSD_PRESET_OPTIONS_COUNT, true, presetNotify, CRCInput::RC_1));

	//infobar
	CMenuWidget *miscSettingsInfobar = new CMenuWidget(LOCALE_MISCSETTINGS_INFOBAR, NEUTRINO_ICON_SETTINGS);
	addMenueIntroItems(*miscSettingsInfobar);
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_PROGRESSBAR_COLOR, &g_settings.progressbar_color, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_CASYSTEM_DISPLAY, &g_settings.casystem_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_DISP_LOG, &g_settings.infobar_show_channellogo, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS, LOCALE_MISCSETTINGS_INFOBAR_DISP_OPTIONS_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_VIRTUAL_ZAP_MODE, &g_settings.virtual_zap_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SAT_DISPLAY, &g_settings.infobar_sat_display, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	miscSettingsInfobar->addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_INFOBAR_SHOW_VAR_HDD, &g_settings.infobar_show_var_hdd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	colorSettings.addItem(new CMenuForwarder(LOCALE_MISCSETTINGS_INFOBAR, true, NULL, miscSettingsInfobar, NULL, CRCInput::RC_2));

	colorSettings.addItem(GenericMenuSeparatorLine);
	colorSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_ROUNDED_CORNERS, &g_settings.rounded_corners, MENU_CORNERSETTINGS_TYPE_OPTIONS, MENU_CORNERSETTINGS_TYPE_OPTION_COUNT, true));
	colorSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_SCRAMBLED_MESSAGE, &g_settings.scrambled_message, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	colorSettings.addItem(new CMenuOptionChooser(LOCALE_INFOVIEWER_SUBCHAN_DISP_POS, &g_settings.infobar_subchan_disp_pos, INFOBAR_SUBCHAN_DISP_POS_OPTIONS, INFOBAR_SUBCHAN_DISP_POS_OPTIONS_COUNT, true));
	colorSettings.addItem(new CMenuOptionChooser(LOCALE_EXTRA_VOLUME_POS, &g_settings.volume_pos, VOLUMEBAR_DISP_POS_OPTIONS, VOLUMEBAR_DISP_POS_OPTIONS_COUNT, true));
	colorSettings.addItem(new CMenuOptionChooser(LOCALE_SETTINGS_MENU_POS, &g_settings.menu_pos, MENU_DISP_POS_OPTIONS, MENU_DISP_POS_OPTIONS_COUNT, true));

	CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_COLORMENU_FADE, &g_settings.widget_fade, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true );
	colorSettings.addItem( oj );
#if 0
	colorSettings.addItem(GenericMenuSeparatorLine);
	//menuefaden nur bei enx-chips!
	CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_COLORMENU_FADE, &g_settings.widget_fade, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true );
	colorSettings.addItem( oj );

	CAlphaSetup* chAlphaSetup = new CAlphaSetup(LOCALE_COLORMENU_GTX_ALPHA, &g_settings.gtx_alpha1, &g_settings.gtx_alpha2);
	colorSettings.addItem( new CMenuForwarder(LOCALE_COLORMENU_GTX_ALPHA, true, NULL, chAlphaSetup));
#endif
}

void CNeutrinoApp::InitColorThemesSettings(CMenuWidget &colorSettings_Themes)
{
	addMenueIntroItems(colorSettings_Themes);
	colorSettings_Themes.addItem(new CMenuForwarder(LOCALE_EXTRA_LOADCOLORS, true, NULL, this, "loadcolors"));
	colorSettings_Themes.addItem(new CMenuForwarder(LOCALE_EXTRA_SAVECOLORS, true, NULL, this, "savecolors"));
	colorSettings_Themes.addItem(GenericMenuSeparatorLine);
	colorSettings_Themes.addItem(new CMenuForwarder(LOCALE_COLORTHEMEMENU_NEUTRINO_THEME, true, NULL, this, "theme_neutrino"));
	colorSettings_Themes.addItem(new CMenuForwarder(LOCALE_COLORTHEMEMENU_CLASSIC_THEME, true, NULL, this, "theme_classic"));
	colorSettings_Themes.addItem( new CMenuForwarder(LOCALE_COLORTHEMEMENU_GRAY, true, NULL, this, "theme_ru") );
	colorSettings_Themes.addItem(new CMenuForwarder(LOCALE_COLORTHEMEMENU_DBLUE_THEME, true, NULL, this, "theme_dblue"));
	colorSettings_Themes.addItem(new CMenuForwarder(LOCALE_COLORTHEMEMENU_DVB2K_THEME, true, NULL, this, "theme_dvb2k"));
	colorSettings_Themes.addItem(new CMenuForwarder(LOCALE_COLORTHEMEMENU_RED_THEME, true, NULL, this, "theme_red"));
}

void CNeutrinoApp::InitColorSettingsMenuColors(CMenuWidget &colorSettings_menuColors)
{
	addMenueIntroItems(colorSettings_menuColors);
#if 0
	colorSettings_menuColors.addItem(GenericMenuSeparator);
	colorSettings_menuColors.addItem(GenericMenuBack);
	colorSettings_menuColors.addItem(GenericMenuSeparatorLine);
#endif
	CMenuWidget *colorSettings_Themes = new CMenuWidget(LOCALE_COLORTHEMEMENU_HEAD, NEUTRINO_ICON_SETTINGS);
	InitColorThemesSettings(*colorSettings_Themes);
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_THEMESELECT, true, NULL, colorSettings_Themes, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED) );

	CColorChooser* chHeadcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Head_red, &g_settings.menu_Head_green, &g_settings.menu_Head_blue,
			&g_settings.menu_Head_alpha, colorSetupNotifier);
	CColorChooser* chHeadTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Head_Text_red, &g_settings.menu_Head_Text_green, &g_settings.menu_Head_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Content_red, &g_settings.menu_Content_green, &g_settings.menu_Content_blue,
			&g_settings.menu_Content_alpha, colorSetupNotifier);
	CColorChooser* chContentTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Content_Text_red, &g_settings.menu_Content_Text_green, &g_settings.menu_Content_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentSelectedcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Content_Selected_red, &g_settings.menu_Content_Selected_green, &g_settings.menu_Content_Selected_blue,
			&g_settings.menu_Content_Selected_alpha, colorSetupNotifier);
	CColorChooser* chContentSelectedTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Content_Selected_Text_red, &g_settings.menu_Content_Selected_Text_green, &g_settings.menu_Content_Selected_Text_blue,
			NULL, colorSetupNotifier);
	CColorChooser* chContentInactivecolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.menu_Content_inactive_red, &g_settings.menu_Content_inactive_green, &g_settings.menu_Content_inactive_blue,
			&g_settings.menu_Content_inactive_alpha, colorSetupNotifier);
	CColorChooser* chContentInactiveTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.menu_Content_inactive_Text_red, &g_settings.menu_Content_inactive_Text_green, &g_settings.menu_Content_inactive_Text_blue,
			NULL, colorSetupNotifier);
	colorSettings_menuColors.addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUHEAD));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chHeadcolor ));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chHeadTextcolor ));
	colorSettings_menuColors.addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentcolor ));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentTextcolor ));
	colorSettings_menuColors.addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_INACTIVE));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentInactivecolor ));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentInactiveTextcolor));
	colorSettings_menuColors.addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORMENUSETUP_MENUCONTENT_SELECTED));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chContentSelectedcolor ));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chContentSelectedTextcolor ));

	CColorChooser* chInfobarcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.infobar_red, 
			&g_settings.infobar_green, &g_settings.infobar_blue, &g_settings.infobar_alpha, colorSetupNotifier);
	CColorChooser* chInfobarTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.infobar_Text_red, 
			&g_settings.infobar_Text_green, &g_settings.infobar_Text_blue, NULL, colorSetupNotifier);

	colorSettings_menuColors.addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORSTATUSBAR_TEXT));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chInfobarcolor ));
	colorSettings_menuColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chInfobarTextcolor ));
}

#if 0
void CNeutrinoApp::InitColorSettingsStatusBarColors(CMenuWidget &colorSettings_statusbarColors)
{
	colorSettings_statusbarColors.addItem(GenericMenuSeparator);

	colorSettings_statusbarColors.addItem(GenericMenuBack);

	CColorChooser* chInfobarcolor = new CColorChooser(LOCALE_COLORMENU_BACKGROUND, &g_settings.infobar_red, &g_settings.infobar_green, &g_settings.infobar_blue,
			&g_settings.infobar_alpha, colorSetupNotifier);
	CColorChooser* chInfobarTextcolor = new CColorChooser(LOCALE_COLORMENU_TEXTCOLOR, &g_settings.infobar_Text_red, &g_settings.infobar_Text_green, &g_settings.infobar_Text_blue,
			NULL, colorSetupNotifier);

	colorSettings_statusbarColors.addItem( new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_COLORSTATUSBAR_TEXT));
	colorSettings_statusbarColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_BACKGROUND, true, NULL, chInfobarcolor ));
	colorSettings_statusbarColors.addItem( new CMenuForwarder(LOCALE_COLORMENU_TEXTCOLOR, true, NULL, chInfobarTextcolor ));
}
#endif

void CNeutrinoApp::InitColorSettingsTiming(CMenuWidget &colorSettings_timing)
{
	addMenueIntroItems(colorSettings_timing);

	for (int i = 0; i < TIMING_SETTING_COUNT; i++)
	{
		CStringInput * colorSettings_timing_item = new CStringInput(timing_setting_name[i], g_settings.timing_string[i], 3, LOCALE_TIMING_HINT_1, LOCALE_TIMING_HINT_2, "0123456789 ", &timingsettingsnotifier);
		colorSettings_timing.addItem(new CMenuForwarder(timing_setting_name[i], true, g_settings.timing_string[i], colorSettings_timing_item));
	}

	colorSettings_timing.addItem(GenericMenuSeparatorLine);
	colorSettings_timing.addItem(new CMenuForwarder(LOCALE_OPTIONS_DEFAULT, true, NULL, this, "osd.def"));
}

#define LCDMENU_STATUSLINE_OPTION_COUNT 2
const CMenuOptionChooser::keyval LCDMENU_STATUSLINE_OPTIONS[LCDMENU_STATUSLINE_OPTION_COUNT] =
{
	{ 0, LOCALE_LCDMENU_STATUSLINE_PLAYTIME },
	{ 1, LOCALE_LCDMENU_STATUSLINE_VOLUME   }
	//,{ 2, LOCALE_LCDMENU_STATUSLINE_BOTH     }
};

void CNeutrinoApp::InitLcdSettings(CMenuWidget &lcdSettings)
{
	addMenueIntroItems(lcdSettings);

	CVfdControler* lcdsliders = new CVfdControler(LOCALE_LCDMENU_HEAD, NULL);

#if 0
	CLcdNotifier* lcdnotifier = new CLcdNotifier();

	CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_LCDMENU_INVERSE, &g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, lcdnotifier);
	lcdSettings.addItem(oj);

	oj = new CMenuOptionChooser(LOCALE_LCDMENU_POWER, &g_settings.lcd_setting[SNeutrinoSettings::LCD_POWER], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, lcdnotifier);
	lcdSettings.addItem(oj);
#endif
	CStringInput * dim_time = new CStringInput(LOCALE_LCDMENU_DIM_TIME, g_settings.lcd_setting_dim_time, 3,
			NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"0123456789 ");
	lcdSettings.addItem(new CMenuForwarder(LOCALE_LCDMENU_DIM_TIME,true, g_settings.lcd_setting_dim_time,dim_time));

	CStringInput * dim_brightness = new CStringInput(LOCALE_LCDMENU_DIM_BRIGHTNESS, g_settings.lcd_setting_dim_brightness, 3,
			NONEXISTANT_LOCALE, NONEXISTANT_LOCALE,"0123456789 ");
	lcdSettings.addItem(new CMenuForwarder(LOCALE_LCDMENU_DIM_BRIGHTNESS,true, g_settings.lcd_setting_dim_brightness,dim_brightness));

	lcdSettings.addItem(GenericMenuSeparatorLine);
	lcdSettings.addItem(new CMenuForwarder(LOCALE_LCDMENU_LCDCONTROLER, true, NULL, lcdsliders));

	lcdSettings.addItem(GenericMenuSeparatorLine);
	CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_LCDMENU_STATUSLINE, &g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME], LCDMENU_STATUSLINE_OPTIONS, LCDMENU_STATUSLINE_OPTION_COUNT, true);
	lcdSettings.addItem(oj);
}

#define KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT 3
const CMenuOptionChooser::keyval KEYBINDINGMENU_BOUQUETHANDLING_OPTIONS[KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT] =
{
	{ 0, LOCALE_KEYBINDINGMENU_BOUQUETCHANNELS_ON_OK },
	{ 1, LOCALE_KEYBINDINGMENU_BOUQUETLIST_ON_OK     },
	{ 2, LOCALE_KEYBINDINGMENU_ALLCHANNELS_ON_OK     }
};

enum keynames {
	KEY_TV_RADIO_MODE,
	KEY_PAGE_UP,
	KEY_PAGE_DOWN,
	KEY_LIST_START,
	KEY_LIST_END,
	KEY_CANCEL_ACTION,
	KEY_SORT,
	KEY_ADD_RECORD,
	KEY_ADD_REMIND,
	KEY_BOUQUET_UP,
	KEY_BOUQUET_DOWN,
	KEY_CHANNEL_UP,
	KEY_CHANNEL_DOWN,
	KEY_SUBCHANNEL_UP,
	KEY_SUBCHANNEL_DOWN,
	KEY_ZAP_HISTORY,
	KEY_LASTCHANNEL,
	MPKEY_REWIND,
	MPKEY_FORWARD,
	MPKEY_PAUSE,
	MPKEY_STOP,
	MPKEY_PLAY,
	MPKEY_AUDIO,
	MPKEY_TIME,
	MPKEY_BOOKMARK,
	KEY_TIMESHIFT,
	MPKEY_PLUGIN,
	KEY_PLUGIN,
	KEY_UNLOCK
};

#define KEYBINDS_COUNT 29
const neutrino_locale_t keydescription_head[KEYBINDS_COUNT] =
{
	LOCALE_KEYBINDINGMENU_TVRADIOMODE,
	LOCALE_KEYBINDINGMENU_PAGEUP,
	LOCALE_KEYBINDINGMENU_PAGEDOWN,
	LOCALE_EXTRA_KEY_LIST_START,
	LOCALE_EXTRA_KEY_LIST_END,
	LOCALE_KEYBINDINGMENU_CANCEL,
	LOCALE_KEYBINDINGMENU_SORT,
	LOCALE_KEYBINDINGMENU_ADDRECORD,
	LOCALE_KEYBINDINGMENU_ADDREMIND,
	LOCALE_KEYBINDINGMENU_BOUQUETUP,
	LOCALE_KEYBINDINGMENU_BOUQUETDOWN,
	LOCALE_KEYBINDINGMENU_CHANNELUP,
	LOCALE_KEYBINDINGMENU_CHANNELDOWN,
	LOCALE_KEYBINDINGMENU_SUBCHANNELUP,
	LOCALE_KEYBINDINGMENU_SUBCHANNELDOWN,
	LOCALE_KEYBINDINGMENU_ZAPHISTORY,
	LOCALE_KEYBINDINGMENU_LASTCHANNEL,
	LOCALE_MPKEY_REWIND,
	LOCALE_MPKEY_FORWARD,
	LOCALE_MPKEY_PAUSE,
	LOCALE_MPKEY_STOP,
	LOCALE_MPKEY_PLAY,
	LOCALE_MPKEY_AUDIO,
	LOCALE_MPKEY_TIME,
	LOCALE_MPKEY_BOOKMARK,
	LOCALE_EXTRA_KEY_TIMESHIFT,
	LOCALE_MPKEY_PLUGIN,
	LOCALE_EXTRA_KEY_PLUGIN,
	LOCALE_EXTRA_KEY_UNLOCK
};

const neutrino_locale_t keydescription[KEYBINDS_COUNT] =
{
	LOCALE_KEYBINDINGMENU_TVRADIOMODE,
	LOCALE_KEYBINDINGMENU_PAGEUP,
	LOCALE_KEYBINDINGMENU_PAGEDOWN,
	LOCALE_EXTRA_KEY_LIST_START,
	LOCALE_EXTRA_KEY_LIST_END,
	LOCALE_KEYBINDINGMENU_CANCEL,
	LOCALE_KEYBINDINGMENU_SORT,
	LOCALE_KEYBINDINGMENU_ADDRECORD,
	LOCALE_KEYBINDINGMENU_ADDREMIND,
	LOCALE_KEYBINDINGMENU_BOUQUETUP,
	LOCALE_KEYBINDINGMENU_BOUQUETDOWN,
	LOCALE_KEYBINDINGMENU_CHANNELUP,
	LOCALE_KEYBINDINGMENU_CHANNELDOWN,
	LOCALE_KEYBINDINGMENU_SUBCHANNELUP,
	LOCALE_KEYBINDINGMENU_SUBCHANNELDOWN,
	LOCALE_KEYBINDINGMENU_ZAPHISTORY,
	LOCALE_KEYBINDINGMENU_LASTCHANNEL,
	LOCALE_MPKEY_REWIND,
	LOCALE_MPKEY_FORWARD,
	LOCALE_MPKEY_PAUSE,
	LOCALE_MPKEY_STOP,
	LOCALE_MPKEY_PLAY,
	LOCALE_MPKEY_AUDIO,
	LOCALE_MPKEY_TIME,
	LOCALE_MPKEY_BOOKMARK,
	LOCALE_EXTRA_KEY_TIMESHIFT,
	LOCALE_MPKEY_PLUGIN,
	LOCALE_EXTRA_KEY_PLUGIN,
	LOCALE_EXTRA_KEY_UNLOCK
};

void CNeutrinoApp::InitKeySettings(CMenuWidget &keySettings)
{
	CMenuWidget* bindSettings = new CMenuWidget(LOCALE_KEYBINDINGMENU_HEAD, NEUTRINO_ICON_KEYBINDING);

	bindSettings->addItem(GenericMenuSeparator);
	bindSettings->addItem(GenericMenuBack);

	addMenueIntroItems(keySettings);
	int shortcut = 1;
	keySettings.addItem(new CMenuForwarder(LOCALE_EXTRA_LOADKEYS, true, NULL, this, "loadkeys", CRCInput::convertDigitToKey(shortcut++)));
	keySettings.addItem(new CMenuForwarder(LOCALE_EXTRA_SAVEKEYS, true, NULL, this, "savekeys", CRCInput::convertDigitToKey(shortcut++)));
	//keySettings.addItem(GenericMenuSeparatorLine);

	keySetupNotifier = new CKeySetupNotifier;
	CStringInput * keySettings_repeat_genericblocker = new CStringInput(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC, g_settings.repeat_genericblocker, 3, LOCALE_REPEATBLOCKER_HINT_1, LOCALE_REPEATBLOCKER_HINT_2, "0123456789 ", keySetupNotifier);
	CStringInput * keySettings_repeatBlocker = new CStringInput(LOCALE_KEYBINDINGMENU_REPEATBLOCK, g_settings.repeat_blocker, 3, LOCALE_REPEATBLOCKER_HINT_1, LOCALE_REPEATBLOCKER_HINT_2, "0123456789 ", keySetupNotifier);
	keySetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);

	keySettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_RC));
	keySettings.addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_REPEATBLOCK, true, g_settings.repeat_blocker, keySettings_repeatBlocker));
	keySettings.addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_REPEATBLOCKGENERIC, true, g_settings.repeat_genericblocker, keySettings_repeat_genericblocker));

	keySettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_SHUTDOWN));
	CMenuOptionChooser *m1 = new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL_RCDELAY, &g_settings.shutdown_real_rcdelay, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, !g_settings.shutdown_real);

	CMiscNotifier* miscNotifier = new CMiscNotifier( m1 );

	keySettings.addItem(new CMenuOptionChooser(LOCALE_MISCSETTINGS_SHUTDOWN_REAL, &g_settings.shutdown_real, OPTIONS_OFF1_ON0_OPTIONS, OPTIONS_OFF1_ON0_OPTION_COUNT, true, miscNotifier));
	keySettings.addItem(m1);

	int * keyvalue_p[KEYBINDS_COUNT] =
	{
		&g_settings.key_tvradio_mode,
		&g_settings.key_channelList_pageup,
		&g_settings.key_channelList_pagedown,
		&g_settings.key_list_start,
		&g_settings.key_list_end,

		&g_settings.key_channelList_cancel,
		&g_settings.key_channelList_sort,
		&g_settings.key_channelList_addrecord,
		&g_settings.key_channelList_addremind,
		&g_settings.key_bouquet_up,

		&g_settings.key_bouquet_down,
		&g_settings.key_quickzap_up,
		&g_settings.key_quickzap_down,
		&g_settings.key_subchannel_up,
		&g_settings.key_subchannel_down,

		&g_settings.key_zaphistory,
		&g_settings.key_lastchannel,

		&g_settings.mpkey_rewind,
		&g_settings.mpkey_forward,
		&g_settings.mpkey_pause,
		&g_settings.mpkey_stop,
		&g_settings.mpkey_play,
		&g_settings.mpkey_audio,
		&g_settings.mpkey_time,
		&g_settings.mpkey_bookmark,

		&g_settings.key_timeshift,
		&g_settings.mpkey_plugin,
		&g_settings.key_plugin,
		&g_settings.key_unlock
	};

	CKeyChooser * keychooser[KEYBINDS_COUNT];

	for (int i = 0; i < KEYBINDS_COUNT; i++)
		keychooser[i] = new CKeyChooser(keyvalue_p[i], keydescription_head[i], NEUTRINO_ICON_SETTINGS);

	keySettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_HEAD));

	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_MODECHANGE));
	bindSettings->addItem(new CMenuForwarder(keydescription[KEY_TV_RADIO_MODE], true, NULL, keychooser[KEY_TV_RADIO_MODE]));

	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_CHANNELLIST));

	CMenuOptionChooser *oj = new CMenuOptionChooser(LOCALE_KEYBINDINGMENU_BOUQUETHANDLING, &g_settings.bouquetlist_mode, KEYBINDINGMENU_BOUQUETHANDLING_OPTIONS, KEYBINDINGMENU_BOUQUETHANDLING_OPTION_COUNT, true );
	bindSettings->addItem(oj);

	for (int i = KEY_PAGE_UP; i <= KEY_BOUQUET_DOWN; i++)
		bindSettings->addItem(new CMenuForwarder(keydescription[i], true, NULL, keychooser[i]));

	bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_SMS_CHANNEL, &g_settings.sms_channel, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	// Zapping keys
	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_KEYBINDINGMENU_QUICKZAP));

	for (int i = KEY_CHANNEL_UP; i <= KEY_LASTCHANNEL; i++)
		bindSettings->addItem(new CMenuForwarder(keydescription[i], true, NULL, keychooser[i]));

	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINMENU_MOVIEPLAYER));
	for (int i = MPKEY_REWIND; i <= MPKEY_PLUGIN; i++)
		bindSettings->addItem(new CMenuForwarder(keydescription[i], true, NULL, keychooser[i]));

	bindSettings->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_MAINSETTINGS_MISC));
	//bindSettings->addItem(new CMenuForwarder(keydescription[KEY_PLUGIN], true, NULL, keychooser[KEY_PLUGIN]));
	bindSettings->addItem(new CMenuForwarder(keydescription[KEY_UNLOCK], true, NULL, keychooser[KEY_UNLOCK]));
	//bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_ZAP_CYCLE, &g_settings.zap_cycle, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_MENU_LEFT_EXIT, &g_settings.menu_left_exit, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_AUDIO_RUN_PLAYER, &g_settings.audio_run_player, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
	//bindSettings->addItem(new CMenuOptionChooser(LOCALE_EXTRA_KEY_CLICK, &g_settings.key_click, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	keySettings.addItem(new CMenuForwarder(LOCALE_KEYBINDINGMENU_HEAD, true, NULL, bindSettings, NULL, CRCInput::convertDigitToKey(shortcut++)));
	//mainSettings.addItem(new CMenuForwarder(LOCALE_MAINSETTINGS_KEYBINDING, true, NULL, &keySettings      , NULL, CRCInput::RC_blue  , NEUTRINO_ICON_BUTTON_BLUE  ));

	// USERMENU
	keySettings.addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_USERMENU_HEAD));
	keySettings.addItem(new CMenuForwarder(LOCALE_USERMENU_BUTTON_RED, true, NULL, new CUserMenuMenu(LOCALE_USERMENU_BUTTON_RED,0), NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	keySettings.addItem(new CMenuForwarder(LOCALE_USERMENU_BUTTON_GREEN, true, NULL, new CUserMenuMenu(LOCALE_USERMENU_BUTTON_GREEN,1), NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	keySettings.addItem(new CMenuForwarder(LOCALE_USERMENU_BUTTON_YELLOW, true, NULL, new CUserMenuMenu(LOCALE_USERMENU_BUTTON_YELLOW,2), NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	keySettings.addItem(new CMenuForwarder(LOCALE_USERMENU_BUTTON_BLUE, true, NULL, new CUserMenuMenu(LOCALE_USERMENU_BUTTON_BLUE,3), NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));

}
// USERMENU
// leave this functions, somebody might want to use it in the future again
void CNeutrinoApp::SelectNVOD()
{
	if (!(g_RemoteControl->subChannels.empty()))
	{
		// NVOD/SubService- Kanal!
		CMenuWidget NVODSelector(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, NEUTRINO_ICON_VIDEO);
		if (getNVODMenu(&NVODSelector))
			NVODSelector.exec(NULL, "");
	}
}

bool CNeutrinoApp::getNVODMenu(CMenuWidget* menu)
{
	if (menu == NULL)
		return false;
	if (g_RemoteControl->subChannels.empty())
		return false;

	menu->addItem(GenericMenuSeparator);

	int count = 0;
	char nvod_id[5];

	for ( CSubServiceListSorted::iterator e=g_RemoteControl->subChannels.begin(); e!=g_RemoteControl->subChannels.end(); ++e)
	{
		sprintf(nvod_id, "%d", count);

		if ( !g_RemoteControl->are_subchannels ) {
			char nvod_time_a[50], nvod_time_e[50], nvod_time_x[50];
			char nvod_s[100];
			struct  tm *tmZeit;

			tmZeit= localtime(&e->startzeit);
			sprintf(nvod_time_a, "%02d:%02d", tmZeit->tm_hour, tmZeit->tm_min);

			time_t endtime = e->startzeit+ e->dauer;
			tmZeit= localtime(&endtime);
			sprintf(nvod_time_e, "%02d:%02d", tmZeit->tm_hour, tmZeit->tm_min);

			time_t jetzt=time(NULL);
			if (e->startzeit > jetzt) {
				int mins=(e->startzeit- jetzt)/ 60;
				sprintf(nvod_time_x, g_Locale->getText(LOCALE_NVOD_STARTING), mins);
			}
			else if ( (e->startzeit<= jetzt) && (jetzt < endtime) ) {
				int proz=(jetzt- e->startzeit)*100/ e->dauer;
				sprintf(nvod_time_x, g_Locale->getText(LOCALE_NVOD_PERCENTAGE), proz);
			}
			else
				nvod_time_x[0]= 0;

			sprintf(nvod_s, "%s - %s %s", nvod_time_a, nvod_time_e, nvod_time_x);
			menu->addItem(new CMenuForwarderNonLocalized(nvod_s, true, NULL, NVODChanger, nvod_id), (count == g_RemoteControl->selected_subchannel));
		} else {
			menu->addItem(new CMenuForwarderNonLocalized(e->subservice_name.c_str(), true, NULL, NVODChanger, nvod_id, CRCInput::convertDigitToKey(count)), (count == g_RemoteControl->selected_subchannel));
		}

		count++;
	}

	if ( g_RemoteControl->are_subchannels ) {
		menu->addItem(GenericMenuSeparatorLine);
		CMenuOptionChooser* oj = new CMenuOptionChooser(LOCALE_NVODSELECTOR_DIRECTORMODE, &g_RemoteControl->director_mode, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW);
		menu->addItem(oj);
	}
	return true;
}

void CNeutrinoApp::SelectAPID()
{
#if 0
	if ( g_RemoteControl->current_PIDs.APIDs.size()> 1 )
	{
		// we have APIDs for this channel!

		CMenuWidget APIDSelector(LOCALE_APIDSELECTOR_HEAD, NEUTRINO_ICON_AUDIO);
		APIDSelector.addItem(GenericMenuSeparator);

		for ( unsigned int count=0; count<g_RemoteControl->current_PIDs.APIDs.size(); count++ )
		{
			char apid[5];
			sprintf(apid, "%d", count);
			APIDSelector.addItem(new CMenuForwarderNonLocalized(g_RemoteControl->current_PIDs.APIDs[count].desc, true, NULL, APIDChanger, apid, CRCInput::convertDigitToKey(count + 1)), (count == g_RemoteControl->current_PIDs.PIDs.selected_apid));
		}
		APIDSelector.exec(NULL, "");
	}
#endif
}

#define MAINMENU_RECORDING_OPTION_COUNT 2
const CMenuOptionChooser::keyval MAINMENU_RECORDING_OPTIONS[MAINMENU_RECORDING_OPTION_COUNT] =
{
	{ 0, LOCALE_MAINMENU_RECORDING_START },
	{ 1, LOCALE_MAINMENU_RECORDING_STOP  }
};

// USERMENU
// This is just a quick helper for the usermenu only. I already made it a class for future use.
#define BUTTONMAX 4
const neutrino_msg_t key_helper_msg_def[BUTTONMAX]={CRCInput::RC_red,CRCInput::RC_green,CRCInput::RC_yellow,CRCInput::RC_blue};
const char * key_helper_icon_def[BUTTONMAX]={NEUTRINO_ICON_BUTTON_RED,NEUTRINO_ICON_BUTTON_GREEN,NEUTRINO_ICON_BUTTON_YELLOW,NEUTRINO_ICON_BUTTON_BLUE};
class CKeyHelper
{
private:
	int number_key;
	bool color_key_used[BUTTONMAX];
public:
	CKeyHelper() {
		reset();
	};
	void reset(void)
	{
		number_key = 1;
		for (int i= 0; i < BUTTONMAX; i++ )
			color_key_used[i] = false;
	};

	/* Returns the next available button, to be used in menu as 'direct' keys. Appropriate
	 * definitions are returnd in msp and icon
	 * A color button could be requested as prefered button (other buttons are not supported yet).
	 * If the appropriate button is already in used, the next number_key button is returned instead
	 * (first 1-9 and than 0). */
	bool get(neutrino_msg_t* msg, const char** icon, neutrino_msg_t prefered_key = CRCInput::RC_nokey)
	{
		bool result = false;
		int button = -1;
		if (prefered_key == CRCInput::RC_red)
			button = 0;
		if (prefered_key == CRCInput::RC_green)
			button = 1;
		if (prefered_key == CRCInput::RC_yellow)
			button = 2;
		if (prefered_key == CRCInput::RC_blue)
			button = 3;

		*msg = CRCInput::RC_nokey;
		*icon = "";
		if (button >= 0 && button < BUTTONMAX)
		{ // try to get color button
			if ( color_key_used[button] == false)
			{
				color_key_used[button] = true;
				*msg = key_helper_msg_def[button];
				*icon = key_helper_icon_def[button];
				result = true;
			}
		}

		if ( result == false && number_key < 10) // no key defined yet, at least try to get a numbered key
		{
			// there is still a available number_key
			*msg = CRCInput::convertDigitToKey(number_key);
			*icon = "";
			if (number_key == 9)
				number_key = 0;
			else if (number_key == 0)
				number_key = 10;
			else
				number_key++;
			result = true;
		}
		return (result);
	};
};

// USERMENU
bool CNeutrinoApp::showUserMenu(int button)
{
	if (button < 0 || button >= SNeutrinoSettings::BUTTON_MAX)
		return false;

	CMenuItem* menu_item = NULL;
	CKeyHelper keyhelper;
	neutrino_msg_t key = CRCInput::RC_nokey;
	const char * icon = NULL;
	int dummy;

	int menu_items = 0;
	int menu_prev = -1;
	static int selected[SNeutrinoSettings::BUTTON_MAX] = {-1, -1, -1, -1};

	// define classes
	CFavorites* tmpFavorites                                = NULL;
	CPauseSectionsdNotifier* tmpPauseSectionsdNotifier      = NULL;
	CAudioSelectMenuHandler* tmpAudioSelectMenuHandler      = NULL;
	CMenuWidget* tmpNVODSelector                            = NULL;
	CStreamInfo2Handler*    tmpStreamInfo2Handler           = NULL;
	CEventListHandler* tmpEventListHandler                  = NULL;
	CEPGplusHandler* tmpEPGplusHandler                      = NULL;
	CEPGDataHandler* tmpEPGDataHandler                      = NULL;

	std::string txt = g_settings.usermenu_text[button];
	if (button == SNeutrinoSettings::BUTTON_RED) {
		if ( txt.empty() )
			txt = g_Locale->getText(LOCALE_INFOVIEWER_EVENTLIST);
	}
	else if ( button == SNeutrinoSettings::BUTTON_GREEN) {
		if ( txt.empty() )
			txt = g_Locale->getText(LOCALE_INFOVIEWER_LANGUAGES);
	}
	else if ( button == SNeutrinoSettings::BUTTON_YELLOW) {
		if ( txt.empty() )
			txt = g_Locale->getText((g_RemoteControl->are_subchannels) ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME);
		//txt = g_Locale->getText(LOCALE_NVODSELECTOR_DIRECTORMODE);
	}
	else if ( button == SNeutrinoSettings::BUTTON_BLUE) {
		if ( txt.empty() )
			txt = g_Locale->getText(LOCALE_INFOVIEWER_STREAMINFO);
	}
	CMenuWidget *menu = new CMenuWidget(txt.c_str() , NEUTRINO_ICON_FEATURES);
	if (menu == NULL)
		return 0;
	menu->addItem(GenericMenuSeparator);

	// go through any postition number
	for (int pos = 0; pos < SNeutrinoSettings::ITEM_MAX ; pos++) {
		// now compare pos with the position of any item. Add this item if position is the same
		switch (g_settings.usermenu[button][pos]) {
		case SNeutrinoSettings::ITEM_NONE:
			// do nothing
			break;
		case SNeutrinoSettings::ITEM_BAR:
			if (menu_prev == -1 || menu_prev == SNeutrinoSettings::ITEM_BAR )
				break;

			menu->addItem(GenericMenuSeparatorLine);
			menu_prev = SNeutrinoSettings::ITEM_BAR;
			break;

		case SNeutrinoSettings::ITEM_FAVORITS:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_FAVORITS;
			tmpFavorites = new CFavorites;
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_FAVORITES_MENUEADD, true, NULL, tmpFavorites, "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_RECORD:
			if (g_settings.recording_type == RECORDING_OFF)
				break;

			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_RECORD;
			keyhelper.get(&key,&icon,CRCInput::RC_red);
			menu_item = new CMenuOptionChooser(LOCALE_MAINMENU_RECORDING, &recordingstatus, MAINMENU_RECORDING_OPTIONS, MAINMENU_RECORDING_OPTION_COUNT, true, this, key, icon);
			menu->addItem(menu_item, false);
			//if(has_hdd)
			//	menu->addItem(new CMenuForwarder(LOCALE_EXTRA_AUTO_TO_RECORD, autoshift, NULL, this, "autolink"), false);
			break;

		case SNeutrinoSettings::ITEM_MOVIEPLAYER_MB:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_MOVIEPLAYER_MB;
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_MOVIEBROWSER_HEAD, true, NULL, moviePlayerGui, "tsmoviebrowser", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_TIMERLIST:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_TIMERLIST;
			keyhelper.get(&key,&icon,CRCInput::RC_yellow);
			menu_item = new CMenuForwarder(LOCALE_TIMERLIST_NAME, true, NULL, Timerlist, "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_REMOTE:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_REMOTE;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_RCLOCK_MENUEADD, true, NULL, this->rcLock, "-1" , key, icon );
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_SUPER:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_SUPER;
			tmpEPGplusHandler = new CEPGplusHandler();
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_EPGPLUS   , true, NULL, tmpEPGplusHandler  ,  "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_LIST:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_LIST;
			tmpEventListHandler = new CEventListHandler();
			keyhelper.get(&key,&icon,CRCInput::RC_red);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_EVENTLIST , true, NULL, tmpEventListHandler,  "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_INFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_INFO;
			tmpEPGDataHandler = new CEPGDataHandler();
			keyhelper.get(&key,&icon,CRCInput::RC_yellow);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_EVENTINFO , true, NULL, tmpEPGDataHandler ,  "-1", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_EPG_MISC:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_EPG_MISC;
			dummy = g_Sectionsd->getIsScanningActive();
			//dummy = sectionsd_scanning;
			tmpPauseSectionsdNotifier = new CPauseSectionsdNotifier;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuOptionChooser(LOCALE_MAINMENU_PAUSESECTIONSD, &dummy, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, tmpPauseSectionsdNotifier , key, icon );
			menu->addItem(menu_item, false);
			menu_items++;
			keyhelper.get(&key,&icon);
			menu_item = new CMenuForwarder(LOCALE_MAINMENU_CLEARSECTIONSD, true, NULL, this, "clearSectionsd", key,icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_AUDIO_SELECT:
			//g_settings.audio_left_right_selectable || g_RemoteControl->current_PIDs.APIDs.size() > 1)
			if (1) {
				menu_items++;
				menu_prev = SNeutrinoSettings::ITEM_AUDIO_SELECT;
				tmpAudioSelectMenuHandler = new CAudioSelectMenuHandler;
				keyhelper.get(&key,&icon);
				menu_item = new CMenuForwarder(LOCALE_AUDIOSELECTMENUE_HEAD, true, NULL, tmpAudioSelectMenuHandler, "-1", key,icon);
				menu->addItem(menu_item, false);
			}
			break;

		case SNeutrinoSettings::ITEM_SUBCHANNEL:
			if (!(g_RemoteControl->subChannels.empty())) {
				// NVOD/SubService- Kanal!
				tmpNVODSelector = new CMenuWidget(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, NEUTRINO_ICON_VIDEO);
				if (getNVODMenu(tmpNVODSelector)) {
					menu_items++;
					menu_prev = SNeutrinoSettings::ITEM_SUBCHANNEL;
					keyhelper.get(&key,&icon);
					menu_item = new CMenuForwarder(g_RemoteControl->are_subchannels ? LOCALE_NVODSELECTOR_SUBSERVICE : LOCALE_NVODSELECTOR_HEAD, true, NULL, tmpNVODSelector, "-1", key,icon);
					menu->addItem(menu_item, false);
				}
			}
			break;

		case SNeutrinoSettings::ITEM_TECHINFO:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_TECHINFO;
			tmpStreamInfo2Handler = new CStreamInfo2Handler();
			keyhelper.get(&key,&icon,CRCInput::RC_blue);
			menu_item = new CMenuForwarder(LOCALE_EPGMENU_STREAMINFO, true, NULL, tmpStreamInfo2Handler    , "-1", key, icon );
			menu->addItem(menu_item, false);
			break;
		case SNeutrinoSettings::ITEM_PLUGIN:
		{
			char id[5];
			int cnt = 0;
			for (unsigned int count = 0; count < (unsigned int) g_PluginList->getNumberOfPlugins(); count++)
			{
				std::string tmp = g_PluginList->getName(count);
				if (g_PluginList->getType(count)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(count) && tmp.find("Teletext") == std::string::npos)
				{
					sprintf(id, "%d", count);
					menu_items++;
					menu_prev = SNeutrinoSettings::ITEM_PLUGIN;

					//keyhelper.get(&key,&icon,CRCInput::RC_blue);
					keyhelper.get(&key,&icon);
					menu_item = new CMenuForwarderNonLocalized(g_PluginList->getName(count), true, NULL, StreamFeaturesChanger, id, key, icon);
					//menu->addItem(menu_item, (cnt == 0));
					menu->addItem(menu_item, 0);
					cnt++;
				}
			}
		}
		break;

		case SNeutrinoSettings::ITEM_VTXT:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_VTXT;
			keyhelper.get(&key,&icon,CRCInput::RC_blue);
			menu_item = new CMenuForwarder(LOCALE_USERMENU_ITEM_VTXT, true, NULL, StreamFeaturesChanger, "teletext", key, icon);
			menu->addItem(menu_item, 0);
			break;

#if 0 // FIXME not supported yet
		case SNeutrinoSettings::ITEM_MOVIEPLAYER_TS:
			menu_items++;
			menu_prev = SNeutrinoSettings::ITEM_MOVIEPLAYER_TS;
			keyhelper.get(&key,&icon,CRCInput::RC_green);
			menu_item = new CMenuForwarder(LOCALE_MOVIEPLAYER_TSPLAYBACK, true, NULL, moviePlayerGui, "tsplayback", key, icon);
			menu->addItem(menu_item, false);
			break;

		case SNeutrinoSettings::ITEM_VTXT:
			for (unsigned int count = 0; count < (unsigned int) g_PluginList->getNumberOfPlugins(); count++)
			{
				std::string tmp = g_PluginList->getName(count);
				if (g_PluginList->getType(count)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(count) && tmp.find("Teletext") != std::string::npos)
				{
					sprintf(id, "%d", count);
					menu_items++;
					menu_prev = SNeutrinoSettings::ITEM_VTXT;

					//keyhelper.get(&key,&icon,CRCInput::RC_blue);
					keyhelper.get(&key,&icon);
					menu_item = new CMenuForwarderNonLocalized(g_PluginList->getName(count), true, NULL, StreamFeaturesChanger, id, key, icon);
					menu->addItem(menu_item, 0);
				}
			}
			break;
#endif
		default:
			printf("[neutrino] WARNING! menu wrong item!!\n");
			break;
		}
	}

	// Allow some tailoring for privat image bakers ;)
	if (button == SNeutrinoSettings::BUTTON_RED) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_GREEN) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_YELLOW) {
	}
	else if ( button == SNeutrinoSettings::BUTTON_BLUE) {
#ifdef _EXPERIMENTAL_SETTINGS_
		//Experimental Settings
		if (menu_prev != -1)
			menu->addItem(GenericMenuSeparatorLine);
		menu_items ++;
		menu_key++;
		// FYI: there is a memory leak with 'new CExperimentalSettingsMenuHandler()
		menu_item = new CMenuForwarder(LOCALE_EXPERIMENTALSETTINGS, true, NULL, new CExperimentalSettingsMenuHandler(), "-1", CRCInput::convertDigitToKey(menu_key));
		menu->addItem(menu_item, false);
#endif
	}

	// show menu if there are more than 2 items only
	// otherwise, we start the item directly (must be the last one)
	if (menu_items > 1 ) {
		menu->setSelected(selected[button]);
		menu->exec(NULL,"");
		selected[button] = menu->getSelected();
	}
	else if (menu_item != NULL)
		menu_item->exec( NULL );

	// restore mute symbol
	//AudioMute(current_muted, true);

	// clear the heap
	if (tmpFavorites)                delete tmpFavorites;
	if (tmpPauseSectionsdNotifier)   delete tmpPauseSectionsdNotifier;
	if (tmpAudioSelectMenuHandler)   delete tmpAudioSelectMenuHandler;
	if (tmpNVODSelector)             delete tmpNVODSelector;
	if (tmpStreamInfo2Handler)       delete tmpStreamInfo2Handler;
	if (tmpEventListHandler)         delete tmpEventListHandler;
	if (tmpEPGplusHandler)           delete tmpEPGplusHandler;
	if (tmpEPGDataHandler)           delete tmpEPGDataHandler;
	if (menu)                        delete menu;
	return 0;
}

void CNeutrinoApp::ShowStreamFeatures()
{
}

/*adds the typical menu intro with separator, back button and separatorline to menu*/
void CNeutrinoApp::addMenueIntroItems(CMenuWidget &item)
{
	item.addItem(GenericMenuSeparator);
	item.addItem(GenericMenuBack);
	item.addItem(GenericMenuSeparatorLine);
}

