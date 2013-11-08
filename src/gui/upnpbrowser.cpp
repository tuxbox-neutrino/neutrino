/*
  Neutrino-GUI  -   DBoxII-Project

  UPnP Browser (c) 2007 by Jochen Friedrich

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sstream>
#include <stdexcept>

#include <global.h>
#include <neutrino.h>
#include <xmltree.h>
#include <upnpclient.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/audioplay.h>
#include <driver/audiofile.h>
#include <driver/audiometadata.h>
#include <driver/screen_max.h>

#include <gui/audiomute.h>
#include <gui/color.h>
#include <gui/movieplayer.h>

#include <gui/components/cc.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/hintbox.h>
#include <system/settings.h>

#include <gui/upnpbrowser.h>
#include <zapit/zapit.h>
#include <video.h>

extern cVideo * videoDecoder;
extern CPictureViewer * g_PicViewer;

const struct button_label RescanButton = {NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_UPNPBROWSER_RESCAN};
const struct button_label BrowseButtons[4] =
{
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_AUDIOPLAYER_STOP },
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_FILEBROWSER_NEXTPAGE },
	{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_FILEBROWSER_PREVPAGE },
	{ NEUTRINO_ICON_BUTTON_OKAY  , LOCALE_AUDIOPLAYER_PLAY }
};

CUpnpBrowserGui::CUpnpBrowserGui()
{
	m_socket = new CUPnPSocket();
	m_frameBuffer = CFrameBuffer::getInstance();
	m_playing_entry_is_shown = false;
	dline = NULL;
}

CUpnpBrowserGui::~CUpnpBrowserGui()
{
	delete m_socket;
	delete dline;
}

int CUpnpBrowserGui::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	CAudioPlayer::getInstance()->init();

	if (parent)
		parent->hide();

	g_Zapit->stopPlayBack();
	//g_Zapit->lockPlayBack();
	CZapit::getInstance()->EnablePlayback(false);

	videoDecoder->ShowPicture(DATADIR "/neutrino/icons/mp3.jpg");

	// tell neutrino we're in audio mode
	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE , NeutrinoMessages::mode_audio);

	// remember last mode
	m_LastMode=(CNeutrinoApp::getInstance()->getLastMode());

	m_width = m_frameBuffer->getScreenWidthRel();
	m_height = m_frameBuffer->getScreenHeightRel();

	m_sheight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();
	m_buttonHeight = std::min(25, m_sheight);
	m_theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	m_mheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	m_fheight = g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->getHeight();
	m_title_height = m_mheight*2 + 20 + m_sheight + 4;
	m_info_height = m_mheight*2;
	m_listmaxshow = (m_height - m_info_height - m_title_height - m_theight - 2*m_buttonHeight) / (m_fheight);
	m_height = m_theight + m_info_height + m_title_height + 2*m_buttonHeight + m_listmaxshow * m_fheight; // recalc height

	m_x=getScreenStartX(m_width);
	if (m_x < ConnectLineBox_Width)
		m_x = ConnectLineBox_Width;
	m_y=getScreenStartY(m_height);

	// Stop sectionsd
	g_Sectionsd->setPauseScanning(true);

	m_deviceliststart=0;
	m_selecteddevice=0;
	timeout = 0;

	selectDevice();

	if (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP)
		CAudioPlayer::getInstance()->stop();

	//g_Zapit->setStandby(false);

	// Start Sectionsd
	g_Sectionsd->setPauseScanning(false);
	videoDecoder->StopPicture();
	m_frameBuffer->Clear();

	//g_Zapit->unlockPlayBack();
	CZapit::getInstance()->EnablePlayback(true);
	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE , m_LastMode);
	g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR, 0);

	return menu_return::RETURN_REPAINT;
}

void CUpnpBrowserGui::splitProtocol(std::string &protocol, std::string &prot, std::string &network, std::string &mime, std::string &additional)
{
	std::string::size_type pos;
	std::string::size_type startpos = 0;

	pos = protocol.find(":", startpos);
	if (pos != std::string::npos)
	{
		prot = protocol.substr(startpos, pos-startpos);
		startpos = pos + 1;

		pos = protocol.find(":", startpos);
		if (pos != std::string::npos)
		{
			network = protocol.substr(startpos, pos-startpos);
			startpos = pos + 1;

			pos = protocol.find(":", startpos);
			if (pos != std::string::npos)
			{
				mime = protocol.substr(startpos, pos-startpos);
				startpos = pos + 1;

				pos = protocol.find(":", startpos);
				if (pos != std::string::npos)
				{
					additional = protocol.substr(startpos, pos-startpos);
				}
			}
		}
	}
//printf("%s -> %s - %s - %s - %s\n", protocol.c_str(), prot.c_str(), network.c_str(), mime.c_str(), additional.c_str());
}

bool CUpnpBrowserGui::discoverDevices()
{
	if (!m_devices.empty())
		return true;

	CHintBox *scanBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_UPNPBROWSER_SCANNING)); // UTF-8
	scanBox->paint();

	try {
		m_devices = m_socket->Discover("urn:schemas-upnp-org:service:ContentDirectory:1");
	}
	catch (std::runtime_error error)
	{
		delete scanBox;
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, error.what(), CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		return false;
	}
	delete scanBox;
	if (m_devices.empty())
	{
		ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, LOCALE_UPNPBROWSER_NOSERVERS, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		return false;
	}
	return true;
}

bool CUpnpBrowserGui::getResults(std::string id, unsigned int start, unsigned int count, std::list<UPnPAttribute> &results)
{
	std::list<UPnPAttribute>attribs;
	std::stringstream sindex;
	std::stringstream scount;

	sindex << start;
	scount << count;

	attribs.push_back(UPnPAttribute("ObjectID", id));
	attribs.push_back(UPnPAttribute("BrowseFlag", "BrowseDirectChildren"));
	attribs.push_back(UPnPAttribute("Filter", "*"));
	attribs.push_back(UPnPAttribute("StartingIndex", sindex.str()));
	attribs.push_back(UPnPAttribute("RequestedCount", scount.str()));
	attribs.push_back(UPnPAttribute("SortCriteria", ""));

	try
	{
		results=m_devices[m_selecteddevice].SendSOAP("urn:schemas-upnp-org:service:ContentDirectory:1", "Browse", attribs);
	}
	catch (std::runtime_error error)
	{
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, error.what(), CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		return false;
	}
	return true;
}

std::vector<UPnPEntry> *CUpnpBrowserGui::decodeResult(std::string result)
{
	XMLTreeParser *parser;
	XMLTreeNode   *root, *node, *snode;
	std::vector<UPnPEntry> *entries;

	parser = new XMLTreeParser("UTF-8");
	parser->Parse(result.c_str(), result.size(), 1);
	root=parser->RootNode();
	if (!root) {
		delete parser;
		return NULL;
	}
	entries = new std::vector<UPnPEntry>;

	for (node=root->GetChild(); node; node=node->GetNext())
	{
		bool isdir;
		std::string title, artist = "", album = "", id, children;
		const char *type, *p;

		if (!strcmp(node->GetType(), "container"))
		{
			std::vector<UPnPResource> resources;
			isdir=true;
			for (snode=node->GetChild(); snode; snode=snode->GetNext())
			{
				type=snode->GetType();
				p = strchr(type,':');
				if (p)
					type=p+1;
				if (!strcmp(type,"title"))
				{
					p=snode->GetData();
					if (!p)
						p = "";
					title=std::string(p);
				}
			}
			p = node->GetAttributeValue("id");
			if (!p)
				p = "";
			id=std::string(p);

			p = node->GetAttributeValue("childCount");
			if (!p)
				p = "";
			children=std::string(p);

			UPnPEntry entry={id, isdir, title, artist, album, children, resources, -1};
			entries->push_back(entry);
		}
		if (!strcmp(node->GetType(), "item"))
		{
			std::vector<UPnPResource> resources;
			int preferred = -1;
			std::string protocol, prot, network, mime, additional;
			isdir=false;
			for (snode=node->GetChild(); snode; snode=snode->GetNext())
			{
				std::string duration, url, size;
				unsigned int i;
				type=snode->GetType();
				p = strchr(type,':');
				if (p)
					type=p+1;

				if (!strcmp(type,"title"))
				{
					p=snode->GetData();
					if (!p)
						p = "";
					title=std::string(p);
				}
				else if (!strcmp(type,"artist"))
				{
					p=snode->GetData();
					if (!p)
						p = "";
					artist=std::string(p);
				}
				else if (!strcmp(type,"album"))
				{
					p=snode->GetData();
					if (!p)
						p = "";
					album=std::string(p);
				}
				else if (!strcmp(type,"res"))
				{
					p = snode->GetData();
					if (!p)
						p = "";
					url=std::string(p);
					p = snode->GetAttributeValue("size");
					if (!p)
						p = "0";
					size=std::string(p);
					p = snode->GetAttributeValue("duration");
					if (!p)
						p = "";
					duration=std::string(p);
					p = snode->GetAttributeValue("protocolInfo");
					if (!p)
						p = "";
					protocol=std::string(p);
					UPnPResource resource = {url, protocol, size, duration};
					resources.push_back(resource);
				}
				int pref=0;
				preferred=-1;
				for (i=0; i<resources.size(); i++)
				{
					protocol=resources[i].protocol;
					splitProtocol(protocol, prot, network, mime, additional);
					if (prot != "http-get")
						continue;

					if (mime.substr(0,6) == "image/" && pref < 1)
					{
						preferred=i;
					}
					if (mime == "image/jpeg" && pref < 1)
					{
						preferred=i;
						pref=1;
					}
					if (mime == "image/gif" && pref < 2)
					{
						preferred=i;
						pref=2;
					}
					if (mime == "audio/mpeg" && pref < 3)
					{
						preferred=i;
						pref=3;
					}
					if (mime == "audio/x-vorbis+ogg" && pref < 4)
					{
						preferred=i;
						pref=4;
					}
					if (mime == "audio/x-flac" && pref < 5)
					{
						preferred=i;
						pref=5;
					}
					if (mime.substr(0,6) == "video/" && pref < 6)
					{
						preferred=i;
						pref=6;
					}
					if (mime == "video/x-flv" && pref < 7)
					{
						preferred=i;
						pref=7;
					}
					if (mime == "video/mp4" && pref < 8)
					{
						preferred=i;
						pref=8;
					}
				}
			}
			p = node->GetAttributeValue("id");
			if (!p)
				p = "";
			id=std::string(p);

			p = node->GetAttributeValue("childCount");
			if (!p)
				p = "";
			children=std::string(p);

			UPnPEntry entry={id, isdir, title, artist, album, children, resources, preferred};

			entries->push_back(entry);
		}
	}
	delete parser;
	return entries;
}

void CUpnpBrowserGui::updateDeviceSelection(int newpos)
{
	if((int) m_selecteddevice != newpos) {
		int prev_selected = m_selecteddevice;
		unsigned int oldliststart = m_deviceliststart;

		m_selecteddevice = newpos;
		m_deviceliststart = (m_selecteddevice/m_listmaxshow)*m_listmaxshow;
		if (oldliststart != m_deviceliststart)
			paintDevices();
		else {
			paintDevice(prev_selected - m_deviceliststart);
			paintDevice(m_selecteddevice - m_deviceliststart);
		}
	}
}

void CUpnpBrowserGui::selectDevice()
{
	bool loop = true;
	bool refresh = true;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	if (!discoverDevices())
		return;

	CAudioMute::getInstance()->enableMuteIcon(false);

	while (loop)
	{
		if (refresh)
		{
			paintDevices();
			refresh=false;
		}

		g_RCInput->getMsg(&msg, &data, 10); // 1 sec timeout to update play/stop state display
		neutrino_msg_t msg_repeatok = msg & ~CRCInput::RC_Repeat;

		if (msg == CRCInput::RC_timeout)
		{
			// nothing
		}
		else if (msg == CRCInput::RC_home)
		{
			loop=false;
		}
		else if (msg_repeatok == (neutrino_msg_t) g_settings.key_list_start) {
			updateDeviceSelection(0);
		}
		else if (msg_repeatok == (neutrino_msg_t) g_settings.key_list_end) {
			updateDeviceSelection(m_devices.size()-1);
		}
		else if (msg_repeatok == CRCInput::RC_up || (int) msg == g_settings.key_channelList_pageup)
		{
			int step = ((int) msg == g_settings.key_channelList_pageup) ? m_listmaxshow : 1;  // browse or step 1
			int new_selected = m_selecteddevice - step;
			if (new_selected < 0) {
				if (m_selecteddevice != 0 && step != 1)
					new_selected = 0;
				else
					new_selected = m_devices.size() - 1;
			}
			updateDeviceSelection(new_selected);
		}
		else if (msg_repeatok == CRCInput::RC_down || (int) msg == g_settings.key_channelList_pagedown)
		{
			int step =  ((int) msg == g_settings.key_channelList_pagedown) ? m_listmaxshow : 1;  // browse or step 1
			int new_selected = m_selecteddevice + step;
			if (new_selected >= (int) m_devices.size()) {
				if ((m_devices.size() - m_listmaxshow -1 < m_selecteddevice) && (m_selecteddevice != (m_devices.size() - 1)) && (step != 1))
					new_selected = m_devices.size() - 1;
				else if (((m_devices.size() / m_listmaxshow) + 1) * m_listmaxshow == m_devices.size() + m_listmaxshow) // last page has full entries
					new_selected = 0;
				else
					new_selected = ((step == (int) m_listmaxshow) && (new_selected < (int) (((m_devices.size() / m_listmaxshow)+1) * m_listmaxshow))) ? (m_devices.size() - 1) : 0;
			}
			updateDeviceSelection(new_selected);
		}
		else if (msg == CRCInput::RC_right || msg == CRCInput::RC_ok)
		{
			m_folderplay = false;
			selectItem("0");
			refresh=true;
		}
		else if (msg == CRCInput::RC_blue)
		{
			m_devices.clear();
			if (!discoverDevices())
				return;
			refresh=true;
		}
		else if (msg == NeutrinoMessages::RECORD_START ||
				msg == NeutrinoMessages::ZAPTO ||
				msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::SHUTDOWN ||
				msg == NeutrinoMessages::SLEEPTIMER)
		{
			loop=false;
			g_RCInput->postMsg(msg, data);
		}
#if 0
		else if (msg == NeutrinoMessages::EVT_TIMER)
		{
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
		else if (msg > CRCInput::RC_MaxRC)
#endif
		else
		{
printf("msg: %x\n", (int) msg);
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
				loop = false;
		}
	}
	CAudioMute::getInstance()->enableMuteIcon(true);
}

void CUpnpBrowserGui::playnext(void)
{
	std::vector<UPnPEntry> *entries = NULL;
	while (true)
	{
		timeout = 0;

		std::list<UPnPAttribute>results;
		std::list<UPnPAttribute>::iterator i;

		printf("playnext: getResults m_playfolder %s m_playid %d\n", m_playfolder.c_str(), m_playid);
		if (!getResults(m_playfolder, m_playid, 1, results)) {
			m_folderplay = false;
			return;
		}
		for (i=results.begin(); i!=results.end(); ++i)
		{
			if (i->first=="NumberReturned")
			{
				if (atoi(i->second.c_str()) != 1)
				{
					m_folderplay = false;
					return;
				}
			}
			else if (i->first=="Result")
			{
				entries=decodeResult(i->second);
			}
		}
		//m_playid++;
		if ((entries != NULL) && (!(*entries)[0].isdir))
		{
			int preferred=(*entries)[0].preferred;
			if (preferred != -1)
			{
				std::string protocol, prot, network, mime, additional;
				protocol=(*entries)[0].resources[preferred].protocol;
				splitProtocol(protocol, prot, network, mime, additional);
				if (mime == "audio/mpeg" || mime == "audio/x-vorbis+ogg" || mime == "audio/x-flac") {
					m_playing_entry = (*entries)[0];
					m_playing_entry_is_shown = false;
					playAudio((*entries)[0].resources[preferred].url, mime);
				}
				else if (mime.substr(0,6) == "video/") {
					playVideo((*entries)[0].title, (*entries)[0].resources[preferred].url);
					m_folderplay = false; // FIXME else no way to stop in video folder
				}
				else if (mime.substr(0,6) == "image/") {
					if (m_folderplay)
						timeout = time(NULL) + atoi(g_settings.picviewer_slide_time);
					showPicture((*entries)[0].resources[preferred].url);
				}
				return;
			}
		} else {
			neutrino_msg_t      msg;
			neutrino_msg_data_t data;
			g_RCInput->getMsg(&msg, &data, 10); // 1 sec timeout to update play/stop state display

			if (msg == CRCInput::RC_home)
			{
				m_folderplay = false;
				break;
			}
		}
	}
	delete entries;
	m_frameBuffer->Clear();
}

bool CUpnpBrowserGui::getItems(std::string id, unsigned int index, std::vector<UPnPEntry> * &entries, unsigned int &total)
{
	bool tfound = false;
	bool rfound = false;
	bool nfound = false;
	unsigned int returned = 0;
	std::list<UPnPAttribute>results;
	std::list<UPnPAttribute>::iterator i;

	delete entries;
	entries = NULL;

	printf("getItems: getResults: index %d count %d\n", index, m_listmaxshow);
	if (!getResults(id, index, m_listmaxshow, results))
		return false;

	for (i=results.begin(); i!=results.end(); ++i) {
		if (i->first=="NumberReturned") {
			returned=atoi(i->second.c_str());
			nfound=true;
		} else if (i->first=="TotalMatches") {
			total=atoi(i->second.c_str());
			tfound=true;
		} else if (i->first=="Result") {
			entries=decodeResult(i->second);
			rfound=true;
		}
	}
	if (!entries || !nfound || !tfound || !rfound || returned != entries->size() || returned == 0)
		return false;

	return true;
}

bool CUpnpBrowserGui::updateItemSelection(std::string id, std::vector<UPnPEntry> * &entries, int newpos, unsigned int &selected, unsigned int &liststart)
{
	if((int) selected != newpos) {
		int prev_selected = selected;
		unsigned int oldliststart = liststart;

		selected = newpos;
		liststart = (selected/m_listmaxshow)*m_listmaxshow;
		printf("updateItemSelection: list start old %d new %d selected old %d new %d\n", oldliststart, liststart, prev_selected, selected);
		if (oldliststart != liststart) {
			unsigned int total;
			if (!getItems(id, liststart, entries, total))
				return false;
			paintItems(entries, selected - liststart, total - liststart, liststart);
		} else {
			paintItem(entries, prev_selected - liststart, selected - liststart);
			paintItem(entries, selected - liststart, selected - liststart);
		}
	}
	return true;
}

bool CUpnpBrowserGui::selectItem(std::string id)
{
	bool loop = true;
	bool endall = false;
	bool refresh = true;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	std::vector<UPnPEntry> *entries = NULL;

	unsigned int liststart = 0;
	unsigned int selected = 0;
	unsigned int total = 0;

	printf("selectItem: [%s]\n", id.c_str());
	if (!getItems(id, liststart, entries, total))
		return endall;

	while (loop) {
		updateTimes();

		if (refresh) {
			printf("selectItem: refresh, timeout = %d\n", (int) timeout);
			if (!timeout)
				paintItems(entries, selected - liststart, total - liststart, liststart);
			refresh=false;
		}

		g_RCInput->getMsg(&msg, &data, 10); // 1 sec timeout to update play/stop state display
		neutrino_msg_t msg_repeatok = msg & ~CRCInput::RC_Repeat;

		if (msg == CRCInput::RC_timeout) {
			// nothing
		}
		else if (msg == CRCInput::RC_home) {
			loop=false;
			endall=true;
		}
		else if (!timeout && (msg == CRCInput::RC_left)) {
			loop=false;
		}
		else if (!timeout && (msg_repeatok == (neutrino_msg_t) g_settings.key_list_start)) {
			updateItemSelection(id, entries, 0, selected, liststart);
		}
		else if (!timeout && (msg_repeatok == (neutrino_msg_t) g_settings.key_list_end)) {
			updateItemSelection(id, entries, total-1, selected, liststart);
		}
		else if (!timeout && (msg_repeatok == CRCInput::RC_up || (int) msg == g_settings.key_channelList_pageup)) {
			int step = ((int) msg == g_settings.key_channelList_pageup) ? m_listmaxshow : 1;  // browse or step 1
			int new_selected = selected - step;
			if (new_selected < 0) {
				if (selected != 0 && step != 1)
					new_selected = 0;
				else
					new_selected = total - 1;
			}
			updateItemSelection(id, entries, new_selected, selected, liststart);
		}
		else if (!timeout && (msg_repeatok == CRCInput::RC_down || (int) msg == g_settings.key_channelList_pagedown)) {
			int step =  ((int) msg == g_settings.key_channelList_pagedown) ? m_listmaxshow : 1;  // browse or step 1
			int new_selected = selected + step;
			if (new_selected >= (int) total) {
				if ((total - m_listmaxshow -1 < selected) && (selected != (total - 1)) && (step != 1))
					new_selected = total - 1;
				else if (((total / m_listmaxshow) + 1) * m_listmaxshow == total + m_listmaxshow) // last page has full entries
					new_selected = 0;
				else
					new_selected = ((step == (int) m_listmaxshow) && (new_selected < (int) (((total / m_listmaxshow)+1) * m_listmaxshow))) ? (total - 1) : 0;
			}
			updateItemSelection(id, entries, new_selected, selected, liststart);
		}
		else if (!timeout && (msg == CRCInput::RC_ok || msg == CRCInput::RC_right)) {
			if ((*entries)[selected - liststart].isdir) {
				endall=selectItem((*entries)[selected - liststart].id);
				if (endall)
					loop=false;
				refresh=true;
			} else {
				m_folderplay = false;
				int preferred=(*entries)[selected - liststart].preferred;
				if (preferred != -1)
				{
					std::string protocol, prot, network, mime, additional;
					protocol=(*entries)[selected - liststart].resources[preferred].protocol;
					splitProtocol(protocol, prot, network, mime, additional);
					if (mime == "audio/mpeg" || mime == "audio/x-vorbis+ogg" || mime == "audio/x-flac")
					{
						m_playing_entry = (*entries)[selected - liststart];
						m_playing_entry_is_shown = false;
						playAudio((*entries)[selected - liststart].resources[preferred].url, mime);
					}
					else if (mime.substr(0,6) == "video/")
					{
						m_frameBuffer->Clear();
						playVideo((*entries)[selected - liststart].title, (*entries)[selected - liststart].resources[preferred].url);
						videoDecoder->ShowPicture(DATADIR "/neutrino/icons/mp3.jpg");
						refresh = true;
					}
					else if (mime.substr(0,6) == "image/")
					{
						videoDecoder->setBlank(true);
						showPicture((*entries)[selected - liststart].resources[preferred].url);
						m_playid = selected;
						while (true)
						{
							g_RCInput->getMsg(&msg, &data, 10); // 1 sec timeout

							if (msg == CRCInput::RC_home || msg == CRCInput::RC_ok)
								break;
							else if (msg == CRCInput::RC_right || msg == CRCInput::RC_down) {
								m_playfolder = id;
								m_playid = (m_playid + 1)%total;
								playnext();
							}
							else if (msg == CRCInput::RC_left || msg == CRCInput::RC_up) {
								m_playfolder = id;
								m_playid--;
								if (m_playid < 0)
									m_playid = total - 1;
								playnext();
							} else
								CNeutrinoApp::getInstance()->handleMsg(msg, data);
						}
						m_frameBuffer->Clear();
						videoDecoder->setBlank(false);
						refresh = true;
					}
				}
			}
		}
		else if (msg == CRCInput::RC_play) {
			m_folderplay = true;
			m_playfolder = (*entries)[selected - liststart].id;
			m_playid = 0;
			playnext();
			m_playid++;
		}
		else if (msg == CRCInput::RC_yellow) {
			if (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP)
				CAudioPlayer::getInstance()->stop();
			m_folderplay = false;
		}
		else if (m_folderplay && msg == (neutrino_msg_t) CRCInput::RC_stop) {
			timeout = 0;
			m_folderplay = false;
			m_frameBuffer->Clear();
			refresh = true;
		}
		else if (m_folderplay && msg == (neutrino_msg_t) CRCInput::RC_prev) {
			timeout = 0;
			m_playid -= 2;
			if (m_playid < 0)
				m_playid = 0;
		}
		else if (m_folderplay && msg == (neutrino_msg_t) CRCInput::RC_next) {
			timeout = 0;
			if (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP)
				CAudioPlayer::getInstance()->stop();
		}
		else if (msg == NeutrinoMessages::RECORD_START ||
				msg == NeutrinoMessages::ZAPTO ||
				msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::SHUTDOWN ||
				msg == NeutrinoMessages::SLEEPTIMER)
		{
			loop = false;
			g_RCInput->postMsg(msg, data);
		}
#if 0
		else if (msg == NeutrinoMessages::EVT_TIMER)
		{
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
		else if (msg > CRCInput::RC_MaxRC)
#endif
		else
		{
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
				loop = false;
			//refresh=true;
		}

		if (m_folderplay && ((!timeout || (timeout <= time(NULL))) && (CAudioPlayer::getInstance()->getState() == CBaseDec::STOP))) {
			playnext();
			m_playid++;
		}
	}

	delete entries;
	timeout = 0;
	m_frameBuffer->Clear();

	return endall;
}

void CUpnpBrowserGui::paintDeviceInfo()
{
	std::string tmp;
	int w, xstart;

	CVFD::getInstance()->showMenuText(0, m_devices[m_selecteddevice].friendlyname.c_str(), -1, true);

	// Info
	m_frameBuffer->paintBoxRel(m_x, m_y, m_width, m_title_height - 10, COL_MENUCONTENT_PLUS_6, RADIUS_MID);
	m_frameBuffer->paintBoxRel(m_x + 2, m_y + 2, m_width - 4, m_title_height - 14, COL_MENUCONTENTSELECTED_PLUS_0, RADIUS_MID);

	// first line
	tmp = m_devices[m_selecteddevice].manufacturer + " " +
	      m_devices[m_selecteddevice].manufacturerurl;
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true); // UTF-8
	w = std::min(w, m_width - 20);
	xstart = (m_width - w) / 2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(m_x + xstart, m_y + 4 + 1*m_mheight, m_width - 20,
			tmp, COL_MENUCONTENTSELECTED_TEXT, 0, true); // UTF-8

	// second line
	tmp = m_devices[m_selecteddevice].modelname + " " +
	      m_devices[m_selecteddevice].modelnumber + " " +
	      m_devices[m_selecteddevice].modeldescription;
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true); // UTF-8
	w = std::min(w, m_width - 20);
	xstart = (m_width - w) / 2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(m_x + xstart, m_y + 4 + 2*m_mheight, m_width - 20,
			tmp, COL_MENUCONTENTSELECTED_TEXT, 0, true); // UTF-8
	// third line
	tmp = m_devices[m_selecteddevice].modelurl;
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true); // UTF-8
	w = std::min(w, m_width - 20);
	xstart = (m_width - w) / 2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(m_x + xstart, m_y + 4 + 3*m_mheight, m_width - 20,
			tmp, COL_MENUCONTENTSELECTED_TEXT, 0, true); // UTF-8
}

void CUpnpBrowserGui::paintDevice(unsigned int _pos)
{
	int ypos = m_y + m_title_height + m_theight + _pos*m_fheight;
	fb_pixel_t color;
	fb_pixel_t bgcolor;

	unsigned int pos = m_deviceliststart + _pos;
	if (pos == m_selecteddevice)
	{
		color   = COL_MENUCONTENT_TEXT_PLUS_2;
		bgcolor = COL_MENUCONTENT_PLUS_2;
		paintDeviceInfo();
	}
	else
	{
		color   = COL_MENUCONTENT_TEXT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}
	m_frameBuffer->paintBoxRel(m_x, ypos, m_width - 15, m_fheight, bgcolor);

	if (pos >= m_devices.size())
		return;

	char sNr[20];
	sprintf(sNr, "%2d", pos + 1);
	std::string num = sNr;

	std::string name = m_devices[pos].friendlyname;

	int w = g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->getRenderWidth(name) + 5;
	g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(m_x + 10, ypos + m_fheight, m_width - 30 - w,
			num, color, m_fheight, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(m_x + m_width - 15 - w, ypos + m_fheight,
			w, name, color, m_fheight, true); // UTF-8
}

void CUpnpBrowserGui::paintDevices()
{
	std::string tmp;
	int ypos, top;

	// LCD
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, "Select UPnP Device");

	// Head
	CComponentsHeader header(m_x, m_y + m_title_height, m_width, m_theight, LOCALE_UPNPBROWSER_HEAD, NEUTRINO_ICON_UPNP);
	if (CNeutrinoApp::getInstance()->isMuted())
		header.addButtonIcon(NEUTRINO_ICON_BUTTON_MUTE_SMALL);
	header.paint(CC_SAVE_SCREEN_NO);

	// Items
	for (unsigned int count=0; count<m_listmaxshow; count++)
		paintDevice(count);

	ypos = m_y + m_title_height + m_theight;
	int sb = m_fheight * m_listmaxshow;
	m_frameBuffer->paintBoxRel(m_x + m_width - 15, ypos, 15, sb, COL_MENUCONTENT_PLUS_1);

	int sbc = ((m_devices.size() - 1) / m_listmaxshow) + 1;
	int sbs = ((m_selecteddevice) / m_listmaxshow);

	m_frameBuffer->paintBoxRel(m_x + m_width - 13, ypos + 2 + sbs*(sb-4)/sbc, 11, (sb-4)/sbc, COL_MENUCONTENT_PLUS_3);

	// Foot
	top = m_y + (m_height - m_info_height - 2 * m_buttonHeight);

	m_frameBuffer->paintBoxRel(m_x, top, m_width, m_buttonHeight+2, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_LARGE, CORNER_BOTTOM);
// 	m_frameBuffer->paintHLine(m_x, m_x + m_width, top, COL_INFOBAR_SHADOW_PLUS_0);
	::paintButtons(m_x, top, 0, 1, &RescanButton, m_width, m_buttonHeight);

	paintItem2DetailsLine (-1); // clear it
}

void CUpnpBrowserGui::paintItem(std::vector<UPnPEntry> *entry, unsigned int pos, unsigned int selected)
{
	int ypos = m_y + m_title_height + m_theight + pos*m_fheight;
	fb_pixel_t color;
	fb_pixel_t bgcolor;

	if (pos == selected)
	{
		color   = COL_MENUCONTENT_TEXT_PLUS_2;
		bgcolor = COL_MENUCONTENT_PLUS_2;
		paintItemInfo(entry, selected);
		paintDetails(entry, pos);
		if ((*entry)[pos].isdir)
			paintItem2DetailsLine (-1); // clear it
		else
			paintItem2DetailsLine (pos);
	}
	else
	{
		color   = COL_MENUCONTENT_TEXT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}
	m_frameBuffer->paintBoxRel(m_x, ypos, m_width - 15, m_fheight, bgcolor);

	if (pos >= entry->size())
		return;

	int preferred=(*entry)[pos].preferred;
	std::string info;
	std::string fileicon;
	if ((*entry)[pos].isdir)
	{
		info = "<DIR>";
		fileicon = NEUTRINO_ICON_FOLDER;
	}
	else
	{
		if (preferred != -1)
		{
			info = (*entry)[pos].resources[preferred].duration;
			fileicon = NEUTRINO_ICON_MP3;
		}
		else
		{
			info = "(none)";
			fileicon = NEUTRINO_ICON_FILE;
		}
	}

	std::string name = (*entry)[pos].title;
	char tmp_time[] = "00:00:00.0";
	int w = g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->getRenderWidth(tmp_time);

	m_frameBuffer->paintIcon(fileicon, m_x + 5 , ypos + (m_fheight - 16) / 2);
	g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(m_x + m_width - 15 - w, ypos + m_fheight,
			w, info, color, m_fheight);
	g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(m_x + 30, ypos + m_fheight, m_width - 50 - w,
			name, color, m_fheight, true); // UTF-8
}

void CUpnpBrowserGui::paintItemInfo(std::vector<UPnPEntry> *entry, unsigned int selected)
{
	std::string tmp;
	std::stringstream ts;
	int w, xstart;
	int preferred=(*entry)[selected].preferred;

	// LCD
	CVFD::getInstance()->showMenuText(0, (*entry)[selected].title.c_str(), -1, true);

	// Info
	m_frameBuffer->paintBoxRel(m_x, m_y, m_width, m_title_height - 10, COL_MENUCONTENT_PLUS_6, RADIUS_MID);
	m_frameBuffer->paintBoxRel(m_x + 2, m_y + 2, m_width - 4, m_title_height - 14, COL_MENUCONTENTSELECTED_PLUS_0, RADIUS_MID);

	// first line
	ts << "Resources: " << (*entry)[selected].resources.size() << " Selected: " << preferred+1 << " ";
	tmp = ts.str();

	if (preferred != -1)
		tmp = tmp + "Duration: " + (*entry)[selected].resources[preferred].duration;
	else
		tmp = tmp + "No resource for Item";

	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true); // UTF-8
	w = std::min(w, m_width - 20);
	xstart = (m_width - w) / 2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(m_x + xstart, m_y + 4 + 1*m_mheight, m_width - 20,
			tmp, COL_MENUCONTENTSELECTED_TEXT, 0, true); // UTF-8

	// second line
	if ((*entry)[selected].isdir)
		tmp = "Directory";
	else
	{
		tmp = "";
		if (preferred != -1)
		{
			std::string proto, network, mime, info;
			splitProtocol((*entry)[selected].resources[preferred].protocol, proto, network, mime, info);
			tmp = "Protocol: " + proto + ", MIME-Type: " + mime;
		}
	}
	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true); // UTF-8
	w = std::min(w, m_width - 20);
	xstart = (m_width - w) / 2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(m_x + xstart, m_y + 4 + 2*m_mheight, m_width - 20,
			tmp, COL_MENUCONTENTSELECTED_TEXT, 0, true); // UTF-8

	//third line
	tmp = "";
	if (!(*entry)[selected].isdir && preferred != -1)
		tmp = "URL: " + (*entry)[selected].resources[preferred].url;

	w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true); // UTF-8
	w = std::min(w, m_width - 20);
	xstart = (m_width - w) / 2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(m_x + xstart, m_y + 4 + 3*m_mheight, m_width - 20,
			tmp, COL_MENUCONTENTSELECTED_TEXT, 0, true); // UTF-8
}

void CUpnpBrowserGui::paintItems(std::vector<UPnPEntry> *entry, unsigned int selected, unsigned int max, unsigned int offset)
{
printf("CUpnpBrowserGui::paintItem:s selected %d max %d offset %d\n", selected, max, offset);
	int ypos, top;

	// LCD
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, "Select UPnP Entry");

	// Head
	std::string name = g_Locale->getText(LOCALE_UPNPBROWSER_HEAD);
	name += " : ";
	name += m_devices[m_selecteddevice].friendlyname;
	CComponentsHeader header(m_x, m_y + m_title_height, m_width, m_theight, name, NEUTRINO_ICON_UPNP);
	if (CNeutrinoApp::getInstance()->isMuted())
		header.addButtonIcon(NEUTRINO_ICON_BUTTON_MUTE_SMALL);
	header.paint(CC_SAVE_SCREEN_NO);

	// Items
	for (unsigned int count=0; count<m_listmaxshow; count++)
		paintItem(entry, count, selected);

	ypos = m_y + m_title_height + m_theight;
	int sb = m_fheight * m_listmaxshow;
	m_frameBuffer->paintBoxRel(m_x + m_width - 15, ypos, 15, sb, COL_MENUCONTENT_PLUS_1);

	int sbc = ((max + offset - 1) / m_listmaxshow) + 1;
	int sbs = ((selected + offset) / m_listmaxshow);

	int sbh = 0;
	if ((sbc > 0) && (sbc > sb-4))
		sbh = 2;
	m_frameBuffer->paintBoxRel(m_x + m_width - 13, ypos + 2 + sbs*((sb-4)/sbc+sbh), 11, (sb-4)/sbc + sbh, COL_MENUCONTENT_PLUS_3);

	// Foot buttons
	top = m_y + (m_height - m_info_height - 2 * m_buttonHeight);
	m_frameBuffer->paintBoxRel(m_x, top, m_width, m_buttonHeight+2, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_LARGE, CORNER_BOTTOM);
	::paintButtons(m_x, top, 0, 4, BrowseButtons, m_width, m_buttonHeight);
}

void CUpnpBrowserGui::paintDetails(std::vector<UPnPEntry> *entry, unsigned int index, bool use_playing)
{
	// Foot info
	int top = m_y + (m_height - m_info_height - 1 * m_buttonHeight) + 2;
	int text_start = m_x + 10;

printf("paintDetails: index %d use_playing %d shown %d\n", index, use_playing, m_playing_entry_is_shown);
	if ((!use_playing) && ((*entry)[index].isdir))
	{
		m_frameBuffer->paintBackgroundBoxRel(m_x+2, top + 2, m_width-4, 2 * m_buttonHeight+8);
		m_playing_entry_is_shown = false;
	}
	else
	{
		int ih = g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->getHeight();
		//m_frameBuffer->paintBoxRel(m_x, top + 2, m_width-2, 2 * ih, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE);
		if (use_playing)
		{
			if (!m_playing_entry_is_shown)
			{
				m_playing_entry_is_shown = true;
				m_frameBuffer->paintBoxRel(m_x, top + 2, m_width-2, 2 * ih, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE);
				g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(text_start,
						top + 1 * m_buttonHeight + 4, m_x + m_width - 8, m_playing_entry.title + " - " +
						m_playing_entry.artist, COL_MENUCONTENTDARK_TEXT, 0, true); // UTF-8
				g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(text_start,
						top + 2 * m_buttonHeight + 4, m_x + m_width - 8, m_playing_entry.album, COL_MENUCONTENTDARK_TEXT, 0, true); // UTF-8
			}
		}
		else
		{
			if (entry == NULL) return;
			m_playing_entry_is_shown = false;
			m_frameBuffer->paintBoxRel(m_x, top + 2, m_width-2, 2 * ih, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE);
			g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(text_start,
					top + 1 * m_buttonHeight + 4, m_x + m_width - 8, (*entry)[index].title + " - " +
					(*entry)[index].artist, COL_MENUCONTENTDARK_TEXT, 0, true); // UTF-8
			g_Font[SNeutrinoSettings::FONT_TYPE_FILEBROWSER_ITEM]->RenderString(text_start,
					top + 2 * m_buttonHeight + 4, m_x + m_width - 8, (*entry)[index].album, COL_MENUCONTENTDARK_TEXT, 0, true); // UTF-8
		}
	}
}

void CUpnpBrowserGui::paintItem2DetailsLine (int pos)
{
	if (dline) {
		dline->kill();
		delete dline;
		dline = NULL;
	}

	if (pos < 0)
		return;

	int xpos  = m_x - ConnectLineBox_Width;
	int ypos1 = m_y + m_title_height+0 + m_theight + pos*m_fheight;
	int ypos2 = m_y + (m_height - m_info_height - 1 * m_buttonHeight) + 2;

	int ypos1a = ypos1 + (m_fheight/2);
	int ypos2a = ypos2 + (m_info_height/2)-4;

	dline = new CComponentsDetailLine(xpos, ypos1a, ypos2a, m_fheight/2+1, m_info_height-RADIUS_LARGE*2);
	dline->paint(CC_SAVE_SCREEN_NO);
}

void CUpnpBrowserGui::updateTimes(const bool force)
{
	int top;
	if (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP)
	{
		bool updatePlayed = force;

		if ((m_time_played != CAudioPlayer::getInstance()->getTimePlayed()))
		{
			m_time_played = CAudioPlayer::getInstance()->getTimePlayed();
			updatePlayed = true;
		}

printf("updateTimes: force %d updatePlayed %d\n", force, updatePlayed);
		char play_time[8];
		snprintf(play_time, 7, "%ld:%02ld", m_time_played / 60, m_time_played % 60);
		char tmp_time[] = "000:00";
		int w = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth(tmp_time);

		if (updatePlayed)
		{
			paintDetails(NULL, 0, true);
			top = m_y + (m_height - m_info_height - 1 * m_buttonHeight) + m_buttonHeight + 4;
			m_frameBuffer->paintBoxRel(m_x + m_width - w - 15, top + 1, w + 4, m_buttonHeight, COL_MENUCONTENTDARK_PLUS_0);
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(m_x + m_width - w - 11, top + 1 + m_buttonHeight, w, play_time, COL_MENUCONTENTDARK_TEXT);
		}
	}
}

void CUpnpBrowserGui::playAudio(std::string name, std::string mime)
{
	CFile::FileType type = mime == "audio/mpeg" ? CFile::FILE_MP3 : mime == "audio/x-vorbis+ogg" ? CFile::FILE_OGG : CFile::FILE_FLAC;
	CAudiofile mp3(name, type);
	CAudioPlayer::getInstance()->play(&mp3, g_settings.audioplayer_highprio == 1);
}

void CUpnpBrowserGui::showPicture(std::string name)
{
	g_PicViewer->SetScaling((CPictureViewer::ScalingMode)g_settings.picviewer_scaling);
	g_PicViewer->SetVisible(g_settings.screen_StartX, g_settings.screen_EndX, g_settings.screen_StartY, g_settings.screen_EndY);

	if (g_settings.video_Format==3)
		g_PicViewer->SetAspectRatio(16.0/9);
	else
		g_PicViewer->SetAspectRatio(4.0/3);

	g_PicViewer->ShowImage(name, false);
	g_PicViewer->Cleanup();
}

void CUpnpBrowserGui::playVideo(std::string name, std::string url)
{
	if (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP)
		CAudioPlayer::getInstance()->stop();

	videoDecoder->StopPicture();
	CMoviePlayerGui::getInstance().SetFile(name, url);
	CMoviePlayerGui::getInstance().exec(NULL, "upnp");
}
