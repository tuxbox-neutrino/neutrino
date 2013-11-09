#ifndef __neutrino_global_h__
#define __neutrino_global_h__
/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <system/settings.h>

#ifndef NEUTRINO_CPP
  #define NEUTRINO_CPP extern
#endif

#define NEUTRINO_SETTINGS_FILE          CONFIGDIR "/neutrino.conf"

#define NEUTRINO_RECORDING_TIMER_SCRIPT CONFIGDIR "/recording.timer"
#define NEUTRINO_RECORDING_START_SCRIPT CONFIGDIR "/recording.start"
#define NEUTRINO_RECORDING_ENDED_SCRIPT CONFIGDIR "/recording.end"
#define NEUTRINO_ENTER_STANDBY_SCRIPT   CONFIGDIR "/standby.on"
#define NEUTRINO_LEAVE_STANDBY_SCRIPT   CONFIGDIR "/standby.off"
#define NEUTRINO_ENTER_INACTIVITY_SCRIPT	CONFIGDIR "/inactivity.on"
#define NEUTRINO_ENTER_DEEPSTANDBY_SCRIPT	CONFIGDIR "/deepstandby.on"
#define NEUTRINO_LEAVE_DEEPSTANDBY_SCRIPT	CONFIGDIR "/deepstandby.off"
#define MOVIEPLAYER_START_SCRIPT        CONFIGDIR "/movieplayer.start"
#define MOVIEPLAYER_END_SCRIPT          CONFIGDIR "/movieplayer.end"
#define NEUTRINO_ENTER_FLASH_SCRIPT	CONFIGDIR "/flash.start"

#define NEUTRINO_SCAN_SETTINGS_FILE     CONFIGDIR "/scan.conf"
#define NEUTRINO_PARENTALLOCKED_FILE    DATADIR   "/neutrino/.plocked"

NEUTRINO_CPP  SNeutrinoSettings	g_settings;
NEUTRINO_CPP  SglobalInfo	g_info;

#ifdef HAVE_CONTROLD
class CControldClient;
NEUTRINO_CPP  CControldClient	*g_Controld;
#endif
class CZapitClient;
NEUTRINO_CPP  CZapitClient	*g_Zapit;
class CSectionsdClient;
NEUTRINO_CPP  CSectionsdClient	*g_Sectionsd;
class CTimerdClient;
NEUTRINO_CPP  CTimerdClient	*g_Timerd;

class FBFontRenderClass;
NEUTRINO_CPP  FBFontRenderClass	*g_fontRenderer;
NEUTRINO_CPP  FBFontRenderClass	*g_dynFontRenderer;

class Font;
NEUTRINO_CPP  Font * g_Font[SNeutrinoSettings::FONT_TYPE_COUNT];
NEUTRINO_CPP  Font * g_SignalFont;

class CRCInput;
NEUTRINO_CPP  CRCInput		*g_RCInput;

class CEpgData;
NEUTRINO_CPP  CEpgData		*g_EpgData;
class CInfoViewer;
NEUTRINO_CPP  CInfoViewer	*g_InfoViewer;
class CNeutrinoEventList;
NEUTRINO_CPP  CNeutrinoEventList		*g_EventList;

class CLocaleManager;
NEUTRINO_CPP CLocaleManager	*g_Locale;
class CVideoSettings;
NEUTRINO_CPP CVideoSettings	*g_videoSettings;
class CRadioText;
NEUTRINO_CPP CRadioText		*g_Radiotext;

#ifndef DISABLE_GUI_MOUNT
#define ENABLE_GUI_MOUNT
#endif

#endif /* __neutrino_global_h__ */
