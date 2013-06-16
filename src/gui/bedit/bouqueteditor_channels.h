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


#ifndef __bouqueteditor_channels__
#define __bouqueteditor_channels__

#include <driver/framebuffer.h>
#include <gui/widget/menue.h>
#include <gui/components/cc_item_infobox.h>
#include <gui/components/cc_detailsline.h>
#include <zapit/client/zapitclient.h>

#include <string>
#include <zapit/channel.h>
#include <zapit/bouquets.h>

class CBEChannelWidget : public CMenuTarget
{

	private:
		CFrameBuffer	*frameBuffer;
		CComponentsDetailLine *dline;
		CComponentsInfoBox *ibox;
		
		enum state_
		{
			beDefault,
			beMoving
		} state;

		unsigned int		selected;
		unsigned int		origPosition;
		unsigned int		newPosition;

		unsigned int		liststart;
		unsigned int		listmaxshow;
		unsigned int		numwidth;
		int			fheight;
		int			theight;
		int			iconoffset;
		int                     iheight; // item height
		int			footerHeight;
		int			info_height;

		std::string		caption;
		bool			channelsChanged;

		CZapitClient::channelsMode mode;

		unsigned int bouquet;

		int		width;
		int		height;
		int		x;
		int		y;

		void paintItem(int pos);
		void paintDetails(int index);
		void initItem2DetailsLine (int pos, int ch_index);
		void clearItem2DetailsLine ();
		void paint();
		void paintHead();
		void paintFoot();
		void hide();
		void updateSelection(unsigned int newpos);

		void deleteChannel();
		void addChannel();
		void beginMoveChannel();
		void finishMoveChannel();
		void cancelMoveChannel();
		void internalMoveChannel( unsigned int fromPosition, unsigned int toPosition);

		std::string getInfoText(int index);
	public:
		CBEChannelWidget( const std::string & Caption, unsigned int Bouquet);
		~CBEChannelWidget();

		//CZapitClient::BouquetChannelList	Channels;
		ZapitChannelList * Channels;
		int exec(CMenuTarget* parent, const std::string & actionKey);
		bool hasChanged();
};

#endif
