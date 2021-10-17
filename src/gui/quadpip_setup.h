/*
	QuadPiP setup implementation - Neutrino-GUI

	Copyright (C) 2021 Mike Bremer (BPanther)
	Homepage: https://forum.mbremer.de

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

#ifndef __quadpip_setup__
#define __quadpip_setup__

#include <gui/widget/menue.h>
#include <string>

class CQuadPiPSetup : public CMenuTarget
{
	private:
		int showQuadPiPSetup();

	public:	
		CQuadPiPSetup();
		~CQuadPiPSetup();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

class CQuadPiPSetupNotifier : public CChangeObserver
{
	public:
		bool changeNotify(const neutrino_locale_t, void * Data);
};

class CQuadPiPSetupSelectChannelWidget : public CMenuWidget
{
	private:
		int InitZapitChannelHelper(CZapitClient::channelsMode mode);

	public:
		CQuadPiPSetupSelectChannelWidget();
		~CQuadPiPSetupSelectChannelWidget();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif
