/*
	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2008-2013 Stefan Seyfried

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

#ifndef __simple_display__
#define __simple_display__

#define CVFD CLCD

typedef enum
{
	FP_ICON_BAR8       = 0x00000004,
	FP_ICON_BAR7       = 0x00000008,
	FP_ICON_BAR6       = 0x00000010,
	FP_ICON_BAR5       = 0x00000020,
	FP_ICON_BAR4       = 0x00000040,
	FP_ICON_BAR3       = 0x00000080,
	FP_ICON_BAR2       = 0x00000100,
	FP_ICON_BAR1       = 0x00000200,
	FP_ICON_FRAME      = 0x00000400,
	FP_ICON_HDD        = 0x00000800,
	FP_ICON_MUTE       = 0x00001000,
	FP_ICON_DOLBY      = 0x00002000,
	FP_ICON_POWER      = 0x00004000,
	FP_ICON_TIMESHIFT  = 0x00008000,
	FP_ICON_SIGNAL     = 0x00010000,
	FP_ICON_TV         = 0x00020000,
	FP_ICON_RADIO      = 0x00040000,
	FP_ICON_HD         = 0x01000001,
	FP_ICON_1080P      = 0x02000001,
	FP_ICON_1080I      = 0x03000001,
	FP_ICON_720P       = 0x04000001,
	FP_ICON_480P       = 0x05000001,
	FP_ICON_480I       = 0x06000001,
	FP_ICON_USB        = 0x07000001,
	FP_ICON_MP3        = 0x08000001,
	FP_ICON_PLAY       = 0x09000001,
	FP_ICON_COL1       = 0x09000002,
	FP_ICON_PAUSE      = 0x0A000001,
	FP_ICON_CAM1       = 0x0B000001,
	FP_ICON_COL2       = 0x0B000002,
	FP_ICON_CAM2       = 0x0C000001,
	FP_ICON_CLOCK,
	FP_ICON_FR,
	FP_ICON_FF,
	FP_ICON_DD,
	FP_ICON_SCRAMBLED,
	FP_ICON_LOCK
} fp_icon;

typedef enum
{
	SPARK_PLAY_FASTBACKWARD = 1,
	SPARK_PLAY = 3,
	SPARK_PLAY_FASTFORWARD = 5,
	SPARK_PAUSE = 6,
	SPARK_REC1 = 7,
	SPARK_MUTE = 8,
	SPARK_CYCLE = 9,
	SPARK_DD = 10,
	SPARK_CA = 11,
	SPARK_CI= 12,
	SPARK_USB = 13,
	SPARK_DOUBLESCREEN = 14,
	SPARK_HDD_A8 = 16,
	SPARK_HDD_A7 = 17,
	SPARK_HDD_A6 = 18,
	SPARK_HDD_A5 = 19,
	SPARK_HDD_A4 = 20,
	SPARK_HDD_A3 = 21,
	SPARK_HDD_FULL = 22,
	SPARK_HDD_A2 = 23,
	SPARK_HDD_A1 = 24,
	SPARK_MP3 = 25,
	SPARK_AC3 = 26,
	SPARK_TVMODE_LOG = 27,
	SPARK_AUDIO = 28,
	SPARK_HDD = 30,
	SPARK_CLOCK = 33,
	SPARK_TER = 37,
	SPARK_SAT = 42,
	SPARK_TIMESHIFT = 43,
	SPARK_CAB = 45,
	SPARK_ALL = 46
} spark_icon;

#include <pthread.h>
#include <string>

class CLCD
{
	public:

		enum MODES
		{
			MODE_TVRADIO,
			MODE_SCART,
			MODE_SHUTDOWN,
			MODE_STANDBY,
			MODE_MENU_UTF8,
			MODE_AUDIO,
			MODE_MOVIE
		};
		enum AUDIOMODES
		{
			AUDIO_MODE_PLAY,
			AUDIO_MODE_STOP,
			AUDIO_MODE_FF,
			AUDIO_MODE_PAUSE,
			AUDIO_MODE_REV
		};

	private:

		CLCD();
		std::string	menutitle;
		std::string	servicename;
		int servicenumber;
		MODES		mode;
		void setled(int red, int green);
		static void	*TimeThread(void *);
		pthread_t	thrTime;
		bool		thread_running;
		int		last_toggle_state_power;
		int		brightness;
		unsigned int	timeout_cnt;
		unsigned int	switch_name_time_cnt;
		void		setlcdparameter(int dimm, int power);
		void		count_down();

	public:
		bool has_lcd;
		void wake_up();
		void setled(void) { return; };
		void setlcdparameter(void);
		void setBacklight(bool) { return; };

		static CLCD* getInstance();
		void init(const char * fontfile, const char * fontname,
		          const char * fontfile2=NULL, const char * fontname2=NULL,
		          const char * fontfile3=NULL, const char * fontname3=NULL); 

		void setMode(const MODES m, const char * const title = "");
		MODES getMode() { return mode; };
		void setHddUsage(int perc);

		void showServicename(const std::string name, const int num, const bool clear_epg = false);
		std::string getServicename(void) { return servicename; }
		void setEPGTitle(const std::string title);
		void setMovieInfo(const AUDIOMODES playmode, const std::string big, const std::string small, const bool centered = false);
		void setMovieAudio(const bool is_ac3);
		std::string getMenutitle() { return menutitle; };
		void showTime(bool force = false);
		/** blocks for duration seconds */
		void showRCLock(int duration = 2);
		void showVolume(const char vol, const bool perform_update = true);
		void showPercentOver(const unsigned char perc, const bool perform_update = true, const MODES m = MODE_TVRADIO);
		void showMenuText(const int position, const char * text, const int highlight = -1, const bool utf_encoded = false);
		void showAudioTrack(const std::string & artist, const std::string & title, const std::string & album);
		void showAudioPlayMode(AUDIOMODES m=AUDIO_MODE_PLAY);
		void showAudioProgress(const char perc, bool isMuted = false);

		void setBrightness(int);
		int getBrightness();
		void setBrightnessStandby(int);
		int getBrightnessStandby();
		void setBrightnessDeepStandby(int) { return ; };
		int getBrightnessDeepStandby() { return 0; };

		void setScrollMode(int scroll_repeats);

		void setContrast(int);
		int getContrast();

		void setPower(int);
		int getPower();

		void togglePower(void);

		void setInverse(int);
		int getInverse();

		void setAutoDimm(int);
		int getAutoDimm();
		void repaintIcons() { return; };
		void setMuted(bool);

		void resume();
		void pause();

		void Lock();
		void Unlock();
		void Clear();
		void SetIcons(int icon, bool show);
		void UpdateIcons();
		void ShowDiskLevel();
		void Reset() {};
		void ShowIcon(fp_icon icon, bool show);
		void ShowText(const char *str, bool update_timestamp = true);
		~CLCD();
};

#endif // __simple_display__
