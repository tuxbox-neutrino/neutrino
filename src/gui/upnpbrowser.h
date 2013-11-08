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

#ifndef __upnpplayergui__
#define __upnpplayergui__

#include <driver/framebuffer.h>
#include <driver/audiofile.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <gui/filebrowser.h>
#include <gui/widget/menue.h>

#include <string>
#include <sstream>
#include <upnpclient.h>

struct UPnPResource
{
	std::string	url;
	std::string	protocol;
	std::string	size;
	std::string	duration;
};

struct UPnPEntry
{
	std::string	id;
	bool		isdir;
	std::string	title;
	std::string	artist;
	std::string	album;
	std::string	children;
	std::vector<UPnPResource> resources;
	int		preferred;
};

class CUpnpBrowserGui : public CMenuTarget
{
	public:
	CUpnpBrowserGui();
	~CUpnpBrowserGui();
	int exec(CMenuTarget* parent, const std::string & actionKey);

	private:
	std::vector<CUPnPDevice> m_devices;
	UPnPEntry      m_playing_entry;
	CUPnPSocket  * m_socket;
	CFrameBuffer * m_frameBuffer;
	bool           m_vol_ost;
	int            m_LastMode;
	int            m_width;
	int            m_height;
	int            m_x;
	int            m_y;
	unsigned int   m_listmaxshow;
	unsigned int   m_deviceliststart;
	unsigned int   m_selecteddevice;
	int            m_fheight; // Fonthoehe Inhalt
	int            m_theight; // Fonthoehe Titel
	int            m_mheight; // Fonthoehe Info
	int            m_sheight; // Fonthoehe Status
	int            m_buttonHeight;
	int            m_title_height;
	int            m_info_height;
	bool           m_folderplay;
	std::string    m_playfolder;
	int            m_playid;
	time_t         m_time_played;
	bool           m_playing_entry_is_shown;
	time_t         timeout;
	CComponentsDetailLine * dline;

	bool discoverDevices();
	void splitProtocol(std::string &protocol, std::string &prot, std::string &network, std::string &mime, std::string &additional);
	bool getResults(std::string id, unsigned int start, unsigned int count, std::list<UPnPAttribute> &results); 
	std::vector<UPnPEntry> *decodeResult(std::string);

	void updateDeviceSelection(int newpos);
	void selectDevice();
	void paintDevices();
	void paintDevice(unsigned int pos);
	void paintDeviceInfo();
	void playnext();

	bool getItems(std::string id, unsigned int index, std::vector<UPnPEntry> * &entries, unsigned int &total);
	bool updateItemSelection(std::string id, std::vector<UPnPEntry> * &entries, int newpos, unsigned int &selected, unsigned int &liststart);
	bool selectItem(std::string);
	void paintItems(std::vector<UPnPEntry> *entry, unsigned int selected, unsigned int max, unsigned int offset);
	void paintItem  (std::vector<UPnPEntry> *entry, unsigned int pos, unsigned int selected);
	void paintItemInfo(std::vector<UPnPEntry> *entry, unsigned int selected);
	void paintDetails(std::vector<UPnPEntry> *entry, unsigned int index, bool use_playing = false);
	void paintItem2DetailsLine (int pos);

	void updateTimes(const bool force = false);
	void playAudio(std::string name, std::string mime);
	void showPicture(std::string name);
	void playVideo(std::string name, std::string url);
};

#endif
