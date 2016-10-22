/*
 * class SIevents (dbox-II-project)
 *
 * Copyright (C) 2001 fnbrd (fnbrd@gmx.de)
 * Homepage: http://dbox2.elxsi.de
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 * Copyright (C) 2008, 2012 Stefan Seyfried
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

#include <stdio.h>
#include <time.h>

#include <map>

#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/short_event_descriptor.h>
#include <dvbsi++/extended_event_descriptor.h>
#include <dvbsi++/linkage_descriptor.h>
#include <dvbsi++/component_descriptor.h>
#include <dvbsi++/content_descriptor.h>
#include <dvbsi++/parental_rating_descriptor.h>

#include "SIlanguage.hpp"
#include "SIutils.hpp"
#include "SIevents.hpp"

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#include <OpenThreads/ScopedLock>

struct descr_generic_header {
	unsigned descriptor_tag                 : 8;
	unsigned descriptor_length              : 8;
} __attribute__ ((packed)) ;

struct descr_short_event_header {
	unsigned descriptor_tag                 : 8;
	unsigned descriptor_length              : 8;
	unsigned language_code_hi               : 8;
	unsigned language_code_mid              : 8;
	unsigned language_code_lo               : 8;
	unsigned event_name_length              : 8;
} __attribute__ ((packed)) ;

struct descr_extended_event_header {
	unsigned descriptor_tag                 : 8;
	unsigned descriptor_length              : 8;
	unsigned descriptor_number              : 4;
	unsigned last_descriptor_number         : 4;
	unsigned iso_639_2_language_code_hi     : 8;
	unsigned iso_639_2_language_code_mid    : 8;
	unsigned iso_639_2_language_code_lo     : 8;
	unsigned length_of_items                : 8;
} __attribute__ ((packed)) ;

inline unsigned min(unsigned a, unsigned b)
{
        return b < a ? b : a;
}

static OpenThreads::Mutex countryMutex;
static std::vector<std::string> countryVector;

unsigned int getCountryIndex(const std::string &country)
{
	unsigned int ix = 0;
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(countryMutex);
	if (countryVector.empty()) {
		countryVector.push_back("DEU"); // 0
		countryVector.push_back("FRA"); // 1
		countryVector.push_back("ITA"); // 2
		countryVector.push_back("ESP"); // 3
	}
	for (std::vector<std::string>::iterator it = countryVector.begin(); it != countryVector.end(); ++it, ++ix)
		if (*it == country)
			return ix;
	countryVector.push_back(country);
	return ix;
}

static OpenThreads::Mutex componentMutex;
static std::vector<std::string> componentVector;
static std::map<std::string,unsigned int> componentMap;

void SIcomponent::setComponent(const std::string &component_description)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(componentMutex);
	if (componentVector.empty()) {
		componentMap[""] = 0;
		componentVector.push_back("");
	}
	std::map<std::string,unsigned int>::const_iterator it = componentMap.find(component_description);
	if (it == componentMap.end()) {
		component = componentVector.size();
		componentMap[component_description] = component;
		componentVector.push_back(component_description);
	} else
		component = it->second;
}

const char *SIcomponent::getComponentName() const
{
	if (component < componentVector.size())
		return componentVector[component].c_str();
	return "";
}

SIparentalRating::SIparentalRating(const std::string &cc, unsigned char rate)
{
	rating=rate;
	countryCode=getCountryIndex(cc);
}

void SIparentalRating::dump(void) const
{
	printf("Rating: %s %hhu (+3)\n", countryVector[countryCode].c_str(), rating);
}

int SIparentalRating::saveXML(FILE *file) const
{
	if(fprintf(file, "\t\t\t<parental_rating country=\"%s\" rating=\"%hhu\"/>\n", countryVector[countryCode].c_str(), rating)<0)
		return 1;
	return 0;
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
	running 	    = 0;
}

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

void SIevent::parse(Event &event)
{
	int tsidonid = (transport_stream_id << 16) | original_network_id;
	time_t start_time = parseDVBtime(event.getStartTimeMjd(), event.getStartTimeBcd());
	extern long int secondsExtendedTextCache;
	time_t now = time(NULL);

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

	int countExtandetText = 0, appendcheck = 0;
	for (DescriptorConstIterator dit = dlist.begin(); dit != dlist.end(); ++dit){
		switch ((*dit)->getTag()) {
			case EXTENDED_EVENT_DESCRIPTOR:
				countExtandetText++;
			break;
			default:
			break;
		}
	}
	appendcheck = countExtandetText;
	for (DescriptorConstIterator dit = dlist.begin(); dit != dlist.end(); ++dit) {
	    switch ((*dit)->getTag()) {
		case SHORT_EVENT_DESCRIPTOR:
		{
			const ShortEventDescriptor *d = (ShortEventDescriptor*) *dit;
			std::string lang = d->getIso639LanguageCode();
			std::transform(lang.begin(), lang.end(), lang.begin(), tolower);
			int table = getCountryCodeDefaultMapping(lang);
			unsigned int _lang = getLangIndex(lang);
			setName(_lang, stringDVBUTF8(d->getEventName(), table, tsidonid));
			setText(_lang, stringDVBUTF8(d->getText(), table, tsidonid));
			break;
		}
		case EXTENDED_EVENT_DESCRIPTOR:
		{
			if(now && secondsExtendedTextCache && start_time > now + secondsExtendedTextCache)
				continue;
			const ExtendedEventDescriptor *d = (ExtendedEventDescriptor*) *dit;
			std::string lang = d->getIso639LanguageCode();
			std::transform(lang.begin(), lang.end(), lang.begin(), tolower);
			int table = getCountryCodeDefaultMapping(lang);
#ifdef USE_ITEM_DESCRIPTION
			const ExtendedEventList *itemlist = d->getItems();
			for (ExtendedEventConstIterator it = itemlist->begin(); it != itemlist->end(); ++it) {
				itemDescription.append(stringDVBUTF8((*it)->getItemDescription(), table, tsidonid));
				itemDescription.append("\n");
				item.append(stringDVBUTF8((*it)->getItem(), table, tsidonid));
				item.append("\n");
			}
#endif
			appendExtendedText(getLangIndex(lang), stringDVBUTF8(d->getText(), table, tsidonid),(appendcheck > countExtandetText),(countExtandetText==1));
			countExtandetText--;
			break;
		}
		case CONTENT_DESCRIPTOR:
		{
			const ContentDescriptor * d = (ContentDescriptor *) *dit;
			const ContentClassificationList *clist = d->getClassifications();
			if (clist->size()) {
#ifdef FULL_CONTENT_CLASSIFICATION
				ssize_t off = classifications.reserve(clist->size() * 2);
				for (ContentClassificationConstIterator cit = clist->begin(); cit != clist->end(); ++cit)
					off = classifications.set(off,
								  (*cit)->getContentNibbleLevel1() << 4 | (*cit)->getContentNibbleLevel2(),
								  (*cit)->getUserNibble1() << 4 | (*cit)->getUserNibble2());
#else
				ContentClassificationConstIterator cit = clist->begin();
				classifications.content = (*cit)->getContentNibbleLevel1() << 4 | (*cit)->getContentNibbleLevel2();
				classifications.user = (*cit)->getUserNibble1() << 4 | (*cit)->getUserNibble2();
#endif
			}
			break;
		}
		case COMPONENT_DESCRIPTOR:
		{
			const ComponentDescriptor *d = (ComponentDescriptor*)*dit;
			SIcomponent c;
			c.streamContent = d->getStreamContent();
			c.componentType = d->getComponentType();
			c.componentTag = d->getComponentTag();
			std::string lang = d->getIso639LanguageCode();
			std::transform(lang.begin(), lang.end(), lang.begin(), tolower);
			int table = getCountryCodeDefaultMapping(lang);
			c.setComponent(stringDVBUTF8(d->getText(), table, tsidonid));
			//components.insert(c);
			components.push_back(c);
			break;
		}
		case PARENTAL_RATING_DESCRIPTOR:
		{
			const ParentalRatingDescriptor *d = (ParentalRatingDescriptor*) *dit;
			const ParentalRatingList *plist = d->getParentalRatings();
			for (ParentalRatingConstIterator it = plist->begin(); it != plist->end(); ++it) {
				SIparentalRating p((*it)->getCountryCode(), (*it)->getRating());
				//ratings.insert(p);
				ratings.push_back(p);
			}
			break;
		}
		case LINKAGE_DESCRIPTOR:
		{
			const LinkageDescriptor * d = (LinkageDescriptor *) *dit;
			SIlinkage l;
			l.linkageType = d->getLinkageType();
			l.transportStreamId = d->getTransportStreamId();
			l.originalNetworkId = d->getOriginalNetworkId();
			l.serviceId = d->getServiceId();
			const PrivateDataByteVector *privateData = d->getPrivateDataBytes();
			l.name = convertDVBUTF8((const char*)&((*privateData)[0]), privateData->size(), 1, tsidonid);
			linkage_descs.insert(linkage_descs.end(), l);
			break;
		}
#if 0 // TODO ? vps was never used
		case PDC_DESCRIPTOR) {
			break;
		}
#endif
		default:
			break;
	    }
	}
}

void SIevent::parseDescriptors(const uint8_t *des, unsigned len)
{
	struct descr_generic_header *desc;
	/* we pass the buffer including the eit_event header, so we have to
	 *            skip it here... */
	des += sizeof(struct eit_event);
	len -= sizeof(struct eit_event);
	while(len>=sizeof(struct descr_generic_header)) {
		desc=(struct descr_generic_header *)des;
		/*printf("Type: %s\n", decode_descr(desc->descriptor_tag)); */
		if(desc->descriptor_tag==SHORT_EVENT_DESCRIPTOR)
			parseShortEventDescriptor((const uint8_t *)desc, len);
		else if(desc->descriptor_tag==EXTENDED_EVENT_DESCRIPTOR)
			parseExtendedEventDescriptor((const uint8_t *)desc, len);
		else if(desc->descriptor_tag==CONTENT_DESCRIPTOR)
			parseContentDescriptor((const uint8_t *)desc, len);
		else if(desc->descriptor_tag==COMPONENT_DESCRIPTOR)
			parseComponentDescriptor((const uint8_t *)desc, len);
		else if(desc->descriptor_tag==PARENTAL_RATING_DESCRIPTOR)
			parseParentalRatingDescriptor((const uint8_t *)desc, len);
		else if(desc->descriptor_tag==LINKAGE_DESCRIPTOR) {
			parseLinkageDescriptor((const uint8_t *)desc, len);
		}
#if 0
		else if(desc->descriptor_tag==PDC_DESCRIPTOR)
			parsePDCDescriptor((const char *)desc, e, len);
#endif
		if((unsigned)(desc->descriptor_length+2)>len)
			break;
		len-=desc->descriptor_length+2;
		des+=desc->descriptor_length+2;
	}
}

void SIevent::parseShortEventDescriptor(const uint8_t *buf, unsigned maxlen)
{
        struct descr_short_event_header *evt=(struct descr_short_event_header *)buf;
        if((evt->descriptor_length+sizeof(descr_generic_header) > maxlen) ||
			(evt->descriptor_length<sizeof(struct descr_short_event_header)-sizeof(descr_generic_header)))
                return;

        int tsidonid = (transport_stream_id << 16) | original_network_id;

	char lang[4];
	lang[0] = tolower(evt->language_code_hi);
	lang[1] = tolower(evt->language_code_mid);
	lang[2] = tolower(evt->language_code_lo);
	lang[3] = '\0';
        std::string language(lang);
	int table = getCountryCodeDefaultMapping(language);
	unsigned int _language = getLangIndex(language);

        buf+=sizeof(struct descr_short_event_header);
        if(evt->event_name_length)
                setName(_language, convertDVBUTF8((const char*) buf, evt->event_name_length, table, tsidonid));

        buf+=evt->event_name_length;
        unsigned char textlength=*((unsigned char *)buf);
        if(textlength > 2)
                setText(_language, convertDVBUTF8((const char*) (++buf), textlength, table, tsidonid));
}

void SIevent::parseExtendedEventDescriptor(const uint8_t *buf, unsigned maxlen)
{
        struct descr_extended_event_header *evt=(struct descr_extended_event_header *)buf;
        if((evt->descriptor_length+sizeof(descr_generic_header)>maxlen) ||
			(evt->descriptor_length<sizeof(struct descr_extended_event_header)-sizeof(descr_generic_header)))
                return;

        int tsidonid = (transport_stream_id << 16) | original_network_id;

	char lang[4];
	lang[0] = tolower(evt->iso_639_2_language_code_hi);
	lang[1] = tolower(evt->iso_639_2_language_code_mid);
	lang[2] = tolower(evt->iso_639_2_language_code_lo);
	lang[3] = '\0';
        std::string language(lang);
	int table = getCountryCodeDefaultMapping(language);
	unsigned int _language = getLangIndex(language);

        unsigned char *items=(unsigned char *)(buf+sizeof(struct descr_extended_event_header));
        while(items < (unsigned char *)(buf + sizeof(struct descr_extended_event_header) + evt->length_of_items)) {
#ifdef USE_ITEM_DESCRIPTION
                if(*items) {
                        itemDescription.append(convertDVBUTF8((const char *)(items+1), min(maxlen-(items+1-buf), *items), table, tsidonid));
                        itemDescription.append("\n");
                }
#endif
                items+=1+*items;
#ifdef USE_ITEM_DESCRIPTION
                if(*items) {
                        item.append(convertDVBUTF8((const char *)(items+1), min(maxlen-(items+1-buf), *items), table, tsidonid));
                        item.append("\n");
                }
#endif
                items+=1+*items;
        }
        if(*items) 
                appendExtendedText(_language, convertDVBUTF8((const char *)(items+1), min(maxlen-(items+1-buf), (*items)), table, tsidonid));
}

void SIevent::parseContentDescriptor(const uint8_t *buf, unsigned maxlen)
{
	struct descr_generic_header *cont=(struct descr_generic_header *)buf;
	if(cont->descriptor_length+sizeof(struct descr_generic_header)>maxlen)
		return;
	if(!cont->descriptor_length)
		return;
#ifdef FULL_CONTENT_CLASSIFICATION
	ssize_t off = classifications.reserve(cont->descriptor_length);
	classifications.set(off, buf + sizeof(struct descr_generic_header), cont->descriptor_length);
#else
	classifications.content = buf[sizeof(struct descr_generic_header)];
	if (cont->descriptor_length > 1)
		classifications.user = buf[sizeof(struct descr_generic_header)+1];
#endif
}

void SIevent::parseComponentDescriptor(const uint8_t *buf, unsigned maxlen)
{
	if(maxlen>=sizeof(struct descr_component_header))
		components.push_back(SIcomponent((const struct descr_component_header *)buf));
		//components.insert(SIcomponent((const struct descr_component_header *)buf));
}

void SIevent::parseParentalRatingDescriptor(const uint8_t *buf, unsigned maxlen)
{
        struct descr_generic_header *cont=(struct descr_generic_header *)buf;
        if(cont->descriptor_length+sizeof(struct descr_generic_header)>maxlen)
                return;
        const uint8_t *s=buf+sizeof(struct descr_generic_header);
        while(s<buf+sizeof(struct descr_generic_header)+cont->descriptor_length-4) {
                //ratings.insert(SIparentalRating(std::string((const char *)s, 3), *(s+3)));
                ratings.push_back(SIparentalRating(std::string((const char *)s, 3), *(s+3)));
                s+=4;
        }
}

void SIevent::parseLinkageDescriptor(const uint8_t *buf, unsigned maxlen)
{
	if(maxlen>=sizeof(struct descr_linkage_header)) {
		SIlinkage l((const struct descr_linkage_header *)buf);
		linkage_descs.insert(linkage_descs.end(), l);
	}
}

char SIevent::getFSK() const
{
	for (SIparentalRatings::const_iterator it = ratings.begin(); it != ratings.end(); ++it)
	{
		if (it->countryCode == 0 /*"DEU"*/) {
			if ((it->rating >= 0x01) && (it->rating <= 0x0F))
				return (it->rating + 3);           // 0x01 to 0x0F minimum age = rating + 3 years
			return (it->rating == 0 ? 0 : 18); // return FSK 18 for : 0x10 to 0xFF defined by the broadcaster
		}
		if( it->countryCode == 1 /*"FRA"*/ && it->rating == 0x10) {
			// workaround for ITA ESP FRA fsk.
		  	return 0;
		}
		if(it->countryCode == 2 /*"ITA"*/ && it->rating == 1) {
			return 0;
		}
		if( it->countryCode == 3 /*"ESP"*/ ) {
			if(it->rating == 0x10 || it->rating == 0x11)
				return 0;
			if(it->rating == 0x12)
				return 18;
			return (it->rating + 1);
		}
	}
	if (!ratings.empty())
	{
		if ((ratings.begin()->rating >= 0x01) && (ratings.begin()->rating <= 0x0F))
			return (ratings.begin()->rating + 3);
		return (ratings.begin()->rating == 0 ? 0 : 18);
	}

	return 0x00; // 0x00         undefined
}

std::string SIevent::getName() const
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		if (!langData.empty())
			return langData.begin()->text[SILangData::langName];
		return "";
	} else {
		std::string retval;
		SIlanguage::filter(langData, SILangData::langName, 1, retval);
		return retval;
	}
}

void SIevent::setName(const std::string &lang, const std::string &name)
{
	setName(getLangIndex(lang), name);
}

void SIevent::setName(unsigned int lang, const std::string &name)
{
	std::string tmp = name;
	std::replace(tmp.begin(), tmp.end(), '\n', ' ');

	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode())
		lang = 0;

	for (std::list<SILangData>::iterator it = langData.begin(); it != langData.end(); ++it)
		if (it->lang == lang) {
			it->text[SILangData::langName] = tmp;
			return;
		}

	SILangData ld;
	ld.lang = lang;
	ld.text[SILangData::langName] = tmp;
	langData.push_back(ld);
}

std::string SIevent::getText() const
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		if (!langData.empty())
			return langData.begin()->text[SILangData::langText];
		return "";
	}
	std::string retval;
	SIlanguage::filter(langData, SILangData::langText, 0, retval);
	return retval;
}

void SIevent::setText(const std::string &lang, const std::string &text)
{
	setText(getLangIndex(lang), text);
}

void SIevent::setText(unsigned int lang, const std::string &text)
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode())
		lang = 0;

	for (std::list<SILangData>::iterator it = langData.begin(); it != langData.end(); ++it)
		if (it->lang == lang) {
			it->text[SILangData::langText] = text;
			return;
		}

	SILangData ld;
	ld.lang = lang;
	ld.text[SILangData::langText] = text;
	langData.push_back(ld);
}

std::string SIevent::getExtendedText() const
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode()) {
		if (!langData.empty())
			return langData.begin()->text[SILangData::langExtendedText];
		return "";
	}
	std::string retval;
	SIlanguage::filter(langData, SILangData::langExtendedText, 0, retval);
	return retval;
}

void SIevent::appendExtendedText(const std::string &lang, const std::string &text, bool append)
{
	appendExtendedText(getLangIndex(lang), text, append);
}

void SIevent::appendExtendedText(unsigned int lang, const std::string &text, bool append, bool endappend)
{
	if (CSectionsdClient::LANGUAGE_MODE_OFF == SIlanguage::getMode())
		lang = 0;

	for (std::list<SILangData>::iterator it = langData.begin(); it != langData.end(); ++it)
		if (it->lang == lang) {
			if (append){
				it->text[SILangData::langExtendedText] += text;
				if(endappend && it->text[SILangData::langExtendedText].capacity() > it->text[SILangData::langExtendedText].size()){
					it->text[SILangData::langExtendedText].reserve();
				}
			}
			else{
				it->text[SILangData::langExtendedText] = text;
			}
			return;
		}

	SILangData ld;
	ld.lang = lang;
	ld.text[SILangData::langExtendedText] = text;
	langData.push_back(ld);
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
	for (std::list<SILangData>::const_iterator i = langData.begin(); i != langData.end(); ++i) {
		if (!i->text[SILangData::langName].empty()) {
			fprintf(file, "\t\t\t<name lang=\"%s\" string=\"", langIndex[i->lang].c_str());
			saveStringToXMLfile(file, i->text[SILangData::langName].c_str());
			fprintf(file, "\"/>\n");
		}
	}
	for (std::list<SILangData>::const_iterator i = langData.begin(); i != langData.end(); ++i) {
		if (!i->text[SILangData::langText].empty()) {
			fprintf(file, "\t\t\t<text lang=\"%s\" string=\"", langIndex[i->lang].c_str());
			saveStringToXMLfile(file, i->text[SILangData::langText].c_str());
			fprintf(file, "\"/>\n");
		}
	}
#ifdef USE_ITEM_DESCRIPTION
	if(!item.empty()) {
		fprintf(file, "\t\t\t<item string=\"");
		saveStringToXMLfile(file, item.c_str());
		fprintf(file, "\"/>\n");
	}
	if(!itemDescription.empty()) {
		fprintf(file, "\t\t\t<item_description string=\"");
		saveStringToXMLfile(file, itemDescription.c_str());
		fprintf(file, "\"/>\n");
	}
#endif
	for (std::list<SILangData>::const_iterator i = langData.begin(); i != langData.end(); ++i) {
		if (!i->text[SILangData::langExtendedText].empty()) {
			fprintf(file, "\t\t\t<extended_text lang=\"%s\" string=\"", langIndex[i->lang].c_str());
			saveStringToXMLfile(file, i->text[SILangData::langExtendedText].c_str());
			fprintf(file, "\"/>\n");
		}
	}
	for_each(times.begin(), times.end(), saveSItimeXML(file));
#ifdef FULL_CONTENT_CLASSIFICATION
	std::string contentClassification, userClassification;
	classifications.get(contentClassification, userClassification);
	for(unsigned i=0; i<contentClassification.length(); i++) {
		/* focus: i think no sense to save 'unknown' */
		if(contentClassification[i] || userClassification[i])
			fprintf(file, "\t\t\t<content class=\"%02x\" user=\"%02x\"/>\n", contentClassification[i], userClassification[i]);
	}
#else
	if (classifications.content || classifications.user)
		fprintf(file, "\t\t\t<content class=\"%02x\" user=\"%02x\"/>\n", classifications.content, classifications.user);
#endif

	for_each(components.begin(), components.end(), saveSIcomponentXML(file));
	for_each(ratings.begin(), ratings.end(), saveSIparentalRatingXML(file));
	for_each(linkage_descs.begin(), linkage_descs.end(), saveSIlinkageXML(file));
	fprintf(file, "\t\t</event>\n");
	return 0;
}

void SIevent::dump(void) const
{
	printf("Unique key: %" PRIx64 "\n", uniqueKey());
	if(original_network_id)
		printf("Original-Network-ID: %hu\n", original_network_id);
	if (service_id)
		printf("Service-ID: %hu\n", service_id);
	printf("Event-ID: %hu\n", eventID);
#ifdef USE_ITEM_DESCRIPTION
	if(!item.empty())
		printf("Item: %s\n", item.c_str());
	if(!itemDescription.empty())
		printf("Item-Description: %s\n", itemDescription.c_str());
#endif
	for (std::list<SILangData>::const_iterator it = langData.begin(); it != langData.end(); ++it) {
		printf("Name (%s): %s\n",	   langIndex[it->lang].c_str(), it->text[SILangData::langName].c_str());
		printf("Text (%s): %s\n",	   langIndex[it->lang].c_str(), it->text[SILangData::langText].c_str());
		printf("Extended-Text (%s): %s\n", langIndex[it->lang].c_str(), it->text[SILangData::langExtendedText].c_str());
	}

#ifdef FULL_CONTENT_CLASSIFICATION
	std::string contentClassification, userClassification;
	classifications.get(contentClassification, userClassification);
	if(!contentClassification.empty()) {
		printf("Content classification:");
		for(unsigned i=0; i<contentClassification.length(); i++)
			printf(" 0x%02hhx", contentClassification[i]);
		printf("\n");
	}
	if(!userClassification.empty()) {
		printf("User classification:");
		for(unsigned i=0; i<userClassification.length(); i++)
			printf(" 0x%02hhx", userClassification[i]);
		printf("\n");
	}
#else
	if (classifications.content || classifications.user) {
		printf("Content classification: 0x%02hhx\n", classifications.content);
		printf("User classification: 0x%02hhx\n", classifications.user);
	}
#endif

	for_each(times.begin(), times.end(), printSItime());
	for_each(components.begin(), components.end(), printSIcomponent());
	for_each(ratings.begin(), ratings.end(), printSIparentalRating());
	for_each(linkage_descs.begin(), linkage_descs.end(), printSIlinkage());
}
#if 0 
//never used
void SIevent::dumpSmall(void) const
{
	for (std::list<std::pair<unsigned int, std::string> >::const_iterator it = langName.begin() ;
			it != langName.end() ; ++it)
		printf("Name (%s): %s\n", it->first.c_str(), it->second.c_str());
	for (std::list<std::pair<unsigned int, std::string> >::const_iterator it = langText.begin() ;
			it != langText.end() ; ++it)
		printf("Text (%s): %s\n", it->first.c_str(), it->second.c_str());
	for (std::list<std::pair<unsigned int, std::string> >::const_iterator it = langExtendedText.begin() ;
			it != langExtendedText.end() ; ++it)
		printf("Extended-Text (%s): %s\n", it->first.c_str(), it->second.c_str());

	for_each(times.begin(), times.end(), printSItime());
	for_each(ratings.begin(), ratings.end(), printSIparentalRating());
	for_each(linkage_descs.begin(), linkage_descs.end(), printSIlinkage());
}
#endif
