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


#ifndef __EVENTLIST_HPP__
#define __EVENTLIST_HPP__

#include <timerdclient/timerdclient.h>
#include <sectionsdclient/sectionsdclient.h>

#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <system/settings.h>
#include <gui/components/cc_item_text.h>

#include "color.h"
#include "infoviewer.h"

#include "widget/menue.h"

#include <string>


class CNeutrinoEventList
{
	// Eventfinder start
	public:
		typedef enum
		{
			SEARCH_EPG_NONE,
			SEARCH_EPG_TITLE,
			SEARCH_EPG_INFO1,
			SEARCH_EPG_INFO2,
			SEARCH_EPG_GENRE,
			SEARCH_EPG_ALL
		}SEARCH_EPG;
		typedef enum
		{
			SEARCH_LIST_NONE,
			SEARCH_LIST_CHANNEL,
			SEARCH_LIST_BOUQUET,
			SEARCH_LIST_ALL
		}SEARCH_LIST;
	private:
	int             m_search_epg_item;
	std::string     m_search_keyword;
	std::string     m_search_autokeyword;
	int             m_search_list;
	t_channel_id    m_search_channel_id;
	t_bouquet_id    m_search_bouquet_id;
	bool m_showChannel;
	int FunctionBarHeight;
	int oldIndex;
	event_id_t oldEventID;
	bool bgRightBoxPaint;

	bool findEvents(void);
    // Eventfinder end

	CFrameBuffer	*frameBuffer;
	CChannelEventList	evtlist;
	CTimerd::TimerList timerlist;
	void readEvents(const t_channel_id channel_id);
	unsigned int	selected;
	unsigned int	current_event;
	unsigned int	liststart;
	unsigned int	listmaxshow;
	unsigned int	numwidth;
	int		fheight; // Fonthoehe Channellist-Inhalt
	int		fheight1,fheight2;
	int		fwidth1,fwidth2;
	int		theight; // Fonthoehe Channellist-Titel
	int		iheight; // Height info bar

	int		key;
	std::string	search_head_name;

	int 		full_width, width, infozone_width, fw;
	int 		height, infozone_height, fh;
	int 		x;
	int 		y;
	std::string	infozone_text;
	int      	sort_mode;
	event_id_t 	item_event_ID;
	CComponentsText	*cc_infozone;

	void paintItem(unsigned pos, t_channel_id channel_id = 0);
	void paintDescription(int index);
	void paint(t_channel_id channel_id = 0);
	void paintHead(t_channel_id channel_id, std::string name);
	void paintHead(std::string _channelname, std::string _channelname_prev, std::string _channelname_next);
	void hide();
	void showFunctionBar(bool show, t_channel_id channel_id);
	
	int timerPre;
	int timerPost;
	void UpdateTimerList(void);
	bool HasTimerConflicts(time_t starttime, time_t duration, event_id_t * epg_ID);
	bool showfollow;
	CTimerd::CTimerEventTypes isScheduled(t_channel_id channel_id, CChannelEvent * event, int * tID = NULL);
	
	public:
		CNeutrinoEventList();
		~CNeutrinoEventList();
		int exec(const t_channel_id channel_id, const std::string& channelname, const std::string& prev = "", const std::string&  next = "", const CChannelEventList &followlist = CChannelEventList ()); // UTF-8
};

class CEventListHandler : public CMenuTarget
{
	public:
		int  exec( CMenuTarget* parent,  const std::string &actionkey);

};

class CEventFinderMenu : public CMenuTarget, CChangeObserver
{
        private:
		CMenuForwarderNonLocalized* m_search_channelname_mf;
                int*                    m_event;
                int*                    m_search_epg_item;
                std::string*    m_search_keyword;
                int*                    m_search_list;
                std::string     m_search_channelname;
                t_channel_id*   m_search_channel_id;
                t_bouquet_id*   m_search_bouquet_id;
        int showMenu(void);
        public:
                CEventFinderMenu(       int*                    event,
                                                        int*                    search_epg_item,
                                                        std::string*    search_keyword,
                                                        int*                    search_list,
                                                                        t_channel_id* search_channel_id,
                                                                        t_bouquet_id* search_bouquet_id);
                int  exec( CMenuTarget* parent,  const std::string &actionkey);
		bool changeNotify(const neutrino_locale_t OptionName, void *);

};
#endif
