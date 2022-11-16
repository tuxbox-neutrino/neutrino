/*
	$port: video_setup.h,v 1.4 2009/11/22 15:36:52 tuxbox-cvs Exp $

	video setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	Copyright (C) 2010-2012 Stefan Seyfried

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

#include <gui/widget/icons.h>
#include <gui/widget/menue_options.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <gui/osd_setup.h>
#include <gui/osd_helpers.h>
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#include <gui/psisetup.h>
#endif

#include <driver/display.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <daemonc/remotecontrol.h>

#include <system/debug.h>
#include <system/helpers.h>

#include <cs_api.h>
#include <hardware/video.h>

#ifdef BOXMODEL_CST_HD2
#include <cnxtfb.h>
#endif

extern cVideo *videoDecoder;
#if ENABLE_PIP
extern cVideo *pipVideoDecoder[3];
#include <gui/pipsetup.h>
#endif
#if ENABLE_QUADPIP
#include <gui/quadpip_setup.h>
#endif
extern int prev_video_mode;
extern CRemoteControl *g_RemoteControl; /* neutrino.cpp */

CVideoSettings::CVideoSettings(int wizard_mode)
{
	frameBuffer = CFrameBuffer::getInstance();

	is_wizard = wizard_mode;

	SyncControlerForwarder = NULL;

	width = 35;
	selected = -1;

	prev_video_mode = g_settings.video_Mode;

	setupVideoSystem(false);
	Init43ModeOptions();
}

CVideoSettings::~CVideoSettings()
{
	videomenu_43mode_options.clear();
}

int CVideoSettings::exec(CMenuTarget *parent, const std::string &/*actionKey*/)
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], init video setup (Mode: %d)...\n", __func__, __LINE__, is_wizard);
	int res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}

	res = showVideoSetup();

	return res;
}

const CMenuOptionChooser::keyval VIDEOMENU_43MODE_OPTIONS[] =
{
	{ DISPLAY_AR_MODE_PANSCAN, LOCALE_VIDEOMENU_PANSCAN },
	{ DISPLAY_AR_MODE_PANSCAN2, LOCALE_VIDEOMENU_PANSCAN2 },
	{ DISPLAY_AR_MODE_LETTERBOX, LOCALE_VIDEOMENU_LETTERBOX },
	{ DISPLAY_AR_MODE_NONE, LOCALE_VIDEOMENU_FULLSCREEN }
	//{ 2, LOCALE_VIDEOMENU_AUTO } // whatever is this auto mode, it seems its totally broken
};
#define VIDEOMENU_43MODE_OPTION_COUNT (sizeof(VIDEOMENU_43MODE_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

#ifndef BOXMODEL_CST_HD2
#define VIDEOMENU_VIDEOSIGNAL_TD_OPTION_COUNT 2
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_TD_OPTIONS[VIDEOMENU_VIDEOSIGNAL_TD_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_SCART   },
	{ ANALOG_SD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_SCART }
};
#endif

#ifdef ANALOG_MODE
#define VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT 8
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT] =
{
	{ ANALOG_MODE(SCART, SD, RGB),   LOCALE_VIDEOMENU_ANALOG_SD_RGB_SCART   }, // composite + RGB (for both SCART and Cinch)
	{ ANALOG_MODE(CINCH, SD, RGB),   LOCALE_VIDEOMENU_ANALOG_SD_RGB_CINCH   }, // composite + RGB (for both SCART and Cinch)
	{ ANALOG_MODE(SCART, SD, YPRPB), LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_SCART }, // YPbPr SCART (with wrongly connected Cinch)
	{ ANALOG_MODE(CINCH, SD, YPRPB), LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_CINCH }, // YPbPr Cinch (with wrongly connected SCART)
	{ ANALOG_MODE(SCART, HD, RGB),   LOCALE_VIDEOMENU_ANALOG_HD_RGB_SCART   },
	{ ANALOG_MODE(CINCH, HD, RGB),   LOCALE_VIDEOMENU_ANALOG_HD_RGB_CINCH   },
	{ ANALOG_MODE(SCART, HD, YPRPB), LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_SCART },
	{ ANALOG_MODE(CINCH, HD, YPRPB), LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_CINCH }
};

#define VIDEOMENU_VIDEOSIGNAL_HD2_OPTION_COUNT 6
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD2_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD2_OPTION_COUNT] =
{
	{ ANALOG_MODE(BOTH, xD, AUTO),  LOCALE_VIDEOMENU_ANALOG_AUTO     }, // Encoder automatically adjusts based on content
	{ ANALOG_MODE(BOTH, xD, CVBS),  LOCALE_VIDEOMENU_ANALOG_CVBS     }, // CVBS on SCART (disables fastblank, un-used dacs)
	{ ANALOG_MODE(BOTH, SD, RGB),   LOCALE_VIDEOMENU_ANALOG_SD_RGB   }, // SD RGB on Cinch and SCART
	{ ANALOG_MODE(BOTH, SD, YPRPB), LOCALE_VIDEOMENU_ANALOG_SD_YPRPB }, // SD YPrPb on Cinch and SCART
	{ ANALOG_MODE(BOTH, HD, RGB),   LOCALE_VIDEOMENU_ANALOG_HD_RGB   }, // HD RGB on Cinch and SCART
	{ ANALOG_MODE(BOTH, HD, YPRPB), LOCALE_VIDEOMENU_ANALOG_HD_YPRPB }, // HD YPrPb on Cinch and SCART
};

#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT] =
{
	{ ANALOG_MODE(SCART, SD, RGB),   LOCALE_VIDEOMENU_ANALOG_SD_RGB_SCART   }, // composite + RGB
	{ ANALOG_MODE(SCART, SD, YPRPB), LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_SCART }, // YPbPr SCART
	{ ANALOG_MODE(SCART, HD, RGB),   LOCALE_VIDEOMENU_ANALOG_HD_RGB_SCART   },
	{ ANALOG_MODE(SCART, HD, YPRPB), LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_SCART },
};

#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT] =
{
	{ ANALOG_MODE(CINCH, SD, RGB),   LOCALE_VIDEOMENU_ANALOG_SD_RGB_CINCH   }, // composite + RGB (for both SCART and Cinch)
	{ ANALOG_MODE(CINCH, SD, YPRPB), LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_CINCH }, // YPbPr Cinch (with wrongly connected SCART)
	{ ANALOG_MODE(CINCH, HD, RGB),   LOCALE_VIDEOMENU_ANALOG_HD_RGB_CINCH   },
	{ ANALOG_MODE(CINCH, HD, YPRPB), LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_CINCH }
};
#else
#define VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT 8
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_SCART   }, // composite + RGB (for both SCART and Cinch)
	{ ANALOG_SD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_CINCH   }, // composite + RGB (for both SCART and Cinch)
	{ ANALOG_SD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_SCART }, // YPbPr SCART (with wrongly connected Cinch)
	{ ANALOG_SD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_CINCH }, // YPbPr Cinch (with wrongly connected SCART)
	{ ANALOG_HD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_SCART   },
	{ ANALOG_HD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_CINCH   },
	{ ANALOG_HD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_SCART },
	{ ANALOG_HD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_CINCH }
};

#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_SCART   }, // composite + RGB
	{ ANALOG_SD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_SCART }, // YPbPr SCART
	{ ANALOG_HD_RGB_SCART,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_SCART   },
	{ ANALOG_HD_YPRPB_SCART, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_SCART },
};

#define VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTIONS[VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT] =
{
	{ ANALOG_SD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_SD_RGB_CINCH   }, // composite + RGB (for both SCART and Cinch)
	{ ANALOG_SD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_SD_YPRPB_CINCH }, // YPbPr Cinch (with wrongly connected SCART)
	{ ANALOG_HD_RGB_CINCH,   LOCALE_VIDEOMENU_ANALOG_HD_RGB_CINCH   },
	{ ANALOG_HD_YPRPB_CINCH, LOCALE_VIDEOMENU_ANALOG_HD_YPRPB_CINCH }
};
#endif

/*
 * key value of -1 means the mode is not available
 * TODO: instead of #ifdef select at run time
 */
#if BOXMODEL_CST_HD1
// numbers corresponding to video.cpp from zapit
CMenuOptionChooser::keyval_ext VIDEOMENU_VIDEOMODE_OPTIONS[VIDEOMENU_VIDEOMODE_OPTION_COUNT] =
{
	{ VIDEO_STD_NTSC,	NONEXISTANT_LOCALE, "NTSC"		},
	{ VIDEO_STD_PAL,	NONEXISTANT_LOCALE, "PAL"		},
	{ VIDEO_STD_SECAM,	NONEXISTANT_LOCALE, "SECAM"		},
	{ VIDEO_STD_480P,	NONEXISTANT_LOCALE, "480p"		},
	{ VIDEO_STD_576P,	NONEXISTANT_LOCALE, "576p"		},
	{ VIDEO_STD_720P50,	NONEXISTANT_LOCALE, "720p 50Hz"		},
	{ VIDEO_STD_720P60,	NONEXISTANT_LOCALE, "720p 60Hz"		},
	{ VIDEO_STD_1080I50,	NONEXISTANT_LOCALE, "1080i 50Hz"	},
	{ VIDEO_STD_1080I60,	NONEXISTANT_LOCALE, "1080i 60Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 23.97Hz"	},
	{ VIDEO_STD_1080P24,	NONEXISTANT_LOCALE, "1080p 24Hz"	},
	{ VIDEO_STD_1080P25,	NONEXISTANT_LOCALE, "1080p 25Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 29.97Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 50Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 60Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 24Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 25Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 30Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 50Hz"	},
	{ VIDEO_STD_AUTO,	NONEXISTANT_LOCALE, "Auto"		}
};
#elif BOXMODEL_CST_HD2
// numbers corresponding to video.cpp from zapit
CMenuOptionChooser::keyval_ext VIDEOMENU_VIDEOMODE_OPTIONS[VIDEOMENU_VIDEOMODE_OPTION_COUNT] =
{
	{ VIDEO_STD_NTSC,	NONEXISTANT_LOCALE, "NTSC"		},
	{ VIDEO_STD_PAL,	NONEXISTANT_LOCALE, "PAL"		},
	{ VIDEO_STD_SECAM,	NONEXISTANT_LOCALE, "SECAM"		},
	{ VIDEO_STD_480P,	NONEXISTANT_LOCALE, "480p"		},
	{ VIDEO_STD_576P,	NONEXISTANT_LOCALE, "576p"		},
	{ VIDEO_STD_720P50,	NONEXISTANT_LOCALE, "720p 50Hz"		},
	{ VIDEO_STD_720P60,	NONEXISTANT_LOCALE, "720p 60Hz"		},
	{ VIDEO_STD_1080I50,	NONEXISTANT_LOCALE, "1080i 50Hz"	},
	{ VIDEO_STD_1080I60,	NONEXISTANT_LOCALE, "1080i 60Hz"	},
	{ VIDEO_STD_1080P2397,	NONEXISTANT_LOCALE, "1080p 23.97Hz"	},
	{ VIDEO_STD_1080P24,	NONEXISTANT_LOCALE, "1080p 24Hz"	},
	{ VIDEO_STD_1080P25,	NONEXISTANT_LOCALE, "1080p 25Hz"	},
	{ VIDEO_STD_1080P2997,	NONEXISTANT_LOCALE, "1080p 29.97Hz"	},
	{ VIDEO_STD_1080P50,	NONEXISTANT_LOCALE, "1080p 50Hz"	},
	{ VIDEO_STD_1080P60,	NONEXISTANT_LOCALE, "1080p 60Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 24Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 25Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 30Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 50Hz"	},
	{ VIDEO_STD_AUTO,	NONEXISTANT_LOCALE, "Auto"		}
};
#elif BOXMODEL_HD51 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_E4HDULTRA || BOXMODEL_PROTEK4K || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE || BOXMODEL_VUPLUS_ALL
CMenuOptionChooser::keyval_ext VIDEOMENU_VIDEOMODE_OPTIONS[VIDEOMENU_VIDEOMODE_OPTION_COUNT] =
{
	{ -1,			NONEXISTANT_LOCALE, "NTSC"		},
	{ VIDEO_STD_PAL,	NONEXISTANT_LOCALE, "PAL"		},
	{ -1,			NONEXISTANT_LOCALE, "SECAM"		},
	{ -1,			NONEXISTANT_LOCALE, "480p"		},
	{ VIDEO_STD_576P,	NONEXISTANT_LOCALE, "576p"		},
	{ VIDEO_STD_720P50,	NONEXISTANT_LOCALE, "720p 50Hz"		},
	{ VIDEO_STD_720P60,	NONEXISTANT_LOCALE, "720p 60Hz"		},
	{ VIDEO_STD_1080I50,	NONEXISTANT_LOCALE, "1080i 50Hz"	},
	{ VIDEO_STD_1080I60,	NONEXISTANT_LOCALE, "1080i 60Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 23.97Hz"	},
	{ VIDEO_STD_1080P24,	NONEXISTANT_LOCALE, "1080p 24Hz"	},
	{ VIDEO_STD_1080P25,	NONEXISTANT_LOCALE, "1080p 25Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 29.97Hz"	},
	{ VIDEO_STD_1080P50,	NONEXISTANT_LOCALE, "1080p 50Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 60Hz"	},
	{ VIDEO_STD_2160P24,	NONEXISTANT_LOCALE, "2160p 24Hz"	},
	{ VIDEO_STD_2160P25,	NONEXISTANT_LOCALE, "2160p 25Hz"	},
	{ VIDEO_STD_2160P30,	NONEXISTANT_LOCALE, "2160p 30Hz"	},
	{ VIDEO_STD_2160P50,	NONEXISTANT_LOCALE, "2160p 50Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "Auto"		}
};
#elif BOXMODEL_OSMIO4K || BOXMODEL_OSMIO4KPLUS
CMenuOptionChooser::keyval_ext VIDEOMENU_VIDEOMODE_OPTIONS[VIDEOMENU_VIDEOMODE_OPTION_COUNT] =
{
	{ -1,			NONEXISTANT_LOCALE, "NTSC"		},
	{ VIDEO_STD_PAL,	NONEXISTANT_LOCALE, "PAL"		},
	{ -1,			NONEXISTANT_LOCALE, "SECAM"		},
	{ -1,			NONEXISTANT_LOCALE, "480p"		},
	{ VIDEO_STD_576P,	NONEXISTANT_LOCALE, "576p"		},
	{ VIDEO_STD_720P50,	NONEXISTANT_LOCALE, "720p 50Hz"		},
	{ VIDEO_STD_720P60,	NONEXISTANT_LOCALE, "720p 60Hz"		},
	{ VIDEO_STD_1080I50,	NONEXISTANT_LOCALE, "1080i 50Hz"	},
	{ VIDEO_STD_1080I60,	NONEXISTANT_LOCALE, "1080i 60Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 23.97Hz"	},
	{ VIDEO_STD_1080P24,	NONEXISTANT_LOCALE, "1080p 24Hz"	},
	{ VIDEO_STD_1080P25,	NONEXISTANT_LOCALE, "1080p 25Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 29.97Hz"	},
	{ VIDEO_STD_1080P50,	NONEXISTANT_LOCALE, "1080p 50Hz"	},
	{ VIDEO_STD_1080P60,	NONEXISTANT_LOCALE, "1080p 60Hz"	},
	{ VIDEO_STD_2160P24,	NONEXISTANT_LOCALE, "2160p 24Hz"	},
	{ VIDEO_STD_2160P25,	NONEXISTANT_LOCALE, "2160p 25Hz"	},
	{ VIDEO_STD_2160P30,	NONEXISTANT_LOCALE, "2160p 30Hz"	},
	{ VIDEO_STD_2160P50,	NONEXISTANT_LOCALE, "2160p 50Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "Auto"		}
};
#else
/* generic PC -> 5 different resolutions, 480, 576, 720 and 1080 lines */
CMenuOptionChooser::keyval_ext VIDEOMENU_VIDEOMODE_OPTIONS[VIDEOMENU_VIDEOMODE_OPTION_COUNT] =
{
	{ VIDEO_STD_NTSC,	NONEXISTANT_LOCALE, "NTSC"		},
	{ VIDEO_STD_PAL,	NONEXISTANT_LOCALE, "PAL"		},
	{ -1,			NONEXISTANT_LOCALE, "SECAM"		},
	{ -1,			NONEXISTANT_LOCALE, "480p"		},
	{ -1,			NONEXISTANT_LOCALE, "576p"		},
	{ VIDEO_STD_720P50,	NONEXISTANT_LOCALE, "720p 50Hz"		},
	{ VIDEO_STD_720P60,	NONEXISTANT_LOCALE, "720p 60Hz"		},
	{ VIDEO_STD_1080I50,	NONEXISTANT_LOCALE, "1080i 50Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080i 60Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 23.97Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 24Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 25Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 29.97Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 50Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "1080p 60Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 24Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 25Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 30Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "2160p 50Hz"	},
	{ -1,			NONEXISTANT_LOCALE, "Auto"		}
};
#endif

#define VIDEOMENU_VIDEOFORMAT_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_VIDEOFORMAT_OPTIONS[VIDEOMENU_VIDEOFORMAT_OPTION_COUNT] =
{
	{ DISPLAY_AR_4_3, LOCALE_VIDEOMENU_VIDEOFORMAT_43 },
	{ DISPLAY_AR_16_9, LOCALE_VIDEOMENU_VIDEOFORMAT_169 },
	{ DISPLAY_AR_14_9, LOCALE_VIDEOMENU_VIDEOFORMAT_149 }
};

#define VIDEOMENU_DBDR_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_DBDR_OPTIONS[VIDEOMENU_DBDR_OPTION_COUNT] =
{
	{ 0, LOCALE_VIDEOMENU_DBDR_NONE },
	{ 1, LOCALE_VIDEOMENU_DBDR_DEBLOCK },
	{ 2, LOCALE_VIDEOMENU_DBDR_BOTH }
};

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#define VIDEOMENU_ZAPPINGMODE_OPTION_COUNT 2
CMenuOptionChooser::keyval VIDEOMENU_ZAPPINGMODE_OPTIONS[VIDEOMENU_ZAPPINGMODE_OPTION_COUNT] =
{
	{ 0, LOCALE_VIDEOMENU_ZAPPINGMODE_MUTE },
	{ 1, LOCALE_VIDEOMENU_ZAPPINGMODE_HOLD }
};

#if BOXMODEL_VUPLUS_ARM
#define VIDEOMENU_HDMI_COLORIMETRY_OPTION_COUNT 3
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_COLORIMETRY_OPTIONS[VIDEOMENU_HDMI_COLORIMETRY_OPTION_COUNT] =
{
	{ HDMI_COLORIMETRY_AUTO, LOCALE_VIDEOMENU_HDMI_COLORIMETRY_AUTO },
	{ HDMI_COLORIMETRY_BT709, LOCALE_VIDEOMENU_HDMI_COLORIMETRY_BT709 },
	{ HDMI_COLORIMETRY_BT470, LOCALE_VIDEOMENU_HDMI_COLORIMETRY_BT470 }
};
#else
#define VIDEOMENU_HDMI_COLORIMETRY_OPTION_COUNT 4
const CMenuOptionChooser::keyval VIDEOMENU_HDMI_COLORIMETRY_OPTIONS[VIDEOMENU_HDMI_COLORIMETRY_OPTION_COUNT] =
{
	{ HDMI_COLORIMETRY_AUTO, LOCALE_VIDEOMENU_HDMI_COLORIMETRY_AUTO },
	{ HDMI_COLORIMETRY_BT2020NCL, LOCALE_VIDEOMENU_HDMI_COLORIMETRY_BT2020NCL },
	{ HDMI_COLORIMETRY_BT2020CL, LOCALE_VIDEOMENU_HDMI_COLORIMETRY_BT2020CL },
	{ HDMI_COLORIMETRY_BT709, LOCALE_VIDEOMENU_HDMI_COLORIMETRY_BT709 }
};
#endif
#endif

int CVideoSettings::showVideoSetup()
{
	// init
	CMenuWidget *videosetup = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width);
	videosetup->setSelected(selected);
	videosetup->setWizardMode(is_wizard);

	CMenuOptionChooser::keyval_ext vmode_options[VIDEOMENU_VIDEOMODE_OPTION_COUNT];
	int vmode_option_count = 0;
	for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		if (VIDEOMENU_VIDEOMODE_OPTIONS[i].key == -1)
			continue;
		vmode_options[vmode_option_count] = VIDEOMENU_VIDEOMODE_OPTIONS[i];
		vmode_option_count++;
	}

	// analog options
	unsigned int system_rev = cs_get_revision();
	CMenuOptionChooser *vs_analg_ch = NULL;
	CMenuOptionChooser *vs_scart_ch = NULL;
	CMenuOptionChooser *vs_chinch_ch = NULL;
	if (system_rev == 0x06)
	{
		vs_analg_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_ANALOG_MODE, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_HD1_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1_OPTION_COUNT, true, this);
		vs_analg_ch->setHint("", LOCALE_MENU_HINT_VIDEO_ANALOG_MODE);
	}
	else if (system_rev > 0x06)
	{
#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
		vs_analg_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_ANALOG_MODE, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_HD2_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD2_OPTION_COUNT, true, this);
		vs_analg_ch->setHint("", LOCALE_MENU_HINT_VIDEO_ANALOG_MODE);
#else
		if (system_rev != 10)
		{
			vs_scart_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_SCART, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_SCART_OPTION_COUNT, true, this);
			vs_scart_ch->setHint("", LOCALE_MENU_HINT_VIDEO_SCART_MODE);
		}
		vs_chinch_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_CINCH, &g_settings.analog_mode2, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTIONS, VIDEOMENU_VIDEOSIGNAL_HD1PLUS_CINCH_OPTION_COUNT, true, this);
		vs_chinch_ch->setHint("", LOCALE_MENU_HINT_VIDEO_CINCH_MODE);
#endif
	}
#ifndef BOXMODEL_CST_HD2
	else if (g_info.hw_caps->has_SCART)
	{
		vs_scart_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_SCART, &g_settings.analog_mode1, VIDEOMENU_VIDEOSIGNAL_TD_OPTIONS, VIDEOMENU_VIDEOSIGNAL_TD_OPTION_COUNT, true, this);
	}
#endif

	// 4:3 mode
	CMenuOptionChooser *vs_43mode_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_43MODE, &g_settings.video_43mode, videomenu_43mode_options, true, this);
	vs_43mode_ch->setHint("", LOCALE_MENU_HINT_VIDEO_43MODE);

	// display format
	CMenuOptionChooser *vs_dispformat_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_VIDEOFORMAT, &g_settings.video_Format, VIDEOMENU_VIDEOFORMAT_OPTIONS, g_info.hw_caps->can_ar_14_9 ? VIDEOMENU_VIDEOFORMAT_OPTION_COUNT : VIDEOMENU_VIDEOFORMAT_OPTION_COUNT - 1, true, this); /* works only if 14:9 is last! */
	vs_dispformat_ch->setHint("", LOCALE_MENU_HINT_VIDEO_FORMAT);

	// video system
	CMenuOptionChooser *vs_videomodes_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_VIDEOMODE, &g_settings.video_Mode, vmode_options, vmode_option_count, true, this, CRCInput::RC_nokey, "", true);
	vs_videomodes_ch->setHint("", LOCALE_MENU_HINT_VIDEO_MODE);

	CMenuOptionChooser *vs_dbdropt_ch = NULL;
	CMenuWidget videomodes(LOCALE_MAINSETTINGS_VIDEO, NEUTRINO_ICON_SETTINGS);
#ifdef BOXMODEL_CST_HD2
	CMenuForwarder *vs_automodes_fw = NULL;
	CMenuWidget automodes(LOCALE_MAINSETTINGS_VIDEO, NEUTRINO_ICON_SETTINGS);
#endif
	CAutoModeNotifier anotify;
	CMenuForwarder *vs_videomodes_fw = NULL;
	// dbdr options only on COOLSTREAM
	if (system_rev != 0x01)
	{
		vs_dbdropt_ch = new CMenuOptionChooser(LOCALE_VIDEOMENU_DBDR, &g_settings.video_dbdr, VIDEOMENU_DBDR_OPTIONS, VIDEOMENU_DBDR_OPTION_COUNT, true, this);
		vs_dbdropt_ch->setHint("", LOCALE_MENU_HINT_VIDEO_DBDR);
	}

	// video system modes submenue
	if (g_info.hw_caps->has_HDMI) // does this make sense on a box without HDMI?
	{
		videomodes.addIntroItems(LOCALE_VIDEOMENU_ENABLED_MODES);

		for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
			if (VIDEOMENU_VIDEOMODE_OPTIONS[i].key != -1)
				videomodes.addItem(new CMenuOptionChooser(VIDEOMENU_VIDEOMODE_OPTIONS[i].valname, &g_settings.enabled_video_modes[i], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, &anotify));

		if (g_info.hw_caps->has_button_vformat)
		{
			vs_videomodes_fw = new CMenuForwarder(LOCALE_VIDEOMENU_ENABLED_MODES, true, NULL, &videomodes, NULL, CRCInput::RC_red);
			vs_videomodes_fw->setHint("", LOCALE_MENU_HINT_VIDEO_MODES);
		}

#ifdef BOXMODEL_CST_HD2
		automodes.addIntroItems(LOCALE_VIDEOMENU_ENABLED_MODES_AUTO);

		for (int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT - 1; i++)
			automodes.addItem(new CMenuOptionChooser(VIDEOMENU_VIDEOMODE_OPTIONS[i].valname, &g_settings.enabled_auto_modes[i], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, &anotify));

		vs_automodes_fw = new CMenuForwarder(LOCALE_VIDEOMENU_ENABLED_MODES_AUTO, true, NULL, &automodes, NULL, CRCInput::RC_green);
		vs_automodes_fw->setHint("", LOCALE_MENU_HINT_VIDEO_MODES_AUTO);
#endif
	}

	neutrino_locale_t tmp_locale = NONEXISTANT_LOCALE;
	// TODO: check the locale
	if (vs_analg_ch != NULL || vs_scart_ch != NULL || vs_chinch_ch != NULL)
		tmp_locale = LOCALE_VIDEOMENU_TV_SCART;
	// ---------------------------------------
	videosetup->addIntroItems(LOCALE_MAINSETTINGS_VIDEO, tmp_locale);
	// ---------------------------------------
	//videosetup->addItem(vs_scart_sep); // separator scart
	if (vs_analg_ch != NULL)
		videosetup->addItem(vs_analg_ch); // analog option
	if (vs_scart_ch != NULL)
		videosetup->addItem(vs_scart_ch); // scart
	if (vs_chinch_ch != NULL)
		videosetup->addItem(vs_chinch_ch); // chinch
	//if (tmp_locale != NONEXISTANT_LOCALE)
	//	videosetup->addItem(GenericMenuSeparatorLine);
	// ---------------------------------------
	videosetup->addItem(vs_43mode_ch); // 4:3 mode
	videosetup->addItem(vs_dispformat_ch); // display format
	videosetup->addItem(vs_videomodes_ch); // video system
	if (vs_dbdropt_ch != NULL)
		videosetup->addItem(vs_dbdropt_ch); // dbdr options
	if (vs_videomodes_fw != NULL)
		videosetup->addItem(vs_videomodes_fw); // video modes submenue
#ifdef BOXMODEL_CST_HD2
	videosetup->addItem(vs_automodes_fw); // video auto modes submenue
#endif

#ifdef BOXMODEL_CST_HD2
	// values are from -128 to 127, but brightness really no sense after +/- 40. changeNotify multiply contrast and saturation to 3
	CMenuOptionNumberChooser *bcont = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_BRIGHTNESS, &g_settings.brightness, true, -42, 42, this);
	bcont->setHint("", LOCALE_MENU_HINT_VIDEO_BRIGHTNESS);
	CMenuOptionNumberChooser *ccont = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_CONTRAST, &g_settings.contrast, true, -42, 42, this);
	ccont->setHint("", LOCALE_MENU_HINT_VIDEO_CONTRAST);
	CMenuOptionNumberChooser *scont = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_SATURATION, &g_settings.saturation, true, -42, 42, this);
	scont->setHint("", LOCALE_MENU_HINT_VIDEO_SATURATION);
	videosetup->addItem(bcont);
	videosetup->addItem(ccont);
	videosetup->addItem(scont);

	CMenuOptionChooser *sd = new CMenuOptionChooser(LOCALE_VIDEOMENU_SDOSD, &g_settings.enable_sd_osd, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, this);
	sd->setHint("", LOCALE_MENU_HINT_VIDEO_SDOSD);
	videosetup->addItem(sd);
#endif
#if ENABLE_PIP
	CPipSetup pip;
	CMenuForwarder *pipsetup = new CMenuForwarder(LOCALE_VIDEOMENU_PIP, g_info.hw_caps->can_pip, NULL, &pip);
	pipsetup->setHint("", LOCALE_MENU_HINT_VIDEO_PIP);
	videosetup->addItem(pipsetup);
#endif

#if ENABLE_QUADPIP
	CMenuForwarder *quadpip = new CMenuForwarder(LOCALE_QUADPIP, g_info.hw_caps->pip_devs >= 1, NULL, new CQuadPiPSetup());
	quadpip->setHint(NEUTRINO_ICON_HINT_QUADPIP, LOCALE_MENU_HINT_QUADPIP);
	videosetup->addItem(quadpip);
#endif

#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	if (file_exists("/proc/stb/video/zapmode"))
	{
		CMenuOptionChooser *zm = new CMenuOptionChooser(LOCALE_VIDEOMENU_ZAPPINGMODE, &g_settings.zappingmode, VIDEOMENU_ZAPPINGMODE_OPTIONS, VIDEOMENU_ZAPPINGMODE_OPTION_COUNT, true, this);
		zm->setHint("", LOCALE_MENU_HINT_VIDEO_ZAPPINGMODE);
		videosetup->addItem(zm);
	}

#if BOXMODEL_VUPLUS_ARM
	if (file_exists("/proc/stb/video/hdmi_colorspace"))
#else
	if (file_exists("/proc/stb/video/hdmi_colorimetry"))
#endif
	{
		CMenuOptionChooser *hm = new CMenuOptionChooser(LOCALE_VIDEOMENU_HDMI_COLORIMETRY, &g_settings.hdmi_colorimetry, VIDEOMENU_HDMI_COLORIMETRY_OPTIONS, VIDEOMENU_HDMI_COLORIMETRY_OPTION_COUNT, true, this);
		hm->setHint("", LOCALE_MENU_HINT_VIDEO_HDMI_COLORIMETRY);
		videosetup->addItem(hm);
	}

	videosetup->addItem(GenericMenuSeparatorLine);

	CPSISetup *psiSetup = CPSISetup::getInstance();

#if 0
	CMenuOptionNumberChooser *mc;

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_STEP, (int *)&g_settings.psi_step, true, 1, 100, NULL);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_PSI_STEP);
	videosetup->addItem(mc);
#endif

	CMenuForwarder *mf = new CMenuForwarder(LOCALE_VIDEOMENU_PSI, true, NULL, psiSetup, NULL);
	mf->setHint("", LOCALE_MENU_HINT_VIDEO_PSI);
	videosetup->addItem(mf);

#if 0
	videosetup->addItem(GenericMenuSeparator);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_CONTRAST, (int *)&g_settings.psi_contrast, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_CONTRAST);
	videosetup->addItem(mc);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_SATURATION, (int *)&g_settings.psi_saturation, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_SATURATION);
	videosetup->addItem(mc);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_BRIGHTNESS, (int *)&g_settings.psi_brightness, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_BRIGHTNESS);
	videosetup->addItem(mc);

	mc = new CMenuOptionNumberChooser(LOCALE_VIDEOMENU_PSI_TINT, (int *)&g_settings.psi_tint, true, 0, 255, psiSetup);
	mc->setHint("", LOCALE_MENU_HINT_VIDEO_TINT);
	videosetup->addItem(mc);
#endif
#endif

	int res = videosetup->exec(NULL, "");
	selected = videosetup->getSelected();
	delete videosetup;
	return res;
}

void CVideoSettings::initVideoSettings()
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], init video settings...\n", __func__, __LINE__);
#if 0
	// FIXME focus: ?? this is different for different boxes
	videoDecoder->SetVideoMode((analog_mode_t) g_settings.analog_mode1);
	videoDecoder->SetVideoMode((analog_mode_t) g_settings.analog_mode2);
#endif
#ifdef BOXMODEL_CST_HD2
	changeNotify(LOCALE_VIDEOMENU_ANALOG_MODE, NULL);
#else
	unsigned int system_rev = cs_get_revision();
	if (system_rev == 0x06)
	{
		changeNotify(LOCALE_VIDEOMENU_ANALOG_MODE, NULL);
	}
	else
	{
		changeNotify(LOCALE_VIDEOMENU_SCART, NULL);
		changeNotify(LOCALE_VIDEOMENU_CINCH, NULL);
	}
#endif
	//setupVideoSystem(false/*don't ask*/); // focus: CVideoSettings constructor do this already ?

#if 0
	videoDecoder->setAspectRatio(-1, g_settings.video_43mode);
	videoDecoder->setAspectRatio(g_settings.video_Format, -1);
#endif
	videoDecoder->setAspectRatio(g_settings.video_Format, g_settings.video_43mode);
#if ENABLE_PIP
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->setAspectRatio(g_settings.video_Format, g_settings.video_43mode);
#endif

	videoDecoder->SetDBDR(g_settings.video_dbdr);
	CAutoModeNotifier anotify;
	anotify.changeNotify(NONEXISTANT_LOCALE, 0);
#ifdef BOXMODEL_CST_HD2
	changeNotify(LOCALE_VIDEOMENU_BRIGHTNESS, NULL);
	changeNotify(LOCALE_VIDEOMENU_CONTRAST, NULL);
	changeNotify(LOCALE_VIDEOMENU_SATURATION, NULL);
	changeNotify(LOCALE_VIDEOMENU_SDOSD, NULL);
#endif
#if ENABLE_PIP
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->Pig(g_settings.pip_x, g_settings.pip_y, g_settings.pip_width, g_settings.pip_height, g_settings.screen_width, g_settings.screen_height);
#endif
}

void CVideoSettings::setupVideoSystem(bool do_ask)
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], setup video system...\n", __func__, __LINE__);
	COsdHelpers::getInstance()->setVideoSystem(g_settings.video_Mode); // FIXME
	COsdHelpers::getInstance()->changeOsdResolution(0, true, false);

	if (do_ask)
	{
		if (prev_video_mode != g_settings.video_Mode)
		{
			frameBuffer->paintBackground();
			if (ShowMsg(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_VIDEO_MODE_OK), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_INFO) != CMsgBox::mbrYes)
			{
				g_settings.video_Mode = prev_video_mode;
				COsdHelpers::getInstance()->setVideoSystem(g_settings.video_Mode);
				COsdHelpers::getInstance()->changeOsdResolution(0, true, false);
			}
			else
				prev_video_mode = g_settings.video_Mode;
		}
	}
}

bool CVideoSettings::changeNotify(const neutrino_locale_t OptionName, void * /* data */)
{
#if 0
	int val = 0;
	if (data)
		val = * (int *) data;
#endif
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
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_VIDEOFORMAT) || ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_43MODE))
	{
		if (g_settings.video_Format != 1 && g_settings.video_Format != 3 && g_settings.video_Format != 2)
			g_settings.video_Format = 3;

		g_Zapit->setMode43(g_settings.video_43mode);
		videoDecoder->setAspectRatio(g_settings.video_Format, -1);
#if ENABLE_PIP
		if (pipVideoDecoder[0] != NULL)
			pipVideoDecoder[0]->setAspectRatio(g_settings.video_Format, g_settings.video_43mode);
#endif
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_VIDEOMODE))
	{
		setupVideoSystem(true /*ask*/);
		return true;
	}
#ifdef BOXMODEL_CST_HD2
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_BRIGHTNESS))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_BRIGHTNESS, g_settings.brightness);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_CONTRAST))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_CONTRAST, g_settings.contrast * 3);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_SATURATION))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_SATURATION, g_settings.saturation * 3);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_SDOSD))
	{
		int val = g_settings.enable_sd_osd;
		dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], SD OSD enable: %d\n", __func__, __LINE__, val);
		int fd = CFrameBuffer::getInstance()->getFileHandle();
		if (ioctl(fd, FBIO_SCALE_SD_OSD, &val))
			perror("FBIO_SCALE_SD_OSD");
	}
#endif
#if 0
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_SHARPNESS))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_SHARPNESS, val);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HUE))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_HUE, val);
	}
#endif
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_ZAPPINGMODE))
	{
		videoDecoder->SetControl(VIDEO_CONTROL_ZAPPING_MODE, g_settings.zappingmode);
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_VIDEOMENU_HDMI_COLORIMETRY))
	{
		videoDecoder->SetHDMIColorimetry((HDMI_COLORIMETRY) g_settings.hdmi_colorimetry);
	}
#endif
	return false;
}

void CVideoSettings::next43Mode(void)
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], setting 4:3 mode...", __func__, __LINE__);
	neutrino_locale_t text;
	unsigned int curmode = 0;

	for (unsigned int i = 0; i < videomenu_43mode_options.size(); i++)
	{
		if (videomenu_43mode_options[i].key == g_settings.video_43mode)
		{
			curmode = i;
			break;
		}
	}
	curmode++;
	if (curmode >= videomenu_43mode_options.size())
		curmode = 0;

	text = videomenu_43mode_options[curmode].value;
	g_settings.video_43mode = videomenu_43mode_options[curmode].key;
	g_Zapit->setMode43(g_settings.video_43mode);
#if ENABLE_PIP
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->setAspectRatio(-1, g_settings.video_43mode);
#endif
	ShowHint(LOCALE_VIDEOMENU_43MODE, g_Locale->getText(text), 450, 2);
}

void CVideoSettings::SwitchFormat()
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], setting video format...\n", __func__, __LINE__);
	neutrino_locale_t text;
	int curmode = 0;

	for (int i = 0; i < VIDEOMENU_VIDEOFORMAT_OPTION_COUNT; i++)
	{
		if (VIDEOMENU_VIDEOFORMAT_OPTIONS[i].key == g_settings.video_Format)
		{
			curmode = i;
			break;
		}
	}
	curmode++;
	if (curmode >= VIDEOMENU_VIDEOFORMAT_OPTION_COUNT)
		curmode = 0;
	if (VIDEOMENU_VIDEOFORMAT_OPTIONS[curmode].key == DISPLAY_AR_14_9 && g_info.hw_caps->can_ar_14_9 == 0)
		curmode = 0;
	text = VIDEOMENU_VIDEOFORMAT_OPTIONS[curmode].value;
	g_settings.video_Format = VIDEOMENU_VIDEOFORMAT_OPTIONS[curmode].key;

	videoDecoder->setAspectRatio(g_settings.video_Format, -1);
#if ENABLE_PIP
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->setAspectRatio(g_settings.video_Format, -1);
#endif
	ShowHint(LOCALE_VIDEOMENU_VIDEOFORMAT, g_Locale->getText(text), 450, 2);
}

void CVideoSettings::nextMode(void)
{
	dprintf(DEBUG_NORMAL, "[CVideoSettings] [%s - %d], setting video mode...\n", __func__, __LINE__);
	const char *text;
	int curmode = 0;
	int i;
	bool disp_cur = 1;
	int res = messages_return::none;

	for (i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++)
	{
		if (VIDEOMENU_VIDEOMODE_OPTIONS[i].key == g_settings.video_Mode)
		{
			curmode = i;
			break;
		}
	}
	text = VIDEOMENU_VIDEOMODE_OPTIONS[curmode].valname;

	while (1)
	{
		CVFD::getInstance()->ShowText(text);

		if (res != messages_return::cancel_info) // avoid unnecessary display of messageboxes, when user is trying to press repeated format button
			res = ShowHint(LOCALE_VIDEOMENU_VIDEOMODE, text, 450, 2);

		if (disp_cur && res != messages_return::handled)
			break;

		disp_cur = 0;

		if (res == messages_return::handled)
		{
			i = 0;
			while (true)
			{
				curmode++;
				if (curmode >= VIDEOMENU_VIDEOMODE_OPTION_COUNT)
					curmode = 0;
				if (VIDEOMENU_VIDEOMODE_OPTIONS[curmode].key == -1)
					continue;
				if (g_settings.enabled_video_modes[curmode])
					break;
				i++;
				if (i >= VIDEOMENU_VIDEOMODE_OPTION_COUNT)
				{
					CVFD::getInstance()->showServicename(g_RemoteControl->getCurrentChannelName(), g_RemoteControl->getCurrentChannelNumber());
					return;
				}
			}

			text = VIDEOMENU_VIDEOMODE_OPTIONS[curmode].valname;
		}
		else if (res == messages_return::cancel_info)
		{
			g_settings.video_Mode = VIDEOMENU_VIDEOMODE_OPTIONS[curmode].key;
			//CVFD::getInstance()->ShowText(text);
			COsdHelpers::getInstance()->setVideoSystem(g_settings.video_Mode);
			COsdHelpers::getInstance()->changeOsdResolution(0, true, false);
			//return;
			disp_cur = 1;
		}
		else
			break;
	}
	CVFD::getInstance()->showServicename(g_RemoteControl->getCurrentChannelName(), g_RemoteControl->getCurrentChannelNumber());
	//ShowHint(LOCALE_VIDEOMENU_VIDEOMODE, text, 450, 2);
}

void CVideoSettings::Init43ModeOptions()
{
	videomenu_43mode_options.clear();
	for (unsigned int i = 0; i < VIDEOMENU_43MODE_OPTION_COUNT; i++)
	{
		if (VIDEOMENU_43MODE_OPTIONS[i].key == DISPLAY_AR_MODE_PANSCAN2 && g_info.hw_caps->can_ps_14_9 == 0)
			continue;
		CMenuOptionChooser::keyval_ext o;
		o = VIDEOMENU_43MODE_OPTIONS[i];
		videomenu_43mode_options.push_back(o);
	}
}
