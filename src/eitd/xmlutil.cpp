/*
 * (C) 2001 by fnbrd,
 * Copyright (C) 2008-2009, 2012-2013 Stefan Seyfried
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
#include <dirent.h>
#include <string>
#include <system/helpers.h>

#include <xmltree/xmlinterface.h>
#include <zapit/client/zapittools.h>

#include <driver/abstime.h>

#include "xmlutil.h"
#include "eitd.h"
#include "debug.h"
#include <system/set_threadname.h>

void addEvent(const SIevent &evt, const time_t zeit, bool cn = false);
extern MySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey mySIeventsOrderServiceUniqueKeyFirstStartTimeEventUniqueKey;
extern bool reader_ready;
extern pthread_rwlock_t eventsLock;
extern bool dvb_time_update;

std::string epg_filter_dir = CONFIGDIR "/zapit/epgfilter.xml";
std::string dvbtime_filter_dir = CONFIGDIR "/zapit/dvbtimefilter.xml";
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

void addEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid)
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

void clearEPGFilter()
{
	EPGFilter *filterptr = CurrentEPGFilter;
	while (filterptr)
	{
		EPGFilter *filternext = filterptr->next;
		delete filterptr;
		filterptr = filternext;
	}
	CurrentEPGFilter = NULL;
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
		xprintf("Add Channel Blacklist for channel 0x%012" PRIx64 ", mask 0x%012" PRIx64 "\n", channel_id, mask);
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
		xprintf("Add channel 0x%012" PRIx64 ", mask 0x%012" PRIx64 " to NoDVBTimelist\n", channel_id, mask);
		ChannelNoDVBTimelist *node = new ChannelNoDVBTimelist;
		node->chan = channel_id;
		node->mask = mask;
		node->next = CurrentNoDVBTime;
		CurrentNoDVBTime = node;
	}
}

bool readEPGFilter(void)
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
		filter = xmlChildrenNode(filter);

		while (filter) {

			onid = xmlGetNumericAttribute(filter, "onid", 16);
			tsid = xmlGetNumericAttribute(filter, "tsid", 16);
			sid  = xmlGetNumericAttribute(filter, "serviceID", 16);
			if (xmlGetNumericAttribute(filter, "blacklist", 10) == 1)
				addBlacklist(onid, tsid, sid);
			else
				addEPGFilter(onid, tsid, sid);

			filter = xmlNextNode(filter);
		}
	}
	xmlFreeDoc(filter_parser);
	return (CurrentEPGFilter != NULL);
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
		filter = xmlChildrenNode(filter);

		while (filter) {

			onid = xmlGetNumericAttribute(filter, "onid", 16);
			tsid = xmlGetNumericAttribute(filter, "tsid", 16);
			sid  = xmlGetNumericAttribute(filter, "serviceID", 16);
			addNoDVBTimelist(onid, tsid, sid);

			filter = xmlNextNode(filter);
		}
		xmlFreeDoc(filter_parser);
	}
	else
	{
		dvb_time_update = true;
	}
}

void deleteOldfileEvents(const char *epgdir)
{
	std::string indexname = std::string(epgdir) + "/index.xml";
	xmlDocPtr filter_parser = parseXmlFile(indexname.c_str());
	std::string filename;
	std::string file;

	if (filter_parser != NULL)
	{
		xmlNodePtr filter = xmlDocGetRootElement(filter_parser);
		filter = xmlChildrenNode(filter);

		while (filter) {
			const char * name = xmlGetAttribute(filter, "name");
			if(name){
				filename=name;
				file = epgdir;
				file +="/";
				file +=filename;
				unlink(file.c_str());
				filter = xmlNextNode(filter);
			}
		}
		xmlFreeDoc(filter_parser);
	}
}

bool readEventsFromFile(std::string &epgname, int &ev_count)
{
	xmlDocPtr event_parser = NULL;
	xmlNodePtr service;
	xmlNodePtr event;
	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;

	if (!(event_parser = parseXmlFile(epgname.c_str()))) {
		dprintf("unable to open %s for reading\n", epgname.c_str());
		return false;
	}
	service = xmlDocGetRootElement(event_parser);
	service = xmlChildrenNode(service);

	while (service) {
		onid = xmlGetNumericAttribute(service, "original_network_id", 16);
		tsid = xmlGetNumericAttribute(service, "transport_stream_id", 16);
		sid = xmlGetNumericAttribute(service, "service_id", 16);
		if(!onid || !tsid || !sid){
			service = xmlNextNode(service);
			continue;
		}
		event = xmlChildrenNode(service);

		while (event) {
			SIevent e(onid,tsid,sid,xmlGetNumericAttribute(event, "id", 16));
			uint8_t tid = xmlGetNumericAttribute(event, "tid", 16);
			std::string contentClassification, userClassification;
			if(tid)
				e.table_id = tid;
			e.table_id |= 0x80; /* make sure on-air data has a lower table_id */

			xmlNodePtr node;

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "name"))) {
				const char *s = xmlGetAttribute(node, "string");
				if (s)
					e.setName(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "lang")), s);
				node = xmlNextNode(node);
			}

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "text"))) {
				const char *s = xmlGetAttribute(node, "string");
				if (s)
					e.setText(ZapitTools::UTF8_to_Latin1(xmlGetAttribute(node, "lang")), s);
				node = xmlNextNode(node);
			}
			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "item"))) {
#ifdef USE_ITEM_DESCRIPTION
				const char *s = xmlGetAttribute(node, "string");
				if (s)
					e.item = s;
#endif
				node = xmlNextNode(node);
			}

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "item_description"))) {
#ifdef USE_ITEM_DESCRIPTION
				const char *s = xmlGetAttribute(node, "string");
				if (s)
					e.itemDescription = s;
#endif
				node = xmlNextNode(node);
			}
			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "extended_text"))) {
				const char *l = xmlGetAttribute(node, "lang");
				const char *s = xmlGetAttribute(node, "string");
				if (l && s)
					e.appendExtendedText(ZapitTools::UTF8_to_Latin1(l), s);
				node = xmlNextNode(node);
			}

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "time"))) {
				e.times.insert(SItime(xmlGetNumericAttribute(node, "start_time", 10),
							xmlGetNumericAttribute(node, "duration", 10)));
				node = xmlNextNode(node);
			}

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "content"))) {
				const char cl = xmlGetNumericAttribute(node, "class", 16);
				contentClassification += cl;
				const char cl2 = xmlGetNumericAttribute(node, "user", 16);
				userClassification += cl2;
				node = xmlNextNode(node);
			}

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "component"))) {
				SIcomponent c;
				c.streamContent = xmlGetNumericAttribute(node, "stream_content", 16);
				c.componentType = xmlGetNumericAttribute(node, "type", 16);
				c.componentTag = xmlGetNumericAttribute(node, "tag", 16);
				const char *s = xmlGetAttribute(node, "text");
				if (s)
					c.setComponent(s);
				//e.components.insert(c);
				e.components.push_back(c);
				node = xmlNextNode(node);
			}

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "parental_rating"))) {
				const char *s = xmlGetAttribute(node, "country");
				if (s)
#if 0
					e.ratings.insert(SIparentalRating(ZapitTools::UTF8_to_Latin1(s),
								(unsigned char) xmlGetNumericAttribute(node, "rating", 10)));
#endif
				e.ratings.push_back(SIparentalRating(ZapitTools::UTF8_to_Latin1(s),
							(unsigned char) xmlGetNumericAttribute(node, "rating", 10)));
				node = xmlNextNode(node);
			}

			node = xmlChildrenNode(event);
			while ((node = xmlGetNextOccurence(node, "linkage"))) {
				SIlinkage l;
				l.linkageType = xmlGetNumericAttribute(node, "type", 16);
				l.transportStreamId = xmlGetNumericAttribute(node, "transport_stream_id", 16);
				l.originalNetworkId = xmlGetNumericAttribute(node, "original_network_id", 16);
				l.serviceId = xmlGetNumericAttribute(node, "service_id", 16);
				const char *s = xmlGetAttribute(node, "linkage_descriptor");
				if (s)
					l.name = s;
				e.linkage_descs.insert(e.linkage_descs.end(), l);
				node = xmlNextNode(node);
			}

			if (!contentClassification.empty()) {
#ifdef FULL_CONTENT_CLASSIFICATION
				ssize_t off = e.classifications.reserve(2 * contentClassification.size());
				if (off > -1)
					for (unsigned i = 0; i < contentClassification.size(); i++)
						off = e.classifications.set(off, contentClassification.at(i), userClassification.at(i));
#else
				e.classifications.content = contentClassification.at(0);
				e.classifications.user = userClassification.at(0);
#endif
			}
			addEvent(e, 0);
			ev_count++;

			event = xmlNextNode(event);
		}

		service = xmlNextNode(service);
	}
	xmlFreeDoc(event_parser);
	return true;
}

static int my_filter(const struct dirent *entry)
{
	int len = strlen(entry->d_name);
	if (len > 3 && entry->d_name[len-3] == 'x' && entry->d_name[len-2] == 'm' && entry->d_name[len-1] == 'l')
		return 1;
	return 0;
}

bool readEventsFromDir(std::string &epgdir, int &ev_count)
{
	struct dirent **namelist;
	int n = scandir(epgdir.c_str(), &namelist, my_filter, NULL);
	printf("[sectionsd] Reading Information from directory %s, file count %d\n", epgdir.c_str(), n);
	if (n <= 0)
		return false;

	for (int i = 0; i < n; i++) {
		std::string epgname = epgdir + namelist[i]->d_name;
		readEventsFromFile(epgname, ev_count);
		free(namelist[i]);
	}
	free(namelist);
	return true;
}

void *insertEventsfromFile(void * data)
{
	set_threadname(__func__);
	reader_ready=false;
	std::string indexname;
	std::string filename;
	std::string epgname;
	xmlNodePtr eventfile;
	int ev_count = 0;
	if (!data) {
		reader_ready = true;
		pthread_exit(NULL);
	}
	std::string epg_dir = (char *) data;
	indexname = epg_dir + "index.xml";

	int64_t now = time_monotonic_ms();
	xmlDocPtr index_parser = parseXmlFile(indexname.c_str());

	if (index_parser == NULL) {
		readEventsFromDir(epg_dir, ev_count);
		printf("[sectionsd] Reading Information finished after %" PRId64 " milliseconds (%d events)\n",
				time_monotonic_ms()-now, ev_count);
		reader_ready = true;
		pthread_exit(NULL);
	}
	printdate_ms(stdout);
	printf("[sectionsd] Reading Information from file %s:\n", indexname.c_str());

	eventfile = xmlDocGetRootElement(index_parser);
	eventfile = xmlChildrenNode(eventfile);

	while (eventfile) {
		const char * name = xmlGetAttribute(eventfile, "name");
		if(name)
			filename=name;

		epgname = epg_dir + filename;
		readEventsFromFile(epgname, ev_count);

		eventfile = xmlNextNode(eventfile);
	}

	xmlFreeDoc(index_parser);
	printdate_ms(stdout);
	printf("[sectionsd] Reading Information finished after %" PRId64 " milliseconds (%d events)\n",
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

void writeEventsToFile(const char *epgdir)
{
	if(check_dir(epgdir)){
		return;
	}

	FILE * indexfile = NULL;
	FILE * eventfile = NULL;
	std::string filename("");
	std::string tmpname("");
	char eventname[17] = "";
	t_original_network_id onid = 0;
	t_transport_stream_id tsid = 0;
	t_service_id sid = 0;
	deleteOldfileEvents(epgdir);

	tmpname  = (std::string)epgdir + "/index.tmp";

	if (!(indexfile = fopen(tmpname.c_str(), "w"))) {
		printf("[sectionsd] unable to open %s for writing\n", tmpname.c_str());
		return;
	}

	printf("[sectionsd] Writing Information to file: %s\n", tmpname.c_str());

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
			snprintf(eventname, 17, "%04x%04x%04x.xml", tsid, onid, sid);
			filename  = (std::string)epgdir + "/" + (std::string)eventname;
			if (!(eventfile = fopen(filename.c_str(), "w"))) {
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

	filename  = (std::string)epgdir + "/index.xml";
	rename(tmpname.c_str(), filename.c_str());

	// sync();
	printf("[sectionsd] Writing Information finished\n");
	return ;
}
