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
#include <mymenu.h>
#include <algorithm>
#include <gui/adzap.h>
#include <gui/widget/hintbox.h>
#include <eitd/sectionsd.h>
#include <driver/screen_max.h>
#include <system/helpers.h>
#include <system/set_threadname.h>

#define ZAPBACK_ALERT_PERIOD 15 // seconds
#define ADZAP_DATA "/tmp/adzap.data"

static const struct button_label CAdZapMenuFooterButtons[] = {
	//{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_ADZAP_DISABLE },
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
	width = 35;

	sem_init(&sem, 0, 0);

	channelId = -1;
	armed = false;
	monitor = false;
	alerted = false;

	pthread_t thr;
	if (pthread_create(&thr, 0, CAdZapMenu::Run, this))
		fprintf(stderr, "ERROR: pthread_create(CAdZapMenu::CAdZapMenu)\n");
	else
		pthread_detach(thr);
}

static bool sortByDateTime(const CChannelEvent & a, const CChannelEvent & b)
{
	return a.startTime < b.startTime;
}

void CAdZapMenu::Init()
{
	CChannelList *channelList = CNeutrinoApp::getInstance()->channelList;
	channelId = channelList ? channelList->getActiveChannel_ChannelID() : -1;
	if(channelList)
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
		{
			if (g_settings.adzap_writeData)
			{
				clock_gettime(CLOCK_REALTIME, &ts);
				ts.tv_sec += 1;

				sem_timedwait(&sem, &ts);
			}
			else
				sem_timedwait(&sem, &zapBackTime);
		}
		else
			sem_wait(&sem);

		if (armed)
		{
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
					if ((channelId != curChannelId) && channelList)
						channelList->zapTo_ChannelID(channelId);
					armed = false;
				}
			}
		}

		if (g_settings.adzap_writeData && (monitor || armed))
			WriteData();
		else
			RemoveData();
	}
}

void CAdZapMenu::WriteData()
{
	int zp = g_settings.adzap_zapBackPeriod;
	clock_gettime(CLOCK_REALTIME, &ts);
	long int zb = armed ? zapBackTime.tv_sec + ZAPBACK_ALERT_PERIOD - ts.tv_sec : 0;

	if (FILE *f = fopen(ADZAP_DATA, "w"))
	{
		fprintf(f, "%" PRIx64 "\n%s\n%d\n%d:%02d\n%ld\n%ld:%02ld\n",
				channelId,
				channelName.c_str(),
				zp,
				zp / 60, zp % 60,
				zb,
				zb / 60, zb % 60);
		fclose(f);
	}
	else
		printf("CAdZapMenu::%s: failed.\n", __func__);
}

void CAdZapMenu::RemoveData()
{
	if (access(ADZAP_DATA, F_OK) == 0)
		unlink(ADZAP_DATA);
}
int CAdZapMenu::exec(CMenuTarget *parent, const std::string & actionKey)
{
	Init();

	int res = menu_return::RETURN_EXIT_ALL;
	bool marked_ok = (actionKey.length() == 1 && g_settings.adzap_zapBackPeriod == (actionKey[0] - '0') * 60);

	if (actionKey == "enable" || marked_ok)
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
		{
			bool selected = (g_settings.adzap_zapBackPeriod == shortcut);
			forwarders[shortcut - 1]->setMarked(selected);
			forwarders[shortcut - 1]->iconName_Info_right = selected ? NEUTRINO_ICON_MARKER_DIALOG_OK : NULL;
		}
		nc->setMarked(false);
		g_settings.adzap_zapBackPeriod *= 60;
		return menu_return::RETURN_REPAINT;
	}

	if (parent)
		parent->hide();

	monitor = false;
	ShowMenu();

	return res;
}

void CAdZapMenu::ShowMenu()
{
	#define ADZAP_ZAP_OPTION_COUNT 3
	const CMenuOptionChooser::keyval ADZAP_ZAP_OPTIONS[ADZAP_ZAP_OPTION_COUNT] =
	{
		{ SNeutrinoSettings::ADZAP_ZAP_OFF,LOCALE_ADZAP_ZAP_OFF},
		{ SNeutrinoSettings::ADZAP_ZAP_TO_LAST,LOCALE_ADZAP_ZAP_TO_LAST_CHANNEL},
		{ SNeutrinoSettings::ADZAP_ZAP_TO_STRAT,LOCALE_ADZAP_ZAP_TO_STRAT_CHANNEL},
	};

	bool show_monitor = monitorLifeTime.tv_sec;

	CMenuWidget *menu = new CMenuWidget(LOCALE_ADZAP, NEUTRINO_ICON_SETTINGS, width);
	//menu->addKey(CRCInput::RC_red, this, "disable");
	menu->addKey(CRCInput::RC_green, this, "enable");
	menu->addKey(CRCInput::RC_blue, this, "monitor");
	menu->addIntroItems();

	CMenuOptionChooser *oc = new CMenuOptionChooser(LOCALE_ADZAP_WRITEDATA, &g_settings.adzap_writeData, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	oc->setHint(NEUTRINO_ICON_HINT_ADZAP, LOCALE_MENU_HINT_ADZAP_WRITEDATA);
	menu->addItem(oc);

	CMenuOptionChooser *oc_zap = new CMenuOptionChooser(LOCALE_ADZAP_ZAP, &g_settings.adzap_zapOnActivation, ADZAP_ZAP_OPTIONS, ADZAP_ZAP_OPTION_COUNT, true);
	oc_zap->setHint(NEUTRINO_ICON_HINT_ADZAP, LOCALE_MENU_HINT_ADZAP_ZAP);
	menu->addItem(oc_zap);

	menu->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_ADZAP_SWITCHBACK));

	neutrino_locale_t minute = LOCALE_ADZAP_MINUTE;
	for (int shortcut = 1; shortcut < 10; shortcut++) {
		char actionKey[2];
		actionKey[0] = '0' + shortcut;
		actionKey[1] = 0;
		bool selected = g_settings.adzap_zapBackPeriod == 60 * shortcut;
		forwarders[shortcut - 1] = new CMenuForwarder(minute, true, NULL, this, actionKey, CRCInput::convertDigitToKey(shortcut));
		forwarders[shortcut - 1]->setMarked(selected);
		forwarders[shortcut - 1]->iconName_Info_right = selected ? NEUTRINO_ICON_MARKER_DIALOG_OK : NULL;
		forwarders[shortcut - 1]->setHint(NEUTRINO_ICON_HINT_ADZAP, "");
		menu->addItem(forwarders[shortcut - 1], selected);
		minute = LOCALE_ADZAP_MINUTES;
	}

	menu->addItem(GenericMenuSeparator);

	int zapBackPeriod = g_settings.adzap_zapBackPeriod / 60;
	nc = new CMenuOptionNumberChooser(minute, &zapBackPeriod, true, 10, 120, this, CRCInput::RC_0);
	nc->setMarked(g_settings.adzap_zapBackPeriod / 60 > 9);
	nc->setHint(NEUTRINO_ICON_HINT_ADZAP, "");
	menu->addItem(nc);

	menu->setFooter(CAdZapMenuFooterButtons, CAdZapMenuFooterButtonCount - (show_monitor ? 0 : 1));
	menu->exec(NULL, "");
	menu->hide();
	delete menu;
	Update();
}

bool CAdZapMenu::changeNotify(const neutrino_locale_t, void * data)
{
	int z = (*(int *)data);
	g_settings.adzap_zapBackPeriod = z * 60;
	for (int shortcut = 1; shortcut < 10; shortcut++)
		forwarders[shortcut - 1]->setMarked(false);
	nc->setMarked(true);
	return false;
}
