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


#include <zapit/client/zapitclient.h>

#include <controldclient/controldclient.h>
#include <sectionsdclient/sectionsdclient.h>
#include <timerdclient/timerdclient.h>

#include "driver/fontrenderer.h"
#include "driver/rcinput.h"
#include "driver/vfd.h"
#include "driver/rfmod.h"

#include "system/localize.h"
#include "system/settings.h"

#include "gui/epgview.h"
#include "gui/infoviewer.h"
#include "gui/eventlist.h"

#ifndef NEUTRINO_CPP
  #define NEUTRINO_CPP extern
#endif

#define NEUTRINO_SETTINGS_FILE          CONFIGDIR "/neutrino.conf"

#define NEUTRINO_RECORDING_TIMER_SCRIPT CONFIGDIR "/recording.timer"
#define NEUTRINO_RECORDING_START_SCRIPT CONFIGDIR "/recording.start"
#define NEUTRINO_RECORDING_ENDED_SCRIPT CONFIGDIR "/recording.end"
#define NEUTRINO_ENTER_STANDBY_SCRIPT   CONFIGDIR "/standby.on"
#define NEUTRINO_LEAVE_STANDBY_SCRIPT   CONFIGDIR "/standby.off"

#define NEUTRINO_SCAN_SETTINGS_FILE     CONFIGDIR "/scan.conf"
#define NEUTRINO_PARENTALLOCKED_FILE    DATADIR   "/neutrino/.plocked"

NEUTRINO_CPP  SNeutrinoSettings	g_settings;
NEUTRINO_CPP  SglobalInfo		g_info;

NEUTRINO_CPP  CControldClient	*g_Controld;
NEUTRINO_CPP  CZapitClient		*g_Zapit;
NEUTRINO_CPP  CSectionsdClient	*g_Sectionsd;
NEUTRINO_CPP  CTimerdClient		*g_Timerd;

NEUTRINO_CPP  FBFontRenderClass	*g_fontRenderer;

NEUTRINO_CPP  Font * g_Font[FONT_TYPE_COUNT];
NEUTRINO_CPP  Font * g_SignalFont;

NEUTRINO_CPP  CRCInput			*g_RCInput;

NEUTRINO_CPP  CEpgData			*g_EpgData;
NEUTRINO_CPP  CInfoViewer		*g_InfoViewer;
NEUTRINO_CPP  EventList			*g_EventList;

NEUTRINO_CPP CLocaleManager		*g_Locale;
NEUTRINO_CPP RFmod		*g_RFmod;

#endif /* __neutrino_global_h__ */
