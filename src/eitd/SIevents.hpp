/*
 * (C) 2001 by fnbrd (fnbrd@gmx.de),
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
#ifndef SIEVENTS_HPP
#define SIEVENTS_HPP

#include <vector>
#include <list>
#include <set>
#include <algorithm>
#include <string>

#include <sectionsdclient/sectionsdtypes.h>
#include <dvbsi++/event_information_section.h>
#include "edvbstring.h"
#include "SIlanguage.hpp"

//#define USE_ITEM_DESCRIPTION

struct eit_event {
	unsigned event_id_hi                    : 8;
	unsigned event_id_lo                    : 8;
	unsigned start_time_hi                  : 8;
	unsigned start_time_hi2                 : 8;
	unsigned start_time_mid                 : 8;
	unsigned start_time_lo2                 : 8;
	unsigned start_time_lo                  : 8;
	unsigned duration_hi                    : 8;
	unsigned duration_mid                   : 8;
	unsigned duration_lo                    : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned running_status                 : 3;
	unsigned free_CA_mode                   : 1;
	unsigned descriptors_loop_length_hi     : 4;
#else
	unsigned descriptors_loop_length_hi     : 4;
	unsigned free_CA_mode                   : 1;
	unsigned running_status                 : 3;
#endif
	unsigned descriptors_loop_length_lo     : 8;
} __attribute__ ((packed)) ;


struct descr_component_header {
	unsigned descriptor_tag                 : 8;
	unsigned descriptor_length              : 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved_future_use            : 4;
	unsigned stream_content                 : 4;
#else
	unsigned stream_content                 : 4;
	unsigned reserved_future_use            : 4;
#endif
	unsigned component_type                 : 8;
	unsigned component_tag                  : 8;
	unsigned iso_639_2_language_code_hi     : 8;
	unsigned iso_639_2_language_code_mid    : 8;
	unsigned iso_639_2_language_code_lo     : 8;
} __attribute__ ((packed)) ;

struct descr_linkage_header {
	unsigned descriptor_tag                 : 8;
	unsigned descriptor_length              : 8;
	unsigned transport_stream_id_hi         : 8;
	unsigned transport_stream_id_lo         : 8;
	unsigned original_network_id_hi         : 8;
	unsigned original_network_id_lo         : 8;
	unsigned service_id_hi                  : 8;
	unsigned service_id_lo                  : 8;
	unsigned linkage_type                   : 8;
} __attribute__ ((packed)) ;

struct descr_pdc_header {
	unsigned descriptor_tag                 : 8;
#if 0
	// unused
	unsigned descriptor_length              : 8;
	unsigned pil0                           : 8;
	unsigned pil1                           : 8;
	unsigned pil2                           : 8;
#endif
} __attribute__ ((packed)) ;

class SIlinkage {
public:
	unsigned char linkageType;
	std::string name;
	t_transport_stream_id transportStreamId;
	t_original_network_id originalNetworkId;
	t_service_id          serviceId;

	SIlinkage(void) {
		linkageType = 0;
		transportStreamId = 0;
		originalNetworkId = 0;
		serviceId = 0;
	}

	SIlinkage(const struct descr_linkage_header *link) {
		linkageType = link->linkage_type;
		transportStreamId = (link->transport_stream_id_hi << 8) | link->transport_stream_id_lo;
		originalNetworkId = (link->original_network_id_hi << 8) | link->original_network_id_lo;
		serviceId = (link->service_id_hi << 8) | link->service_id_lo;
		if (link->descriptor_length > sizeof(struct descr_linkage_header) - 2)
			name = convertDVBUTF8(((const char *)link)+sizeof(struct descr_linkage_header), link->descriptor_length-(sizeof(struct descr_linkage_header)-2), 0, 0);
	}

	void dump(void) const {
		printf("Linkage Type: 0x%02hhx\n", linkageType);
		if (name.length())
			printf("Name: %s\n", name.c_str());
		printf("Transport Stream Id: 0x%04hhx\n", transportStreamId);
		printf("Original Network Id: 0x%04hhx\n", originalNetworkId);
		printf("Service Id: 0x%04hhx\n", serviceId);
	}

	int saveXML(FILE *file) const {
		fprintf(file, "\t\t\t<linkage type=\"%02x\" linkage_descriptor=\"", linkageType);
		saveStringToXMLfile(file, name.c_str());
		fprintf(file, "\" transport_stream_id=\"%04x\" original_network_id=\"%04x\" service_id=\"%04x\" />\n", transportStreamId, originalNetworkId, serviceId);
//		%s, , name.c_str())<0)
//			return 1;
		return 0;
	}
	// Der Operator zum sortieren
	bool operator < (const SIlinkage& l) const {
		return name < l.name;
	}
	bool operator==(const SIlinkage& s) const {
		return (linkageType == s.linkageType) &&
			(transportStreamId == s.transportStreamId) &&
			(originalNetworkId == s.originalNetworkId) &&
			(serviceId == s.serviceId) &&
			(name == s.name);
	}
	bool operator!=(const SIlinkage& s) const {
		return (linkageType != s.linkageType) ||
			(transportStreamId != s.transportStreamId) ||
			(originalNetworkId != s.originalNetworkId) ||
			(serviceId != s.serviceId) ||
			(name != s.name);
	}
};
typedef std::vector<class SIlinkage> SIlinkage_descs;

// Fuer for_each
struct printSIlinkage : public std::unary_function<class SIlinkage, void>
{
	void operator() (const SIlinkage &l) { l.dump();}
};

// Fuer for_each
struct saveSIlinkageXML : public std::unary_function<class SIlinkage, void>
{
	FILE *f;
	saveSIlinkageXML(FILE *fi) { f=fi;}
	void operator() (const SIlinkage &l) { l.saveXML(f);}
};

class SIcomponent 
{
	public:
		unsigned int component;
		unsigned char componentType;
		unsigned char componentTag;
		unsigned char streamContent;

		SIcomponent(void) {
			component = 0;
			streamContent=0;
			componentType=0;
			componentTag=0;      
		}
		SIcomponent(const struct descr_component_header *comp) {
			component = 0;
			streamContent=comp->stream_content;
			componentType=comp->component_type;
			componentTag=comp->component_tag;
			if(comp->descriptor_length>sizeof(struct descr_component_header)-2)
				setComponent(convertDVBUTF8(((const char *)comp) + sizeof(struct descr_component_header),
					comp->descriptor_length-(sizeof(struct descr_component_header)-2), 0, 0));
		}

		void setComponent(const std::string &component_description);
		const char *getComponentName() const;

		void dump(void) const {
			const char *comp = getComponentName();
			if(*comp)
				printf("Component: %s\n", comp);
			printf("Stream Content: 0x%02hhx\n", streamContent);
			printf("Component type: 0x%02hhx\n", componentType);
			printf("Component tag: 0x%02hhx\n", componentTag);
		}
		int saveXML(FILE *file) const {
			fprintf(file, "\t\t\t<component tag=\"%02x\" type=\"%02x\" stream_content=\"%02x\" text=\"", componentTag, componentType, streamContent);
			saveStringToXMLfile(file, getComponentName());
			fprintf(file, "\"/>\n");
			return 0;
		}
		// Der Operator zum sortieren
		bool operator < (const SIcomponent& c) const {
			return streamContent < c.streamContent;
		}
		bool operator==(const SIcomponent& c) const {
			return (componentType == c.componentType) &&
				(componentTag == c.componentTag) &&
				(streamContent == c.streamContent) &&
				(component == c.component);
		}
		bool operator!=(const SIcomponent& c) const {
			return (componentType != c.componentType) ||
				(componentTag != c.componentTag) ||
				(streamContent != c.streamContent) ||
				(component != c.component);
		}
};

//typedef std::multiset <SIcomponent, std::less<SIcomponent> > SIcomponents;
typedef std::vector <SIcomponent> SIcomponents;

// Fuer for_each
struct printSIcomponent : public std::unary_function<class SIcomponent, void>
{
	void operator() (const SIcomponent &c) { c.dump();}
};

// Fuer for_each
struct saveSIcomponentXML : public std::unary_function<class SIcomponent, void>
{
	FILE *f;
	saveSIcomponentXML(FILE *fi) { f=fi;}
	void operator() (const SIcomponent &c) { c.saveXML(f);}
};

class SIparentalRating 
{
	public:
		unsigned int countryCode;
		unsigned char rating; // Bei 1-16 -> Minimales Alter = rating +3

		SIparentalRating(const std::string &cc, unsigned char rate);

		// Der Operator zum sortieren
		bool operator < (const SIparentalRating& c) const {
			return countryCode < c.countryCode;
		}
		bool operator==(const SIparentalRating& p) const {
			return (rating == p.rating) &&
				(countryCode == p.countryCode);
		}
		bool operator!=(const SIparentalRating& p) const {
			return (rating != p.rating) ||
				(countryCode != p.countryCode);
		}
		void dump(void) const;
		int saveXML(FILE *file) const;
};
//typedef std::set <SIparentalRating, std::less<SIparentalRating> > SIparentalRatings;
typedef std::vector <SIparentalRating> SIparentalRatings;

// Fuer for_each
struct printSIparentalRating : public std::unary_function<SIparentalRating, void>
{
	void operator() (const SIparentalRating &r) { r.dump();}
};

// Fuer for_each
struct saveSIparentalRatingXML : public std::unary_function<SIparentalRating, void>
{
	FILE *f;
	saveSIparentalRatingXML(FILE *fi) { f=fi;}
	void operator() (const SIparentalRating &r) { r.saveXML(f);}
};

class SItime {
	public:
		time_t startzeit;  // lokale Zeit, 0 -> time shifted (cinedoms)
		unsigned dauer; // in Sekunden, 0 -> time shifted (cinedoms)

		SItime(time_t s, unsigned d) {
			startzeit=s;
			dauer=d; // in Sekunden, 0 -> time shifted (cinedoms)
		}

		// Der Operator zum sortieren
		bool operator < (const SItime& t) const {
			return startzeit < t.startzeit;
		}
		void dump(void) const {
			printf("Startzeit: %s", ctime(&startzeit));
			printf("Dauer: %02u:%02u:%02u (%umin, %us)\n", dauer/3600, (dauer%3600)/60, dauer%60, dauer/60, dauer);
		}
		int saveXML(FILE *file) const { // saves the time
			fprintf(file, "\t\t\t<time start_time=\"%u\" duration=\"%u\"/>\n", (unsigned int) startzeit, dauer);
			return 0;
		}
		bool operator==(const SItime& t) const {
			return (dauer == t.dauer) &&
				(startzeit == t.startzeit);
		}
		bool operator!=(const SItime& t) const {
			return (dauer != t.dauer) ||
				(startzeit != t.startzeit);
		}
};

typedef std::set <SItime, std::less<SItime> > SItimes;

// Fuer for_each
struct printSItime : public std::unary_function<SItime, void>
{
	void operator() (const SItime &t) { t.dump();}
};

// Fuer for_each
struct saveSItimeXML : public std::unary_function<SItime, void>
{
	FILE *f;
	saveSItimeXML(FILE *fi) { f=fi;}
	void operator() (const SItime &t) { t.saveXML(f);}
};

class SIevent
{
	private:
		std::list<SILangData> langData;
		int running;

		void parseShortEventDescriptor(const uint8_t *buf, unsigned maxlen);
		void parseExtendedEventDescriptor(const uint8_t *buf, unsigned maxlen);
		void parseContentDescriptor(const uint8_t *buf, unsigned maxlen);
		void parseComponentDescriptor(const uint8_t *buf, unsigned maxlen);
		void parseParentalRatingDescriptor(const uint8_t *buf, unsigned maxlen);
		void parseLinkageDescriptor(const uint8_t *buf, unsigned maxlen);

	protected:
		int saveXML0(FILE *f) const;
		int saveXML2(FILE *f) const;

	public:
		t_service_id          service_id;
		t_original_network_id original_network_id;
		t_transport_stream_id transport_stream_id;
		unsigned short eventID;
		//time_t vps;
		unsigned char table_id;
		unsigned char version;

		SIcomponents components;
		SIparentalRatings ratings;
		SIlinkage_descs linkage_descs;
		SItimes times;

#ifdef USE_ITEM_DESCRIPTION
		std::string itemDescription; // Aus dem Extended Descriptor
		std::string item; // Aus dem Extended Descriptor
#endif
		struct SIeventClassifications
		{
#ifdef FULL_CONTENT_CLASSIFICATION
			uint8_t *data;
			unsigned int size;

			SIeventClassifications& operator = (const SIeventClassifications& c)
			{
				if (this != &c) {
					size = 0;
					if (data) {
						free(data);
						data = NULL;
					}
					if (c.data) {
						data = (uint8_t *) malloc(c.size);
						if (data) {
							memcpy(data, c.data, c.size);
							size = c.size;
						}
					}
				}
				return *this;
			}

			SIeventClassifications(const SIeventClassifications& c)
			{
				if (this != &c) {
					data = NULL;
					*this = c;
				}
			}

			bool operator==(const SIeventClassifications& c) const
			{
				if (!data && !c.data)
					return true;
				if (!(data && c.data))
					return false;
				if (size != c.size)
					return false;
				return !memcmp(data, c.data, size);
			}

			bool operator!=(const SIeventClassifications& c) const
			{
				return *this != c;
			}

			SIeventClassifications()
			{
				data = NULL;
				size = 0;
			}

			~SIeventClassifications()
			{
				if (data)
					free(data);
			}

			void get(std::string &contentClassifications, std::string &userClassifications) const
			{
				contentClassifications.clear();
				userClassifications.clear();
				if (size) {
					uint8_t *d = data, *e = data + size;
					uint8_t cc[size/2], uc[size/2];
					for (unsigned int i = 0; d < e; i++) {
						cc[i] = *d++;
						uc[i] = *d++;
					}
					contentClassifications.assign((char *) cc, size/2);
					userClassifications.assign((char *) uc, size/2);
				}
			}

			ssize_t reserve(unsigned int r)
			{
				if (r & 1)
					return -1;

				if (size) {
					uint8_t * _data = (uint8_t *) realloc(data, size + r);
					if (!_data)
						return -1;
					data = _data;
				} else {
					data = (uint8_t *) malloc(r);
					if (!data)
						return -1;
				}
				size_t off = size;
				size += r;
				return off;
			}

			ssize_t set(ssize_t off, uint8_t content, uint8_t user)
			{
				if (off < 0 || off + 2 > (ssize_t) size)
					return -1;
				data[off++] = content;
				data[off++] = user;
				return off;
			}

			ssize_t set(ssize_t off, const uint8_t *_data, size_t len)
			{
				if (len & 1 || off < 0 || off + len > size)
					return -1;
				memcpy (data + off, _data, len);
				return off + len;
			}
#else
			uint8_t content;
			uint8_t user;

			SIeventClassifications()
			{
				content = 0;
				user = 0;
			}
#endif
		};

		SIeventClassifications classifications;

		SIevent(const t_original_network_id, const t_transport_stream_id, const t_service_id, const unsigned short);
		SIevent(void) {
			service_id = 0;
			original_network_id = 0;
			transport_stream_id = 0;
			eventID    = 0;
			//vps = 0;
			table_id = 0xFF; /* 0xFF means "not set" */
			version = 0xFF;
			running = false;
		}
		SIevent(const struct eit_event *e);

		void parse(Event &event);
		void parseDescriptors(const uint8_t *des, unsigned len);

		// Name aus dem Short-Event-Descriptor
		std::string getName() const;
		void setName(const std::string &lang, const std::string &name);
		void setName(unsigned int lang, const std::string &name);

		// Text aus dem Short-Event-Descriptor
		std::string getText() const;
		void setText(const std::string &lang, const std::string &text);
		void setText(unsigned int lang, const std::string &text);

		// Aus dem Extended Descriptor
		std::string getExtendedText() const;
		void appendExtendedText(const std::string &lang, const std::string &text, bool append = true);
		void appendExtendedText(unsigned int lang, const std::string &text, bool append = true, bool endappend = false);
		void setExtendedText(const std::string &lang, const std::string &text) {
			appendExtendedText(lang, text, false);
		}
		void setExtendedText(unsigned int lang, const std::string &text) {
			appendExtendedText(lang, text, false);
		}

		t_channel_id get_channel_id(void) const {
			return CREATE_CHANNEL_ID(service_id, original_network_id, transport_stream_id);
		}

		t_event_id uniqueKey(void) const {
			return CREATE_EVENT_ID(get_channel_id(), eventID);
		}
		int runningStatus(void) const {
			return running;
		}
		// Der Operator zum sortieren
		bool operator < (const SIevent& e) const {
			return uniqueKey()<e.uniqueKey();
		}
		int saveXML(FILE *file) const { // saves the event
			return saveXML0(file) || saveXML2(file);
		}
		int saveXML(FILE *file, const char *serviceName) const; // saves the event
		char getFSK() const;

		void dump(void) const; // dumps the event to stdout
		void dumpSmall(void) const; // dumps the event to stdout (not all information)
};

//typedef std::set <SIevent, std::less<SIevent> > SIevents;
typedef std::vector <SIevent> SIevents;

// Fuer for_each
struct printSIevent : public std::unary_function<SIevent, void>
{
	void operator() (const SIevent &e) { e.dump();}
};

// Fuer for_each
struct saveSIeventXML : public std::unary_function<SIevent, void>
{
	FILE *f;
	saveSIeventXML(FILE *fi) { f=fi;}
	void operator() (const SIevent &e) { e.saveXML(f);}
};

#if 0
// Fuer for_each
struct saveSIeventXMLwithServiceName : public std::unary_function<SIevent, void>
{
  FILE *f;
  const SIservices *s;
  saveSIeventXMLwithServiceName(FILE *fi, const SIservices &svs) {f=fi; s=&svs;}
  void operator() (const SIevent &e) {
    SIservices::iterator k=s->find(SIservice(e.service_id, e.original_network_id, e.transport_stream_id));
    if(k!=s->end()) {
      if(k->serviceName.length())
      e.saveXML(f, k->serviceName.c_str());
    }
    else
      e.saveXML(f);
  }
};
#endif

#if 0
// Fuer for_each
struct printSIeventWithService : public std::unary_function<SIevent, void>
{
  printSIeventWithService(const SIservices &svs) { s=&svs;}
  void operator() (const SIevent &e) {
    SIservices::iterator k=s->find(SIservice(e.service_id, e.original_network_id, e.transport_stream_id));
    if(k!=s->end()) {
      char servicename[50];
      strncpy(servicename, k->serviceName.c_str(), sizeof(servicename)-1);
      servicename[sizeof(servicename)-1]=0;
      removeControlCodes(servicename);
      printf("Service-Name: %s\n", servicename);
//      printf("Provider-Name: %s\n", k->providerName.c_str());
    }
    e.dump();
//    e.dumpSmall();
    printf("\n");
  }
  const SIservices *s;
};

class SIevents : public std::set <SIevent, std::less<SIevent> >
{
  public:
    // Entfernt anhand der Services alle time shifted events (Service-Typ 0)
    // und sortiert deren Zeiten in die Events mit dem Text ein.
    void mergeAndRemoveTimeShiftedEvents(const SIservices &);
    // Loescht alte Events (aufgrund aktueller Zeit - seconds und Zeit im Event)
    void removeOldEvents(long seconds);
};
#endif

#endif // SIEVENTS_HPP
