/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009-2016 Stefan Seyfried

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

#include <global.h>
#include <neutrino.h>
#include <gui/eventlist.h>
#include <gui/epgplus.h>
#include <gui/epgview.h>
#include <gui/followscreenings.h>
#include <gui/moviebrowser/mb.h>
#include <gui/timerlist.h>
#include <gui/user_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/mountchooser.h>
#include <gui/pictureviewer.h>

#include "widget/hintbox.h"
#include "widget/buttons.h"
#include "bouquetlist.h"
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>

#include <driver/display.h>
#include <driver/screen_max.h>
#include <driver/fade.h>
#include <driver/record.h>
#include <driver/fontrenderer.h>

#include <system/helpers.h>
#include <zapit/client/zapittools.h>
#include <zapit/zapit.h>
#include <daemonc/remotecontrol.h>
#include <eitd/sectionsd.h>
#include <timerdclient/timerdclient.h>

#include <algorithm>

extern CBouquetList        * bouquetList;
extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */

extern CPictureViewer * g_PicViewer;

#if 0
// sort operators
bool sortById (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.eventID < b.eventID ;
}

inline static bool sortbyEventid (const CChannelEvent& a, const CChannelEvent& b)
{
	return (a.channelID == b.channelID && a.eventID == b.eventID && a.startTime == b.startTime); 
}
#endif

inline bool sortByDescription (const CChannelEvent& a, const CChannelEvent& b)
{
	if(a.description == b.description)
		return a.startTime < b.startTime;
	else
		return a.description < b.description ;
}
inline static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}

CEventList::CEventList()
{
	frameBuffer = CFrameBuffer::getInstance();
	selected = 0;
	current_event = 0;
	liststart = 0;
	sort_mode = 0;

	m_search_list = SEARCH_LIST_NONE;
	m_search_epg_item = SEARCH_EPG_TITLE;
	m_search_channel_id = 1;
	m_search_bouquet_id= 1;
	m_search_genre = 1;
	m_search_fsk = 1;
	full_width = width = 0;
	height = 0;
	x = y = 0;
	
	infozone = NULL;
	infozone_text = "";
	item_event_ID = 0;
	oldIndex = -1;
	oldEventID = -1;
	infozone_background = false;
	header = NULL;
	pb = NULL;
	navibar = NULL;
}

CEventList::~CEventList()
{
	ResetModules();
}

void CEventList::ResetModules()
{
	if (header){
		delete header; header = NULL;
	}
	if (navibar){
		delete navibar; navibar = NULL;
	}
	if (pb){
		delete pb; pb = NULL;
	}
}

void CEventList::UpdateTimerList(void)
{
	timerlist.clear();
	g_Timerd->getTimerList (timerlist);
	g_Timerd->getRecordingSafety(timerPre,timerPost);
}

// Function: HasTimerConflicts
// search for timer conflicts for given time 
// return: true if found any conflict, you can watch with parameter epg_ID
bool CEventList::HasTimerConflicts(time_t starttime, time_t duration, event_id_t * epg_ID)
{	
	for(uint i= 0; i < timerlist.size(); i++)
		
	{			
		if(timerlist[i].stopTime > starttime-timerPre && timerlist[i].alarmTime < starttime+duration+timerPost)
		{
			*epg_ID = timerlist[i].epgID;
			return true;
		}
	}
	
	*epg_ID = 0;
	return  false;
}

void CEventList::readEvents(const t_channel_id channel_id)
{
	CEitManager::getInstance()->getEventsServiceKey(channel_id , evtlist);
	time_t azeit=time(NULL);

	CChannelEventList::iterator e;
	if ( !evtlist.empty() ) {

		CEPGData epgData;
		// todo: what if there are more than one events in the Portal
		if (CEitManager::getInstance()->getActualEPGServiceKey(channel_id, &epgData ))
		{
//			epgData.eventID;
//			epgData.epg_times.startzeit;
			CSectionsdClient::LinkageDescriptorList	linkedServices;
			if (CEitManager::getInstance()->getLinkageDescriptorsUniqueKey( epgData.eventID, linkedServices ) )
			{
				if ( linkedServices.size()> 1 )
				{
					CChannelEventList evtlist2; // stores the temporary eventlist of the subchannel channelid
					t_channel_id channel_id2;
#if 0
					for (e=evtlist.begin(); e!=evtlist.end(); ++e )
					{
						if ( e->startTime > azeit ) {
							break;
						}
					}
					// next line is to have a valid e
					if (evtlist.end() == e) --e;
#endif
					for (unsigned int i=0; i<linkedServices.size(); i++)
					{
						channel_id2 = CREATE_CHANNEL_ID(
								linkedServices[i].serviceId,
								linkedServices[i].originalNetworkId,
								linkedServices[i].transportStreamId);

						// do not add parent events
						if (channel_id != channel_id2) {
							CEitManager::getInstance()->getEventsServiceKey(channel_id2 , evtlist2);

							for (unsigned int loop=0 ; loop<evtlist2.size(); loop++ )
							{
								// check if event is in the range of the portal parent event
#if 0
								if ( (evtlist2[loop].startTime >= azeit) /*&&
								     (evtlist2[loop].startTime < e->startTime + (int)e->duration)*/ )
#endif
								{
									//FIXME: bad ?evtlist2[loop].sub = true;
									evtlist.push_back(evtlist2[loop]);
								}
							}
							evtlist2.clear();
						}
					}
				}
			}
		}
		// Houdini added for Private Premiere EPG, start sorted by start date/time
		sort(evtlist.begin(),evtlist.end(),sortByDateTime);

		timerlist.clear();
		g_Timerd->getTimerList (timerlist);
	}

	current_event = (unsigned int)-1;
	for ( e=evtlist.begin(); e!=evtlist.end(); ++e )
	{
		if ( e->startTime > azeit ) {
			break;
		}
		current_event++;
	}

	if ( evtlist.empty() )
	{
		CChannelEvent evt;

		evt.description = g_Locale->getText(LOCALE_EPGLIST_NOEVENTS);
		evt.eventID = 0;
		evt.channelID = 0;
		evt.startTime = 0;
		evt.duration = 0;
		evtlist.push_back(evt);

	}
	if (current_event == (unsigned int)-1)
		current_event = 0;
	selected= current_event;

	return;
}

void CEventList::getChannelNames(t_channel_id &channel_id, std::string &current_channel_name, std::string &prev_channel_name, std::string &next_channel_name, neutrino_msg_t msg)
{
	t_bouquet_id current_bouquet_id = bouquetList->getActiveBouquetNumber();
	const unsigned int channel_nr = bouquetList->Bouquets[current_bouquet_id]->channelList->getSize();
	if(channel_nr < 2){
		channel_id = 0;
		return;
	}
	unsigned int tmp_channel = 0;
	for(unsigned int channel = 0; channel < channel_nr; channel++)
	{
		t_channel_id channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(channel)->getChannelID();
		if(channel_id_tmp == channel_id){
			if ( msg==CRCInput::RC_right || msg==CRCInput::RC_forward ) {
				channel = (channel+1) %channel_nr;
			}else if ( msg==CRCInput::RC_left || msg==CRCInput::RC_rewind ){ //RC_rewind
				channel = (channel == 0) ? channel_nr -1 : channel - 1;
			}
			channel_id = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(channel)->getChannelID();
			current_channel_name = CServiceManager::getInstance()->GetServiceName(channel_id);

			tmp_channel = (channel == 0) ? channel_nr - 1 : channel - 1;
			channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(tmp_channel)->getChannelID();
			prev_channel_name = CServiceManager::getInstance()->GetServiceName(channel_id_tmp);

			tmp_channel = (channel+1) %channel_nr;
			channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(tmp_channel)->getChannelID();
			next_channel_name = CServiceManager::getInstance()->GetServiceName(channel_id_tmp);
			break;
		}
	}
}

int CEventList::exec(const t_channel_id channel_id, const std::string& channelname, const std::string& channelname_prev, const std::string& channelname_next,const CChannelEventList &followlist) // UTF-8
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool in_search = false;
	showfollow = false;
	t_channel_id epg_id = channel_id;

	CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(channel_id);
	if (ch)
		epg_id = ch->getEpgID();

	full_width = frameBuffer->getScreenWidthRel();
	x = getScreenStartX(full_width);

	if (g_settings.eventlist_additional)
		width = full_width / 3 * 2;
	else
		width = full_width;
	height = frameBuffer->getScreenHeightRel();

	// Calculate header_height
	header_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	footer_height = header_height;
	const int pic_h = 39;
	header_height = std::max(header_height, pic_h);

	largefont_height = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->getHeight();
	{
		int h1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getHeight();
		int h2 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->getHeight();
		smallfont_height = std::max(h1, h2);
	}
	unit_short_minute = g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE);
	item_height = smallfont_height + OFFSET_INNER_MIN + largefont_height;

	navibar_height = largefont_height+2*OFFSET_INNER_MIN;

	listmaxshow = (height - header_height - footer_height - OFFSET_SHADOW - navibar_height)/item_height;
	height = header_height + footer_height + OFFSET_SHADOW + navibar_height + listmaxshow*item_height; // recalc height
	y = getScreenStartY(height);

	// calculate width of right info_zone
	infozone_width = full_width - width;

	// init right info_zone
	if ((g_settings.eventlist_additional) && (infozone == NULL))
		infozone = new CComponentsText(x+width+OFFSET_INNER_MID, y + header_height, infozone_width-2*OFFSET_INNER_MID, listmaxshow*item_height);

	int res = menu_return::RETURN_REPAINT;
	//printf("CEventList::exec: channel_id %llx\n", channel_id);
	if(m_search_list == SEARCH_LIST_NONE) // init globals once only
	{
		m_search_epg_item = SEARCH_EPG_TITLE;
		//m_search_keyword = "";
		m_search_list = SEARCH_LIST_CHANNEL;
		//m_search_channel_id = channel_id;
		m_search_bouquet_id= bouquetList->getActiveBouquetNumber();
		//m_search_source_text = "";
	}
	m_search_channel_id = channel_id;
	m_showChannel = false; // do not show the channel in normal mode, we just need it in search mode

	sort_mode=0;

	COSDFader fader(g_settings.theme.menu_Content_alpha);
	fader.StartFadeIn();
	if(!followlist.empty()){
		insert_iterator <std::vector<CChannelEvent> >ii(evtlist,evtlist.begin());
		copy(followlist.begin(), followlist.end(), ii);
		showfollow = true;
	}else{
		readEvents(epg_id);
	}
	UpdateTimerList();

	bool dont_hide = false;
	paintHead(channel_id, channelname, channelname_prev, channelname_next);
	paint(channel_id);
	paintFoot(channel_id);

	int oldselected = selected;

	int timeout = g_settings.timing[SNeutrinoSettings::TIMING_EPG];
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);

		if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer())) {
			if(fader.FadeDone())
				loop = false;
		}
		else if (msg == CRCInput::RC_timeout)
		{
			selected = oldselected;
			if(fader.StartFadeOut()) {
				timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
				msg = 0;
			} else
				loop = false;
		}
		
		//manage painting of function bar during scrolling, depends of timerevent types
		if (msg == CRCInput::RC_up || (int) msg == g_settings.key_pageup || 
			msg == CRCInput::RC_down || (int) msg == g_settings.key_pagedown)
		{
			int prev_selected = selected;
			int new_sel = UpDownKey(evtlist, msg, listmaxshow, selected);
			if (new_sel >= 0) {
				selected = new_sel;
				paintDescription(selected);
			}
			paintItem(prev_selected - liststart, channel_id);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;

			if(oldliststart!=liststart)
				paint(channel_id);
			else
				paintItem(selected - liststart, channel_id);

			paintFoot(channel_id);
		}
		//sort
		else if (!showfollow && (msg == (neutrino_msg_t)g_settings.key_channelList_sort))
		{
			uint64_t selected_id = evtlist[selected].eventID;
			if(sort_mode==0)
			{
				sort_mode++;
				sort(evtlist.begin(),evtlist.end(),sortByDescription);
			}
#if 0
			else if(sort_mode==1)
			{
				sort_mode++;
				sort(evtlist.begin(),evtlist.end(),sortById);
			}
#endif
			else
			{
				sort_mode=0;
				sort(evtlist.begin(),evtlist.end(),sortByDateTime);
			}
			// find selected
			for ( selected=0 ; selected < evtlist.size(); selected++ )
			{
				if ( evtlist[selected].eventID == selected_id )
					break;
			}
			oldselected=selected;
			if(selected <=listmaxshow)
				liststart=0;
			else
				liststart=(selected/listmaxshow)*listmaxshow;
			paint(channel_id);
		}

		//  -- I commented out the following part (code is working)
		//  -- reason: this is a little bit confusing, because e.g. you can enter the function
		//  -- with RED, but pressing RED doesn't leave - it triggers a record timer instead
		//  -- I think it's sufficient, to press RIGHT or HELP to get movie details and then
		//  -- press "auto record" or "auto switch"  (rasc 2003-06-28)
		//  --- hm, no need to comment out that part, leave the decision to the user
		//  --- either set addrecord timer key to "no key" and leave eventlist with red (default now),
		//  --- or set addrecord timer key to "red key" (zwen 2003-07-29)

		else if ((msg == (neutrino_msg_t)g_settings.key_channelList_addrecord) && !g_settings.minimode)
		{
			if (g_settings.recording_type != CNeutrinoApp::RECORDING_OFF)
			{
				int tID = -1;
				CTimerd::CTimerEventTypes etype = isScheduled(evtlist[selected].channelID, &evtlist[selected], &tID);
				if(etype == CTimerd::TIMER_RECORD) //remove timer event 
				{
					g_Timerd->removeTimerEvent(tID);
					timerlist.clear();
					g_Timerd->getTimerList (timerlist);
					paint(evtlist[selected].channelID);
					paintFoot(evtlist[selected].channelID);
					continue;
				}
				std::string recDir = g_settings.network_nfs_recordingdir;
				if (g_settings.recording_choose_direct_rec_dir)
				{
					int id = -1;
					CMountChooser recDirs(LOCALE_TIMERLIST_RECORDING_DIR,NEUTRINO_ICON_SETTINGS,&id,NULL,g_settings.network_nfs_recordingdir.c_str());
					if (recDirs.hasItem())
					{
						hide();
						recDirs.exec(NULL,"");
						paint(evtlist[selected].channelID);
						timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);
					} 
					else
					{
						printf("[CEventList] no network devices available\n");
					}
					
					if (id != -1)
						recDir = g_settings.network_nfs[id].local_dir;
					else
						recDir = "";
				}
				bool doRecord = true;
				if (g_settings.recording_already_found_check)
				{
					CHintBox loadBox(LOCALE_RECORDING_ALREADY_FOUND_CHECK, LOCALE_MOVIEBROWSER_SCAN_FOR_MOVIES);
					loadBox.paint();
					CMovieBrowser moviebrowser;
					const char *rec_title = evtlist[selected].description.c_str();
					bool already_found = moviebrowser.gotMovie(rec_title);
					loadBox.hide();
					if (already_found)
					{
						printf("already found in moviebrowser: %s\n", rec_title);
						char message[1024];
						snprintf(message, sizeof(message)-1, g_Locale->getText(LOCALE_RECORDING_ALREADY_FOUND), rec_title);
						doRecord = (ShowMsg(LOCALE_RECORDING_ALREADY_FOUND_CHECK, message, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo) == CMsgBox::mbrYes);
					}
				}
				t_channel_id used_id = IS_WEBTV(channel_id) ? channel_id : evtlist[selected].channelID;
				if (!recDir.empty() && doRecord) //add/remove recording timer events and check/warn for conflicts
				{
					CFollowScreenings m(used_id,
						evtlist[selected].startTime,
						evtlist[selected].startTime + evtlist[selected].duration,
						evtlist[selected].description, evtlist[selected].eventID, TIMERD_APIDS_CONF, true, "", &evtlist);
					m.exec(NULL, "");
					timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);
				}
				timerlist.clear();
				g_Timerd->getTimerList (timerlist);
				paint(used_id);
				paintFoot(used_id);
			}
		}
		else if ( msg == (neutrino_msg_t) g_settings.key_channelList_addremind )//add/remove zapto timer event
		{
			int tID = -1;
			CTimerd::CTimerEventTypes etype = isScheduled(evtlist[selected].channelID, &evtlist[selected], &tID);
			if(etype == CTimerd::TIMER_ZAPTO) {
				g_Timerd->removeTimerEvent(tID);
				timerlist.clear();
				g_Timerd->getTimerList (timerlist);
				paint(evtlist[selected].channelID);
				paintFoot(evtlist[selected].channelID);
				continue;
			}
			
			g_Timerd->addZaptoTimerEvent(IS_WEBTV(channel_id) ? channel_id : evtlist[selected].channelID,
					evtlist[selected].startTime - (g_settings.zapto_pre_time * 60),
					evtlist[selected].startTime - ANNOUNCETIME - (g_settings.zapto_pre_time * 60), 0,
					evtlist[selected].eventID, evtlist[selected].startTime, 0);
			//ShowMsg(LOCALE_TIMER_EVENTTIMED_TITLE, LOCALE_TIMER_EVENTTIMED_MSG, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
			timerlist.clear();
			g_Timerd->getTimerList (timerlist);
			paint(evtlist[selected].channelID );
			paintFoot(evtlist[selected].channelID );
			timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);
		}
		else if (msg == (neutrino_msg_t)g_settings.key_channelList_cancel)
		{
			if(in_search) {
				in_search = false;
				m_showChannel = false;
				paintHead(channel_id, channelname);
				readEvents(epg_id);
				paint(channel_id);
				paintFoot(channel_id);
			} else {
				selected = oldselected;
				if(fader.StartFadeOut()) {
					timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
					msg = 0;
				} else
					loop = false;
			}
		}
		else if ( msg==CRCInput::RC_red ){
			loop= false;
		}
		else if ( msg==CRCInput::RC_left || msg==CRCInput::RC_right || msg==CRCInput::RC_rewind || msg==CRCInput::RC_forward ) {
			// maybe remove RC_rewind and RC_forward in the future?
			std::string next_channel_name, prev_channel_name, current_channel_name;
			t_channel_id _channel_id = channel_id;
			getChannelNames(_channel_id, current_channel_name, prev_channel_name, next_channel_name, msg);
			if(_channel_id){
				infozone_background = false;
				loop = false;
				dont_hide = true;
				exec(_channel_id, current_channel_name, prev_channel_name, next_channel_name);
			}
		}
		else if (msg == CRCInput::RC_0) {
			hide();

			CTimerList *Timerlist = new CTimerList;
			Timerlist->exec(NULL, "");
			delete Timerlist;
			timerlist.clear();
			g_Timerd->getTimerList (timerlist);

			paintHead(channel_id, channelname);
			oldIndex = -1;
			oldEventID = -1;
			infozone_background = false;
			paint(channel_id);
			paintFoot(channel_id);
			timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);
		}
		else if (msg == CRCInput::RC_epg)
		{
			if (g_settings.eventlist_epgplus)
			{
				hide();
				CEPGplusHandler eplus;
				eplus.exec(NULL, "");
				loop = false;
			}
		}
		else if (msg==CRCInput::RC_help || msg==CRCInput::RC_ok || msg==CRCInput::RC_info)
		{
			if ( evtlist[selected].eventID != 0 )
			{
				hide();

				//FIXME res = g_EpgData->show(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id, evtlist[selected].eventID, &evtlist[selected].startTime);
				res = g_EpgData->show(evtlist[selected].channelID, evtlist[selected].eventID, &evtlist[selected].startTime, true, showfollow );
				if ( res == menu_return::RETURN_EXIT_ALL )
				{
					loop = false;
				}
				else
				{
					//FIXME why getMsg ?
					g_RCInput->getMsg( &msg, &data, 0 );

					if ( ( msg != CRCInput::RC_red ) && ( msg != CRCInput::RC_timeout ) )
					{
						// RC_red schlucken
						g_RCInput->postMsg( msg, data );
					}
					/* in case timer was added in g_EpgData */
					timerlist.clear();
					g_Timerd->getTimerList (timerlist);
					paintHead(channel_id, in_search ? search_head_name : channelname);
					oldIndex = -1;
					oldEventID = -1;
					infozone_background = false;
					paint(channel_id);
					paintFoot(channel_id);
					timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);
				}
			}
		}
		else if (!showfollow  && ( msg==CRCInput::RC_green ))
		{
			oldIndex = -1;
			oldEventID = -1;
			infozone_background = false;
			in_search = findEvents(channel_id, channelname);
			timeoutEnd = CRCInput::calcTimeoutEnd(timeout == 0 ? 0xFFFF : timeout);
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			g_RCInput->postMsg (msg, 0);
			res = menu_return::RETURN_EXIT_ALL;
			loop = false;
		}
		else if (msg == NeutrinoMessages::EVT_SERVICESCHANGED || msg == NeutrinoMessages::EVT_BOUQUETSCHANGED) {
			g_RCInput->postMsg(msg, data);
			loop = false;
			res = menu_return::RETURN_EXIT_ALL;
		}
		else
		{
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
			{
				loop = false;
				res = menu_return::RETURN_EXIT_ALL;
			}
		}
	}

	if (infozone)
		delete infozone;
	infozone = NULL;
	oldIndex = -1;
	oldEventID = -1;
	infozone_background = false;

	if(!dont_hide){
		hide();
		fader.StopFade();
	}
	return res;
}

void CEventList::hide()
{
	ResetModules();
	frameBuffer->paintBackgroundBoxRel(x, y, full_width + OFFSET_SHADOW, height + OFFSET_SHADOW);
}

CTimerd::CTimerEventTypes CEventList::isScheduled(t_channel_id channel_id, CChannelEvent * event, int * tID)
{
	CTimerd::TimerList::iterator timer = timerlist.begin();
	for(; timer != timerlist.end(); ++timer) {
		if(timer->channel_id == channel_id && (timer->eventType == CTimerd::TIMER_ZAPTO || timer->eventType == CTimerd::TIMER_RECORD)) {
			if(timer->epgID == event->eventID) {
				if(timer->epg_starttime == event->startTime) {
					bool isTimeShiftTimer = false;
					if( timer->eventType == CTimerd::TIMER_RECORD){
						isTimeShiftTimer = CRecordManager::getInstance()->CheckRecordingId_if_Timeshift(timer->eventID);
					}
					if(tID)
						*tID = timer->eventID;
					if(isTimeShiftTimer)//skip TSHIFT RECORD
						continue;

					return timer->eventType;
				}
			}
		}
	}
	return (CTimerd::CTimerEventTypes) 0;
}

void CEventList::paintItem(unsigned int pos, t_channel_id channel_idI)
{
	int ypos = y + header_height + pos*item_height;
	unsigned int currpos = liststart + pos;

	bool i_selected	= currpos == selected;
	bool i_marked	= currpos == current_event;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected, i_marked);

	if (i_selected || i_marked)
		i_radius = RADIUS_LARGE;

	if (i_radius)
		frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, ypos, width - SCROLLBAR_WIDTH, item_height, bgcolor, i_radius);

	if(currpos<evtlist.size())
	{
		std::string datetime_str, duration_str;
		if ( evtlist[currpos].eventID != 0 )
		{
			char tmpstr[256];
			struct tm *tmStartZeit = localtime(&evtlist[currpos].startTime);

			datetime_str = g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));
			datetime_str += strftime(", %H:%M", tmStartZeit);
			datetime_str += strftime(", %d", tmStartZeit);
			datetime_str += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));

			if ( m_showChannel ) // show the channel if we made a event search only (which could be made through all channels ).
			{
				t_channel_id channel = evtlist[currpos].channelID;
				datetime_str += "      ";
				datetime_str += CServiceManager::getInstance()->GetServiceName(channel);
			}

			snprintf(tmpstr,sizeof(tmpstr), "[%d %s]", evtlist[currpos].duration / 60, unit_short_minute);
			duration_str = tmpstr;
		}

		// 1st line
		int datetime_width = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->getRenderWidth(datetime_str);
		int duration_width = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getRenderWidth(duration_str);

		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->RenderString(x + OFFSET_INNER_SMALL, ypos + OFFSET_INNER_MIN + smallfont_height, datetime_width, datetime_str, color);

		int seit = ( evtlist[currpos].startTime - time(NULL) ) / 60;
		if ( (seit> 0) && (seit<100) && (!duration_str.empty()) )
		{
			char beginnt[100];
			snprintf(beginnt, sizeof(beginnt), "%s %d %s", g_Locale->getText(LOCALE_WORD_IN), seit, unit_short_minute);
			int w = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getRenderWidth(beginnt);
			g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->RenderString(x + width - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID - duration_width - w, ypos + OFFSET_INNER_MIN + smallfont_height, w, beginnt, color);
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->RenderString(x + width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - duration_width, ypos + OFFSET_INNER_MIN + smallfont_height, duration_width, duration_str, color);
		
		// 2nd line
		// set status icons
		t_channel_id channel_tmp = m_showChannel ? evtlist[currpos].channelID : channel_idI;
		int timerID = -1;
		CTimerd::CTimerEventTypes etype = isScheduled(channel_tmp, &evtlist[currpos],&timerID);
		const char * icontype = etype == CTimerd::TIMER_ZAPTO ? NEUTRINO_ICON_ZAP : 0;
		if(etype == CTimerd::TIMER_RECORD){
			icontype = NEUTRINO_ICON_REC;// NEUTRINO_ICON_RECORDING_EVENT_MARKER
		}else{
			if (timerID > 0 && CRecordManager::getInstance()->CheckRecordingId_if_Timeshift(timerID))
				icontype = NEUTRINO_ICON_AUTO_SHIFT;
		}
		int iw = 0, ih = 0;
		if(icontype != 0) {
			frameBuffer->getIconSize(icontype, &iw, &ih);
			frameBuffer->paintIcon(icontype, x + OFFSET_INNER_MID, ypos + OFFSET_INNER_MIN + smallfont_height, largefont_height);
			iw += OFFSET_INNER_MID;
		}
		
		// detecting timer conflict and set start position of event text depending of possible painted icon
		bool conflict = HasTimerConflicts(evtlist[currpos].startTime, evtlist[currpos].duration, &item_event_ID);
		//printf ("etype %d , conflicts %d -> %s, conflict event_ID %d -> current event_ID %d\n", etype, conflict, evtlist[currpos].description.c_str(), item_event_ID, evtlist[currpos].eventID);
		
		int i2w = 0, i2h = 0;
		if (conflict && item_event_ID != evtlist[currpos].eventID)
		{	
			//paint_warning = true;
			frameBuffer->getIconSize(NEUTRINO_ICON_IMPORTANT, &i2w, &i2h);
			frameBuffer->paintIcon(NEUTRINO_ICON_IMPORTANT, x + iw + OFFSET_INNER_MID, ypos + OFFSET_INNER_MIN + smallfont_height, largefont_height);
			iw += i2w + OFFSET_INNER_MID;
		}
		
		// paint 2nd line text
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->RenderString(x + iw + OFFSET_INNER_MID, ypos + item_height, width - iw - 2*OFFSET_INNER_MID, evtlist[currpos].description, color);

		showProgressBar(currpos);
	}
}

void CEventList::paintDescription(int index)
{
	if (!g_settings.eventlist_additional)
		return;

	if (evtlist[index].eventID == oldEventID) {
		if (oldEventID == 0) {
			if (index == oldIndex)
				return;
		}
		else
			return;
	}
	oldEventID = evtlist[index].eventID;
	oldIndex = index;

	CEPGData epgData;
	if ( evtlist[index].eventID != 0 )
		CEitManager::getInstance()->getEPGid(evtlist[index].eventID, evtlist[index].startTime, &epgData);
	else
		CEitManager::getInstance()->getActualEPGServiceKey(evtlist[index].channelID, &epgData );

	infozone_text = "";
	if (!epgData.info1.empty() && !epgData.info2.empty() && (epgData.info2.find(epgData.info1) != 0)) {
		infozone_text += epgData.info1;
		infozone_text += "\n";
		infozone_text += epgData.info2;
	}
	else
	if(!epgData.info2.empty()){
		infozone_text = epgData.info2;
	}
	else if (!epgData.info1.empty()){
		infozone_text = epgData.info1;
	}
	else
		infozone_text = g_Locale->getText(LOCALE_EPGLIST_NOEVENTS);

	infozone->setText(infozone_text, CTextBox::TOP, g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_EVENT]);
	infozone->doPaintBg(false);
	infozone->doPaintTextBoxBg(true);
	//FIXME infozone->enableShadow(CC_SHADOW_RIGHT, -1, true);
	infozone->forceTextPaint();
	infozone->paint(CC_SAVE_SCREEN_NO);
}

void CEventList::paintHead(t_channel_id _channel_id, std::string _channelname, std::string _channelname_prev, std::string _channelname_next)
{
	if (header == NULL){
		header = new CComponentsHeader();
		header->getTextObject()->enableTboxSaveScreen(g_settings.theme.menu_Head_gradient);//enable screen save for title text if color gradient is in use
		header->enableClock(true, "%H:%M", "%H %M", true);
		header->enableColBodyGradient(g_settings.theme.menu_Head_gradient, COL_MENUCONTENT_PLUS_0, g_settings.theme.menu_Head_gradient_direction);
		header->setDimensionsAll(x, y, full_width, header_height);
		header->enableShadow(CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT, -1, true);
	}
	//header->getClockObject()->setCorner(RADIUS_LARGE, CORNER_TOP_RIGHT);

	if (header->isPainted())
		header->getChannelLogoObject()->hide();
	if (g_settings.channellist_show_channellogo)
		header->setChannelLogo(_channel_id,_channelname);
	header->setCaption(_channelname, CCHeaderTypes::CC_TITLE_LEFT);

	header->paint(CC_SAVE_SCREEN_NO);

	if(_channelname_prev.empty() && _channelname_next.empty()){
		getChannelNames(_channel_id, _channelname, _channelname_prev, _channelname_next, 0);
	}

	paintNaviBar(_channelname_prev, _channelname_next);
}

void CEventList::paintNaviBar(std::string _channelname_prev, std::string _channelname_next)
{
	int navibar_y = y + height - OFFSET_SHADOW - footer_height - navibar_height;

	if (!navibar)
	{
		navibar = new CNaviBar(x, navibar_y, full_width, navibar_height);
		navibar->setFont(g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]);
		//FIXME navibar->enableShadow(CC_SHADOW_RIGHT, -1, true);
	}

	navibar->enableArrows(!_channelname_prev.empty(), !_channelname_next.empty());
	navibar->setText(_channelname_prev, _channelname_next);

	navibar->paint(false);

	// shadow
	frameBuffer->paintBoxRel(x + full_width, navibar_y + OFFSET_SHADOW, OFFSET_SHADOW, navibar_height, COL_SHADOW_PLUS_0);
}

void CEventList::showProgressBar(int pos)
{
	int epg_done= -1;
	if ((( time(NULL)- evtlist[pos].startTime )>= 0 ) && (evtlist[pos].duration > 0))
	{
		unsigned nProcentagePassed = ((time(NULL) - evtlist[pos].startTime) * 100 / evtlist[pos].duration);
		if (nProcentagePassed<= 100)
			epg_done= nProcentagePassed;
	}

	if (!pb)
	{
		int pbw = full_width/10;
		int pbx = x + (full_width - pbw)/2;
		int pbh = navibar_height - 2*OFFSET_INNER_SMALL;
		int pby = y + height - OFFSET_SHADOW - footer_height - navibar_height + (navibar_height - pbh)/2;

		pb = new CProgressBar(pbx, pby, pbw, pbh);
		pb->setType(CProgressBar::PB_TIMESCALE);
		pb->doPaintBg(false);
	}

	//show progressbar
	if (epg_done > 0)
	{
		pb->setValues(epg_done, 100);
		pb->paint(true);
	}
	else
	{
		pb->hide();
	}

}

void CEventList::paint(t_channel_id channel_id)
{
	liststart = (selected/listmaxshow)*listmaxshow;

	// paint background for right box
	if (g_settings.eventlist_additional && !infozone_background)
	{
		frameBuffer->paintBoxRel(x + width,y + header_height, infozone_width, item_height*listmaxshow, COL_MENUCONTENT_PLUS_0);
		infozone_background = true;
	}

	for(unsigned int count=0; count < listmaxshow; count++)
	{
		paintItem(count, channel_id);
	}

	// paint content for right box
	paintDescription(selected);

	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, evtlist.size(), listmaxshow, selected);
	paintScrollBar(x + width - SCROLLBAR_WIDTH, y + header_height, SCROLLBAR_WIDTH, item_height*listmaxshow, total_pages, current_page);

	//FIXME shadow
	frameBuffer->paintBoxRel(x + full_width, y + header_height + OFFSET_SHADOW, OFFSET_SHADOW, item_height*listmaxshow, COL_SHADOW_PLUS_0);
}

void CEventList::paintFoot(t_channel_id channel_id)
{
	CColorKeyHelper keyhelper; //user_menue.h
	neutrino_msg_t dummy = CRCInput::RC_nokey;
	const char * icon = NULL;
	struct button_label buttons[5]; //TODO dbt: add directly into footer object with setButtonLabels()
	int btn_cnt = 0;

	int tID = -1; //any value, not NULL
	CTimerd::CTimerEventTypes is_timer = isScheduled(channel_id, &evtlist[selected], &tID);

	// -- Button: Timer Record & Channelswitch
	if ((g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) && ((uint) g_settings.key_channelList_addrecord != CRCInput::RC_nokey)) {	
		if (!g_settings.minimode) {
			// FIXME : display other icons depending on g_settings.key_channelList_addrecord
			keyhelper.get(&dummy, &icon, g_settings.key_channelList_addrecord);
			buttons[btn_cnt].button = icon;
			if (is_timer == CTimerd::TIMER_RECORD)
				buttons[btn_cnt].locale = LOCALE_TIMERLIST_DELETE;
			else
				buttons[btn_cnt].locale = LOCALE_EVENTLISTBAR_RECORDEVENT;
			btn_cnt++;
		}
	}

	if(!showfollow){
		buttons[btn_cnt].button = NEUTRINO_ICON_BUTTON_GREEN;
		buttons[btn_cnt].locale = LOCALE_EVENTFINDER_SEARCH; // search button
		btn_cnt++;
	}

	// Button: Timer Channelswitch
	if ((uint) g_settings.key_channelList_addremind != CRCInput::RC_nokey) {
		if (!g_settings.minimode) {
			// FIXME : display other icons depending on g_settings.key_channelList_addremind
			keyhelper.get(&dummy, &icon, g_settings.key_channelList_addremind);
			buttons[btn_cnt].button = icon;
			if (is_timer == CTimerd::TIMER_ZAPTO)
				buttons[btn_cnt].locale = LOCALE_TIMERLIST_DELETE;
			else
				buttons[btn_cnt].locale = LOCALE_EVENTLISTBAR_CHANNELSWITCH;
			btn_cnt++;
		}
	}

	if(!showfollow){
		// Button: Event Re-Sort
		if ((uint) g_settings.key_channelList_sort != CRCInput::RC_nokey) {
			// FIXME : display other icons depending on g_settings.key_channelList_sort
			keyhelper.get(&dummy, &icon, g_settings.key_channelList_sort);
			buttons[btn_cnt].button = icon;
			buttons[btn_cnt].locale = LOCALE_EVENTLISTBAR_EVENTSORT;
			btn_cnt++;
		}
	}

	// info button
	if (evtlist[0].eventID != 0) {
		buttons[btn_cnt].button = NEUTRINO_ICON_BUTTON_INFO_SMALL;
		buttons[btn_cnt].locale = LOCALE_EPGMENU_EVENTINFO;
		btn_cnt++;
	}

#if 0
	buttons[btn_cnt].button = NEUTRINO_ICON_BUTTON_INFO_SMALL;
	buttons[btn_cnt].locale = LOCALE_EPGPLUS_HEAD;
	btn_cnt++;

	buttons[btn_cnt].button = NEUTRINO_ICON_BUTTON_0;
	buttons[btn_cnt].locale = LOCALE_TIMERLIST_NAME;
	btn_cnt++;
#endif

	CComponentsFooter footer;
	footer.enableShadow(CC_SHADOW_ON, -1, true);
	footer.paintButtons(x, y + height - OFFSET_SHADOW - footer_height, full_width, footer_height, btn_cnt, buttons);
}

int CEventListHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
{
	int res = menu_return::RETURN_EXIT_ALL;

	if (parent)
		parent->hide();

	CEventList *e = new CEventList;
	CChannelList  *channelList = CNeutrinoApp::getInstance()->channelList;
	e->exec(CZapit::getInstance()->GetCurrentChannelID(), channelList->getActiveChannelName());
	delete e;

	return res;
}

bool CEventList::findEvents(t_channel_id channel_id, std::string channelname)
{
	bool res = false;
	int event = 0;

	if((m_search_keyword.empty() || m_search_keyword == m_search_autokeyword) && evtlist[selected].eventID != 0)
	{
		m_search_keyword = evtlist[selected].description;
		m_search_autokeyword = m_search_keyword;
	}

	CEventFinderMenu menu(&event,
			&m_search_epg_item,
			&m_search_keyword,
			&m_search_list,
			&m_search_channel_id,
			&m_search_bouquet_id,
			&m_search_genre,
			&m_search_fsk);

	hide();
	menu.exec(NULL,"");
	search_head_name = g_Locale->getText(LOCALE_EVENTFINDER_SEARCH);
	if (event == 1)
	{
		res = true;
		m_showChannel = true;   // force the event list to paint the channel name
		if(!evtlist.empty())
			evtlist.clear();
		if(m_search_list == SEARCH_LIST_CHANNEL)
		{
			CEitManager::getInstance()->getEventsServiceKey(m_search_channel_id, evtlist, m_search_epg_item,m_search_keyword,false, m_search_genre,m_search_fsk);
		}
		else if(m_search_list == SEARCH_LIST_BOUQUET)
		{
			int channel_nr = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getSize();
			for(int channel = 0; channel < channel_nr; channel++)
			{
				channel_id = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getChannelFromIndex(channel)->getChannelID();
				CEitManager::getInstance()->getEventsServiceKey(channel_id, evtlist, m_search_epg_item,m_search_keyword,false, m_search_genre,m_search_fsk);
			}
		}
		else if(m_search_list == SEARCH_LIST_ALL)
		{
		  
			CHintBox box(LOCALE_TIMING_EPG,g_Locale->getText(LOCALE_EVENTFINDER_SEARCHING));
			box.paint();
			std::vector<t_channel_id> v;
			int channel_nr =  CNeutrinoApp::getInstance ()->channelList->getSize();//unique channelList TV or Radio
			for(int channel = 0; channel < channel_nr; channel++){
			    channel_id =  CNeutrinoApp::getInstance ()->channelList->getChannelFromIndex(channel)->getChannelID();
			    v.push_back(channel_id);
			}
		
			std::map<t_channel_id, t_channel_id> ch_id_map;
			std::vector<t_channel_id>::iterator it;
			for (it = v.begin(); it != v.end(); ++it){
				ch_id_map[*it & 0xFFFFFFFFFFFFULL] = *it;
			}
			CEitManager::getInstance()->getEventsServiceKey(0,evtlist, m_search_epg_item,m_search_keyword, true,m_search_genre,m_search_fsk);//all_chann

			if(!evtlist.empty()){
				std::map<t_channel_id, t_channel_id>::iterator map_it;
				CChannelEventList::iterator e;
				for ( e=evtlist.begin(); e!=evtlist.end();++e){
					map_it = ch_id_map.find(e->channelID);
					if (map_it != ch_id_map.end()){
						e->channelID = map_it->second;//map channelID48 to channelID
					}
					else{
						evtlist.erase(e--);// remove event for not found channels in channelList
					}
				}
			}

			box.hide();
		}
		if(!evtlist.empty()){
			sort(evtlist.begin(),evtlist.end(),sortByDateTime);
		}

		current_event = (unsigned int)-1;
		time_t azeit=time(NULL);

		CChannelEventList::iterator e;
		for ( e=evtlist.begin(); e!=evtlist.end(); ++e )
		{
			if ( e->startTime > azeit ) {
				break;
			}
			current_event++;
		}
		if(evtlist.empty())
		{
			CChannelEvent evt;
			//evt.description = m_search_keyword + ": " + g_Locale->getText(LOCALE_EPGVIEWER_NOTFOUND);
			evt.description = g_Locale->getText(LOCALE_EPGVIEWER_NOTFOUND);
			evt.eventID = 0;
			evtlist.push_back(evt);
		}
		if (current_event == (unsigned int)-1)
			current_event = 0;
		selected= current_event;

		search_head_name += ": '";
		search_head_name += m_search_keyword;
		search_head_name += "'";

		if(!m_search_keyword.empty()){
			g_settings.epg_search_history.push_front(m_search_keyword);
			std::list<std::string>::iterator it = g_settings.epg_search_history.begin();
			++it;
			while (it != g_settings.epg_search_history.end()) {
				if (*it == m_search_keyword)
					it = g_settings.epg_search_history.erase(it);
				else
					++it;
			}
			g_settings.epg_search_history_size = g_settings.epg_search_history.size();
			if (g_settings.epg_search_history_size > g_settings.epg_search_history_max)
				g_settings.epg_search_history_size = g_settings.epg_search_history_max;
		}

	}
	if (event)
		paintHead(0, search_head_name);
	else
		paintHead(channel_id, channelname);
	paint();
	paintFoot(channel_id);
	return(res);
}

/*
class CSearchNotifier : public CChangeObserver
{
    private:
        CMenuItem* menuItem;
    public:
        CSearchNotifier( CMenuItem* i){menuItem=i};
        bool changeNotify(const neutrino_locale_t t, void * data)
        {
            int selected = *(int*)data;
            menuItem->setActive(1);
            menuItem
		}
};
*/
/*
bool CEventFinderMenuHandler::changeNotify(const neutrino_locale_t OptionName, void *Data)
{
	if(OptionName == )
	{
	}

	return true;
}
*/
const short GENRE_GROUP_COUNT = 11;
const CMenuOptionChooser::keyval GENRE_GROUP[GENRE_GROUP_COUNT] =
{
	{ 0xFF, LOCALE_GENRE_ALL },
	{ 0x18, LOCALE_GENRE_MOVIE },
	{ 0x24, LOCALE_GENRE_NEWS },
	{ 0x33, LOCALE_GENRE_SHOW },
	{ 0x4B, LOCALE_GENRE_SPORTS },
	{ 0x55, LOCALE_GENRE_CHILDRENS_PROGRAMMES },
	{ 0x66, LOCALE_GENRE_MUSIC_DANCE },
	{ 0x7B, LOCALE_GENRE_ARTS },
	{ 0x83, LOCALE_GENRE_SOCIAL_POLITICAL },
	{ 0x97, LOCALE_GENRE_DOCUS_MAGAZINES },
	{ 0xA7, LOCALE_GENRE_TRAVEL_HOBBIES }
};
const short FSK_COUNT = 8;
const CMenuOptionChooser::keyval FSK[FSK_COUNT] =
{
	{   0, LOCALE_FSK_ALL },
	{  -7, LOCALE_FSK_TO_7 },
	{ -12, LOCALE_FSK_TO_12 },
	{ -16, LOCALE_FSK_TO_16 },
	{   7, LOCALE_FSK_FROM_7 },
	{  12, LOCALE_FSK_FROM_12 },
	{  16, LOCALE_FSK_FROM_16 },
	{  18, LOCALE_FSK_FROM_18 }
};
#define SEARCH_LIST_OPTION_COUNT 3
const CMenuOptionChooser::keyval SEARCH_LIST_OPTIONS[SEARCH_LIST_OPTION_COUNT] =
{
//	{ CEventList::SEARCH_LIST_NONE,		LOCALE_PICTUREVIEWER_RESIZE_NONE },
	{ CEventList::SEARCH_LIST_CHANNEL,	LOCALE_TIMERLIST_CHANNEL },
	{ CEventList::SEARCH_LIST_BOUQUET,	LOCALE_BOUQUETLIST_HEAD },
	{ CEventList::SEARCH_LIST_ALL,		LOCALE_CHANNELLIST_HEAD }
};


#define SEARCH_EPG_OPTION_COUNT 4
const CMenuOptionChooser::keyval SEARCH_EPG_OPTIONS[SEARCH_EPG_OPTION_COUNT] =
{
//	{ CEventList::SEARCH_EPG_NONE,	LOCALE_PICTUREVIEWER_RESIZE_NONE },
	{ CEventList::SEARCH_EPG_TITLE,	LOCALE_FONTSIZE_EPG_TITLE },
	{ CEventList::SEARCH_EPG_INFO1,	LOCALE_FONTSIZE_EPG_INFO1 },
	{ CEventList::SEARCH_EPG_INFO2,	LOCALE_FONTSIZE_EPG_INFO2 },
//	{ CEventList::SEARCH_EPG_GENRE,	LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR },
	{ CEventList::SEARCH_EPG_ALL,	LOCALE_EVENTFINDER_SEARCH_ALL_EPG }
};

CEventFinderMenu::CEventFinderMenu(int*		event,
				int* 		search_epg_item,
				std::string* 	search_keyword,
				int* 		search_list,
				t_channel_id*	search_channel_id,
				t_bouquet_id*	search_bouquet_id,
				int* 		search_genre,
				int*		search_fsk)
{
	m_event			= event;
	m_search_epg_item	= search_epg_item;
	m_search_keyword	= search_keyword;
	m_search_list		= search_list;
	m_search_channel_id	= search_channel_id;
	m_search_bouquet_id	= search_bouquet_id;
	m_search_genre		= search_genre;
	m_search_fsk		= search_fsk;

	width = 40;
	selected = -1;
}

int CEventFinderMenu::exec(CMenuTarget* parent, const std::string &actionkey)
{
	int res = menu_return::RETURN_REPAINT;

	if(actionkey.empty())
	{
		if(parent != NULL)
			parent->hide();
		//printf("0\n");
		showMenu();
	}
	else if(actionkey =="#1")
	{
		//printf("1\n");
		*m_event = true;
		res = menu_return::RETURN_EXIT_ALL;
	}
	else if(actionkey =="#2")
	{
		//printf("3\n");
		// get channel id / bouquet id
		if(*m_search_list == CEventList::SEARCH_LIST_CHANNEL)
		{
			int nNewBouquet;
			nNewBouquet = bouquetList->show();
			//printf("new_bouquet_id %d\n",nNewBouquet);
			if (nNewBouquet > -1)
			{
				int nNewChannel = bouquetList->Bouquets[nNewBouquet]->channelList->show();
				CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
				//printf("nNewChannel %d\n",nNewChannel);
				if (nNewChannel > -1)
				{
					*m_search_bouquet_id = nNewBouquet;
					*m_search_channel_id = bouquetList->Bouquets[nNewBouquet]->channelList->getActiveChannel_ChannelID();
					m_search_channelname = CServiceManager::getInstance()->GetServiceName(*m_search_channel_id);
				}
			}
		}
		else if(*m_search_list == CEventList::SEARCH_LIST_BOUQUET)
		{
			int nNewBouquet;
			nNewBouquet = bouquetList->show();
			//printf("new_bouquet_id %d\n",nNewBouquet);
			if (nNewBouquet > -1)
			{
				*m_search_bouquet_id = nNewBouquet;
				m_search_channelname = bouquetList->Bouquets[nNewBouquet]->channelList->getName();
			}
		}
	}
	else if(actionkey =="#history")
	{

		if (parent)
			parent->hide();
		CMenuWidget* m = new CMenuWidget(LOCALE_EVENTFINDER_HISTORY, NEUTRINO_ICON_MOVIEPLAYER, width);
		m->addKey(CRCInput::RC_spkr, this, "#clear");
		m->setSelected(selected);
		m->addItem(GenericMenuSeparator);
		m->addItem(GenericMenuBack);
		m->addItem(GenericMenuSeparatorLine);
		std::list<std::string>::iterator it = g_settings.epg_search_history.begin();
		for (int i = 0; i < g_settings.epg_search_history_size; i++, ++it)
			m->addItem(new CMenuForwarder((*it).c_str(), true, NULL, this, (*it).c_str(), CRCInput::convertDigitToKey(i + 1)));
		m->exec(NULL, "");
		m->hide();
		delete m;
		return menu_return::RETURN_REPAINT;
	}
	if (actionkey == "#clear") {
		g_settings.epg_search_history.clear();
		g_settings.epg_search_history_size = 0;
		return menu_return::RETURN_EXIT;
	}

	std::list<std::string>::iterator it = g_settings.epg_search_history.begin();
	for (int i = 0; i < g_settings.epg_search_history_size; i++, ++it){
		if((*it)== actionkey){
			*m_search_keyword = actionkey;
			g_RCInput->postMsg((neutrino_msg_t) CRCInput::RC_blue, 0);
			return menu_return::RETURN_EXIT;
		}
	}

	return res;
}

int CEventFinderMenu::showMenu(void)
{
	int res = menu_return::RETURN_REPAINT;
	m_search_channelname_mf = NULL;
	*m_event = false;

	if(*m_search_list == CEventList::SEARCH_LIST_CHANNEL)
	{
		m_search_channelname = CServiceManager::getInstance()->GetServiceName(*m_search_channel_id);
	}
	else if(*m_search_list == CEventList::SEARCH_LIST_BOUQUET)
	{
		if (*m_search_bouquet_id >= bouquetList->Bouquets.size()){
			*m_search_bouquet_id = bouquetList->getActiveBouquetNumber();
		}
		if(!bouquetList->Bouquets.empty())
			m_search_channelname = bouquetList->Bouquets[*m_search_bouquet_id]->channelList->getName();
		else
			m_search_channelname ="";
	}
	else if(*m_search_list == CEventList::SEARCH_LIST_ALL)
	{
		m_search_channelname ="";
	}

	int shortcut = 1;

	CKeyboardInput stringInput(LOCALE_EVENTFINDER_KEYWORD,m_search_keyword);

	CMenuForwarder* mf0	= new CMenuForwarder(LOCALE_EVENTFINDER_KEYWORD, true, *m_search_keyword, &stringInput, NULL, CRCInput::RC_red);
	CMenuOptionChooser* mo0	= new CMenuOptionChooser(LOCALE_EVENTFINDER_SEARCH_WITHIN_LIST, m_search_list, SEARCH_LIST_OPTIONS, SEARCH_LIST_OPTION_COUNT, true, this, CRCInput::convertDigitToKey(shortcut++));

	m_search_channelname_mf	= new CMenuForwarder("", *m_search_list != CEventList::SEARCH_LIST_ALL, m_search_channelname, this, "#2", CRCInput::convertDigitToKey(shortcut++));
	CMenuOptionChooser* mo1	= new CMenuOptionChooser(LOCALE_EVENTFINDER_SEARCH_WITHIN_EPG, m_search_epg_item, SEARCH_EPG_OPTIONS, SEARCH_EPG_OPTION_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
	CMenuForwarder* mf1	= new CMenuForwarder(LOCALE_EVENTFINDER_START_SEARCH, true, NULL, this, "#1", CRCInput::RC_green);

	CMenuOptionChooser* mgenre = new CMenuOptionChooser(LOCALE_EVENTFINDER_GENRE, m_search_genre, GENRE_GROUP, GENRE_GROUP_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));
	CMenuOptionChooser* mfsk = new CMenuOptionChooser(LOCALE_EVENTFINDER_FSK, m_search_fsk, FSK, FSK_COUNT, true, NULL, CRCInput::convertDigitToKey(shortcut++));

	CMenuWidget searchMenu(LOCALE_EVENTFINDER_HEAD, NEUTRINO_ICON_FEATURES, 40);

	CMenuForwarder* mf2	= new CMenuForwarder(LOCALE_EVENTFINDER_HISTORY, true, NULL, this, "#history", CRCInput::RC_yellow);
	CMenuOptionNumberChooser* moc1 = new CMenuOptionNumberChooser(LOCALE_EVENTFINDER_MAX_HISTORY, &g_settings.epg_search_history_max, true, 0, 50, NULL);

	searchMenu.addItem(GenericMenuSeparator);
	searchMenu.addItem(GenericMenuBack);
	searchMenu.addItem(GenericMenuSeparatorLine);
	searchMenu.addItem(mf0);
	searchMenu.addItem(mf1);
	searchMenu.addItem(GenericMenuSeparatorLine);
	searchMenu.addItem(mo0);
	searchMenu.addItem(m_search_channelname_mf);
	searchMenu.addItem(mo1);
	searchMenu.addItem(GenericMenuSeparatorLine);
	searchMenu.addItem(mgenre);
	searchMenu.addItem(mfsk);
	searchMenu.addItem(GenericMenuSeparatorLine);
	searchMenu.addItem(mf2);
	searchMenu.addItem(moc1);

	res = searchMenu.exec(NULL,"");
	return(res);
}

bool CEventFinderMenu::changeNotify(const neutrino_locale_t OptionName, void *)
{
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_EVENTFINDER_SEARCH_WITHIN_LIST))
	{
		if (*m_search_list == CEventList::SEARCH_LIST_CHANNEL)
		{
			m_search_channelname = g_Zapit->getChannelName(*m_search_channel_id);
			m_search_channelname_mf->setActive(true);
		}
		else if (*m_search_list == CEventList::SEARCH_LIST_BOUQUET)
		{
			if (*m_search_bouquet_id >= bouquetList->Bouquets.size()){
				*m_search_bouquet_id = bouquetList->getActiveBouquetNumber();
			}
			if(!bouquetList->Bouquets.empty()){
				m_search_channelname = bouquetList->Bouquets[*m_search_bouquet_id]->channelList->getName();
				m_search_channelname_mf->setActive(true);
			}
		}
		else if (*m_search_list == CEventList::SEARCH_LIST_ALL)
		{
			m_search_channelname = "";
			m_search_channelname_mf->setActive(false);
		}
	}
	return false;
}

