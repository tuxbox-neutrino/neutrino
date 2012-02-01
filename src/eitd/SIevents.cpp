//
// $Id: SIevents.cpp,v 1.35 2008/08/16 19:23:18 seife Exp $
//
// classes SIevent and SIevents (dbox-II-project)
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
//

#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/poll.h> // fuer poll()

#include <set>
#include <algorithm>
#include <string>

#include "SIlanguage.hpp"
#include "SIutils.hpp"
#include "SIservices.hpp"
#include "SIevents.hpp"
#ifdef UPDATE_NETWORKS
#include "SIbouquets.hpp"
#include "SInetworks.hpp"
#endif
#include "SIsections.hpp"
#include <dmxapi.h>

const std::string languangeOFF = "OFF";

SIevent::SIevent(const struct eit_event *e)
{
	eventID = (e->event_id_hi << 8) | e->event_id_lo;
	time_t start_time = changeUTCtoCtime(((const unsigned char *)e) + 2);
	unsigned long duration = 0;

	if (!((e->duration_hi == 0xff) && (e->duration_mid == 0xff) && (e->duration_lo == 0xff)))
		duration = ((e->duration_hi)>>4)*10*3600L + ((e->duration_hi)&0x0f)*3600L +
			   ((e->duration_mid)>>4)*10*60L + ((e->duration_mid)&0x0f)*60L +
			   ((e->duration_lo)>>4)*10 + ((e->duration_lo)&0x0f);

	if (start_time && duration)
		times.insert(SItime(start_time, duration));

	running = (int)e->running_status;

	table_id = 0xFF; /* not set */
	version = 0xFF;
	service_id = 0;
	original_network_id = 0;
	transport_stream_id = 0;
}

SIevent::SIevent(const t_original_network_id _original_network_id, const t_transport_stream_id _transport_stream_id, const t_service_id _service_id,
		 const unsigned short _event_id)
{
	original_network_id = _original_network_id;
	transport_stream_id = _transport_stream_id;
	service_id          = _service_id;
	eventID		    = _event_id;
	table_id            = 0xFF; /* not set */
	version 	    = 0xFF;
	/*	contentClassification = "";
		userClassification = "";
		itemDescription = "";
		item = "";
		extendedText = "";*/
}

// Std-Copy
SIevent::SIevent(const SIevent &e)
{
	eventID=e.eventID;
	langName=e.langName;
	langText=e.langText;
//  startzeit=e.startzeit;
//  dauer=e.dauer;
	times=e.times;
	service_id          = e.service_id;
	original_network_id = e.original_network_id;
	transport_stream_id = e.transport_stream_id;
	itemDescription=e.itemDescription;
	item=e.item;
	langExtendedText=e.langExtendedText;
	contentClassification=e.contentClassification;
	userClassification=e.userClassification;
	components=e.components;
	ratings=e.ratings;
	linkage_descs=e.linkage_descs;
	running=e.running;
	vps = e.vps;
	table_id = e.table_id;
	version =  e.version;
}

int SIevent::saveXML(FILE *file, const char *serviceName) const
{
	if(saveXML0(file))
		return 1;
	if(serviceName) {
		if(fprintf(file, "    <service_name>")<0)
			return 2;
		saveStringToXMLfile(file, serviceName);
		if(fprintf(file, "</service_name>\n")<0)
			return 3;
	}
	return saveXML2(file);
}

char SIevent::getFSK() const
{
	for (SIparentalRatings::iterator it = ratings.begin(); it != ratings.end(); ++it)
	{
		if (it->countryCode == "DEU")
		{
			if ((it->rating >= 0x01) && (it->rating <= 0x0F))
				return (it->rating + 3);           //                     0x01 to 0x0F minimum age = rating + 3 years
			else
				return (it->rating == 0 ? 0 : 18); // return FSK 18 for : 0x10 to 0xFF defined by the broadcaster
		}
	}
	if (!ratings.empty())
	{
		if ((ratings.begin()->rating >= 0x01) && (ratings.begin()->rating <= 0x0F))
			return (ratings.begin()->rating + 3);
		else
			return (ratings.begin()->rating == 0 ? 0 : 18);
	}

	return 0x00;                                           //                     0x00         undefined
}

int SIevent::saveXML0(FILE *file) const
{
	if(fprintf(file, "\t\t<event id=\"%04x\">\n", eventID)<0)
		return 1;
	return 0;
}

int SIevent::saveXML2(FILE *file) const
{
	for (std::map<std::string, std::string>::const_iterator
			i = langName.begin() ;
			i != langName.end() ;
			i++) {
		if (i->second.length()) {
			fprintf(file, "\t\t\t<name lang=\"%s\" string=\"", i->first.c_str());
			saveStringToXMLfile(file, i->second.c_str());
			fprintf(file, "\"/>\n");
		}
	}
	for (std::map<std::string, std::string>::const_iterator
			i = langText.begin() ;
			i != langText.end() ;
			i++) {
		if (i->second.length()) {
			fprintf(file, "\t\t\t<text lang=\"%s\" string=\"", i->first.c_str());
			saveStringToXMLfile(file, i->second.c_str());
			fprintf(file, "\"/>\n");
		}
	}
	if(item.length()) {
		fprintf(file, "\t\t\t<item string=\"");
		saveStringToXMLfile(file, item.c_str());
		fprintf(file, "\"/>\n");
	}
	if(itemDescription.length()) {
		fprintf(file, "\t\t\t<item_description string=\"");
		saveStringToXMLfile(file, itemDescription.c_str());
		fprintf(file, "\"/>\n");
	}
	for (std::map<std::string, std::string>::const_iterator
			i = langExtendedText.begin() ;
			i != langExtendedText.end() ;
			i++) {
		if (i->second.length()) {
			fprintf(file, "\t\t\t<extended_text lang=\"%s\" string=\"", i->first.c_str());
			saveStringToXMLfile(file, i->second.c_str());
			fprintf(file, "\"/>\n");
		}
	}
	for_each(times.begin(), times.end(), saveSItimeXML(file));
	for(unsigned i=0; i<contentClassification.length(); i++)
		fprintf(file, "\t\t\t<content class=\"%02x\" user=\"%02x\"/>\n", contentClassification[i], userClassification[i]);
	for_each(components.begin(), components.end(), saveSIcomponentXML(file));
	for_each(ratings.begin(), ratings.end(), saveSIparentalRatingXML(file));
	for_each(linkage_descs.begin(), linkage_descs.end(), saveSIlinkageXML(file));
	fprintf(file, "\t\t</event>\n");
	return 0;
}

std::string SIevent::getName() const
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		std::map<std::string, std::string>::const_iterator it = langName.begin() ;
		if (it != langName.end()) return it->second;
		else return("");
	} else {
		std::string retval;
		SIlanguage::filter(langName, 1, retval);
		return retval;
	}
}

void SIevent::setName(const std::string &lang, const std::string &name)
{
	std::string tmp = name;
	std::replace(tmp.begin(), tmp.end(), '\n', ' ');
//printf("setName: lang %s text %s\n", lang.c_str(), name.c_str());
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		langName[languangeOFF] = tmp; //name;
	} else {
		langName[lang] = tmp; //name;
	}
}

std::string SIevent::getText() const
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		std::map<std::string, std::string>::const_iterator it = langText.begin() ;
		if (it != langText.end()) return it->second;
		else return("");
	} else {
		std::string retval;
		SIlanguage::filter(langText, 0, retval);
		return retval;
	}
}

void SIevent::setText(const std::string &lang, const std::string &text)
{
//printf("setText: lang %s text %s\n", lang.c_str(), text.c_str());
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		langText[languangeOFF] = text;
	} else {
		langText[lang] = text;
	}
}

std::string SIevent::getExtendedText() const
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		std::map<std::string, std::string>::const_iterator it = langExtendedText.begin() ;
		if (it != langExtendedText.end()) return it->second;
		else return("");
	} else {
		std::string retval;
		SIlanguage::filter(langExtendedText, 0, retval);
		return retval;
	}
}

void SIevent::appendExtendedText(const std::string &lang, const std::string &text)
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		langExtendedText[languangeOFF] += text;
	} else {
		langExtendedText[lang] += text;
	}
}

void SIevent::setExtendedText(const std::string &lang, const std::string &text)
{
//printf("setExtendedText: lang %s text %s\n", lang.c_str(), text.c_str());
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		langExtendedText[languangeOFF] = text;
	} else {
		langExtendedText[lang] = text;
	}
}

void SIevent::dump(void) const
{
	printf("Unique key: %llx\n", uniqueKey());
	if(original_network_id)
		printf("Original-Network-ID: %hu\n", original_network_id);
	if (service_id)
		printf("Service-ID: %hu\n", service_id);
	printf("Event-ID: %hu\n", eventID);
	if(item.length())
		printf("Item: %s\n", item.c_str());
	if(itemDescription.length())
		printf("Item-Description: %s\n", itemDescription.c_str());

	for (std::map<std::string, std::string>::const_iterator it = langName.begin() ;
			it != langName.end() ; ++it)
		printf("Name (%s): %s\n", it->first.c_str(), it->second.c_str());
	for (std::map<std::string, std::string>::const_iterator it = langText.begin() ;
			it != langText.end() ; ++it)
		printf("Text (%s): %s\n", it->first.c_str(), it->second.c_str());
	for (std::map<std::string, std::string>::const_iterator it = langExtendedText.begin() ;
			it != langExtendedText.end() ; ++it)
		printf("Extended-Text (%s): %s\n", it->first.c_str(), it->second.c_str());

	if(contentClassification.length()) {
		printf("Content classification:");
		for(unsigned i=0; i<contentClassification.length(); i++)
			printf(" 0x%02hhx", contentClassification[i]);
		printf("\n");
	}
	if(userClassification.length()) {
		printf("User classification:");
		for(unsigned i=0; i<userClassification.length(); i++)
			printf(" 0x%02hhx", userClassification[i]);
		printf("\n");
	}
	/*
	  if(startzeit)
	  printf("Startzeit: %s", ctime(&startzeit));
	  if(dauer)
	  printf("Dauer: %02u:%02u:%02u (%umin, %us)\n", dauer/3600, (dauer%3600)/60, dauer%60, dauer/60, dauer);
	*/
	for_each(times.begin(), times.end(), printSItime());
	for_each(components.begin(), components.end(), printSIcomponent());
	for_each(ratings.begin(), ratings.end(), printSIparentalRating());
	for_each(linkage_descs.begin(), linkage_descs.end(), printSIlinkage());
}

void SIevent::dumpSmall(void) const
{
	for (std::map<std::string, std::string>::const_iterator it = langName.begin() ;
			it != langName.end() ; ++it)
		printf("Name (%s): %s\n", it->first.c_str(), it->second.c_str());
	for (std::map<std::string, std::string>::const_iterator it = langText.begin() ;
			it != langText.end() ; ++it)
		printf("Text (%s): %s\n", it->first.c_str(), it->second.c_str());
	for (std::map<std::string, std::string>::const_iterator it = langExtendedText.begin() ;
			it != langExtendedText.end() ; ++it)
		printf("Extended-Text (%s): %s\n", it->first.c_str(), it->second.c_str());
	/*
	  if(startzeit)
	  printf("Startzeit: %s", ctime(&startzeit));
	  if(dauer)
	  printf("Dauer: %02u:%02u:%02u (%umin, %us)\n", dauer/3600, (dauer%3600)/60, dauer%60, dauer/60, dauer);
	*/
	for_each(times.begin(), times.end(), printSItime());
	for_each(ratings.begin(), ratings.end(), printSIparentalRating());
	for_each(linkage_descs.begin(), linkage_descs.end(), printSIlinkage());
}
