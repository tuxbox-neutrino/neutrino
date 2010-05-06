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

#ifndef __bouqueteditor_bouquets__
#define __bouqueteditor_bouquets__

#include <driver/framebuffer.h>
#include <gui/widget/menue.h>

#include <zapit/client/zapitclient.h>
#include <zapit/channel.h>
#include <zapit/bouquets.h>

#include <string>

/* class for handling when bouquets changed.                  */
/* This class should be a temporarily work around             */
/* and should be replaced by standard neutrino event handlers */
/* (libevent) */
class CBouquetEditorEvents
{
public:
	virtual void onBouquetsChanged() {};
};

class CBEBouquetWidget : public CMenuWidget
{

	private:

		CFrameBuffer	*frameBuffer;

		enum
		{
			beDefault,
			beMoving
		} state;

		enum
		{
			beRename,
			beHide,
			beLock
		} blueFunction;

		unsigned int		selected;
		unsigned int		origPosition;
		unsigned int		newPosition;

		unsigned int		liststart;
		unsigned int		listmaxshow;
		int			fheight; // Fonthoehe Bouquetlist-Inhalt
		int			theight; // Fonthoehe Bouquetlist-Titel

		int 			ButtonHeight;
		int			iconoffset;
		int			iheight; // item height
		//std::string				name;
		bool	bouquetsChanged;
		int		width;
		int		height;
		int		x;
		int		y;

		void paintItem(int pos);
		void paint();
		void paintHead();
		void paintFoot();
		void hide();

		void deleteBouquet();
		void addBouquet();
		void beginMoveBouquet();
		void finishMoveBouquet();
		void cancelMoveBouquet();
		void internalMoveBouquet( unsigned int fromPosition, unsigned int toPosition);
		void renameBouquet();
		void switchHideBouquet();
		void switchLockBouquet();

		void saveChanges();
		void discardChanges();

		std::string inputName(const char * const defaultName, const neutrino_locale_t caption);

	public:
		CBEBouquetWidget();

		//CZapitClient::BouquetList Bouquets;
		BouquetList * Bouquets;
		int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
