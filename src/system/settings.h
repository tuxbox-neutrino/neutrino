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

#ifndef __settings__
#define __settings__

#include <system/localize.h>
#include <configfile.h>
#include <zapit/client/zapitclient.h>

#include <string>

#define VIDEOMENU_VIDEOMODE_OPTION_COUNT 12

struct SNeutrinoSettings
{
	//video
	int video_Format;
	int video_Mode;
	int analog_mode1;
	int analog_mode2;
	int video_43mode;
	unsigned char video_csync;
	char current_volume;
	int channel_mode;
	int channel_mode_radio;

	//misc
	int shutdown_real;
	int shutdown_real_rcdelay;
	char shutdown_count[4];
	char record_safety_time_before[3];
	char record_safety_time_after[3];
	int infobar_sat_display;
	int infobar_subchan_disp_pos;
	int misc_spts;
	int fan_speed;
	int infobar_show;
	int show_infomenu;
	int infobar_show_channellogo;
	int progressbar_color;
	int casystem_display;
	int scrambled_message;
	int volume_pos;
	int menu_pos;
	int infobar_show_var_hdd;
	//audio
	int audio_AnalogMode;
	int audio_DolbyDigital;
	int audio_avs_Control;
	int auto_lang;
	int auto_subs;
	char audio_PCMOffset[3];
	int srs_enable;
	int srs_algo;
	int srs_ref_volume;
	int srs_nmgr_enable;
	int hdmi_dd;
	int spdif_dd;
	int analog_out;
	//video
	int video_dbdr;
	int hdmi_cec_mode;
	int hdmi_cec_view_on;
	int hdmi_cec_standby;
	int enabled_video_modes[VIDEOMENU_VIDEOMODE_OPTION_COUNT];
	int cpufreq;
	int standby_cpufreq;
	int make_hd_list;
	int avsync;
	int clockrec;
	int rounded_corners;
	int ci_standby_reset;

	//vcr
	int vcr_AutoSwitch;

	//language
	char language[25];
	char timezone[150];

	char pref_lang[3][30];
	char pref_subs[3][30];

	// EPG
	int epg_save;
	std::string epg_cache;
	std::string epg_old_events;
	std::string epg_max_events;
	std::string epg_extendedcache;
	std::string epg_dir;
	std::string network_ntpserver;
	std::string network_ntprefresh;
	int network_ntpenable;

	//timing
#define TIMING_SETTING_COUNT 6
	enum TIMING_SETTINGS {
		TIMING_MENU        = 0,
		TIMING_CHANLIST    = 1,
		TIMING_EPG         = 2,
		TIMING_INFOBAR     = 3,
		TIMING_FILEBROWSER = 4,
		TIMING_NUMERICZAP  = 5
	};

	int  timing       [TIMING_SETTING_COUNT]   ;
	char timing_string[TIMING_SETTING_COUNT][4];

	//widget settings
	int widget_fade;

	//colors
	unsigned char gtx_alpha1;
	unsigned char gtx_alpha2;

	unsigned char menu_Head_alpha;
	unsigned char menu_Head_red;
	unsigned char menu_Head_green;
	unsigned char menu_Head_blue;

	unsigned char menu_Head_Text_alpha;
	unsigned char menu_Head_Text_red;
	unsigned char menu_Head_Text_green;
	unsigned char menu_Head_Text_blue;

	unsigned char menu_Content_alpha;
	unsigned char menu_Content_red;
	unsigned char menu_Content_green;
	unsigned char menu_Content_blue;

	unsigned char menu_Content_Text_alpha;
	unsigned char menu_Content_Text_red;
	unsigned char menu_Content_Text_green;
	unsigned char menu_Content_Text_blue;

	unsigned char menu_Content_Selected_alpha;
	unsigned char menu_Content_Selected_red;
	unsigned char menu_Content_Selected_green;
	unsigned char menu_Content_Selected_blue;

	unsigned char menu_Content_Selected_Text_alpha;
	unsigned char menu_Content_Selected_Text_red;
	unsigned char menu_Content_Selected_Text_green;
	unsigned char menu_Content_Selected_Text_blue;

	unsigned char menu_Content_inactive_alpha;
	unsigned char menu_Content_inactive_red;
	unsigned char menu_Content_inactive_green;
	unsigned char menu_Content_inactive_blue;

	unsigned char menu_Content_inactive_Text_alpha;
	unsigned char menu_Content_inactive_Text_red;
	unsigned char menu_Content_inactive_Text_green;
	unsigned char menu_Content_inactive_Text_blue;

	unsigned char infobar_alpha;
	unsigned char infobar_red;
	unsigned char infobar_green;
	unsigned char infobar_blue;

	unsigned char infobar_Text_alpha;
	unsigned char infobar_Text_red;
	unsigned char infobar_Text_green;
	unsigned char infobar_Text_blue;

	//network
#define NETWORK_NFS_NR_OF_ENTRIES 8
	std::string network_nfs_ip[NETWORK_NFS_NR_OF_ENTRIES];
	char network_nfs_mac[NETWORK_NFS_NR_OF_ENTRIES][31];
	char network_nfs_local_dir[NETWORK_NFS_NR_OF_ENTRIES][100];
	char network_nfs_dir[NETWORK_NFS_NR_OF_ENTRIES][100];
	int  network_nfs_automount[NETWORK_NFS_NR_OF_ENTRIES];
	char network_nfs_mount_options1[NETWORK_NFS_NR_OF_ENTRIES][31];
	char network_nfs_mount_options2[NETWORK_NFS_NR_OF_ENTRIES][31];
	int  network_nfs_type[NETWORK_NFS_NR_OF_ENTRIES];
	char network_nfs_username[NETWORK_NFS_NR_OF_ENTRIES][31];
	char network_nfs_password[NETWORK_NFS_NR_OF_ENTRIES][31];
	char network_nfs_audioplayerdir[100];
	char network_nfs_picturedir[100];
	char network_nfs_moviedir[100];
	char network_nfs_recordingdir[100];
	char timeshiftdir[100];

	//recording
	int  recording_type;
	int  recording_stopplayback;
	int  recording_stopsectionsd;
	std::string recording_server_ip;
	char recording_server_port[10];
	int  recording_server_wakeup;
	char recording_server_mac[31];
	int  recording_vcr_no_scart;
	char recording_splitsize[10];
	int  recording_use_o_sync;
	int  recording_use_fdatasync;
	unsigned char recording_audio_pids_default;
	int recording_audio_pids_std;
	int recording_audio_pids_alt;
	int recording_audio_pids_ac3;
	int  recording_stream_vtxt_pid;
	int  recording_stream_pmt_pid;
	char recording_ringbuffers[10];
	int recording_choose_direct_rec_dir;
	int recording_epg_for_filename;
	int recording_save_in_channeldir;
	int recording_in_spts_mode;
	int  recording_zap_on_announce;

	//streaming
	int  streaming_type;
	std::string streaming_server_ip;
	char streaming_server_port[10];
	char streaming_server_cddrive[21];
	char streaming_videorate[6];
	char streaming_audiorate[6];
	char streaming_server_startdir[40];
	int streaming_transcode_audio;
	int streaming_force_avi_rawaudio;
	int streaming_force_transcode_video;
	int streaming_transcode_video_codec;
	int streaming_resolution;

	int filesystem_is_utf8;
	// default plugin for ts-movieplayer (red button)
	std::string movieplayer_plugin;
	std::string onekey_plugin;
	std::string plugin_hdd_dir;
	//key configuration
	int key_tvradio_mode;

	int key_channelList_pageup;
	int key_channelList_pagedown;
	int key_channelList_cancel;
	int key_channelList_sort;
	int key_channelList_addrecord;
	int key_channelList_addremind;

	int key_quickzap_up;
	int key_quickzap_down;
	int key_bouquet_up;
	int key_bouquet_down;
	int key_subchannel_up;
	int key_subchannel_down;
	int key_zaphistory;
	int key_lastchannel;
	int key_list_start;
	int key_list_end;
	int key_power_off;
	int menu_left_exit;
	int audio_run_player;
	int key_click;
	int timeshift_pause;
	int auto_timeshift;
	int temp_timeshift;
	int auto_delete;
	int record_hours;

	int mpkey_rewind;
	int mpkey_forward;
	int mpkey_pause;
	int mpkey_stop;
	int mpkey_play;
	int mpkey_audio;
	int mpkey_time;
	int mpkey_bookmark;
	int mpkey_plugin;
	int key_timeshift;
	int key_plugin;

	int rf_subcarrier;
	int rf_soundenable;
	int rf_channel;
	int rf_finetune;
	int rf_standby;

	int key_unlock;
	int cacheTXT;
	int minimode;
	int mode_clock;
	int virtual_zap_mode;
	int spectrum;
	int pip_width;
	int pip_height;
	int pip_x;
	int pip_y;
	int bigFonts;
	int channellist_epgtext_align_right;
	int channellist_extended;

	char repeat_blocker[4];
	char repeat_genericblocker[4];
	int audiochannel_up_down_enable;

	//screen configuration
	int screen_StartX;
	int screen_StartY;
	int screen_EndX;
	int screen_EndY;
	int screen_StartX_crt;
	int screen_StartY_crt;
	int screen_EndX_crt;
	int screen_EndY_crt;
	int screen_StartX_lcd;
	int screen_StartY_lcd;
	int screen_EndX_lcd;
	int screen_EndY_lcd;
	int screen_preset;
	int screen_width;
	int screen_height;

	//Software-update
	int softupdate_mode;
	char softupdate_url_file[31];
	char softupdate_proxyserver[31];
	char softupdate_proxyusername[31];
	char softupdate_proxypassword[31];

	//BouquetHandling
	int bouquetlist_mode;

	// parentallock
	int parentallock_prompt;
	int parentallock_lockage;
	char parentallock_pincode[5];


	// Font sizes
#define FONT_TYPE_COUNT 22
	enum FONT_TYPES {
		FONT_TYPE_MENU                =  0,
		FONT_TYPE_MENU_TITLE          =  1,
		FONT_TYPE_MENU_INFO           =  2,
		FONT_TYPE_EPG_TITLE           =  3,
		FONT_TYPE_EPG_INFO1           =  4,
		FONT_TYPE_EPG_INFO2           =  5,
		FONT_TYPE_EPG_DATE            =  6,
		FONT_TYPE_EVENTLIST_TITLE     =  7,
		FONT_TYPE_EVENTLIST_ITEMLARGE =  8,
		FONT_TYPE_EVENTLIST_ITEMSMALL =  9,
		FONT_TYPE_EVENTLIST_DATETIME  = 10,
		FONT_TYPE_GAMELIST_ITEMLARGE  = 11,
		FONT_TYPE_GAMELIST_ITEMSMALL  = 12,
		FONT_TYPE_CHANNELLIST         = 13,
		FONT_TYPE_CHANNELLIST_DESCR   = 14,
		FONT_TYPE_CHANNELLIST_NUMBER  = 15,
		FONT_TYPE_CHANNEL_NUM_ZAP     = 16,
		FONT_TYPE_INFOBAR_NUMBER      = 17,
		FONT_TYPE_INFOBAR_CHANNAME    = 18,
		FONT_TYPE_INFOBAR_INFO        = 19,
		FONT_TYPE_INFOBAR_SMALL       = 20,
		FONT_TYPE_FILEBROWSER_ITEM    = 21
	};

	// lcdd
#define LCD_SETTING_COUNT 8
	enum LCD_SETTINGS {
		LCD_BRIGHTNESS         = 0,
		LCD_STANDBY_BRIGHTNESS = 1,
		LCD_CONTRAST           = 2,
		LCD_POWER              = 3,
		LCD_INVERSE            = 4,
		LCD_SHOW_VOLUME        = 5,
		LCD_AUTODIMM           = 6,
		LCD_DEEPSTANDBY_BRIGHTNESS  = 7
	};
	int lcd_setting[LCD_SETTING_COUNT];

	char lcd_setting_dim_time[4];
	char lcd_setting_dim_brightness[4];
	int led_tv_mode;
	int led_standby_mode;
	int led_deep_mode;
	int led_blink;
#define FILESYSTEM_ENCODING_TO_UTF8(a) (g_settings.filesystem_is_utf8 ? (a) : ZapitTools::Latin1_to_UTF8(a).c_str())
#define UTF8_TO_FILESYSTEM_ENCODING(a) (g_settings.filesystem_is_utf8 ? (a) : ZapitTools::UTF8_to_Latin1(a).c_str())
#define FILESYSTEM_ENCODING_TO_UTF8_STRING(a) (g_settings.filesystem_is_utf8 ? (a) : Latin1_to_UTF8(a))


#if HAVE_DVB_API_VERSION == 1
#define MISC_SETTING_FILES_COUNT 7
#else
#define MISC_SETTING_FILES_COUNT 4
#endif

// #define MISC_SETTING_SPTS_MODE 0

	int misc_option[MISC_SETTING_FILES_COUNT];


	// pictureviewer
	char   picviewer_slide_time[3];
	int    picviewer_scaling;
	std::string picviewer_decode_server_ip;
	char    picviewer_decode_server_port[6];

	//audioplayer
	int   audioplayer_display;
	int   audioplayer_follow;
	char  audioplayer_screensaver[3];
	int   audioplayer_highprio;
	int   audioplayer_select_title_by_name;
	int   audioplayer_repeat_on;
	int   audioplayer_show_playlist;
	int   audioplayer_enable_sc_metadata;

	//Filebrowser
	int filebrowser_showrights;
	int filebrowser_sortmethod;
	int filebrowser_denydirectoryleave;

	//uboot
	int	uboot_lcd_inverse;
	int	uboot_lcd_contrast;
	int	uboot_console;
	int	uboot_console_bak;
	int	power_standby;
	int	emlog;
	int	rotor_swap;
	int	ts_mode;
	int	hw_sect;
	int	hdd_sleep;
	int	hdd_noise;
	int	hdd_fs;
	int	logo_num;
	int	zap_cycle;
	int	sms_channel;
	char	font_file[100];
	char	ttx_font_file[100];
	char	update_dir[100];
	// USERMENU
	typedef enum
	{
		BUTTON_RED = 0,  // Do not change ordering of members, add new item just before BUTTON_MAX!!!
		BUTTON_GREEN = 1,
		BUTTON_YELLOW = 2,
		BUTTON_BLUE = 3,
		BUTTON_MAX   // MUST be always the last in the list
	} USER_BUTTON;
	typedef enum
	{
		ITEM_NONE = 0, // Do not change ordering of members, add new item just before ITEM_MAX!!!
		ITEM_BAR = 1,
		ITEM_EPG_LIST = 2,
		ITEM_EPG_SUPER = 3,
		ITEM_EPG_INFO = 4,
		ITEM_EPG_MISC = 5,
		ITEM_AUDIO_SELECT = 6,
		ITEM_SUBCHANNEL = 7,
		ITEM_RECORD = 8,
		ITEM_MOVIEPLAYER_MB = 9,
		ITEM_TIMERLIST = 10,
		ITEM_FAVORITS = 12,
		ITEM_VTXT = 11,
		ITEM_TECHINFO = 13,
		ITEM_REMOTE = 14,
		ITEM_PLUGIN = 15,
#if 0
		ITEM_MOVIEPLAYER_TS = 16,
		ITEM_RESTART_CAMD = 17,
#endif
		ITEM_MAX   // MUST be always the last in the list
	} USER_ITEM;
	std::string usermenu_text[BUTTON_MAX];
	int usermenu[BUTTON_MAX][ITEM_MAX];  // (USER_ITEM)  [button][position in Menue] = feature item

};

/* some default Values */

extern const int               default_timing     [TIMING_SETTING_COUNT];
extern const neutrino_locale_t timing_setting_name[TIMING_SETTING_COUNT];

// lcdd
#define DEFAULT_VFD_BRIGHTNESS			15
#define DEFAULT_VFD_STANDBYBRIGHTNESS		5

#define DEFAULT_LCD_BRIGHTNESS			0xff
#define DEFAULT_LCD_STANDBYBRIGHTNESS		0xaa
#define DEFAULT_LCD_CONTRAST			0x0F
#define DEFAULT_LCD_POWER			0x01
#define DEFAULT_LCD_INVERSE			0x00
#define DEFAULT_LCD_AUTODIMM			0x00
#define DEFAULT_LCD_SHOW_VOLUME			0x01

#define CORNER_RADIUS_LARGE             9
#define CORNER_RADIUS_MID               8
#define CORNER_RADIUS_SMALL             7

#define RADIUS_LARGE    (g_settings.rounded_corners ? CORNER_RADIUS_LARGE : 0)
#define RADIUS_MID      (g_settings.rounded_corners ? CORNER_RADIUS_MID : 0)
#define RADIUS_SMALL    (g_settings.rounded_corners ? CORNER_RADIUS_SMALL : 0)

// shadow
#define SHADOW_OFFSET                   6

/* end default values */

struct SglobalInfo
{
	unsigned char     box_Type;
	delivery_system_t delivery_system;
};

const int RECORDING_OFF    = 0;
const int RECORDING_SERVER = 1;
const int RECORDING_VCR    = 2;
const int RECORDING_FILE   = 3;

const int PARENTALLOCK_PROMPT_NEVER          = 0;
const int PARENTALLOCK_PROMPT_ONSTART        = 1;
const int PARENTALLOCK_PROMPT_CHANGETOLOCKED = 2;
const int PARENTALLOCK_PROMPT_ONSIGNAL       = 3;

#define MAX_SATELLITES 80
#define get_set CNeutrinoApp::getInstance()->getScanSettings()

class CScanSettings
{
public:
	CConfigFile	configfile;
	int		bouquetMode;
	int		scanType;
	int		diseqcMode;
	uint32_t	diseqcRepeat;
	char                      satNameNoDiseqc[50];
	delivery_system_t         delivery_system;
	int		scanSectionsd;
	int		scan_mode;
	int		scan_fta_flag;
	int		TP_fec;
	int		TP_pol;
	int		TP_mod;
	char		TP_freq[10];
	char		TP_rate[9];
	int		fast_type;
	int		fast_op;

	CScanSettings();

	void useDefaults(const delivery_system_t _delivery_system);
	bool loadSettings(const char * const fileName, const delivery_system_t _delivery_system);
	bool saveSettings(const char * const fileName);
};


#endif
