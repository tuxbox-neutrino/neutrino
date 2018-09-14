/*
 * $Id: channel.cpp,v 1.17 2003/10/14 12:48:59 thegoodguy Exp $
 *
 * (C) 2002 by Steffen Hehn <mcclean@berlios.de>
 * (C) 2002, 2003 by Andreas Oberritter <obi@tuxbox.org>
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

#include <cstdio>
#include <system/helpers.h>
#include <zapit/zapit.h>
#include <zapit/channel.h>

extern Zapit_config zapitCfg;


CZapitChannel::CZapitChannel(const std::string & p_name, t_service_id p_sid, t_transport_stream_id p_tsid, t_original_network_id p_onid, unsigned char p_service_type, t_satellite_position p_satellite_position, freq_id_t p_freq, const std::string script_name)
{
	name = p_name;
	service_id = p_sid;
	transport_stream_id = p_tsid;
	original_network_id = p_onid;
	serviceType = p_service_type;
	satellitePosition = p_satellite_position;
	freq = p_freq;
	channel_id = CREATE_CHANNEL_ID64;
	epg_id = channel_id;
	script = script_name;
	Init();
//printf("NEW CHANNEL %s %x\n", name.c_str(), (int) this);
}

CZapitChannel::CZapitChannel(const std::string & p_name, t_channel_id p_channel_id, unsigned char p_service_type, t_satellite_position p_satellite_position, freq_id_t p_freq, const std::string script_name)
{
	name = p_name;
	channel_id = p_channel_id;
	service_id = GET_SERVICE_ID_FROM_CHANNEL_ID(channel_id);
	transport_stream_id = GET_TRANSPORT_STREAM_ID_FROM_CHANNEL_ID(channel_id);
	original_network_id = GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(channel_id);
	serviceType = p_service_type;
	satellitePosition = p_satellite_position;
	freq = p_freq;
	epg_id = channel_id;
	script = script_name;
	Init();
}

// For WebTV/WebRadio ...
CZapitChannel::CZapitChannel(const char *p_name, t_channel_id p_channel_id, const char *p_url, const char *p_desc, t_channel_id epgid, const char* script_name, int mode)
{
	if (!p_name || !p_url)
		return;
	name = std::string(p_name);
	url = std::string(p_url);
	if (p_desc)
		desc = std::string(p_desc);
	channel_id = p_channel_id;
	service_id = 0;
	transport_stream_id = 0;
	original_network_id = 0;
	if (mode == MODE_WEBTV)
		serviceType = ST_DIGITAL_TELEVISION_SERVICE;
	else
		serviceType = ST_DIGITAL_RADIO_SOUND_SERVICE;
	satellitePosition = 0;
	freq = 0;
	epg_id = epgid;
	if (script_name)
		script = std::string(script_name);
	else
		script = "";
	Init();
}

void CZapitChannel::Init()
{
	uname = DEFAULT_CH_UNAME;
	//caPmt = NULL;
	rawPmt = NULL;
	pmtLen = 0;
	type = 0;
	number = 0;
	scrambled = 0;
	pname = NULL;
	pmtPid = 0;
	resetPids();
	ttx_language_code = "";
	last_unlocked_EPGid = 0;
	last_unlocked_time = 0;
	has_bouquet = false;
	record_demux = 2;
	pip_demux = 2;
	polarization = 0;
	flags = 0;
	delsys = DVB_S;
	bLockCount = 0;
	bLocked = DEFAULT_CH_LOCKED;
	bUseCI = false;
	thrLogo = NULL;
	altlogo = "";
}

CZapitChannel::~CZapitChannel(void)
{
//printf("DEL CHANNEL %s %x subs %d\n", name.c_str(), (int) this, getSubtitleCount());
	resetPids();
	//setCaPmt(NULL);
	setRawPmt(NULL);
	camap.clear();
}

CZapitAudioChannel *CZapitChannel::getAudioChannel(unsigned char index)
{
	CZapitAudioChannel *retval = NULL;

	if ((index == 0xFF) && (currentAudioChannel < getAudioChannelCount()))
		retval = audioChannels[currentAudioChannel];
	else if (index < getAudioChannelCount())
		retval = audioChannels[index];

	return retval;
}

unsigned short CZapitChannel::getAudioPid(unsigned char index)
{
	unsigned short retval = 0;

	if ((index == 0xFF) && (currentAudioChannel < getAudioChannelCount()))
		retval = audioChannels[currentAudioChannel]->pid;
	else if (index < getAudioChannelCount())
		retval = audioChannels[index]->pid;

	return retval;
}

int CZapitChannel::addAudioChannel(const unsigned short pid, const CZapitAudioChannel::ZapitAudioChannelType audioChannelType, const std::string & description, const unsigned char componentTag)
{
	std::vector <CZapitAudioChannel *>::iterator aI;

	for (aI = audioChannels.begin(); aI != audioChannels.end(); ++aI)
		if ((* aI)->pid == pid) {
			(* aI)->description = description;
                        (* aI)->audioChannelType = audioChannelType;
                        (* aI)->componentTag = componentTag;
			return -1;
		}
	CZapitAudioChannel *tmp = new CZapitAudioChannel();
	tmp->pid = pid;
	tmp->audioChannelType = audioChannelType;
	tmp->description = description;
	tmp->componentTag = componentTag;
	audioChannels.push_back(tmp);
	return 0;
}

void CZapitChannel::resetPids(void)
{
	std::vector<CZapitAudioChannel *>::iterator aI;

	for (aI = audioChannels.begin(); aI != audioChannels.end(); ++aI) {
		delete *aI;
	}

	audioChannels.clear();
	currentAudioChannel = 0;

	pcrPid = 0;
	//pmtPid = 0;
	teletextPid = 0;
	videoPid = 0;
	audioPid = 0;

	/*privatePid = 0;*/
	pidsFlag = false;
        std::vector<CZapitAbsSub *>::iterator subI;
        for (subI = channelSubs.begin(); subI != channelSubs.end(); ++subI){
            delete *subI;
        }
        channelSubs.clear();
        currentSub = 0;
}

unsigned char CZapitChannel::getServiceType(bool real)
{ 
	if(real)
		return serviceType; 
	else
		return serviceType == ST_DIGITAL_RADIO_SOUND_SERVICE ?
			ST_DIGITAL_RADIO_SOUND_SERVICE : ST_DIGITAL_TELEVISION_SERVICE;	
}

bool CZapitChannel::isUHD()
{
	switch(serviceType) {
		case 0x1f:
			if (delsys == DVB_T2)
				return false;
			else
				return true;
		case ST_DIGITAL_TELEVISION_SERVICE:
		case 0x19:
		{
			if (strstr(name.c_str(), "UHD"))
				return true;
			if (strstr(name.c_str(), "4k"))
				return true;
		}
		default:
			return false;
	}
}

bool CZapitChannel::isHD()
{
	switch(serviceType) {
		case 0x1f:
			if (delsys == DVB_T2)
				return true;
			else
				return false;
		case 0x11: case 0x19:
//printf("[zapit] HD channel: %s type 0x%X\n", name.c_str(), serviceType);
			return true;
		case ST_DIGITAL_TELEVISION_SERVICE: {
				  const char *temp = name.c_str();
				  int len = name.size();
				  if((len > 1) && temp[len-2] == 'H' && temp[len-1] == 'D') {
//printf("[zapit] HD channel: %s type 0x%X\n", name.c_str(), serviceType);
					  return true;
				  }
				  return false;
			  }
		case ST_DIGITAL_RADIO_SOUND_SERVICE:
			return false;
		default:
			//printf("[zapit] Unknown channel type 0x%X name %s !!!!!!\n", serviceType, name.c_str());
			return false;
	}
}

void CZapitChannel::addTTXSubtitle(const unsigned int pid, const std::string langCode, const unsigned char magazine_number, const unsigned char page_number, const bool impaired)
{
	CZapitTTXSub* oldSub = 0;
	CZapitTTXSub* tmpSub = 0;
	unsigned char mag_nr = magazine_number ? magazine_number : 8;

printf("[subtitle] TTXSub: PID=0x%04x, lang=%3.3s, page=%1X%02X\n", pid, langCode.c_str(), mag_nr, page_number);
	std::vector<CZapitAbsSub*>::iterator subI;
	for (subI=channelSubs.begin(); subI!=channelSubs.end();++subI){
		if ((*subI)->thisSubType==CZapitAbsSub::TTX){
			tmpSub=reinterpret_cast<CZapitTTXSub*>(*subI);
			if (tmpSub->ISO639_language_code == langCode) {
				oldSub = tmpSub;
				if (tmpSub->pId==pid &&
						tmpSub->teletext_magazine_number==mag_nr &&
						tmpSub->teletext_page_number==page_number &&
						tmpSub->hearingImpaired==impaired) {
					return;
				}
				break;
			}
		}
	}

	if (oldSub) {
		tmpSub=oldSub;
	} else {
		tmpSub = new CZapitTTXSub();
		channelSubs.push_back(tmpSub);
	}
	tmpSub->pId=pid;
	tmpSub->ISO639_language_code=langCode;
	tmpSub->teletext_magazine_number=mag_nr;
	tmpSub->teletext_page_number=page_number;
	tmpSub->hearingImpaired=impaired;
}

void CZapitChannel::addDVBSubtitle(const unsigned int pid, const std::string langCode, const unsigned char subtitling_type, const unsigned short composition_page_id, const unsigned short ancillary_page_id)                                                                                                                                                                                 
{
	CZapitDVBSub* oldSub = 0;
	CZapitDVBSub* tmpSub = 0;
	std::vector<CZapitAbsSub*>::iterator subI;
printf("[subtitles] DVBSub: PID=0x%04x, lang=%3.3s, cpageid=%04x, apageid=%04x\n", pid, langCode.c_str(), composition_page_id, ancillary_page_id);
	for (subI=channelSubs.begin(); subI!=channelSubs.end();++subI){
		if ((*subI)->thisSubType==CZapitAbsSub::DVB){
			tmpSub=reinterpret_cast<CZapitDVBSub*>(*subI);
			if (tmpSub->ISO639_language_code==langCode) {
				//oldSub = tmpSub;
				if (tmpSub->pId==pid &&
						tmpSub->subtitling_type==subtitling_type &&
						tmpSub->composition_page_id==composition_page_id &&
						tmpSub->ancillary_page_id==ancillary_page_id) {

					return;
				}
				break;
			}
		}
	}


	if (oldSub) {
		tmpSub = oldSub;
	} else {
		tmpSub = new CZapitDVBSub();
		channelSubs.push_back(tmpSub);
	}

	tmpSub->pId=pid;
	tmpSub->ISO639_language_code=langCode;
	tmpSub->subtitling_type=subtitling_type;
	tmpSub->composition_page_id=composition_page_id;
	tmpSub->ancillary_page_id=ancillary_page_id;
}

CZapitAbsSub* CZapitChannel::getChannelSub(int index)
{
    CZapitAbsSub* retval = NULL;

    if ((index < 0) && (currentSub < getSubtitleCount())){
        retval = channelSubs[currentSub];
    } else {
        if ((index >= 0) && (index < (int)getSubtitleCount())) {
            retval = channelSubs[index];
        }
    }
    return retval;
}
#if 0 
//never used
void CZapitChannel::setChannelSub(int subIdx)
{
    if (subIdx < (int)channelSubs.size()){
        currentSub=subIdx;
    }
}

int CZapitChannel::getChannelSubIndex(void)
{
    return currentSub < getSubtitleCount() ? currentSub : -1;
}
#endif
#if 0
void CZapitChannel::setCaPmt(CCaPmt *pCaPmt)
{ 
	if(caPmt)
		delete caPmt;
	caPmt = pCaPmt; 
}
#endif

void CZapitChannel::setRawPmt(unsigned char * pmt, int len)
{
	if(rawPmt)
		delete[] rawPmt;
	rawPmt = pmt;
	pmtLen = len;
}

void CZapitChannel::dumpServiceXml(FILE * fd, const char * action)
{
	if(action) {
		fprintf(fd, "\t\t\t<S action=\"%s\" i=\"%04x\" n=\"%s\" t=\"%x\" s=\"%d\" num=\"%d\" f=\"%d\"/>\n", action,
				getServiceId(), convert_UTF8_To_UTF8_XML(name.c_str()).c_str(),
				getServiceType(), scrambled, number, flags);

	} else if(getPidsFlag()) {
		fprintf(fd, "\t\t\t<S i=\"%04x\" n=\"%s\" v=\"%x\" a=\"%x\" p=\"%x\" pmt=\"%x\" tx=\"%x\" t=\"%x\" vt=\"%d\" s=\"%d\" num=\"%d\" f=\"%d\"/>\n",
				getServiceId(), convert_UTF8_To_UTF8_XML(name.c_str()).c_str(),
				getVideoPid(), getPreAudioPid(),
				getPcrPid(), getPmtPid(), getTeletextPid(),
				getServiceType(true), type, scrambled, number, flags);
	} else {
		fprintf(fd, "\t\t\t<S i=\"%04x\" n=\"%s\" t=\"%x\" s=\"%d\" num=\"%d\" f=\"%d\"/>\n",
				getServiceId(), convert_UTF8_To_UTF8_XML(name.c_str()).c_str(),
				getServiceType(true), scrambled, number, flags);
	}
}

void CZapitChannel::dumpBouquetXml(FILE * fd, bool bUser)
{
	// TODO : gui->manage configuration: writeChannelsNames (if nessessary or wanted) in zapit.conf
	// menu > installation > Servicesscan > under 'Bouquet' => "Write DVB-names in bouquets:" f.e. =>0=never 1=ubouquets 2=bouquets 3=both
	int write_names =  zapitCfg.writeChannelsNames;

	fprintf(fd, "\t\t<S");

	// references (mandatory)
	if (url.empty())
		fprintf(fd, " i=\"%x\" t=\"%x\" on=\"%x\" s=\"%hd\" frq=\"%hd\"",getServiceId(), getTransportStreamId(), getOriginalNetworkId(), getSatellitePosition(), getFreqId());
	else
		fprintf(fd, " u=\"%s\"",convert_UTF8_To_UTF8_XML(url.c_str()).c_str());

	// service names
	if (bUser || !url.empty()) {
		if ((write_names & CBouquetManager::BWN_UBOUQUETS) == CBouquetManager::BWN_UBOUQUETS)
			fprintf(fd, " n=\"%s\"", convert_UTF8_To_UTF8_XML(name.c_str()).c_str());
	}
	else {
		if ((write_names & CBouquetManager::BWN_BOUQUETS) == CBouquetManager::BWN_BOUQUETS)
			fprintf(fd, " n=\"%s\"", convert_UTF8_To_UTF8_XML(name.c_str()).c_str());
	}

	// usecase optional attributes
	if (bUser && !uname.empty()) fprintf(fd, " un=\"%s\"", convert_UTF8_To_UTF8_XML(uname.c_str()).c_str());
	// TODO : be sure to save bouquets.xml after "l"L is changed only in ubouquets.xml (set dirty_bit ?)
	if (bLocked!=DEFAULT_CH_LOCKED) fprintf(fd," l=\"%d\"", bLocked ? 1 : 0);

	fprintf(fd, "/>\n");
}

void CZapitChannel::setThrAlternateLogo(const std::string &pLogo)
{
	//printf("CZapitChannel::setAlternateLogo: [%s]\n", pLogo.c_str());

	altlogo = pLogo;

	if(thrLogo != 0)
	{
		pthread_cancel(thrLogo);
		pthread_join(thrLogo, NULL);
		thrLogo = 0;
	}

	pthread_create(&thrLogo, NULL, LogoThread, (void *)this);
}

void* CZapitChannel::LogoThread(void* channel)
{
	CZapitChannel *cc = (CZapitChannel *)channel;
	std::string lname = cc->getAlternateLogo();
	cc->setAlternateLogo(dlTmpName(lname));

	pthread_exit(0);

	return NULL;

}
