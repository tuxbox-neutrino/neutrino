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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __controldclient__
#define __controldclient__

#include <connection/basicclient.h>
#include <controldclient/controldMsg.h>


#define VCR_STATUS_OFF 0
#define VCR_STATUS_ON 1
#define VCR_STATUS_16_9 2

class CControldClient:private CBasicClient
{
 private:
	virtual const unsigned char   getVersion   () const;
	virtual const          char * getSocketName() const;

 public:

		enum events
		{
			EVT_VOLUMECHANGED,
			EVT_MUTECHANGED,
			EVT_MODECHANGED,
			EVT_VCRCHANGED
		};

		//VideoFormat
		static const char VIDEOFORMAT_AUTO = 0;
		static const char VIDEOFORMAT_16_9 = 1;
		static const char VIDEOFORMAT_4_3  = 2;
		static const char VIDEOFORMAT_4_3_PS = 3;

		//VideoOutput
		static const char VIDEOOUTPUT_COMPOSITE = 0;
		static const char VIDEOOUTPUT_RGB       = 1;
		static const char VIDEOOUTPUT_SVIDEO    = 2;
		static const char VIDEOOUTPUT_YUV_VBS   = 3;
		static const char VIDEOOUTPUT_YUV_CVBS  = 4;

		//mute
		static const bool VOLUME_MUTE = true;
		static const bool VOLUME_UNMUTE = false;

		//scartmode
		static const char SCARTMODE_ON  = 1;
		static const char SCARTMODE_OFF = 0;

		/*
			setVolume(volume_type) : Setzten der Lautstärke
			Parameter: 0..100 - 0=leise 100=laut
			           volume_type     : device AVS/OST/UNKOWN(=last used)
		*/
		void setVolume(const char volume, const CControld::volume_type volume_type = CControld::TYPE_UNKNOWN);
		char getVolume(const CControld::volume_type volume_type = CControld::TYPE_UNKNOWN);

		/*
			setMute(bool, volume_type) : setzen von Mute
			Parameter: mute == true    : ton aus
						  mute == false   : ton an
			           volume_type     : device AVS/OST/UNKOWN(=last used)
		*/
		void setMute(const bool mute, const CControld::volume_type volume_type = CControld::TYPE_UNKNOWN);
		bool getMute(const CControld::volume_type volume_type = CControld::TYPE_UNKNOWN);

		/*
			Mute(volume_type) : Ton ausschalten
			Parameter: volume_type   : device AVS/OST/UNKOWN(=last used)
		*/
		void Mute(const CControld::volume_type volume_type = CControld::TYPE_UNKNOWN);

		/*
			UnMute(bool) : Ton wieder einschalten
			Parameter: volume_type   : device AVS/OST/UNKOWN(=last used)
			           
		*/
		void UnMute(const CControld::volume_type volume_type = CControld::TYPE_UNKNOWN);


		/*
			setVideoFormat(char) : Setzten des Bildformates ( 4:3 / 16:9 )
			Parameter: VIDEOFORMAT_AUTO = auto
			           VIDEOFORMAT_4_3  = 4:3
			           VIDEOFORMAT_16_9 = 16:9
		*/
		void setVideoFormat(char);
		char getVideoFormat();
		/*
			getAspectRatio : Aktueller Wert aus dem Bitstream
					2: 4:3
					3: 16:9
					4: 2:2.1
		*/
		char getAspectRatio();

		/*
			setVideoOutput(char) : Setzten des Videooutputs ( composite (= cvbs) / svideo / rgb+cvbs / yuv+vbs / yuv+cvbs )
			Parameter: VIDEOOUTPUT_COMPOSITE = cvbs (composite) video
			           VIDEOOUTPUT_SVIDEO    = svideo
			           VIDEOOUTPUT_RGB       = rgb+cvbs
			           VIDEOOUTPUT_YUV_VBS   = yuv+vbs
			           VIDEOOUTPUT_YUV_CVBS  = yuv+cvbs
		*/
		void setVideoOutput(char);
		char getVideoOutput();

                /*
			setVCROutput(char) : Setzten des Videooutputs für VCR ( composite / svideo )
			Parameter: VIDEOOUTPUT_COMPOSITE = cvbs
			VIDEOOUTPUT_SVIDEO    = svideo
		*/
		void setVCROutput(char);
		char getVCROutput();

		/*
			setBoxType(CControldClient::tuxbox_vendor_t) : Setzten des Boxentyps ( nokia / sagem / philips )
		*/
		void setBoxType(const CControld::tuxbox_maker_t);
		CControld::tuxbox_maker_t getBoxType();

		/*
			setScartMode(char) : Scartmode ( an / aus )
			Parameter: SCARTMODE_ON  = auf scartinput schalten
			           SCARTMODE_OFF = wieder dvb anzeigen


		*/
		void setScartMode(bool);
		char getScartMode();


		/*
			die dBox in standby bringen
		*/
		void videoPowerDown(bool);

		/*
			standby abfragen
		*/
		bool getVideoPowerDown();

		/*
			die Dbox herunterfahren
		*/
		void shutdown();


		/*
			ein beliebiges Event anmelden
		*/
		void registerEvent(unsigned int eventID, unsigned int clientID, const char * const udsName);

		/*
			ein beliebiges Event abmelden
		*/
		void unRegisterEvent(unsigned int eventID, unsigned int clientID);

		void saveSettings();

		/*
		  setzen der sync correction im RGB mode des saa7126 (csync) 0=aus, 31=max
		*/
		
		void setRGBCsync(char csync);
		/*
		  lesen der sync correction im RGB mode des saa7126 (csync) 0=aus, 31=max
		*/
		char getRGBCsync();
                void setVideoMode(char);
                char getVideoMode();
};

#endif
