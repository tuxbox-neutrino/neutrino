//
// $Id: epg.cpp,v 1.18 2005/11/20 15:11:40 mogway Exp $
//
// Beispiel zur Benutzung der SI class lib (dbox-II-project)
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2001 fnbrd (fnbrd@gmx.de)
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// $Log: epg.cpp,v $
// Revision 1.18  2005/11/20 15:11:40  mogway
//
//
// automatic services update for neutrino. Patch provided by Nirvana
//
// Forum thread: http://forum.tuxbox.org/forum/viewtopic.php?t=39266
//
// Revision 1.17  2003/03/14 04:27:55  obi
// make it compileable with -W -Werror
//
// Revision 1.16  2001/09/20 09:14:07  fnbrd
// Small changes.
//
// Revision 1.15  2001/06/11 19:22:54  fnbrd
// Events haben jetzt mehrere Zeiten, fuer den Fall von NVODs (cinedoms)
//
// Revision 1.14  2001/05/19 20:15:08  fnbrd
// Kleine Aenderungen (und epgXML).
//
// Revision 1.13  2001/05/18 13:11:46  fnbrd
// Fast komplett, fehlt nur noch die Auswertung der time-shifted events
// (Startzeit und Dauer der Cinedoms).
//
// Revision 1.12  2001/05/16 15:23:47  fnbrd
// Alles neu macht der Mai.
//
// Revision 1.11  2001/05/16 07:17:10  fnbrd
// SDT geht, epg erweitert.
//
// Revision 1.10  2001/05/15 19:51:55  fnbrd
// epgSmall funktioniert jetzt zufriedenstellend.
//
// Revision 1.9  2001/05/15 05:02:55  fnbrd
// Weiter gearbeitet.
//
// Revision 1.8  2001/05/14 13:44:23  fnbrd
// Erweitert.
//
// Revision 1.7  2001/05/13 12:42:00  fnbrd
// Unnoetiges Zeug entfernt.
//
// Revision 1.6  2001/05/13 12:37:11  fnbrd
// Noch etwas verbessert.
//
// Revision 1.5  2001/05/13 00:39:30  fnbrd
// Etwas aufgeraeumt.
//
// Revision 1.4  2001/05/13 00:08:54  fnbrd
// Kleine Debugausgabe dazu.
//
// Revision 1.2  2001/05/12 23:55:04  fnbrd
// Ueberarbeitet, geht aber noch nicht ganz.
//

//#define READ_PRESENT_INFOS

#include <stdio.h>
#include <time.h>

#include <set>
#include <algorithm>
#include <string>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#include "SIbouquets.hpp"
#include "SIsections.hpp"

int main(int /*argc*/, char** /*argv*/)
{
	time_t starttime, endtime;
	SIsectionsSDT sdtset;
#ifdef READ_PRESENT_INFOS
	SIsectionsEIT epgset;
#else
	SIsectionsEITschedule epgset;
#endif

	tzset(); // TZ auswerten

	starttime=time(NULL);
	epgset.readSections();
	sdtset.readSections();
	endtime=time(NULL);
	printf("EIT Sections read: %d\n", epgset.size());
	printf("SDT Sections read: %d\n", sdtset.size());
	printf("Time needed: %ds\n", (int)difftime(endtime, starttime));
//  for_each(epgset.begin(), epgset.end(), printSmallSectionHeader());
//  for_each(epgset.begin(), epgset.end(), printSIsection());
	SIevents events;
	for(SIsectionsEIT::iterator k=epgset.begin(); k!=epgset.end(); k++)
		events.insert((k->events()).begin(), (k->events()).end());

	SIservices services;
	for(SIsectionsSDT::iterator k=sdtset.begin(); k!=sdtset.end(); k++)
		services.insert((k->services()).begin(), (k->services()).end());

	// Damit wir die Zeiten der nvods richtig einsortiert bekommen
	// Ist bei epgLarge eigentlich nicht noetig, da die NVODs anscheinend nur im present/actual (epgSmall) vorkommen
	events.mergeAndRemoveTimeShiftedEvents(services);

	for_each(events.begin(), events.end(), printSIeventWithService(services));
//  for_each(events.begin(), events.end(), printSIevent());
//  for_each(epgset.begin(), epgset.end(), printSIsectionEIT());

//  int i=0;
//  for(SIsectionsEIT::iterator s=epgset.begin(); s!=epgset.end(); s++, i++) {
//    char fname[20];
//    sprintf(fname, "seit%d", i+1);
//    s->saveBufferToFile(fname);
//  }
	return 0;
}
