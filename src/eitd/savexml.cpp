//
//    sectionsd.cpp (network daemon for SI-sections)
//    (dbox-II-project)
//
//    Copyright (C) 2001 by fnbrd
//
//    Homepage: http://dbox2.elxsi.de
//
//    Copyright (C) 2008, 2009 Stefan Seyfried
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

#include <config.h>
#include <malloc.h>
#include <dmxapi.h>
#include <dmx.h>
#include <debug.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include <set>
#include <map>
#include <algorithm>
#include <string>
#include <limits>

#include <sys/wait.h>
#include <sys/time.h>

#include <connection/basicsocket.h>
#include <connection/basicserver.h>

#include <xmltree/xmlinterface.h>
#include <zapit/client/zapittools.h>

// Daher nehmen wir SmartPointers aus der Boost-Lib (www.boost.org)
#include <boost/shared_ptr.hpp>

#include <sectionsdclient/sectionsdMsg.h>
#include <sectionsdclient/sectionsdclient.h>
#include <eventserver.h>
#include <driver/abstime.h>

#include "eitd.h"

void addEvent(const SIevent &evt, const time_t zeit, bool cn = false);
extern MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey;
extern bool reader_ready;
extern pthread_rwlock_t eventsLock;

inline void readLockEvents(void)
{
	pthread_rwlock_rdlock(&eventsLock);
}
inline void unlockEvents(void)
{
	pthread_rwlock_unlock(&eventsLock);
}

void *insertEventsfromFile(void * data)
{
	xmlDocPtr event_parser = NULL;
	xmlNodePtr eventfile = NULL;
	xmlNodePtr service = NULL;
	xmlNodePtr event = NULL;
	xmlNodePtr node = NULL;
	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;
	char cclass[20];
	char cuser[20];
	std::string indexname;
	std::string filename;
	std::string epgname;
	int ev_count = 0;
	char * epg_dir = (char *) data;

	indexname = std::string(epg_dir) + "index.xml";

	xmlDocPtr index_parser = parseXmlFile(indexname.c_str());

	if (index_parser == NULL) {
		reader_ready = true;
		pthread_exit(NULL);
	}
	time_t now = time_monotonic_ms();
	printdate_ms(stdout);
	printf("[sectionsd] Reading Information from file %s:\n", indexname.c_str());

	eventfile = xmlDocGetRootElement(index_parser)->xmlChildrenNode;

	while (eventfile) {
		filename = xmlGetAttribute(eventfile, "name");
		epgname = epg_dir + filename;
		if (!(event_parser = parseXmlFile(epgname.c_str()))) {
			dprintf("unable to open %s for reading\n", epgname.c_str());
			eventfile = eventfile->xmlNextNode;
			continue;
		}
		service = xmlDocGetRootElement(event_parser)->xmlChildrenNode;

		while (service) {
			onid = xmlGetNumericAttribute(service, "original_network_id", 16);
			tsid = xmlGetNumericAttribute(service, "transport_stream_id", 16);
			sid = xmlGetNumericAttribute(service, "service_id", 16);

			event = service->xmlChildrenNode;

			while (event) {

				SIevent e(onid,tsid,sid,xmlGetNumericAttribute(event, "id", 16));

				node = event->xmlChildrenNode;

				while (xmlGetNextOccurence(node, "name") != NULL) {
					e.setName(	std::string(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "lang"))),
							std::string(xmlGetAttribute(node, "string")));
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "text") != NULL) {
					e.setText(	std::string(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "lang"))),
							std::string(xmlGetAttribute(node, "string")));
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "item") != NULL) {
					e.item = std::string(xmlGetAttribute(node, "string"));
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "item_description") != NULL) {
					e.itemDescription = std::string(xmlGetAttribute(node, "string"));
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "extended_text") != NULL) {
					e.appendExtendedText(	std::string(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "lang"))),
							std::string(xmlGetAttribute(node, "string")));
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "time") != NULL) {
					e.times.insert(SItime(xmlGetNumericAttribute(node, "start_time", 10),
								xmlGetNumericAttribute(node, "duration", 10)));
					node = node->xmlNextNode;
				}

				int count = 0;
				while (xmlGetNextOccurence(node, "content") != NULL) {
					cclass[count] = xmlGetNumericAttribute(node, "class", 16);
					cuser[count] = xmlGetNumericAttribute(node, "user", 16);
					node = node->xmlNextNode;
					count++;
				}
				e.contentClassification = std::string(cclass, count);
				e.userClassification = std::string(cuser, count);

				while (xmlGetNextOccurence(node, "component") != NULL) {
					SIcomponent c;
					c.streamContent = xmlGetNumericAttribute(node, "stream_content", 16);
					c.componentType = xmlGetNumericAttribute(node, "type", 16);
					c.componentTag = xmlGetNumericAttribute(node, "tag", 16);
					c.component = std::string(xmlGetAttribute(node, "text"));
					e.components.insert(c);
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "parental_rating") != NULL) {
					e.ratings.insert(SIparentalRating(std::string(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "country"))),
								(unsigned char) xmlGetNumericAttribute(node, "rating", 10)));
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "linkage") != NULL) {
					SIlinkage l;
					l.linkageType = xmlGetNumericAttribute(node, "type", 16);
					l.transportStreamId = xmlGetNumericAttribute(node, "transport_stream_id", 16);
					l.originalNetworkId = xmlGetNumericAttribute(node, "original_network_id", 16);
					l.serviceId = xmlGetNumericAttribute(node, "service_id", 16);
					l.name = std::string(xmlGetAttribute(node, "linkage_descriptor"));
					e.linkage_descs.insert(e.linkage_descs.end(), l);

					node = node->xmlNextNode;
				}
				addEvent(e, 0);
				ev_count++;

				event = event->xmlNextNode;
			}

			service = service->xmlNextNode;
		}
		xmlFreeDoc(event_parser);

		eventfile = eventfile->xmlNextNode;
	}

	xmlFreeDoc(index_parser);
	printdate_ms(stdout);
	printf("[sectionsd] Reading Information finished after %ld miliseconds (%d events)\n",
			time_monotonic_ms()-now, ev_count);

	reader_ready = true;

	pthread_exit(NULL);
}

static void write_epg_xml_header(FILE * fd, const t_original_network_id onid, const t_transport_stream_id tsid, const t_service_id sid)
{
	fprintf(fd,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!--\n"
		"  This file was automatically generated by the sectionsd.\n"
		"  It contains all event entries which have been cached\n"
		"  at time the box was shut down.\n"
		"-->\n"
		"<dvbepg>\n");
	fprintf(fd,"\t<service original_network_id=\"%04x\" transport_stream_id=\"%04x\" service_id=\"%04x\">\n",onid,tsid,sid);
}

static void write_index_xml_header(FILE * fd)
{
	fprintf(fd,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!--\n"
		"  This file was automatically generated by the sectionsd.\n"
		"  It contains all event entries which have been cached\n"
		"  at time the box was shut down.\n"
		"-->\n"
		"<dvbepgfiles>\n");
}

static void write_epgxml_footer(FILE *fd)
{
	fprintf(fd, "\t</service>\n");
	fprintf(fd, "</dvbepg>\n");
}

static void write_indexxml_footer(FILE *fd)
{
	fprintf(fd, "</dvbepgfiles>\n");
}

void cp(char * from, char * to)
{
	char cmd[256];
	snprintf(cmd, 256, "cp -f %s %s", from, to);
	system(cmd);
}

void writeEventsToFile(char *epgdir)
{
	FILE * indexfile = NULL;
	FILE * eventfile =NULL;
	char filename[100] = "";
	char tmpname[100] = "";
	char eventname[17] = "";
	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;

	sprintf(tmpname, "%s/index.tmp", epgdir);

	if (!(indexfile = fopen(tmpname, "w"))) {
		printf("[sectionsd] unable to open %s for writing\n", tmpname);
		return;
	}

	printf("[sectionsd] Writing Information to file: %s\n", tmpname);

	write_index_xml_header(indexfile);

	readLockEvents();

	MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey::iterator e =
		mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.begin();
	for (; e != mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey.end(); ++e) {
		if ( (onid != (*e)->original_network_id) || (tsid != (*e)->transport_stream_id) || (sid != (*e)->service_id) ) {
			if (eventfile != NULL) {
				write_epgxml_footer(eventfile);
				fclose(eventfile);
			}
			onid = (*e)->original_network_id;
			tsid = (*e)->transport_stream_id;
			sid = (*e)->service_id;
			snprintf(eventname, 17, "%04x%04x%04x.xml", onid, tsid, sid);
			sprintf(filename, "%s/%s", epgdir, eventname);
			if (!(eventfile = fopen(filename, "w"))) {
				goto _done;
			}
			fprintf(indexfile, "\t<eventfile name=\"%s\"/>\n", eventname);
			write_epg_xml_header(eventfile, onid, tsid, sid);
		}
		(*e)->saveXML(eventfile);
	}
	write_epgxml_footer(eventfile);
	fclose(eventfile);
_done:
	unlockEvents();
	write_indexxml_footer(indexfile);
	fclose(indexfile);

	printf("[sectionsd] Writing Information finished\n");

	sprintf(filename, "%s/index.xml", epgdir);

	cp(tmpname, filename);
	unlink(tmpname);

	return ;
}
