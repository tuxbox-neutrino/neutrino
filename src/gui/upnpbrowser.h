/*
  Neutrino-GUI  -   DBoxII-Project

  Copyright (C) 2001 Steffen Hehn 'McClean'
  Homepage: http://dbox.cyberphoria.org/

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
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __upnpplayergui__
#define __upnpplayergui__

#include <driver/audiofile.h>
#include <gui/filebrowser.h>
#include <gui/widget/menue.h>
#include <gui/widget/listhelpers.h>

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
	std::string	albumArtURI;
	std::string	children;
	std::string	proto;
	std::string	mime;
	std::vector<UPnPResource> resources;
	int		preferred;
	int		type;
};

class CFrameBuffer;
class CUpnpBrowserGui : public CMenuTarget, public CListHelpers
{
	public:
	CUpnpBrowserGui();
	~CUpnpBrowserGui();
	int exec(CMenuTarget* parent, const std::string & actionKey);

	private:
	std::vector<CUPnPDevice> m_devices;
	sigc::connection sigFonts;
	sigc::connection sigPall;
	UPnPEntry      m_playing_entry;
	CUPnPSocket  * m_socket;
	CFrameBuffer * m_frameBuffer;
	int            m_LastMode;
	int            m_width;
	int            m_height;
	int            m_x;
	int            m_y;
	unsigned int   m_listmaxshow;
	unsigned int   m_deviceliststart;
	unsigned int   m_selecteddevice;

	int		font_item;

	int            m_topbox_height;
	int            m_header_height;
	int            m_header_y;
	int            m_item_height;
	int            m_item_y;
	int            m_footer_height;
	int            m_footer_y;
	int            m_infobox_height;
	int            m_infobox_y;

	bool           m_folderplay;
	std::string    m_playfolder;
	int            m_playid;
	time_t         m_time_played;
	bool           m_playing_entry_is_shown;
	time_t         timeout;
	int	       video_key_msg;
	CComponentsDetailsLine * dline;
	CComponentsFooter footer;
	CComponentsInfoBox topbox, infobox, timebox;
	CComponentsPicture *image;

	bool discoverDevices();
	void splitProtocol(std::string &protocol, std::string &prot, std::string &network, std::string &mime, std::string &additional);
	bool getResults(std::string id, unsigned int start, unsigned int count, std::list<UPnPAttribute> &results);
	std::vector<UPnPEntry> *decodeResult(std::string);
	void Init();
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
	void paintItem(std::vector<UPnPEntry> *entry, unsigned int pos, unsigned int selected);
	void paintItemInfo(UPnPEntry *entry);
	void paintDetails(UPnPEntry *entry, bool use_playing = false);
	void paintItem2DetailsLine(int pos);

	void updateTimes(const bool force = false);
	void updateMode();
	void playAudio(std::string name, int type);
	void stopAudio();
	void showPicture(std::string name);
	void playVideo(std::string name, std::string url);
};

#endif
