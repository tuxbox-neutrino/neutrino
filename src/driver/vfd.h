/*
	LCD-Daemon  -   DBoxII-Project

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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __vfd__
#define __vfd__

//#define VFD_UPDATE

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef VFD_UPDATE
// TODO Why is USE_FILE_OFFSET64 not defined, if file.h is included here????
#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64 1
#endif
#include "driver/file.h"
#endif // LCD_UPDATE

#include <pthread.h>
#include <string>

#include <cs_frontpanel.h>

class CVFD
{
	public:

		enum MODES
		{
			MODE_TVRADIO,
			MODE_SCART,
			MODE_SHUTDOWN,
			MODE_STANDBY,
			MODE_MENU_UTF8,
			MODE_AUDIO
#ifdef VFD_UPDATE
                ,       MODE_FILEBROWSER,
                        MODE_PROGRESSBAR,
                        MODE_PROGRESSBAR2,
                        MODE_INFOBOX
#endif // LCD_UPDATE

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
#ifdef BOXMODEL_CST_HD2
		fp_display_caps_t		caps;
#endif
		MODES				mode;

		std::string			servicename;
		int				service_number;
		bool				support_text;
		bool				support_numbers;
		char				volume;
		unsigned char			percentOver;
		bool				muted;
		bool				showclock;
		pthread_t			thrTime;
		int                             last_toggle_state_power;
		bool				clearClock;
		unsigned int                    timeout_cnt;
		unsigned int                    switch_name_time_cnt;
		int fd;
		int brightness;
		std::string text;

		void count_down();

		CVFD();

		static void* TimeThread(void*);
		void setlcdparameter(int dimm, int power);
		void setled(int led1, int led2);
	public:

		~CVFD();
		bool has_lcd;
		bool has_led_segment;
		void setlcdparameter(void);
		void setled(void);
		void setled(bool on_off);
		void setBacklight(bool on_off);
		static CVFD* getInstance();
		void init(const char * fontfile, const char * fontname);

		void setMode(const MODES m, const char * const title = "");

		void showServicename(const std::string & name, int number = -1); // UTF-8
		void setEPGTitle(const std::string) { return; }
		void showTime(bool force = false);
		/** blocks for duration seconds */
		void showRCLock(int duration = 2);
		void showVolume(const char vol, const bool perform_update = true);
		void showPercentOver(const unsigned char perc, const bool perform_update = true, const MODES origin = MODE_TVRADIO);
		void showMenuText(const int position, const char * text, const int highlight = -1, const bool utf_encoded = false);
		void showAudioTrack(const std::string & artist, const std::string & title, const std::string & album);
		void showAudioPlayMode(AUDIOMODES m=AUDIO_MODE_PLAY);
		void showAudioProgress(const unsigned char perc);
		void setBrightness(int);
		int getBrightness();

		void setBrightnessStandby(int);
		int getBrightnessStandby();
		void setBrightnessDeepStandby(int);
		int getBrightnessDeepStandby();

		void setScrollMode(int) { return; }

		void setPower(int);
		int getPower();

		void togglePower(void);

		void setInverse(int);
		int getInverse();

		void setMuted(bool);

		void resume();
		void pause();
		void Lock();
		void Unlock();
		void Clear();
		void UpdateIcons() { return; }
		void ShowIcon(fp_icon icon, bool show);
		void ShowText(const char *str);
		void ShowNumber(int number);
		void wake_up();
		MODES getMode(void) { return mode; };
		std::string getServicename(void) { return servicename; }
#ifdef LCD_UPDATE
        private:
                CFileList* m_fileList;
                int m_fileListPos;
                std::string m_fileListHeader;

                std::string m_infoBoxText;
                std::string m_infoBoxTitle;
                int m_infoBoxTimer;   // for later use
                bool m_infoBoxAutoNewline;

                bool m_progressShowEscape;
                std::string  m_progressHeaderGlobal;
                std::string  m_progressHeaderLocal;
                int m_progressGlobal;
                int m_progressLocal;
        public:

                void showFilelist(int flist_pos = -1,CFileList* flist = NULL,const char * const mainDir=NULL);
                void showInfoBox(const char * const title = NULL,const char * const text = NULL,int autoNewline = -1,int timer = -1);
                void showProgressBar(int global = -1,const char * const text = NULL,int show_escape = -1,int timer = -1);
                void showProgressBar2(int local = -1,const char * const text_local = NULL,int global = -1,const char * const text_global = NULL,int show_escape = -1);
#endif // LCD_UPDATE

};


#endif
