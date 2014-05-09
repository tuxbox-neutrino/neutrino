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

#ifndef __bouqueteditor_chanselect__
#define __bouqueteditor_chanselect__

#include <driver/framebuffer.h>
#include <gui/widget/listbox.h>
#include <gui/components/cc.h>
#include <zapit/client/zapitclient.h>

#include <string>
#include <zapit/channel.h>
#include <zapit/bouquets.h>

class CBEChannelSelectWidget : public CListBox
{
	private:

		unsigned int	bouquet;
		short int channellist_sort_mode;
		enum{SORT_ALPHA,SORT_FREQ,SORT_SAT,SORT_CH_NUMBER, SORT_END};
		CZapitClient::channelsMode mode;
		bool isChannelInBouquet( int index);
		CComponentsDetailLine *dline;
		CComponentsInfoBox *ibox;

		uint	getItemCount();
		void paintItem(uint32_t itemNr, int paintNr, bool selected);
		void paintDetails(int index);
		void initItem2DetailsLine (int pos, int ch_index);
		void paintFoot();
		void onOkKeyPressed();
		void onRedKeyPressed();
		void hide();


		int	footerHeight;
		int	info_height;
		
		std::string getInfoText(int index);
	public:
		ZapitChannelList Channels;
		ZapitChannelList * bouquetChannels;

		CBEChannelSelectWidget(const std::string & Caption, unsigned int Bouquet, CZapitClient::channelsMode Mode);
		~CBEChannelSelectWidget();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		bool hasChanged();

};

#endif

