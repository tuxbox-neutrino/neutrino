/*
 * (C) 2001 by fnbrd,
 * Copyright (C) 2008, 2009 Stefan Seyfried
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include <xmltree/xmlinterface.h>
#include <zapit/client/zapittools.h>

#include <driver/abstime.h>

#include "xmlutil.h"
#include "eitd.h"
#include "debug.h"

void addEvent(const SIevent &evt, const time_t zeit, bool cn = false);
extern MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey;
extern bool reader_ready;
extern pthread_rwlock_t eventsLock;
extern bool dvb_time_update;

std::string epg_filter_dir = "/var/tuxbox/config/zapit/epgfilter.xml";
std::string dvbtime_filter_dir = "/var/tuxbox/config/zapit/dvbtimefilter.xml";
bool epg_filter_is_whitelist = false;
bool epg_filter_except_current_next = false;

inline void readLockEvents(void)
{
	pthread_rwlock_rdlock(&eventsLock);
}
inline void unlockEvents(void)
{
	pthread_rwlock_unlock(&eventsLock);
}

struct EPGFilter
{
	t_original_network_id onid;
	t_transport_stream_id tsid;
	t_service_id sid;
	EPGFilter *next;
};

struct ChannelBlacklist
{
	t_channel_id chan;
	t_channel_id mask;
	ChannelBlacklist *next;
};

struct ChannelNoDVBTimelist
{
	t_channel_id chan;
	t_channel_id mask;
	ChannelNoDVBTimelist *next;
};

static EPGFilter *CurrentEPGFilter = NULL;
static ChannelBlacklist *CurrentBlacklist = NULL;
static ChannelNoDVBTimelist *CurrentNoDVBTime = NULL;

bool checkEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	EPGFilter *filterptr = CurrentEPGFilter;
	while (filterptr)
	{
		if (((filterptr->onid == onid) || (filterptr->onid == 0)) &&
				((filterptr->tsid == tsid) || (filterptr->tsid == 0)) &&
				((filterptr->sid == sid) || (filterptr->sid == 0)))
			return true;
		filterptr = filterptr->next;
	}
	return false;
}

bool checkBlacklist(t_channel_id channel_id)
{
	ChannelBlacklist *blptr = CurrentBlacklist;
	while (blptr)
	{
		if (blptr->chan == (channel_id & blptr->mask))
			return true;
		blptr = blptr->next;
	}
	return false;
}

bool checkNoDVBTimelist(t_channel_id channel_id)
{
	ChannelNoDVBTimelist *blptr = CurrentNoDVBTime;
	while (blptr)
	{
		if (blptr->chan == (channel_id & blptr->mask))
			return true;
		blptr = blptr->next;
	}
	return false;
}

static void addEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	if (!checkEPGFilter(onid, tsid, sid))
	{
		dprintf("Add EPGFilter for onid=\"%04x\" tsid=\"%04x\" service_id=\"%04x\"\n", onid, tsid, sid);
		EPGFilter *node = new EPGFilter;
		node->onid = onid;
		node->tsid = tsid;
		node->sid = sid;
		node->next = CurrentEPGFilter;
		CurrentEPGFilter = node;
	}
}

static void addBlacklist(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	t_channel_id channel_id =
		CREATE_CHANNEL_ID(sid, onid, tsid);
	t_channel_id mask =
		CREATE_CHANNEL_ID(
				(sid ? 0xFFFF : 0), (onid ? 0xFFFF : 0), (tsid ? 0xFFFF : 0)
				);
	if (!checkBlacklist(channel_id))
	{
		xprintf("Add Channel Blacklist for channel 0x%012llx, mask 0x%012llx\n", channel_id, mask);
		ChannelBlacklist *node = new ChannelBlacklist;
		node->chan = channel_id;
		node->mask = mask;
		node->next = CurrentBlacklist;
		CurrentBlacklist = node;
	}
}

static void addNoDVBTimelist(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
{
	t_channel_id channel_id =
		CREATE_CHANNEL_ID(sid, onid, tsid);
	t_channel_id mask =
		CREATE_CHANNEL_ID(
				(sid ? 0xFFFF : 0), (onid ? 0xFFFF : 0), (tsid ? 0xFFFF : 0)
				);
	if (!checkNoDVBTimelist(channel_id))
	{
		xprintf("Add channel 0x%012llx, mask 0x%012llx to NoDVBTimelist\n", channel_id, mask);
		ChannelNoDVBTimelist *node = new ChannelNoDVBTimelist;
		node->chan = channel_id;
		node->mask = mask;
		node->next = CurrentNoDVBTime;
		CurrentNoDVBTime = node;
	}
}

void readEPGFilter(void)
{
	xmlDocPtr filter_parser = parseXmlFile(epg_filter_dir.c_str());

	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;

	if (filter_parser != NULL)
	{
		dprintf("Reading EPGFilters\n");

		xmlNodePtr filter = xmlDocGetRootElement(filter_parser);
		if (xmlGetNumericAttribute(filter, "is_whitelist", 10) == 1)
			epg_filter_is_whitelist = true;
		if (xmlGetNumericAttribute(filter, "except_current_next", 10) == 1)
			epg_filter_except_current_next = true;
		filter = filter->xmlChildrenNode;

		while (filter) {

			onid = xmlGetNumericAttribute(filter, "onid", 16);
			tsid = xmlGetNumericAttribute(filter, "tsid", 16);
			sid  = xmlGetNumericAttribute(filter, "serviceID", 16);
			if (xmlGetNumericAttribute(filter, "blacklist", 10) == 1)
				addBlacklist(onid, tsid, sid);
			else
				addEPGFilter(onid, tsid, sid);

			filter = filter->xmlNextNode;
		}
	}
	xmlFreeDoc(filter_parser);
}

void readDVBTimeFilter(void)
{
	xmlDocPtr filter_parser = parseXmlFile(dvbtime_filter_dir.c_str());

	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;

	if (filter_parser != NULL)
	{
		dprintf("Reading DVBTimeFilters\n");

		xmlNodePtr filter = xmlDocGetRootElement(filter_parser);
		filter = filter->xmlChildrenNode;

		while (filter) {

			onid = xmlGetNumericAttribute(filter, "onid", 16);
			tsid = xmlGetNumericAttribute(filter, "tsid", 16);
			sid  = xmlGetNumericAttribute(filter, "serviceID", 16);
			addNoDVBTimelist(onid, tsid, sid);

			filter = filter->xmlNextNode;
		}
		xmlFreeDoc(filter_parser);
	}
	else
	{
		dvb_time_update = true;
	}
}

void deleteOldfileEvents(char *epgdir)
{
	std::string indexname = std::string(epgdir) + "/index.xml";
	xmlDocPtr filter_parser = parseXmlFile(indexname.c_str());
	std::string filename;
	std::string file;

	if (filter_parser != NULL)
	{
		xmlNodePtr filter = xmlDocGetRootElement(filter_parser);
		filter = filter->xmlChildrenNode;

		while (filter) {
			filename = xmlGetAttribute(filter, "name");
			file = epgdir;
			file +="/";
			file +=filename;
			unlink(file.c_str());
			filter = filter->xmlNextNode;
		}
		xmlFreeDoc(filter_parser);
	}
}

void *insertEventsfromFile(void * data)
{
	reader_ready=false;
	xmlDocPtr event_parser = NULL;
	xmlNodePtr eventfile = NULL;
	xmlNodePtr service = NULL;
	xmlNodePtr event = NULL;
	xmlNodePtr node = NULL;
	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;
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
				uint8_t tid = xmlGetNumericAttribute(event, "tid", 16);
				if(tid)
					e.table_id = tid;

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
#ifdef USE_ITEM_DESCRIPTION
				while (xmlGetNextOccurence(node, "item") != NULL) {
					e.item = std::string(xmlGetAttribute(node, "string"));
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "item_description") != NULL) {
					e.itemDescription = std::string(xmlGetAttribute(node, "string"));
					node = node->xmlNextNode;
				}
#endif
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

				while (xmlGetNextOccurence(node, "content") != NULL) {
					char cl = xmlGetNumericAttribute(node, "class", 16);
					e.contentClassification += cl;
					cl = xmlGetNumericAttribute(node, "user", 16);
					e.userClassification += cl;
					node = node->xmlNextNode;
				}

				while (xmlGetNextOccurence(node, "component") != NULL) {
					SIcomponent c;
					c.streamContent = xmlGetNumericAttribute(node, "stream_content", 16);
					c.componentType = xmlGetNumericAttribute(node, "type", 16);
					c.componentTag = xmlGetNumericAttribute(node, "tag", 16);
					c.component = std::string(xmlGetAttribute(node, "text"));
					//e.components.insert(c);
					e.components.push_back(c);
					node = node->xmlNextNode;
				}
				while (xmlGetNextOccurence(node, "parental_rating") != NULL) {
#if 0
					e.ratings.insert(SIparentalRating(std::string(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "country"))),
								(unsigned char) xmlGetNumericAttribute(node, "rating", 10)));
#endif
					e.ratings.push_back(SIparentalRating(std::string(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "country"))),
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
	deleteOldfileEvents(epgdir);


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

	rename(tmpname, filename);

	return ;
}
