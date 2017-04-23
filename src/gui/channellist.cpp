/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2007-2016 Stefan Seyfried

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
	along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sstream>

#include <gui/channellist.h>

#include <unistd.h>
#include <global.h>
#include <neutrino.h>

#include <driver/screen_max.h>
#include <driver/abstime.h>
#include <driver/record.h>
#include <driver/fade.h>
#include <driver/display.h>
#include <driver/radiotext.h>
#include <driver/fontrenderer.h>

#include <gui/color.h>
#include <gui/color_custom.h>
#include <gui/epgview.h>
#include <gui/eventlist.h>
#include <gui/infoviewer.h>
#include <gui/osd_setup.h>
#include <gui/components/cc.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/movieplayer.h>
#include <gui/infoclock.h>
#if 0
#include <gui/widget/messagebox.h>
#include <gui/widget/hintbox.h>
#else
#include <gui/widget/msgbox.h>
#endif
#include <system/helpers.h>
#include <system/settings.h>
#include <system/set_threadname.h>

#include <gui/bouquetlist.h>
#include <daemonc/remotecontrol.h>
#include <zapit/client/zapittools.h>
#include <gui/pictureviewer.h>
#include <gui/bedit/bouqueteditor_chanselect.h>

#include <zapit/zapit.h>
#include <zapit/satconfig.h>
#include <zapit/getservices.h>
#include <zapit/femanager.h>
#include <zapit/debug.h>

#include <eitd/sectionsd.h>

extern CBouquetList * bouquetList;       /* neutrino.cpp */
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */
extern CBouquetList   * AllFavBouquetList;
extern CBouquetList   * TVfavList;
extern CBouquetList   * RADIOfavList;

extern bool autoshift;
static CComponentsPIP	*cc_minitv = NULL;
extern CBouquetManager *g_bouquetManager;
extern int old_b_id;
static CComponentsHeader *header = NULL;
extern bool timeset;

CChannelList::CChannelList(const char * const pName, bool phistoryMode, bool _vlist)
{
	frameBuffer = CFrameBuffer::getInstance();
	x = y = 0;
	info_height = 0;
	name = pName;
	selected = 0;
	selected_in_new_mode = 0;
	liststart = 0;
	tuned = 0xfffffff;
	zapProtection = NULL;
	this->historyMode = phistoryMode;
	vlist = _vlist;
	new_zap_mode = 0;
	selected_chid = 0;
	footerHeight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]->getHeight()+6; //initial height value for buttonbar
	theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	fdescrheight = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getHeight();

	previous_channellist_additional = -1;
	eventFont = SNeutrinoSettings::FONT_TYPE_CHANNELLIST_EVENT;
	dline = NULL;
	cc_minitv = NULL;
	logo_off = 0;
	minitv_is_active = false;
	headerNew = true;
	bouquet = NULL;
	chanlist = &channels;
	move_state = beDefault;
	edit_state = false;
	channelsChanged = false;

	paint_events_index = -2;
}

CChannelList::~CChannelList()
{
	ResetModules();
}

void CChannelList::SetChannelList(ZapitChannelList* zlist)
{
	channels = *zlist;
}

void CChannelList::addChannel(CZapitChannel* channel)
{
	(*chanlist).push_back(channel);
}

/* update the events for the visible channel list entries
   from = start entry, to = end entry. If both = zero, update all */
void CChannelList::updateEvents(unsigned int from, unsigned int to)
{
	if ((*chanlist).empty())
		return;

	if (to == 0 || to > (*chanlist).size())
		to = (*chanlist).size();

	if (from > (*chanlist).size() || from >= to)
		return;

	size_t chanlist_size = to - from;
	if (chanlist_size <= 0) // WTF???
		return;

	CChannelEventList events;
	if (displayNext) {
		time_t atime = time(NULL);
		unsigned int count;
		for (count = from; count < to; count++) {
			CEitManager::getInstance()->getEventsServiceKey((*chanlist)[count]->getEpgID(), events);
			(*chanlist)[count]->nextEvent.startTime = (long)0x7fffffff;
			for ( CChannelEventList::iterator e= events.begin(); e != events.end(); ++e ) {
				if ((long)e->startTime > atime &&
						(e->startTime < (long)(*chanlist)[count]->nextEvent.startTime))
				{
					(*chanlist)[count]->nextEvent = *e;
					break;
				}
			}
		}
	} else {
		t_channel_id *p_requested_channels;
		p_requested_channels = new t_channel_id[chanlist_size];
		if (! p_requested_channels) {
			fprintf(stderr,"%s:%d allocation failed!\n", __FUNCTION__, __LINE__);
			return;
		}
		for (uint32_t count = 0; count < chanlist_size; count++)
			p_requested_channels[count] = (*chanlist)[count + from]->getEpgID();

		CChannelEventList levents;
		CEitManager::getInstance()->getChannelEvents(levents, p_requested_channels, chanlist_size);
		for (uint32_t count=0; count < chanlist_size; count++) {
			(*chanlist)[count + from]->currentEvent = CChannelEvent();
			for (CChannelEventList::iterator e = levents.begin(); e != levents.end(); ++e) {
				if (((*chanlist)[count + from]->getEpgID()&0xFFFFFFFFFFFFULL) == e->get_channel_id()) {
					(*chanlist)[count + from]->currentEvent = *e;
					break;
				}
			}
		}
		levents.clear();
		delete[] p_requested_channels;
	}
}

void CChannelList::SortAlpha(void)
{
	sort((*chanlist).begin(), (*chanlist).end(), CmpChannelByChName());
}

void CChannelList::SortSat(void)
{
	sort((*chanlist).begin(), (*chanlist).end(), CmpChannelBySat());
}

void CChannelList::SortTP(void)
{
	sort((*chanlist).begin(), (*chanlist).end(), CmpChannelByFreq());
}

void CChannelList::SortChNumber(void)
{
	sort((*chanlist).begin(), (*chanlist).end(), CmpChannelByChNum());
}

CZapitChannel* CChannelList::getChannel(int number)
{
	for (uint32_t i=0; i< (*chanlist).size(); i++) {
		if ((*chanlist)[i]->number == number)
			return (*chanlist)[i];
	}
	return(NULL);
}

CZapitChannel* CChannelList::getChannel(t_channel_id channel_id)
{
	for (uint32_t i=0; i< (*chanlist).size(); i++) {
		if ((*chanlist)[i]->getChannelID() == channel_id)
			return (*chanlist)[i];
	}
	return(NULL);
}

int CChannelList::getKey(int id)
{
	if (id > -1 && id < (int)(*chanlist).size())
		return (*chanlist)[id]->number;
	return 0;
}

const std::string CChannelList::getActiveChannelName(void) const
{
	static const std::string empty_string;
	if (selected < (*chanlist).size())
		return (*chanlist)[selected]->getName();
	return empty_string;
}

t_satellite_position CChannelList::getActiveSatellitePosition(void) const
{
	if (selected < (*chanlist).size())
		return (*chanlist)[selected]->getSatellitePosition();
	return 0;
}

t_channel_id CChannelList::getActiveChannel_ChannelID(void) const
{
	if (selected < (*chanlist).size())
		return (*chanlist)[selected]->getChannelID();
	return 0;
}

int CChannelList::getActiveChannelNumber(void) const
{
	if (selected < (*chanlist).size())
		return (*chanlist)[selected]->number;
	return 0;
}

CZapitChannel * CChannelList::getActiveChannel(void) const
{
	static CZapitChannel channel("Channel not found", 0, 0, 0, 0);
	if (selected < (*chanlist).size())
		return (*chanlist)[selected];
	return &channel;
}

int CChannelList::doChannelMenu(void)
{
	int select = -1;
	int shortcut = 0;
	static int old_selected = 0;
	char cnt[5];
	bool unlocked = true;
	int ret = 0;

	if(g_settings.minimode)
		return 0;

	CMenuWidget* menu = new CMenuWidget(LOCALE_CHANNELLIST_EDIT, NEUTRINO_ICON_SETTINGS);
	menu->enableFade(false);
	menu->enableSaveScreen(true);

	//ensure stop info clock before paint context menu
	CInfoClock::getInstance()->block();

	//ensure stop header clock before paint context menu
	if (g_settings.menu_pos == CMenuWidget::MENU_POS_TOP_RIGHT){
		//using native callback to ensure stop header clock before paint this menu window
		menu->OnBeforePaint.connect(sigc::mem_fun(header->getClockObject(), &CComponentsFrmClock::block));
		//... and start header clock after hide menu window
		menu->OnAfterHide.connect(sigc::mem_fun(header->getClockObject(), &CComponentsFrmClock::unblock));
	}

	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	bool empty = (*chanlist).empty();
	bool allow_edit = (bouquet && bouquet->zapitBouquet && !bouquet->zapitBouquet->bOther && !bouquet->zapitBouquet->bWebtv);
	bool got_history = (CNeutrinoApp::getInstance()->channelList->getLastChannels().size() > 1);

	int i = 0;
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_NAME, allow_edit, NULL, selector, cnt, CRCInput::RC_red), old_selected == i++);
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ADD_TO_BOUQUET, !empty, NULL, selector, cnt, CRCInput::RC_green), old_selected == i++);
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_FAVORITES_MENUEADD, !empty, NULL, selector, cnt, CRCInput::RC_yellow), old_selected == i++);

	bool reset_enabled = empty ? false : (*chanlist)[selected]->flags & CZapitChannel::NEW;
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_CHANNELLIST_RESET_FLAGS, reset_enabled, NULL, selector, cnt, CRCInput::convertDigitToKey(shortcut++)), old_selected == i++);

	bool reset_all = !empty && (name == g_Locale->getText(LOCALE_BOUQUETNAME_NEW));
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_CHANNELLIST_RESET_ALL, reset_all, NULL, selector, cnt, CRCInput::convertDigitToKey(shortcut++)), old_selected == i++);

	menu->addItem(GenericMenuSeparator);
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_CHANNELLIST_HISTORY_CLEAR, got_history, NULL, selector, cnt, CRCInput::convertDigitToKey(shortcut++)), old_selected == i++);

	menu->addItem(new CMenuSeparator(CMenuSeparator::LINE));
	snprintf(cnt, sizeof(cnt), "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_MAINMENU_SETTINGS, true, NULL, selector, cnt, CRCInput::convertDigitToKey(shortcut++)), old_selected == i++);
	menu->exec(NULL, "");
	delete menu;
	delete selector;

	if(select >= 0) {
		//signed int bouquet_id = 0;
		old_selected = select;
		t_channel_id channel_id = empty ? 0 : (*chanlist)[selected]->getChannelID();
		bool tvmode = CZapit::getInstance()->getMode() & CZapitClient::MODE_TV;
		CBouquetList *blist = tvmode ? TVfavList : RADIOfavList;
		bool fav_found = true;
		switch(select) {
		case 0: // edit mode
			if (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED) {
				int pl_z = g_settings.parentallock_zaptime * 60;
				if (g_settings.personalize[SNeutrinoSettings::P_MSER_BOUQUET_EDIT] == CPersonalizeGui::PERSONALIZE_MODE_PIN) {
					unlocked = false;
				} else if (bouquet && bouquet->zapitBouquet && bouquet->zapitBouquet->bLocked) {
					/* on locked bouquet, enough to check any channel */
					unlocked = ((*chanlist)[selected]->last_unlocked_time + pl_z > time_monotonic());
				} else {
					/* check all locked channels for last_unlocked_time, overwrite only if already unlocked */
					for (unsigned int j = 0 ; j < (*chanlist).size(); j++) {
						if ((*chanlist)[j]->bLocked)
							unlocked = unlocked && ((*chanlist)[j]->last_unlocked_time + pl_z > time_monotonic());
					}
				}
				if (!unlocked) {
					CZapProtection *zp = new CZapProtection(g_settings.parentallock_pincode, 0x100);
					unlocked = zp->check();
					delete zp;
				}
			}
			if (unlocked)
				editMode(true);
			ret = -1;
			break;
		case 1: // add to
			if (!addChannelToBouquet())
				return -1;
			ret = 1;
			break;
		case 2: // add to my favorites
			for (unsigned n = 0; n < AllFavBouquetList->Bouquets.size(); n++) {
				if (AllFavBouquetList->Bouquets[n]->zapitBouquet && AllFavBouquetList->Bouquets[n]->zapitBouquet->bFav) {
					CZapitChannel *ch = AllFavBouquetList->Bouquets[n]->zapitBouquet->getChannelByChannelID(channel_id);
					if (ch == NULL) {
						AllFavBouquetList->Bouquets[n]->zapitBouquet->addService((*chanlist)[selected]);
						fav_found = false;
					}
					break;
				}
			}
			for (unsigned n = 0; n < blist->Bouquets.size() && !fav_found; n++) {
				if (blist->Bouquets[n]->zapitBouquet && blist->Bouquets[n]->zapitBouquet->bFav) {
					blist->Bouquets[n]->zapitBouquet->getChannels(blist->Bouquets[n]->channelList->channels, tvmode);
					saveChanges();
					fav_found = true;
					break;
				}
			}
			if (!fav_found) {
				CNeutrinoApp::getInstance()->MarkFavoritesChanged();
				CNeutrinoApp::getInstance()->MarkChannelsInit();
			}
			ret = 1;
			break;
		case 3: // reset new
		case 4: // reset all new
			if (select == 3) {
				(*chanlist)[selected]->flags = CZapitChannel::UPDATED;
			} else {
				for (unsigned int j = 0 ; j < (*chanlist).size(); j++)
					(*chanlist)[j]->flags = CZapitChannel::UPDATED;
			}
			CNeutrinoApp::getInstance()->MarkChannelsChanged();
			/* if make_new_list == ON, signal to re-init services */
			if(g_settings.make_new_list)
				CNeutrinoApp::getInstance()->MarkChannelsInit();
			break;
		case 5: // clear channel history
			{
				CNeutrinoApp::getInstance()->channelList->getLastChannels().clear();
				printf("%s:%d lastChList cleared\n", __FUNCTION__, __LINE__);
				ret = -2; // exit channellist
			}
			break;
		case 6: // settings
			{
				previous_channellist_additional = g_settings.channellist_additional;
				COsdSetup osd_setup;
				osd_setup.showContextChanlistMenu(this);
				//FIXME check font/options changed ?
				hide();
				calcSize();
				ret = -1;
			}
			break;
		default:
			break;
		}
	}
	return ret;
}

int CChannelList::exec()
{
	displayNext = 0; // always start with current events
	displayList = 1; // always start with event list
	int nNewChannel = show();
	if ( nNewChannel > -1 && nNewChannel < (int) (*chanlist).size()) {
		if(this->historyMode && (*chanlist)[nNewChannel]) {
			int new_mode = CNeutrinoApp::getInstance()->channelList->getLastChannels().get_mode((*chanlist)[nNewChannel]->getChannelID());
			if(new_mode >= 0)
				CNeutrinoApp::getInstance()->SetChannelMode(new_mode);
		}
		CNeutrinoApp::getInstance()->channelList->zapToChannel((*chanlist)[nNewChannel]);
	}

	return nNewChannel;
}

void CChannelList::calcSize()
{
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8 /*, name.c_str()*/);

	// recalculate theight, fheight and footerHeight for a possilble change of fontsize factor
	theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	fdescrheight = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getHeight();

	if (fheight == 0)
		fheight = 1; /* avoid div-by-zero crash on invalid font */
	footerHeight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]->getHeight()+6;

	minitv_is_active = ( (g_settings.channellist_additional == 2) && (CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_ts) );
	// calculate width
	full_width = minitv_is_active ? (frameBuffer->getScreenWidth()-2*DETAILSLINE_WIDTH) : frameBuffer->getScreenWidthRel();

	if (g_settings.channellist_additional)
		width = full_width / 3 * 2;
	else
		width = full_width;

	// calculate height (the infobox below mainbox is handled outside height)
	if (g_settings.channellist_show_infobox)
		info_height = 2*fheight + fdescrheight + 2*OFFSET_INNER_SMALL;
	else
		info_height = 0;
	height = minitv_is_active ? frameBuffer->getScreenHeight() : frameBuffer->getScreenHeightRel();
	height = height - OFFSET_INTER - info_height;

	// calculate x position
	x = getScreenStartX(full_width);
	if (x < DETAILSLINE_WIDTH)
		x = DETAILSLINE_WIDTH;

	// calculate header height
	const int pic_h = 39;
	theight = std::max(theight, pic_h);

	// calculate max entrys in mainbox
	listmaxshow = (height - theight - footerHeight) / fheight;

	// recalculate height to avoid spaces between last entry in mainbox and footer
	height = theight + listmaxshow*fheight + footerHeight;

	// calculate y position
	y = getScreenStartY(height + OFFSET_INTER + info_height);

	// calculate width/height of right info_zone and pip-box
	infozone_width = full_width - width;
	pig_width = infozone_width;
	if (minitv_is_active)
		pig_height = (pig_width * 9) / 16;
	else
		pig_height = 0;
	infozone_height = height - theight - pig_height - footerHeight;
}

bool CChannelList::updateSelection(int newpos)
{
	bool actzap = false;
	if((int) selected == newpos)
		return actzap;

	int prev_selected = selected;
	selected = newpos;
	if (move_state == beMoving) {
		internalMoveChannel(prev_selected, selected);
	} else {
		unsigned int oldliststart = liststart;

		liststart = (selected/listmaxshow)*listmaxshow;
		if (oldliststart != liststart) {
			paintBody();
			updateVfd();
		} else {
			paintItem(prev_selected - liststart);
			paintItem(selected - liststart);
			showChannelLogo();
		}

		if(!edit_state && (new_zap_mode == 2 /* active */) && SameTP()) {
			actzap = true;
			zapTo(selected);
		}
	}
	return actzap;
}

int CChannelList::getPrevNextBouquet(bool next)
{
	bool found = true;
	int dir = next ? 1 : -1;
	int b_size = bouquetList->Bouquets.size(); /* bigger than 0 */
	int nNext = (bouquetList->getActiveBouquetNumber() + b_size + dir) % b_size;
	if(bouquetList->Bouquets[nNext]->channelList->isEmpty() ) {
		found = false;
		int n_old = nNext;
		nNext = (nNext + b_size + dir) % b_size;
		for (int i = nNext; i != n_old; i = (i + b_size + dir) % b_size) {
			if( !bouquetList->Bouquets[i]->channelList->isEmpty() ) {
				found = true;
				nNext = i;
				break;
			}
		}
	}
	if (found)
		return nNext;

	return -1;
}

#define CHANNEL_SMSKEY_TIMEOUT 800
/* return: >= 0 to zap, -1 on cancel, -3 on list mode change, -4 list edited, -2 zap but no restore old list/chan ?? */
int CChannelList::show()
{
	int res = CHANLIST_CANCEL;

	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool actzap = 0;

	new_zap_mode = g_settings.channellist_new_zap_mode;

	calcSize();
	displayNext = false;

	COSDFader fader(g_settings.theme.menu_Content_alpha);
	fader.StartFadeIn();
	CInfoClock::getInstance()->ClearDisplay();
	paint();

	int oldselected = selected;
	int zapOnExit = false;
	bool bShowBouquetList = false;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

	bool loop=true;
	bool dont_hide = false;
	while (loop) {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);
		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

		bool empty = (*chanlist).empty();
		if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer())) {
			if(fader.FadeDone())
				loop = false;
		}
		else if ( ( msg == CRCInput::RC_timeout ) || ( msg == (neutrino_msg_t)g_settings.key_channelList_cancel) ) {
			if (move_state == beMoving) {
				cancelMoveChannel();
			} else if (edit_state) {
				editMode(false);
				paint();
			} else {
				res = CHANLIST_CANCEL;
				if(!actzap) {
					selected = oldselected;
				} else {
					res = CHANLIST_NO_RESTORE;
					selected = selected_in_new_mode;
				}
				if(fader.StartFadeOut()) {
					timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
					msg = 0;
				} else
					loop=false;
			}
		}
		else if(!edit_state && !empty && msg == (neutrino_msg_t) g_settings.key_record) { //start direct recording from channellist
			if((g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) && SameTP() /* && !IS_WEBTV((*chanlist)[selected]->getChannelID()) */) {
				printf("[neutrino channellist] start direct recording...\n");
				hide();
				if (!CRecordManager::getInstance()->Record((*chanlist)[selected]->getChannelID())) {
					paint();
				} else {
					selected = oldselected;
					loop=false;
				}
			}
		}
		else if(!edit_state && !empty && msg == CRCInput::RC_stop ) { //stop recording
			if(CRecordManager::getInstance()->RecordingStatus((*chanlist)[selected]->getChannelID())){
				int recmode = CRecordManager::getInstance()->GetRecordMode((*chanlist)[selected]->getChannelID());
				bool timeshift = recmode & CRecordManager::RECMODE_TSHIFT;
				bool tsplay = CMoviePlayerGui::getInstance().timeshift;
				if (recmode && !(timeshift && tsplay))
				{
					if (CRecordManager::getInstance()->AskToStop((*chanlist)[selected]->getChannelID()))
					{
						CRecordManager::getInstance()->Stop((*chanlist)[selected]->getChannelID());
						calcSize();
						paintBody();
					}
				}else{
					// stop TSHIFT: go to play mode
					g_RCInput->postMsg (msg, data);
					res = CHANLIST_CANCEL_ALL;
					loop = false;
				}
			}
		}
		else if (!empty && ((msg == CRCInput::RC_red) || (msg == CRCInput::RC_epg))) {
			if (edit_state) {
				if (move_state != beMoving && msg == CRCInput::RC_red)
					deleteChannel();
			} else {
				hide();

				/* RETURN_EXIT_ALL on FAV/SAT buttons or messages_return::cancel_all from CNeutrinoApp::getInstance()->handleMsg() */
				if ( g_EventList->exec((*chanlist)[selected]->getChannelID(), (*chanlist)[selected]->getName()) == menu_return::RETURN_EXIT_ALL) {
					res = CHANLIST_CANCEL_ALL;
					loop = false;
				} else {
					paint();
					timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
				}
			}
		}
		else if (!empty && msg == CRCInput::RC_yellow) {
			if (edit_state) {
				beginMoveChannel();
				paintItem(selected - liststart);
			} else {
				bShowBouquetList = true;
				loop = false;
			}
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			if (!edit_state) {
				//FIXME: what about LIST_MODE_WEBTV?
				int newmode = msg == CRCInput::RC_sat ? LIST_MODE_SAT : LIST_MODE_FAV;
				CNeutrinoApp::getInstance()->SetChannelMode(newmode);
				res = CHANLIST_CHANGE_MODE;
				loop = false;
			}
		}
		else if (!edit_state && msg == CRCInput::RC_setup) {
			fader.StopFade();
			int ret = doChannelMenu();
			if (ret > 0) {
				/* select next entry */
				if (selected + 1 < (*chanlist).size())
					selected++;
			}
			if (ret == -2) // exit channellist
				loop = false;
			else {
				if (ret != 0)
					paint();
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
			}
		}
		else if (!empty && msg == (neutrino_msg_t) g_settings.key_list_start) {
			actzap = updateSelection(0);
		}
		else if (!empty && msg == (neutrino_msg_t) g_settings.key_list_end) {
			actzap = updateSelection((*chanlist).size()-1);
		}
		else if (!empty && (msg == CRCInput::RC_up || (int)msg == g_settings.key_pageup ||
				    msg == CRCInput::RC_down || (int)msg == g_settings.key_pagedown))
		{
			displayList = 1;
			int new_selected = UpDownKey((*chanlist), msg, listmaxshow, selected);
			if (new_selected >= 0)
				actzap = updateSelection(new_selected);
		}
		else if (!edit_state && (msg == (neutrino_msg_t)g_settings.key_bouquet_up ||
					msg == (neutrino_msg_t)g_settings.key_bouquet_down)) {
			if (dline)
				dline->kill(); //kill details line on change to next page
			if (!bouquetList->Bouquets.empty()) {
				int nNext = getPrevNextBouquet(msg == (neutrino_msg_t)g_settings.key_bouquet_up);
				if(nNext >= 0) {
					bouquetList->activateBouquet(nNext, false);
					res = bouquetList->showChannelList();
					loop = false;
				}
				dont_hide = true;
			}
		}
		else if (msg == CRCInput::RC_ok ) {
			if (!empty) {
				if (move_state == beMoving) {
					finishMoveChannel();
				} else if (edit_state) {
					zapTo(selected);
					actzap = true;
					oldselected = selected;
					paintBody(); // refresh zapped vs selected
				} else if(SameTP()) {
					zapOnExit = true;
					loop=false;
				}
			}
		}
		else if (!edit_state && ( msg == CRCInput::RC_spkr ) && new_zap_mode ) {
			if(CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_ts) {
				switch (new_zap_mode) {
					case 2: /* active */
						new_zap_mode = 1; /* allow */
						break;
					case 1: /* allow */
						new_zap_mode = 2; /* active */
						break;
					default:
						break;

				}
				paintButtonBar(SameTP());
			}
		}
		else if (!empty && CRCInput::isNumeric(msg) && (this->historyMode || g_settings.sms_channel)) {
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
					for(i = selected+1; i < (*chanlist).size(); i++) {
						char firstCharOfTitle = (*chanlist)[i]->getName().c_str()[0];
						if(tolower(firstCharOfTitle) == smsKey) {
							//printf("SMS chan found was= %d selected= %d i= %d %s\n", was_sms, selected, i, (*chanlist)[i]->channel->getName().c_str());
							break;
						}
					}
					if(i >= (*chanlist).size()) {
						for(i = 0; i < (*chanlist).size(); i++) {
							char firstCharOfTitle = (*chanlist)[i]->getName().c_str()[0];
							if(tolower(firstCharOfTitle) == smsKey) {
								//printf("SMS chan found was= %d selected= %d i= %d %s\n", was_sms, selected, i, (*chanlist)[i]->channel->getName().c_str());
								break;
							}
						}
					}
					if(i < (*chanlist).size())
						updateSelection(i);

					smsInput.resetOldKey();
				}
			}
		}
		else if(CRCInput::isNumeric(msg)) {
			if (!edit_state) {
				//pushback key if...
				selected = oldselected;
				g_RCInput->postMsg( msg, data );
				loop = false;
			}
		}
		else if (!empty && msg == CRCInput::RC_blue )
		{
			if (edit_state) { // rename
				if (move_state != beMoving)
					renameChannel();
			} else {
				if (g_settings.channellist_additional)
					displayList = !displayList;
				else
					displayNext = !displayNext;

				paint();
			}
		}
		else if (msg == CRCInput::RC_green )
		{
			if (edit_state) {
				if (move_state != beMoving)
					addChannel();
			} else {
				int mode = CNeutrinoApp::getInstance()->GetChannelMode();
				if(mode != LIST_MODE_FAV) {
					g_settings.channellist_sort_mode++;
					if(g_settings.channellist_sort_mode > SORT_MAX-1)
						g_settings.channellist_sort_mode = SORT_ALPHA;
					CNeutrinoApp::getInstance()->SetChannelMode(mode);
					oldselected = selected;
					paint();
				}
			}
		}
		else if (!empty && edit_state && move_state != beMoving && msg == CRCInput::RC_stop )
		{
			lockChannel();
		}
		else if (!empty && edit_state && move_state != beMoving && msg == CRCInput::RC_forward )
		{
			moveChannelToBouquet();
		}
#ifdef ENABLE_PIP
		else if (!empty && ((msg == CRCInput::RC_play) || (msg == (neutrino_msg_t) g_settings.key_pip_close))) {
			if(SameTP()) {
				if (CZapit::getInstance()->GetPipChannelID() == (*chanlist)[selected]->getChannelID()) {
					g_Zapit->stopPip();
					calcSize();
					paintBody();
				} else {
					handleMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, true);
				}
			}
		}
#endif
		else if (!empty && ((msg == CRCInput::RC_info) || (msg == CRCInput::RC_help))) {
			hide();
			CChannelEvent *p_event=NULL;
			if (displayNext)
				p_event = &((*chanlist)[selected]->nextEvent);

			if(p_event && p_event->eventID)
				g_EpgData->show((*chanlist)[selected]->getChannelID(), p_event->eventID, &(p_event->startTime));
			else
				g_EpgData->show((*chanlist)[selected]->getChannelID());

			paint();
		} else if (msg == NeutrinoMessages::EVT_SERVICESCHANGED || msg == NeutrinoMessages::EVT_BOUQUETSCHANGED) {
			g_RCInput->postMsg(msg, data);
			loop = false;
			res = CHANLIST_CANCEL_ALL;
		} else {
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
				loop = false;
				res = CHANLIST_CANCEL_ALL;
			}
		}
	}

	paint_events(-2); // cancel paint_events thread

	if (move_state == beMoving)
		cancelMoveChannel();
	if (edit_state)
		editMode(false);

	if(!dont_hide){
		if (new_zap_mode && (g_settings.channellist_new_zap_mode != new_zap_mode))
			g_settings.channellist_new_zap_mode = new_zap_mode;
		new_zap_mode = 0;

		hide();
		fader.StopFade();
	}

	if (bShowBouquetList) {
		res = bouquetList->exec(true);
		printf("CChannelList:: bouquetList->exec res %d\n", res);
	}


	if(NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode())
		return -1;

	if(zapOnExit)
		res = selected;

	printf("CChannelList::show *********** res %d\n", res);
	return(res);
}

void CChannelList::hide()
{
	paint_events(-2); // cancel paint_events thread
	if ((g_settings.channellist_additional == 2) || (previous_channellist_additional == 2)) // with miniTV
	{
		if (cc_minitv)
			delete cc_minitv;
		cc_minitv = NULL;
	}
	if(header)
		header->kill();

	frameBuffer->paintBackgroundBoxRel(x, y, full_width, height + OFFSET_INTER + info_height);
	clearItem2DetailsLine();
	CInfoClock::getInstance()->enableInfoClock(!CInfoClock::getInstance()->isBlocked());
}

bool CChannelList::showInfo(int number, int epgpos)
{
	CZapitChannel* channel = getChannel(number);
	if(channel == NULL)
		return false;

	g_InfoViewer->showTitle(channel, true, epgpos); // UTF-8
	return true;
}

int CChannelList::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data, bool pip)
{
	if (msg != NeutrinoMessages::EVT_PROGRAMLOCKSTATUS) // right now the only message handled here.
		return messages_return::unhandled;
	checkLockStatus(data, pip);
	return messages_return::handled;

}

bool CChannelList::checkLockStatus(neutrino_msg_data_t data, bool pip)
{
	bool startvideo = true;
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
	if ((*chanlist)[selected]->last_unlocked_EPGid == g_RemoteControl->current_EPGid && g_RemoteControl->current_EPGid != 0)
		goto out;

	/* PARENTALLOCK_PROMPT_CHANGETOLOCKED: only pre-locked channels, don't care for fsk sent in SI */
	if (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED && data < 0x100)
		goto out;

	/* if a pre-locked channel is inside the zap time, open it. */
	if (data >= 0x100 && (*chanlist)[selected]->last_unlocked_time + g_settings.parentallock_zaptime * 60 > time_monotonic())
		goto out;

	if (pip && (*chanlist)[selected]->Locked() == g_settings.parentallock_defaultlocked)
		goto out;

	/* OK, let's ask for a PIN */
	if (!pip) {
		g_RemoteControl->is_video_started = true;
		g_RemoteControl->stopvideo();
		//printf("stopped video\n");
	}
	zapProtection = new CZapProtection(g_settings.parentallock_pincode, data);

	if (zapProtection->check())
	{
		// remember it for the next time
		/* data < 0x100: lock age -> remember EPG ID */
		if (data < 0x100)
			(*chanlist)[selected]->last_unlocked_EPGid = g_RemoteControl->current_EPGid;
		else
		{
			/* data >= 0x100: pre-locked bouquet -> remember unlock time */
			(*chanlist)[selected]->last_unlocked_time = time_monotonic();
			int bnum = bouquetList->getActiveBouquetNumber();
			/* unlock only real locked bouquet, not whole satellite or all channels! */
			if (bnum >= 0 && bouquetList->Bouquets[bnum]->zapitBouquet &&
			    bouquetList->Bouquets[bnum]->zapitBouquet->bLocked != g_settings.parentallock_defaultlocked)
			{
				/* unlock the whole bouquet */
				int i;
				for (i = 0; i < bouquetList->Bouquets[bnum]->channelList->getSize(); i++)
					bouquetList->Bouquets[bnum]->channelList->getChannelFromIndex(i)->last_unlocked_time = (*chanlist)[selected]->last_unlocked_time;
			}
		}
	}
	else
	{
		/* last_unlocked_time == 0 is the magic to tell zapTo() to not record the time.
		   Without that, zapping to a locked channel twice would open it without the PIN */
		(*chanlist)[selected]->last_unlocked_time = 0;
		startvideo = false;
	}
	delete zapProtection;
	zapProtection = NULL;

out:
	if (startvideo) {
		if(pip) {
#ifdef ENABLE_PIP
			if (CNeutrinoApp::getInstance()->StartPip((*chanlist)[selected]->getChannelID())) {
				calcSize();
				paintBody();
			}
#endif
		} else
			g_RemoteControl->startvideo();
		return true;
	}
	return false;
}

bool CChannelList::adjustToChannelID(const t_channel_id channel_id)
{
	printf("CChannelList::adjustToChannelID me %p [%s] list size %d channel_id %" PRIx64 "\n", this, getName(), (int)(*chanlist).size(), channel_id);
	selected_chid = channel_id;
	for (unsigned int i = 0; i < (*chanlist).size(); i++) {
		if ((*chanlist)[i]->getChannelID() == channel_id) {
			selected = i;
			tuned = i;
			return true;
		}
	}
//printf("CChannelList::adjustToChannelID me %x to %llx bToo %s FAILED\n", (int) this, channel_id, bToo ? "yes" : "no");fflush(stdout);

	return false;
}

#if 0
int CChannelList::hasChannel(int nChannelNr)
{
	for (uint32_t i=0; i<(*chanlist).size(); i++) {
		if (getKey(i) == nChannelNr)
			return(i);
	}
	return(-1);
}
#endif

int CChannelList::hasChannelID(t_channel_id channel_id)
{
	for (uint32_t i=0; i < (*chanlist).size(); i++) {
		if ((*chanlist)[i]->getChannelID() == channel_id)
			return i;
	}
	return -1;
}

// for adjusting bouquet's channel list after numzap or quickzap
void CChannelList::setSelected( int nChannelNr)
{
//printf("CChannelList::setSelected me %s %d -> %s\n", name.c_str(), nChannelNr, (nChannelNr < (*chanlist).size() && (*chanlist)[nChannelNr] != NULL) ? (*chanlist)[nChannelNr]->getName().c_str() : "********* NONE *********");
	//FIXME real difference between tuned and selected ?!
	selected_chid = 0;
	tuned = nChannelNr;
	if (nChannelNr < (int) (*chanlist).size()) {
		selected = nChannelNr;
		selected_chid = (*chanlist)[tuned]->getChannelID();
	}
}

// -- Zap to channel with channel_id
bool CChannelList::zapTo_ChannelID(const t_channel_id channel_id, bool force)
{
	printf("**************************** CChannelList::zapTo_ChannelID %" PRIx64 "\n", channel_id);
	for (unsigned int i = 0; i < (*chanlist).size(); i++) {
		if ((*chanlist)[i]->getChannelID() == channel_id) {
			zapTo(i, force);
			return true;
		}
	}
	return false;
}

bool CChannelList::showEmptyError()
{
	if ((*chanlist).empty()) {
		DisplayErrorMessage(g_Locale->getText(LOCALE_CHANNELLIST_NONEFOUND)); // UTF-8
		return true;
	}
	return false;
}

/* forceStoreToLastChannels defaults to false */
/* TODO make this private to call only from "current" list, where selected/pos not means channel number */
void CChannelList::zapTo(int pos, bool force)
{
	if(showEmptyError())
		return;

	if ( (pos >= (signed int) (*chanlist).size()) || (pos < 0) ) {
		pos = 0;
	}
	CZapitChannel* chan = (*chanlist)[pos];

	zapToChannel(chan, force);
	tuned = pos;
	if(edit_state || new_zap_mode == 2 /* active */)
		selected_in_new_mode = pos;
	else
		selected = pos;
}

/* to replace zapTo_ChannelID and zapTo from "whole" list ? */
void CChannelList::zapToChannel(CZapitChannel *channel, bool force)
{
	if(showEmptyError())
		return;

	if(channel == NULL)
		return;

	/* we record when we switched away from a channel, so that the parental-PIN code can
	   check for timeout. last_unlocked_time == 0 means: the PIN was not entered
	   "tuned" is the *old* channel, before zap */
	if (tuned < (*chanlist).size() && (*chanlist)[tuned]->last_unlocked_time != 0)
		(*chanlist)[tuned]->last_unlocked_time = time_monotonic();

	printf("**************************** CChannelList::zapToChannel me %p %s tuned %d new %s -> %" PRIx64 "\n", this, name.c_str(), tuned, channel->getName().c_str(), channel->getChannelID());
	if(tuned < (*chanlist).size())
		selected_chid = (*chanlist)[tuned]->getChannelID();

	if(force || (selected_chid != channel->getChannelID())) {
		if ((g_settings.radiotext_enable) && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio) && (g_Radiotext))
		{
			// stop radiotext PES decoding before zapping
			g_Radiotext->radiotext_stop();
		}

		selected_chid = channel->getChannelID();
		g_RemoteControl->zapTo_ChannelID(selected_chid, channel->getName(), channel->number, (channel->Locked() == g_settings.parentallock_defaultlocked));
		CNeutrinoApp::getInstance()->adjustToChannelID(channel->getChannelID());
	}
	if(new_zap_mode != 2 /* not active */) {
		/* remove recordModeActive from infobar */
		if(g_settings.auto_timeshift && !CNeutrinoApp::getInstance()->recordingstatus)
			g_InfoViewer->handleMsg(NeutrinoMessages::EVT_RECORDMODE, 0);

		g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR, 0 );
		CNeutrinoApp::getInstance()->channelList->getLastChannels().set_mode(channel->getChannelID());
	}
}

/* Called only from "all" channel list */
int CChannelList::numericZap(int key)
{
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
	if ((key == g_settings.key_zaphistory) || (key == g_settings.key_current_transponder)) {
		if((!autoshift && CNeutrinoApp::getInstance()->recordingstatus) || (key == g_settings.key_current_transponder)) {
			CChannelList * orgList = CNeutrinoApp::getInstance()->channelList;
			CChannelList * channelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_CURRENT_TP), false, true);

			if(key == g_settings.key_current_transponder) {
				t_channel_id recid = (*chanlist)[selected]->getChannelID() >> 16;
				for ( unsigned int i = 0 ; i < (*orgList->chanlist).size(); i++) {
					if(((*orgList->chanlist)[i]->getChannelID() >> 16) == recid)
						channelList->addChannel((*orgList->chanlist)[i]);
				}
			} else {
				for ( unsigned int i = 0 ; i < (*orgList->chanlist).size(); i++) {
					if(SameTP((*orgList->chanlist)[i]))
						channelList->addChannel((*orgList->chanlist)[i]);
				}
			}
			if ( !channelList->isEmpty()) {
				channelList->adjustToChannelID(orgList->getActiveChannel_ChannelID());
				this->frameBuffer->paintBackground();
				res = channelList->exec();
				CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
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
			if ( !channelList->isEmpty() ) {
				this->frameBuffer->paintBackground();
				res = channelList->exec();
				CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
			}
			delete channelList;
		}
		return res;
	}
	size_t maxchansize = MaxChanNr().size();
	int fw = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->getMaxDigitWidth();
	int sx = maxchansize * fw + (fw/2);
	int sy = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->getHeight() + 6;

	int ox = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - sx)/2;
	int oy = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - sy)/2;
	char valstr[10];
	int chn = CRCInput::getNumericValue(key);
	int pos = 1;
	int lastchan= -1;
	bool doZap = false;
	bool showEPG = false;
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	g_InfoViewer->setSwitchMode(CInfoViewer::IV_MODE_NUMBER_ZAP);

	while(1) {
		if (lastchan != chn) {
			snprintf(valstr, sizeof(valstr), "%d", chn);

			while(strlen(valstr) < maxchansize)
				strcat(valstr,"-");   //"_"
			frameBuffer->paintBoxRel(ox, oy, sx, sy, COL_INFOBAR_PLUS_0);

			for (int i = maxchansize-1; i >= 0; i--) {
				valstr[i+ 1] = 0;
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->RenderString(ox+fw/3+ i*fw, oy+sy-3, sx, &valstr[i], COL_INFOBAR_TEXT);
			}

			showInfo(chn);
			lastchan = chn;
		}

		g_RCInput->getMsg( &msg, &data, g_settings.timing[SNeutrinoSettings::TIMING_NUMERICZAP] * 10 );

		if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_ok) {
			doZap = true;
			break;
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg) || msg == CRCInput::RC_right) {
			// do nothing
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
			g_EventList->exec(chan->getChannelID(), chan->getName());
	}
	g_InfoViewer->resetSwitchMode();
	return res;
}

CZapitChannel* CChannelList::getPrevNextChannel(int key, unsigned int &sl)
{
	if(sl >= (*chanlist).size())
		sl = (*chanlist).size()-1;

	CZapitChannel* channel = NULL;
	int bsize = bouquetList->Bouquets.size();
	int bactive = bouquetList->getActiveBouquetNumber();

	if(!g_settings.zap_cycle && bsize > 1) {
		size_t cactive = sl;

		printf("CChannelList::getPrevNextChannel: selected %d total %d active bouquet %d total %d\n", (int)cactive, (int)(*chanlist).size(), bactive, bsize);
		if ((key == g_settings.key_quickzap_down) || (key == CRCInput::RC_left)) {
			if(cactive == 0) {
				bactive = getPrevNextBouquet(false);
				if (bactive >= 0) {
					bouquetList->activateBouquet(bactive, false);
					cactive = bouquetList->Bouquets[bactive]->channelList->getSize() - 1;
				}
			} else
				--cactive;
		}
		else if ((key == g_settings.key_quickzap_up) || (key == CRCInput::RC_right)) {
			cactive++;
			if(cactive >= (*chanlist).size()) {
				bactive = getPrevNextBouquet(true);
				if (bactive >= 0) {
					bouquetList->activateBouquet(bactive, false);
					cactive = 0;
				}
			}
		}
		sl = cactive;
		channel = bouquetList->Bouquets[bactive]->channelList->getChannelFromIndex(cactive);
		printf("CChannelList::getPrevNextChannel: selected %u total %d active bouquet %d total %d channel %x (%s)\n",
				cactive, (*chanlist).size(), bactive, bsize, (int) channel, channel ? channel->getName().c_str(): "");
	} else {
		if ((key == g_settings.key_quickzap_down) || (key == CRCInput::RC_left)) {
			if(sl == 0)
				sl = (*chanlist).size()-1;
			else
				sl--;
		}
		else if ((key==g_settings.key_quickzap_up) || (key == CRCInput::RC_right)) {
			sl = (sl+1)%(*chanlist).size();
		}
		channel = (*chanlist)[sl];
	}
	return channel;
}

void CChannelList::virtual_zap_mode(bool up)
{
	if(showEmptyError())
		return;

	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

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
	g_InfoViewer->resetSwitchMode(); //disable virtual_zap_mode

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
			g_EventList->exec(channel->getChannelID(), channel->getName());
	}
}

bool CChannelList::quickZap(int key, bool /* cycle */)
{
	if((*chanlist).empty())
		return true;

	unsigned int sl = selected;
	/* here selected value doesnt matter, zap will do adjust */
	CZapitChannel* channel = getPrevNextChannel(key, sl);
	bool ret = false;
	if(channel && SameTP(channel)) {
		CNeutrinoApp::getInstance()->channelList->zapToChannel(channel);
		ret = true;
	}
	g_RCInput->clearRCMsg(); //FIXME test for n.103
	return ret;
}

void CChannelList::paintDetails(int index)
{
	if (!g_settings.channellist_show_infobox)
		return;

	int ypos   = y + height + OFFSET_INTER;
	int ypos_a = ypos + OFFSET_INNER_SMALL;

	frameBuffer->paintBoxRel(x, ypos, full_width, info_height, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE);
	frameBuffer->paintBoxFrame(x, ypos, full_width, info_height, 2, COL_FRAME_PLUS_0, RADIUS_LARGE);

	if ((*chanlist).empty())
		return;

	//colored_events init
	bool colored_event_C = (g_settings.theme.colored_events_channellist == 1);
	bool colored_event_N = (g_settings.theme.colored_events_channellist == 2);

	CChannelEvent *p_event = NULL;
	if (displayNext)
		p_event = &(*chanlist)[index]->nextEvent;
	else
		p_event = &(*chanlist)[index]->currentEvent;

	if (/* !IS_WEBTV((*chanlist)[index]->getChannelID()) && */ p_event && !p_event->description.empty()) {
		char cNoch[50] = {0}; // UTF-8
		char cSeit[50] = {0}; // UTF-8

		struct		tm *pStartZeit = localtime(&p_event->startTime);
		unsigned 	seit = ( time(NULL) - p_event->startTime ) / 60;
		snprintf(cSeit, sizeof(cSeit), "%s %02d:%02d",(displayNext) ? g_Locale->getText(LOCALE_CHANNELLIST_START):g_Locale->getText(LOCALE_CHANNELLIST_SINCE), pStartZeit->tm_hour, pStartZeit->tm_min);
		if (displayNext) {
			snprintf(cNoch, sizeof(cNoch), "(%d %s)", p_event->duration / 60, unit_short_minute);
		} else {
			int noch = (p_event->startTime + p_event->duration - time(NULL)) / 60;
			if ((noch< 0) || (noch>=10000))
				noch= 0;
			snprintf(cNoch, sizeof(cNoch), "(%u / %d %s)", seit, noch, unit_short_minute);
		}
		int seit_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cSeit);
		int noch_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cNoch);

		std::string text1= p_event->description;
		std::string text2= p_event->text;

		int xstart = OFFSET_INNER_MID;
		if (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1) > (full_width - 3*OFFSET_INNER_MID - seit_len) )
		{
			// zu breit, Umbruch versuchen...
			int pos;
			do {
				pos = text1.find_last_of("[ -.]+");
				if ( pos!=-1 )
					text1 = text1.substr( 0, pos );
			} while ( ( pos != -1 ) && (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1) > (full_width - 3*OFFSET_INNER_MID - seit_len) ) );

			std::string text3 = ""; /* not perfect, but better than crashing... */
			if (p_event->description.length() > text1.length())
				text3 = p_event->description.substr(text1.length()+ 1);

			if (!text2.empty() && !text3.empty())
				text3= text3+ " - ";

			xstart += g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text3);
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + OFFSET_INNER_MID, ypos_a + 2*fheight, full_width - 3*OFFSET_INNER_MID - noch_len, text3, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_MENUCONTENTDARK_TEXT);
		}

		if (!(text2.empty())) {
			while ( text2.find_first_of("[ -.+*#?=!$%&/]+") == 0 )
				text2 = text2.substr( 1 );
			text2 = text2.substr( 0, text2.find('\n') );
#if 0 //FIXME: to discuss, eat too much cpu time if string long enough
			int pos = 0;
			while ( ( pos != -1 ) && (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text2) > (full_width - 3*OFFSET_INNER_MID - noch_len) ) ) {
				pos = text2.find_last_of(" ");

				if ( pos!=-1 ) {
					text2 = text2.substr( 0, pos );
				}
			}
#endif
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x + xstart, ypos_a + fdescrheight+ fheight, full_width- xstart- 3*OFFSET_INNER_MID- noch_len, text2, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_MENUCONTENTDARK_TEXT);
		}

		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ OFFSET_INNER_MID, ypos_a + fheight, full_width - 3*OFFSET_INNER_MID - seit_len, text1, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_MENUCONTENTDARK_TEXT);
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ full_width- OFFSET_INNER_MID- seit_len, ypos_a + fheight              , seit_len, cSeit, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_MENUCONTENTDARK_TEXT);
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ full_width- OFFSET_INNER_MID- noch_len, ypos_a + fdescrheight+ fheight, noch_len, cNoch, colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_MENUCONTENTDARK_TEXT);
	}
	else if (IS_WEBTV((*chanlist)[index]->getChannelID())) {
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ OFFSET_INNER_MID, ypos_a + fheight, full_width - 3*OFFSET_INNER_MID, (*chanlist)[index]->getDesc(), colored_event_C ? COL_COLORED_EVENTS_TEXT : COL_MENUCONTENTDARK_TEXT, 0, true);
	}
	if (g_settings.channellist_foot == 0 && IS_WEBTV((*chanlist)[index]->getChannelID())) {
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ OFFSET_INNER_MID, ypos_a + 2*fheight + fdescrheight, full_width - 3*OFFSET_INNER_MID, (*chanlist)[index]->getUrl(), COL_MENUCONTENTDARK_TEXT, 0, true);
	} else if(g_settings.channellist_foot == 0) {
		transponder t;
		CServiceManager::getInstance()->GetTransponder((*chanlist)[index]->getTransponderId(), t);

		std::string desc = t.description();
		if((*chanlist)[index]->pname)
			desc = desc + " (" + std::string((*chanlist)[index]->pname) + ")";
		else
			desc = desc + " (" + CServiceManager::getInstance()->GetSatelliteName((*chanlist)[index]->getSatellitePosition()) + ")";

		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ OFFSET_INNER_MID, ypos_a + 2*fheight +fdescrheight, full_width - 3*OFFSET_INNER_MID, desc.c_str(), COL_MENUCONTENTDARK_TEXT);
	}
	else if( !displayNext && g_settings.channellist_foot == 1) { // next Event

		CSectionsdClient::CurrentNextInfo CurrentNext;
		CEitManager::getInstance()->getCurrentNextServiceKey((*chanlist)[index]->getEpgID(), CurrentNext);
		if (!CurrentNext.next_name.empty()) {
			char buf[128] = {0};
			char cFrom[50] = {0}; // UTF-8
			struct tm *pStartZeit = localtime (& CurrentNext.next_zeit.startzeit);
			snprintf(cFrom, sizeof(cFrom), "%s %02d:%02d",g_Locale->getText(LOCALE_WORD_FROM),pStartZeit->tm_hour, pStartZeit->tm_min );
			snprintf(buf, sizeof(buf), "%s", CurrentNext.next_name.c_str());
			int from_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cFrom);

			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ OFFSET_INNER_MID, ypos_a + 2*fheight+ fdescrheight, full_width - 3*OFFSET_INNER_MID - from_len, buf, colored_event_N ? COL_COLORED_EVENTS_TEXT :COL_MENUCONTENTDARK_TEXT);
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x+ full_width- OFFSET_INNER_MID- from_len, ypos_a + 2*fheight+ fdescrheight, from_len, cFrom, colored_event_N ? COL_COLORED_EVENTS_TEXT : COL_MENUCONTENTDARK_TEXT);
		}
	}
}

void CChannelList::clearItem2DetailsLine()
{
	paintItem2DetailsLine (-1);
}

void CChannelList::paintItem2DetailsLine (int pos)
{
	if (dline){
		dline->kill(); //kill details line
		delete dline;
		dline = NULL;
	}

	if (!g_settings.channellist_show_infobox)
		return;

	int xpos  = x - DETAILSLINE_WIDTH;
	int ypos1 = y + theight + pos*fheight + (fheight/2);
	int ypos2 = y + height + OFFSET_INTER + (info_height/2);

	// paint Line if detail info (and not valid list pos)
	if (pos >= 0) {
		if (dline == NULL)
			dline = new CComponentsDetailsLine(xpos, ypos1, ypos2, fheight/2, info_height-RADIUS_LARGE*2);
		dline->paint(false);
	}
}

void CChannelList::paintAdditionals(int index)
{
	if (g_settings.channellist_additional)
	{
		if (displayList)
			paint_events(index);
		else
			showdescription(selected);
	}
}

void CChannelList::showChannelLogo()
{
	if ((*chanlist).empty())
		return;
	if(g_settings.channellist_show_channellogo){
		header->setChannelLogo((*chanlist)[selected]->getChannelID(), (*chanlist)[selected]->getName());
		header->getChannelLogoObject()->hide();
		header->getChannelLogoObject()->clearSavedScreen();
		header->getChannelLogoObject()->allowPaint(true);
		header->getChannelLogoObject()->paint();
	}
}

#define NUM_LIST_BUTTONS_SORT 9
struct button_label SChannelListButtons_SMode[NUM_LIST_BUTTONS_SORT] =
{
	{ NEUTRINO_ICON_BUTTON_RED,             LOCALE_MISCSETTINGS_EPG_HEAD},
	{ NEUTRINO_ICON_BUTTON_GREEN,           LOCALE_CHANNELLIST_FOOT_SORT_ALPHA},
	{ NEUTRINO_ICON_BUTTON_YELLOW,          LOCALE_BOUQUETLIST_HEAD},
	{ NEUTRINO_ICON_BUTTON_BLUE,            LOCALE_INFOVIEWER_NEXT},
	{ NEUTRINO_ICON_BUTTON_RECORD_INACTIVE, NONEXISTANT_LOCALE},
	{ NEUTRINO_ICON_BUTTON_PLAY,            LOCALE_EXTRA_KEY_PIP_CLOSE},
	{ NEUTRINO_ICON_BUTTON_INFO_SMALL,      NONEXISTANT_LOCALE},
	{ NEUTRINO_ICON_BUTTON_MENU_SMALL,      NONEXISTANT_LOCALE},
	{ NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE, NONEXISTANT_LOCALE}
};

#define NUM_LIST_BUTTONS_EDIT 6
const struct button_label SChannelListButtons_Edit[NUM_LIST_BUTTONS_EDIT] =
{
        { NEUTRINO_ICON_BUTTON_RED   , LOCALE_BOUQUETEDITOR_DELETE     },
        { NEUTRINO_ICON_BUTTON_GREEN , LOCALE_BOUQUETEDITOR_ADD        },
        { NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETEDITOR_MOVE       },
        { NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_BOUQUETEDITOR_RENAME },
        { NEUTRINO_ICON_BUTTON_FORWARD  , LOCALE_BOUQUETEDITOR_MOVE_TO },
        { NEUTRINO_ICON_BUTTON_STOP  , LOCALE_BOUQUETEDITOR_LOCK       }
};

void CChannelList::paintButtonBar(bool is_current)
{
	//printf("[neutrino channellist] %s...%d, selected %d\n", __FUNCTION__, __LINE__, selected);
	unsigned int smode = CNeutrinoApp::getInstance()->GetChannelMode();

	int y_foot = y + (height - footerHeight);
	if (edit_state) {
		::paintButtons(x, y_foot, full_width, NUM_LIST_BUTTONS_EDIT, SChannelListButtons_Edit, full_width, footerHeight);
		return;
	}
	t_channel_id channel_id = 0;
	if (!(*chanlist).empty())
		channel_id = (*chanlist)[selected]->getChannelID();

	struct button_label Button[NUM_LIST_BUTTONS_SORT];
	bool do_record = CRecordManager::getInstance()->RecordingStatus(getActiveChannel_ChannelID());

	int bcnt = 0;
	for (int i = 0; i < NUM_LIST_BUTTONS_SORT; i++) {
		Button[bcnt] = SChannelListButtons_SMode[i];
		if (!channel_id && i != 2 && i != 7)
			continue;
		if (i == 1) {
			/* check green / sort */
			if(smode != LIST_MODE_FAV) {
				switch (g_settings.channellist_sort_mode) {
					case SORT_ALPHA:
						Button[bcnt].locale = LOCALE_CHANNELLIST_FOOT_SORT_ALPHA;
						break;
					case SORT_TP:
						Button[bcnt].locale = LOCALE_CHANNELLIST_FOOT_SORT_FREQ;
						break;
					case SORT_SAT:
						Button[bcnt].locale = LOCALE_CHANNELLIST_FOOT_SORT_SAT;
						break;
					case SORT_CH_NUMBER:
						Button[bcnt].locale = LOCALE_CHANNELLIST_FOOT_SORT_CHNUM;
						break;
					default:
						break;
				}
			} else
				continue;
		}
		if (i == 3) {
			//manage now/next button
			if (g_settings.channellist_additional) {
				if (displayList)
					Button[bcnt].locale = LOCALE_FONTSIZE_CHANNELLIST_DESCR;
				else
					Button[bcnt].locale = LOCALE_FONTMENU_EVENTLIST;
			} else {
				if (displayNext)
					Button[bcnt].locale = LOCALE_INFOVIEWER_NOW;
				else
					Button[bcnt].locale = LOCALE_INFOVIEWER_NEXT;
			}
		}
		if (i == 4) {
			//manage record button
			if (g_settings.recording_type == RECORDING_OFF)
				continue;
			if (IS_WEBTV(channel_id))
				continue;
			if (!displayNext){
				if (do_record){
					Button[bcnt].locale = LOCALE_MAINMENU_RECORDING_STOP;
					Button[bcnt].button = NEUTRINO_ICON_BUTTON_STOP;
				} else if (is_current) {
					Button[bcnt].locale = LOCALE_MAINMENU_RECORDING;
					Button[bcnt].button = NEUTRINO_ICON_BUTTON_RECORD_ACTIVE;
				} else {
					Button[bcnt].locale = NONEXISTANT_LOCALE;
					Button[bcnt].button = NEUTRINO_ICON_BUTTON_RECORD_INACTIVE;
				}
			}
		}
		if (i == 5) {
			//manage pip button
#ifdef ENABLE_PIP
			if (!is_current || IS_WEBTV(channel_id))
#endif
				continue;
		}

		if (i == 8) {
			/* check mute / zap mode */
			if (new_zap_mode)
				Button[bcnt].button = new_zap_mode == 2 /* active */ ?
					NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE : NEUTRINO_ICON_BUTTON_MUTE_ZAP_INACTIVE;
			else
				continue;
		}
		bcnt++;
	}
	//paint buttons
	::paintButtons(x, y_foot, full_width, bcnt, Button, full_width, footerHeight);
}

void CChannelList::paintItem(int pos, const bool firstpaint)
{
	if( (*chanlist).empty() ){
		return;
	}
	int ypos = y+ theight + pos*fheight;
	bool is_available = true;
	bool paintbuttons = false;
	unsigned int curr = liststart + pos;

	if (curr < (*chanlist).size())
	{
		if (edit_state)
			is_available = !((*chanlist)[curr]->flags & CZapitChannel::NOT_PRESENT);
		else
			is_available = SameTP((*chanlist)[curr]);
	}

	if (selected >= (*chanlist).size())
		selected = (*chanlist).size()-1;

	bool i_selected	= curr == selected;
	bool i_marked	= getKey(curr) == CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber() && new_zap_mode != 2 /*active*/;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t ecolor; // we need one more color for displayNext
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected, i_marked);
	ecolor = color;

	if (i_selected || i_marked)
		i_radius = RADIUS_LARGE;

	if (i_selected)
	{
		paintItem2DetailsLine (pos);
		paintDetails(curr);
		paintAdditionals(curr);
		paintbuttons = true;
	}

	if (displayNext)
	{
		/*
		   I think it's unnecessary to change colors in this case.
		   The user should know when he has pressed the blue button.
		*/
		if (g_settings.theme.colored_events_channellist == 2 /* next */)
			ecolor = COL_COLORED_EVENTS_TEXT;
		else
			ecolor = COL_MENUCONTENTINACTIVE_TEXT;
	}

	if (!is_available)
		color = COL_MENUCONTENTINACTIVE_TEXT;

	if (!firstpaint || i_selected || getKey(curr) == CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber())
		  frameBuffer->paintBoxRel(x,ypos, width- 15, fheight, bgcolor, i_radius);

	if(curr < (*chanlist).size()) {
		char nameAndDescription[255];
		char tmp[10];
		CZapitChannel* chan = (*chanlist)[curr];
		int prg_offset = 0;
		int title_offset = 0;
		int rec_mode;
		if (g_settings.theme.progressbar_design_channellist != CProgressBar::PB_OFF)
		{
			prg_offset = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("00:00");
			title_offset = OFFSET_INNER_MID;
		}

		snprintf(tmp, sizeof(tmp), "%d", this->historyMode ? pos : chan->number);

		CChannelEvent *p_event=NULL;
		if (displayNext)
			p_event = &chan->nextEvent;
		else
			p_event = &chan->currentEvent;

		//record check
		rec_mode = CRecordManager::getInstance()->GetRecordMode((*chanlist)[curr]->getChannelID());

		//set recording icon
		const char *record_icon = NULL;
		if (rec_mode & CRecordManager::RECMODE_REC)
			record_icon = NEUTRINO_ICON_REC;
		else if (rec_mode & CRecordManager::RECMODE_TSHIFT)
			record_icon = NEUTRINO_ICON_AUTO_SHIFT;

		//set pip icon
		const char *pip_icon = NULL;
#ifdef ENABLE_PIP
		if ((*chanlist)[curr]->getChannelID() == CZapit::getInstance()->GetPipChannelID())
			pip_icon = NEUTRINO_ICON_PIP;
#endif

		//set webtv icon
		const char *webtv_icon = NULL;
		if (!chan->getUrl().empty())
			webtv_icon = NEUTRINO_ICON_STREAMING;

		//set scramble icon
		const char *scramble_icon = NULL;
		if (chan->scrambled)
			scramble_icon = NEUTRINO_ICON_SCRAMBLED;

		//calculate and paint right status icons
		int icon_w = 0;
		int icon_h = 0;
		int offset_right = OFFSET_INNER_MID;
		int icon_x_right = x + width - 15 - offset_right;

		if (scramble_icon)
		{
			frameBuffer->getIconSize(scramble_icon, &icon_w, &icon_h);
			if (frameBuffer->paintIcon(scramble_icon, icon_x_right - icon_w, ypos, fheight))
			{
				offset_right += icon_w + OFFSET_INNER_MID;
				icon_x_right -= icon_w + OFFSET_INNER_MID;
			}
		}

		if (webtv_icon)
		{
			frameBuffer->getIconSize(webtv_icon, &icon_w, &icon_h);
			if (frameBuffer->paintIcon(webtv_icon, icon_x_right - icon_w, ypos, fheight))
			{
				offset_right += icon_w + OFFSET_INNER_MID;
				icon_x_right -= icon_w + OFFSET_INNER_MID;
			}
		}

		if (pip_icon)
		{
			frameBuffer->getIconSize(pip_icon, &icon_w, &icon_h);
			if (frameBuffer->paintIcon(pip_icon, icon_x_right - icon_w, ypos, fheight))
			{
				offset_right += icon_w + OFFSET_INNER_MID;
				icon_x_right -= icon_w + OFFSET_INNER_MID;
			}
		}

		if (record_icon)
		{
			frameBuffer->getIconSize(record_icon, &icon_w, &icon_h);
			if (frameBuffer->paintIcon(record_icon, icon_x_right - icon_w, ypos, fheight))
			{
				offset_right += icon_w + OFFSET_INNER_MID;
			}
		}

		//paint buttons
		if (paintbuttons)
			paintButtonBar(is_available);

		//channel numbers
		if (curr == selected && move_state == beMoving)
		{
			frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icon_w, &icon_h);
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + OFFSET_INNER_MID + numwidth - icon_w, ypos, fheight);
		}
		else if (edit_state && chan->bLocked)
		{
			frameBuffer->getIconSize(NEUTRINO_ICON_LOCK, &icon_w, &icon_h);
			frameBuffer->paintIcon(NEUTRINO_ICON_LOCK, x + OFFSET_INNER_MID + numwidth - icon_w, ypos, fheight);
		}
		else if (g_settings.channellist_show_numbers)
		{
			int numpos = x + OFFSET_INNER_MID + numwidth - g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth(tmp);
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(numpos, ypos + fheight, numwidth + 5, tmp, color, fheight);
		}
		else if (!edit_state)
		{
			numwidth = -5;
		}

		int l=0;
		if (this->historyMode)
			l = snprintf(nameAndDescription, sizeof(nameAndDescription), ": %d %s", chan->number, chan->getName().c_str());
		else
			l = snprintf(nameAndDescription, sizeof(nameAndDescription), "%s", chan->getName().c_str());

		int pb_width = prg_offset;
		int pb_height = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getDigitHeight();
		CProgressBar pb(x + OFFSET_INNER_MID + numwidth + title_offset, ypos + (fheight-pb_height)/2, pb_width, pb_height, COL_MENUCONTENT_PLUS_0);
		pb.setType(CProgressBar::PB_TIMESCALE);
		pb.setDesign(g_settings.theme.progressbar_design_channellist);
		pb.setCornerType(0);
		pb.setStatusColors(COL_MENUCONTENT_PLUS_3, COL_MENUCONTENT_PLUS_1);
		int pb_frame = 0;
		if (g_settings.theme.progressbar_design_channellist == CProgressBar::PB_MONO && !g_settings.theme.progressbar_gradient)
		{
			// add small frame to mono progressbars w/o gradient for a better visibility
			pb_frame = 1;
		}
		pb.setFrameThickness(pb_frame);
		pb.doPaintBg(false);

		if (!(p_event->description.empty()))
		{
			snprintf(nameAndDescription+l, sizeof(nameAndDescription)-l, g_settings.channellist_epgtext_align_right ? "  " : " - ");
			unsigned int ch_name_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(nameAndDescription);
			unsigned int ch_desc_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(p_event->description);

			int max_desc_len = width - numwidth - prg_offset - ch_name_len - 15 - 3*OFFSET_INNER_MID - offset_right; // 15 = scrollbar

			if (max_desc_len < 0)
				max_desc_len = 0;
			if ((int) ch_desc_len > max_desc_len)
				ch_desc_len = max_desc_len;

			if (g_settings.theme.progressbar_design_channellist != CProgressBar::PB_OFF)
			{
				if(displayNext)
				{
					struct tm *pStartZeit = localtime(&p_event->startTime);

					snprintf(tmp, sizeof(tmp), "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
					g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x + OFFSET_INNER_MID + numwidth + OFFSET_INNER_MID, ypos + fheight, width - numwidth - 15 - prg_offset - 2*OFFSET_INNER_MID, tmp, ecolor, fheight);
				}
				else
				{
					time_t jetzt=time(NULL);
					int runningPercent = 0;

					if (((jetzt - p_event->startTime + 30) / 60) < 0 )
						runningPercent= 0;
					else
						runningPercent=(jetzt-p_event->startTime) * pb_width / p_event->duration;

					pb.setValues(runningPercent, pb_width);
					pb.paint();
				}
			}

			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + OFFSET_INNER_MID + numwidth + OFFSET_INNER_MID + prg_offset + OFFSET_INNER_MID, ypos + fheight, width - numwidth - 4*OFFSET_INNER_MID - 15 - prg_offset, nameAndDescription, color);
			if (g_settings.channellist_epgtext_align_right)
			{
				// align right
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x + width - 15 - offset_right - ch_desc_len, ypos + fheight, ch_desc_len, p_event->description, ecolor);
			}
			else
			{
				// align left
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x + OFFSET_INNER_MID + numwidth + OFFSET_INNER_MID + prg_offset + OFFSET_INNER_MID + ch_name_len, ypos + fheight, ch_desc_len, p_event->description, ecolor);
			}
		}
		else
		{
			if (g_settings.theme.progressbar_design_channellist != CProgressBar::PB_OFF)
			{
				pb.setValues(0, pb_width);
				pb.paint();
			}
			//name
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + OFFSET_INNER_MID + numwidth + OFFSET_INNER_MID + prg_offset + OFFSET_INNER_MID, ypos + fheight, width - numwidth - 4*OFFSET_INNER_MID - 15 - prg_offset, nameAndDescription, color);
		}
		if (!firstpaint && curr == selected)
			updateVfd();
	}
}


void CChannelList::updateVfd()
{
	if (selected >= (*chanlist).size())
		return;

	CZapitChannel* chan = (*chanlist)[selected];
	CChannelEvent *p_event=NULL;
	if (displayNext)
		p_event = &chan->nextEvent;
	else
		p_event = &chan->currentEvent;

	if (!(chan->currentEvent.description.empty())) {
		char nameAndDescription[255];
		snprintf(nameAndDescription, sizeof(nameAndDescription), "%s - %s",
				chan->getName().c_str(), p_event->description.c_str());
		CVFD::getInstance()->showMenuText(0, nameAndDescription, -1, true); // UTF-8
	} else
		CVFD::getInstance()->showMenuText(0, chan->getName().c_str(), -1, true); // UTF-8
}

void CChannelList::paint()
{
	TIMER_START();
	paintHead();
	TIMER_STOP("CChannelList::paint() after paint head");
	paintBody();
	TIMER_STOP("CChannelList::paint() after paint body");
	updateVfd();
	TIMER_STOP("CChannelList::paint() paint total");
}

void CChannelList::paintHead()
{
	if (header == NULL){
		header = new CComponentsHeader();
		header->getTextObject()->enableTboxSaveScreen(g_settings.theme.menu_Head_gradient);//enable screen save for title text if color gradient is in use
	}

	header->setDimensionsAll(x, y, full_width, theight);
	header->setCorner(RADIUS_LARGE, CORNER_TOP);

	if (bouquet && bouquet->zapitBouquet && bouquet->zapitBouquet->bLocked != g_settings.parentallock_defaultlocked)
		header->setIcon(NEUTRINO_ICON_LOCK);

	string header_txt 		= !edit_state ? name : string(g_Locale->getText(LOCALE_CHANNELLIST_EDIT)) + ": " + name;
	fb_pixel_t header_txt_col 	= (edit_state ? COL_RED : COL_MENUHEAD_TEXT);
	header->setColorBody(COL_MENUHEAD_PLUS_0);

	header->setCaption(header_txt, CTextBox::NO_AUTO_LINEBREAK, header_txt_col);

	if (timeset) {
		if(!edit_state){
			if (header->getContextBtnObject())
				if (!header->getContextBtnObject()->empty())
					header->removeContextButtons();
			header->enableClock(true, "%H:%M", "%H %M", true);
			logo_off = header->getClockObject()->getWidth() + OFFSET_INNER_MID;

			header->getClockObject()->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);
		}else{
			if (header->getClockObject()){
				header->disableClock();
				header->setContextButton(CComponentsHeader::CC_BTN_EXIT);
			}
		}
	}
	else
		logo_off = OFFSET_INNER_MID;

	if(g_settings.channellist_show_channellogo){
		//ensure to have clean background
		header->getChannelLogoObject()->hide();
		header->setChannelLogo((*chanlist)[selected]->getChannelID(), (*chanlist)[selected]->getName());
		header->getChannelLogoObject()->allowPaint(false);
	}
	header->paint(CC_SAVE_SCREEN_NO);
	showChannelLogo();
}

CComponentsHeader* CChannelList::getHeaderObject()
{
	return header;
}

void CChannelList::ResetModules()
{
	delete header;
	header = NULL;
	if(dline){
		delete dline;
		dline = NULL;
	}
	if (cc_minitv){
		delete 	cc_minitv;
		cc_minitv = NULL;
	}
}

void CChannelList::paintBody()
{
	int icon_w = 0, icon_h = 0;
	numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth(MaxChanNr());
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icon_w, &icon_h);
	numwidth = std::max(icon_w, (int) numwidth);
	frameBuffer->getIconSize(NEUTRINO_ICON_LOCK, &icon_w, &icon_h);
	numwidth = std::max(icon_w, (int) numwidth);

	liststart = (selected/listmaxshow)*listmaxshow;
	updateEvents(this->historyMode ? 0:liststart, this->historyMode ? 0:(liststart + listmaxshow));

	if (minitv_is_active)
		paintPig(x+width, y+theight, pig_width, pig_height);

	// paint background for main box
	frameBuffer->paintBoxRel(x, y+theight, width, height-footerHeight-theight, COL_MENUCONTENT_PLUS_0);
	if (g_settings.channellist_additional)
	{
		// disable displayNext
		displayNext = false;
		// paint background for right box
		frameBuffer->paintBoxRel(x+width,y+theight+pig_height,infozone_width,infozone_height,COL_MENUCONTENT_PLUS_0);
	}

	unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);

	for(unsigned int count = 0; count < listmaxshow; count++)
		paintItem(count, true);

	const int ypos = y+ theight;
	const int sb = height - theight - footerHeight; // paint scrollbar over full height of main box
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_SCROLLBAR_PASSIVE_PLUS_0);
	unsigned int listmaxshow_tmp = listmaxshow ? listmaxshow : 1;//avoid division by zero
	int sbc= (((*chanlist).size()- 1)/ listmaxshow_tmp)+ 1;
	const int sbs= (selected/listmaxshow_tmp);
	if (sbc < 1)
		sbc = 1;

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs*(sb-4)/sbc, 11, (sb-4)/sbc, COL_SCROLLBAR_ACTIVE_PLUS_0);
	showChannelLogo();
	if ((*chanlist).empty())
		paintButtonBar(false);
}

bool CChannelList::isEmpty() const
{
	return (*chanlist).empty();
}

int CChannelList::getSize() const
{
	return (*chanlist).size();
}

int CChannelList::getSelectedChannelIndex() const
{
	return this->selected;
}

bool CChannelList::SameTP(t_channel_id channel_id)
{
	bool iscurrent = true;

	if(CNeutrinoApp::getInstance()->recordingstatus) {
		if (IS_WEBTV(channel_id))
			return true;

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
			channel = (*chanlist)[selected];

		if (IS_WEBTV(channel->getChannelID()))
			return true;

		iscurrent = CFEManager::getInstance()->canTune(channel);
	}
	return iscurrent;
}

std::string CChannelList::MaxChanNr()
{
	int n = 1;
	for (zapit_list_it_t it = (*chanlist).begin(); it != (*chanlist).end(); ++it)
	{
		n = std::max(n, (*it)->number);
	}
	return to_string(n);
}

void CChannelList::paintPig(int _x, int _y, int w, int h)
{
	//init minitv object with basic properties
	if (cc_minitv == NULL){
		cc_minitv = new CComponentsPIP (0, 0);
		cc_minitv->setPicture(NEUTRINO_ICON_AUDIOPLAY);
		cc_minitv->setFrameThickness(5);
	}
	//set changeable minitv properties
	cc_minitv->setDimensionsAll(_x, _y, w, h);
	cc_minitv->setCorner(0);
	cc_minitv->setColorFrame(COL_MENUCONTENT_PLUS_0);
	cc_minitv->paint(false);
}

void CChannelList::paint_events(int index)
{
	if (index == -2 && paint_events_index > -2) {
		pthread_mutex_lock(&paint_events_mutex);
		paint_events_index = index;
		sem_post(&paint_events_sem);
		pthread_join(paint_events_thr, NULL);
		sem_destroy(&paint_events_sem);
		pthread_mutex_unlock(&paint_events_mutex);
	} else if (paint_events_index == -2) {
		if (index == -2)
			return;
		// First paint_event. No need to lock.
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
		pthread_mutex_init(&paint_events_mutex, &attr);

		sem_init(&paint_events_sem, 0, 0);
		paint_events_index = index;
		if (!pthread_create(&paint_events_thr, NULL, paint_events, (void *) this))
			sem_post(&paint_events_sem);
		else
			paint_events_index = -2;
	} else {
		pthread_mutex_lock(&paint_events_mutex);
		paint_events_index = index;
		pthread_mutex_unlock(&paint_events_mutex);
		sem_post(&paint_events_sem);
	}
}

void *CChannelList::paint_events(void *arg)
{
	CChannelList *me = (CChannelList *) arg;
	me->paint_events();
	pthread_exit(NULL);
}

void CChannelList::paint_events()
{
	set_threadname(__func__);

	while (paint_events_index != -2) {
		sem_wait(&paint_events_sem);
		if (paint_events_index < 0)
			continue;
		while(!sem_trywait(&paint_events_sem));
		int current_index = paint_events_index;

		CChannelEventList evtlist;
		readEvents((*chanlist)[current_index]->getEpgID(), evtlist);
		if (current_index == paint_events_index) {
			pthread_mutex_lock(&paint_events_mutex);
			if (current_index == paint_events_index)
				paint_events_index = -1;
			pthread_mutex_unlock(&paint_events_mutex);
			paint_events(evtlist);
		}
	}
}

void CChannelList::paint_events(CChannelEventList &evtlist)
{
	ffheight = g_Font[eventFont]->getHeight();
	frameBuffer->paintBoxRel(x+ width,y+ theight+pig_height, infozone_width, infozone_height,COL_MENUCONTENT_PLUS_0);

	char startTime[10];
	int eventStartTimeWidth = 4 * g_Font[eventFont]->getMaxDigitWidth() + g_Font[eventFont]->getRenderWidth(":") + OFFSET_INNER_SMALL; // use a fixed value
	int startTimeWidth = 0;
	CChannelEventList::iterator e;
	time_t azeit;
	time(&azeit);
	unsigned int u_azeit = ( azeit > -1)? azeit:0;

	if ( evtlist.empty() )
	{
		CChannelEvent evt;

		evt.description = g_Locale->getText(LOCALE_EPGLIST_NOEVENTS);
		evt.eventID = 0;
		evt.startTime = 0;
		evt.duration = 0;
		evtlist.push_back(evt);
	}

	int i=1;
	for (e=evtlist.begin(); e!=evtlist.end(); ++e )
	{
		//Remove events in the past
		if ( (u_azeit > (e->startTime + e->duration)) && (!(e->eventID == 0)))
		{
			do
			{
				//printf("%d seconds in the past - deleted %s\n", dif, e->description.c_str());
				e = evtlist.erase( e );
				if (e == evtlist.end())
					break;
			}
			while ( u_azeit > (e->startTime + e->duration));
		}
		if (e == evtlist.end())
			break;

		//Display the remaining events
		if ((y+ theight+ pig_height + i*ffheight) < (y+ theight+ pig_height + infozone_height))
		{
			fb_pixel_t color = COL_MENUCONTENTDARK_TEXT;
			if (e->eventID)
			{
				bool first = (i == 1);
				if ((first && g_settings.theme.colored_events_channellist == 1 /* current */) || (!first && g_settings.theme.colored_events_channellist == 2 /* next */))
					color = COL_COLORED_EVENTS_TEXT;
				struct tm *tmStartZeit = localtime(&e->startTime);
				strftime(startTime, sizeof(startTime), "%H:%M", tmStartZeit );
				//printf("%s %s\n", startTime, e->description.c_str());
				startTimeWidth = eventStartTimeWidth;
				g_Font[eventFont]->RenderString(x+ width+ OFFSET_INNER_MID, y+ theight+ pig_height + i*ffheight, startTimeWidth, startTime, color);
			}
			g_Font[eventFont]->RenderString(x+ width+ OFFSET_INNER_MID +startTimeWidth, y+ theight+ pig_height + i*ffheight, infozone_width - startTimeWidth - 2*OFFSET_INNER_MID, e->description, color);
		}
		else
		{
			break;
		}
		i++;
	}
}

static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}

void CChannelList::readEvents(const t_channel_id channel_id, CChannelEventList &evtlist)
{
	CEitManager::getInstance()->getEventsServiceKey(channel_id , evtlist);

	if ( evtlist.empty() )
	{
		CChannelEvent evt;
		evt.description = g_Locale->getText(LOCALE_EPGLIST_NOEVENTS);
		evt.eventID = 0;
		evt.startTime = 0;
		evtlist.push_back(evt);
	}
	else
		sort(evtlist.begin(),evtlist.end(),sortByDateTime);
}

void CChannelList::showdescription(int index)
{
	std::string strEpisode = "";	// Episode title in case info1 gets stripped
	ffheight = g_Font[eventFont]->getHeight();
	CZapitChannel* chan = (*chanlist)[index];
	CChannelEvent *p_event = &chan->currentEvent;
	epgData.info1.clear();
	epgData.info2.clear();
	epgText.clear();
	CEitManager::getInstance()->getEPGid(p_event->eventID, p_event->startTime, &epgData);

	if (!epgData.info1.empty()) {
		bool bHide = false;
		if (false == epgData.info2.empty()) {
			// Look for the first . in info1, usually this is the title of an episode.
			std::string::size_type nPosDot = epgData.info1.find('.');
			if (std::string::npos != nPosDot) {
				nPosDot += 2; // Skip dot and first blank
				if (nPosDot < epgData.info2.length() && nPosDot < epgData.info1.length()) {   // Make sure we don't overrun the buffer

					// Check if the stuff after the dot equals the beginning of info2
					if (0 == epgData.info2.find(epgData.info1.substr(nPosDot, epgData.info1.length() - nPosDot))) {
						strEpisode = epgData.info1.substr(0, nPosDot) + "\n";
						bHide = true;
					}
				}
			}
			// Compare strings normally if not positively found to be equal before
			if (false == bHide && 0 == epgData.info2.find(epgData.info1)) {
				bHide = true;
			}
		}
		if (false == bHide) {
			processTextToArray(epgData.info1);
		}
	}

	//scan epg-data - sort to list
	if (((epgData.info2.empty())) && (!(strEpisode.empty())))
		processTextToArray(g_Locale->getText(LOCALE_EPGVIEWER_NODETAILED)); // UTF-8
	else
		processTextToArray(strEpisode + epgData.info2); // UTF-8

	frameBuffer->paintBoxRel(x+ width,y+ theight+pig_height, infozone_width, infozone_height,COL_MENUCONTENT_PLUS_0);
	for (int i = 1; (i < (int)epgText.size()+1) && ((y+ theight+ pig_height + i*ffheight) < (y+ theight+ pig_height + infozone_height)); i++)
		g_Font[eventFont]->RenderString(x+ width+5, y+ theight+ pig_height + i*ffheight, infozone_width - 20, epgText[i-1].first, COL_MENUCONTENTDARK_TEXT);
}

void CChannelList::addTextToArray(const std::string & text, int screening) // UTF-8
{
	//printf("line: >%s<\n", text.c_str() );
	if (text==" ")
	{
		emptyLineCount ++;
		if (emptyLineCount<2)
			epgText.push_back(epg_pair(text,screening));
	}
	else
	{
		emptyLineCount = 0;
		epgText.push_back(epg_pair(text,screening));
	}
}

void CChannelList::processTextToArray(std::string text, int screening) // UTF-8
{
	std::string	aktLine = "";
	std::string	aktWord = "";
	int	aktWidth = 0;
	text += ' ';
	const char *text_= text.c_str();

	while (*text_!=0)
	{
		if ( (*text_==' ') || (*text_=='\n') || (*text_=='-') || (*text_=='.') )
		{
			if (*text_!='\n')
				aktWord += *text_;

			int aktWordWidth = g_Font[eventFont]->getRenderWidth(aktWord);
			if ((aktWordWidth+aktWidth)<(infozone_width - 20))
			{//space ok, add
				aktWidth += aktWordWidth;
				aktLine += aktWord;

				if (*text_=='\n')
				{	//enter-handler
					addTextToArray( aktLine, screening );
					aktLine = "";
					aktWidth= 0;
				}
				aktWord = "";
			}
			else
			{//new line needed
				addTextToArray( aktLine, screening);
				aktLine = aktWord;
				aktWidth = aktWordWidth;
				aktWord = "";
				if (*text_=='\n')
					continue;
			}
		}
		else
		{
			aktWord += *text_;
		}
		text_++;
	}
	//add the rest
	addTextToArray( aktLine + aktWord, screening );
}

void CChannelList::saveChanges(bool fav)
{
	g_bouquetManager->renumServices();
	if (fav)
		CNeutrinoApp::getInstance()->MarkFavoritesChanged();
	else
		CNeutrinoApp::getInstance()->MarkBouquetsChanged();
}

void CChannelList::editMode(bool enable)
{
	if (!bouquet || !bouquet->zapitBouquet)
		return;

	displayNext = false;
	edit_state = enable;
	printf("STATE: %s\n", edit_state ? "EDIT" : "SHOW");
	bool tvmode = CZapit::getInstance()->getMode() & CZapitClient::MODE_TV;
	if (edit_state) {
		chanlist = tvmode ? &bouquet->zapitBouquet->tvChannels : &bouquet->zapitBouquet->radioChannels;
	} else {
		chanlist = &channels;
		if (channelsChanged) {
			channelsChanged = false;
			bouquet->zapitBouquet->getChannels(channels, tvmode);
			saveChanges(bouquet->zapitBouquet->bUser);
			if (!g_settings.show_empty_favorites && (*chanlist).empty())
				CNeutrinoApp::getInstance()->MarkChannelsInit();
		}
		if (selected >= chanlist->size())
			selected = chanlist->empty() ? 0 : (chanlist->size() - 1);
	}
	adjustToChannelID(selected_chid);
}

void CChannelList::beginMoveChannel()
{
	move_state = beMoving;
	origPosition = selected;
	newPosition = selected;
}

void CChannelList::finishMoveChannel()
{
	move_state = beDefault;
	paintBody();
}

void CChannelList::cancelMoveChannel()
{
	move_state = beDefault;
	internalMoveChannel(newPosition, origPosition);
	channelsChanged = false;
}

void CChannelList::internalMoveChannel( unsigned int fromPosition, unsigned int toPosition)
{
	if ( (int) toPosition == -1 ) return;
	if ( toPosition == chanlist->size()) return;
	if (!bouquet || !bouquet->zapitBouquet)
		return;

	bool tvmode = CZapit::getInstance()->getMode() & CZapitClient::MODE_TV;
	bouquet->zapitBouquet->moveService(fromPosition, toPosition, tvmode ? 1 : 2);

	channelsChanged = true;
	chanlist = tvmode ? &bouquet->zapitBouquet->tvChannels : &bouquet->zapitBouquet->radioChannels;

	selected = toPosition;
	newPosition = toPosition;
	paintBody();
}

void CChannelList::deleteChannel(bool ask)
{
	if (selected >= chanlist->size())
		return;
	if (!bouquet || !bouquet->zapitBouquet)
		return;

	if (ask && ShowMsg(LOCALE_FILEBROWSER_DELETE, (*chanlist)[selected]->getName(), CMsgBox::mbrNo, CMsgBox::mbYes|CMsgBox::mbNo)!=CMsgBox::mbrYes)
		return;

	bouquet->zapitBouquet->removeService((*chanlist)[selected]->getChannelID());

	bool tvmode = CZapit::getInstance()->getMode() & CZapitClient::MODE_TV;
	chanlist = tvmode ? &bouquet->zapitBouquet->tvChannels : &bouquet->zapitBouquet->radioChannels;

	if (selected >= chanlist->size())
		selected = chanlist->empty() ? 0 : (chanlist->size() - 1);

	channelsChanged = true;
	paintBody();
	updateVfd();
}

void CChannelList::addChannel()
{
	if (!bouquet || !bouquet->zapitBouquet)
		return;

	hide();
	bool tvmode = CZapit::getInstance()->getMode() & CZapitClient::MODE_TV;
	std::string caption = name + ": " + g_Locale->getText(LOCALE_BOUQUETEDITOR_ADD);

        CBEChannelSelectWidget* channelSelectWidget = new CBEChannelSelectWidget(caption, bouquet->zapitBouquet, tvmode ? CZapitClient::MODE_TV : CZapitClient::MODE_RADIO);

        channelSelectWidget->exec(NULL, "");
        if (channelSelectWidget->hasChanged())
        {
                channelsChanged = true;
		chanlist = tvmode ? &bouquet->zapitBouquet->tvChannels : &bouquet->zapitBouquet->radioChannels;
		if (selected >= chanlist->size())
			selected = chanlist->empty() ? 0 : (chanlist->size() - 1);
        }
        delete channelSelectWidget;
	paint();
}

void CChannelList::renameChannel()
{
	std::string newName= inputName((*chanlist)[selected]->getName().c_str(), LOCALE_BOUQUETEDITOR_NEWBOUQUETNAME);

	if (newName != (*chanlist)[selected]->getName())
	{
		if(newName.empty())
			(*chanlist)[selected]->setUserName("");
		else
			(*chanlist)[selected]->setUserName(newName);

		channelsChanged = true;
	}
	paint();
}

void CChannelList::lockChannel()
{
	(*chanlist)[selected]->bLocked = !(*chanlist)[selected]->bLocked;
	CNeutrinoApp::getInstance()->MarkFavoritesChanged();
	if (selected + 1 < (*chanlist).size())
		g_RCInput->postMsg((neutrino_msg_t) CRCInput::RC_down, 0);
	else
		paintItem(selected - liststart);
}

bool CChannelList::addChannelToBouquet()
{
	bool fav_found = true;
	signed int bouquet_id = AllFavBouquetList->exec(false);
	hide();
	if(bouquet_id < 0)
		return false;

	t_channel_id channel_id = (*chanlist)[selected]->getChannelID();
	bool tvmode = CZapit::getInstance()->getMode() & CZapitClient::MODE_TV;
	CBouquetList *blist = tvmode ? TVfavList : RADIOfavList;
	if (AllFavBouquetList->Bouquets[bouquet_id]->zapitBouquet) {
		CZapitBouquet *zapitBouquet = AllFavBouquetList->Bouquets[bouquet_id]->zapitBouquet;
		CZapitChannel *ch = zapitBouquet->getChannelByChannelID(channel_id);
		if (ch == NULL) {
			fav_found = false;
			zapitBouquet->addService((*chanlist)[selected]);
			for (unsigned n = 0; n < blist->Bouquets.size(); n++) {
				if (blist->Bouquets[n]->zapitBouquet == zapitBouquet) {
					zapitBouquet->getChannels(blist->Bouquets[n]->channelList->channels, tvmode);
					saveChanges();
					fav_found = true;
					break;
				}
			}
		} else {
			ShowMsg(LOCALE_EXTRA_ADD_TO_BOUQUET, LOCALE_EXTRA_CHALREADYINBQ, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
			return false;
		}
	}
	if (!fav_found) {
		CNeutrinoApp::getInstance()->MarkFavoritesChanged();
		CNeutrinoApp::getInstance()->MarkChannelsInit();
	}
	return true;
}

void CChannelList::moveChannelToBouquet()
{
	if (addChannelToBouquet())
		deleteChannel(false);
	else
		paintBody();

	paintHead();
}

std::string CChannelList::inputName(const char * const defaultName, const neutrino_locale_t caption)
{
	std::string Name = defaultName;

	CKeyboardInput * nameInput = new CKeyboardInput(caption, &Name);
	nameInput->exec(NULL, "");

	delete nameInput;

	return std::string(Name);
}
