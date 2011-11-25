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


#ifndef __neutrino__
#define __neutrino__

#include <configfile.h>

#include <neutrinoMessages.h>
#include <driver/framebuffer.h>
#include <system/setting_helpers.h>
#include <system/configure_network.h>
#include <gui/timerlist.h>
#include <gui/network_setup.h>
#include <timerdclient/timerdtypes.h>
#include <gui/channellist.h>          /* CChannelList */
#include <gui/rc_lock.h>
#include <daemonc/remotecontrol.h>    /* st_rmsg      */

#include <gui/user_menue.h>
#include <zapit/client/zapitclient.h>

#include <string>

#define widest_number "2"

#define ANNOUNCETIME (1 * 60)

/**************************************************************************************
*                                                                                     *
*          CNeutrinoApp -  main run-class                                             *
*                                                                                     *
**************************************************************************************/

typedef struct neutrino_font_descr
{
	const char * name;
	const char * filename;
	int          size_offset;
} neutrino_font_descr_struct;

typedef struct font_sizes
{
	const neutrino_locale_t name;
	const unsigned int      defaultsize;
	const unsigned int      style;
	const unsigned int      size_offset;
} font_sizes_struct;

typedef struct font_sizes_groups
{
	const neutrino_locale_t                     groupname;
	const unsigned int                          count;
	const SNeutrinoSettings::FONT_TYPES * const content;
	const char * const                          actionkey;
} font_sizes_groups_struct;

class CNeutrinoApp : public CMenuTarget, CChangeObserver
{
public:
	enum
	{
		RECORDING_OFF    = 0,
		RECORDING_SERVER = 1,
		RECORDING_VCR    = 2,
		RECORDING_FILE   = 3
	};
	
	

private:
	CFrameBuffer * frameBuffer;

	enum
	{
		mode_unknown = -1,
		mode_tv = 1,
		mode_radio = 2,
		mode_scart = 3,
		mode_standby = 4,
		mode_audio = 5,
		mode_pic = 6,
		mode_ts = 7,
		mode_off = 8,
		mode_mask = 0xFF,
		norezap = 0x100
	};

	CConfigFile			configfile;
	CScanSettings			scanSettings;
	CUserMenu 			usermenu;
	int                             network_dhcp;
	int                             network_automatic_start;

	neutrino_font_descr_struct      font;

	int				mode;
	int				lastMode;
	bool				softupdate;
//	bool				fromflash;
	bool 				init_cec_setting;
	int				lastChannelMode;

	struct timeval                  standby_pressed_at;

	CZapitClient::responseGetLastChannel    firstchannel;
	st_rmsg				sendmessage;

	int				current_muted;

	bool				skipShutdownTimer;
	bool 				pbBlinkChange;
	CColorSetupNotifier		*colorSetupNotifier;
	CNetworkSetup			*networksetup;
	CMoviePluginChangeExec 		*MoviePluginChanger;
	COnekeyPluginChangeExec		*OnekeyPluginChanger;
	CIPChangeNotifier		*MyIPChanger;

	void firstChannel();
	void setupNetwork( bool force= false );
	void setupNFS();

	void tvMode( bool rezap = true );
	void radioMode( bool rezap = true );
	void scartMode( bool bOnOff );
	void standbyMode( bool bOnOff );
	void AudioMute( int newValue, bool isEvent= false );
	void setvol(int vol, int avs);
	void saveEpg();

	void ExitRun(const bool write_si = true, int retcode = 0);
	void RealRun(CMenuWidget &mainSettings);
	void InitZapper();
	void InitServiceSettings(CMenuWidget &);
	void InitScreenSettings(CMenuWidget &);
	void InitMainMenu(CMenuWidget &mainMenu, CMenuWidget &mainSettings, CMenuWidget &service);

	void SetupFrameBuffer();
	void CmdParser(int argc, char **argv);
	CNeutrinoApp();

public:
	void saveSetup(const char * fname);
	int loadSetup(const char * fname);
	void loadKeys(const char * fname = NULL);
	void saveKeys(const char * fname = NULL);
	void SetupTiming();
	void SetupFonts();
	void setupRecordingDevice(void);
	
	void setVolume(const neutrino_msg_t key, const bool bDoPaint = true, bool nowait = false);
	~CNeutrinoApp();
	CScanSettings& getScanSettings() {
		return scanSettings;
	};

	CChannelList			*TVchannelList;
	CChannelList			*RADIOchannelList;
	CChannelList			*channelList;

	static CNeutrinoApp* getInstance();

	void channelsInit(bool bOnly = false);
	int run(int argc, char **argv);

	//callback stuff only....
	int exec(CMenuTarget* parent, const std::string & actionKey);

// 	//onchange
 	bool changeNotify(const neutrino_locale_t OptionName, void *);

	int handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data);

	int getMode() {
		return mode;
	}
	int getLastMode() {
		return lastMode;
	}
	bool isMuted() {
		return current_muted;
	}
	int recordingstatus;
	void SendSectionsdConfig(void);
	int GetChannelMode(void) {
		return lastChannelMode;
	};
	void SetChannelMode(int mode);
	void quickZap(int msg);
	void StopSubtitles();
	void StartSubtitles(bool show = true);
	void SelectSubtitles();
	void showInfo(void);
	CConfigFile* getConfigFile() {return &configfile;};
};
#endif


