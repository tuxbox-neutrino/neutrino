/*
 * $Id: bouquets.cpp,v 1.101 2004/08/02 08:09:44 thegoodguy Exp $
 *
 * BouquetManager for zapit - d-box2 linux project
 *
 * (C) 2002 by Simplex    <simplex@berlios.de>,
 *             rasc       <rasc@berlios.de>,
 *             thegoodguy <thegoodguy@berlios.de>
 *
 * (C) 2009, 2011-2013 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <map>
#include <set>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#include <global.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <system/helpers.h>
#include <system/set_threadname.h>

#include <zapit/bouquets.h>
#include <zapit/debug.h>
#include <zapit/getservices.h>
#include <zapit/settings.h>
#include <zapit/zapit.h>
#include <xmlinterface.h>

#define M3U_START_MARKER        "#EXTM3U"
#define M3U_INFO_MARKER         "#EXTINF"
#define TVG_INFO_ID_MARKER      "tvg-id="
#define TVG_INFO_NAME_MARKER    "tvg-name="
#define TVG_INFO_LOGO_MARKER    "tvg-logo="
#define TVG_INFO_SHIFT_MARKER   "tvg-shift="
#define GROUP_NAME_MARKER       "group-title="

extern CBouquetManager *g_bouquetManager;
extern CPictureViewer *g_PicViewer;
pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;

#define GET_ATTR(node, name, fmt, arg)                                  \
        do {                                                            \
                const char * ptr = xmlGetAttribute(node, name);               \
                if ((ptr == NULL) || (sscanf(ptr, fmt, &arg) <= 0))     \
                        arg = 0;                                        \
        }                                                               \
        while (0)

/**** class CBouquet ********************************************************/
// -- servicetype 0 queries TV and Radio Channels
CZapitChannel* CZapitBouquet::getChannelByChannelID(const t_channel_id channel_id, const unsigned char serviceType)
{
	CZapitChannel* result = NULL;

	ZapitChannelList* channels = &tvChannels;

	switch (serviceType) {
		case ST_RESERVED: // ?
		case ST_DIGITAL_TELEVISION_SERVICE:
		case ST_NVOD_REFERENCE_SERVICE:
		case ST_NVOD_TIME_SHIFTED_SERVICE:
			channels = &tvChannels;
			break;

		case ST_DIGITAL_RADIO_SOUND_SERVICE:
			channels = &radioChannels;
			break;
	}

	unsigned int i;
	for (i = 0; (i < channels->size()) && ((*channels)[i]->getChannelID() != channel_id); i++)
	{};

	if (i<channels->size())
		result = (*channels)[i];

	if ((serviceType == ST_RESERVED) && (result == NULL)) {
		result = getChannelByChannelID(channel_id, ST_DIGITAL_RADIO_SOUND_SERVICE);
	}

	return result;
}

void CZapitBouquet::sortBouquet(void)
{
	sort(tvChannels.begin(), tvChannels.end(), CmpChannelByChName());
	sort(radioChannels.begin(), radioChannels.end(), CmpChannelByChName());
}

void CZapitBouquet::sortBouquetByNumber(void)
{
	sort(tvChannels.begin(), tvChannels.end(), CmpChannelByChNum());
	sort(radioChannels.begin(), radioChannels.end(), CmpChannelByChNum());
}

void CZapitBouquet::addService(CZapitChannel* newChannel)
{
	switch (newChannel->getServiceType())
	{
		case ST_DIGITAL_TELEVISION_SERVICE:
		case ST_NVOD_REFERENCE_SERVICE:
		case ST_NVOD_TIME_SHIFTED_SERVICE:
			tvChannels.push_back(newChannel);
			break;

		case ST_DIGITAL_RADIO_SOUND_SERVICE:
			radioChannels.push_back(newChannel);
			break;
	}
	if (bLocked)
		newChannel->bLockCount++;
}

void CZapitBouquet::removeService(CZapitChannel* oldChannel)
{
	if (oldChannel != NULL) {
		ZapitChannelList* channels = &tvChannels;
		switch (oldChannel->getServiceType()) {
			case ST_DIGITAL_TELEVISION_SERVICE:
			case ST_NVOD_REFERENCE_SERVICE:
			case ST_NVOD_TIME_SHIFTED_SERVICE:
				channels = &tvChannels;
				break;

			case ST_DIGITAL_RADIO_SOUND_SERVICE:
				channels = &radioChannels;
				break;
		}

		if (bLocked)
			oldChannel->bLockCount--;
		(*channels).erase(remove(channels->begin(), channels->end(), oldChannel), channels->end());
	}
}

void CZapitBouquet::moveService(const unsigned int oldPosition, const unsigned int newPosition, const unsigned char serviceType)
{
	ZapitChannelList* channels = &tvChannels;
	switch (serviceType) {
		case ST_DIGITAL_TELEVISION_SERVICE:
		case ST_NVOD_REFERENCE_SERVICE:
		case ST_NVOD_TIME_SHIFTED_SERVICE:
			channels = &tvChannels;
			break;

		case ST_DIGITAL_RADIO_SOUND_SERVICE:
			channels = &radioChannels;
			break;
	}
	if ((oldPosition < channels->size()) && (newPosition < channels->size())) {
		ZapitChannelList::iterator it = channels->begin();

		advance(it, oldPosition);
		CZapitChannel* tmp = *it;
		channels->erase(it);

		it = channels->begin();
		advance(it, newPosition);
		channels->insert(it, tmp);
	}
}

bool CZapitBouquet::getTvChannels(ZapitChannelList &list, int flags)
{
	list.clear();
	for (ZapitChannelList::iterator it = tvChannels.begin(); it != tvChannels.end(); ++it) {
		if ((*it)->flags & flags)
			list.push_back(*it);
	}
	return (!list.empty());
}

bool CZapitBouquet::getRadioChannels(ZapitChannelList &list, int flags)
{
	list.clear();
	for (ZapitChannelList::iterator it = radioChannels.begin(); it != radioChannels.end(); ++it) {
		if ((*it)->flags & flags)
			list.push_back(*it);
	}
	return (!list.empty());
}

bool CZapitBouquet::getChannels(ZapitChannelList &list, bool tv, int flags)
{
	list.clear();
	ZapitChannelList *current = tv ? &tvChannels : &radioChannels;
	for (ZapitChannelList::iterator it = current->begin(); it != current->end(); ++it) {
		if ((*it)->flags & flags)
			list.push_back(*it);
	}
	return (!list.empty());
}
#if 0
size_t CZapitBouquet::recModeRadioSize(const transponder_id_t transponder_id)
{
	size_t size = 0;

	for (size_t i = 0; i < radioChannels.size(); i++)
		if (transponder_id == radioChannels[i]->getTransponderId())
			size++;

	return size;
}

size_t CZapitBouquet::recModeTVSize(const transponder_id_t transponder_id)
{
	size_t size = 0;

	for (size_t i = 0; i < tvChannels.size(); i++)
		if (transponder_id == tvChannels[i]->getTransponderId())
			size++;

	return size;
}
#endif

CBouquetManager::~CBouquetManager()
{
	clearAll();
}

void CBouquetManager::writeBouquetHeader(FILE * bouq_fd, uint32_t i, const char * bouquetName)
{
//printf("[bouquets] writing bouquet header: %s\n", bouquetName);
	fprintf(bouq_fd, "\t<Bouquet name=\"%s\"", bouquetName);
	if (Bouquets[i]->BqID!=DEFAULT_BQ_ID) fprintf(bouq_fd, " bqID=\"%04x\"", Bouquets[i]->BqID); // optional_attribute > default=0
	if (Bouquets[i]->bHidden!=DEFAULT_BQ_HIDDEN) fprintf(bouq_fd, " hidden=\"%d\"", Bouquets[i]->bHidden ? 1 : 0);
	if (Bouquets[i]->bLocked!=DEFAULT_BQ_LOCKED) fprintf(bouq_fd, " locked=\"%d\"", Bouquets[i]->bLocked ? 1 : 0);
	if (Bouquets[i]->bScanEpg!=DEFAULT_BQ_SCANEPG) fprintf(bouq_fd, " epg=\"%d\"", Bouquets[i]->bScanEpg ? 1 : 0);
	if (Bouquets[i]->bUseCI) fprintf(bouq_fd, " ci=\"1\"");
	fprintf(bouq_fd, ">\n");
}

void CBouquetManager::writeBouquetFooter(FILE * bouq_fd)
{
	fprintf(bouq_fd, "\t</Bouquet>\n");
}

void CBouquetManager::writeChannels(FILE * bouq_fd, ZapitChannelList &list, bool bUser)
{
	for(zapit_list_it_t it = list.begin(); it != list.end(); it++)
		(*it)->dumpBouquetXml(bouq_fd, bUser);
}

void CBouquetManager::writeBouquetChannels(FILE * bouq_fd, uint32_t i, bool bUser)
{
	writeChannels(bouq_fd, Bouquets[i]->tvChannels, bUser);
	writeChannels(bouq_fd, Bouquets[i]->radioChannels, bUser);
}

void CBouquetManager::writeBouquet(FILE * bouq_fd, uint32_t i, bool /* bUser */)
{
	writeBouquetHeader(bouq_fd, i, convert_UTF8_To_UTF8_XML(Bouquets[i]->Name.c_str()).c_str());
	writeBouquetChannels(bouq_fd, i, Bouquets[i]->bUser);
	writeBouquetFooter(bouq_fd);
}

/**** class CBouquetManager *************************************************/
void CBouquetManager::saveBouquets(void)
{
	FILE * bouq_fd;

	printf("CBouquetManager::saveBouquets: %s\n", BOUQUETS_XML);
	bouq_fd = fopen(BOUQUETS_XML, "w");
	if (!bouq_fd) {
		perror(BOUQUETS_XML);
		return;
	}
	fprintf(bouq_fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit api=\"4\">\n");
	for (unsigned int i = 0; i < Bouquets.size(); i++) {
		if (Bouquets[i] != remainChannels) {
			DBG("save Bouquets: name %s user: %d\n", Bouquets[i]->Name.c_str(), Bouquets[i]->bUser);
			if(!Bouquets[i]->bUser && !Bouquets[i]->bWebtv && !Bouquets[i]->bWebradio) {
				writeBouquet(bouq_fd, i,false);
			}
		}
	}
	fprintf(bouq_fd, "</zapit>\n");
	fdatasync(fileno(bouq_fd));
	fclose(bouq_fd);
	chmod(BOUQUETS_XML, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

void CBouquetManager::saveUBouquets(void)
{
	FILE * ubouq_fd;

	printf("CBouquetManager::saveUBouquets: %s\n", UBOUQUETS_XML);
	ubouq_fd = fopen(UBOUQUETS_XML, "w");
	if (!ubouq_fd) {
		perror(BOUQUETS_XML);
		return;
	}
	fprintf(ubouq_fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit api=\"4\">\n");
	for (unsigned int i = 0; i < Bouquets.size(); i++) {
		if (Bouquets[i] != remainChannels) {
			if(Bouquets[i]->bUser) {
				writeBouquet(ubouq_fd, i, true);
			}
		}
	}
	fprintf(ubouq_fd, "</zapit>\n");
	fdatasync(fileno(ubouq_fd));
	fclose(ubouq_fd);
	chmod(UBOUQUETS_XML, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

void CBouquetManager::saveBouquets(const CZapitClient::bouquetMode bouquetMode, const char * const providerName, t_satellite_position satellitePosition)
{
	if (bouquetMode == CZapitClient::BM_DELETEBOUQUETS) {
		INFO("removing existing bouquets");
		if (satellitePosition != INVALID_SAT_POSITION) {
			if (CFEManager::getInstance()->cableOnly())
				g_bouquetManager->clearAll(false);
			else
				g_bouquetManager->deletePosition(satellitePosition);
		}
	}
	if (bouquetMode == CZapitClient::BM_DONTTOUCHBOUQUETS)
		return;

	if (bouquetMode == CZapitClient::BM_CREATESATELLITEBOUQUET) {
		while (Bouquets.size() > 1) {
			BouquetList::iterator it = Bouquets.begin() + 1;
			Bouquets[0]->tvChannels.insert(Bouquets[0]->tvChannels.end(), (*it)->tvChannels.begin(), (*it)->tvChannels.end());
			Bouquets[0]->radioChannels.insert(Bouquets[0]->radioChannels.end(), (*it)->radioChannels.begin(), (*it)->radioChannels.end());
			delete (*it);
			Bouquets.erase(it);
		}
		if( !Bouquets.empty() )
			Bouquets[0]->Name = providerName;
	}

	if ((bouquetMode == CZapitClient::BM_UPDATEBOUQUETS) || (bouquetMode == CZapitClient::BM_DELETEBOUQUETS)) {
		while (!(Bouquets.empty())) {
			CZapitBouquet* bouquet;
			int dest = g_bouquetManager->existsBouquet(Bouquets[0]->Name.c_str(), true);
			DBG("save Bouquets: dest %d for name %s\n", dest, Bouquets[0]->Name.c_str());
			if(dest == -1) {
				bouquet = g_bouquetManager->addBouquet(Bouquets[0]->Name.c_str(), false);
				dest = g_bouquetManager->existsBouquet(Bouquets[0]->Name.c_str(), true);
			}
			else
				bouquet = g_bouquetManager->Bouquets[dest];

			/* list from scanned exist in file */
			for(unsigned int i = 0; i < Bouquets[0]->tvChannels.size(); i++) {
				if(!(g_bouquetManager->existsChannelInBouquet(dest, Bouquets[0]->tvChannels[i]->getChannelID()))) {
					bouquet->addService(Bouquets[0]->tvChannels[i]);
					//Bouquets[0]->tvChannels[i]->pname = (char *) bouquet->Name.c_str();
					DBG("save Bouquets: adding channel %s\n", Bouquets[0]->tvChannels[i]->getName().c_str());
				}
			}
			for(unsigned int i = 0; i < Bouquets[0]->radioChannels.size(); i++) {
				if(!(g_bouquetManager->existsChannelInBouquet(dest, Bouquets[0]->radioChannels[i]->getChannelID()))) {
					bouquet->addService(Bouquets[0]->radioChannels[i]);
					//Bouquets[0]->tvChannels[i]->pname = (char *) bouquet->Name.c_str();
					DBG("save Bouquets: adding channel %s\n", Bouquets[0]->radioChannels[i]->getName().c_str());
				}
			}

			bouquet->sortBouquet();
			delete Bouquets[0];
			Bouquets.erase(Bouquets.begin());
		}
	}
}

void CBouquetManager::sortBouquets(void)
{
	sort(Bouquets.begin(), Bouquets.end(), CmpBouquetByChName());
}

void CBouquetManager::parseBouquetsXml(const char *fname, bool bUser)
{
	xmlDocPtr parser;

	parser = parseXmlFile(fname);
	if (parser == NULL)
		return;

	xmlNodePtr root = xmlDocGetRootElement(parser);
	xmlNodePtr search = xmlChildrenNode(root);
	xmlNodePtr channel_node;

	if (search) {
		if(!bUser){
			readEPGMapping();
		}

		t_original_network_id original_network_id;
		t_service_id service_id;
		t_transport_stream_id transport_stream_id;
		int16_t satellitePosition;
		freq_id_t freq = 0;

		INFO("reading bouquets from %s", fname);

		while ((search = xmlGetNextOccurence(search, "Bouquet")) != NULL) {
			const char * name = xmlGetAttribute(search, "name");
			if(name == NULL)
				name = const_cast<char*>("Unknown");

			CZapitBouquet* newBouquet = addBouquet(name, bUser);
			// per default in contructor: newBouquet->BqID = 0; //set to default, override if bqID exists
			GET_ATTR(search, "bqID", SCANF_BOUQUET_ID_TYPE, newBouquet->BqID);
			const char* hidden = xmlGetAttribute(search, "hidden");
			const char* locked = xmlGetAttribute(search, "locked");
			const char* scanepg = xmlGetAttribute(search, "epg");
			const char* useci = xmlGetAttribute(search, "ci");
			newBouquet->bHidden = hidden ? (strcmp(hidden, "1") == 0) : false;
			newBouquet->bLocked = locked ? (strcmp(locked, "1") == 0) : false;
			newBouquet->bFav = (strcmp(name, DEFAULT_BQ_NAME_FAV) == 0);
			if (newBouquet->bFav)
				newBouquet->bName = g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME);
			else
				newBouquet->bName = name;
			newBouquet->bScanEpg = scanepg ? (strcmp(scanepg, "1") == 0) : false;
			newBouquet->bUseCI = useci ? (strcmp(useci, "1") == 0) : false;
			channel_node = xmlChildrenNode(search);
			while ((channel_node = xmlGetNextOccurence(channel_node, "S")) != NULL) {
				std::string name2;
				name = xmlGetAttribute(channel_node, "n");
				if (name)
					name2 = name;
				std::string uname;
				const char *uName = xmlGetAttribute(channel_node, "un");
				if (uName)
					uname = uName;
				const char *url = xmlGetAttribute(channel_node, "u");
				GET_ATTR(channel_node, "i", SCANF_SERVICE_ID_TYPE, service_id);
				GET_ATTR(channel_node, "on", SCANF_ORIGINAL_NETWORK_ID_TYPE, original_network_id);
				GET_ATTR(channel_node, "s", SCANF_SATELLITE_POSITION_TYPE, satellitePosition);
				GET_ATTR(channel_node, "t", SCANF_TRANSPORT_STREAM_ID_TYPE, transport_stream_id);
				GET_ATTR(channel_node, "frq", SCANF_SATELLITE_POSITION_TYPE, freq);
				bool clock = xmlGetNumericAttribute(channel_node, "l", 10);
				if(freq > 20000)
					freq = freq/1000;

				CZapitChannel* chan;
				t_channel_id chid = create_channel_id64(service_id, original_network_id, transport_stream_id,
									satellitePosition, freq, url);
				/* FIXME to load old cable settings with new cable "positions" started from 0xF00 */
				if(!url && (bUser || CFEManager::getInstance()->cableOnly()))
					chan = CServiceManager::getInstance()->FindChannelFuzzy(chid, satellitePosition, freq);
				else
					chan = CServiceManager::getInstance()->FindChannel(chid);
				if (chan != NULL) {
					DBG("%04x %04x %04x %s\n", transport_stream_id, original_network_id, service_id, xmlGetAttribute(channel_node, "n"));
					if(bUser && !(uname.empty()))
						chan->setUserName(uname);
					if(!bUser)
						chan->pname = (char *) newBouquet->Name.c_str();
					chan->bLocked = clock;
					chan->bUseCI = newBouquet->bUseCI;
					//remapinng epg_id
					t_channel_id new_epgid = reMapEpgID(chan->getChannelID());
					if(new_epgid)
						chan->setEPGid(new_epgid);
					std::string new_epgxml = reMapEpgXML(chan->getChannelID());
					if(!new_epgxml.empty()) {
						char buf[100];
						snprintf(buf, sizeof(buf), "%llx", chan->getChannelID() & 0xFFFFFFFFFFFFULL);
						chan->setScriptName("#" + new_epgxml + "=" + buf);
					}
					newBouquet->addService(chan);
				} else if (bUser) {
					if (url) {
						chid = create_channel_id64(0, 0, 0, 0, 0, url);
						chan = new CZapitChannel(name2.c_str(), chid, url, NULL);
					}
					else
						chan = new CZapitChannel(name2, CREATE_CHANNEL_ID64, 1 /*service_type*/,
								satellitePosition, freq);

					CServiceManager::getInstance()->AddChannel(chan);
					chan->flags = CZapitChannel::NOT_FOUND;
					chan->bLocked = clock;
					if(!(uname.empty()))
						chan->setUserName(uname);
					newBouquet->addService(chan);
					CServiceManager::getInstance()->SetServicesChanged(false);
				}

				channel_node = xmlNextNode(channel_node);
				if(!bUser) {
					/* set satellite position for provider bouquets.
					   reset position to 0, if position not match - means mixed bouquet */
					if (newBouquet->satellitePosition == INVALID_SAT_POSITION)
						newBouquet->satellitePosition = satellitePosition;
					else if (newBouquet->satellitePosition != satellitePosition)
						newBouquet->satellitePosition = 0;
				}
			}
			if(!bUser)
				newBouquet->sortBouquet();
			search = xmlNextNode(search);
		}
		INFO("total: %d bouquets", (int)Bouquets.size());
		if(!bUser && !EpgIDMapping.empty()){
			EpgIDMapping.clear();
		}
		if(!bUser && !EpgXMLMapping.empty()){
			EpgXMLMapping.clear();
		}
	}
	xmlFreeDoc(parser);
}

void CBouquetManager::loadBouquets(bool ignoreBouquetFile)
{
	TIMER_START();
	clearAll();
	if (ignoreBouquetFile == false) {
		parseBouquetsXml(BOUQUETS_XML, false);
		sortBouquets();
	}

	loadWebtv();
	loadWebradio();
	loadLogos();
	parseBouquetsXml(UBOUQUETS_XML, true);
	renumServices();
	CServiceManager::getInstance()->SetCIFilter();
	TIMER_STOP("[zapit] bouquet loading took");
}

void CBouquetManager::renumChannels(ZapitChannelList &list, int & counter, char * pname)
{
	for(zapit_list_it_t it = list.begin(); it != list.end(); it++) {
		if(!(*it)->number)
			(*it)->number = counter++;

		if(!(*it)->pname && pname)
			(*it)->pname = pname;

		if ((*it)->pname)
			(*it)->has_bouquet = true;
	}
}

void CBouquetManager::makeRemainingChannelsBouquet(void)
{
	ZapitChannelList unusedChannels;
	//FIXME services loaded before config.
	//bool tomake = CZapit::getInstance()->makeRemainingChannelsBouquet();

	/* reset channel number and has_bouquet flag */
	CServiceManager::getInstance()->ResetChannelNumbers();

	//int i = 1, j = 1;
	int i = CServiceManager::getInstance()->GetMaxNumber(false);
	int j = CServiceManager::getInstance()->GetMaxNumber(true);
	/* FIXME temp debug */
	printf("############## CBouquetManager::makeRemainingChannelsBouquet: numbers start at: tv %d radio %d ############\n", i, j);
	for (std::vector<CZapitBouquet*>::const_iterator it = Bouquets.begin(); it != Bouquets.end(); ++it) {
		renumChannels((*it)->tvChannels, i, (*it)->bUser ? NULL : (char *) (*it)->Name.c_str());
		renumChannels((*it)->radioChannels, j, (*it)->bUser ? NULL : (char *) (*it)->Name.c_str());
	}

	if(/*!tomake ||*/ CServiceManager::getInstance()->GetAllUnusedChannels(unusedChannels) == false)
		return;

	sort(unusedChannels.begin(), unusedChannels.end(), CmpChannelByChName());

	// TODO: use locales
	if (remainChannels == NULL)
		remainChannels = addBouquet(Bouquets.empty() ? DEFAULT_BQ_NAME_ALL : DEFAULT_BQ_NAME_OTHER, false); // UTF-8 encoded
	remainChannels->bOther = true;
	remainChannels->bName = g_Locale->getText(LOCALE_BOUQUETNAME_OTHER);

	for (ZapitChannelList::const_iterator it = unusedChannels.begin(); it != unusedChannels.end(); ++it) {
		remainChannels->addService(*it);
	}

	renumChannels(remainChannels->tvChannels, i);
	renumChannels(remainChannels->radioChannels, j);
}

void CBouquetManager::renumServices()
{
#if 0
	if(remainChannels)
		deleteBouquet(remainChannels);

	remainChannels = NULL;
#endif
	if(remainChannels) {
		remainChannels->tvChannels.clear();
		remainChannels->radioChannels.clear();
	}

	makeRemainingChannelsBouquet();
}

CZapitBouquet* CBouquetManager::addBouquet(const std::string & name, bool ub, bool myfav, bool to_begin)
{
	CZapitBouquet* newBouquet = new CZapitBouquet(myfav ? DEFAULT_BQ_NAME_FAV : name);
	newBouquet->bUser = ub;
	newBouquet->bFav = myfav;
	if (newBouquet->bFav)
		newBouquet->bName = g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME);
	else
		newBouquet->bName = name;
	newBouquet->satellitePosition = INVALID_SAT_POSITION;

//printf("CBouquetManager::addBouquet: %s, user %s\n", name.c_str(), ub ? "YES" : "NO");
	if(ub) {
		BouquetList::iterator it;
		if (to_begin) {
			it = Bouquets.begin();
		} else {
			for(it = Bouquets.begin(); it != Bouquets.end(); ++it)
				if(!(*it)->bUser)
					break;
		}
		Bouquets.insert(it, newBouquet);
	} else
		Bouquets.push_back(newBouquet);
	return newBouquet;
}

void CBouquetManager::deleteBouquet(const unsigned int id)
{
	if (id < Bouquets.size() && Bouquets[id] != remainChannels)
		deleteBouquet(Bouquets[id]);
}

void CBouquetManager::deleteBouquet(const CZapitBouquet* bouquet)
{
	if (bouquet != NULL) {
		BouquetList::iterator it = find(Bouquets.begin(), Bouquets.end(), bouquet);

		if (it != Bouquets.end()) {
			if ((*it)->bLocked) {
				ZapitChannelList *channels = &(*it)->tvChannels;
				for(unsigned int i = 0; i < channels->size(); i++)
					((*channels)[i])->bLockCount--;

				channels = &(*it)->radioChannels;
				for(unsigned int i = 0; i < channels->size(); i++)
					((*channels)[i])->bLockCount--;
			}

			Bouquets.erase(it);
			delete bouquet;
		}
	}
}

void CBouquetManager::setBouquetLock(const unsigned int id, bool state)
{
	if (id < Bouquets.size())
		setBouquetLock(Bouquets[id], state);
}

void CBouquetManager::setBouquetLock(CZapitBouquet* bouquet, bool state)
{
	bouquet->bLocked = state;
        int add = bouquet->bLocked * 2 - 1;

        ZapitChannelList *channels = &bouquet->tvChannels;
        for(unsigned int i = 0; i < channels->size(); i++)
                ((*channels)[i])->bLockCount += add;

        channels = &bouquet->radioChannels;
        for(unsigned int i = 0; i < channels->size(); i++)
                ((*channels)[i])->bLockCount += add;

}

#if 0
int str_compare_withoutspace(char const *s1, char const *s2)
{
	int cmp_result = 0;
	while(isspace(*s1)) ++s1;
	while(isspace(*s2)) ++s2;

	while(!cmp_result && *s1++ && *s2++){
		while(isspace(*s1)) ++s1;
		while(isspace(*s2)) ++s2;
		cmp_result = tolower(*s1) - tolower(*s2);
	}
	return cmp_result;
} 
#endif

// -- Find Bouquet-Name, if BQ exists   (2002-04-02 rasc)
// -- Return: Bouqet-ID (found: 0..n)  or -1 (Bouquet does not exist)
int CBouquetManager::existsBouquet(char const * const name, bool ignore_user)
{
	for (unsigned int i = 0; i < Bouquets.size(); i++) {
		if ((!ignore_user || !Bouquets[i]->bUser) && (Bouquets[i]->Name == name))
		{
			return (int)i;
		}
#if 0
		else if (strcasecmp(Bouquets[i]->Name.c_str(), name)==0)
		{
			int lower1 = 0, lower2 =  0;
			int upper1 = 0 ,upper2 = 0;
			std::string str2 = name;
			size_t  pos=0;
			size_t pos2 = Bouquets[i]->Name.find("] "); 
			if(pos != std::string::npos){
				pos += pos2;
			}

			while (Bouquets[i]->Name[pos]){
				if(islower(Bouquets[i]->Name[pos])){
					++lower1;
				}
				if(isupper(Bouquets[i]->Name[pos])){
					++upper1;
				}
				if(islower(str2[pos])){
					++lower2;
				}
				if(isupper(str2[pos])){
					++upper2;
				}
				pos++;  
			}
			if ( ( upper2 && (lower1 < lower2)) || (lower2==0 && upper1==0 && upper2==lower1) ){
				Bouquets[i]->Name = str2;
			}
			return (int)i;
		}else if(!(str_compare_withoutspace(Bouquets[i]->Name.c_str(), name)) )
		{
			if(strlen(name) > Bouquets[i]->Name.length()){
				Bouquets[i]->Name = name;
			}
			return (int)i;
		}
#endif
	}
	return -1;
}

int CBouquetManager::existsUBouquet(char const * const name, bool myfav)
{
	unsigned int i;

	for (i = 0; i < Bouquets.size(); i++) {
		if(myfav) {
			if (Bouquets[i]->bFav)
				return (int)i;
		}
		else if (Bouquets[i]->bUser && (Bouquets[i]->Name == name))
			return (int)i;
		else if (Bouquets[i]->bUser && (Bouquets[i]->bName == name))
			return (int)i;
	}
	return -1;
}
// -- Check if channel exists in BQ   (2002-04-05 rasc)
// -- Return: True/false
bool CBouquetManager::existsChannelInBouquet( unsigned int bq_id, const t_channel_id channel_id)
{
	bool     status = false;
	CZapitChannel  *ch = NULL;

	if (bq_id <= Bouquets.size()) {
		// query TV-Channels  && Radio channels
		ch = Bouquets[bq_id]->getChannelByChannelID(channel_id, 0);
		if (ch)  status = true;
	}
	return status;
}

void CBouquetManager::moveBouquet(const unsigned int oldId, const unsigned int newId)
{
	if ((oldId < Bouquets.size()) && (newId < Bouquets.size())) {
		BouquetList::iterator it = Bouquets.begin();

		advance(it, oldId);
		CZapitBouquet* tmp = *it;
		Bouquets.erase(it);

		it = Bouquets.begin();
		advance(it, newId);
		Bouquets.insert(it, tmp);
	}
}

void CBouquetManager::clearAll(bool user)
{
	BouquetList tmplist;
	for (unsigned int i =0; i < Bouquets.size(); i++) {
		if (user || !Bouquets[i]->bUser)
			delete Bouquets[i];
		else
			tmplist.push_back(Bouquets[i]);
	}

	Bouquets.clear();
	if (!user)
		Bouquets = tmplist;
	remainChannels = NULL;
	if(thrLogo != 0)
	{
		pthread_cancel(thrLogo);
		pthread_join(thrLogo, NULL);
		thrLogo = 0;
	}
	pthread_mutex_lock (&mutex);
	LogoList.clear();
	pthread_mutex_unlock (&mutex);
}

void CBouquetManager::deletePosition(t_satellite_position satellitePosition)
{
	BouquetList tmplist;
	for (unsigned int i =0; i < Bouquets.size(); i++) {
		if (satellitePosition == Bouquets[i]->satellitePosition) {
			printf("CBouquetManager::deletePosition: delete [%s]\n", Bouquets[i]->Name.c_str());
			delete Bouquets[i];
		} else
			tmplist.push_back(Bouquets[i]);
	}
	Bouquets = tmplist;
}

CZapitBouquet* CBouquetManager::addBouquetIfNotExist(const std::string &name)
{
	CZapitBouquet* bouquet = NULL;

	int bouquetId = existsBouquet(name.c_str(), true);
	if (bouquetId == -1)
		bouquet = addBouquet(name, false);
	else
		bouquet = Bouquets[bouquetId];

	return bouquet;
}

void CBouquetManager::loadWebtv()
{
	loadWebchannels(MODE_WEBTV);
}

void CBouquetManager::loadWebradio()
{
	loadWebchannels(MODE_WEBRADIO);
}

void CBouquetManager::loadWebchannels(int mode)
{
	std::list<std::string> *webchannels_xml = (mode == MODE_WEBTV) ? CZapit::getInstance()->GetWebTVXML() : CZapit::getInstance()->GetWebRadioXML();
	if (!webchannels_xml)
		return;

	for (std::list<std::string>::iterator it = webchannels_xml->begin(); it != webchannels_xml->end(); ++it)
	{
		std::string filename = (*it);
		std::string extension = getFileExt(filename);
		std::string tmp_name = randomFile(extension, LOGODIR_TMP);
		bool remove_tmp = false;

		if (filename.compare(0, 1, "/") == 0)
			tmp_name = filename;
		else
		{
			if (::downloadUrl(filename, tmp_name))
				remove_tmp = true;
		}

		if (!access(tmp_name.c_str(), R_OK))
		{
			INFO("Loading %s from %s ...", (mode == MODE_WEBTV) ? "webtv" : "webradio", filename.c_str());

			// check for extension
			bool xml = false;
			bool m3u = false;
			bool e2tv = false;

			if (strcasecmp("xml", extension.c_str()) == 0)
				xml = true;
			if (strcasecmp("m3u", extension.c_str()) == 0)
				m3u = true;
			if (strcasecmp("tv", extension.c_str()) == 0)
				e2tv = true;

			if (xml)
			{
				xmlDocPtr parser = parseXmlFile(tmp_name.c_str());
				if (parser == NULL)
					continue;

				xmlNodePtr l0 = xmlDocGetRootElement(parser);
				xmlNodePtr l1 = xmlChildrenNode(l0);
				if (l1)
				{
					CZapitBouquet* pbouquet = NULL;
					const char *prov = xmlGetAttribute(l0, "name");
					if (!prov)
						prov = (mode == MODE_WEBTV) ? "WebTV" : "WebRadio";
					pbouquet = addBouquetIfNotExist(prov);
					if (mode == MODE_WEBTV)
						pbouquet->bWebtv = true;
					else
						pbouquet->bWebradio = true;

					while ((xmlGetNextOccurence(l1, (mode == MODE_WEBTV) ? "webtv" : "webradio")))
					{
						const char *title = xmlGetAttribute(l1, "title");
						const char *url = xmlGetAttribute(l1, "url");
						const char *desc = xmlGetAttribute(l1, "description");
						const char *genre = xmlGetAttribute(l1, "genre");
						const char *epgid = xmlGetAttribute(l1, "epgid");
						const char *script = xmlGetAttribute(l1, "script");
						t_channel_id epg_id = 0;
						if (epgid)
						{
							if (strcmp(epgid, "auto") == 0 && title)
							{
								CZapitChannel * channel = CServiceManager::getInstance()->FindChannelByPattern(title);
								if (channel && !IS_WEBCHAN(channel->getChannelID()))
								{
									epg_id = channel->getChannelID();
									INFO("* auto epg_id found for %s: " PRINTF_CHANNEL_ID_TYPE, title, epg_id);
								}
							}
							else
							epg_id = strtoull(epgid, NULL, 16);
						}

						CZapitBouquet* gbouquet = pbouquet;
						if (genre)
						{
							std::string bname = prov ? std::string(std::string(prov) + " ") + genre : genre;
							gbouquet = addBouquetIfNotExist(bname);
							if (mode == MODE_WEBTV)
								gbouquet->bWebtv = true;
							else
								gbouquet->bWebradio = true;
						}
						if (title && url)
						{
							t_channel_id chid = create_channel_id64(0, 0, 0, 0, 0, url);
							CZapitChannel * channel = new CZapitChannel(title, chid, url, desc, epg_id, script, mode);
							CServiceManager::getInstance()->AddChannel(channel);
							channel->flags = CZapitChannel::UPDATED;
							if (gbouquet)
								gbouquet->addService(channel);
						}

						l1 = xmlNextNode(l1);
					}
				}
				xmlFreeDoc(parser);
			}
			else if (m3u)
			{
				std::ifstream infile;
				char cLine[1024];
				std::string desc = "";
				std::string title = "";
				std::string group = "";
				std::string epgid = "";
				std::string alogo = "";
				CZapitBouquet* pbouquet = NULL;

				infile.open(tmp_name.c_str(), std::ifstream::in);

				while (infile.good())
				{
					infile.getline(cLine, sizeof(cLine));
					// remove CR
					if(cLine[strlen(cLine) - 1] == '\r')
						cLine[strlen(cLine) - 1] = 0;
					std::string strLine = cLine;

					if (strLine.empty())
						continue;

					if (strLine.find(M3U_START_MARKER) != std::string::npos)
						continue;

					if (strLine.find(M3U_INFO_MARKER) != std::string::npos)
					{
						int iColon = (int)strLine.find_first_of(':');
						int iComma = (int)strLine.find_last_of(',');
						title = "";
						group = "";
						desc = "";
						alogo = "";

						if (iColon >= 0 && iComma >= 0 && iComma > iColon)
						{
							iComma++;
							iColon++;
							title = strLine.substr(iComma);
							std::string strInfoLine = strLine.substr(iColon, --iComma - iColon);
							desc = ReadMarkerValue(strInfoLine, TVG_INFO_NAME_MARKER);
							group = ReadMarkerValue(strInfoLine, GROUP_NAME_MARKER);
							epgid = ReadMarkerValue(strInfoLine, TVG_INFO_ID_MARKER);
							alogo = ReadMarkerValue(strInfoLine, TVG_INFO_LOGO_MARKER);
						}

						pbouquet = addBouquetIfNotExist((mode == MODE_WEBTV) ? "WebTV" : "WebRadio");
						if (mode == MODE_WEBTV)
							pbouquet->bWebtv = true;
						else
							pbouquet->bWebradio = true;

					}
					else if (strLine[0] != '#')
					{
						char *url = NULL;
						if ((url = strstr(cLine, "http://")) || (url = strstr(cLine, "https://")) || (url = strstr(cLine, "rtmp://")) || (url = strstr(cLine, "rtsp://")) || (url = strstr(cLine, "rtp://")) || (url = strstr(cLine, "mmsh://")) )
						{
							if (url != NULL)
							{
								if (desc.empty())
									desc = "m3u stream";

								CZapitBouquet* gbouquet = pbouquet;
								if (!group.empty())
								{
									std::string bname = (mode == MODE_WEBTV) ? "[WebTV] " : "[WebRadio] ";
									bname += group;
									gbouquet = addBouquetIfNotExist(bname);
									if (mode == MODE_WEBTV)
										gbouquet->bWebtv = true;
									else
										gbouquet->bWebradio = true;
								}

								t_channel_id chid = create_channel_id64(0, 0, 0, 0, 0, url);
								std::string epg_script = "";
								if (!epgid.empty()) {
									char buf[100];
									snprintf(buf, sizeof(buf), "%llx", chid & 0xFFFFFFFFFFFFULL);
									// keep the tvg-id for later epg injection
									epg_script = "#" + epgid + "=" + buf;
								}
								CZapitChannel * channel = new CZapitChannel(title.c_str(), chid, url, desc.c_str(), chid, epg_script.c_str(), mode);
								CServiceManager::getInstance()->AddChannel(channel);
								desc = "m3u_loading_logos";
								if (!alogo.empty() && !g_PicViewer->GetLogoName(chid,title,desc))
								{
									channel->setAlternateLogo(alogo);
									pthread_mutex_lock (&mutex);
									LogoList.push_back(channel);
									pthread_mutex_unlock (&mutex);
								}
								channel->flags = CZapitChannel::UPDATED;
								if (gbouquet)
									gbouquet->addService(channel);
							}
						}
					}
				}
				infile.close();
			}
			else if (e2tv)
			{
				FILE * f = fopen(tmp_name.c_str(), "r");

				std::string title;
				std::string URL;
				std::string url;
				std::string desc;
				t_channel_id epg_id = 0;
				CZapitBouquet* pbouquet = NULL;

				if(f != NULL)
				{
					while(true)
					{
						char line[1024];
						if (!fgets(line, 1024, f))
							break;

						size_t len = strlen(line);
						if (len < 2)
							// Lines with less than one char aren't meaningful
							continue;

						// strip newline
						line[--len] = 0;

						// strip carriage return (when found)
						if (line[len - 1] == '\r')
							line[len - 1 ] = 0;

						if (strncmp(line, "#SERVICE 4097:0:1:0:0:0:0:0:0:0:", 32) == 0)
							url = line + 32;
						else if (strncmp(line, "#DESCRIPTION", 12) == 0)
						{
							int offs = line[12] == ':' ? 14 : 13;

							title = line + offs;

							desc = "e2 stream";

							pbouquet = addBouquetIfNotExist((mode == MODE_WEBTV) ? "WebTV" : "WebRadio");
							if (mode == MODE_WEBTV)
								pbouquet->bWebtv = true;
							else
								pbouquet->bWebradio = true;

							t_channel_id chid = create_channel_id64(0, 0, 0, 0, 0, ::decodeUrl(url).c_str());
							CZapitChannel * channel = new CZapitChannel(title.c_str(), chid, ::decodeUrl(url).c_str(), desc.c_str(), epg_id, NULL, mode);
							CServiceManager::getInstance()->AddChannel(channel);
							channel->flags = CZapitChannel::UPDATED;
							if (pbouquet)
								pbouquet->addService(channel);

						}
					}
					fclose(f);
				}
			}
		}
		if (remove_tmp)
			remove(tmp_name.c_str());
	}
}

void CBouquetManager::loadLogos()
{

	if(thrLogo != 0)
	{
		pthread_cancel(thrLogo);
		pthread_join(thrLogo, NULL);
		thrLogo = 0;
	}

	if (LogoList.size() > 0)
		pthread_create(&thrLogo, NULL, LogoThread, (void*)&LogoList);
}

void* CBouquetManager::LogoThread(void* _logolist)
{
	set_threadname(__func__);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock (&mutex);
	ZapitChannelList *LogoList = (ZapitChannelList *)_logolist;
	for (ZapitChannelList::iterator it = LogoList->begin(); it != LogoList->end(); ++it)
	{
		CZapitChannel *cc = (*it);
		if (cc)
			cc->setAlternateLogo(downloadUrlToLogo(cc->getAlternateLogo(), LOGODIR_TMP, cc->getChannelID()));
	}
	LogoList->clear();
	pthread_mutex_unlock(&mutex);
	pthread_exit(0);
	return NULL;
}

CBouquetManager::ChannelIterator::ChannelIterator(CBouquetManager* owner, const bool TV)
{
	Owner = owner;
	tv = TV;
	if (Owner->Bouquets.empty())
		c = -2;
	else {
		b = 0;
		c = -1;
		(*this)++;
	}
}

CBouquetManager::ChannelIterator CBouquetManager::ChannelIterator::operator ++(int)
{
	if (c != -2)  // we can add if it's not the end marker
	{
		c++;
		if ((unsigned int) c >= getBouquet()->size()) {
			for (b++; b < Owner->Bouquets.size(); b++)
				if ( !getBouquet()->empty() ) {
					c = 0;
					goto end;
				}
			c = -2;
		}
	}
 end:
	return(*this);
}

CZapitChannel* CBouquetManager::ChannelIterator::operator *()
{
	return (*getBouquet())[c];               // returns junk if we are an end marker !!
}

CBouquetManager::ChannelIterator CBouquetManager::ChannelIterator::FindChannelNr(const unsigned int channel)
{
	c = channel;
	for (b = 0; b < Owner->Bouquets.size(); b++)
		if (getBouquet()->size() > (unsigned int)c)
			goto end;
		else
			c -= getBouquet()->size();
	c = -2;
 end:
	return (*this);
}

#if 0
int CBouquetManager::ChannelIterator::getLowestChannelNumberWithChannelID(const t_channel_id channel_id)
{
	int i = 0;

	for (b = 0; b < Owner->Bouquets.size(); b++)
		for (c = 0; (unsigned int) c < getBouquet()->size(); c++, i++)
			if ((**this)->getChannelID() == channel_id)
			    return (**this)->number -1;
			    //return i;
	return -1; // not found
}
#endif

int CBouquetManager::ChannelIterator::getNrofFirstChannelofBouquet(const unsigned int bouquet_nr)
{
	if (bouquet_nr >= Owner->Bouquets.size())
		return -1;  // not found

	int i = 0;

	for (b = 0; b < bouquet_nr; b++)
		i += getBouquet()->size();

	return i;
}

t_channel_id CBouquetManager::reMapEpgID(t_channel_id channelid)
{
	if(!EpgIDMapping.empty()){
		std::map<t_channel_id, t_channel_id>::iterator it = EpgIDMapping.find(channelid);
		if ( it != EpgIDMapping.end() )
			return it->second;
	}
	return 0;
}

std::string CBouquetManager::reMapEpgXML(t_channel_id channelid)
{
	if(!EpgXMLMapping.empty()){
		std::map<t_channel_id, std::string>::iterator it = EpgXMLMapping.find(channelid);
		if ( it != EpgXMLMapping.end() )
			return it->second;
	}
	return "";
}

void CBouquetManager::readEPGMapping()
{
	if(!EpgIDMapping.empty())
		EpgIDMapping.clear();

	if(!EpgXMLMapping.empty())
		EpgXMLMapping.clear();

	const std::string epg_map_dir = CONFIGDIR "/zapit/epgmap.xml";
	xmlDocPtr epgmap_parser = parseXmlFile(epg_map_dir.c_str());

	if (epgmap_parser != NULL)
	{

		xmlNodePtr epgmap = xmlDocGetRootElement(epgmap_parser);
		if(epgmap)
			epgmap = xmlChildrenNode(epgmap);

		while (epgmap)
		{
			const char *channelid = xmlGetAttribute(epgmap, "channel_id");
			const char *epgid = xmlGetAttribute(epgmap, "new_epg_id");
			const char *xmlepg = xmlGetData(epgmap); // returns empty string, not NULL if nothing found
			t_channel_id epg_id = 0;
			t_channel_id channel_id = 0;
			if (epgid)
				epg_id = strtoull(epgid, NULL, 16);
			if (channelid)
				channel_id = strtoull(channelid, NULL, 16);
			if(channel_id && epg_id){
				EpgIDMapping[channel_id]=epg_id;
			}
			if(channel_id && ((xmlepg != NULL) && (xmlepg[0] != '\0'))){
				EpgXMLMapping[channel_id]=xmlepg;
			}
			epgmap = xmlNextNode(epgmap);
		}
	}
	xmlFreeDoc(epgmap_parser);
}

void CBouquetManager::convert_E2_EPGMapping(std::string mapfile_in, std::string mapfile_out)
{
	FILE * outfile = NULL;
	xmlDocPtr epgmap_parser = parseXmlFile(mapfile_in.c_str());

	if (epgmap_parser != NULL)
	{
		outfile = fopen(mapfile_out.c_str(), "w");
		xmlNodePtr epgmap = xmlDocGetRootElement(epgmap_parser);
		if(epgmap)
			epgmap = xmlChildrenNode(epgmap);

		fprintf(outfile,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<zapit>\n");

		while (epgmap) {
			const char *e2_channelid = xmlGetData(epgmap);
			const char *xmlepg = xmlGetAttribute(epgmap, "id");
			u_int sid = 0,tsid = 0,onid = 0,sat = 0;
			sscanf(e2_channelid,"1:0:1:%X:%X:%X:%X0000:0:0:0:", &sid, &tsid, &onid, &sat);
			t_channel_id short_channel_id = create_channel_id(sid,onid,tsid);
			CZapitChannel * cc = CServiceManager::getInstance()->FindChannel48(short_channel_id);
			if (cc) {
				if(xmlepg){
					fprintf(outfile,"\t<filter channel_id=\"%012" PRIx64 "\" >%s</filter> --%s\n",cc->getChannelID(), xmlepg, cc->getName().c_str());
				}
			}
			epgmap = xmlNextNode(epgmap);
		}
		fprintf(outfile, "</zapit>\n");
		fclose(outfile);
	}
	xmlFreeDoc(epgmap_parser);
}

std::string CBouquetManager::ReadMarkerValue(std::string strLine, const char* strMarkerName)
{
	if (strLine.find(strMarkerName) != std::string::npos)
	{
		strLine = strLine.substr(strLine.find(strMarkerName));
		strLine = strLine.substr(strLine.find_first_of('"')+1);
		strLine = strLine.substr(0,strLine.find_first_of('"'));
		return strLine;
	}

	return std::string("");
}
