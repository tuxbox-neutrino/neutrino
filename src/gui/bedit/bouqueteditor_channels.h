/*
	neutrino bouquet editor - channels editor

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


#ifndef __bouqueteditor_channels__
#define __bouqueteditor_channels__

#include <string>

#include <gui/components/cc.h>
#include <gui/widget/listhelpers.h>
#include <gui/widget/menue.h>
#include <zapit/bouquets.h>
#include <zapit/channel.h>
#include <zapit/client/zapitclient.h>

#include "bouqueteditor_globals.h"

class CBEChannelWidget : public CBEGlobals, public CMenuTarget, public CListHelpers
{
	private:
		enum state_
		{
			beDefault,
			beMoving
		} state;

		unsigned int selected;
		unsigned int origPosition;
		unsigned int newPosition;
		unsigned int liststart;

		bool channelsChanged;
		std::string caption;

		CZapitClient::channelsMode mode;

		unsigned int bouquet;

		void paintHead();

		void paintItem(int pos);
		void paintItems();
		void paintFoot();


		void updateSelection(unsigned int newpos);

		void deleteChannel();
		void addChannel();
		void switchLockChannel();
		void renameChannel();
		void beginMoveChannel();
		void finishMoveChannel();
		void cancelMoveChannel();
		void moveChannelToBouquet();
		void internalMoveChannel(unsigned int fromPosition, unsigned int toPosition);

		std::string getInfoText(int index);
		std::string inputName(const char* const defaultName, const neutrino_locale_t caption);

	public:
		CBEChannelWidget( const std::string & Caption, unsigned int Bouquet);
		~CBEChannelWidget();

		//CZapitClient::BouquetChannelList	Channels;
		ZapitChannelList * Channels;
		int exec(CMenuTarget* parent, const std::string & actionKey);
		void hide(){CBEGlobals::hide();}
		bool hasChanged();
		unsigned int getBouquet() { return bouquet; };
};

#endif
