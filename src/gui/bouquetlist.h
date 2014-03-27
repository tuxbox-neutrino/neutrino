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


#ifndef __bouquetlist__
#define __bouquetlist__

#include <gui/channellist.h>

#include <driver/framebuffer.h>
#include <system/lastchannel.h>
#include <zapit/bouquets.h>

#include <string>
#include <vector>


typedef enum bouquetSwitchMode
{
    bsmBouquets,	// pressing OK shows list of all Bouquets
    bsmChannels,	// pressing OK shows list of all channels of active bouquets
    bsmAllChannels	// OK shows lsit of all channels
} BouquetSwitchMode;

class CBouquet
{

	public:
		int				unique_key;
		bool			bLocked;
		CChannelList*	channelList;
		CZapitBouquet * zapitBouquet;
		t_satellite_position satellitePosition;

		CBouquet(const int Unique_key, const char * const Name, const bool locked, bool vlist = false)
		{
			zapitBouquet = NULL;
			unique_key = Unique_key;
			bLocked = locked;
			satellitePosition = INVALID_SAT_POSITION;
			channelList = new CChannelList(Name, false, vlist);
		}

		~CBouquet()
		{
			delete channelList;
		}
};


class CBouquetList
{
	private:
		CFrameBuffer		*frameBuffer;

		std::string		name;
		unsigned int		selected;
		unsigned int		liststart;
		unsigned int		listmaxshow;
		unsigned int		numwidth;
		unsigned int		maxpos;
		int			fheight; // Fonthoehe Bouquetlist-Inhalt
		int			theight; // Fonthoehe Bouquetlist-Titel
		int			footerHeight;

		int		width;
		int		height;
		int		x;
		int		y;

		bool		favonly;
		bool		save_bouquets;

		void paintItem(int pos);
		void paint();
		void paintHead();
		void hide();
		int doMenu();
		void updateSelection(int newpos);

	public:
		CBouquetList(const char * const Name = NULL);
		~CBouquetList();

		std::vector<CBouquet*>	Bouquets;

		CBouquet* addBouquet(const char * const name, int BouquetKey=-1, bool locked=false );
		CBouquet* addBouquet(CZapitBouquet * zapitBouquet);
		void deleteBouquet(CBouquet* bouquet);
		t_bouquet_id getActiveBouquetNumber();
		int activateBouquet(int id, bool bShowChannelList);
		int show(bool bShowChannelList = true);
		int showChannelList(int nBouquet = -1);
		//void adjustToChannel(int nChannelNr);
		bool adjustToChannelID(t_channel_id channel_id);
		int exec( bool bShowChannelList);
		bool hasChannelID(t_channel_id channel_id);
};


#endif
