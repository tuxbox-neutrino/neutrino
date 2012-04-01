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

#include <global.h>
#include <neutrino.h>
#include <gui/eventlist.h>
#include <gui/epgplus.h>
#include <gui/timerlist.h>
#include <gui/user_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/mountchooser.h>
#include <gui/pictureviewer.h>

#include "widget/hintbox.h"
#include "widget/buttons.h"
#include "gui/bouquetlist.h"
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>
#include <driver/fade.h>

#include <zapit/client/zapitclient.h> /* CZapitClient::Utf8_to_Latin1 */
#include <zapit/client/zapittools.h>
#include <zapit/zapit.h>
#include <daemonc/remotecontrol.h>

#include <algorithm>

extern CBouquetList        * bouquetList;
extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */

extern CPictureViewer * g_PicViewer;

void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata);
bool sectionsd_getLinkageDescriptorsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors);

#if 0
// sort operators
bool sortById (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.eventID < b.eventID ;
}
#endif
inline bool sortByDescription (const CChannelEvent& a, const CChannelEvent& b)
{
	if(a.description == b.description)
		return a.eventID < b.eventID;
	else
		return a.description < b.description ;
}
inline static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}

EventList::EventList()
{
	frameBuffer = CFrameBuffer::getInstance();
	selected = 0;
	current_event = 0;
	liststart = 0;
	sort_mode = 0;

	m_search_list = SEARCH_LIST_NONE;
	m_search_epg_item = SEARCH_LIST_NONE;
	m_search_epg_item = SEARCH_EPG_TITLE;
	m_search_channel_id = 1;
	m_search_bouquet_id= 1;
	
	fw = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getWidth(); //font width
	fh = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight(); //font height
	width  = w_max (62 * fw, 40);
	height = h_max (23 * fh, 20);
	
	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;
	
	item_event_ID = 0;
	FunctionBarHeight = 0;
}

EventList::~EventList()
{
}

void EventList::UpdateTimerList(void)
{
	timerlist.clear();
	g_Timerd->getTimerList (timerlist);
	g_Timerd->getRecordingSafety(timerPre,timerPost);
}

// Function: HasTimerConflicts
// search for timer conflicts for given time 
// return: true if found any conflict, you can watch with parameter epg_ID
bool EventList::HasTimerConflicts(time_t starttime, time_t duration, event_id_t * epg_ID)
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

void EventList::readEvents(const t_channel_id channel_id)
{
	//evtlist = g_Sectionsd->getEventsServiceKey(channel_id &0xFFFFFFFFFFFFULL);
	evtlist.clear();
	sectionsd_getEventsServiceKey(channel_id , evtlist);
	time_t azeit=time(NULL);

	CChannelEventList::iterator e;
	if ( !evtlist.empty() ) {

		CEPGData epgData;
		// todo: what if there are more than one events in the Portal
		//if (g_Sectionsd->getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData ))
		if (sectionsd_getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData ))
		{
//			epgData.eventID;
//			epgData.epg_times.startzeit;
			CSectionsdClient::LinkageDescriptorList	linkedServices;
			//if ( g_Sectionsd->getLinkageDescriptorsUniqueKey( epgData.eventID, linkedServices ) )
			if ( sectionsd_getLinkageDescriptorsUniqueKey( epgData.eventID, linkedServices ) )
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
						channel_id2 = CREATE_CHANNEL_ID_FROM_SERVICE_ORIGINALNETWORK_TRANSPORTSTREAM_ID(
								linkedServices[i].serviceId,
								linkedServices[i].originalNetworkId,
								linkedServices[i].transportStreamId);

						// do not add parent events
						if (channel_id != channel_id2) {
							//evtlist2 = g_Sectionsd->getEventsServiceKey(channel_id2);
							evtlist2.clear();
							sectionsd_getEventsServiceKey(channel_id2 , evtlist2);

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

	if ( evtlist.size() == 0 )
	{
		CChannelEvent evt;

		evt.description = g_Locale->getText(LOCALE_EPGLIST_NOEVENTS);
		evt.eventID = 0;
		evtlist.push_back(evt);

	}
	if (current_event == (unsigned int)-1)
		current_event = 0;
	selected= current_event;

	return;
}


int EventList::exec(const t_channel_id channel_id, const std::string& channelname, const std::string& channelname_prev, const std::string& channelname_next) // UTF-8
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool in_search = false;

	// Calculate iheight
	struct button_label tmp_button[1] = { { NEUTRINO_ICON_BUTTON_RED, NONEXISTANT_LOCALE } };
	iheight = ::paintButtons(0, 0, 0, 1, tmp_button, 0, 0, "", false, COL_INFOBAR_SHADOW, NULL, 0, false);
	if(iheight < fh)
		iheight = fh;

	// Calculate theight
	theight  = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getHeight();
	const int pic_h = 39;
	theight = std::max(theight, pic_h);
	int icol_w = 0, icol_h = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_HELP, &icol_w, &icol_h);
	theight = std::max(theight, icol_h);

	fheight1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->getHeight();
	{
		int h1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getHeight();
		int h2 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->getHeight();
		fheight2 = std::max( h1, h2 );
	}
	fheight = fheight1 + fheight2 + 2;
	fwidth1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->getRenderWidth("DDD, 00:00,  ");
	fwidth2 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getRenderWidth("[999 min] ");


	listmaxshow = (height-theight-iheight-0)/fheight;
	height = theight+iheight+0+listmaxshow*fheight; // recalc height

	int res = menu_return::RETURN_REPAINT;
	//printf("EventList::exec: channel_id %llx\n", channel_id);
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

	COSDFader fader(g_settings.menu_Content_alpha);
	fader.StartFadeIn();

	readEvents(channel_id);
	UpdateTimerList();

	if(channelname_prev.empty(), channelname_next.empty()){
		paintHead(channel_id, channelname);
	}else{
		paintHead(channelname, channelname_prev, channelname_next);

	}
	paint(channel_id);
	showFunctionBar(true, channel_id);
	frameBuffer->blit();

	int oldselected = selected;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

		if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetTimer())) {
			if(fader.Fade())
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
		if (msg == CRCInput::RC_up || (int) msg == g_settings.key_channelList_pageup || 
			msg == CRCInput::RC_down || (int) msg == g_settings.key_channelList_pagedown)
		{
			bool paint_buttonbar = false; //function bar
			int step = 0;
			int prev_selected = selected;
			// TODO: do we need this at all? Search button is always painted IIUC...
			if ((g_settings.key_channelList_addremind != (int)CRCInput::RC_nokey) ||
			    (g_settings.key_channelList_sort != (int)CRCInput::RC_nokey) ||
			    ((g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) &&
			     (g_settings.key_channelList_addrecord != (int)CRCInput::RC_nokey)))
				paint_buttonbar = true;
			
			if (msg == CRCInput::RC_up || (int) msg == g_settings.key_channelList_pageup)
			{
				step = ((int) msg == g_settings.key_channelList_pageup) ? listmaxshow : 1;  // browse or step 1
				selected -= step;
				if((prev_selected-step) < 0)            // because of uint
					selected = evtlist.size() - 1;
			}
			else if (msg == CRCInput::RC_down || (int) msg == g_settings.key_channelList_pagedown)
			{
				step = ((int) msg == g_settings.key_channelList_pagedown) ? listmaxshow : 1;  // browse or step 1
				selected += step;

				if(selected >= evtlist.size()) 
				{
					if (((evtlist.size() / listmaxshow) + 1) * listmaxshow == evtlist.size() + listmaxshow) // last page has full entries
						selected = 0;
					else
						selected = ((step == (int)listmaxshow) && (selected < (((evtlist.size() / listmaxshow) + 1) * listmaxshow))) ? (evtlist.size() - 1) : 0;
				}
			}
			paintItem(prev_selected - liststart, channel_id);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;

			if(oldliststart!=liststart)
				paint(channel_id);
			else
				paintItem(selected - liststart, channel_id);

			showFunctionBar(paint_buttonbar, channel_id);
		}
		//sort
		else if (msg == (neutrino_msg_t)g_settings.key_channelList_sort)
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
			hide();
			paintHead(channel_id, channelname);
			paint(channel_id);
			showFunctionBar(true, channel_id);

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
				CTimerd::CTimerEventTypes etype = isScheduled(channel_id, &evtlist[selected], &tID);
				if(etype == CTimerd::TIMER_RECORD) //remove timer event 
				{
					g_Timerd->removeTimerEvent(tID);
					timerlist.clear();
					g_Timerd->getTimerList (timerlist);
					paint(channel_id);
					showFunctionBar(true, channel_id);
					continue;
				}
				char *recDir = g_settings.network_nfs_recordingdir;
				if (g_settings.recording_choose_direct_rec_dir)
				{
					int id = -1;
					CMountChooser recDirs(LOCALE_TIMERLIST_RECORDING_DIR,NEUTRINO_ICON_SETTINGS,&id,NULL,g_settings.network_nfs_recordingdir);
					if (recDirs.hasItem())
					{
						hide();
						recDirs.exec(NULL,"");
						paint(channel_id);
						timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
					} 
					else
					{
						printf("[CEventList] no network devices available\n");
					}
					
					if (id != -1)
						recDir = g_settings.network_nfs_local_dir[id];
					else
						recDir = NULL;
				}
				if (recDir != NULL) //add/remove recording timer events and check/warn for conflicts
				{
					//FIXME: bad ?if (g_Timerd->addRecordTimerEvent(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id,
					if (g_Timerd->addRecordTimerEvent(channel_id,
								evtlist[selected].startTime,
								evtlist[selected].startTime + evtlist[selected].duration,
								evtlist[selected].eventID, evtlist[selected].startTime,
								evtlist[selected].startTime - (ANNOUNCETIME + 120),
								TIMERD_APIDS_CONF, true, recDir,false) == -1)
					{
						if(askUserOnTimerConflict(evtlist[selected].startTime - (ANNOUNCETIME + 120), evtlist[selected].startTime + evtlist[selected].duration)) //check for timer conflict
						{
							//g_Timerd->addRecordTimerEvent(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id,
							g_Timerd->addRecordTimerEvent(channel_id,
									evtlist[selected].startTime,
									evtlist[selected].startTime + evtlist[selected].duration,
									evtlist[selected].eventID, evtlist[selected].startTime,
									evtlist[selected].startTime - (ANNOUNCETIME + 120),
									TIMERD_APIDS_CONF, true, recDir,true);
									
							//ask user whether the timer event should be set anyway
							ShowLocalizedMessage(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
							timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
						}
					} 
					else 
					{
						//ShowLocalizedMessage(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
						timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
					}
				}
				timerlist.clear();
				g_Timerd->getTimerList (timerlist);
				paint(channel_id);
				showFunctionBar(true, channel_id);
			}
		}
		else if ( msg == (neutrino_msg_t) g_settings.key_channelList_addremind )//add/remove zapto timer event
		{
			int tID = -1;
			CTimerd::CTimerEventTypes etype = isScheduled(channel_id, &evtlist[selected], &tID);
			if(etype == CTimerd::TIMER_ZAPTO) {
				g_Timerd->removeTimerEvent(tID);
				timerlist.clear();
				g_Timerd->getTimerList (timerlist);
				paint(channel_id);
				showFunctionBar(true, channel_id);
				continue;
			}
			
			// FIXME g_Timerd->addZaptoTimerEvent(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id,		
			g_Timerd->addZaptoTimerEvent(channel_id,
					evtlist[selected].startTime - (g_settings.zapto_pre_time * 60),
					evtlist[selected].startTime - ANNOUNCETIME - (g_settings.zapto_pre_time * 60), 0,
					evtlist[selected].eventID, evtlist[selected].startTime, 0);
			//ShowLocalizedMessage(LOCALE_TIMER_EVENTTIMED_TITLE, LOCALE_TIMER_EVENTTIMED_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
			timerlist.clear();
			g_Timerd->getTimerList (timerlist);
			paint(channel_id);
			showFunctionBar(true, channel_id);
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
		}
		else if (msg == (neutrino_msg_t)g_settings.key_channelList_cancel)
		{
			if(in_search) {
				in_search = false;
				paintHead(channel_id, channelname);
				readEvents(channel_id);
				paint(channel_id);
				showFunctionBar(true, channel_id);
			} else {
				selected = oldselected;
				if(fader.StartFadeOut()) {
					timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
					msg = 0;
				} else
					loop = false;
			}
		}
		else if ( msg==CRCInput::RC_left || msg==CRCInput::RC_red ){
			loop= false;
		}
		else if ( msg==CRCInput::RC_rewind ||  msg==CRCInput::RC_forward) {

		  	t_bouquet_id current_bouquet_id= bouquetList->getActiveBouquetNumber();
			t_channel_id	channel_id_tmp, _channel_id = channel_id;
			const unsigned int channel_nr = bouquetList->Bouquets[current_bouquet_id]->channelList->getSize();
			std::string next_channel_name;
			std::string prev_channel_name ;
			std::string current_channel_name;
			unsigned int tmp_channel = 0;
			for(unsigned int channel = 0; channel < channel_nr; channel++)
			{
				channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(channel)->channel_id;
				if(channel_id_tmp == channel_id){
					if ( msg==CRCInput::RC_forward) {
						channel = (channel+1) %channel_nr;
					}else { //RC_rewind
						channel = (channel == 0) ? channel_nr -1 : channel - 1;
					}
					_channel_id = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(channel)->channel_id;
					current_channel_name = g_Zapit->getChannelName(_channel_id);

					tmp_channel = (channel == 0) ? channel_nr - 1 : channel - 1;
					channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(tmp_channel)->channel_id;
					prev_channel_name = g_Zapit->getChannelName(channel_id_tmp);

					tmp_channel = (channel+1) %channel_nr;
					channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(tmp_channel)->channel_id;
					next_channel_name = g_Zapit->getChannelName(channel_id_tmp);
					break;
				}
			}
			loop = false;
			exec(_channel_id, current_channel_name, prev_channel_name, next_channel_name);
		}
		else if (msg == CRCInput::RC_0) {
			hide();

			CTimerList *Timerlist = new CTimerList;
			Timerlist->exec(NULL, "");
			delete Timerlist;
			timerlist.clear();
			g_Timerd->getTimerList (timerlist);

			paintHead(channel_id, channelname);
			paint(channel_id);
			showFunctionBar(true, channel_id);
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
		}
		else if (msg == CRCInput::RC_epg)
		{
			hide();
			CEPGplusHandler eplus;
			eplus.exec(NULL, "");
			loop = false;
		}
		else if (msg==CRCInput::RC_help || msg==CRCInput::RC_right || msg==CRCInput::RC_ok || msg==CRCInput::RC_info)
		{
			if ( evtlist[selected].eventID != 0 )
			{
				hide();

				//FIXME res = g_EpgData->show(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id, evtlist[selected].eventID, &evtlist[selected].startTime);
				res = g_EpgData->show(evtlist[selected].channelID, evtlist[selected].eventID, &evtlist[selected].startTime);
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
					paintHead(channel_id,in_search ? search_head_name: channelname);
					paint(channel_id);
					showFunctionBar(true, channel_id);
					timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
				}
			}
		}
		else if ( msg==CRCInput::RC_green )
		{
			in_search = findEvents();
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
		}
		else if (msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
			g_RCInput->postMsg (msg, 0);
			res = menu_return::RETURN_EXIT_ALL;
			loop = false;
		}
		else
		{
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
			{
				loop = false;
				res = menu_return::RETURN_EXIT_ALL;
			}
		}
		frameBuffer->blit();
	}

	hide();
	fader.Stop();
	return res;
}

void EventList::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
	showFunctionBar (false, 0);
	frameBuffer->blit();

}

CTimerd::CTimerEventTypes EventList::isScheduled(t_channel_id channel_id, CChannelEvent * event, int * tID)
{
	CTimerd::TimerList::iterator timer = timerlist.begin();
	for(; timer != timerlist.end(); timer++) {
		if(timer->channel_id == channel_id && (timer->eventType == CTimerd::TIMER_ZAPTO || timer->eventType == CTimerd::TIMER_RECORD)) {
			if(timer->epgID == event->eventID) {
				if(timer->epg_starttime == event->startTime) {
					if(tID)
						*tID = timer->eventID;
					return timer->eventType;
				}		
			}
		}
	}
	return (CTimerd::CTimerEventTypes) 0;
}

void EventList::paintItem(unsigned int pos, t_channel_id channel_idI)
{
	uint8_t    color;
	fb_pixel_t bgcolor;
	int ypos = y+ theight+0 + pos*fheight;
	std::string datetime1_str, datetime2_str, duration_str;
	unsigned int curpos = liststart + pos;
	const char * icontype = 0;

	if (curpos==selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		if(RADIUS_LARGE)
			frameBuffer->paintBoxRel(x, ypos, width- 15, fheight, COL_MENUCONTENT_PLUS_0, 0);

	}
	else if (curpos == current_event )
	{
		color   = COL_MENUCONTENT + 1;
		bgcolor = COL_MENUCONTENT_PLUS_1;
	}
	else
	{
		color   = COL_MENUCONTENT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	frameBuffer->paintBoxRel(x, ypos, width- 15, fheight, bgcolor, color == COL_MENUCONTENTSELECTED ? RADIUS_LARGE : 0);

	if(curpos<evtlist.size())
	{
		if ( evtlist[curpos].eventID != 0 )
		{
			char tmpstr[256];
			struct tm *tmStartZeit = localtime(&evtlist[curpos].startTime);


			datetime1_str = g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));

			strftime(tmpstr, sizeof(tmpstr), ". %H:%M, ", tmStartZeit );
			datetime1_str += tmpstr;

			strftime(tmpstr, sizeof(tmpstr), " %d. ", tmStartZeit );
			datetime2_str = tmpstr;

			datetime2_str += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));

			datetime2_str += '.';

			if ( m_showChannel ) // show the channel if we made a event search only (which could be made through all channels ).
			{
				t_channel_id channel = evtlist[curpos].channelID;
				datetime2_str += "      ";
				datetime2_str += g_Zapit->getChannelName(channel);
			}

			snprintf(tmpstr,sizeof(tmpstr), "[%d min]", evtlist[curpos].duration / 60 );
			duration_str = tmpstr;
		}

		// 1st line
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->RenderString(x+5,         ypos+ fheight1+3, fwidth1+5,            datetime1_str, color, 0, true); // UTF-8
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->RenderString(x+5+fwidth1, ypos+ fheight1+3, width-fwidth1-10- 20, datetime2_str, color, 0, true); // UTF-8

		int seit = ( evtlist[curpos].startTime - time(NULL) ) / 60;
		if ( (seit> 0) && (seit<100) && (duration_str.length()!=0) )
		{
			char beginnt[100];
			snprintf((char*) &beginnt,sizeof(beginnt), "in %d min", seit);
			int w = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getRenderWidth(beginnt) + 10;

			g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->RenderString(x+width-fwidth2-5- 20- w, ypos+ fheight1+3, fwidth2, beginnt, color);
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->RenderString(x+width-fwidth2-5- 20, ypos+ fheight1+3, fwidth2, duration_str, color, 0, true); // UTF-8
		
		// 2nd line
		// set status icons
		CTimerd::CTimerEventTypes etype = isScheduled(channel_idI, &evtlist[curpos]);
		icontype = etype == CTimerd::TIMER_ZAPTO ? NEUTRINO_ICON_ZAP : etype == CTimerd::TIMER_RECORD ? NEUTRINO_ICON_RECORDING_EVENT_MARKER : 0;
		
		int iw = 0, ih;
		if(icontype != 0) {
			frameBuffer->getIconSize(icontype, &iw, &ih);
			frameBuffer->paintIcon(icontype, x+5, ypos + fheight1+3 - (fheight1 - ih)/2, fheight1);
		}
		
		// detecting timer conflict and set start position of event text depending of possible painted icon
		bool conflict = HasTimerConflicts(evtlist[curpos].startTime, evtlist[curpos].duration, &item_event_ID);
		int i2w = 0, i2h;
 		//printf ("etype %d , conflicts %d -> %s, conflict event_ID %d -> current event_ID %d\n", etype, conflict, evtlist[curpos].description.c_str(), item_event_ID, evtlist[curpos].eventID);
		
		//TODO: solution for zapto timer events
		if (conflict && item_event_ID != evtlist[curpos].eventID)
		{	
			//paint_warning = true;
 			frameBuffer->getIconSize(NEUTRINO_ICON_IMPORTANT, &i2w, &i2h);
 			frameBuffer->paintIcon(NEUTRINO_ICON_IMPORTANT, x+iw+7, ypos + fheight1+3 - (fheight1 - i2h)/2, fheight1);
 			iw += i2w+4;
		}
		
		// paint 2nd line text
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->RenderString(x+10+iw, ypos+ fheight, width- 25- 20, evtlist[curpos].description, color, 0, true);
		
		
	}
	frameBuffer->blit();
}
void EventList::paintHead(std::string _channelname, std::string _channelname_prev, std::string _channelname_next)
{
	const short font_h = 8;
	int iw = 0, ih = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_INFO, &iw, &ih);
	frameBuffer->paintBoxRel(x,y, width,theight+0, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	int name_width =((width-8-iw)/3);

	short prev_len = g_Font[font_h]->getRenderWidth(_channelname_prev.c_str(),true);
	short next_len = g_Font[font_h]->getRenderWidth(_channelname_next.c_str(),true);
	short middle_len = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getRenderWidth(_channelname.c_str(),true);
	short middle_offset =  (width- next_len- prev_len- middle_len-iw-8)/2;
	if(middle_offset < 0){
		int fw_h = g_Font[font_h]->getWidth();
		int newsize = abs(middle_offset / fw_h) + 1;
		if(_channelname_prev.size() > _channelname_next.size() ){
			_channelname_prev.resize( _channelname_prev.size() - newsize);
		}else{
			_channelname_next.resize( _channelname_next.size() - newsize);
		}
		middle_offset = 0;
	}

	g_Font[font_h]->RenderString(x+4,y+theight+1, width, _channelname_prev.c_str(), COL_INFOBAR, 0, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->RenderString(x+prev_len+middle_offset,y+theight+1, width, _channelname.c_str(), COL_MENUHEAD, 0, true); // UTF-8
	g_Font[font_h]->RenderString(x+(name_width*3)- next_len,y+theight+1, width, _channelname_next.c_str(), COL_INFOBAR, 0, true); // UTF-8

}

void EventList::paintHead(t_channel_id _channel_id, std::string _channelname)
{
	bool logo_ok = false;
	frameBuffer->paintBoxRel(x,y, width,theight+0, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);

	std::string lname;
	int logo_w = 0;
	int logo_h = 0;
	int logo_w_max = width / 4;
	if(g_settings.infobar_show_channellogo && g_PicViewer->GetLogoName(_channel_id, _channelname, lname, &logo_w, &logo_h)){
			if((logo_h > theight) || (logo_w > logo_w_max))
				g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, logo_w_max, theight);
		logo_ok = g_PicViewer->DisplayImage(lname, x+10, y+(theight-logo_h)/2, logo_w, logo_h);
	}
	else 
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->RenderString(x+15+(logo_ok? 5+logo_w:0),y+theight+1, width, _channelname.c_str(), COL_MENUHEAD, 0, true); // UTF-8
}

void EventList::paint(t_channel_id channel_id)
{
	liststart = (selected/listmaxshow)*listmaxshow;

	int iw = 0, ih = 0;
	if (evtlist[0].eventID != 0) {
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_INFO, &iw, &ih);
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_INFO, x+ width - 4 - iw, y, theight);
	}


	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count, channel_id);
	}

	int ypos = y+ theight;
	int sb = fheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((evtlist.size()- 1)/ listmaxshow)+ 1;
	int sbs= (selected/listmaxshow);

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs * (sb-4)/sbc, 11, (sb-4)/sbc, COL_MENUCONTENT_PLUS_3);

}

void  EventList::showFunctionBar (bool show, t_channel_id channel_id)
{
	int border_space = 4;
	int bx = x + 2*border_space;
	int bw = width - 16;
	int bh = iheight;
	int by = y + height-iheight;

	CColorKeyHelper keyhelper; //user_menue.h
	neutrino_msg_t dummy = CRCInput::RC_nokey;
	const char * icon = NULL;
	struct button_label buttons[4];
	int btn_cnt = 0;

	bh = std::max(FunctionBarHeight, bh);
	frameBuffer->paintBackgroundBoxRel(x,by,width,bh);
	// -- hide only?
	if (! show) return;

	int icol_w, icol_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icol_w, &icol_h);
// 	int fh = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight();

	frameBuffer->paintBoxRel(x, by, width, iheight, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_MID, CORNER_BOTTOM);
	
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

	buttons[btn_cnt].button = NEUTRINO_ICON_BUTTON_GREEN;
	buttons[btn_cnt].locale = LOCALE_EVENTFINDER_SEARCH; // search button
	btn_cnt++;

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

	// Button: Event Re-Sort
	if ((uint) g_settings.key_channelList_sort != CRCInput::RC_nokey) {
		// FIXME : display other icons depending on g_settings.key_channelList_sort
		keyhelper.get(&dummy, &icon, g_settings.key_channelList_sort);
		buttons[btn_cnt].button = icon;
		buttons[btn_cnt].locale = LOCALE_EVENTLISTBAR_EVENTSORT;
		btn_cnt++;
	}
	FunctionBarHeight = std::max(::paintButtons(bx, by, bw, btn_cnt, buttons, bw), FunctionBarHeight);
}

int CEventListHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
{
	int           res = menu_return::RETURN_EXIT_ALL;
	EventList     *e;
	CChannelList  *channelList;


	if (parent) {
		parent->hide();
	}
	e = new EventList;

	channelList = CNeutrinoApp::getInstance()->channelList;
	//e->exec(channelList->getActiveChannel_ChannelID(), channelList->getActiveChannelName()); // UTF-8
	//e->exec(g_Zapit->getCurrentServiceID(), channelList->getActiveChannelName()); // UTF-8
	e->exec(CZapit::getInstance()->GetCurrentChannelID(), channelList->getActiveChannelName()); // UTF-8
	delete e;

	return res;
}



/************************************************************************************************/
bool EventList::findEvents(void)
/************************************************************************************************/
{
	bool res = false;
	int event = 0;
	t_channel_id channel_id = 0;  //g_Zapit->getCurrentServiceID()

	CEventFinderMenu menu(	&event,
							&m_search_epg_item,
							&m_search_keyword,
							&m_search_list,
							&m_search_channel_id,
							&m_search_bouquet_id
						  );
	hide();
	menu.exec(NULL,"");
	search_head_name = g_Locale->getText(LOCALE_EVENTFINDER_SEARCH);

	if(event == 1)
	{
		res = true;
		m_showChannel = true;   // force the event list to paint the channel name
		evtlist.clear();
		if(m_search_list == SEARCH_LIST_CHANNEL)
		{
			//g_Sectionsd->getEventsServiceKeySearchAdd(evtlist,m_search_channel_id & 0xFFFFFFFFFFFFULL,m_search_epg_item,m_search_keyword);
			sectionsd_getEventsServiceKey(m_search_channel_id, evtlist, m_search_epg_item,m_search_keyword);
		}
		else if(m_search_list == SEARCH_LIST_BOUQUET)
		{
			int channel_nr = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getSize();
			for(int channel = 0; channel < channel_nr; channel++)
			{
				channel_id = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getChannelFromIndex(channel)->channel_id;
				//g_Sectionsd->getEventsServiceKeySearchAdd(evtlist,channel_id & 0xFFFFFFFFFFFFULL,m_search_epg_item,m_search_keyword);
				sectionsd_getEventsServiceKey(channel_id, evtlist, m_search_epg_item,m_search_keyword);
			}
		}
		else if(m_search_list == SEARCH_LIST_ALL)
		{
			CHintBox box(LOCALE_TIMING_EPG,g_Locale->getText(LOCALE_EVENTFINDER_SEARCHING));
			box.paint();
			int bouquet_nr = bouquetList->Bouquets.size();
			for(int bouquet = 0; bouquet < bouquet_nr; bouquet++)
			{
				int channel_nr = bouquetList->Bouquets[bouquet]->channelList->getSize();
				for(int channel = 0; channel < channel_nr; channel++)
				{
				    channel_id = bouquetList->Bouquets[bouquet]->channelList->getChannelFromIndex(channel)->channel_id;
					//g_Sectionsd->getEventsServiceKeySearchAdd(evtlist,channel_id & 0xFFFFFFFFFFFFULL,m_search_epg_item,m_search_keyword);
					sectionsd_getEventsServiceKey(channel_id,evtlist, m_search_epg_item,m_search_keyword);
				}
			}
			box.hide();
		}
		sort(evtlist.begin(),evtlist.end(),sortByDateTime);
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
			if ( evtlist.size() == 0 )
			{
				CChannelEvent evt;
				//evt.description = m_search_keyword + ": " + g_Locale->getText(LOCALE_EPGVIEWER_NOTFOUND);
				evt.description = g_Locale->getText(LOCALE_EPGVIEWER_NOTFOUND);
				evt.eventID = 0;
				evtlist.push_back(evt);
			}
		}
		if (current_event == (unsigned int)-1)
			current_event = 0;
		selected= current_event;

		search_head_name += ": '";
		search_head_name += m_search_keyword;
		search_head_name += "'";
	}
	paintHead(0, search_head_name);
	paint();
	showFunctionBar(true, channel_id);
	return(res);
}

/************************************************************************************************/
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
/************************************************************************************************
bool CEventFinderMenuHandler::changeNotify(const neutrino_locale_t OptionName, void *Data)
{
	if(OptionName == )
	{
	}

	return true;
}
*/

#define SEARCH_LIST_OPTION_COUNT 3
const CMenuOptionChooser::keyval SEARCH_LIST_OPTIONS[SEARCH_LIST_OPTION_COUNT] =
{
//	{ EventList::SEARCH_LIST_NONE,		LOCALE_PICTUREVIEWER_RESIZE_NONE },
	{ EventList::SEARCH_LIST_CHANNEL,	LOCALE_TIMERLIST_CHANNEL },
	{ EventList::SEARCH_LIST_BOUQUET,	LOCALE_BOUQUETLIST_HEAD },
	{ EventList::SEARCH_LIST_ALL,		LOCALE_CHANNELLIST_HEAD }
};


#define SEARCH_EPG_OPTION_COUNT 3
const CMenuOptionChooser::keyval SEARCH_EPG_OPTIONS[SEARCH_EPG_OPTION_COUNT] =
{
//	{ EventList::SEARCH_EPG_NONE,	LOCALE_PICTUREVIEWER_RESIZE_NONE },
	{ EventList::SEARCH_EPG_TITLE,	LOCALE_FONTSIZE_EPG_TITLE },
	{ EventList::SEARCH_EPG_INFO1,	LOCALE_FONTSIZE_EPG_INFO1 },
	{ EventList::SEARCH_EPG_INFO2,	LOCALE_FONTSIZE_EPG_INFO2 }
//	,{ EventList::SEARCH_EPG_GENRE,	LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR }
};



/************************************************************************************************/
CEventFinderMenu::CEventFinderMenu(	int* 			event,
									int* 			search_epg_item,
									std::string* 	search_keyword,
									int* 			search_list,
									t_channel_id*	search_channel_id,
									t_bouquet_id*	search_bouquet_id)
/************************************************************************************************/
{
	m_event = event;
	m_search_epg_item   = search_epg_item;
	m_search_keyword	= search_keyword;
	m_search_list       = search_list;
	m_search_channel_id = search_channel_id;
	m_search_bouquet_id = search_bouquet_id;
}


/************************************************************************************************/
int CEventFinderMenu::exec(CMenuTarget* parent, const std::string &actionkey)
/************************************************************************************************/
{
	int res = menu_return::RETURN_REPAINT;


	if(actionkey =="")
	{
		if(parent != NULL)
			parent->hide();
		//printf("0\n");
		showMenu();
	}
	else if(actionkey =="1")
	{
		//printf("1\n");
		*m_event = true;
		res = menu_return::RETURN_EXIT_ALL;
	}
	else if(actionkey =="2")
	{
		//printf("2\n");
		/*
		if(*m_search_list == EventList::SEARCH_LIST_CHANNEL)
		{
			mf[1]->setActive(true);
			m_search_channelname = g_Zapit->getChannelName(*m_search_channel_id);;
		}
		else if(*m_search_list == EventList::SEARCH_LIST_BOUQUET)
		{
			mf[1]->setActive(true);
			m_search_channelname = bouquetList->Bouquets[*m_search_bouquet_id]->channelList->getName();
		}
		else if(*m_search_list == EventList::SEARCH_LIST_ALL)
		{
			mf[1]->setActive(false);
			m_search_channelname = "";
		}
		*/
	}
	else if(actionkey =="3")
	{
		//printf("3\n");
		// get channel id / bouquet id
		if(*m_search_list == EventList::SEARCH_LIST_CHANNEL)
		{
			int nNewBouquet;
			nNewBouquet = bouquetList->show();
			//printf("new_bouquet_id %d\n",nNewBouquet);
			if (nNewBouquet > -1)
			{
				int nNewChannel = bouquetList->Bouquets[nNewBouquet]->channelList->show();
				//printf("nNewChannel %d\n",nNewChannel);
				if (nNewChannel > -1)
				{
					*m_search_bouquet_id = nNewBouquet;
					*m_search_channel_id = bouquetList->Bouquets[nNewBouquet]->channelList->getActiveChannel_ChannelID();
					m_search_channelname = g_Zapit->getChannelName(*m_search_channel_id);
				}
			}
		}
		else if(*m_search_list == EventList::SEARCH_LIST_BOUQUET)
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
	else if(actionkey =="4")
	{
		//printf("4\n");
	}

	return res;
}

/************************************************************************************************/
int CEventFinderMenu::showMenu(void)
/************************************************************************************************/
{
	int res = menu_return::RETURN_REPAINT;
	*m_event = false;

	if(*m_search_list == EventList::SEARCH_LIST_CHANNEL)
	{
		m_search_channelname = g_Zapit->getChannelName(*m_search_channel_id);
	}
	else if(*m_search_list == EventList::SEARCH_LIST_BOUQUET)
	{
		m_search_channelname = bouquetList->Bouquets[*m_search_bouquet_id]->channelList->getName();
	}
	else if(*m_search_list == EventList::SEARCH_LIST_ALL)
	{
		m_search_channelname =="";
	}

	CStringInputSMS stringInput(LOCALE_EVENTFINDER_KEYWORD,m_search_keyword, 20, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789 -_/()<>=+.,:!?\\'");

	CMenuForwarder* mf2	= new CMenuForwarder(LOCALE_EVENTFINDER_KEYWORD ,true, *m_search_keyword, &stringInput, NULL, CRCInput::RC_1 );
	CMenuOptionChooser* mo0 	= new CMenuOptionChooser(LOCALE_EVENTFINDER_SEARCH_WITHIN_LIST , m_search_list, SEARCH_LIST_OPTIONS, SEARCH_LIST_OPTION_COUNT, true, NULL, CRCInput::RC_2);
	CMenuForwarderNonLocalized* mf1	= new CMenuForwarderNonLocalized("", *m_search_list != EventList::SEARCH_LIST_ALL, m_search_channelname, this, "3", CRCInput::RC_3 );
	CMenuOptionChooser* mo1 	= new CMenuOptionChooser(LOCALE_EVENTFINDER_SEARCH_WITHIN_EPG, m_search_epg_item, SEARCH_EPG_OPTIONS, SEARCH_EPG_OPTION_COUNT, true, NULL, CRCInput::RC_4);
	CMenuForwarder* mf0 		= new CMenuForwarder(LOCALE_EVENTFINDER_START_SEARCH, true, NULL, this, "1", CRCInput::RC_5 );

	CMenuWidget searchMenu(LOCALE_EVENTFINDER_HEAD, NEUTRINO_ICON_FEATURES);

        searchMenu.addItem(GenericMenuSeparator);
        searchMenu.addItem(mf2, false);
        searchMenu.addItem(GenericMenuSeparatorLine);
        searchMenu.addItem(mo0, false);
        searchMenu.addItem(mf1, false);
        searchMenu.addItem(mo1, false);
        searchMenu.addItem(GenericMenuSeparatorLine);
        searchMenu.addItem(mf0, false);

	res = searchMenu.exec(NULL,"");
	return(res);
}
			
