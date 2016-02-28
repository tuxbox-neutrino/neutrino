/*
	adzap.cpp

	(C) 2012-2013 by martii
	(C) 2016 Sven Hoefer (svenhoefer)

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <global.h>
#include <neutrino.h>
#include <algorithm>
#include <gui/adzap.h>
#include <gui/widget/hintbox.h>
#include <eitd/sectionsd.h>
#include <driver/screen_max.h>
#include <system/set_threadname.h>

#define ZAPBACK_ALERT_PERIOD 15 // seconds

static const struct button_label CAdZapMenuFooterButtons[] = {
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_ADZAP_DISABLE },
	{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_ADZAP_ENABLE },
	{ NEUTRINO_ICON_BUTTON_BLUE,	LOCALE_ADZAP_MONITOR }
};
#define CAdZapMenuFooterButtonCount (sizeof(CAdZapMenuFooterButtons)/sizeof(button_label))

static CAdZapMenu *azm = NULL;

CAdZapMenu *CAdZapMenu::getInstance()
{
	if (!azm)
		azm = new CAdZapMenu();
	return azm;
}

CAdZapMenu::CAdZapMenu()
{
	frameBuffer = CFrameBuffer::getInstance();
	width = 40;

	sem_init(&sem, 0, 0);

	pthread_t thr;
	if (pthread_create(&thr, 0, CAdZapMenu::Run, this))
		fprintf(stderr, "ERROR: pthread_create(CAdZapMenu::CAdZapMenu)\n");
	else
		pthread_detach(thr);
	channelId = -1;
	armed = false;
	monitor = false;
	alerted = false;
}

static bool sortByDateTime(const CChannelEvent & a, const CChannelEvent & b)
{
	return a.startTime < b.startTime;
}

void CAdZapMenu::Init()
{
	CChannelList *channelList = CNeutrinoApp::getInstance()->channelList;
	channelId = channelList ? channelList->getActiveChannel_ChannelID() : -1;
	channelName = channelList->getActiveChannelName();
	evtlist.clear();
	CEitManager::getInstance()->getEventsServiceKey(channelId & 0xFFFFFFFFFFFFULL, evtlist);
	monitorLifeTime.tv_sec = 0;
	if (!evtlist.empty())
	{
		sort(evtlist.begin(), evtlist.end(), sortByDateTime);
		monitorLifeTime.tv_sec = getMonitorLifeTime();
		Update();
	}
	printf("CAdZapMenu::%s: monitorLifeTime.tv_sec: %d\n", __func__, (uint) monitorLifeTime.tv_sec);
}

time_t CAdZapMenu::getMonitorLifeTime()
{
	if (evtlist.empty())
		return 0;

	CChannelEventList::iterator eli;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	for (eli = evtlist.begin(); eli != evtlist.end(); ++eli)
	{
		if ((u_int) eli->startTime + (u_int) eli->duration > (u_int) ts.tv_sec)
			return (uint) eli->startTime + eli->duration;
	}
	return 0;
}

void CAdZapMenu::Update()
{
	clock_gettime(CLOCK_REALTIME, &zapBackTime);
	zapBackTime.tv_sec += g_settings.adzap_zapBackPeriod - ZAPBACK_ALERT_PERIOD;
	sem_post(&sem);
}

void *CAdZapMenu::Run(void *arg)
{
	CAdZapMenu *me = (CAdZapMenu *) arg;
	me->Run();
	pthread_exit(NULL);
}

void CAdZapMenu::Run()
{
	set_threadname("CAdZapMenu::Run");
	while (true)
	{
		CChannelList *channelList = NULL;
		t_channel_id curChannelId = -1;

		if (monitor)
		{
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 1;

			sem_timedwait(&sem, &ts);

			if (monitor && (monitorLifeTime.tv_sec > ts.tv_sec))
			{
				channelList = CNeutrinoApp::getInstance()->channelList;
				curChannelId = channelList ? channelList->getActiveChannel_ChannelID() : -1;
				if (!armed && (channelId != curChannelId))
				{
					armed = true;
					clock_gettime(CLOCK_REALTIME, &zapBackTime);
					zapBackTime.tv_sec += g_settings.adzap_zapBackPeriod - ZAPBACK_ALERT_PERIOD;
					alerted = false;
				}
				else if (channelId == curChannelId)
				{
					armed = false;
					alerted = false;
				}
			}
			else
			{
				monitor = false;
				armed = false;
				alerted = false;
			}
		}
		else if (armed)
			sem_timedwait(&sem, &zapBackTime);
		else
			sem_wait(&sem);

		if (armed)
		{
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			if (ts.tv_sec >= zapBackTime.tv_sec)
			{
				if (!channelList)
				{
					channelList = CNeutrinoApp::getInstance()->channelList;
					curChannelId = channelList ? channelList->getActiveChannel_ChannelID() : -1;
				}
				if (!alerted)
				{
					if (channelId != curChannelId)
					{
						char name[1024];
						snprintf(name, sizeof(name)-1, g_Locale->getText(LOCALE_ADZAP_ANNOUNCE), ZAPBACK_ALERT_PERIOD, channelName.c_str());
						ShowHint(LOCALE_ADZAP, name);
					}
					alerted = true;
					zapBackTime.tv_sec += ZAPBACK_ALERT_PERIOD;
				}
				else
				{
					alerted = false;
					if (channelList)
						channelList->zapTo_ChannelID(channelId);
					armed = false;
				}
			}
		}
	}
}

int CAdZapMenu::exec(CMenuTarget *parent, const std::string & actionKey)
{
	Init();

	int res = menu_return::RETURN_EXIT_ALL;

	if (actionKey == "enable")
	{
		if (!monitor)
			armed = true;
		alerted = false;
		Update();
		return res;
	}
	if (actionKey == "disable")
	{
		armed = false;
		monitor = false;
		alerted = false;
		Update();
		return res;
	}
	if (actionKey == "monitor")
	{
		armed = false;
		monitor = true;
		alerted = false;
		if (!evtlist.empty())
			monitorLifeTime.tv_sec = getMonitorLifeTime();
		printf("CAdZapMenu::%s: monitorLifeTime.tv_sec: %d\n", __func__, (uint) monitorLifeTime.tv_sec);
		Update();
		return res;
	}
	if (actionKey == "adzap")
	{
		if (armed || monitor) {
			armed = false;
			monitor = false;
			alerted = false;
			Update();
			ShowHint(LOCALE_ADZAP, LOCALE_ADZAP_CANCEL, 450, 1);
			return res;
		}
	}
	if (actionKey.length() == 1)
	{
		g_settings.adzap_zapBackPeriod = actionKey[0] - '0';
		for (int shortcut = 1; shortcut < 10; shortcut++)
			forwarders[shortcut - 1]->setMarked(shortcut == g_settings.adzap_zapBackPeriod);
		g_settings.adzap_zapBackPeriod *= 60;
		return menu_return::RETURN_REPAINT;
	}

	if (parent)
		parent->hide();

	monitor = false;
	Settings();

	return res;
}

void CAdZapMenu::Settings()
{
	bool show_monitor = monitorLifeTime.tv_sec;

	CMenuWidget *menu = new CMenuWidget(LOCALE_ADZAP, "settings", width);
	menu->addKey(CRCInput::RC_red, this, "disable");
	menu->addKey(CRCInput::RC_green, this, "enable");
	menu->addKey(CRCInput::RC_blue, this, "monitor");
	menu->addIntroItems(NONEXISTANT_LOCALE, LOCALE_ADZAP_SWITCHBACK);

	neutrino_locale_t minute = LOCALE_ADZAP_MINUTE;
	for (int shortcut = 1; shortcut < 10; shortcut++) {
		char actionKey[2];
		actionKey[0] = '0' + shortcut;
		actionKey[1] = 0;
		bool selected = g_settings.adzap_zapBackPeriod == 60 * shortcut;
		forwarders[shortcut - 1] = new CMenuForwarder(minute, true, NULL, this, actionKey, CRCInput::convertDigitToKey(shortcut));
		forwarders[shortcut - 1]->setMarked(selected);
		menu->addItem(forwarders[shortcut - 1], selected);
		minute = LOCALE_ADZAP_MINUTES;
	}

	menu->setFooter(CAdZapMenuFooterButtons, CAdZapMenuFooterButtonCount - (show_monitor ? 0 : 1));
	menu->exec(NULL, "");
	menu->hide();
	delete menu;
	Update();
}
