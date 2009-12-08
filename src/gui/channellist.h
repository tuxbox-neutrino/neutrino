#ifndef __channellist__
#define __channellist__

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

#include <driver/framebuffer.h>
#include <gui/widget/menue.h>
#include <system/lastchannel.h>

#include <sectionsdclient/sectionsdclient.h>
#include <zapit/client/zapitclient.h>
#include <zapit/channel.h>

#include <string>
#include <vector>

enum {
	LIST_MODE_FAV,
	LIST_MODE_PROV,
	LIST_MODE_SAT,
	LIST_MODE_ALL
};

class CChannelList
{
	private:
		CFrameBuffer		*frameBuffer;
		unsigned int		selected;
		CLastChannel		lastChList;
		unsigned int		liststart;
		unsigned int		listmaxshow;
		unsigned int		numwidth;
		int			fheight; // Fonthoehe Channellist-Inhalt
		int			theight; // Fonthoehe Channellist-Titel
		int			buttonHeight;

		std::string             name;
		//std::vector<CChannel*>	chanlist;
		std::vector<CZapitChannel*>	chanlist;
		CZapProtection* 	zapProtection;

		int 			width;
		int 			height;
		int 			x;
		int 			y;

		bool historyMode;
		bool vlist; // "virtual" list, not bouquet
		bool displayNext;

		void paintDetails(int index);
		void clearItem2DetailsLine ();
		void paintItem2DetailsLine (int pos, int ch_index);
		void paintItem(int pos);
		void paint();
		void paintHead();
		void hide();

	public:
		CChannelList(const char * const Name, bool historyMode = false, bool _vlist = false );
		~CChannelList();
		//void addChannel(int key, int number, const std::string& name, const t_satellite_position satellitePosition, t_channel_id ids = 0); // UTF-8
		void addChannel(CZapitChannel* chan, int num = 0);
		void putChannel(CZapitChannel* chan);
		//void addChannel(CZapitChannel* channel, int key, int number, bool locked = false);
		CZapitChannel* getChannel(int number);
		CZapitChannel* getChannel(t_channel_id channel_id);
		CZapitChannel* getChannelFromIndex( uint32_t index) { if (chanlist.size() > index) return chanlist[index]; else return NULL;};
		CZapitChannel* operator[]( uint32_t index) { if (chanlist.size() > index) return chanlist[index]; else return NULL;};
		int getKey(int);

		const char * const   getName                   (void) const { return name.c_str(); };
		const std::string &  getActiveChannelName      (void) const; // UTF-8
		t_satellite_position getActiveSatellitePosition(void) const;
		int                  getActiveChannelNumber    (void) const;
		t_channel_id         getActiveChannel_ChannelID(void) const;

/*		CChannel *   getChannelFromChannelID(const t_channel_id channel_id); */

		void zapTo(int pos, bool forceStoreToLastChannels = false);
		void virtual_zap_mode(bool up);
		bool zapTo_ChannelID(const t_channel_id channel_id);
		bool adjustToChannelID(const t_channel_id channel_id, bool bToo = true);
		bool showInfo(int pos, int epgpos = 0);
		void updateEvents(void);
		int 	numericZap(int key);
		int  	show();
		int	exec();
		void quickZap(int key, bool cycle = false);
		int  hasChannel(int nChannelNr);
		int  hasChannelID(t_channel_id channel_id);
		void setSelected( int nChannelNr); // for adjusting bouquet's channel list after numzap or quickzap

		int handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data);

		int getSize() const;
		int getSelectedChannelIndex() const;
		void setSize(int newsize);
		unsigned int		tuned;
		int doChannelMenu(void);
		void SortAlpha(void);
		void SortSat(void);
		void ClearList(void);
		//friend class CZapitChannel;
};
#endif
