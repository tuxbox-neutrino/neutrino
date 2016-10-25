/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2013 martii
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

//#include <algorithm>
#include <gui/followscreenings.h>

#include <system/helpers.h>

#include <gui/widget/buttons.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/icons.h>
#include <gui/widget/messagebox.h>
#include <gui/timerlist.h>

#include <global.h>
#include <neutrino.h>

CFollowScreenings::~CFollowScreenings()
{
	followlist.clear();
}

CChannelEventList *CFollowScreenings::getFollowScreenings(void)
{
	if (evtlist && followlist.empty()) {
		CChannelEventList::iterator e;
		for (e = evtlist->begin(); e != evtlist->end(); ++e)
		{
			if (e->startTime < starttime) // this includes the current event
				continue;
			if (! e->eventID)
				continue;
			if (e->description != title)
				continue;
			followlist.push_back(*e);
		}
	}
	return &followlist;
}

int CFollowScreenings::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
	unsigned long a;
	if (1 == sscanf(actionKey.c_str(), "%lu", &a)) {
		int ix = 0;
		CChannelEventList::iterator e;
		for (e = followlist.begin(); e != followlist.end(); e++, ix++)
			if ((time_t)a == e->startTime) {
				time_t start = e->startTime - (ANNOUNCETIME + 120);
				time_t stop = e->startTime + e->duration;
				CTimerd::TimerList overlappingTimers = Timer.getOverlappingTimers(start, stop);
				CTimerd::TimerList::iterator i;
				for (i = overlappingTimers.begin(); i != overlappingTimers.end(); i++)
					if (i->eventType == CTimerd::TIMER_RECORD) {
						if (channel_id == i->channel_id && e->startTime == i->epg_starttime) {
							Timer.removeTimerEvent(i->eventID);
							if (!forwarders.empty())
								forwarders[ix]->iconName_Info_right = "";
#if 0
							else
								ShowMsg(LOCALE_TIMER_EVENTREMOVED_TITLE, LOCALE_TIMER_EVENTREMOVED_MSG,
									CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
#endif
							return menu_return::RETURN_REPAINT;
						}
						if (!SAME_TRANSPONDER(channel_id, i->channel_id)) {
							if (!askUserOnTimerConflict(start, stop, channel_id))
								return menu_return::RETURN_REPAINT;
						}
					}

				if (g_Timerd->addRecordTimerEvent(channel_id, e->startTime, e->startTime + e->duration, e->eventID,
								  e->startTime, e->startTime - (ANNOUNCETIME + 120 ), apids, true, e->startTime - (ANNOUNCETIME + 120) > time(NULL), recDir, true) == -1) {
					//FIXME -- no error handling, but this shouldn't happen ...
				} else {
					if (!forwarders.empty())
						forwarders[ix]->iconName_Info_right = NEUTRINO_ICON_REC;
#if 0
					else
						ShowMsg(LOCALE_TIMER_EVENTRECORD_TITLE, LOCALE_TIMER_EVENTRECORD_MSG,
							CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
#endif
					return menu_return::RETURN_REPAINT;
				}
				break; // for
			}
		return menu_return::RETURN_EXIT_ALL;
	}
	show();
	return menu_return::RETURN_EXIT_ALL;
}

void CFollowScreenings::updateRightIcon(int ix, time_t start, unsigned int duration) {
	time_t stop = start + duration;
	start -= (ANNOUNCETIME + 120);
	CTimerd::TimerList overlappingTimers = Timer.getOverlappingTimers(start, stop);
	start += (ANNOUNCETIME + 120);
	CTimerd::TimerList::iterator i;
	for (i = overlappingTimers.begin(); i != overlappingTimers.end(); i++)
		if (i->eventType == CTimerd::TIMER_RECORD) {
			if (channel_id == i->channel_id && start == i->epg_starttime) {
				forwarders[ix]->iconName_Info_right = NEUTRINO_ICON_REC;
				return;
			}
			if (!SAME_TRANSPONDER(channel_id, i->channel_id)) {
				forwarders[ix]->iconName_Info_right = NEUTRINO_ICON_IMPORTANT;
				return;
			}
		}
}

void CFollowScreenings::show()
{
	char actionstr[32];

	getFollowScreenings();

	if (followlist.size() == 1) {
		snprintf(actionstr, sizeof(actionstr), "%lu", followlist.front().startTime);
		exec(NULL, actionstr);
	} else {
		CMenuWidget m(LOCALE_EPGVIEWER_SELECT_SCREENING, NEUTRINO_ICON_SETTINGS);
		const char *icon = NEUTRINO_ICON_BUTTON_RED;
		neutrino_msg_t directKey = CRCInput::RC_red;
		CChannelEventList::iterator e;
		int i = 0;
		for (e = followlist.begin(); e != followlist.end(); e++, i++)
		{
			struct tm *tmStartZeit = localtime(&(e->startTime));
			std::string screening_date = g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));
			screening_date += '.';
			screening_date += strftime(" %d.", tmStartZeit);
			screening_date += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));
			screening_date += strftime(". %H:%M", tmStartZeit );
			snprintf(actionstr, sizeof(actionstr), "%lu", e->startTime);
			forwarders.push_back(new CMenuForwarder(screening_date, true, NULL, this, actionstr, directKey, icon));
			updateRightIcon(i, e->startTime, e->duration);
			m.addItem(forwarders[i]);
			directKey = CRCInput::convertDigitToKey(1 + i);
			icon = NULL;
		}
		m.enableSaveScreen(true);
		m.exec(NULL, "");
	}
}

