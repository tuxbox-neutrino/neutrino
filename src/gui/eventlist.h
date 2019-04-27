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

#include <timerdclient/timerdtypes.h>

#include <driver/neutrino_msg_t.h>
#include <system/settings.h>
#include <gui/components/cc.h>
#include <gui/widget/navibar.h>
#include "infoviewer.h"

#include "widget/menue.h"
#include "widget/listhelpers.h"

#include <string>

class CFramebuffer;
class CEventList : public CListHelpers
{
	public:
		typedef enum
		{
			SEARCH_EPG_NONE,
			SEARCH_EPG_TITLE,
			SEARCH_EPG_INFO1,
			SEARCH_EPG_INFO2,
			SEARCH_EPG_GENRE,
			SEARCH_EPG_ALL
		} SEARCH_EPG;

		typedef enum
		{
			SEARCH_LIST_NONE,
			SEARCH_LIST_CHANNEL,
			SEARCH_LIST_BOUQUET,
			SEARCH_LIST_ALL
		} SEARCH_LIST;

	private:
	int             m_search_epg_item;
	std::string     m_search_keyword;
	std::string     m_search_autokeyword;
	int             m_search_list;
	int    		m_search_genre;
	int    		m_search_fsk;
	t_channel_id    m_search_channel_id;
	t_bouquet_id    m_search_bouquet_id;
	bool m_showChannel;
	int oldIndex;
	t_event_id oldEventID;

	bool findEvents(t_channel_id channel_id, std::string channelname);

	CFrameBuffer	*frameBuffer;
	CChannelEventList	evtlist;
	CTimerd::TimerList timerlist;
	void readEvents(const t_channel_id channel_id);
	unsigned int	selected;
	unsigned int	current_event;
	unsigned int	liststart;
	unsigned int	listmaxshow;
	int		item_height;
	int		largefont_height, smallfont_height;
	int		header_height;
	int		footer_height;

	std::string	search_head_name;

	int 		full_width, width, infozone_width;
	int		navibar_height;
	int 		height;
	int 		x;
	int 		y;
	std::string	infozone_text;
	bool		infozone_background;
	int      	sort_mode;
	t_event_id 	item_event_ID;
	CComponentsText	*infozone;
	CComponentsHeader *header;
	CProgressBar 	*pb;
	CNaviBar 	*navibar;
	const char *	unit_short_minute;

	void paintItem(unsigned pos, t_channel_id channel_id = 0);
	void paintDescription(int index);
	void paint(t_channel_id channel_id = 0);
	void paintHead(t_channel_id _channel_id, std::string _channelname, std::string _channelname_prev = "", std::string _channelname_next = "");
	void paintNaviBar(std::string _channelname_prev, std::string _channelname_next);
	void showProgressBar(int pos);
	void hide();
	void paintFoot(t_channel_id channel_id);
	void getChannelNames(t_channel_id &channel_id, std::string &current_channel_name, std::string &prev_channel_name, std::string &next_channel_name, neutrino_msg_t msg);

	int timerPre;
	int timerPost;
	void UpdateTimerList(void);
	bool HasTimerConflicts(time_t starttime, time_t duration, t_event_id * epg_ID);
	bool showfollow;
	CTimerd::CTimerEventTypes isScheduled(t_channel_id channel_id, CChannelEvent * event, int * tID = NULL);
	
	public:
		CEventList();
		~CEventList();
		int exec(const t_channel_id channel_id, const std::string& channelname, const std::string& prev = "", const std::string&  next = "", const CChannelEventList &followlist = CChannelEventList ()); // UTF-8
		void ResetModules();
};

class CEventListHandler : public CMenuTarget
{
	public:
		int exec(CMenuTarget* parent, const std::string &actionkey);
};

class CEventFinderMenu : public CMenuTarget, CChangeObserver
{
        private:
		CMenuForwarder* m_search_channelname_mf;
		int*           	m_event;
		int*   	        m_search_epg_item;
		int*   		m_search_genre;
		int*		m_search_fsk;
		std::string*    m_search_keyword;
		int*   	        m_search_list;
		std::string     m_search_channelname;
		t_channel_id*   m_search_channel_id;
		t_bouquet_id*   m_search_bouquet_id;
		int 		width;
		int 		selected;
		int showMenu(void);
        public:
		CEventFinderMenu(int*		event,
				int*		search_epg_item,
				std::string*	search_keyword,
				int*		search_list,
				t_channel_id*	search_channel_id,
				t_bouquet_id*	search_bouquet_id,
				int*		search_genre,
				int*		search_fsk);

                int exec( CMenuTarget* parent, const std::string &actionkey);
		bool changeNotify(const neutrino_locale_t OptionName, void *);

};
#endif
