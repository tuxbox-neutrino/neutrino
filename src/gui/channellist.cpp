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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sstream>

#include <gui/channellist.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>
#include <driver/abstime.h>
#include <driver/record.h>
#include <driver/fade.h>

#include <gui/color.h>
#include <gui/eventlist.h>
#include <gui/infoviewer.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/progressbar.h>
#include <gui/osd_setup.h>

#include <system/settings.h>
#include <system/lastchannel.h>
#include <gui/customcolor.h>

#include <gui/bouquetlist.h>
#include <daemonc/remotecontrol.h>
#include <zapit/client/zapittools.h>
#include <gui/pictureviewer.h>

#include <zapit/zapit.h>
#include <zapit/satconfig.h>
#include <zapit/getservices.h>
#include <zapit/femanager.h>
#include <zapit/debug.h>

extern CBouquetList * bouquetList;       /* neutrino.cpp */
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */
extern CPictureViewer * g_PicViewer;
extern CBouquetList   * TVbouquetList;
extern CBouquetList   * TVsatList;
extern CBouquetList   * TVfavList;
extern CBouquetList   * TVallList;
extern CBouquetList   * RADIObouquetList;
extern CBouquetList   * RADIOsatList;
extern CBouquetList   * RADIOfavList;
extern CBouquetList   * RADIOallList;

extern bool autoshift;

extern CBouquetManager *g_bouquetManager;
void sectionsd_getChannelEvents(CChannelEventList &eList, const bool tv_mode, t_channel_id *chidlist, int clen);
void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
void sectionsd_getCurrentNextServiceKey(t_channel_id uniqueServiceKey, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next );

extern int old_b_id;

CChannelList::CChannelList(const char * const pName, bool phistoryMode, bool _vlist, bool )
{
	frameBuffer = CFrameBuffer::getInstance();
	name = pName;
	selected = 0;
	selected_in_new_mode = 0;
	liststart = 0;
	tuned = 0xfffffff;
	zapProtection = NULL;
	this->historyMode = phistoryMode;
	vlist = _vlist;
	selected_chid = 0;
	this->new_mode_active = false;
	footerHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+6; //initial height value for buttonbar
//printf("************ NEW LIST %s : %x\n", name.c_str(), (int) this);fflush(stdout);
}

CChannelList::~CChannelList()
{
//printf("************ DELETE LIST %s : %x\n", name.c_str(), this);fflush(stdout);
	chanlist.clear();
}

void CChannelList::ClearList(void)
{
//printf("************ CLEAR LIST %s : %x\n", name.c_str(), this);fflush(stdout);
	chanlist.clear();
	chanlist.resize(1);
}

void CChannelList::setSize(int newsize)
{
	//chanlist.reserve(newsize);
	chanlist.resize(newsize);
}

void CChannelList::SetChannelList(ZapitChannelList* channels)
{
	chanlist = *channels;
}

void CChannelList::addChannel(CZapitChannel* channel, int num)
{
//printf("************ %s : addChannel: %s %x\n", name.c_str(), channel->getName().c_str(), channel);fflush(stdout);
	if(num)
		channel->number = num;
	chanlist.push_back(channel);
}

void CChannelList::putChannel(CZapitChannel* channel)
{
	int num = channel->number - 1;
	if(num < 0) {
		printf("%s error inserting at %d\n", __FUNCTION__, num);
		return;
	}
	if(num >= (int) chanlist.size()) {
		chanlist.resize((unsigned) num + 1);
	}
	chanlist[num] = channel;
//printf("************ %s : me %x putChannel: %d: %s %x -> %x [0] %x\n", name.c_str(), this, num, channel->getName().c_str(), channel, chanlist[num], chanlist[0]);fflush(stdout);
}

/* uodate the events for the visible channel list entries
   from = start entry, to = end entry. If both = zero, update all */
void CChannelList::updateEvents(unsigned int from, unsigned int to)
{
	CChannelEventList events;

	if (to == 0 || to > chanlist.size())
		to = chanlist.size();

	size_t chanlist_size = to - from;
	if (chanlist_size <= 0) // WTF???
		return;

	if (displayNext) {
		time_t atime = time(NULL);
		unsigned int count;
		for (count = from; count < to; count++) {
			events.clear();
			sectionsd_getEventsServiceKey(chanlist[count]->channel_id, events);
			chanlist[count]->nextEvent.startTime = (long)0x7fffffff;
			for ( CChannelEventList::iterator e= events.begin(); e != events.end(); ++e ) {
				if ((long)e->startTime > atime &&
						(e->startTime < (long)chanlist[count]->nextEvent.startTime))
				{
					chanlist[count]->nextEvent = *e;
					break;
				}
			}
		}
	} else {
		t_channel_id *p_requested_channels;
		int size_requested_channels = chanlist_size * sizeof(t_channel_id);
		p_requested_channels = new t_channel_id[size_requested_channels];
		if (! p_requested_channels) {
			fprintf(stderr,"%s:%d allocation failed!\n", __FUNCTION__, __LINE__);
			return;
		}
		for (uint32_t count = 0; count < chanlist_size; count++) {
			p_requested_channels[count] = chanlist[count + from]->channel_id&0xFFFFFFFFFFFFULL;
		}
		CChannelEventList levents;
		sectionsd_getChannelEvents(levents, (CNeutrinoApp::getInstance()->getMode()) != NeutrinoMessages::mode_radio, p_requested_channels, size_requested_channels);
		for (uint32_t count=0; count < chanlist_size; count++) {
			chanlist[count]->currentEvent = CChannelEvent();
			for (CChannelEventList::iterator e = levents.begin(); e != levents.end(); ++e) {
				if ((chanlist[count + from]->channel_id&0xFFFFFFFFFFFFULL) == e->get_channel_id()) {
					chanlist[count + from]->currentEvent = *e;
					break;
				}
			}
		}
		delete[] p_requested_channels;
	}
	events.clear();
}

void CChannelList::SortAlpha(void)
{
	sort(chanlist.begin(), chanlist.end(), CmpChannelByChName());
}

void CChannelList::SortSat(void)
{
	sort(chanlist.begin(), chanlist.end(), CmpChannelBySat());
}

void CChannelList::SortTP(void)
{
	sort(chanlist.begin(), chanlist.end(), CmpChannelByFreq());
}

CZapitChannel* CChannelList::getChannel(int number)
{
	for (uint32_t i=0; i< chanlist.size(); i++) {
		if (chanlist[i]->number == number)
			return chanlist[i];
	}
	return(NULL);
}

CZapitChannel* CChannelList::getChannel(t_channel_id channel_id)
{
	for (uint32_t i=0; i< chanlist.size(); i++) {
		if (chanlist[i]->channel_id == channel_id)
			return chanlist[i];
	}
	return(NULL);
}

int CChannelList::getKey(int id)
{
	return chanlist[id]->number;
}

static const std::string empty_string;

const std::string CChannelList::getActiveChannelName(void) const
{
	if (selected < chanlist.size())
		return chanlist[selected]->getName();
	else
		return empty_string;
}

t_satellite_position CChannelList::getActiveSatellitePosition(void) const
{
	if (selected < chanlist.size())
		return chanlist[selected]->getSatellitePosition();
	else
		return 0;
}

t_channel_id CChannelList::getActiveChannel_ChannelID(void) const
{
	if (selected < chanlist.size()) {
//printf("CChannelList::getActiveChannel_ChannelID me %x selected = %d %llx\n", (int) this, selected, chanlist[selected]->channel_id);
		return chanlist[selected]->channel_id;
	} else
		return 0;
}

int CChannelList::getActiveChannelNumber(void) const
{
	//return (selected + 1);
	if (selected < chanlist.size())
		return chanlist[selected]->number;
	return 0;
}

CZapitChannel * CChannelList::getActiveChannel(void) const
{
	static CZapitChannel channel("Channel not found", 0, 0, 0, 0);
	if (selected < chanlist.size())
		return chanlist[selected];
	return &channel;
}

int CChannelList::doChannelMenu(void)
{
	int i = 0;
	int select = -1;
	static int old_selected = 0;
	char cnt[5];
	bool enabled = true;

	if(g_settings.minimode)
		return 0;

	if(vlist)
	{
		enabled = false;
		if(old_selected < 2)//FIXME take care if some items added before 0, 1
			old_selected = 2;
	}

	CMenuWidget* menu = new CMenuWidget(LOCALE_CHANNELLIST_EDIT, NEUTRINO_ICON_SETTINGS);
	menu->enableFade(false);
	menu->enableSaveScreen(true);
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_DELETE, enabled, NULL, selector, cnt, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED), old_selected == i++);
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_MOVE, enabled, NULL, selector, cnt, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN), old_selected == i++);
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ADD_TO_BOUQUET, true, NULL, selector, cnt, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW), old_selected == i++);
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_FAVORITES_MENUEADD, true, NULL, selector, cnt, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE), old_selected == i++);
	snprintf(cnt, sizeof(cnt), "%d", i);
	bool reset_enabled = chanlist[selected]->flags & CZapitChannel::NEW;
	menu->addItem(new CMenuForwarder(LOCALE_CHANNELLIST_RESET_FLAGS, reset_enabled, NULL, selector, cnt, CRCInput::convertDigitToKey(0)), old_selected == i++);
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, selector, cnt, CRCInput::convertDigitToKey(0)), old_selected == i++);
	menu->exec(NULL, "");
	delete menu;
	delete selector;

	if(select >= 0) {
		signed int bouquet_id = 0, old_bouquet_id = 0, new_bouquet_id = 0;
		old_selected = select;
		t_channel_id channel_id = chanlist[selected]->channel_id;
		switch(select) {
		case 0: {
			hide();
			int result = ShowMsgUTF ( LOCALE_BOUQUETEDITOR_DELETE, g_Locale->getText(LOCALE_BOUQUETEDITOR_DELETE_QUESTION), CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo );

			if(result == CMessageBox::mbrYes) {
				bouquet_id = bouquetList->getActiveBouquetNumber();
				/* FIXME if bouquet name not unique, this is bad,
				 * existsBouquet can find wrong bouquet */
				if(!strcmp(bouquetList->Bouquets[bouquet_id]->channelList->getName(), g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME)))
					bouquet_id = g_bouquetManager->existsUBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
				else
					bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[bouquet_id]->channelList->getName());

				if (bouquet_id == -1)
					return 0;
				if(g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id)) {
					g_bouquetManager->Bouquets[bouquet_id]->removeService(channel_id);
					return 1;
				}
			}
			break;
		}
		case 1: // move
			old_bouquet_id = bouquetList->getActiveBouquetNumber();
			old_bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[old_bouquet_id]->channelList->getName());
			do {
				new_bouquet_id = bouquetList->exec(false);
			} while(new_bouquet_id == -3);

			hide();
			if(new_bouquet_id < 0)
				return 0;
			new_bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[new_bouquet_id]->channelList->getName());
			if ((new_bouquet_id == -1) || (new_bouquet_id == old_bouquet_id))
				return 0;

			if(!g_bouquetManager->existsChannelInBouquet(new_bouquet_id, channel_id)) {
				CZapit::getInstance()->addChannelToBouquet(new_bouquet_id, channel_id);
			}
			if(g_bouquetManager->existsChannelInBouquet(old_bouquet_id, channel_id)) {
				g_bouquetManager->Bouquets[old_bouquet_id]->removeService(channel_id);
			}
			return 1;

			break;
		case 2: // add to
			do {
				bouquet_id = bouquetList->exec(false);
			} while(bouquet_id == -3);
			hide();
			if(bouquet_id < 0)
				return 0;

			if(!strcmp(bouquetList->Bouquets[bouquet_id]->channelList->getName(), g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME)))
				bouquet_id = g_bouquetManager->existsUBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
			else
				bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[bouquet_id]->channelList->getName());

			if (bouquet_id == -1)
				return 0;
			if(!g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id)) {
				CZapit::getInstance()->addChannelToBouquet(bouquet_id, channel_id);
				return 1;
			}
			break;
		case 3: // add to my favorites
			bouquet_id = g_bouquetManager->existsUBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
			if(bouquet_id == -1) {
				g_bouquetManager->addBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
				bouquet_id = g_bouquetManager->existsUBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
			}
			if(!g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id)) {
				CZapit::getInstance()->addChannelToBouquet(bouquet_id, channel_id);
				return 1;
			}

			break;
		case 4: // reset new
			chanlist[selected]->flags &= ~CZapitChannel::NEW;
			CServiceManager::getInstance()->SetServicesChanged(true);
			/* if make_new_list == ON, signal to re-init services */
			if(g_settings.make_new_list)
				return 1;
			break;
		case 5: // settings
			{
				COsdSetup osd_setup;
				osd_setup.showContextChanlistMenu();
				//FIXME check font/options changed ?
				hide();
				calcSize();
			}
			break;
		default:
			break;
		}
	}
	return 0;
}

int CChannelList::exec()
{
	displayNext = 0; // always start with current events
	int nNewChannel = show();
	if ( nNewChannel > -1 && nNewChannel < (int) chanlist.size()) {
		if(this->historyMode && chanlist[nNewChannel]) {
			int new_mode = CNeutrinoApp::getInstance()->channelList->getLastChannels().get_mode(chanlist[nNewChannel]->channel_id);
			if(new_mode >= 0)
				CNeutrinoApp::getInstance()->SetChannelMode(new_mode);
		}
		CNeutrinoApp::getInstance()->channelList->zapToChannel(chanlist[nNewChannel]);
	}

	return nNewChannel;
}

void CChannelList::calcSize()
{
	const int pic_h = 39;
	int  fw = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getWidth();
	width  = w_max (((g_settings.channellist_extended)?(frameBuffer->getScreenWidth() / 20 * (fw+6)):(frameBuffer->getScreenWidth() / 20 * (fw+5))), 100);
	height = h_max ((frameBuffer->getScreenHeight() / 20 * 16), (frameBuffer->getScreenHeight() / 20 * 2));

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, name.c_str());

	/* assuming all color icons must have same size */
	int icol_w, icol_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);

	theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();

	theight = std::max(theight, pic_h);

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_HELP, &icol_w, &icol_h);
	theight = std::max(theight, icol_h);

	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MENU, &icol_w, &icol_h);
	theight = std::max(theight, icol_h);

	if(g_settings.channellist_new_zap_mode)
	{
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE, &icol_w, &icol_h);
		theight = std::max(theight, icol_h);
	}

	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	if (fheight == 0)
		fheight = 1; /* avoid crash on invalid font */

	listmaxshow = (height - theight - footerHeight -0)/fheight;
	height = theight + footerHeight + listmaxshow * fheight;
	info_height = 2*fheight + g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getHeight() + 10;

	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - (height+ info_height)) / 2;
}

bool CChannelList::updateSelection(int newpos)
{
	bool actzap = false;
	if((int) selected != newpos) {
		int prev_selected = selected;
		unsigned int oldliststart = liststart;

		selected = newpos;
		liststart = (selected/listmaxshow)*listmaxshow;
		if (oldliststart != liststart)
			paint();
		else {
			paintItem(prev_selected - liststart);
			paintItem(selected - liststart);
			showChannelLogo();
		}

		if(this->new_mode_active && SameTP()) {
			actzap = true;
			zapTo(selected);
		}
	}
	return actzap;
}

#define CHANNEL_SMSKEY_TIMEOUT 800
/* return: >= 0 to zap, -1 on cancel, -3 on list mode change, -4 list edited, -2 zap but no restore old list/chan ?? */
int CChannelList::show()
{
	/* temporary debugging stuff */
	struct timeval t1, t2;
	gettimeofday(&t1, NULL);

	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool actzap = 0;
	int res = -1;
	if (chanlist.empty()) {
		return res;
	}

	this->new_mode_active = 0;

	calcSize();
	displayNext = false;

	COSDFader fader(g_settings.menu_Content_alpha);
	fader.StartFadeIn();

	paintHead();
	paint();

	gettimeofday(&t2, NULL);
	fprintf(stderr, "CChannelList::show(): %llu ms to paint channellist\n",
		((t2.tv_sec * 1000000ULL + t2.tv_usec) - (t1.tv_sec * 1000000ULL + t1.tv_usec)) / 1000ULL);

	int oldselected = selected;
	int zapOnExit = false;
	bool bShowBouquetList = false;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

	bool loop=true;
	while (loop) {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);
		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

		if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetTimer())) {
			if(fader.Fade()) {
				loop = false;
			}
		}
		else if ( ( msg == CRCInput::RC_timeout ) || ( msg == (neutrino_msg_t)g_settings.key_channelList_cancel) ) {
			res = -1;
			if(!actzap) {
				selected = oldselected;
			}
			else {
				res = -4;
				selected = selected_in_new_mode;
			}
			if(fader.StartFadeOut()) {
				timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
				msg = 0;
			} else
				loop=false;
		}
		else if( msg == CRCInput::RC_record) { //start direct recording from channellist 
			if(!CRecordManager::getInstance()->RecordingStatus(chanlist[selected]->channel_id)) 
			{
				printf("[neutrino channellist] start direct recording...\n");
				hide();
				if (CRecordManager::getInstance()->Record(chanlist[selected]->channel_id))
				{
					if(SameTP())
					{
						zapOnExit = true;
						loop=false;
					}
					else
						DisplayInfoMessage(g_Locale->getText(LOCALE_CHANNELLIST_RECORDING_NOT_POSSIBLE)); // UTF-8	
				}
					
			}		
				
		}
		else if( msg == CRCInput::RC_stop ) { //stopp recording
			if(CRecordManager::getInstance()->RecordingStatus())
			{
				if (CRecordManager::getInstance()->AskToStop(chanlist[selected]->channel_id))
				{
					CRecordManager::getInstance()->Stop(chanlist[selected]->channel_id);
					paint();
				}
			}
		}
		else if ((msg == CRCInput::RC_red) || (msg == CRCInput::RC_epg)) {
			hide();

			if ( g_EventList->exec(chanlist[selected]->channel_id, chanlist[selected]->getName()) == menu_return::RETURN_EXIT_ALL) {
				res = -2;
				loop = false;
			}
			paintHead();
			paint();
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
		}
		else if (msg == CRCInput::RC_yellow) {
			bShowBouquetList = true;
			loop=false;
		}
		else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
			g_RCInput->postMsg (msg, 0);
			loop = false;
			res = -1;
		}
		else if ( msg == CRCInput::RC_setup) {
			old_b_id = bouquetList->getActiveBouquetNumber();
			fader.Stop();
			CNeutrinoApp::getInstance()->g_channel_list_changed = doChannelMenu();
			if(CNeutrinoApp::getInstance()->g_channel_list_changed) {
				res = -4;
				loop = false;
			} else {
				old_b_id = -1;
				paintHead();
				paint();
			}
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_start) {
			actzap = updateSelection(0);
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_end) {
			actzap = updateSelection(chanlist.size()-1);
		}
		else if (msg == CRCInput::RC_up || (int) msg == g_settings.key_channelList_pageup)
		{
			int step = ((int) msg == g_settings.key_channelList_pageup) ? listmaxshow : 1;  // browse or step 1
			int new_selected = selected - step;
			if (new_selected < 0) {
				if (selected != 0 && step != 1)
					new_selected = 0;
				else
					new_selected = chanlist.size() - 1;
			}
			actzap = updateSelection(new_selected);
		}
		else if (msg == CRCInput::RC_down || (int) msg == g_settings.key_channelList_pagedown)
		{
			int step =  ((int) msg == g_settings.key_channelList_pagedown) ? listmaxshow : 1;  // browse or step 1
			int new_selected = selected + step;
			if (new_selected >= (int) chanlist.size()) {
				if ((chanlist.size() - listmaxshow -1 < selected) && (selected != (chanlist.size() - 1)) && (step != 1))
					new_selected = chanlist.size() - 1;
				else if (((chanlist.size() / listmaxshow) + 1) * listmaxshow == chanlist.size() + listmaxshow) // last page has full entries
					new_selected = 0;
				else
					new_selected = ((step == (int) listmaxshow) && (new_selected < (int) (((chanlist.size() / listmaxshow)+1) * listmaxshow))) ? (chanlist.size() - 1) : 0;
			}
			actzap = updateSelection(new_selected);
		}
		else if (msg == (neutrino_msg_t)g_settings.key_bouquet_up) {
			if (!bouquetList->Bouquets.empty()) {
				bool found = true;
				uint32_t nNext = (bouquetList->getActiveBouquetNumber()+1) % bouquetList->Bouquets.size();
				if(bouquetList->Bouquets[nNext]->channelList->getSize() <= 0) {
					found = false;
					nNext = nNext < bouquetList->Bouquets.size()-1 ? nNext+1 : 0;
					for(uint32_t i = nNext; i < bouquetList->Bouquets.size(); i++) {
						if(bouquetList->Bouquets[i]->channelList->getSize() > 0) {
							found = true;
							nNext = i;
							break;
						}
					}
				}
				if(found) {
					bouquetList->activateBouquet(nNext, false);
					res = bouquetList->showChannelList();
					loop = false;
				}
			}
		}
		else if (msg == (neutrino_msg_t)g_settings.key_bouquet_down) {
			if (!bouquetList->Bouquets.empty()) {
				bool found = true;
				int nNext = (bouquetList->getActiveBouquetNumber()+bouquetList->Bouquets.size()-1) % bouquetList->Bouquets.size();
				if(bouquetList->Bouquets[nNext]->channelList->getSize() <= 0) {
					found = false;
					nNext = nNext > 0 ? nNext-1 : bouquetList->Bouquets.size()-1;
					for(int i = nNext; i > 0; i--) {
						if(bouquetList->Bouquets[i]->channelList->getSize() > 0) {
							found = true;
							nNext = i;
							break;
						}
					}
				}
				if(found) {
					bouquetList->activateBouquet(nNext, false);
					res = bouquetList->showChannelList();
					loop = false;
				}
			}
		}
		else if ( msg == CRCInput::RC_ok ) {
			if(SameTP()) {
				zapOnExit = true;
				loop=false;
			}
		}
		else if (( msg == CRCInput::RC_spkr ) && g_settings.channellist_new_zap_mode ) {
			this->new_mode_active = (this->new_mode_active ? 0 : 1);
			paintHead();
			showChannelLogo();
		}
		else if (CRCInput::isNumeric(msg) && (this->historyMode || g_settings.sms_channel)) {
			if (this->historyMode) { //numeric zap
				selected = CRCInput::getNumericValue(msg);
				zapOnExit = true;
				loop = false;
			}
			else if(g_settings.sms_channel) { 				
				unsigned char smsKey = 0;
				SMSKeyInput smsInput;
				smsInput.setTimeout(CHANNEL_SMSKEY_TIMEOUT);

				do {
					smsKey = smsInput.handleMsg(msg);
					//printf("SMS new key: %c\n", smsKey);
					g_RCInput->getMsg_ms(&msg, &data, CHANNEL_SMSKEY_TIMEOUT-100);
				} while ((msg >= CRCInput::RC_1) && (msg <= CRCInput::RC_9));

				if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_nokey) {
					uint32_t i;
					for(i = selected+1; i < chanlist.size(); i++) {
						char firstCharOfTitle = chanlist[i]->getName().c_str()[0];
						if(tolower(firstCharOfTitle) == smsKey) {
							//printf("SMS chan found was= %d selected= %d i= %d %s\n", was_sms, selected, i, chanlist[i]->channel->getName().c_str());
							break;
						}
					}
					if(i >= chanlist.size()) {
						for(i = 0; i < chanlist.size(); i++) {
							char firstCharOfTitle = chanlist[i]->getName().c_str()[0];
							if(tolower(firstCharOfTitle) == smsKey) {
								//printf("SMS chan found was= %d selected= %d i= %d %s\n", was_sms, selected, i, chanlist[i]->channel->getName().c_str());
								break;
							}
						}
					}
					if(i < chanlist.size()) {
						int prevselected=selected;
						selected=i;

						paintItem(prevselected - liststart);
						unsigned int oldliststart = liststart;
						liststart = (selected/listmaxshow)*listmaxshow;
						if(oldliststart!=liststart) {
							paint();
						} else {
							paintItem(selected - liststart);
							showChannelLogo();
						}
					}
					smsInput.resetOldKey();
				}
			}
		}
		else if(CRCInput::isNumeric(msg)) {
			//pushback key if...
			selected = oldselected;
			g_RCInput->postMsg( msg, data );
			loop=false;
		}
		else if ( msg == CRCInput::RC_blue )
		{
			displayNext = !displayNext;
			paint();
			paintHead(); // update button bar
			showChannelLogo();
		}
		else if ( msg == CRCInput::RC_green )
		{
			int mode = CNeutrinoApp::getInstance()->GetChannelMode();
			if(mode){
				g_settings.channellist_sort_mode++;
				if(g_settings.channellist_sort_mode > 2)
					g_settings.channellist_sort_mode = 0;
				CNeutrinoApp::getInstance()->SetChannelMode(mode);
				paint();
				paintHead(); // update button bar
			}
		}

		else if ((msg == CRCInput::RC_info) || (msg == CRCInput::RC_help)) {
			hide();
			CChannelEvent *p_event=NULL;
			if (displayNext)
			{
				p_event = &(chanlist[selected]->nextEvent);
			}

			if(p_event && p_event->eventID)
			{
				g_EpgData->show(chanlist[selected]->channel_id,p_event->eventID,&(p_event->startTime));
			}
			else
			{
				g_EpgData->show(chanlist[selected]->channel_id);
			}
			paintHead();
			paint();
		} else {
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
				loop = false;
				res = - 2;
			}
		}
	}
	hide();

	fader.Stop();

	if (bShowBouquetList) {
		res = bouquetList->exec(true);
		printf("CChannelList:: bouquetList->exec res %d\n", res);
	}
#if 0
	/* FIXME call this somewhere after show */
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
#endif
	this->new_mode_active = 0;

	if(NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode())
		return -1;

	if(zapOnExit) {
		res = selected;
		//selected_chid = chanlist[selected]->channel_id;
	}

	printf("CChannelList::show *********** res %d\n", res);
	return(res);
}

void CChannelList::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width, height+ info_height+ 5);
	clearItem2DetailsLine();
}

bool CChannelList::showInfo(int number, int epgpos)
{
	CZapitChannel* channel = getChannel(number);
	if(channel == NULL)
		return false;

	g_InfoViewer->showTitle(channel, true, epgpos); // UTF-8
	return true;
}

int CChannelList::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	bool startvideo = true;

	if (msg != NeutrinoMessages::EVT_PROGRAMLOCKSTATUS) // right now the only message handled here.
		return messages_return::unhandled;

	//printf("===> program-lock-status: %d zp: %d\n", data, zapProtection != NULL);

	if (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_NEVER)
		goto out;

	// 0x100 als FSK-Status zeigt an, dass (noch) kein EPG zu einem Kanal der NICHT angezeigt
	// werden sollte (vorgesperrt) da ist
	// oder das bouquet des Kanals ist vorgesperrt

	if (zapProtection != NULL) {
		zapProtection->fsk = data;
		startvideo = false;
		goto out;
	}

	// require password if either
	// CHANGETOLOCK mode and channel/bouquet is pre locked (0x100)
	// ONSIGNAL mode and fsk(data) is beyond configured value
	// if program has already been unlocked, dont require pin
	if (data < (neutrino_msg_data_t)g_settings.parentallock_lockage)
		goto out;

	/* already unlocked */
	if (chanlist[selected]->last_unlocked_EPGid == g_RemoteControl->current_EPGid && g_RemoteControl->current_EPGid != 0)
		goto out;

	/* PARENTALLOCK_PROMPT_CHANGETOLOCKED: only pre-locked channels, don't care for fsk sent in SI */
	if (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED && data < 0x100)
		goto out;

	/* if a pre-locked channel is inside the zap time, open it. Hardcoded to one hour for now. */
	if (data >= 0x100 && chanlist[selected]->last_unlocked_time + 3600 > time_monotonic())
		goto out;

	/* OK, let's ask for a PIN */
	g_RemoteControl->stopvideo();
	//printf("stopped video\n");
	zapProtection = new CZapProtection(g_settings.parentallock_pincode, data);

	if (zapProtection->check())
	{
		// remember it for the next time
		/* data < 0x100: lock age -> remember EPG ID */
		if (data < 0x100)
			chanlist[selected]->last_unlocked_EPGid = g_RemoteControl->current_EPGid;
		else
		{
			/* data >= 0x100: pre locked bouquet -> remember unlock time */
			chanlist[selected]->last_unlocked_time = time_monotonic();
			int bnum = bouquetList->getActiveBouquetNumber();
			if (bnum >= 0)
			{
				/* unlock the whole bouquet */
				int i;
				for (i = 0; i < bouquetList->Bouquets[bnum]->channelList->getSize(); i++)
					bouquetList->Bouquets[bnum]->channelList->getChannelFromIndex(i)->last_unlocked_time = chanlist[selected]->last_unlocked_time;
			}
		}
	}
	else
	{
		/* last_unlocked_time == 0 is the magic to tell zapTo() to not record the time.
		   Without that, zapping to a locked channel twice would open it without the PIN */
		chanlist[selected]->last_unlocked_time = 0;
		startvideo = false;
	}
	delete zapProtection;
	zapProtection = NULL;

out:
	if (startvideo)
		g_RemoteControl->startvideo();

	return messages_return::handled;
}

/* bToo default to true */
/* TODO make this member of CNeutrinoApp, because this only called from "whole" list ? */
bool CChannelList::adjustToChannelID(const t_channel_id channel_id, bool bToo)
{
	unsigned int i;

	selected_chid = channel_id;
	printf("CChannelList::adjustToChannelID me %p [%s] list size %d channel_id %llx\n", this, getName(), chanlist.size(), channel_id);
	fflush(stdout);
	for (i = 0; i < chanlist.size(); i++) {
		if(chanlist[i] == NULL) {
			printf("CChannelList::adjustToChannelID REPORT BUG !! ******************************** %d is NULL !!\n", i);
			continue;
		}
		if (chanlist[i]->channel_id == channel_id) {
			selected = i;
			tuned = i;

			lastChList.store (selected, channel_id, false);

			if (bToo) {
				int old_mode = CNeutrinoApp::getInstance()->GetChannelMode();
				int new_mode = old_mode;
				bool has_channel;
				if(CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_tv) {
					has_channel = TVfavList->adjustToChannelID(channel_id);
					if(!has_channel && old_mode == LIST_MODE_FAV)
						new_mode = LIST_MODE_PROV;
					has_channel = TVbouquetList->adjustToChannelID(channel_id);
					if(!has_channel && old_mode == LIST_MODE_PROV) {
						new_mode = LIST_MODE_SAT;
					}
					has_channel = TVsatList->adjustToChannelID(channel_id);
					if(!has_channel && old_mode == LIST_MODE_SAT)
						new_mode = LIST_MODE_ALL;
					has_channel = TVallList->adjustToChannelID(channel_id);
				}
				else if(CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio) {
					has_channel = RADIOfavList->adjustToChannelID(channel_id);
					if(!has_channel && old_mode == LIST_MODE_FAV)
						new_mode = LIST_MODE_PROV;
					has_channel = RADIObouquetList->adjustToChannelID(channel_id);
					if(!has_channel && old_mode == LIST_MODE_PROV)
						new_mode = LIST_MODE_SAT;
					has_channel = RADIOsatList->adjustToChannelID(channel_id);
					if(!has_channel && old_mode == LIST_MODE_SAT)
						new_mode = LIST_MODE_ALL;
					has_channel = RADIOallList->adjustToChannelID(channel_id);
				}
				if(old_mode != new_mode)
					CNeutrinoApp::getInstance()->SetChannelMode(new_mode);
			}
//printf("CChannelList::adjustToChannelID me %x to %llx bToo %s OK: %d\n", (int) this, channel_id, bToo ? "yes" : "no", i);fflush(stdout);
			return true;
		}
	}
//printf("CChannelList::adjustToChannelID me %x to %llx bToo %s FAILED\n", (int) this, channel_id, bToo ? "yes" : "no");fflush(stdout);

	return false;
}

#if 0
int CChannelList::hasChannel(int nChannelNr)
{
	for (uint32_t i=0; i<chanlist.size(); i++) {
		if (getKey(i) == nChannelNr)
			return(i);
	}
	return(-1);
}
#endif

int CChannelList::hasChannelID(t_channel_id channel_id)
{
	for (uint32_t i=0; i < chanlist.size(); i++) {
		if(chanlist[i] == NULL) {
			printf("CChannelList::hasChannelID REPORT BUG !! ******************************** %d is NULL !!\n", i);
			continue;
		}
		if (chanlist[i]->channel_id == channel_id)
			return i;
	}
	return -1;
}

// for adjusting bouquet's channel list after numzap or quickzap
void CChannelList::setSelected( int nChannelNr)
{
//printf("CChannelList::setSelected me %s %d -> %s\n", name.c_str(), nChannelNr, (nChannelNr < chanlist.size() && chanlist[nChannelNr] != NULL) ? chanlist[nChannelNr]->getName().c_str() : "********* NONE *********");
	//FIXME real difference between tuned and selected ?!
	selected_chid = 0;
	tuned = nChannelNr;
	if (nChannelNr < (int) chanlist.size()) {
		selected = nChannelNr;
		selected_chid = chanlist[tuned]->getChannelID();
	}
}

// -- Zap to channel with channel_id
bool CChannelList::zapTo_ChannelID(const t_channel_id channel_id)
{
	printf("**************************** CChannelList::zapTo_ChannelID %llx\n", channel_id);
	for (unsigned int i = 0; i < chanlist.size(); i++) {
		if (chanlist[i]->channel_id == channel_id) {
			zapTo(i);
			return true;
		}
	}
	return false;
}

bool CChannelList::showEmptyError()
{
	if (chanlist.empty()) {
		DisplayErrorMessage(g_Locale->getText(LOCALE_CHANNELLIST_NONEFOUND)); // UTF-8
		return true;
	}
	return false;
}

/* forceStoreToLastChannels defaults to false */
/* TODO make this private to call only from "current" list, where selected/pos not means channel number */
void CChannelList::zapTo(int pos, bool /* forceStoreToLastChannels */)
{
	if(showEmptyError())
		return;

	if ( (pos >= (signed int) chanlist.size()) || (pos < 0) ) {
		pos = 0;
	}
	CZapitChannel* chan = chanlist[pos];

	zapToChannel(chan);
	tuned = pos;
	if(this->new_mode_active)
		selected_in_new_mode = pos;
	else
		selected = pos;
}

/* to replace zapTo_ChannelID and zapTo from "whole" list ? */
void CChannelList::zapToChannel(CZapitChannel *channel)
{
	if(showEmptyError())
		return;

	if(channel == NULL)
		return;

	/* we record when we switched away from a channel, so that the parental-PIN code can
	   check for timeout. last_unlocked_time == 0 means: the PIN was not entered
	   "tuned" is the *old* channel, before zap */
	if (tuned < chanlist.size() && chanlist[tuned]->last_unlocked_time != 0)
		chanlist[tuned]->last_unlocked_time = time_monotonic();

	printf("**************************** CChannelList::zapToChannel me %p %s tuned %d new %s -> %llx\n", this, name.c_str(), tuned, channel->getName().c_str(), channel->channel_id);
	if(tuned < chanlist.size())
		selected_chid = chanlist[tuned]->getChannelID();

	if(selected_chid != channel->getChannelID()) {
		if ((g_settings.radiotext_enable) && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio) && (g_Radiotext))
		{
			// stop radiotext PES decoding before zapping
			g_Radiotext->radiotext_stop();
		}

		selected_chid = channel->getChannelID();
		g_RemoteControl->zapTo_ChannelID(channel->getChannelID(), channel->getName(), !channel->bAlwaysLocked);
		CNeutrinoApp::getInstance()->channelList->adjustToChannelID(channel->getChannelID());
	}
	if(!this->new_mode_active) {
		/* remove recordModeActive from infobar */
		if(g_settings.auto_timeshift && !CNeutrinoApp::getInstance()->recordingstatus) {
			g_InfoViewer->handleMsg(NeutrinoMessages::EVT_RECORDMODE, 0);
		}
		g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR, 0 );
		CNeutrinoApp::getInstance()->channelList->getLastChannels().set_mode(channel->channel_id);
	}
}

/* Called only from "all" channel list */
int CChannelList::numericZap(int key)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = -1;

	if(showEmptyError())
		return res;

	// -- quickzap "0" to last seen channel...
	if (key == g_settings.key_lastchannel) {
		t_channel_id channel_id = lastChList.getlast(1);
		if(channel_id && SameTP(channel_id)) {
			lastChList.clear_storedelay(); // ignore store delay
			int new_mode = lastChList.get_mode(channel_id);
			if(new_mode >= 0)
				CNeutrinoApp::getInstance()->SetChannelMode(new_mode);
			zapTo_ChannelID(channel_id);
			res = 0;
		}
		return res;
	}
	if ((key == g_settings.key_zaphistory) || (key == CRCInput::RC_games)) {
		if((!autoshift && CNeutrinoApp::getInstance()->recordingstatus) || (key == CRCInput::RC_games)) {
			CChannelList * orgList = CNeutrinoApp::getInstance()->channelList;
			CChannelList * channelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_CURRENT_TP), false, true);

			if(key == CRCInput::RC_games) {
				t_channel_id recid = chanlist[selected]->channel_id >> 16;
				for ( unsigned int i = 0 ; i < orgList->chanlist.size(); i++) {
					if((orgList->chanlist[i]->channel_id >> 16) == recid)
						channelList->addChannel(orgList->chanlist[i]);
				}
			} else {
				for ( unsigned int i = 0 ; i < orgList->chanlist.size(); i++) {
					if(SameTP(orgList->chanlist[i]))
						channelList->addChannel(orgList->chanlist[i]);
				}
			}
			if (channelList->getSize() != 0) {
				channelList->adjustToChannelID(orgList->getActiveChannel_ChannelID(), false);
				this->frameBuffer->paintBackground();
				res = channelList->exec();
			}
			delete channelList;
			return res;
		}
		// -- zap history bouquet, similar to "0" quickzap, but shows a menue of last channels
		if (this->lastChList.size() > 1) {
			CChannelList * channelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_HISTORY), true, true);

			for(unsigned int i = 1; i < this->lastChList.size() ; ++i) {
				t_channel_id channel_id = this->lastChList.getlast(i);
				if(channel_id) {
					CZapitChannel* channel = getChannel(channel_id);
					if(channel) channelList->addChannel(channel);
				}
			}
			if (channelList->getSize() != 0) {
				this->frameBuffer->paintBackground();
				res = channelList->exec();
			}
			delete channelList;
		}
		return res;
	}

	int sx = 4 * g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->getRenderWidth(widest_number) + 14;
	int sy = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->getHeight() + 6;

	int ox = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - sx)/2;
	int oy = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - sy)/2;
	char valstr[10];
	int chn = CRCInput::getNumericValue(key);
	int pos = 1;
	int lastchan= -1;
	bool doZap = false;
	bool showEPG = false;

	while(1) {
		if (lastchan != chn) {
			snprintf((char*) &valstr, sizeof(valstr), "%d", chn);
			while(strlen(valstr) < 4)
				strcat(valstr,"-");   //"_"

			frameBuffer->paintBoxRel(ox, oy, sx, sy, COL_INFOBAR_PLUS_0);

			for (int i = 3; i >= 0; i--) {
				valstr[i+ 1] = 0;
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->RenderString(ox+7+ i*((sx-14)>>2), oy+sy-3, sx, &valstr[i], COL_INFOBAR);
			}

			showInfo(chn);
			lastchan = chn;
		}

		g_RCInput->getMsg( &msg, &data, g_settings.timing[SNeutrinoSettings::TIMING_NUMERICZAP] * 10 );

		if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_ok) {
			doZap = true;
			break;
		}
		else if (msg == CRCInput::RC_favorites || msg == CRCInput::RC_sat || msg == CRCInput::RC_right) {
		}
		else if (CRCInput::isNumeric(msg)) {
			if (pos == 4) {
				chn = 0;
				pos = 1;
			} else {
				chn *= 10;
				pos++;
			}
			chn += CRCInput::getNumericValue(msg);
		}
		else if (msg == (neutrino_msg_t)g_settings.key_quickzap_down) {
			if(chn > 1)
				chn--;
		}
		else if (msg == (neutrino_msg_t)g_settings.key_quickzap_up) {
			chn++;
		}
		else if (msg == CRCInput::RC_left) {
			/* "backspace" */
			if(pos > 0) {
				pos--;
				if(chn > 10)
					chn /= 10;
			}
		}
		else if (msg == CRCInput::RC_home)
		{
			break;
		}
		else if (msg == CRCInput::RC_red) {
			showEPG = true;
			break;
		}
		else if (CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all) {
			break;
		}
	}

	frameBuffer->paintBackgroundBoxRel(ox, oy, sx, sy);

	CZapitChannel* chan = getChannel(chn);
	if (doZap) {
		if(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0)
			g_InfoViewer->killTitle();

		if(chan && SameTP(chan)) {
			zapToChannel(chan);
			res = 0;
		} else
			g_InfoViewer->killTitle();

	} else {
		g_InfoViewer->showTitle(getActiveChannel(), true);
		g_InfoViewer->killTitle();
		if (chan && showEPG)
			g_EventList->exec(chan->channel_id, chan->getName());
	}
	return res;
}

CZapitChannel* CChannelList::getPrevNextChannel(int key, unsigned int &sl)
{
	CZapitChannel* channel = chanlist[sl];
	int bsize = bouquetList->Bouquets.size();
	int bactive = bouquetList->getActiveBouquetNumber();

	if(!g_settings.zap_cycle && bsize > 1) {
		size_t cactive = sl;

		printf("CChannelList::getPrevNextChannel: selected %d total %d active bouquet %d total %d\n", cactive, chanlist.size(), bactive, bsize);
		if ((key == g_settings.key_quickzap_down) || (key == CRCInput::RC_left)) {
			if(cactive == 0) {
				if(bactive == 0)
					bactive = bsize - 1;
				else
					bactive--;
				bouquetList->activateBouquet(bactive, false);
				cactive = bouquetList->Bouquets[bactive]->channelList->getSize() - 1;
			} else 
				--cactive;
		}
		else if ((key == g_settings.key_quickzap_up) || (key == CRCInput::RC_right)) {
			cactive++;
			if(cactive >= chanlist.size()) {
				bactive = (bactive + 1)  % bsize;
				bouquetList->activateBouquet(bactive, false);
				cactive = 0;
			}
		}
		sl = cactive;
		channel = bouquetList->Bouquets[bactive]->channelList->getChannelFromIndex(cactive);
		printf("CChannelList::getPrevNextChannel: selected %d total %d active bouquet %d total %d channel %x (%s)\n",
				cactive, chanlist.size(), bactive, bsize, (int) channel, channel ? channel->getName().c_str(): "");
	} else {
		if ((key == g_settings.key_quickzap_down) || (key == CRCInput::RC_left)) {
			if(sl == 0)
				sl = chanlist.size()-1;
			else
				sl--;
		}
		else if ((key==g_settings.key_quickzap_up) || (key == CRCInput::RC_right)) {
			sl = (sl+1)%chanlist.size();
		}
		channel = chanlist[sl];
	}
	return channel;
}

void CChannelList::virtual_zap_mode(bool up)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	if(showEmptyError())
		return;

	unsigned int sl = selected;
	int old_bactive = bouquetList->getActiveBouquetNumber();
	int bactive = old_bactive;

	CZapitChannel* channel = getPrevNextChannel(up ? CRCInput::RC_right : CRCInput::RC_left, sl);

	bool doZap = false;
	bool showEPG = false;
	int epgpos = 0;

	while(1) {
		if (channel)
			g_InfoViewer->showTitle(channel, true, epgpos);

		epgpos = 0;
		g_RCInput->getMsg(&msg, &data, 15*10); // 15 seconds, not user changable

		if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right)) {
			channel = bouquetList->Bouquets[bactive]->channelList->getPrevNextChannel(msg, sl);
			bactive = bouquetList->getActiveBouquetNumber();
		}
		else if (msg == CRCInput::RC_up || msg == CRCInput::RC_down) {
			epgpos = (msg == CRCInput::RC_up) ? -1 : 1;
		}
		else if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) {
			if(msg == CRCInput::RC_ok)
				doZap = true;
			break;
		}
		else if (msg == CRCInput::RC_red) {
			showEPG = true;
			break;
		}
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all) {
			break;
		}
	}
	g_InfoViewer->clearVirtualZapMode();

	if (doZap) {
		if(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0)
			g_InfoViewer->killTitle();
		if(channel && SameTP(channel))
			zapToChannel(channel);
		else
			g_InfoViewer->killTitle();
	} else {
		g_InfoViewer->showTitle(getActiveChannel(), true);
		g_InfoViewer->killTitle();
		bouquetList->activateBouquet(old_bactive, false);

		if (showEPG && channel)
			g_EventList->exec(channel->channel_id, channel->getName());
	}
}

void CChannelList::quickZap(int key, bool /* cycle */)
{
	if(chanlist.empty())
		return;

	unsigned int sl = selected;
	/* here selected value doesnt matter, zap will do adjust */
	CZapitChannel* channel = getPrevNextChannel(key, sl);
	if(channel)
		CNeutrinoApp::getInstance()->channelList->zapToChannel(channel);
	g_RCInput->clearRCMsg(); //FIXME test for n.103
}

void CChannelList::paintDetails(int index)
{
	CChannelEvent *p_event = NULL;

	//colored_events init
	bool colored_event_C = false;
	bool colored_event_N = false;
	if (g_settings.colored_events_channellist == 1)
		colored_event_C = true;
	if (g_settings.colored_events_channellist == 2)
		colored_event_N = true;

	if (displayNext) {
		p_event = &chanlist[index]->nextEvent;
	} else {
		p_event = &chanlist[index]->currentEvent;
	}

	frameBuffer->paintBoxRel(x+2, y + height + 2, width-4, info_height - 4, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE);//round

	if (!p_event->description.empty()) {
		char cNoch[50] = {0}; // UTF-8
		char cSeit[50] = {0}; // UTF-8

		struct		tm *pStartZeit = localtime(&p_event->startTime);
		unsigned 	seit = ( time(NULL) - p_event->startTime ) / 60;
		snprintf(cSeit, sizeof(cSeit), "%s %02d:%02d",(displayNext) ? g_Locale->getText(LOCALE_CHANNELLIST_START):g_Locale->getText(LOCALE_CHANNELLIST_SINCE), pStartZeit->tm_hour, pStartZeit->tm_min);
		if (displayNext) {
			snprintf(cNoch, sizeof(cNoch), "(%d min)", p_event->duration / 60);
		} else {
			int noch = (p_event->startTime + p_event->duration - time(NULL)) / 60;
			if ((noch< 0) || (noch>=10000))
				noch= 0;
			snprintf(cNoch, sizeof(cNoch), "(%d / %d min)", seit, noch);
		}
		int seit_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cSeit, true); // UTF-8
		int noch_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cNoch, true); // UTF-8

		std::string text1= p_event->description;
		std::string text2= p_event->text;

		int xstart = 10;
		if (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1, true) > (width - 30 - seit_len) )
		{
			// zu breit, Umbruch versuchen...
			int pos;
			do {
				pos = text1.find_last_of("[ -.]+");
				if ( pos!=-1 )
					text1 = text1.substr( 0, pos );
			} while ( ( pos != -1 ) && (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1, true) > (width - 30 - seit_len) ) );

			std::string text3 = ""; /* not perfect, but better than crashing... */
			if (p_event->description.length() > text1.length())
				text3 = p_event->description.substr(text1.length()+ 1);

			if (!text2.empty() && !text3.empty())
				text3= text3+ " - ";

			xstart += g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text3, true);
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 10, y+ height+ 5+ 2* fheight, width - 30- noch_len, text3, colored_event_C ? COL_COLORED_EVENTS_CHANNELLIST : COL_MENUCONTENTDARK, 0, true);
		}

		if (!(text2.empty())) {
			while ( text2.find_first_of("[ -.+*#?=!$%&/]+") == 0 )
				text2 = text2.substr( 1 );
			text2 = text2.substr( 0, text2.find('\n') );
#if 0 //FIXME: to discuss, eat too much cpu time if string long enough
			int pos = 0;
			while ( ( pos != -1 ) && (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text2, true) > (width - 30 - noch_len) ) ) {
				pos = text2.find_last_of(" ");

				if ( pos!=-1 ) {
					text2 = text2.substr( 0, pos );
				}
			}
#endif
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ xstart, y+ height+ 5+ 2* fheight, width- xstart- 30- noch_len, text2, colored_event_C ? COL_COLORED_EVENTS_CHANNELLIST : COL_MENUCONTENTDARK, 0, true);
		}

		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 10, y+ height+ 5+ fheight, width - 30 - seit_len, text1, colored_event_C ? COL_COLORED_EVENTS_CHANNELLIST : COL_MENUCONTENTDARK, 0, true);
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ width- 10- seit_len, y+ height+ 5+    fheight, seit_len, cSeit, colored_event_C ? COL_COLORED_EVENTS_CHANNELLIST : COL_MENUCONTENTDARK, 0, true); // UTF-8
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ width- 10- noch_len, y+ height+ 5+ 2* fheight, noch_len, cNoch, colored_event_C ? COL_COLORED_EVENTS_CHANNELLIST : COL_MENUCONTENTDARK, 0, true); // UTF-8
	}
	if(g_settings.channellist_foot == 0) {
		transponder t;
		CServiceManager::getInstance()->GetTransponder(chanlist[index]->getTransponderId(), t);

		std::string desc = t.description();
		if(chanlist[index]->pname)
			desc = desc + " (" + std::string(chanlist[index]->pname) + ")";
		else
			desc = desc + " (" + CServiceManager::getInstance()->GetSatelliteName(chanlist[index]->getSatellitePosition()) + ")";

		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 10, y+ height+ 5+ 3*fheight, width - 30, desc.c_str(), COL_MENUCONTENTDARK, 0, true);
	}
	else if( !displayNext && g_settings.channellist_foot == 1) { // next Event
		char buf[128] = {0};
		char cFrom[50] = {0}; // UTF-8
		CSectionsdClient::CurrentNextInfo CurrentNext;
		sectionsd_getCurrentNextServiceKey(chanlist[index]->channel_id & 0xFFFFFFFFFFFFULL, CurrentNext);
		if (!CurrentNext.next_name.empty()) {
			struct tm *pStartZeit = localtime (& CurrentNext.next_zeit.startzeit);
			snprintf(cFrom, sizeof(cFrom), "%s %02d:%02d",g_Locale->getText(LOCALE_WORD_FROM),pStartZeit->tm_hour, pStartZeit->tm_min );
			snprintf(buf, sizeof(buf), "%s", CurrentNext.next_name.c_str());
			int from_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cFrom, true); // UTF-8

			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 10, y+ height+ 5+ 3*fheight, width - 30 - from_len, buf, colored_event_N ? COL_COLORED_EVENTS_CHANNELLIST :COL_MENUCONTENTDARK, 0, true);
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ width- 10- from_len, y+ height+ 5+ 3*fheight, from_len, cFrom, colored_event_N ? COL_COLORED_EVENTS_CHANNELLIST : COL_MENUCONTENTDARK, 0, true); // UTF-8
		}
	}
}

void CChannelList::clearItem2DetailsLine()
{
	paintItem2DetailsLine (-1, 0);
}

void CChannelList::paintItem2DetailsLine (int pos, int /*ch_index*/)
{
#define ConnectLineBox_Width	16

	int xpos  = x - ConnectLineBox_Width;
	int ypos1 = y + theight+0 + pos*fheight;
	int ypos2 = y + height;
	int ypos1a = ypos1 + (fheight/2)-2;
	int ypos2a = ypos2 + (info_height/2)-2;
	fb_pixel_t col1 = COL_MENUCONTENT_PLUS_6;
	fb_pixel_t col2 = COL_MENUCONTENT_PLUS_1;

	// Clear
	frameBuffer->paintBackgroundBoxRel(xpos,y, ConnectLineBox_Width, height+info_height);

	// paint Line if detail info (and not valid list pos)
	if (pos >= 0) { //pos >= 0 &&  chanlist[ch_index]->currentEvent.description != "") {
		if(1) // FIXME why -> ? (!g_settings.channellist_extended)
		{
			int fh = fheight > 10 ? fheight - 10: 5;
			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos1+5, 4, fh,     col1);
			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos1+5, 1, fh,     col2);

			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos2+7, 4,info_height-14, col1);
			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-4, ypos2+7, 1,info_height-14, col2);

			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 4,ypos2a-ypos1a, col1);
			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 1,ypos2a-ypos1a+4, col2);

			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 12,4, col1);
			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos1a, 12,1, col2);

			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-16, ypos2a, 12,4, col1);
			frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width-12, ypos2a, 8,1, col2);

//			frameBuffer->paintBoxRel(x, ypos2, width, info_height, col1, RADIUS_LARGE);
			frameBuffer->paintBoxFrame(x, ypos2, width, info_height, 2, col1, RADIUS_LARGE);

		}
	}
}

void CChannelList::showChannelLogo()
{
	if(g_settings.infobar_show_channellogo){
		static int logo_w = 0;
		static int logo_h = 0;
		int logo_w_max = width / 4;
		frameBuffer->paintBoxRel(x + width - logo_off - logo_w, y+(theight-logo_h)/2, logo_w, logo_h, COL_MENUHEAD_PLUS_0);

		std::string lname;
		if(g_PicViewer->GetLogoName(chanlist[selected]->channel_id, chanlist[selected]->getName(), lname, &logo_w, &logo_h)) {
			if((logo_h > theight) || (logo_w > logo_w_max))
				g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, logo_w_max, theight);
			g_PicViewer->DisplayImage(lname, x + width - logo_off - logo_w, y+(theight-logo_h)/2, logo_w, logo_h);
		}
	}
}

#define NUM_LIST_BUTTONS 4
struct button_label SChannelListButtons[NUM_LIST_BUTTONS] =
{
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_INFOVIEWER_EVENTLIST},
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETLIST_HEAD},
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_INFOVIEWER_NEXT},
	{ NEUTRINO_ICON_BUTTON_RECORD_INACTIVE, NONEXISTANT_LOCALE}
};
#define NUM_LIST_BUTTONS_SORT 5
struct button_label SChannelListButtons_SMode[NUM_LIST_BUTTONS_SORT] =
{
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_INFOVIEWER_EVENTLIST},
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_CHANNELLIST_FOOT_SORT_ALPHA},
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETLIST_HEAD},
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_INFOVIEWER_NEXT},
	{ NEUTRINO_ICON_BUTTON_RECORD_INACTIVE, NONEXISTANT_LOCALE}
};

void CChannelList::paintButtonBar(bool is_current)
{
	//printf("[neutrino channellist] %s...%d, selected %d\n", __FUNCTION__, __LINE__, selected);
	unsigned int smode = CNeutrinoApp::getInstance()->GetChannelMode();
	int num_buttons = smode ? NUM_LIST_BUTTONS_SORT : NUM_LIST_BUTTONS;

	struct button_label Button[num_buttons];
	const neutrino_locale_t button_ids[] = {LOCALE_INFOVIEWER_NOW,LOCALE_INFOVIEWER_NEXT,LOCALE_MAINMENU_RECORDING,LOCALE_MAINMENU_RECORDING_STOP,NONEXISTANT_LOCALE,
						LOCALE_CHANNELLIST_FOOT_SORT_ALPHA,LOCALE_CHANNELLIST_FOOT_SORT_FREQ,LOCALE_CHANNELLIST_FOOT_SORT_SAT};
	const std::vector<neutrino_locale_t> buttonID_rest (button_ids, button_ids + sizeof(button_ids) / sizeof(neutrino_locale_t) );

	for (int i = 0;i<num_buttons;i++)
	{
		if(smode)
			Button[i] = SChannelListButtons_SMode[i];
		else
			Button[i] = SChannelListButtons[i];
	}

	int Bindex = 2 + (smode ? 1:0);

	//manage now/next button
	if (displayNext)
		Button[Bindex].locale = LOCALE_INFOVIEWER_NOW;
	else
		Button[Bindex].locale = LOCALE_INFOVIEWER_NEXT;

	Bindex++;
	//manage record button
	bool do_record = CRecordManager::getInstance()->RecordingStatus(getActiveChannel_ChannelID());
	
	if (g_settings.recording_type != RECORDING_OFF && !displayNext){
		if (is_current && !do_record){
			Button[Bindex].locale = LOCALE_MAINMENU_RECORDING;
			Button[Bindex].button = NEUTRINO_ICON_BUTTON_RECORD_ACTIVE;
		}else if (do_record){
			Button[Bindex].locale = LOCALE_MAINMENU_RECORDING_STOP;
			Button[Bindex].button = NEUTRINO_ICON_BUTTON_STOP;
		}else{
			Button[Bindex].locale = NONEXISTANT_LOCALE;
			Button[Bindex].button = NEUTRINO_ICON_BUTTON_RECORD_INACTIVE;
		}
	}
	if(smode)
	{
		switch (g_settings.channellist_sort_mode)
		{
			case 0:
				Button[1].locale = LOCALE_CHANNELLIST_FOOT_SORT_ALPHA;
			break;
			case 1:
				Button[1].locale = LOCALE_CHANNELLIST_FOOT_SORT_FREQ;
			break;
			case 2:
				Button[1].locale = LOCALE_CHANNELLIST_FOOT_SORT_SAT;
			break;
		}
	}

	//paint buttons
	int y_foot = y + (height - footerHeight);
	::paintButtons(x, y_foot, width,num_buttons, Button, footerHeight,0,false,COL_INFOBAR_SHADOW,NULL,0,true, buttonID_rest);
}

void CChannelList::paintItem(int pos)
{
	int ypos = y+ theight+0 + pos*fheight;
	uint8_t    color;
	fb_pixel_t bgcolor;
	bool iscurrent = true;
	bool paintbuttons = false;
	unsigned int curr = liststart + pos;
	int rec_mode;

#if 0
	if(CNeutrinoApp::getInstance()->recordingstatus && !autoshift && curr < chanlist.size()) {
		iscurrent = (chanlist[curr]->channel_id >> 16) == (rec_channel_id >> 16);
		//printf("recording %llx current %llx current = %s\n", rec_channel_id, chanlist[liststart + pos]->channel_id, iscurrent? "yes" : "no");
	}
#endif
	if(curr < chanlist.size())
		iscurrent = SameTP(chanlist[curr]);

	if (curr == selected) {
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		paintItem2DetailsLine (pos, curr);
		paintDetails(curr);
		frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, bgcolor, RADIUS_LARGE);
		paintbuttons = true;
	} else {
		color = iscurrent ? COL_MENUCONTENT : COL_MENUCONTENTINACTIVE;
		bgcolor = iscurrent ? COL_MENUCONTENT_PLUS_0 : COL_MENUCONTENTINACTIVE_PLUS_0;
		frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, bgcolor, 0);
	}

	if(curr < chanlist.size()) {
		char nameAndDescription[255];
		char tmp[10];
		CZapitChannel* chan = chanlist[curr];
		int prg_offset=0;
		int title_offset=0;
		uint8_t tcolor=(liststart + pos == selected) ? color : COL_MENUCONTENTINACTIVE;
		int xtheight=fheight-2;

		if(g_settings.channellist_extended)
		{
			prg_offset = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("00:00");
			title_offset=6;
		}

		snprintf((char*) tmp, sizeof(tmp), "%d", this->historyMode ? pos : chan->number);

		CChannelEvent *p_event=NULL;
		if (displayNext) {
			p_event = &chan->nextEvent;
		} else {
			p_event = &chan->currentEvent;
		}

		if (pos == 0)
		{
			int w_max, w_min, h;
			ChannelList_Rec = 0;
			int recmode_icon_max = CRecordManager::RECMODE_REC, recmode_icon_min = CRecordManager::RECMODE_TSHIFT;
			frameBuffer->getIconSize(NEUTRINO_ICON_REC, &w_max, &h);
			frameBuffer->getIconSize(NEUTRINO_ICON_AUTO_SHIFT, &w_min, &h);
			if (w_max < w_min)
			{
				recmode_icon_max = CRecordManager::RECMODE_TSHIFT;
				recmode_icon_min = CRecordManager::RECMODE_REC;
				h = w_max;
				w_max = w_min;
				w_min = h;
			}
			for (uint32_t i = 0; i < chanlist.size(); i++)
			{
				rec_mode = CRecordManager::getInstance()->GetRecordMode(chanlist[i]->channel_id);
				if (rec_mode == recmode_icon_max)
				{
					ChannelList_Rec = w_max;
					break;
				} else if (rec_mode == recmode_icon_min)
					ChannelList_Rec = w_min;
			}
			if (ChannelList_Rec > 0)
				ChannelList_Rec += 8;
		}

		//record check
		rec_mode = CRecordManager::getInstance()->GetRecordMode(chanlist[curr]->channel_id);
		
		//set recording icon
		const char * rec_icon = "";
		if (rec_mode == CRecordManager::RECMODE_REC)
			rec_icon = NEUTRINO_ICON_REC;
		else if (rec_mode == CRecordManager::RECMODE_TSHIFT)
			rec_icon = NEUTRINO_ICON_AUTO_SHIFT;
	
		//calculating icons
		int  icon_x = (x+width-15-2) - RADIUS_LARGE/2;
		int r_icon_w=0;  int s_icon_h=0; int s_icon_w=0;
		frameBuffer->getIconSize(NEUTRINO_ICON_SCRAMBLED, &s_icon_w, &s_icon_h);
		r_icon_w = ChannelList_Rec;
		int r_icon_x = icon_x;
		
		//paint scramble icon
		if(chan->scrambled)
			if (frameBuffer->paintIcon(NEUTRINO_ICON_SCRAMBLED, icon_x - s_icon_w, ypos, fheight))//ypos + (fheight - 16)/2);
				r_icon_x = r_icon_x - s_icon_w;
		
 		//paint recording icon
 		//bool do_rec = CRecordManager::getInstance()->RecordingStatus(chanlist[curr]->channel_id);
		if (rec_mode != CRecordManager::RECMODE_OFF)
			frameBuffer->paintIcon(rec_icon, r_icon_x - r_icon_w, ypos, fheight);//ypos + (fheight - 16)/2);
		
		//paint buttons
		if (paintbuttons)
			paintButtonBar(iscurrent);
		
		int icon_space = r_icon_w+s_icon_w;

		//number
		int numpos = x+5+numwidth- g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth(tmp);
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(numpos,ypos+fheight, numwidth+5, tmp, color, fheight);

		int l=0;
		if (this->historyMode)
			l = snprintf(nameAndDescription, sizeof(nameAndDescription), ": %d %s", chan->number, chan->getName().c_str());
		else
			l = snprintf(nameAndDescription, sizeof(nameAndDescription), "%s", chan->getName().c_str());

		CProgressBar pb(false); /* never colored */
		int pb_space = prg_offset - title_offset;
		int pb_max = pb_space - 4;
		if (!(p_event->description.empty())) {
			snprintf(nameAndDescription+l, sizeof(nameAndDescription)-l,g_settings.channellist_epgtext_align_right ? "  ":" - ");
			unsigned int ch_name_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(nameAndDescription, true);
			unsigned int ch_desc_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(p_event->description, true);

			int max_desc_len = width - numwidth - prg_offset - ch_name_len - 15 - 20; // 15 = scrollbar, 20 = spaces
			if (chan->scrambled || (g_settings.channellist_extended ||g_settings.channellist_epgtext_align_right))
				max_desc_len -= icon_space; /* do we need space for the lock/rec icon? */
				
			if (max_desc_len < 0)
				max_desc_len = 0;
			if ((int) ch_desc_len > max_desc_len)
				ch_desc_len = max_desc_len;

			if(g_settings.channellist_extended) {
				if(displayNext)
				{
					struct		tm *pStartZeit = localtime(&p_event->startTime);

					snprintf((char*) tmp, sizeof(tmp), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
//					g_Font[SNeutrinoSettings::FONT_TYPE_IMAGEINFO_SMALL]->RenderString(x+ 5+ numwidth+ 6, ypos+ xtheight, width- numwidth- 20- 15 -poffs, tmp, COL_MENUCONTENT, 0, true);
					g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x+ 5+ numwidth+ 6, ypos+ xtheight, width- numwidth- 20- 15 -prg_offset, tmp, tcolor, 0, true);
				}
				else
				{
					time_t jetzt=time(NULL);
					int runningPercent = 0;

					if (((jetzt - p_event->startTime + 30) / 60) < 0 )
					{
						runningPercent= 0;
					}
					else
					{
						runningPercent=(jetzt-p_event->startTime) * pb_max / p_event->duration;
						if (runningPercent > pb_max)	// this would lead to negative value in paintBoxRel
							runningPercent = pb_max;	// later on which can be fatal...
					}

					int pb_activeCol , pb_passiveCol;
					if (liststart + pos != selected) {
						pb_activeCol = COL_MENUCONTENT_PLUS_3;
						pb_passiveCol = COL_MENUCONTENT_PLUS_1;
					} else {
						pb_activeCol = COL_MENUCONTENTSELECTED_PLUS_2;
						pb_passiveCol = COL_MENUCONTENTSELECTED_PLUS_0;
					}
					pb.paintProgressBar(x+5+numwidth + title_offset, ypos + fheight/4, pb_space + 2, fheight/2, runningPercent, pb_max, pb_activeCol, pb_passiveCol, pb_activeCol);
				}
			}

			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 5+ numwidth+ 10+prg_offset, ypos+ fheight, width- numwidth- 40- 15-prg_offset, nameAndDescription, color, 0, true);
			if (g_settings.channellist_epgtext_align_right) {
				// align right
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x + width - 20 - ch_desc_len - icon_space - 4, ypos + fheight, ch_desc_len, p_event->description, (curr == selected)?COL_MENUCONTENTSELECTED:(!displayNext ? COL_MENUCONTENT : COL_MENUCONTENTINACTIVE) , 0, true);
			}
			else {
				// align left
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ 5+ numwidth+ 10+ ch_name_len+ 5+prg_offset, ypos+ fheight, ch_desc_len, p_event->description, (curr == selected)?COL_MENUCONTENTSELECTED:(!displayNext ? COL_MENUCONTENT : COL_MENUCONTENTINACTIVE) , 0, true);
			}
		}
		else {
			if(g_settings.channellist_extended) {
				int pbz_activeCol, pbz_passiveCol;
				if (liststart + pos != selected) {
					pbz_activeCol = COL_MENUCONTENT_PLUS_1;
					pbz_passiveCol = COL_MENUCONTENT_PLUS_0;
				} else {
					pbz_activeCol = COL_MENUCONTENTSELECTED_PLUS_2;
					pbz_passiveCol = COL_MENUCONTENTSELECTED_PLUS_0;
				}
				pb.paintProgressBar(x+5+numwidth + title_offset, ypos + fheight/4, pb_space + 2, fheight/2, 0, pb_max, pbz_activeCol, pbz_passiveCol, pbz_activeCol, 0, NULL, 0, NULL, true);
			}
			//name
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 5+ numwidth+ 10+prg_offset, ypos+ fheight, width- numwidth- 40- 15-prg_offset, nameAndDescription, color, 0, true); // UTF-8
		}
		if (curr == selected) {
			if (!(chan->currentEvent.description.empty())) {
				snprintf(nameAndDescription, sizeof(nameAndDescription), "%s - %s",
					 chan->getName().c_str(), p_event->description.c_str());
				CVFD::getInstance()->showMenuText(0, nameAndDescription, -1, true); // UTF-8
			} else
				CVFD::getInstance()->showMenuText(0, chan->getName().c_str(), -1, true); // UTF-8
		}
	}
}

void CChannelList::paintHead()
{
	int timestr_len = 0;
	char timestr[10] = {0};
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	bool gotTime = g_Sectionsd->getIsTimeSet();

	if(gotTime) {
		strftime(timestr, 10, "%H:%M", tm);
		timestr_len = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(timestr, true); // UTF-8
	}

	int iw1, iw2, iw3, ih = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_INFO, &iw1, &ih);
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MENU, &iw2, &ih);
	if (g_settings.channellist_new_zap_mode)
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE, &iw3, &ih);

	// head
	frameBuffer->paintBoxRel(x,y, width,theight+0, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);//round

	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_INFO, x + width - iw1 - 4, y, theight); //y+ 5 );
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_MENU, x + width - iw1 - iw2 - 8, y, theight);//y + 5); // icon for bouquet list button
	if (g_settings.channellist_new_zap_mode)
		frameBuffer->paintIcon(this->new_mode_active ?
				       NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE : NEUTRINO_ICON_BUTTON_MUTE_ZAP_INACTIVE,
				       x + width - iw1 - iw2 - iw3 - 12, y, theight);

	if (gotTime) {
		int iw3x = (g_settings.channellist_new_zap_mode) ? iw3 : -4;
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x + width - iw1 - iw2 - iw3x - 16 -timestr_len,
				y+theight, timestr_len, timestr, COL_MENUHEAD, 0, true); // UTF-8
		timestr_len += 4;
	}

	timestr_len += iw1 + iw2 + 12;
	if (g_settings.channellist_new_zap_mode)
		timestr_len += iw3 + 4;
	logo_off = timestr_len + 4;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+10,y+theight+0, width - timestr_len, name, COL_MENUHEAD, 0, true); // UTF-8
}

void CChannelList::paint()
{
	zapit_list_it_t chan_it;
	std::stringstream ss;
	std::string chan_width;
	int chan_nr_max = 1;
	unsigned int nr = 0;
	for (chan_it=chanlist.begin(); chan_it!=chanlist.end(); ++chan_it) {
		chan_nr_max = std::max(chan_nr_max, chanlist[nr++]->number);
	}
	ss << chan_nr_max;
	ss >> chan_width;
	numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth(chan_width.c_str());

	liststart = (selected/listmaxshow)*listmaxshow;
	updateEvents(this->historyMode ? 0:liststart, this->historyMode ? 0:(liststart + listmaxshow));

	frameBuffer->paintBoxRel(x, y+theight, width, height-footerHeight-theight, COL_MENUCONTENT_PLUS_0, 0, CORNER_BOTTOM);
	for(unsigned int count = 0; count < listmaxshow; count++) {
		paintItem(count);
	}
	const int ypos = y+ theight;
	const int sb = fheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	const int sbc= ((chanlist.size()- 1)/ listmaxshow)+ 1;
	const int sbs= (selected/listmaxshow);

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs*(sb-4)/sbc, 11, (sb-4)/sbc, COL_MENUCONTENT_PLUS_3);
	showChannelLogo();
}

int CChannelList::getSize() const
{
	return this->chanlist.size();
}

int CChannelList::getSelectedChannelIndex() const
{
	return this->selected;
}

bool CChannelList::SameTP(t_channel_id channel_id)
{
	bool iscurrent = true;

#if 0
	if(CNeutrinoApp::getInstance()->recordingstatus && !autoshift)
		iscurrent = (channel_id >> 16) == (rec_channel_id >> 16);
#endif
	if(CNeutrinoApp::getInstance()->recordingstatus) {
#if 0
		if(channel_id == 0)
			channel_id = chanlist[selected]->channel_id;
		iscurrent = CRecordManager::getInstance()->SameTransponder(channel_id);
#endif
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
		if(channel)
			iscurrent = SameTP(channel);
		else
			iscurrent = false;
	}
	return iscurrent;
}

bool CChannelList::SameTP(CZapitChannel * channel)
{
	bool iscurrent = true;

	if(CNeutrinoApp::getInstance()->recordingstatus) {
		if(channel == NULL)
			channel = chanlist[selected];
		iscurrent = CFEManager::getInstance()->canTune(channel);
	}
	return iscurrent;
}
