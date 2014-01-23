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
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/sleeptimer.h>
#include <gui/infoviewer.h>

#include <system/helpers.h>
#include <gui/widget/stringinput.h>

#include <timerdclient/timerdclient.h>

#include <global.h>

#include <daemonc/remotecontrol.h>
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

#include <stdlib.h>

bool CSleepTimerWidget::is_running = false;

int CSleepTimerWidget::exec(CMenuTarget* parent, const std::string &/*actionKey*/)
{
	int res = menu_return::RETURN_REPAINT;

	if (is_running)
	{
		printf("[CSleepTimerWidget] %s: another instance is already running.\n", __FUNCTION__);
		return res;
	}
	is_running = true;

	int    shutdown_min = 0;
	std::string   value;
	CStringInput  *inbox;

	if (parent)
		parent->hide();

	if(permanent) {
		value = to_string(g_settings.shutdown_min);
		if (value.length() < 3)
			value.insert(0, 3 - value.length(), '0');
		inbox = new CStringInput(LOCALE_SLEEPTIMERBOX_TITLE2, &value, 3, LOCALE_SLEEPTIMERBOX_HINT1, LOCALE_SLEEPTIMERBOX_HINT3, "0123456789 ");
	} else {
		shutdown_min = g_Timerd->getSleepTimerRemaining();  // remaining shutdown time?
		value = to_string(shutdown_min);
		if (g_settings.sleeptimer_min == 0) {
			CSectionsdClient::CurrentNextInfo info_CurrentNext;
			g_InfoViewer->getEPG(g_RemoteControl->current_channel_id, info_CurrentNext);
			if ( info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
				time_t jetzt=time(NULL);
				int current_epg_zeit_dauer_rest = (info_CurrentNext.current_zeit.dauer+150 - (jetzt - info_CurrentNext.current_zeit.startzeit ))/60 ;
				if(shutdown_min == 0 && current_epg_zeit_dauer_rest > 0 && current_epg_zeit_dauer_rest < 1000)
				{
					value = to_string(current_epg_zeit_dauer_rest);
				}
			}
		} else {
			value = to_string(g_settings.sleeptimer_min);
		}
		if (value.length() < 3)
			value.insert(0, 3 - value.length(), '0');

		inbox = new CStringInput(LOCALE_SLEEPTIMERBOX_TITLE, &value, 3, LOCALE_SLEEPTIMERBOX_HINT1, LOCALE_SLEEPTIMERBOX_HINT2, "0123456789 ");
	}
	int ret = inbox->exec (NULL, "");

	delete inbox;

	/* exit pressed, cancel timer setup */
	if(ret == menu_return::RETURN_EXIT_REPAINT)
	{
		is_running = false;
		return res;
	}

	int new_val = atoi(value.c_str());
	if(permanent) {
		g_settings.shutdown_min = new_val;
		printf("permanent sleeptimer min: %d\n", g_settings.shutdown_min);
	}
	else if(shutdown_min != new_val) {
		shutdown_min = new_val;
		printf("sleeptimer min: %d\n", shutdown_min);
		if (shutdown_min == 0)	// if set to zero remove existing sleeptimer 
		{
			int timer_id = g_Timerd->getSleeptimerID();
			if (timer_id > 0)
				g_Timerd->removeTimerEvent(timer_id);
		}
		else	// set the sleeptimer to actual time + shutdown mins and announce 1 min before
		{
			time_t now = time(NULL);
			g_Timerd->setSleeptimer(now + (shutdown_min - 1) * 60, now + shutdown_min * 60, 0);
		}
	}

	is_running = false;

	return res;
}

std::string &CSleepTimerWidget::getValue(void)
{
	if (permanent) {
		valueStringTmp = (g_settings.shutdown_min > 0) ? to_string(g_settings.shutdown_min) + " " + g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE) : "";
	} else {
		int remaining = g_Timerd->getSleepTimerRemaining();
		valueStringTmp = (remaining > 0) ? to_string(remaining) + " " + g_Locale->getText(LOCALE_UNIT_SHORT_MINUTE) : "";
	}
	return valueStringTmp;
}
