/*
	neutrino bouquet editor - bouquets editor

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

#ifndef __bouqueteditor_bouquets__
#define __bouqueteditor_bouquets__

#include <string>

#include <gui/components/cc.h>
#include <gui/widget/listhelpers.h>
#include <gui/widget/menue.h>
#include <zapit/bouquets.h>
#include <zapit/channel.h>
#include <zapit/client/zapitclient.h>

#include "bouqueteditor_globals.h"

/* class for handling when bouquets changed.                  */
/* This class should be a temporarily work around             */
/* and should be replaced by standard neutrino event handlers */
/* (libevent) */
class CBouquetEditorEvents
{
public:
	virtual void onBouquetsChanged() {};
};

class CBEBouquetWidget : public CBEGlobals, public CMenuTarget, public CListHelpers
{
	private:
		enum
		{
			beDefault,
			beMoving
		} state;

		unsigned int selected;
		unsigned int origPosition;
		unsigned int newPosition;

		unsigned int liststart;

		int iconoffset;

		bool bouquetsChanged;

		void paintHead();
		void paintBody();
		void paintItem(int pos);
		void paintItems();
		void paintFoot();
		void hide();
		void updateSelection(unsigned int newpos);

		void deleteBouquet();
		void addBouquet();
		void beginMoveBouquet();
		void finishMoveBouquet();
		void cancelMoveBouquet();
		void internalMoveBouquet(unsigned int fromPosition, unsigned int toPosition);
		void renameBouquet();
		void switchHideBouquet();
		void switchLockBouquet();

		void saveChanges();
		void discardChanges();

		std::string inputName(const char* const defaultName, const neutrino_locale_t caption);

	public:
		CBEBouquetWidget();

		//CZapitClient::BouquetList Bouquets;
		BouquetList * Bouquets;
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
