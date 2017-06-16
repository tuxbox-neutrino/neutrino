/*
  Neutrino-GUI  -   DBoxII-Project

  UPnP Browser (c) 2007 by Jochen Friedrich
               (c) 2009-2011,2016 Stefan Seyfried
               (c) 2016 Thilo Graf
               (c) 2016 Sven Hoefer

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sstream>
#include <stdexcept>

#include <global.h>
#include <neutrino.h>
#include <xmlinterface.h>
#include <upnpclient.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/audioplay.h>
#include <driver/audiofile.h>
#include <driver/audiometadata.h>
#include <driver/screen_max.h>
#include <driver/display.h>
#include <driver/pictureviewer/pictureviewer.h>

#include <gui/audiomute.h>
#include <gui/color.h>
#include <gui/movieplayer.h>
#include <gui/components/cc.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/hintbox.h>
#include <gui/infoclock.h>
#include <gui/upnpbrowser.h>
#include <system/settings.h>
#include <system/helpers.h>
#include <zapit/zapit.h>
#include <video.h>

extern cVideo * videoDecoder;
extern CPictureViewer * g_PicViewer;

const struct button_label RescanButton = {NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_UPNPBROWSER_RESCAN};
const struct button_label BrowseButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_FILEBROWSER_NEXTPAGE },
	{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_FILEBROWSER_PREVPAGE },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_AUDIOPLAYER_STOP },
	{ NEUTRINO_ICON_BUTTON_OKAY  , LOCALE_AUDIOPLAYER_PLAY },
	{ NEUTRINO_ICON_BUTTON_HOME ,  LOCALE_MENU_BACK, }
};

CUpnpBrowserGui::CUpnpBrowserGui()
{
	m_socket = new CUPnPSocket();
	m_frameBuffer = CFrameBuffer::getInstance();
	m_playing_entry_is_shown = false;

	Init();

	dline = NULL;
	image = NULL;

	sigc::slot0<void> reinit = sigc::mem_fun(this, &CUpnpBrowserGui::Init);
	CNeutrinoApp::getInstance()->OnAfterSetupFonts.connect(reinit);
	CFrameBuffer::getInstance()->OnAfterSetPallette.connect(reinit);
}

void CUpnpBrowserGui::Init()
{
	font_item = SNeutrinoSettings::FONT_TYPE_MENU;

	topbox.enableFrame(true, 2);
	topbox.setCorner(RADIUS_LARGE);
	topbox.setColorAll(COL_FRAME_PLUS_0, COL_MENUHEAD_PLUS_0, COL_SHADOW_PLUS_0, COL_MENUHEAD_TEXT);
	topbox.setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]);
	topbox.enableColBodyGradient(g_settings.theme.menu_Head_gradient, COL_SHADOW_PLUS_0, g_settings.theme.menu_Head_gradient_direction);
	topbox.enableShadow(CC_SHADOW_ON, -1, true);

	infobox.enableFrame(true, 2);
	infobox.setCorner(RADIUS_LARGE);
	infobox.setColorAll(topbox.getColorFrame(), COL_MENUCONTENTDARK_PLUS_0);
	infobox.setTextColor(COL_MENUCONTENTDARK_TEXT);
	infobox.setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);
	infobox.enableColBodyGradient(g_settings.theme.menu_Hint_gradient, COL_SHADOW_PLUS_0, g_settings.theme.menu_Hint_gradient_direction);
	infobox.enableShadow(CC_SHADOW_ON, -1, true);

	timebox.enableFrame(true, 2);
	timebox.setCorner(RADIUS_LARGE);
	timebox.setColorAll(infobox.getColorFrame(), infobox.getColorBody());
	timebox.setTextColor(infobox.getTextColor());
	timebox.setTextFont(g_Font[SNeutrinoSettings::FONT_TYPE_MENU]);
	timebox.enableColBodyGradient(g_settings.theme.menu_Hint_gradient, COL_SHADOW_PLUS_0, g_settings.theme.menu_Hint_gradient_direction);
	timebox.enableShadow(CC_SHADOW_ON, -1, true);

	m_width = m_frameBuffer->getScreenWidthRel();
	m_height = m_frameBuffer->getScreenHeightRel();

	int _top_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getHeight();
	int _title_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	int _info_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();

	m_item_height = g_Font[font_item]->getHeight();

	m_header_height = _title_height;
	m_footer_height = _title_height;
	m_topbox_height = _top_height*3 + OFFSET_INNER_MID; // topbox: 3 lines + inner offset
	m_infobox_height = _info_height*2 + OFFSET_INNER_LARGE; // infobox/timebox: 2 lines + inner offset

	/* From top to bottom we have:
	 *
	 * topbox (with shadow)
	 * OFFSET_INTER
	 * mainwindow (with shadow)
	 *  - header
	 *  - body (items*listshowmax)
	 *  - footer
	 * OFFSET_INTER
	 * infobox/timebox (with shadow)
	*/

	m_listmaxshow = (m_height - m_topbox_height - OFFSET_SHADOW - OFFSET_INTER - m_header_height - m_footer_height - OFFSET_SHADOW - OFFSET_INTER - m_infobox_height - OFFSET_SHADOW) / (m_item_height);

	// recalc height
	m_height = m_topbox_height + OFFSET_SHADOW + OFFSET_INTER + m_header_height + (m_listmaxshow * m_item_height) + m_footer_height + OFFSET_SHADOW + OFFSET_INTER + m_infobox_height + OFFSET_SHADOW;

	footer.setHeight(m_footer_height);
	footer.enableShadow(CC_SHADOW_ON, -1, true);

	m_x=getScreenStartX(m_width);
	if (m_x < DETAILSLINE_WIDTH) // shouldn't happen
		m_x = DETAILSLINE_WIDTH;
	m_y=getScreenStartY(m_height);

	// calc positions
	m_header_y = m_y + m_topbox_height + OFFSET_SHADOW + OFFSET_INTER;
	m_item_y = m_header_y + m_header_height;
	m_footer_y = m_item_y + (m_listmaxshow * m_item_height);
	m_infobox_y = m_footer_y + m_footer_height + OFFSET_SHADOW + OFFSET_INTER;
}

CUpnpBrowserGui::~CUpnpBrowserGui()
{
	delete m_socket;
	if (dline){
		delete dline; dline = NULL;
	}
	if (image)
		delete image, image = NULL;
}

int CUpnpBrowserGui::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	CAudioPlayer::getInstance()->init();

	if (parent)
		parent->hide();

	/* stop playback, disable playback start */
	CNeutrinoApp::getInstance()->stopPlayBack(true);
	m_frameBuffer->showFrame("mp3.jpg");

	// tell neutrino we're in upnp mode
	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE , NeutrinoMessages::mode_upnp);

	// remember last mode
	m_LastMode=(CNeutrinoApp::getInstance()->getLastMode());

	m_deviceliststart=0;
	m_selecteddevice=0;
	timeout = 0;

	selectDevice();

	stopAudio();

	m_frameBuffer->stopFrame();
	m_frameBuffer->Clear();

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

	CHintBox hintbox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_UPNPBROWSER_SCANNING)); // UTF-8
	hintbox.paint();

	try {
		m_devices = m_socket->Discover("urn:schemas-upnp-org:service:ContentDirectory:1");
	}
	catch (std::runtime_error error)
	{
		hintbox.hide();
		DisplayErrorMessage(error.what());
		return false;
	}
	hintbox.hide();
	if (m_devices.empty())
	{
		DisplayInfoMessage(g_Locale->getText(LOCALE_UPNPBROWSER_NOSERVERS));
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
		DisplayErrorMessage(error.what());
		return false;
	}
	return true;
}

std::vector<UPnPEntry> *CUpnpBrowserGui::decodeResult(std::string result)
{
	xmlNodePtr   root, node, snode;
	std::vector<UPnPEntry> *entries;

	xmlDocPtr parser = parseXml(result.c_str(),"UTF-8");
	root = xmlDocGetRootElement(parser);
	if (!root) {
		xmlFreeDoc(parser);
		return NULL;
	}
	entries = new std::vector<UPnPEntry>;

	for (node=xmlChildrenNode(root); node; node=xmlNextNode(node))
	{
		bool isdir;
		std::string title, artist = "", album = "", albumArtURI = "", id, children;
		const char *type, *p;

		if (!strcmp(xmlGetName(node), "container"))
		{
			std::vector<UPnPResource> resources;
			isdir=true;
			for (snode=xmlChildrenNode(node); snode; snode=xmlNextNode(snode))
			{
				type=xmlGetName(snode);
				p = strchr(type,':');
				if (p)
					type=p+1;
				if (!strcmp(type,"title"))
				{
					p=xmlGetData(snode);
					if (!p)
						p = "";
					title=std::string(p);
				}
			}
			p = xmlGetAttribute(node, "id");
			if (!p)
				p = "";
			id=std::string(p);

			p = xmlGetAttribute(node, "childCount");
			if (!p)
				p = "";
			children=std::string(p);

			UPnPEntry entry={id, isdir, title, artist, album, albumArtURI, children, "", "", resources, -1, CFile::FILE_DIR};
			entries->push_back(entry);
		}
		if (!strcmp(xmlGetName(node), "item"))
		{
			std::vector<UPnPResource> resources;
			int preferred = -1;
			std::string protocol, prot, network, mime, additional;
			CFile::FileType ftype = CFile::FILE_UNKNOWN;
			isdir=false;
			for (snode=xmlChildrenNode(node); snode; snode=xmlNextNode(snode))
			{
				std::string duration, url, size;
				unsigned int i;
				type=xmlGetName(snode);
				p = strchr(type,':');
				if (p)
					type=p+1;

				if (!strcmp(type,"title"))
				{
					p=xmlGetData(snode);
					if (!p)
						p = "";
					title=std::string(p);
				}
				else if (!strcmp(type,"artist"))
				{
					p=xmlGetData(snode);
					if (!p)
						p = "";
					artist=std::string(p);
				}
				else if (!strcmp(type,"album"))
				{
					p=xmlGetData(snode);
					if (!p)
						p = "";
					album=std::string(p);
				}
				else if (!strcmp(type,"albumArtURI"))
				{
					p=xmlGetData(snode);
					if (!p)
						p = "";
					albumArtURI=std::string(p);
				}
				else if (!strcmp(type,"res"))
				{
					p = xmlGetData(snode);
					if (!p)
						p = "";
					url=std::string(p);
					p = xmlGetAttribute(snode, "size");
					if (!p)
						p = "0";
					size=std::string(p);
					p = xmlGetAttribute(snode, "duration");
					if (!p)
						p = "";
					duration=std::string(p);
					p = xmlGetAttribute(snode, "protocolInfo");
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
						ftype = CFile::FILE_MP3;
					}
					if ((mime == "audio/ogg" || mime == "audio/x-ogg") && pref < 4)
					{
						ftype = CFile::FILE_OGG;
						preferred=i;
						pref=4;
					}
					if (mime == "audio/x-flac" && pref < 5)
					{
						preferred=i;
						pref=5;
						ftype = CFile::FILE_FLAC;
					}
					if (mime == "audio/x-wav" && pref < 6)
					{
						preferred=i;
						pref=6;
						ftype = CFile::FILE_WAV;
					}
					if (mime.substr(0,6) == "video/" && pref < 7)
					{
						preferred=i;
						pref=7;
					}
					if (mime == "video/x-flv" && pref < 8)
					{
						preferred=i;
						pref=8;
					}
					if (mime == "video/mp4" && pref < 9)
					{
						preferred=i;
						pref=9;
					}
				}
			}
			p = xmlGetAttribute(node, "id");
			if (!p)
				p = "";
			id=std::string(p);

			p = xmlGetAttribute(node, "childCount");
			if (!p)
				p = "";
			children=std::string(p);

			UPnPEntry entry={id, isdir, title, artist, album, albumArtURI, children, prot, mime, resources, preferred, ftype};
			entries->push_back(entry);
		}
	}
	xmlFreeDoc(parser);
	return entries;
}

void CUpnpBrowserGui::updateDeviceSelection(int newpos)
{
	if (newpos < 0) /* do not explode if called with -1 arg... */
		return;
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
		else if (msg_repeatok == CRCInput::RC_up || (int)msg == g_settings.key_pageup ||
			 msg_repeatok == CRCInput::RC_down || (int)msg == g_settings.key_pagedown)
		{
			int new_selected = UpDownKey(m_devices, msg_repeatok, m_listmaxshow, m_selecteddevice);
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
				msg == NeutrinoMessages::LEAVE_ALL ||
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
			//printf("msg: %x\n", (int) msg);
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
				std::string &mime = (*entries)[0].mime;
				if (mime.substr(0,6) == "audio/") {
					m_playing_entry = (*entries)[0];
					m_playing_entry_is_shown = false;
					playAudio((*entries)[0].resources[preferred].url, (*entries)[0].type);
				}
				else if (mime.substr(0,6) == "video/") {
					playVideo((*entries)[0].title, (*entries)[0].resources[preferred].url);
					m_folderplay = false; // FIXME else no way to stop in video folder
				}
				else if (mime.substr(0,6) == "image/") {
					if (m_folderplay)
						timeout = time(NULL) + g_settings.picviewer_slide_time;
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
		printf("updateItemSelection: list start old %u new %d selected old %d new %d\n", oldliststart, liststart, prev_selected, selected);
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

	while (loop && entries!=NULL) {
		updateTimes();
		updateMode();

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
		else if (!timeout && (msg_repeatok == CRCInput::RC_up || (int) msg == g_settings.key_pageup)) {
			int step = ((int) msg == g_settings.key_pageup) ? m_listmaxshow : 1;  // browse or step 1
			int new_selected = selected - step;
			if (new_selected < 0) {
				if (selected != 0 && step != 1)
					new_selected = 0;
				else
					new_selected = total - 1;
			}
			updateItemSelection(id, entries, new_selected, selected, liststart);
		}
		else if (!timeout && (msg_repeatok == CRCInput::RC_down || (int) msg == g_settings.key_pagedown)) {
			int step =  ((int) msg == g_settings.key_pagedown) ? m_listmaxshow : 1;  // browse or step 1
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
			if ((selected - liststart) >= (*entries).size())
				continue;
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
					std::string &mime = (*entries)[selected - liststart].mime;
					if (mime.substr(0,6) == "audio/")
					{
						m_playing_entry = (*entries)[selected - liststart];
						m_playing_entry_is_shown = false;
						playAudio((*entries)[selected - liststart].resources[preferred].url, (*entries)[selected - liststart].type);
					}
					else if (mime.substr(0,6) == "video/")
					{
						m_frameBuffer->Clear();
						playVideo((*entries)[selected - liststart].title, (*entries)[selected - liststart].resources[preferred].url);
						m_frameBuffer->showFrame("mp3.jpg");
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
			if ((selected - liststart) >= (*entries).size())
				continue;
			m_folderplay = true;
			m_playfolder = (*entries)[selected - liststart].id;
			m_playid = 0;
			playnext();
			m_playid++;
		}
		else if (msg == CRCInput::RC_yellow) {
			m_folderplay = false;
			stopAudio();
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
			stopAudio();
		}
		else if (msg == NeutrinoMessages::RECORD_START ||
				msg == NeutrinoMessages::ZAPTO ||
				msg == NeutrinoMessages::STANDBY_ON ||
				msg == NeutrinoMessages::LEAVE_ALL ||
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

#if 0
	/*
	   maybe it's better to stop audio here,
	   because infobox and timebox will never been painted and updated
	*/
	if (endall)
	{
		stopAudio();
	}
#endif

	delete entries;
	timeout = 0;
	m_frameBuffer->Clear();

	return endall;
}

void CUpnpBrowserGui::paintDeviceInfo()
{
	CVFD::getInstance()->showMenuText(0, m_devices[m_selecteddevice].friendlyname.c_str(), -1, true);

	// Info
	std::string tmp;

	// first line
	tmp = m_devices[m_selecteddevice].manufacturer + " " +
	      m_devices[m_selecteddevice].manufacturerurl + "\n";

	// second line
	tmp += m_devices[m_selecteddevice].modelname + " " +
	      m_devices[m_selecteddevice].modelnumber + " " +
	      m_devices[m_selecteddevice].modeldescription + "\n";

	// third line
	tmp += m_devices[m_selecteddevice].modelurl;

	topbox.setDimensionsAll(m_x, m_y, m_width, m_topbox_height);
	topbox.setCorner(RADIUS_LARGE);
	topbox.setText(tmp, CTextBox::AUTO_WIDTH);
	topbox.paint0();
}

void CUpnpBrowserGui::paintDevice(unsigned int _pos)
{
	int ypos = m_item_y + _pos*m_item_height;
	unsigned int pos = m_deviceliststart + _pos;

	bool i_selected	= pos == m_selecteddevice;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected);

	if (i_selected)
	{
		paintDeviceInfo();
		i_radius = RADIUS_LARGE;
	}

	if (i_radius)
		m_frameBuffer->paintBoxRel(m_x, ypos, m_width - SCROLLBAR_WIDTH, m_item_height, COL_MENUCONTENT_PLUS_0);
	m_frameBuffer->paintBoxRel(m_x, ypos, m_width - SCROLLBAR_WIDTH, m_item_height, bgcolor, i_radius);

	if (pos >= m_devices.size())
		return;

	std::string num = to_string(pos + 1);
	std::string name = m_devices[pos].friendlyname;

	int w = g_Font[font_item]->getRenderWidth(name);
	g_Font[font_item]->RenderString(m_x + OFFSET_INNER_MID, ypos + m_item_height, m_width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - w, num, color, m_item_height);
	g_Font[font_item]->RenderString(m_x + m_width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - w, ypos + m_item_height, w, name, color, m_item_height);
}

void CUpnpBrowserGui::paintDevices()
{
	// LCD
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, "Select UPnP Device");

	// Head
	CComponentsHeaderLocalized header(m_x, m_header_y, m_width, m_header_height, LOCALE_UPNPBROWSER_HEAD, NEUTRINO_ICON_UPNP);
	header.enableShadow( CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT, -1, true);
	if (CNeutrinoApp::getInstance()->isMuted()) //TODO: consider mute mode on runtime
		header.addContextButton(NEUTRINO_ICON_BUTTON_MUTE_SMALL);
	else
		header.removeContextButtons();
	header.setCorner(RADIUS_LARGE, CORNER_TOP);
	header.paint(CC_SAVE_SCREEN_NO);

	// Items
	for (unsigned int count=0; count<m_listmaxshow; count++)
		paintDevice(count);

	// scrollbar
	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, m_devices.size(), m_listmaxshow, m_selecteddevice);
	paintScrollBar(m_x + m_width - SCROLLBAR_WIDTH, m_item_y, SCROLLBAR_WIDTH, m_item_height*m_listmaxshow, total_pages, current_page);

	//shadow
	m_frameBuffer->paintBoxRel(m_x + m_width, m_item_y + OFFSET_SHADOW, OFFSET_SHADOW, m_item_height*m_listmaxshow, COL_SHADOW_PLUS_0);

	// Foot
	footer.setCorner(RADIUS_LARGE, CORNER_BOTTOM);
	footer.paintButtons(m_x, m_footer_y, m_width, m_footer_height, 1, &RescanButton, m_width/2);

	paintItem2DetailsLine(-1); // clear it
}

void CUpnpBrowserGui::paintItem(std::vector<UPnPEntry> *entries, unsigned int pos, unsigned int selected)
{
	int ypos = m_item_y + pos*m_item_height;

	bool i_selected	= pos == selected;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected);

	if (i_selected)
		i_radius = RADIUS_LARGE;

	if (i_radius)
		m_frameBuffer->paintBoxRel(m_x, ypos, m_width - SCROLLBAR_WIDTH, m_item_height, COL_MENUCONTENT_PLUS_0);
	m_frameBuffer->paintBoxRel(m_x, ypos, m_width - SCROLLBAR_WIDTH, m_item_height, bgcolor, i_radius);

	if (pos >= (*entries).size())
		return;

	UPnPEntry *entry = &(*entries)[pos];

	if (i_selected)
	{
		paintItemInfo(entry);
		paintDetails(entry);
		if (entry->isdir)
			paintItem2DetailsLine(-1); // clear it
		else
			paintItem2DetailsLine(pos);
	}

	int preferred=entry->preferred;
	std::string info;
	std::string fileicon;
	if (entry->isdir)
	{
		info = "<DIR>";
		fileicon = NEUTRINO_ICON_FOLDER;
	}
	else
	{
		if (preferred != -1)
		{
			info = entry->resources[preferred].duration;
			fileicon = NEUTRINO_ICON_MP3;
		}
		else
		{
			info = "(none)";
			fileicon = NEUTRINO_ICON_FILE;
		}
	}

	std::string name = entry->title;
	char tmp_time[] = "0:00:00.00";
	int w = g_Font[font_item]->getRenderWidth(tmp_time);

	int icon_w = 0;
	int icon_h = 0;
	int icon_o = 0;
	m_frameBuffer->getIconSize(fileicon.c_str(), &icon_w, &icon_h);
	if (icon_w && icon_h)
	{
		icon_o = icon_w + OFFSET_INNER_MID;
		m_frameBuffer->paintIcon(fileicon, m_x + OFFSET_INNER_MID, ypos + (m_item_height - icon_h)/2);
	}
	g_Font[font_item]->RenderString(m_x + OFFSET_INNER_MID + icon_o, ypos + m_item_height, m_width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - w, name, color, m_item_height);
	g_Font[font_item]->RenderString(m_x + m_width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - w, ypos + m_item_height, w, info, color, m_item_height);
}

void CUpnpBrowserGui::paintItemInfo(UPnPEntry *entry)
{
	std::string tmp;
	std::stringstream ts;

	int preferred=entry->preferred;

	// LCD
	CVFD::getInstance()->showMenuText(0, entry->title.c_str(), -1, true);

	// first line
	ts << "Resources: " << entry->resources.size() << " Selected: " << preferred+1 << " ";
	tmp = ts.str();

	if (preferred != -1)
		tmp = tmp + "Duration: " + entry->resources[preferred].duration;
	else
		tmp = tmp + "No resource for Item";
	tmp += "\n";

	// second line
	if (entry->isdir)
		tmp += "Directory";
	else
	{
		tmp += "";
		if (preferred != -1)
			tmp += "Protocol: " + entry->proto + ", MIME-Type: " + entry->mime;
	}
	tmp += "\n";

	//third line
	if (!entry->isdir && preferred != -1)
		tmp += "URL: " + entry->resources[preferred].url;

	static std::string lastname = "", tmpname = "";
	if(!entry->albumArtURI.empty()){
		if(lastname != entry->albumArtURI){
			tmpname = lastname = entry->albumArtURI.c_str();
			tmpname = g_PicViewer->DownloadImage(tmpname);
			int h_image = infobox.getHeight() - OFFSET_INTER - infobox.getCornerRadius();
			int y_image = infobox.getYPos() + infobox.getHeight()/2 - h_image/2;
			if (!image){
				image = new CComponentsPicture(0, y_image, tmpname, NULL, CC_SHADOW_OFF, COL_MENUCONTENTDARK_PLUS_0);
				image->doPaintBg(false);
				image->SetTransparent(CFrameBuffer::TM_BLACK);
			}
			image->setPicture(tmpname);
			image->setHeight(h_image, true);
			int x_image = infobox.getXPos() + infobox.getWidth() - image->getWidth() - OFFSET_INTER - infobox.getCornerRadius();
			image->setXPos(x_image);
		}
	}else{
		if (image){
			delete image; image = NULL;
		}
	}

	topbox.setCorner(RADIUS_LARGE);
	topbox.setText(tmp, CTextBox::AUTO_WIDTH);
	topbox.paint0();
}

void CUpnpBrowserGui::paintItems(std::vector<UPnPEntry> *entry, unsigned int selected, unsigned int max, unsigned int offset)
{
	printf("CUpnpBrowserGui::paintItem:s selected %d max %d offset %d\n", selected, max, offset);

	//block infoclock
	CInfoClock::getInstance()->block();

	// LCD
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, "Select UPnP Entry");

	// Head
	std::string name = g_Locale->getText(LOCALE_UPNPBROWSER_HEAD);
	name += " : ";
	name += m_devices[m_selecteddevice].friendlyname;
	CComponentsHeader header(m_x, m_header_y, m_width, m_header_height, name, NEUTRINO_ICON_UPNP);
	if (CNeutrinoApp::getInstance()->isMuted())
		header.setContextButton(NEUTRINO_ICON_BUTTON_MUTE_SMALL);
	header.paint(CC_SAVE_SCREEN_NO);

	// Items
	for (unsigned int count=0; count<m_listmaxshow; count++)
		paintItem(entry, count, selected);

	//scrollbar
	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, max + offset, m_listmaxshow, selected + offset);
	paintScrollBar(m_x + m_width - SCROLLBAR_WIDTH, m_item_y, SCROLLBAR_WIDTH, m_item_height*m_listmaxshow, total_pages, current_page);

	// Foot buttons
	size_t numbuttons = sizeof(BrowseButtons)/sizeof(BrowseButtons[0]);
	footer.paintButtons(m_x, m_footer_y, m_width, m_footer_height, numbuttons, BrowseButtons, m_width/numbuttons);
}

void CUpnpBrowserGui::paintDetails(UPnPEntry *entry, bool use_playing)
{
	// Foot info
	char tmp_time[] = "000:00";
	int timebox_width = timebox.getFont()->getRenderWidth(tmp_time) + OFFSET_INNER_MID*2;
	infobox.setDimensionsAll(m_x, m_infobox_y, m_width - OFFSET_SHADOW - OFFSET_INTER - timebox_width, m_infobox_height);
	infobox.setCorner(RADIUS_LARGE);
	timebox.setDimensionsAll(m_x + m_width - timebox_width, infobox.getYPos(), timebox_width, m_infobox_height);
	timebox.setCorner(RADIUS_LARGE);

	printf("paintDetails: use_playing %d shown %d\n", use_playing, m_playing_entry_is_shown);
	if ((!use_playing) && entry->isdir){
		infobox.kill();
		timebox.kill();
		m_playing_entry_is_shown = false;
	}else{
		string text = "";
		if (use_playing){
			if (!m_playing_entry_is_shown){
				m_playing_entry_is_shown = true;
				text = m_playing_entry.title;
				text += !m_playing_entry.artist.empty() ? " - " + m_playing_entry.artist : "";
				text += "\n" + m_playing_entry.album;
				infobox.setText(text, CTextBox::AUTO_WIDTH);
				infobox.paint0();
			}
		}else{
			if (!entry)
				return;
			m_playing_entry_is_shown = false;
			text = entry->title;
			text += !entry->artist.empty() ? " - " + entry->artist : "";
			text += "\n" + entry->album;
			infobox.setText(text, CTextBox::AUTO_WIDTH);
			infobox.paint0();
		}
		if (image)
			image->paint0();
		timebox.paint0();
	}
}

void CUpnpBrowserGui::paintItem2DetailsLine(int pos)
{
	if (pos < 0)
		return;

	int xpos  = m_x - DETAILSLINE_WIDTH;
	int ypos1 = m_item_y + pos*m_item_height;
	int ypos2 = infobox.getYPos() + infobox.getHeight() - infobox.getHeight()/2;

	int ypos1a = ypos1 + (m_item_height/2);

	if (!dline)
		dline = new CComponentsDetailsLine();
	dline->setDimensionsAll(xpos, ypos1a, ypos2, m_item_height/2, infobox.getHeight() - RADIUS_LARGE*2);
	dline->paint();
}

void CUpnpBrowserGui::updateTimes(const bool force)
{
	if (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP){
		bool updatePlayed = force;

		if ((m_time_played != CAudioPlayer::getInstance()->getTimePlayed())){
			m_time_played = CAudioPlayer::getInstance()->getTimePlayed();
			updatePlayed = true;
		}

		//printf("updateTimes: force %d updatePlayed %d\n", force, updatePlayed);
		char play_time[8];
		snprintf(play_time, 7, "%ld:%02ld", m_time_played / 60, m_time_played % 60);

		if (updatePlayed){
			timebox.setText(play_time, CTextBox::CENTER);
			timebox.paint0();
		}
	}
}

void CUpnpBrowserGui::updateMode()
{
	/* switch back to mode_upnp if audio has stopped automatically */
	if ((CAudioPlayer::getInstance()->getState() == CBaseDec::STOP) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_audio))
	{
		CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_upnp | NeutrinoMessages::norezap);
	}
}

void CUpnpBrowserGui::playAudio(std::string name, int type)
{
	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_audio);

	CAudiofile mp3(name, (CFile::FileType) type);
	CAudioPlayer::getInstance()->play(&mp3, g_settings.audioplayer_highprio == 1);
}

void CUpnpBrowserGui::stopAudio()
{
	if (CAudioPlayer::getInstance()->getState() != CBaseDec::STOP)
	{
		CAudioPlayer::getInstance()->stop();
	}

	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_upnp | NeutrinoMessages::norezap);
}

void CUpnpBrowserGui::showPicture(std::string name)
{
	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_pic);

	g_PicViewer->SetScaling((CPictureViewer::ScalingMode)g_settings.picviewer_scaling);
	g_PicViewer->SetVisible(g_settings.screen_StartX, g_settings.screen_EndX, g_settings.screen_StartY, g_settings.screen_EndY);

	if (g_settings.video_Format==3)
		g_PicViewer->SetAspectRatio(16.0/9);
	else
		g_PicViewer->SetAspectRatio(4.0/3);

	g_PicViewer->ShowImage(name, false);
	g_PicViewer->Cleanup();

	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_upnp | NeutrinoMessages::norezap);
}

void CUpnpBrowserGui::playVideo(std::string name, std::string url)
{
	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_ts);

	stopAudio();

	m_frameBuffer->stopFrame();
	CMoviePlayerGui::getInstance().SetFile(name, url);
	CMoviePlayerGui::getInstance().exec(NULL, "upnp");

	CNeutrinoApp::getInstance()->handleMsg(NeutrinoMessages::CHANGEMODE, NeutrinoMessages::mode_upnp | NeutrinoMessages::norezap);
}
