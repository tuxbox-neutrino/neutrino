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

#include <gui/widget/stringinput.h>

#include <timerdclient/timerdclient.h>

#include <global.h>

#include <daemonc/remotecontrol.h>
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

#include <stdlib.h>

bool CSleepTimerWidget::is_running = false;

int CSleepTimerWidget::exec(CMenuTarget* parent, const std::string &actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (is_running)
	{
		printf("[CSleepTimerWidget] %s: another instance is already running.\n", __FUNCTION__);
		return res;
	}
	is_running = true;

	int    shutdown_min = 0;
	char   value[16];
	CStringInput  *inbox;
	bool   permanent = (actionKey == "permanent");

	if (parent)
		parent->hide();

	if(permanent) {
		strcpy(value,g_settings.shutdown_min);
	} else {
		shutdown_min = g_Timerd->getSleepTimerRemaining();  // remaining shutdown time?
		sprintf(value,"%03d", shutdown_min);
	}

	if(permanent) {
		inbox = new CStringInput(LOCALE_SLEEPTIMERBOX_TITLE2, value, 3, LOCALE_SLEEPTIMERBOX_HINT1, LOCALE_SLEEPTIMERBOX_HINT3, "0123456789 ");
	} else {
		if (g_settings.sleeptimer_min == 0) {
			CSectionsdClient::CurrentNextInfo info_CurrentNext;
			g_InfoViewer->getEPG(g_RemoteControl->current_channel_id, info_CurrentNext);
			if ( info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) {
				time_t jetzt=time(NULL);
				int current_epg_zeit_dauer_rest = (info_CurrentNext.current_zeit.dauer+150 - (jetzt - info_CurrentNext.current_zeit.startzeit ))/60 ;
				if(shutdown_min == 0 && current_epg_zeit_dauer_rest > 0 && current_epg_zeit_dauer_rest < 1000)
				{
					sprintf(value,"%03d", current_epg_zeit_dauer_rest);
				}
			}
		}
		else
			sprintf(value,"%03d", g_settings.sleeptimer_min);

		inbox = new CStringInput(LOCALE_SLEEPTIMERBOX_TITLE, value, 3, LOCALE_SLEEPTIMERBOX_HINT1, LOCALE_SLEEPTIMERBOX_HINT2, "0123456789 ");
	}
	int ret = inbox->exec (NULL, "");

	delete inbox;

	/* exit pressed, cancel timer setup */
	if(ret == menu_return::RETURN_EXIT_REPAINT)
	{
		is_running = false;
		return res;
	}

	int new_val = atoi(value);
	if(permanent) {
		sprintf(g_settings.shutdown_min,"%03d", new_val);
		printf("permanent sleeptimer min: %s\n", g_settings.shutdown_min);
	}
	else if(shutdown_min != new_val) {
		shutdown_min = new_val;
		printf("sleeptimer min: %d\n", shutdown_min);
		if (shutdown_min == 0)	// if set to zero remove existing sleeptimer 
		{
			if(g_Timerd->getSleeptimerID() > 0) {
				g_Timerd->removeTimerEvent(g_Timerd->getSleeptimerID());
			}
		}
		else	// set the sleeptimer to actual time + shutdown mins and announce 1 min before
			g_Timerd->setSleeptimer(time(NULL) + ((shutdown_min -1) * 60),time(NULL) + shutdown_min * 60,0);
	}

	is_running = false;

	return res;
}
