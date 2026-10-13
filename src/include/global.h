/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

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
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __neutrino_global_h__
#define __neutrino_global_h__

#include <system/settings.h>

#ifndef NEUTRINO_CPP
#define NEUTRINO_CPP extern
#endif

#define IMAGE_VERSION_FILE TARGET_ROOT "/.version"
#define IMAGE_METADATA_FILE TARGET_ROOT "/etc/image-version"

#define NEUTRINO_SETTINGS_FILE CONFIGDIR "/neutrino.conf"
/* Who may reach the box over the web server: the one login (its password as a
   hash), the API tokens, the sessions, who may read without signing in and
   which proxies are believed. Beside the server's own file and not inside it,
   because a credential does not belong in a file that the settings menu, the
   old configuration route and the server itself each rewrite wholesale. Absent
   on a box nobody has configured, and the API layer then runs on what it
   declares as its own defaults. */
#define HTTPD_ACCESS_FILE CONFIGDIR "/nhttpd-access.conf"
/* nhttpd's own file: how the server runs (its port and address, threading, the
   directories it serves, the page login). The one place those are kept; the
   API layer reads and writes them here. The first start without the file
   above also takes the user name and the password out of it, hashed, so the
   API layer starts with the credentials this box already has.

   Named beside the file above because the storage layer refuses to hand out
   either of them; the check that holds those two lists together reads both
   names from here. */
#define HTTPD_SERVER_FILE CONFIGDIR "/nhttpd.conf"
/* Where builds before nhttpd-access.conf, and boxes from the image this layer
   was taken from, kept the file above. Taken over once, on the first start
   without nhttpd-access.conf, and then removed; one found beside it is a
   leftover holding an old password hash and is removed as well. */
#define HTTPD_ACCESS_FORMER_FILE CONFIGDIR "/ni-web.conf"
#define NEUTRINO_SCAN_SETTINGS_FILE CONFIGDIR "/scan.conf"
#define NEUTRINO_PARENTALLOCKED_FILE DATADIR "/neutrino/.plocked"

// control scripts
#define NEUTRINO_RECORDING_TIMER_SCRIPT "recording.timer"
#define NEUTRINO_RECORDING_START_SCRIPT "recording.start"
#define NEUTRINO_RECORDING_ENDED_SCRIPT "recording.end"
#define NEUTRINO_ENTER_STANDBY_SCRIPT "standby.on"
#define NEUTRINO_LEAVE_STANDBY_SCRIPT "standby.off"
#define NEUTRINO_ENTER_INACTIVITY_SCRIPT "inactivity.on"
#define NEUTRINO_ENTER_DEEPSTANDBY_SCRIPT "deepstandby.on"
#define NEUTRINO_LEAVE_DEEPSTANDBY_SCRIPT "deepstandby.off"
#define NEUTRINO_APP_START_SCRIPT "neutrino.start"

// control scripts w/o counterparts in /var
#define NEUTRINO_ENTER_FLASH_SCRIPT CONTROLDIR "/flash.start"
//#define NEUTRINO_CONF_MIGRATION_SCRIPT CONTROLDIR "/migration.sh"

#define COVERDIR_TMP "/tmp/.cover"
#define LOGODIR_TMP "/tmp/.logo"

NEUTRINO_CPP SNeutrinoSettings g_settings;
NEUTRINO_CPP SglobalInfo g_info;

class FBFontRenderClass;
NEUTRINO_CPP FBFontRenderClass *g_fontRenderer;
NEUTRINO_CPP FBFontRenderClass *g_fixedFontRenderer;
NEUTRINO_CPP FBFontRenderClass *g_dynFontRenderer;
NEUTRINO_CPP FBFontRenderClass *g_shellFontRenderer;
NEUTRINO_CPP FBFontRenderClass *g_iconFontRenderer;

class Font;
NEUTRINO_CPP Font *g_Font[SNeutrinoSettings::FONT_TYPE_COUNT];
NEUTRINO_CPP Font *g_FixedFont[SNeutrinoSettings::FONT_TYPE_FIXED_COUNT];
NEUTRINO_CPP Font *g_SignalFont;
NEUTRINO_CPP Font *g_ShellFont;
NEUTRINO_CPP Font *g_IconFont;

#ifdef HAVE_CONTROLD
class CControldClient;
NEUTRINO_CPP CControldClient *g_Controld;
#endif

class CZapitClient;
NEUTRINO_CPP CZapitClient *g_Zapit;

class CSectionsdClient;
NEUTRINO_CPP CSectionsdClient *g_Sectionsd;

class CTimerdClient;
NEUTRINO_CPP CTimerdClient *g_Timerd;

class CRCInput;
NEUTRINO_CPP CRCInput *g_RCInput;

class CEpgData;
NEUTRINO_CPP CEpgData *g_EpgData;

class CInfoViewer;
NEUTRINO_CPP CInfoViewer *g_InfoViewer;

class CEventList;
NEUTRINO_CPP CEventList *g_EventList;

class CLocaleManager;
NEUTRINO_CPP CLocaleManager *g_Locale;

class CVideoSettings;
NEUTRINO_CPP CVideoSettings *g_videoSettings;

class CRadioText;
NEUTRINO_CPP CRadioText *g_Radiotext;

class CRadioTextGUI;
NEUTRINO_CPP CRadioTextGUI *g_RadiotextWin;

#ifndef DISABLE_GUI_MOUNT
#define ENABLE_GUI_MOUNT
#endif

#endif /* __neutrino_global_h__ */
