/*
	neutrino bouquet editor - channel selection

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2017 Sven Hoefer

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __bouqueteditor_chanselect__
#define __bouqueteditor_chanselect__

#include <string>

#include <gui/components/cc.h>
#include <gui/widget/listhelpers.h>
#include <gui/widget/menue.h>
#include <zapit/bouquets.h>
#include <zapit/channel.h>
#include <zapit/client/zapitclient.h>

#include "bouqueteditor_globals.h"

class CBEChannelSelectWidget : public CBEGlobals, public CMenuTarget, public CListHelpers
{
	private:
		enum {
			SORT_ALPHA,
			SORT_FREQ,
			SORT_SAT,
			SORT_CH_NUMBER,
			SORT_END
		};

		unsigned int selected;
		unsigned int liststart;

		CZapitClient::channelsMode mode;
		CZapitBouquet * bouquet;
		short int channellist_sort_mode;
		bool isChannelInBouquet(int index);

		bool channelChanged;
		std::string caption;

		void paintHead();
		void paintBody();
		void paintItem(int pos);
		void paintItems();
		void paintFoot();


		void updateSelection(unsigned int newpos);

		void sortChannels();
		void selectChannel();

		std::string getInfoText(int index);

	public:
		CBEChannelSelectWidget(const std::string & Caption, CZapitBouquet* Bouquet, CZapitClient::channelsMode Mode);
		~CBEChannelSelectWidget();

		ZapitChannelList Channels;
		ZapitChannelList * bouquetChannels;
		int exec(CMenuTarget* parent, const std::string & actionKey);
		void hide(){CBEGlobals::hide();}
		bool hasChanged();
};

#endif
