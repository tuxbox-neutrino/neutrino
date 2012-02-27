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

#include <stdio.h>
#include <time.h>

#include "SIlanguage.hpp"
#include "SIutils.hpp"
#include "SIevents.hpp"

#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/short_event_descriptor.h>
#include <dvbsi++/extended_event_descriptor.h>
#include <dvbsi++/linkage_descriptor.h>
#include <dvbsi++/component_descriptor.h>
#include <dvbsi++/content_descriptor.h>
#include <dvbsi++/parental_rating_descriptor.h>

const std::string languangeOFF = "OFF";

SIevent::SIevent(const t_original_network_id _original_network_id, const t_transport_stream_id _transport_stream_id, const t_service_id _service_id,
		 const unsigned short _event_id)
{
	original_network_id = _original_network_id;
	transport_stream_id = _transport_stream_id;
	service_id          = _service_id;
	eventID		    = _event_id;
	table_id            = 0xFF; /* not set */
	version 	    = 0xFF;
}

void SIevent::parse(Event &event)
{
	int tsidonid = (transport_stream_id << 16) | original_network_id;
	time_t start_time = parseDVBtime(event.getStartTimeMjd(), event.getStartTimeBcd());

	running = event.getRunningStatus();

	uint32_t duration = event.getDuration();
	uint8_t duration_hi = (duration >> 16) & 0xFF;
	uint8_t duration_mid = (duration >> 8) & 0xFF;
	uint8_t duration_lo = duration & 0xFF;

	if (!((duration_hi == 0xff) && (duration_mid == 0xff) && (duration_lo == 0xff)))
		duration = ((duration_hi)>>4)*10*3600L + ((duration_hi)&0x0f)*3600L +
			   ((duration_mid)>>4)*10*60L + ((duration_mid)&0x0f)*60L +
			   ((duration_lo)>>4)*10 + ((duration_lo)&0x0f);

//printf("SIevent::parse  : eventID %x start %d duration %d\n", eventID, (int) start_time, (int) duration);
	if (start_time && duration)
		times.insert(SItime(start_time, duration));
	const DescriptorList &dlist = *event.getDescriptors();
	for (DescriptorConstIterator dit = dlist.begin(); dit != dlist.end(); ++dit) {
		uint8_t dtype = (*dit)->getTag();
		if(dtype == SHORT_EVENT_DESCRIPTOR) {
			const ShortEventDescriptor *d = (ShortEventDescriptor*) *dit;
			std::string lang = d->getIso639LanguageCode();
			std::transform(lang.begin(), lang.end(), lang.begin(), tolower);
			int table = getCountryCodeDefaultMapping(lang);
			setName(lang, stringDVBUTF8(d->getEventName(), table, tsidonid));
			setText(lang, stringDVBUTF8(d->getText(), table, tsidonid));
		}
		else if(dtype == EXTENDED_EVENT_DESCRIPTOR) {
			const ExtendedEventDescriptor *d = (ExtendedEventDescriptor*) *dit;
			std::string lang = d->getIso639LanguageCode();
			std::transform(lang.begin(), lang.end(), lang.begin(), tolower);
			int table = getCountryCodeDefaultMapping(lang);

			const ExtendedEventList *itemlist = d->getItems();
			for (ExtendedEventConstIterator it = itemlist->begin(); it != itemlist->end(); ++it) {
				itemDescription.append(stringDVBUTF8((*it)->getItemDescription(), table, tsidonid));
				itemDescription.append("\n");
				item.append(stringDVBUTF8((*it)->getItem(), table, tsidonid));
				item.append("\n");
			}
			appendExtendedText(lang, stringDVBUTF8(d->getText(), table, tsidonid));
		}
		else if(dtype == CONTENT_DESCRIPTOR) {
			const ContentDescriptor * d = (ContentDescriptor *) *dit;
			const ContentClassificationList *clist = d->getClassifications();
			for (ContentClassificationConstIterator cit = clist->begin(); cit != clist->end(); ++cit) {
				ContentClassification * c = *cit;
				char content = c->getContentNibbleLevel1() << 4 | c->getContentNibbleLevel2();
				contentClassification += content;
				char user = c->getUserNibble1() << 4 | c->getUserNibble2();
				contentClassification += user;
			}
		}
		else if(dtype == COMPONENT_DESCRIPTOR) {
			const ComponentDescriptor *d = (ComponentDescriptor*)*dit;
			SIcomponent c;
			c.streamContent = d->getStreamContent();
			c.componentType = d->getComponentType();
			c.componentTag = d->getComponentTag();
			std::string lang = d->getIso639LanguageCode();
			std::transform(lang.begin(), lang.end(), lang.begin(), tolower);
			int table = getCountryCodeDefaultMapping(lang);
			c.component = stringDVBUTF8(d->getText(), table, tsidonid);
			components.insert(c);
		}
		else if(dtype == PARENTAL_RATING_DESCRIPTOR) {
			const ParentalRatingDescriptor *d = (ParentalRatingDescriptor*) *dit;
			const ParentalRatingList *plist = d->getParentalRatings();
			for (ParentalRatingConstIterator it = plist->begin(); it != plist->end(); ++it) {
				SIparentalRating p((*it)->getCountryCode(), (*it)->getRating());
				ratings.insert(p);
			}
		}
		else if(dtype == LINKAGE_DESCRIPTOR) {
			const LinkageDescriptor * d = (LinkageDescriptor *) *dit;
			SIlinkage l;
			l.linkageType = d->getLinkageType();
			l.transportStreamId = d->getTransportStreamId();
			l.originalNetworkId = d->getOriginalNetworkId();
			l.serviceId = d->getServiceId();
			const PrivateDataByteVector *privateData = d->getPrivateDataBytes();
			l.name = convertDVBUTF8((const char*)&((*privateData)[0]), privateData->size(), 1, tsidonid);
			linkage_descs.insert(linkage_descs.end(), l);
		}
#if 0 // TODO ? vps was never used
		else if(dtype == PDC_DESCRIPTOR) {
		}
#endif
	}
}

char SIevent::getFSK() const
{
	for (SIparentalRatings::iterator it = ratings.begin(); it != ratings.end(); ++it)
	{
		if (it->countryCode == "DEU")
		{
			if ((it->rating >= 0x01) && (it->rating <= 0x0F))
				return (it->rating + 3);           // 0x01 to 0x0F minimum age = rating + 3 years
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

	return 0x00; // 0x00         undefined
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

int SIevent::saveXML0(FILE *file) const
{
	if(fprintf(file, "\t\t<event id=\"%04x\" tid=\"%02x\">\n", eventID, table_id)<0)
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

	for_each(times.begin(), times.end(), printSItime());
	for_each(ratings.begin(), ratings.end(), printSIparentalRating());
	for_each(linkage_descs.begin(), linkage_descs.end(), printSIlinkage());
}
