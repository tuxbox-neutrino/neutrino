//
// $Id: epgMini.cpp,v 1.3 2004/02/13 14:40:00 thegoodguy Exp $
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
// $Log: epgMini.cpp,v $
// Revision 1.3  2004/02/13 14:40:00  thegoodguy
// further usage of event_id_t, t_service_id and friends (hopefully without breaking anything
//
// Revision 1.2  2001/06/10 15:21:22  fnbrd
// mit XML-Ausgabe
//
// Revision 1.1  2001/06/10 14:55:51  fnbrd
// Kleiner Aenderungen und Ergaenzungen (epgMini).
//
//

#include <stdio.h>
#include <time.h>

#include <set>
#include <algorithm>
#include <string>

#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#include "SIsections.hpp"

int main(int argc, char **argv)
{
	SIsectionsSDT sdtset;

	if(argc!=2) {
		fprintf(stderr, "Aufruf: %s Sendername\n", argv[0]);
		return 1;
	}

	tzset(); // TZ auswerten

	sdtset.readSections();

	// Die for-Schleifen sind laestig,
	// Evtl. sollte man aus den sets maps machen, damit man den key einfacher aendern
	// kann und somit find() funktioniert
	for(SIsectionsSDT::iterator k=sdtset.begin(); k!=sdtset.end(); k++)
		for(SIservices::iterator ks=k->services().begin(); ks!=k->services().end(); ks++) {
			// Erst mal die Controlcodes entfernen
//      printf("Servicename: '%s'\n", ks->serviceName.c_str());
			char servicename[50];
			strncpy(servicename, ks->serviceName.c_str(), sizeof(servicename)-1);
			servicename[sizeof(servicename)-1]=0;
			removeControlCodes(servicename);
			// Jetz pruefen ob der Servicename der gewuenschte ist
//      printf("Servicename: '%s'\n", servicename);
			if(!strcmp(servicename, argv[1])) {
				// Event (serviceid) suchen
				SIevent evt=SIevent::readActualEvent(ks->service_id);
				if(evt.service_id != 0)
					evt.saveXML(stdout); // gefunden
//	  evt.dump(); // gefunden
				else
					printf("Kein aktuelles EPG fuer %s gefunden!\n", argv[1]);
				return 0;
			}
		}
	return 1;
}
