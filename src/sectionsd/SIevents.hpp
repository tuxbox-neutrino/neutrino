#ifndef SIEVENTS_HPP
#define SIEVENTS_HPP
//
// $Id: SIevents.hpp,v 1.29 2008/08/16 19:23:18 seife Exp $
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

#include <endian.h>
#include <vector>
#include <map>

#include <sectionsdclient/sectionsdtypes.h>
#include "edvbstring.h"
//#include "SIutils.hpp"

// forward references
class SIservice;
class SIservices;
class SIbouquets;

struct eit_event {
	unsigned event_id_hi			: 8;
	unsigned event_id_lo			: 8;
	unsigned start_time_hi			: 8;
	unsigned start_time_hi2			: 8;
	unsigned start_time_mid			: 8;
	unsigned start_time_lo2			: 8;
	unsigned start_time_lo			: 8;
	unsigned duration_hi			: 8;
	unsigned duration_mid			: 8;
	unsigned duration_lo			: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned running_status			: 3;
	unsigned free_CA_mode			: 1;
	unsigned descriptors_loop_length_hi	: 4;
#else
	unsigned descriptors_loop_length_hi	: 4;
	unsigned free_CA_mode			: 1;
	unsigned running_status			: 3;
#endif
	unsigned descriptors_loop_length_lo	: 8;
} __attribute__ ((packed)) ;


struct descr_component_header {
	unsigned descriptor_tag			: 8;
	unsigned descriptor_length		: 8;
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned reserved_future_use		: 4;
	unsigned stream_content			: 4;
#else
	unsigned stream_content			: 4;
	unsigned reserved_future_use		: 4;
#endif
	unsigned component_type			: 8;
	unsigned component_tag			: 8;
	unsigned iso_639_2_language_code_hi	: 8;
	unsigned iso_639_2_language_code_mid	: 8;
	unsigned iso_639_2_language_code_lo	: 8;
} __attribute__ ((packed)) ;

struct descr_linkage_header {
	unsigned descriptor_tag			: 8;
	unsigned descriptor_length		: 8;
	unsigned transport_stream_id_hi		: 8;
	unsigned transport_stream_id_lo		: 8;
	unsigned original_network_id_hi		: 8;
	unsigned original_network_id_lo		: 8;
	unsigned service_id_hi			: 8;
	unsigned service_id_lo			: 8;
	unsigned linkage_type			: 8;
} __attribute__ ((packed)) ;

struct descr_pdc_header {
	unsigned descriptor_tag			: 8;
	unsigned descriptor_length		: 8;
	unsigned pil0				: 8;
	unsigned pil1				: 8;
	unsigned pil2				: 8;
} __attribute__ ((packed)) ;

class SIlinkage {
public:
	SIlinkage(const struct descr_linkage_header *link) {
		linkageType = link->linkage_type;
		transportStreamId = (link->transport_stream_id_hi << 8) | link->transport_stream_id_lo;
		originalNetworkId = (link->original_network_id_hi << 8) | link->original_network_id_lo;
		serviceId = (link->service_id_hi << 8) | link->service_id_lo;
		if (link->descriptor_length > sizeof(struct descr_linkage_header) - 2)
			//name = std::string(((const char *)link) + sizeof(struct descr_linkage_header), link->descriptor_length - (sizeof(struct descr_linkage_header) - 2));
			name = convertDVBUTF8(((const char *)link)+sizeof(struct descr_linkage_header), link->descriptor_length-(sizeof(struct descr_linkage_header)-2), 0, 0);
	}

	// Std-copy
	SIlinkage(const SIlinkage &l) {
		linkageType = l.linkageType;
		transportStreamId = l.transportStreamId;
		originalNetworkId = l.originalNetworkId;
		serviceId = l.serviceId;
		name = l.name;
	}

	// default
	SIlinkage(void) {
		linkageType = 0;
		transportStreamId = 0;
		originalNetworkId = 0;
		serviceId = 0;
//		name = ;
	}
	
	// Der Operator zum sortieren
	bool operator < (const SIlinkage& l) const {
		return name < l.name;
	}

	void dump(void) const {
		printf("Linakge Type: 0x%02hhx\n", linkageType);
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

	unsigned char linkageType; // Linkage Descriptor
	std::string name; // Text aus dem Linkage Descriptor
	t_transport_stream_id transportStreamId; // Linkage Descriptor
	t_original_network_id originalNetworkId; // Linkage Descriptor
	t_service_id          serviceId;         // Linkage Descriptor
};

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

//typedef std::multiset <SIlinkage, std::less<SIlinkage> > SIlinkage_descs;
typedef std::vector<class SIlinkage> SIlinkage_descs;

class SIcomponent {
  public:
    SIcomponent(const struct descr_component_header *comp) {
      streamContent=comp->stream_content;
      componentType=comp->component_type;
      componentTag=comp->component_tag;
      if(comp->descriptor_length>sizeof(struct descr_component_header)-2)
        //component=std::string(((const char *)comp)+sizeof(struct descr_component_header), comp->descriptor_length-(sizeof(struct descr_component_header)-2));
        component=convertDVBUTF8(((const char *)comp)+sizeof(struct descr_component_header), comp->descriptor_length-(sizeof(struct descr_component_header)-2), 0, 0);
    }
    // Std-copy
    SIcomponent(const SIcomponent &c) {
      streamContent=c.streamContent;
      componentType=c.componentType;
      componentTag=c.componentTag;
      component=c.component;
    }
    
    SIcomponent(void) {
      streamContent=0;
      componentType=0;
      componentTag=0;      
    }
    // Der Operator zum sortieren
    bool operator < (const SIcomponent& c) const {
      return streamContent < c.streamContent;
//      return component < c.component;
    }
    void dump(void) const {
      if(component.length())
        printf("Component: %s\n", component.c_str());
      printf("Stream Content: 0x%02hhx\n", streamContent);
      printf("Component type: 0x%02hhx\n", componentType);
      printf("Component tag: 0x%02hhx\n", componentTag);
    }
    int saveXML(FILE *file) const {
      fprintf(file, "\t\t\t<component tag=\"%02x\" type=\"%02x\" stream_content=\"%02x\" text=\"", componentTag, componentType, streamContent);
      saveStringToXMLfile(file,component.c_str());
      fprintf(file, "\"/>\n");
//      %s
//	return 1;
//	saveStringToXMLfile(file, component.c_str());
//	fprintf(file, "\"/>\n");
      return 0;
    }
    std::string component; // Text aus dem Component Descriptor
    unsigned char componentType; // Component Descriptor
    unsigned char componentTag; // Component Descriptor
    unsigned char streamContent; // Component Descriptor
};

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

typedef std::multiset <SIcomponent, std::less<SIcomponent> > SIcomponents;

class SIparentalRating {
  public:
    SIparentalRating(const std::string &cc, unsigned char rate) {
      rating=rate;
      countryCode=cc;
    }
    // Std-Copy
    SIparentalRating(const SIparentalRating &r) {
      rating=r.rating;
      countryCode=r.countryCode;
    }
    // Der Operator zum sortieren
    bool operator < (const SIparentalRating& c) const {
      return countryCode < c.countryCode;
    }
    void dump(void) const {
      printf("Rating: %s %hhu (+3)\n", countryCode.c_str(), rating);
    }
    int saveXML(FILE *file) const {
      if(fprintf(file, "\t\t\t<parental_rating country=\"%s\" rating=\"%hhu\"/>\n", countryCode.c_str(), rating)<0)
        return 1;
      return 0;
    }
    std::string countryCode;
    unsigned char rating; // Bei 1-16 -> Minumim Alter = rating +3
};

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

typedef std::set <SIparentalRating, std::less<SIparentalRating> > SIparentalRatings;

class SItime {
  public:
    SItime(time_t s, unsigned d) {
      startzeit=s;
      dauer=d; // in Sekunden, 0 -> time shifted (cinedoms)
    }
    // Std-Copy
    SItime(const SItime &t) {
      startzeit=t.startzeit;
      dauer=t.dauer;
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
      // Ist so noch nicht in Ordnung, das sollte untergliedert werden,
      // da sonst evtl. time,date,duration,time,date,... auftritt
      // und eine rein sequentielle Ordnung finde ich nicht ok.
      /*
      struct tm *zeit=localtime(&startzeit);
      fprintf(file, "\t\t\t\t\t<time>%02d:%02d:%02d</time>\n", zeit->tm_hour, zeit->tm_min, zeit->tm_sec);
      fprintf(file, "\t\t\t\t\t<date>%02d.%02d.%04d</date>\n", zeit->tm_mday, zeit->tm_mon+1, zeit->tm_year+1900);
      fprintf(file, "\t\t\t\t\t<duration>%u</duration>\n", dauer);
      */
      fprintf(file, "\t\t\t<time start_time=\"%u\" duration=\"%u\"/>\n", (unsigned int) startzeit, dauer);
      return 0;
    }
    time_t startzeit;  // lokale Zeit, 0 -> time shifted (cinedoms)
    unsigned dauer; // in Sekunden, 0 -> time shifted (cinedoms)
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

class SIevent {
public:
	t_service_id          service_id;
	t_original_network_id original_network_id;
	t_transport_stream_id transport_stream_id;
	
	SIevent(const struct eit_event *);
	// Std-Copy
	SIevent(const SIevent &);
	SIevent(const t_original_network_id, const t_transport_stream_id, const t_service_id, const unsigned short);
	SIevent(void) {
		service_id = 0;
		original_network_id = 0;
		transport_stream_id = 0;
		eventID    = 0;
		vps = 0;
		table_id = 0xFF; /* 0xFF means "not set" */
		version = 0xFF;
		running = 0;
//      dauer=0;
//      startzeit=0;
	}
	unsigned short eventID;
	// Name aus dem Short-Event-Descriptor
	std::string getName() const;
	void setName(const std::string &lang, const std::string &name);

	// Text aus dem Short-Event-Descriptor
	std::string getText() const;
	void setText(const std::string &lang, const std::string &text);

	std::string itemDescription; // Aus dem Extended Descriptor
	std::string item; // Aus dem Extended Descriptor

	// Aus dem Extended Descriptor
	std::string getExtendedText() const;
	void appendExtendedText(const std::string &lang, const std::string &text);
	void setExtendedText(const std::string &lang, const std::string &text);

	std::string contentClassification; // Aus dem Content Descriptor, als String, da mehrere vorkommen koennen
	std::string userClassification; // Aus dem Content Descriptor, als String, da mehrere vorkommen koennen
	//    time_t startzeit; // lokale Zeit, 0 -> time shifted (cinedoms)
	//    unsigned dauer; // in Sekunden, 0 -> time shifted (cinedoms)
	
	t_channel_id get_channel_id(void) const {
		return CREATE_CHANNEL_ID;
	}

	event_id_t uniqueKey(void) const {
		return CREATE_EVENT_ID(CREATE_CHANNEL_ID, eventID);
	}
	int runningStatus(void) const {
		return running;
	}
    SIcomponents components;
    SIparentalRatings ratings;
    SIlinkage_descs linkage_descs;
    SItimes times;
    time_t vps;
    unsigned char table_id;
    unsigned char version;
    // Der Operator zum sortieren
    bool operator < (const SIevent& e) const {
      return uniqueKey()<e.uniqueKey();
    }
    int saveXML(FILE *file) const { // saves the event
      return saveXML0(file) || saveXML2(file);
    }
    int saveXML(FILE *file, const char *serviceName) const; // saves the event
    void dump(void) const; // dumps the event to stdout
    void dumpSmall(void) const; // dumps the event to stdout (not all information)
#ifndef DO_NOT_INCLUDE_STUFF_NOT_NEEDED_FOR_SECTIONSD
    // Liefert das aktuelle EPG des senders mit der uebergebenen serviceID,
    // bei Fehler ist die serviceID des zurueckgelieferten Events 0
    static SIevent readActualEvent(t_service_id serviceID, unsigned timeoutInSeconds=2);
#endif
    char getFSK() const;
 protected:
    int saveXML0(FILE *f) const;
    int saveXML2(FILE *f) const;
 private:
    std::map<std::string, std::string> langName;
    std::map<std::string, std::string> langText;
    std::map<std::string, std::string> langExtendedText;
	int running;
};

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

#endif // SIEVENTS_HPP
