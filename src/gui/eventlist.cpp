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
#include <gui/eventlist.h>
#include <gui/epgplus.h>
#include <gui/timerlist.h>

#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/mountchooser.h>
#include <gui/pictureviewer.h>

#include <global.h>
#include <neutrino.h>

#include "widget/hintbox.h"
#include "gui/bouquetlist.h"
#include <gui/widget/stringinput.h>

extern CBouquetList        * bouquetList;
extern t_channel_id live_channel_id;

#include <zapit/client/zapitclient.h> /* CZapitClient::Utf8_to_Latin1 */
#include <driver/screen_max.h>

#include <zapit/client/zapittools.h>

#include <algorithm>
extern CPictureViewer * g_PicViewer;
#define PIC_W 52
#define PIC_H 39

#define ROUND_RADIUS 9

void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata);
bool sectionsd_getLinkageDescriptorsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors);

// sort operators
bool sortById (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.eventID < b.eventID ;
}
bool sortByDescription (const CChannelEvent& a, const CChannelEvent& b)
{
	if(a.description == b.description)
		return a.eventID < b.eventID;
	else
		return a.description < b.description ;
}
static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
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
}

EventList::~EventList()
{
}

void EventList::readEvents(const t_channel_id channel_id)
{
	//evtlist = g_Sectionsd->getEventsServiceKey(channel_id &0xFFFFFFFFFFFFULL);
	evtlist.clear();
	sectionsd_getEventsServiceKey(channel_id &0xFFFFFFFFFFFFULL, evtlist);
	time_t azeit=time(NULL);

	CChannelEventList::iterator e;

	if ( evtlist.size() != 0 ) {

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
							sectionsd_getEventsServiceKey(channel_id2 &0xFFFFFFFFFFFFULL, evtlist2);

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
  		// Houdini: dirty workaround for RTL double events, remove them
  		CChannelEventList::iterator e2;
  		for ( e=evtlist.begin(); e!=evtlist.end(); ++e )
  		{
  			e2 = e+1;
  			if ( e2!=evtlist.end() && (e->startTime == e2->startTime)) {
  				evtlist.erase(e2);
  			}
  		}
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


int EventList::exec(const t_channel_id channel_id, const std::string& channelname) // UTF-8
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool in_search = 0;

	width  = w_max (580, 20);
	height = h_max (480, 20);

	iheight = 30;	// info bar height (see below, hard coded at this time)
	theight  = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->getHeight();

	if(theight < PIC_H) theight = PIC_H;

	fheight1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->getHeight();
	{
		int h1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getHeight();
		int h2 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->getHeight();
		fheight2 = (h1 > h2) ? h1 : h2;
	}
	fheight = fheight1 + fheight2 + 2;
	fwidth1 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->getRenderWidth("DDD, 00:00,  ");
	fwidth2 = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getRenderWidth("[999 min] ");


	listmaxshow = (height-theight-iheight-0)/fheight;
	height = theight+iheight+0+listmaxshow*fheight; // recalc height

	x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
	y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - height) / 2;

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

	name = channelname;
	sort_mode=0;
	paintHead(channel_id, name);
	readEvents(channel_id);
	paint(channel_id);
	showFunctionBar(true);

	int oldselected = selected;

	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

		if (msg == CRCInput::RC_up || (int) msg == g_settings.key_channelList_pageup)
		{
			int step = 0;
			int prev_selected = selected;

			step = ((int) msg == g_settings.key_channelList_pageup) ? listmaxshow : 1;  // browse or step 1
			selected -= step;
			if((prev_selected-step) < 0)            // because of uint
				selected = evtlist.size() - 1;

			paintItem(prev_selected - liststart, channel_id);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;

			if(oldliststart!=liststart)
				paint(channel_id);
			else
				paintItem(selected - liststart, channel_id);

#if 0
			if ((g_settings.key_channelList_addremind != CRCInput::RC_nokey) ||
					((g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) &&
					 (g_settings.key_channelList_addrecord != CRCInput::RC_nokey)))
			{
				showFunctionBar(true);
			}
#endif
		}
		else if (msg == CRCInput::RC_down || (int) msg == g_settings.key_channelList_pagedown)
		{
			unsigned int step = 0;
			int prev_selected = selected;

			step = ((int) msg == g_settings.key_channelList_pagedown) ? listmaxshow : 1;  // browse or step 1
			selected += step;

			if(selected >= evtlist.size()) {
				if (((evtlist.size() / listmaxshow) + 1) * listmaxshow == evtlist.size() + listmaxshow) // last page has full entries
					selected = 0;
				else
					selected = ((step == listmaxshow) && (selected < (((evtlist.size() / listmaxshow) + 1) * listmaxshow))) ? (evtlist.size() - 1) : 0;
			}

			paintItem(prev_selected - liststart, channel_id);
			unsigned int oldliststart = liststart;
			liststart = (selected/listmaxshow)*listmaxshow;
			if(oldliststart!=liststart)
				paint(channel_id);
			else
				paintItem(selected - liststart, channel_id);
#if 0
			if ((g_settings.key_channelList_addremind != CRCInput::RC_nokey) ||
					((g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) &&
					 (g_settings.key_channelList_addrecord != CRCInput::RC_nokey)))
			{
				showFunctionBar(true);
			}
#endif
		}

		else if (msg == (neutrino_msg_t)g_settings.key_channelList_sort)
		{
			unsigned long long selected_id = evtlist[selected].eventID;
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
			paintHead(channel_id, name);
			paint(channel_id);
			showFunctionBar(true);

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
				if(etype == CTimerd::TIMER_RECORD) {
					g_Timerd->removeTimerEvent(tID);
					timerlist.clear();
					g_Timerd->getTimerList (timerlist);
					paint(channel_id);
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
					} else
					{
						printf("[CEventList] no network devices available\n");
					}
					if (id != -1)
						recDir = g_settings.network_nfs_local_dir[id];
					else
						recDir = NULL;
				}
				if (recDir != NULL)
				{

					//FIXME: bad ?if (g_Timerd->addRecordTimerEvent(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id,
					if (g_Timerd->addRecordTimerEvent(channel_id,
								evtlist[selected].startTime,
								evtlist[selected].startTime + evtlist[selected].duration,
								evtlist[selected].eventID, evtlist[selected].startTime,
								evtlist[selected].startTime - (ANNOUNCETIME + 120),
								TIMERD_APIDS_CONF, true, recDir,false) == -1)
					{
						if(askUserOnTimerConflict(evtlist[selected].startTime - (ANNOUNCETIME + 120),
									evtlist[selected].startTime + evtlist[selected].duration))
						{
							//g_Timerd->addRecordTimerEvent(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id,
							g_Timerd->addRecordTimerEvent(channel_id,
									evtlist[selected].startTime,
									evtlist[selected].startTime + evtlist[selected].duration,
									evtlist[selected].eventID, evtlist[selected].startTime,
									evtlist[selected].startTime - (ANNOUNCETIME + 120),
									TIMERD_APIDS_CONF, true, recDir,true);
							ShowLocalizedMessage(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
						}
					} else {
						ShowLocalizedMessage(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
					}
				}
				timerlist.clear();
				g_Timerd->getTimerList (timerlist);
				paint(channel_id);
			}
		}
		else if ( msg == (neutrino_msg_t) g_settings.key_channelList_addremind )
		{
			int tID = -1;
			CTimerd::CTimerEventTypes etype = isScheduled(channel_id, &evtlist[selected], &tID);
			if(etype == CTimerd::TIMER_ZAPTO) {
				g_Timerd->removeTimerEvent(tID);
				timerlist.clear();
				g_Timerd->getTimerList (timerlist);
				paint(channel_id);
				continue;
			}
			// FIXME g_Timerd->addZaptoTimerEvent(evtlist[selected].sub ? GET_CHANNEL_ID_FROM_EVENT_ID(evtlist[selected].eventID) : channel_id,
			g_Timerd->addZaptoTimerEvent(channel_id,
					evtlist[selected].startTime,
					evtlist[selected].startTime - ANNOUNCETIME, 0,
					evtlist[selected].eventID, evtlist[selected].startTime, 0);
			ShowLocalizedMessage(LOCALE_TIMER_EVENTTIMED_TITLE, LOCALE_TIMER_EVENTTIMED_MSG, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
			timerlist.clear();
			g_Timerd->getTimerList (timerlist);
			paint(channel_id);
		}

		else if (msg == CRCInput::RC_timeout)
		{
			selected = oldselected;
			loop = false;
		}
		else if (msg == (neutrino_msg_t)g_settings.key_channelList_cancel)
		{
			if(in_search) {
				in_search = false;
				name = channelname;
				paintHead(channel_id, name);
				readEvents(channel_id);
				paint(channel_id);
				showFunctionBar(true);
			} else {
				selected = oldselected;
				loop = false;
			}
		}
		else if ( msg==CRCInput::RC_left || msg==CRCInput::RC_red )
		{
			loop= false;
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
				res = g_EpgData->show(channel_id, evtlist[selected].eventID, &evtlist[selected].startTime);
				if ( res == menu_return::RETURN_EXIT_ALL )
				{
					loop = false;
				}
				else
				{
					g_RCInput->getMsg( &msg, &data, 0 );

					if ( ( msg != CRCInput::RC_red ) && ( msg != CRCInput::RC_timeout ) )
					{
						// RC_red schlucken
						g_RCInput->postMsg( msg, data );
					}
					timerlist.clear();
					g_Timerd->getTimerList (timerlist);

					paintHead(channel_id, name);
					paint(channel_id);
					showFunctionBar(true);
				}
			}
		}
		else if ( msg==CRCInput::RC_green )
		{
			in_search = findEvents();
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
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
	}

	hide();

	return res;
}

void EventList::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
	showFunctionBar (false);

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

void EventList::paintItem(unsigned int pos, t_channel_id channel_id)
{
	uint8_t    color;
	fb_pixel_t bgcolor;
	int ypos = y+ theight+0 + pos*fheight;
	std::string datetime1_str, datetime2_str, duration_str;
	const char * icontype = 0;

	if (liststart+pos==selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (liststart+pos == current_event )
	{
		color   = COL_MENUCONTENT + 1;
		bgcolor = COL_MENUCONTENT_PLUS_1;
	}
	else
	{
		color   = COL_MENUCONTENT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	frameBuffer->paintBoxRel(x, ypos, width- 15, fheight, bgcolor, color == COL_MENUCONTENTSELECTED ? ROUND_RADIUS : 0);

	if(liststart+pos<evtlist.size())
	{
		if ( evtlist[liststart+pos].eventID != 0 )
		{
			char tmpstr[256];
			struct tm *tmStartZeit = localtime(&evtlist[liststart+pos].startTime);


			datetime1_str = g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));

			strftime(tmpstr, sizeof(tmpstr), ". %H:%M, ", tmStartZeit );
			datetime1_str += tmpstr;

			strftime(tmpstr, sizeof(tmpstr), " %d. ", tmStartZeit );
			datetime2_str = tmpstr;

			datetime2_str += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));

			datetime2_str += '.';

			if ( m_showChannel ) // show the channel if we made a event search only (which could be made through all channels ).
			{
				t_channel_id channel = evtlist[liststart+pos].get_channel_id();
				datetime2_str += "      ";
				datetime2_str += g_Zapit->getChannelName(channel);
			}

			sprintf(tmpstr, "[%d min]", evtlist[liststart+pos].duration / 60 );
			duration_str = tmpstr;
#if 0
			CTimerd::TimerList::iterator timer = timerlist.begin();
			for(; timer != timerlist.end(); timer++) {
				if(timer->channel_id == channel_id && (timer->eventType == CTimerd::TIMER_ZAPTO || timer->eventType == CTimerd::TIMER_RECORD)) {
					if(timer->epgID == evtlist[liststart+pos].eventID) {
						if(timer->epg_starttime == evtlist[liststart+pos].startTime)
						  icontype = timer->eventType == CTimerd::TIMER_ZAPTO ? NEUTRINO_ICON_BUTTON_YELLOW : NEUTRINO_ICON_BUTTON_RED;
						break;
					}
				}
			}
#endif
		}
		CTimerd::CTimerEventTypes etype = isScheduled(channel_id, &evtlist[liststart+pos]);
		icontype = etype == CTimerd::TIMER_ZAPTO ? NEUTRINO_ICON_BUTTON_YELLOW : etype == CTimerd::TIMER_RECORD ? NEUTRINO_ICON_BUTTON_RED : 0;

		// 1st line
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->RenderString(x+5,         ypos+ fheight1+3, fwidth1+5,            datetime1_str, color, 0, true); // UTF-8
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_DATETIME]->RenderString(x+5+fwidth1, ypos+ fheight1+3, width-fwidth1-10- 20, datetime2_str, color, 0, true); // UTF-8

		int seit = ( evtlist[liststart+pos].startTime - time(NULL) ) / 60;
		if ( (seit> 0) && (seit<100) && (duration_str.length()!=0) )
		{
			char beginnt[100];
			sprintf((char*) &beginnt, "in %d min", seit);
			int w = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->getRenderWidth(beginnt) + 10;

			g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->RenderString(x+width-fwidth2-5- 20- w, ypos+ fheight1+3, fwidth2, beginnt, color);
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMSMALL]->RenderString(x+width-fwidth2-5- 20, ypos+ fheight1+3, fwidth2, duration_str, color, 0, true); // UTF-8
		if(icontype != 0)
			frameBuffer->paintIcon(icontype, x+2, ypos + fheight - 16 - (fheight1 - 16)/2);
		// 2nd line
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->RenderString(x+ 20, ypos+ fheight, width- 25- 20, evtlist[liststart+pos].description, color, 0, true);
	}
}

void EventList::paintHead(t_channel_id channel_id, std::string channelname)
{
	bool logo_ok = false;

	frameBuffer->paintBoxRel(x,y, width,theight+0, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, CORNER_TOP);

	std::string lname;
	if(g_PicViewer->GetLogoName(channel_id, channelname, lname))
		logo_ok = g_PicViewer->DisplayImage(lname, x+10, y+(theight-PIC_H)/2, PIC_W, PIC_H);

        //logo_ok = g_PicViewer->DisplayLogo(channel_id, x+10, y+(theight-PIC_H)/2, PIC_W, PIC_H);

	g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_TITLE]->RenderString(x+15+(logo_ok? 5+PIC_W:0),y+theight+1, width, name.c_str(), COL_MENUHEAD, 0, true); // UTF-8
}

void EventList::paint(t_channel_id channel_id)
{
	liststart = (selected/listmaxshow)*listmaxshow;

	if (evtlist[0].eventID != 0)
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_HELP, x+ width- 30, y+ 5 );

	frameBuffer->paintBoxRel(x, y+theight, width, height-theight-iheight, COL_MENUCONTENT_PLUS_0, 0, CORNER_TOP);
	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count, channel_id);
	}

	int ypos = y+ theight;
	int sb = fheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((evtlist.size()- 1)/ listmaxshow)+ 1;
	float sbh= (sb- 4)/ sbc;
	int sbs= (selected/listmaxshow);

	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ int(sbs* sbh) , 11, int(sbh),  COL_MENUCONTENT_PLUS_3);

}

void  EventList::showFunctionBar (bool show)
{
  int  bx,by,bw,bh;
  int  cellwidth;		// 4 cells
  int  h_offset, pos;
  int  bdx;

  bw = width;
  bh = iheight;
  bx = x;
  by = y + height-iheight;
  h_offset = 5;
  cellwidth = bw / 4;
  bdx = iheight-1;


    frameBuffer->paintBackgroundBoxRel(bx,by,bw,bh);
    // -- hide only?
    if (! show) return;

    // -- frameBuffer->paintBoxRel(x,y,w,h, COL_INFOBAR_SHADOW_PLUS_1);
    //frameBuffer->paintBoxRel(bx,by,bw,bh, COL_MENUHEAD_PLUS_0);
    frameBuffer->paintBoxRel(bx,by,bw,bh, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, CORNER_BOTTOM);


    // -- Button: Timer Record & Channelswitch
    if ((g_settings.recording_type != CNeutrinoApp::RECORDING_OFF) &&
	((unsigned int) g_settings.key_channelList_addrecord != CRCInput::RC_nokey))
    {
	    pos = 0;
	    // FIXME : display other icons depending on g_settings.key_channelList_addrecord
	    if ((g_settings.key_channelList_addrecord == CRCInput::RC_red) && !g_settings.minimode) {
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, bx+8+cellwidth*pos, by+h_offset);
	    	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(bx+bdx+cellwidth*pos, by+bh-h_offset, bw-30, g_Locale->getText(LOCALE_EVENTLISTBAR_RECORDEVENT), COL_INFOBAR, 0, true); // UTF-8
	    }
    }
    if (1)
    {
                pos = 1;
                frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, bx+8+cellwidth*pos, by+h_offset);
                g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(bx+bdx+cellwidth*pos, by+bh-h_offset, bw-30, g_Locale->getText(LOCALE_EVENTFINDER_SEARCH), COL_INFOBAR,
 0, true); // UTF-8
    }

    // Button: Timer Channelswitch
    if ((unsigned int) g_settings.key_channelList_addremind != CRCInput::RC_nokey)
    {
	    pos = 2;
	    // FIXME : display other icons depending on g_settings.key_channelList_addremind
	    if (g_settings.key_channelList_addremind == CRCInput::RC_yellow)
		    frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, bx+8+cellwidth*pos, by+h_offset );
	    g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(bx+bdx+cellwidth*pos, by+bh-h_offset, bw-30, g_Locale->getText(LOCALE_EVENTLISTBAR_CHANNELSWITCH), COL_INFOBAR, 0, true); // UTF-8
    }

    // Button: Event Re-Sort
    if ((unsigned int) g_settings.key_channelList_sort != CRCInput::RC_nokey)
    {
	    pos = 3;
	 //FIXME: display other icons depending on g_settings.key_channelList_sort value
	    if (g_settings.key_channelList_sort == CRCInput::RC_blue)
		    frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_BLUE, bx+8+cellwidth*pos, by+h_offset );
	    g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(bx+bdx+cellwidth*pos, by+bh-h_offset, bw-30, g_Locale->getText(LOCALE_EVENTLISTBAR_EVENTSORT), COL_INFOBAR, 0, true); // UTF-8
    }
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
	e->exec(live_channel_id, channelList->getActiveChannelName()); // UTF-8
	delete e;

	return res;
}



/************************************************************************************************/
int EventList::findEvents(void)
/************************************************************************************************/
{
	int res = 0;
	int event = 0;
	t_channel_id channel_id;  //g_Zapit->getCurrentServiceID()

	CEventFinderMenu menu(	&event,
							&m_search_epg_item,
							&m_search_keyword,
							&m_search_list,
							&m_search_channel_id,
							&m_search_bouquet_id
						  );
	hide();
	menu.exec(NULL,"");

	if(event == 1)
	{
		res = 1;
		m_showChannel = true;   // force the event list to paint the channel name
		evtlist.clear();
		if(m_search_list == SEARCH_LIST_CHANNEL)
		{
			//g_Sectionsd->getEventsServiceKeySearchAdd(evtlist,m_search_channel_id & 0xFFFFFFFFFFFFULL,m_search_epg_item,m_search_keyword);
			sectionsd_getEventsServiceKey(m_search_channel_id & 0xFFFFFFFFFFFFULL, evtlist, m_search_epg_item,m_search_keyword);
		}
		else if(m_search_list == SEARCH_LIST_BOUQUET)
		{
			int channel_nr = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getSize();
			for(int channel = 0; channel < channel_nr; channel++)
			{
				channel_id = bouquetList->Bouquets[m_search_bouquet_id]->channelList->getChannelFromIndex(channel)->channel_id;
				//g_Sectionsd->getEventsServiceKeySearchAdd(evtlist,channel_id & 0xFFFFFFFFFFFFULL,m_search_epg_item,m_search_keyword);
				sectionsd_getEventsServiceKey(channel_id & 0xFFFFFFFFFFFFULL, evtlist, m_search_epg_item,m_search_keyword);
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
					sectionsd_getEventsServiceKey(channel_id & 0xFFFFFFFFFFFFULL,evtlist, m_search_epg_item,m_search_keyword);
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

		name = g_Locale->getText(LOCALE_EVENTFINDER_SEARCH);
		name += ": '";
		name += m_search_keyword;
		name += "'";
	}
	paintHead(0, "");
	paint();
	showFunctionBar(true);
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
			int nNewChannel;
			int nNewBouquet;
			nNewBouquet = bouquetList->show();
			//printf("new_bouquet_id %d\n",nNewBouquet);
			if (nNewBouquet > -1)
			{
				nNewChannel = bouquetList->Bouquets[nNewBouquet]->channelList->show();
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

	CStringInputSMS stringInput(LOCALE_EVENTFINDER_KEYWORD,m_search_keyword, 20, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-.: ");

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

