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
#include <gui/timerlist.h>
#include <gui/user_menue.h>

#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/mountchooser.h>
#include <gui/pictureviewer.h>

#include "widget/hintbox.h"
#include "widget/buttons.h"
#include "bouquetlist.h"
#include <gui/widget/stringinput.h>

#include <driver/screen_max.h>
#include <driver/fade.h>
#include <driver/display.h>

#include <zapit/client/zapittools.h>
#include <zapit/zapit.h>
#include <daemonc/remotecontrol.h>
#include <eitd/sectionsd.h>

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

CNeutrinoEventList::CNeutrinoEventList()
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
	
	full_width = width = 0;
	height = 0;
	
	x = y = 0;
	
	cc_infozone = NULL;
	infozone_text = "";
	item_event_ID = 0;
	oldIndex = -1;
	oldEventID = -1;
	bgRightBoxPaint = false;
}

CNeutrinoEventList::~CNeutrinoEventList()
{
}

void CNeutrinoEventList::UpdateTimerList(void)
{
	timerlist.clear();
	g_Timerd->getTimerList (timerlist);
	g_Timerd->getRecordingSafety(timerPre,timerPost);
}

// Function: HasTimerConflicts
// search for timer conflicts for given time 
// return: true if found any conflict, you can watch with parameter epg_ID
bool CNeutrinoEventList::HasTimerConflicts(time_t starttime, time_t duration, event_id_t * epg_ID)
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

void CNeutrinoEventList::readEvents(const t_channel_id channel_id)
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
		evtlist.push_back(evt);

	}
	if (current_event == (unsigned int)-1)
		current_event = 0;
	selected= current_event;

	return;
}


int CNeutrinoEventList::exec(const t_channel_id channel_id, const std::string& channelname, const std::string& channelname_prev, const std::string& channelname_next,const CChannelEventList &followlist) // UTF-8
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool in_search = false;
	showfollow = false;

	full_width = frameBuffer->getScreenWidthRel();
	x = getScreenStartX(full_width);

	if (g_settings.eventlist_additional)
		width = full_width / 3 * 2;
	else
		width = full_width;
	height = frameBuffer->getScreenHeightRel();

	// Calculate iheight (we assume the red button is the largest one?)
	struct button_label tmp_button[1] = { { NEUTRINO_ICON_BUTTON_RED, LOCALE_EVENTLISTBAR_RECORDEVENT } };
	iheight = ::paintButtons(0, 0, 0, 1, tmp_button, 0, 0, "", false, COL_INFOBAR_SHADOW_TEXT, NULL, 0, false);

	// Calculate theight
	theight = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getHeight();
	const int pic_h = 39;
	theight = std::max(theight, pic_h);

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
	y = getScreenStartY(height);

	// calculate width of right info_zone
	infozone_width = full_width - width;

	// init right info_zone
	if (g_settings.eventlist_additional)
		cc_infozone = new CComponentsText(x+width+10, y+theight, infozone_width-20, listmaxshow*fheight);

	int res = menu_return::RETURN_REPAINT;
	//printf("CNeutrinoEventList::exec: channel_id %llx\n", channel_id);
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
	if(!followlist.empty()){
		insert_iterator <std::vector<CChannelEvent> >ii(evtlist,evtlist.begin());
		copy(followlist.begin(), followlist.end(), ii);
		showfollow = true;
	}else{
		readEvents(channel_id);
	}
	UpdateTimerList();

	bool dont_hide = false;
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
				paintDescription(selected);
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
				paintDescription(selected);
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
					showFunctionBar(true, evtlist[selected].channelID);
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
						paint(evtlist[selected].channelID);
						timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
					} 
					else
					{
						printf("[CNeutrinoEventList] no network devices available\n");
					}
					
					if (id != -1)
						recDir = g_settings.network_nfs_local_dir[id];
					else
						recDir = NULL;
				}
				if (recDir != NULL) //add/remove recording timer events and check/warn for conflicts
				{
					if (g_Timerd->addRecordTimerEvent(evtlist[selected].channelID ,
								evtlist[selected].startTime,
								evtlist[selected].startTime + evtlist[selected].duration,
								evtlist[selected].eventID, evtlist[selected].startTime,
								evtlist[selected].startTime - (ANNOUNCETIME + 120),
								TIMERD_APIDS_CONF, true, recDir,false) == -1)
					{
						if(askUserOnTimerConflict(evtlist[selected].startTime - (ANNOUNCETIME + 120), evtlist[selected].startTime + evtlist[selected].duration)) //check for timer conflict
						{
							g_Timerd->addRecordTimerEvent(evtlist[selected].channelID ,
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
				paint(evtlist[selected].channelID );
				showFunctionBar(true, evtlist[selected].channelID );
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
				showFunctionBar(true, evtlist[selected].channelID);
				continue;
			}
			
			g_Timerd->addZaptoTimerEvent(evtlist[selected].channelID ,
					evtlist[selected].startTime - (g_settings.zapto_pre_time * 60),
					evtlist[selected].startTime - ANNOUNCETIME - (g_settings.zapto_pre_time * 60), 0,
					evtlist[selected].eventID, evtlist[selected].startTime, 0);
			//ShowLocalizedMessage(LOCALE_TIMER_EVENTTIMED_TITLE, LOCALE_TIMER_EVENTTIMED_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
			timerlist.clear();
			g_Timerd->getTimerList (timerlist);
			paint(evtlist[selected].channelID );
			showFunctionBar(true, evtlist[selected].channelID );
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
					current_channel_name = CServiceManager::getInstance()->GetServiceName(_channel_id);

					tmp_channel = (channel == 0) ? channel_nr - 1 : channel - 1;
					channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(tmp_channel)->channel_id;
					prev_channel_name = CServiceManager::getInstance()->GetServiceName(channel_id_tmp);

					tmp_channel = (channel+1) %channel_nr;
					channel_id_tmp = bouquetList->Bouquets[current_bouquet_id]->channelList->getChannelFromIndex(tmp_channel)->channel_id;
					next_channel_name = CServiceManager::getInstance()->GetServiceName(channel_id_tmp);
					break;
				}
			}
			loop = false;
			dont_hide = true;
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
			oldIndex = -1;
			oldEventID = -1;
			bgRightBoxPaint = false;
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
					paintHead(channel_id,in_search ? search_head_name: channelname);
					oldIndex = -1;
					oldEventID = -1;
					bgRightBoxPaint = false;
					paint(channel_id);
					showFunctionBar(true, channel_id);
					timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);
				}
			}
		}
		else if (!showfollow  && ( msg==CRCInput::RC_green ))
		{
			oldIndex = -1;
			oldEventID = -1;
			bgRightBoxPaint = false;
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

	if (cc_infozone)
		delete cc_infozone;
	cc_infozone = NULL;
	oldIndex = -1;
	oldEventID = -1;
	bgRightBoxPaint = false;

	if(!dont_hide){
		hide();
		fader.Stop();
	}
	return res;
}

void CNeutrinoEventList::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, full_width,height);
	frameBuffer->blit();
}

CTimerd::CTimerEventTypes CNeutrinoEventList::isScheduled(t_channel_id channel_id, CChannelEvent * event, int * tID)
{
	CTimerd::TimerList::iterator timer = timerlist.begin();
	for(; timer != timerlist.end(); ++timer) {
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

void CNeutrinoEventList::paintItem(unsigned int pos, t_channel_id channel_idI)
{
	fb_pixel_t color;
	fb_pixel_t bgcolor;
	int ypos = y+ theight+0 + pos*fheight;
	unsigned int curpos = liststart + pos;

	if(RADIUS_LARGE)
		frameBuffer->paintBoxRel(x, ypos, width- 15, fheight, COL_MENUCONTENT_PLUS_0, 0);

	if (curpos==selected)
	{
		color   = COL_MENUCONTENTSELECTED_TEXT;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (curpos == current_event )
	{
		color   = COL_MENUCONTENT_TEXT_PLUS_1;
		bgcolor = COL_MENUCONTENT_PLUS_1;
	}
	else
	{
		color   = COL_MENUCONTENT_TEXT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

       if (!RADIUS_LARGE || (curpos==selected && RADIUS_LARGE) || (curpos==current_event && RADIUS_LARGE))
		frameBuffer->paintBoxRel(x, ypos, width- 15, fheight, bgcolor, RADIUS_LARGE);

	if(curpos<evtlist.size())
	{
		std::string datetime1_str, datetime2_str, duration_str;
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
				datetime2_str += CServiceManager::getInstance()->GetServiceName(channel);
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
		CTimerd::CTimerEventTypes etype = isScheduled(m_showChannel ? evtlist[curpos].channelID : channel_idI, &evtlist[curpos]);
		const char * icontype = etype == CTimerd::TIMER_ZAPTO ? NEUTRINO_ICON_ZAP : etype == CTimerd::TIMER_RECORD ? NEUTRINO_ICON_RECORDING_EVENT_MARKER : 0;
		
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
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->RenderString(x+10+iw, ypos+ fheight, width- 25- 20 -iw, evtlist[curpos].description, color, 0, true);
	}
	frameBuffer->blit();
}

void CNeutrinoEventList::paintDescription(int index)
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

	if(!epgData.info2.empty()){
		infozone_text = epgData.info2;
	}
	else if (!epgData.info1.empty()){
		infozone_text = epgData.info1;
	}
	else
		infozone_text = g_Locale->getText(LOCALE_EPGLIST_NOEVENTS);

	cc_infozone->setText(infozone_text, CTextBox::TOP, g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_EVENT]);
	cc_infozone->doPaintTextBoxBg(true);
	cc_infozone->doPaintBg(false);
	cc_infozone->forceTextPaint();
	cc_infozone->paint(CC_SAVE_SCREEN_NO);
}

void CNeutrinoEventList::paintHead(std::string _channelname, std::string _channelname_prev, std::string _channelname_next)
{
	frameBuffer->paintBoxRel(x,y, full_width,theight+0, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);

	const short font_h = 8 /* FONT_TYPE_EVENTLIST_ITEMLARGE */;
	short pn_y_off = std::max((theight - g_Font[font_h]->getHeight()) / 2, 0);
	short prev_len = g_Font[font_h]->getRenderWidth(_channelname_prev.c_str(),true);
	short next_len = g_Font[font_h]->getRenderWidth(_channelname_next.c_str(),true);
	short middle_len = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getRenderWidth(_channelname.c_str(),true);
	short middle_offset = (full_width- next_len- prev_len- middle_len)/2;
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

	g_Font[font_h]->RenderString(x+10,y+theight-pn_y_off+1, prev_len, _channelname_prev.c_str(), COL_INFOBAR_TEXT, 0, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->RenderString(x+prev_len+middle_offset,y+theight+1, middle_len, _channelname.c_str(), COL_MENUHEAD_TEXT, 0, true); // UTF-8
	g_Font[font_h]->RenderString(x+full_width-next_len-10,y+theight-pn_y_off+1, next_len, _channelname_next.c_str(), COL_INFOBAR_TEXT, 0, true); // UTF-8

}

void CNeutrinoEventList::paintHead(t_channel_id _channel_id, std::string _channelname)
{
	bool logo_ok = false;
	frameBuffer->paintBoxRel(x,y, full_width,theight+0, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);

	std::string lname;
	int logo_w = 0;
	int logo_h = 0;
	int logo_w_max = full_width / 4;
	if(g_settings.infobar_show_channellogo && g_PicViewer->GetLogoName(_channel_id, _channelname, lname, &logo_w, &logo_h)){
			if((logo_h > theight) || (logo_w > logo_w_max))
				g_PicViewer->rescaleImageDimensions(&logo_w, &logo_h, logo_w_max, theight);
		logo_ok = g_PicViewer->DisplayImage(lname, x+10, y+(theight-logo_h)/2, logo_w, logo_h);
	}
	else 
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->RenderString(x+15+(logo_ok? 5+logo_w:0),y+theight+1, full_width, _channelname.c_str(), COL_MENUHEAD_TEXT, 0, true); // UTF-8
}

void CNeutrinoEventList::paint(t_channel_id channel_id)
{
	liststart = (selected/listmaxshow)*listmaxshow;

	// paint background for right box
	if (g_settings.eventlist_additional && !bgRightBoxPaint) {
		frameBuffer->paintBoxRel(x+width,y+theight,infozone_width,listmaxshow*fheight,COL_MENUCONTENT_PLUS_0);
		bgRightBoxPaint = true;
	}

	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count, channel_id);
	}

	// paint content for right box
	paintDescription(selected);

	int ypos = y+ theight;
	int sb = fheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((evtlist.size()- 1)/ listmaxshow)+ 1;
	int sbs= (selected/listmaxshow);
	if (sbc < 1)
		sbc = 1;

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs * (sb-4)/sbc, 11, (sb-4)/sbc, COL_MENUCONTENT_PLUS_3);

}

void  CNeutrinoEventList::showFunctionBar (bool show, t_channel_id channel_id)
{
	int bx = x;
	int bw = full_width;
	int bh = iheight;
	int by = y + height - bh;

	if (! show) {
		// -- hide only?
		frameBuffer->paintBackgroundBoxRel(bx,by,bw,bh);
		return;
	}

	CColorKeyHelper keyhelper; //user_menue.h
	neutrino_msg_t dummy = CRCInput::RC_nokey;
	const char * icon = NULL;
	struct button_label buttons[5];
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

	::paintButtons(bx, by, bw, btn_cnt, buttons, bw, bh);
}

int CEventListHandler::exec(CMenuTarget* parent, const std::string &/*actionkey*/)
{
	int           res = menu_return::RETURN_EXIT_ALL;
	if (parent) {
		parent->hide();
	}
	CNeutrinoEventList *e = new CNeutrinoEventList;
	CChannelList  *channelList = CNeutrinoApp::getInstance()->channelList;
	e->exec(CZapit::getInstance()->GetCurrentChannelID(), channelList->getActiveChannelName()); // UTF-8
	delete e;

	return res;
}

/************************************************************************************************/
bool CNeutrinoEventList::findEvents(void)
/************************************************************************************************/
{
	bool res = false;
	int event = 0;
	t_channel_id channel_id = 0;

	if((m_search_keyword.empty() || m_search_keyword == m_search_autokeyword) && evtlist[selected].eventID != 0)
	{
		m_search_keyword = evtlist[selected].description;
		m_search_autokeyword = m_search_keyword;
	}

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
		if(!evtlist.empty())
			evtlist.clear();
		if(m_search_list == SEARCH_LIST_CHANNEL)
		{
			CEitManager::getInstance()->getEventsServiceKey(m_search_channel_id, evtlist, m_search_epg_item,m_search_keyword);
		}
		else if(m_search_list == SEARCH_LIST_BOUQUET)
		{
			int channel_nr = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getSize();
			for(int channel = 0; channel < channel_nr; channel++)
			{
				channel_id = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getChannelFromIndex(channel)->channel_id;
				CEitManager::getInstance()->getEventsServiceKey(channel_id, evtlist, m_search_epg_item,m_search_keyword);
			}
		}
		else if(m_search_list == SEARCH_LIST_ALL)
		{
		  
			CHintBox box(LOCALE_TIMING_EPG,g_Locale->getText(LOCALE_EVENTFINDER_SEARCHING));
			box.paint();
			std::vector<t_channel_id> v;
			int channel_nr =  CNeutrinoApp::getInstance ()->channelList->getSize();//unique channelList TV or Radio
			for(int channel = 0; channel < channel_nr; channel++){
			    channel_id =  CNeutrinoApp::getInstance ()->channelList->getChannelFromIndex(channel)->channel_id;
			    v.push_back(channel_id);
			}
		
			std::map<t_channel_id, t_channel_id> ch_id_map;
			std::vector<t_channel_id>::iterator it;
			for (it = v.begin(); it != v.end(); ++it){
				ch_id_map[*it & 0xFFFFFFFFFFFFULL] = *it;
			}
			CEitManager::getInstance()->getEventsServiceKey(0,evtlist, m_search_epg_item,m_search_keyword, true);//all_chann

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
			if ( evtlist.empty() )
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
//	{ CNeutrinoEventList::SEARCH_LIST_NONE,		LOCALE_PICTUREVIEWER_RESIZE_NONE },
	{ CNeutrinoEventList::SEARCH_LIST_CHANNEL,	LOCALE_TIMERLIST_CHANNEL },
	{ CNeutrinoEventList::SEARCH_LIST_BOUQUET,	LOCALE_BOUQUETLIST_HEAD },
	{ CNeutrinoEventList::SEARCH_LIST_ALL,		LOCALE_CHANNELLIST_HEAD }
};


#define SEARCH_EPG_OPTION_COUNT 4
const CMenuOptionChooser::keyval SEARCH_EPG_OPTIONS[SEARCH_EPG_OPTION_COUNT] =
{
//	{ CNeutrinoEventList::SEARCH_EPG_NONE,	LOCALE_PICTUREVIEWER_RESIZE_NONE },
	{ CNeutrinoEventList::SEARCH_EPG_TITLE,	LOCALE_FONTSIZE_EPG_TITLE },
	{ CNeutrinoEventList::SEARCH_EPG_INFO1,	LOCALE_FONTSIZE_EPG_INFO1 },
	{ CNeutrinoEventList::SEARCH_EPG_INFO2,	LOCALE_FONTSIZE_EPG_INFO2 },
//	{ CNeutrinoEventList::SEARCH_EPG_GENRE,	LOCALE_MOVIEBROWSER_INFO_GENRE_MAJOR },
	{ CNeutrinoEventList::SEARCH_EPG_ALL,	LOCALE_EVENTFINDER_SEARCH_ALL_EPG }
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
		if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_CHANNEL)
		{
			mf[1]->setActive(true);
			m_search_channelname = CServiceManager::getInstance()->GetServiceName(*m_search_channel_id);;
		}
		else if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_BOUQUET)
		{
			mf[1]->setActive(true);
			m_search_channelname = bouquetList->Bouquets[*m_search_bouquet_id]->channelList->getName();
		}
		else if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_ALL)
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
		if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_CHANNEL)
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
		else if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_BOUQUET)
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
	m_search_channelname_mf = NULL;
	*m_event = false;

	if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_CHANNEL)
	{
		m_search_channelname = CServiceManager::getInstance()->GetServiceName(*m_search_channel_id);
	}
	else if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_BOUQUET)
	{
		if (*m_search_bouquet_id >= bouquetList->Bouquets.size()){
			  *m_search_bouquet_id = bouquetList->getActiveBouquetNumber();;
		}
		if(!bouquetList->Bouquets.empty())
			m_search_channelname = bouquetList->Bouquets[*m_search_bouquet_id]->channelList->getName();
		else
			m_search_channelname ="";
	}
	else if(*m_search_list == CNeutrinoEventList::SEARCH_LIST_ALL)
	{
		m_search_channelname ="";
	}

	CStringInputSMS stringInput(LOCALE_EVENTFINDER_KEYWORD,m_search_keyword, 20, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789 -_/()<>=+.,:!?\\'");

	CMenuForwarder* mf0 		= new CMenuForwarder(LOCALE_EVENTFINDER_KEYWORD, true, *m_search_keyword, &stringInput, NULL, CRCInput::RC_1, NEUTRINO_ICON_BUTTON_1);
	CMenuOptionChooser* mo0 	= new CMenuOptionChooser(LOCALE_EVENTFINDER_SEARCH_WITHIN_LIST, m_search_list, SEARCH_LIST_OPTIONS, SEARCH_LIST_OPTION_COUNT, true, this, CRCInput::RC_2, NEUTRINO_ICON_BUTTON_2);
	m_search_channelname_mf		= new CMenuForwarderNonLocalized("", *m_search_list != CNeutrinoEventList::SEARCH_LIST_ALL, m_search_channelname, this, "3", CRCInput::RC_3, NEUTRINO_ICON_BUTTON_3);
	CMenuOptionChooser* mo1 	= new CMenuOptionChooser(LOCALE_EVENTFINDER_SEARCH_WITHIN_EPG, m_search_epg_item, SEARCH_EPG_OPTIONS, SEARCH_EPG_OPTION_COUNT, true, NULL, CRCInput::RC_4);
	CMenuForwarder* mf1		= new CMenuForwarder(LOCALE_EVENTFINDER_START_SEARCH, true, NULL, this, "1", CRCInput::RC_5, NEUTRINO_ICON_BUTTON_5);

	CMenuWidget searchMenu(LOCALE_EVENTFINDER_HEAD, NEUTRINO_ICON_FEATURES);

        searchMenu.addItem(GenericMenuSeparator);
        searchMenu.addItem(mf0);
        searchMenu.addItem(GenericMenuSeparatorLine);
	searchMenu.addItem(mo0);
	searchMenu.addItem(m_search_channelname_mf);
	searchMenu.addItem(mo1);
        searchMenu.addItem(GenericMenuSeparatorLine);
	searchMenu.addItem(mf1);

	res = searchMenu.exec(NULL,"");
	return(res);
}


/************************************************************************************************/
bool CEventFinderMenu::changeNotify(const neutrino_locale_t OptionName, void *)
/************************************************************************************************/
{
  
	if (ARE_LOCALES_EQUAL(OptionName, LOCALE_EVENTFINDER_SEARCH_WITHIN_LIST))
	{
		if (*m_search_list == CNeutrinoEventList::SEARCH_LIST_CHANNEL)
		{
			m_search_channelname = g_Zapit->getChannelName(*m_search_channel_id);
			m_search_channelname_mf->setActive(true);
		}
		else if (*m_search_list == CNeutrinoEventList::SEARCH_LIST_BOUQUET)
		{
			if (*m_search_bouquet_id >= bouquetList->Bouquets.size()){
				*m_search_bouquet_id = bouquetList->getActiveBouquetNumber();
			}
			if(!bouquetList->Bouquets.empty()){
				m_search_channelname = bouquetList->Bouquets[*m_search_bouquet_id]->channelList->getName();
				m_search_channelname_mf->setActive(true);
			}
		}
		else if (*m_search_list == CNeutrinoEventList::SEARCH_LIST_ALL)
		{
			m_search_channelname = "";
			m_search_channelname_mf->setActive(false);
		}
	}
	return false;
}

