/*
 * $Header: /cvs/tuxbox/apps/dvb/zapit/lib/zapitclient.cpp,v 1.105 2004/10/27 16:08:41 lucgas Exp $ *
 *
 * Zapit client interface - DBoxII-Project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de> & the DBoxII-Project
 * (C) 2007-2012 Stefan Seyfried
 *
 * License: GPL
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

#include <config.h>
#include <cstdio>
#include <cstring>

#include <OpenThreads/ScopedLock>

/* libevent */
#include <eventserver.h>

#include <zapit/client/zapittypes.h>
#include <zapit/client/zapitclient.h>
#include <zapit/client/msgtypes.h>
#include <zapit/client/zapittools.h>

#ifdef PEDANTIC_VALGRIND_SETUP
#define VALGRIND_PARANOIA memset(&msg, 0, sizeof(msg))
#else
#define VALGRIND_PARANOIA {}
#endif

unsigned char CZapitClient::getVersion() const
{
	return CZapitMessages::ACTVERSION;
}

const char * CZapitClient::getSocketName() const
{
	return ZAPIT_UDS_NAME;
}

void CZapitClient::shutdown()
{
	send(CZapitMessages::CMD_SHUTDOWN);
	close_connection();
}

//***********************************************/
/*					     */
/* general functions for zapping	       */
/*					     */
/***********************************************/

/* zaps to channel of specified bouquet */
/* bouquets are numbered starting at 0 */
void CZapitClient::zapTo(const unsigned int bouquet, const unsigned int channel)
{
	CZapitMessages::commandZapto msg;
	VALGRIND_PARANOIA;

	msg.bouquet = bouquet;
	msg.channel = channel - 1;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO, (char*)&msg, sizeof(msg));

	close_connection();
}

/* zaps to channel by nr */
void CZapitClient::zapTo(const unsigned int channel)
{
	CZapitMessages::commandZaptoChannelNr msg;
	VALGRIND_PARANOIA;

	msg.channel = channel - 1;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_CHANNELNR, (const char *) & msg, sizeof(msg));

	close_connection();
}

t_channel_id CZapitClient::getCurrentServiceID()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_CURRENT_SERVICEID);

	CZapitMessages::responseGetCurrentServiceID response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();

	return response.channel_id;
}

CZapitClient::CCurrentServiceInfo CZapitClient::getCurrentServiceInfo()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_CURRENT_SERVICEINFO);

	CZapitClient::CCurrentServiceInfo response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();
	return response;
}

#if 0
void CZapitClient::getLastChannel(t_channel_id &channel_id, int &mode)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_LAST_CHANNEL);

	CZapitClient::responseGetLastChannel response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	channel_id = response.channel_id;
	mode = response.mode;

	close_connection();
}
#endif

#if 0
int32_t CZapitClient::getCurrentSatellitePosition(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_CURRENT_SATELLITE_POSITION);

	int32_t response;
	CBasicClient::receive_data((char *)&response, sizeof(response));

	close_connection();
	return response;
}
#endif

void CZapitClient::setAudioChannel(const unsigned int channel)
{
	CZapitMessages::commandSetAudioChannel msg;
	VALGRIND_PARANOIA;

	msg.channel = channel;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_AUDIOCHAN, (const char *) & msg, sizeof(msg));

	close_connection();
}

/* zaps to onid_sid, returns the "zap-status" */
unsigned int CZapitClient::zapTo_serviceID(const t_channel_id channel_id)
{
	CZapitMessages::commandZaptoServiceID msg;
	VALGRIND_PARANOIA;

	msg.channel_id = channel_id;
	msg.record = false;
	msg.pip = false;
	msg.epg = false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_SERVICEID, (const char *) & msg, sizeof(msg));

	CZapitMessages::responseZapComplete response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();

	return response.zapStatus;
}

unsigned int CZapitClient::zapTo_record(const t_channel_id channel_id)
{
	CZapitMessages::commandZaptoServiceID msg;
	VALGRIND_PARANOIA;

	msg.channel_id = channel_id;
	msg.record = true;
	msg.pip = false;
	msg.epg = false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_SERVICEID, (const char *) & msg, sizeof(msg));

	CZapitMessages::responseZapComplete response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();

	return response.zapStatus;
}

unsigned int CZapitClient::zapTo_pip(const t_channel_id channel_id)
{
	CZapitMessages::commandZaptoServiceID msg;

	msg.channel_id = channel_id;
	msg.record = false;
	msg.pip = true;
	msg.epg = false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_SERVICEID, (const char *) & msg, sizeof(msg));

	CZapitMessages::responseZapComplete response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();

	return response.zapStatus;
}

unsigned int CZapitClient::zapTo_epg(const t_channel_id channel_id, bool standby)
{
	CZapitMessages::commandZaptoEpg msg;

	msg.channel_id = channel_id;
	msg.standby = standby;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_EPG, (const char *) & msg, sizeof(msg));

	CZapitMessages::responseZapComplete response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();

	return response.zapStatus;
}

unsigned int CZapitClient::zapTo_subServiceID(const t_channel_id channel_id)
{
	CZapitMessages::commandZaptoServiceID msg;
	VALGRIND_PARANOIA;

	msg.channel_id = channel_id;
	msg.record = false;
	msg.pip = false;
	msg.epg = false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_SUBSERVICEID, (const char *) & msg, sizeof(msg));

	CZapitMessages::responseZapComplete response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();

	return response.zapStatus;
}

/* zaps to channel, does NOT wait for completion (uses event) */
void CZapitClient::zapTo_serviceID_NOWAIT(const t_channel_id channel_id)
{
	CZapitMessages::commandZaptoServiceID msg;
	VALGRIND_PARANOIA;

	msg.channel_id = channel_id;
	msg.record = false;
	msg.pip = false;
	msg.epg = false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_SERVICEID_NOWAIT, (const char *) & msg, sizeof(msg));

	close_connection();
}

/* zaps to subservice, does NOT wait for completion (uses event) */
void CZapitClient::zapTo_subServiceID_NOWAIT(const t_channel_id channel_id)
{
	CZapitMessages::commandZaptoServiceID msg;
	VALGRIND_PARANOIA;

	msg.channel_id = channel_id;
	msg.record = false;
	msg.pip = false;
	msg.epg = false;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_ZAPTO_SUBSERVICEID_NOWAIT, (const char *) & msg, sizeof(msg));

	close_connection();
}


void CZapitClient::setMode(const channelsMode mode)
{
	CZapitMessages::commandSetMode msg;
	VALGRIND_PARANOIA;

	msg.mode = mode;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_MODE, (const char *) & msg, sizeof(msg));

	close_connection();
}

int CZapitClient::getMode()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_MODE);

	CZapitMessages::responseGetMode response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();
	return response.mode;
}

void CZapitClient::setSubServices( subServiceList& subServices )
{
	unsigned int i;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SETSUBSERVICES);

	for (i = 0; i< subServices.size(); i++)
		send_data((char*)&subServices[i], sizeof(subServices[i]));

	close_connection();
}

void CZapitClient::getPIDS(responseGetPIDs& pids)
{
	CZapitMessages::responseGeneralInteger responseInteger;
	responseGetAPIDs                       responseAPID;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GETPIDS);

	CBasicClient::receive_data((char* )&(pids.PIDs), sizeof(pids.PIDs));

	pids.APIDs.clear();

	if (CBasicClient::receive_data((char* )&responseInteger, sizeof(responseInteger)))
	{
		pids.APIDs.reserve(responseInteger.number);

		while (responseInteger.number-- > 0)
		{
			CBasicClient::receive_data((char*)&responseAPID, sizeof(responseAPID));
			pids.APIDs.push_back(responseAPID);
		}
	}

	close_connection();
}

void CZapitClient::zaptoNvodSubService(const int num)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;

	msg.val = num;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_NVOD_SUBSERVICE_NUM, (const char *) & msg, sizeof(msg));

	close_connection();
}

/* gets all bouquets */
/* bouquets are numbered starting at 0 */
void CZapitClient::getBouquets(BouquetList& bouquets, const bool emptyBouquetsToo, const bool utf_encoded, channelsMode mode)
{
	char buffer[30 + 1];

	CZapitMessages::commandGetBouquets msg;
	VALGRIND_PARANOIA;

	msg.emptyBouquetsToo = emptyBouquetsToo;
	msg.mode = mode;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_BOUQUETS, (char*)&msg, sizeof(msg));

	responseGetBouquets response;
	while (CBasicClient::receive_data((char*)&response, sizeof(responseGetBouquets)))
	{
		if (response.bouquet_nr == RESPONSE_GET_BOUQUETS_END_MARKER)
			break;

		if (!utf_encoded)
		{
			buffer[30] = (char) 0x00;
			strncpy(buffer, response.name, sizeof(buffer)-1);
			strncpy(response.name, ZapitTools::UTF8_to_Latin1(buffer).c_str(), sizeof(buffer)-1);
		}
		bouquets.push_back(response);
	}

	close_connection();
}


bool CZapitClient::receive_channel_list(BouquetChannelList& channels, const bool utf_encoded)
{
	CZapitMessages::responseGeneralInteger responseInteger;
	responseGetBouquetChannels             response;

	channels.clear();

	if (CBasicClient::receive_data((char* )&responseInteger, sizeof(responseInteger)))
	{
		channels.reserve(responseInteger.number);

		while (responseInteger.number-- > 0)
		{
			if (!CBasicClient::receive_data((char*)&response, sizeof(responseGetBouquetChannels)))
				return false;

			response.nr++;
			if (!utf_encoded)
			{
                                char buffer[CHANNEL_NAME_SIZE + 1];
                                buffer[CHANNEL_NAME_SIZE] = (char) 0x00;
                                strncpy(buffer, response.name, CHANNEL_NAME_SIZE-1);
                                strncpy(response.name, ZapitTools::UTF8_to_Latin1(buffer).c_str(), CHANNEL_NAME_SIZE-1);
			}
			channels.push_back(response);
		}
	}
	return true;
}
#if 0 
//never used
bool CZapitClient::receive_nchannel_list(BouquetNChannelList& channels)
{
	CZapitMessages::responseGeneralInteger responseInteger;
	responseGetBouquetNChannels             response;

	channels.clear();

	if (CBasicClient::receive_data((char* )&responseInteger, sizeof(responseInteger)))
	{
		channels.reserve(responseInteger.number);

		while (responseInteger.number-- > 0)
		{
			if (!CBasicClient::receive_data((char*)&response, sizeof(responseGetBouquetNChannels)))
				return false;

			response.nr++;
			channels.push_back(response);
		}
	}
	return true;
}
#endif

/* gets all channels that are in specified bouquet */
/* bouquets are numbered starting at 0 */
bool CZapitClient::getBouquetChannels(const unsigned int bouquet, BouquetChannelList& channels, channelsMode mode, const bool utf_encoded)
{
	bool                                      return_value;
	CZapitMessages::commandGetBouquetChannels msg;
	VALGRIND_PARANOIA;

	msg.bouquet = bouquet;
	msg.mode = mode;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	return_value = (send(CZapitMessages::CMD_GET_BOUQUET_CHANNELS, (char*)&msg, sizeof(msg))) ? receive_channel_list(channels, utf_encoded) : false;

	close_connection();
	return return_value;
}

#if 0
bool CZapitClient::getBouquetNChannels(const unsigned int bouquet, BouquetNChannelList& channels, channelsMode mode, const bool /*utf_encoded*/)
{
	bool                                      return_value;
	CZapitMessages::commandGetBouquetChannels msg;

	msg.bouquet = bouquet;
	msg.mode = mode;

	return_value = (send(CZapitMessages::CMD_GET_BOUQUET_NCHANNELS, (char*)&msg, sizeof(msg))) ? receive_nchannel_list(channels) : false;

	close_connection();
	return return_value;
}
#endif
/* gets all channels */
bool CZapitClient::getChannels( BouquetChannelList& channels, channelsMode mode, channelsOrder order, const bool utf_encoded)
{
	bool                               return_value;
	CZapitMessages::commandGetChannels msg;
	VALGRIND_PARANOIA;

	msg.mode = mode;
	msg.order = order;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	return_value = (send(CZapitMessages::CMD_GET_CHANNELS, (char*)&msg, sizeof(msg))) ? receive_channel_list(channels, utf_encoded) : false;

	close_connection();
	return return_value;
}



/* request information about a particular channel_id */

/* channel name */
std::string CZapitClient::getChannelName(const t_channel_id channel_id)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_CHANNEL_NAME, (char *) & channel_id, sizeof(channel_id));

	CZapitMessages::responseGetChannelName response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
	return std::string(response.name);
}

#if 0
/* is channel a TV channel ? */
bool CZapitClient::isChannelTVChannel(const t_channel_id channel_id)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_IS_TV_CHANNEL, (char *) & channel_id, sizeof(channel_id));

	CZapitMessages::responseGeneralTrueFalse response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
	return response.status;
}
#endif



/* restore bouquets so as if they were just loaded */
void CZapitClient::restoreBouquets()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_RESTORE);

	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
}

/* reloads channels and services*/
void CZapitClient::reinitChannels()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_REINIT_CHANNELS);

	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response), true);
	close_connection();
}

//called when sectionsd updates currentservices.xml
void CZapitClient::reloadCurrentServices()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_RELOAD_CURRENTSERVICES);
#if 0
	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response), true);
#endif
	close_connection();
}

void CZapitClient::muteAudio(const bool mute)
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;

	msg.truefalse = mute;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_MUTE, (char*)&msg, sizeof(msg));

	close_connection();
}
// Get mute status
bool CZapitClient::getMuteStatus()
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_MUTE_STATUS, (char*)&msg, sizeof(msg));
	CBasicClient::receive_data((char*)&msg, sizeof(msg));
	close_connection();
	return msg.truefalse;
}

void CZapitClient::setVolume(const unsigned int left, const unsigned int right)
{
	CZapitMessages::commandVolume msg;
	VALGRIND_PARANOIA;

	msg.left = left;
	msg.right = right;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_VOLUME, (char*)&msg, sizeof(msg));

	close_connection();
}

void CZapitClient::getVolume(unsigned int *left, unsigned int *right)
{
        CZapitMessages::commandVolume msg;
	VALGRIND_PARANOIA;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
        send(CZapitMessages::CMD_GET_VOLUME, 0, 0);

        CBasicClient::receive_data((char*)&msg, sizeof(msg));
        *left = msg.left;
        *right = msg.right;

        close_connection();
}

void CZapitClient::lockRc(const bool b)
{
	CZapitMessages::commandBoolean msg;
	//VALGRIND_PARANOIA;

	msg.truefalse = b;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_LOCKRC, (char*)&msg, sizeof(msg));

	close_connection();
}
#if 0 
//never used
delivery_system_t CZapitClient::getDeliverySystem(void)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_DELIVERY_SYSTEM, 0, 0);

	CZapitMessages::responseDeliverySystem response;

	if (!CBasicClient::receive_data((char* )&response, sizeof(response)))
		response.system = DVB_S;  // return DVB_S if communication fails

	close_connection();

	return response.system;
}
#endif
#if 0
bool CZapitClient::get_current_TP(transponder* TP)
{
	TP_params TP_temp;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_CURRENT_TP);
	bool reply = CBasicClient::receive_data((char*)&TP_temp, sizeof(TP_temp));
	memmove(TP, &TP_temp, sizeof(TP_temp));
	close_connection();
	return reply;
}
#endif
/* sends diseqc 1.2 motor command */
void CZapitClient::sendMotorCommand(uint8_t cmdtype, uint8_t address, uint8_t cmd, uint8_t num_parameters, uint8_t param1, uint8_t param2)
{
	CZapitMessages::commandMotor msg;
	VALGRIND_PARANOIA;

	msg.cmdtype = cmdtype;
	msg.address = address;
	msg.cmd = cmd;
	msg.num_parameters = num_parameters;
	msg.param1 = param1;
	msg.param2 = param2;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SEND_MOTOR_COMMAND, (char*)&msg, sizeof(msg));

	close_connection();
}


/***********************************************/
/*                                             */
/*  Scanning stuff                             */
/*                                             */
/***********************************************/

/* start TS-Scan */
bool CZapitClient::startScan(const int scan_mode)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	bool reply = send(CZapitMessages::CMD_SCANSTART, (char*)&scan_mode, sizeof(scan_mode));

	close_connection();

	return reply;
}
bool CZapitClient::stopScan()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
        bool reply = send(CZapitMessages::CMD_SCANSTOP);
        close_connection();
        return reply;
}
#if 0
bool CZapitClient::setConfig(Zapit_config Cfg)
{
        //bool reply = send(CZapitMessages::CMD_LOADCONFIG);
	bool reply = send(CZapitMessages::CMD_SETCONFIG, (char*)&Cfg, sizeof(Cfg));
        close_connection();
        return reply;
}
void CZapitClient::getConfig (Zapit_config * Cfg)
{
	send(CZapitMessages::CMD_GETCONFIG);
	CBasicClient::receive_data((char *) Cfg, sizeof(Zapit_config));
	close_connection();
}
#endif
bool CZapitClient::Rezap()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
        bool reply = send(CZapitMessages::CMD_REZAP);
        close_connection();
        return reply;
}

/* start manual scan */
bool CZapitClient::scan_TP(TP_params TP)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	bool reply = send(CZapitMessages::CMD_SCAN_TP, (char*)&TP, sizeof(TP));
	close_connection();
	return reply;
}
bool CZapitClient::tune_TP(TP_params TP)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	bool reply = send(CZapitMessages::CMD_TUNE_TP, (char*)&TP, sizeof(TP));
	close_connection();
	return reply;
}

/* query if ts-scan is ready - response gives status */
bool CZapitClient::isScanReady(unsigned int &satellite,  unsigned int &processed_transponder, unsigned int &transponder, unsigned int &services )
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANREADY);

	CZapitMessages::responseIsScanReady response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	satellite = response.satellite;
	processed_transponder = response.processed_transponder;
	transponder = response.transponder;
	services = response.services;

	close_connection();
	return response.scanReady;
}

/* query possible satellits*/
void CZapitClient::getScanSatelliteList(SatelliteList& satelliteList)
{
	uint32_t  satlength;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANGETSATLIST);

	responseGetSatelliteList response;
	while (CBasicClient::receive_data((char*)&satlength, sizeof(satlength)))
	{
		if (satlength == SATNAMES_END_MARKER)
			break;

		if (!CBasicClient::receive_data((char*)&(response), satlength))
			break;

		satelliteList.push_back(response);
	}

	close_connection();

}

/* tell zapit which satellites to scan*/
void CZapitClient::setScanSatelliteList( ScanSatelliteList& satelliteList )
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANSETSCANSATLIST);

	for (uint32_t i=0; i<satelliteList.size(); i++)
	{
		send_data((char*)&satelliteList[i], sizeof(satelliteList[i]));
	}
	close_connection();
}

#if 0
/* tell zapit stored satellite positions in diseqc 1.2 motor */
void CZapitClient::setScanMotorPosList( ScanMotorPosList& motorPosList )
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANSETSCANMOTORPOSLIST);

	for (uint32_t i = 0; i < motorPosList.size(); i++)
	{
		send_data((char*)&motorPosList[i], sizeof(motorPosList[i]));
	}
	close_connection();
}
#endif

/* set diseqcType*/
void CZapitClient::setDiseqcType(const diseqc_t diseqc)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANSETDISEQCTYPE, (const char *) & diseqc, sizeof(diseqc));
	close_connection();
}

/* set diseqcRepeat*/
void CZapitClient::setDiseqcRepeat(const uint32_t  repeat)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANSETDISEQCREPEAT, (const char *) & repeat, sizeof(repeat));
	close_connection();
}

/* set diseqcRepeat*/
void CZapitClient::setScanBouquetMode(const bouquetMode mode)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANSETBOUQUETMODE, (const char *) & mode, sizeof(mode));
	close_connection();
}

#if 0
/* set Scan-TYpe for channelsearch */
void CZapitClient::setScanType(const scanType mode)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SCANSETTYPE, (const char *) & mode, sizeof(mode));
  	close_connection();
}
#endif
#if 0
//
// -- query Frontend Signal parameters
//
void CZapitClient::getFESignal (struct responseFESignal &f)
{
	struct responseFESignal rsignal;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_FE_SIGNAL);
	CBasicClient::receive_data((char *) &rsignal, sizeof(rsignal));

	f.sig = rsignal.sig;
	f.snr = rsignal.snr;
	f.ber = rsignal.ber;

	close_connection();
}
#endif

/***********************************************/
/*                                             */
/* Bouquet editing functions                   */
/*                                             */
/***********************************************/

/* adds bouquet at the end of the bouquetlist  */
void CZapitClient::addBouquet(const char * const name)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	if (send(CZapitMessages::CMD_BQ_ADD_BOUQUET))
		send_string(name);

	close_connection();
}

/* moves a bouquet from one position to another */
/* bouquets are numbered starting at 0 */
void CZapitClient::moveBouquet(const unsigned int bouquet, const unsigned int newPos)
{
	CZapitMessages::commandMoveBouquet msg;
	VALGRIND_PARANOIA;

	msg.bouquet = bouquet;
	msg.newPos = newPos;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_MOVE_BOUQUET, (char*)&msg, sizeof(msg));
	close_connection();
}

/* deletes a bouquet with all its channels*/
/* bouquets are numbered starting at 0 */
void CZapitClient::deleteBouquet(const unsigned int bouquet)
{
	CZapitMessages::commandDeleteBouquet msg;
	VALGRIND_PARANOIA;

	msg.bouquet = bouquet;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_DELETE_BOUQUET, (char*)&msg, sizeof(msg));

	close_connection();
}

/* assigns new name to bouquet */
/* bouquets are numbered starting at 0 */
void CZapitClient::renameBouquet(const unsigned int bouquet, const char * const newName)
{
	CZapitMessages::commandRenameBouquet msg;
	VALGRIND_PARANOIA;

	msg.bouquet = bouquet;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	if (send(CZapitMessages::CMD_BQ_RENAME_BOUQUET, (char*)&msg, sizeof(msg)))
		send_string(newName);

	close_connection();
}

// -- check if Bouquet-Name exists
// -- Return: Bouquet-ID  or  -1 == no Bouquet found
/* bouquets are numbered starting at 0 */
signed int CZapitClient::existsBouquet(const char * const name)
{
	CZapitMessages::responseGeneralInteger response;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	if (send(CZapitMessages::CMD_BQ_EXISTS_BOUQUET))
		send_string(name);

	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
	return response.number;
}

#if 0
// -- check if Channel already is in Bouquet
// -- Return: true/false
/* bouquets are numbered starting at 0 */
bool CZapitClient::existsChannelInBouquet(const unsigned int bouquet, const t_channel_id channel_id)
{
	CZapitMessages::commandExistsChannelInBouquet msg;
	CZapitMessages::responseGeneralTrueFalse response;

	msg.bouquet    = bouquet;
	msg.channel_id = channel_id;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_EXISTS_CHANNEL_IN_BOUQUET, (char*)&msg, sizeof(msg));

	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
	return (unsigned int) response.status;
}
#endif

#if 0
/* moves a channel of a bouquet from one position to another, channel lists begin at position=1*/
/* bouquets are numbered starting at 0 */
void CZapitClient::moveChannel( unsigned int bouquet, unsigned int oldPos, unsigned int newPos, channelsMode mode)
{
	CZapitMessages::commandMoveChannel msg;

	msg.bouquet = bouquet;
	msg.oldPos  = oldPos - 1;
	msg.newPos  = newPos - 1;
	msg.mode    = mode;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_MOVE_CHANNEL, (char*)&msg, sizeof(msg));

	close_connection();
}
#endif

/* adds a channel at the end of then channel list to specified bouquet */
/* same channels can be in more than one bouquet */
/* bouquets can contain both tv and radio channels */
/* bouquets are numbered starting at 0 */
void CZapitClient::addChannelToBouquet(const unsigned int bouquet, const t_channel_id channel_id)
{
	CZapitMessages::commandAddChannelToBouquet msg;
	VALGRIND_PARANOIA;

	msg.bouquet    = bouquet;
	msg.channel_id = channel_id;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_ADD_CHANNEL_TO_BOUQUET, (char*)&msg, sizeof(msg));

	close_connection();
}

/* removes a channel from specified bouquet */
/* bouquets are numbered starting at 0 */
void CZapitClient::removeChannelFromBouquet(const unsigned int bouquet, const t_channel_id channel_id)
{
	CZapitMessages::commandRemoveChannelFromBouquet msg;
	VALGRIND_PARANOIA;

	msg.bouquet    = bouquet;
	msg.channel_id = channel_id;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_REMOVE_CHANNEL_FROM_BOUQUET, (char*)&msg, sizeof(msg));

	close_connection();
}

/* set a bouquet's lock-state*/
/* bouquets are numbered starting at 0 */
void CZapitClient::setBouquetLock(const unsigned int bouquet, const bool b)
{
	CZapitMessages::commandBouquetState msg;
	VALGRIND_PARANOIA;

	msg.bouquet = bouquet;
	msg.state   = b;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_SET_LOCKSTATE, (char*)&msg, sizeof(msg));

	close_connection();
}

/* set a bouquet's hidden-state*/
/* bouquets are numbered starting at 0 */
void CZapitClient::setBouquetHidden(const unsigned int bouquet, const bool hidden)
{
	CZapitMessages::commandBouquetState msg;
	VALGRIND_PARANOIA;

	msg.bouquet = bouquet;
	msg.state   = hidden;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_SET_HIDDENSTATE, (char*)&msg, sizeof(msg));
	close_connection();
}

/* renums the channellist, means gives the channels new numbers */
/* based on the bouquet order and their order within bouquets */
/* necessarily after bouquet editing operations*/
void CZapitClient::renumChannellist()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_RENUM_CHANNELLIST);
	close_connection();
}


/* saves current bouquet configuration to bouquets.xml*/
void CZapitClient::saveBouquets(const bool saveall)
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;
	msg.truefalse = saveall;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_BQ_SAVE_BOUQUETS, (char*)&msg, sizeof(msg));

	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();
}

void CZapitClient::setStandby(const bool enable)
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;
	msg.truefalse = enable;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_STANDBY, (char*)&msg, sizeof(msg));
	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
}

void CZapitClient::setVideoSystem(int video_system)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;
	msg.val = video_system;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_VIDEO_SYSTEM, (char*)&msg, sizeof(msg));
	close_connection();
}

void CZapitClient::startPlayBack(const bool sendpmt)
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;
	msg.truefalse = sendpmt;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SB_START_PLAYBACK, (char*)&msg, sizeof(msg));
	close_connection();
}

void CZapitClient::stopPlayBack(const bool sendpmt)
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;
	msg.truefalse = sendpmt;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SB_STOP_PLAYBACK, (char*)&msg, sizeof(msg));
	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
}

void CZapitClient::stopPip()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_STOP_PIP);
	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
}

void CZapitClient::lockPlayBack(const bool sendpmt)
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;
	msg.truefalse = sendpmt;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SB_LOCK_PLAYBACK, (char*)&msg, sizeof(msg));
	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
}
void CZapitClient::unlockPlayBack(const bool sendpmt)
{
	CZapitMessages::commandBoolean msg;
	VALGRIND_PARANOIA;
	msg.truefalse = sendpmt;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SB_UNLOCK_PLAYBACK, (char*)&msg, sizeof(msg));
	CZapitMessages::responseCmd response;
	CBasicClient::receive_data((char* )&response, sizeof(response));
	close_connection();
}

bool CZapitClient::isPlayBackActive()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SB_GET_PLAYBACK_ACTIVE);

	CZapitMessages::responseGetPlaybackState response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();
	return response.activated;
}
#if 0
void CZapitClient::setDisplayFormat(const video_display_format_t format)
{
	CZapitMessages::commandInt msg;
	msg.val = format;
	send(CZapitMessages::CMD_SET_DISPLAY_FORMAT, (char*)&msg, sizeof(msg));
	close_connection();
}
#endif
void CZapitClient::setAudioMode(const int mode)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;
	msg.val = mode;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_AUDIO_MODE, (char*)&msg, sizeof(msg));
	close_connection();
}

#if 0
void CZapitClient::getAudioMode(int * mode)
{
        CZapitMessages::commandInt msg;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
        send(CZapitMessages::CMD_GET_AUDIO_MODE, 0, 0);
        CBasicClient::receive_data((char* )&msg, sizeof(msg));
        * mode = msg.val;
        close_connection();
}
#endif
void CZapitClient::setRecordMode(const bool activate)
{
	CZapitMessages::commandSetRecordMode msg;
	VALGRIND_PARANOIA;
	msg.activate = activate;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_RECORD_MODE, (char*)&msg, sizeof(msg));
	close_connection();
}
#if 0 
//never used
void CZapitClient::setEventMode(const bool activate)
{
	CZapitMessages::commandSetRecordMode msg;
	msg.activate = activate;
	send(CZapitMessages::CMD_SET_EVENT_MODE, (char*)&msg, sizeof(msg));
	close_connection();
}
#endif
bool CZapitClient::isRecordModeActive()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_RECORD_MODE);

	CZapitMessages::responseGetRecordModeState response;
	CBasicClient::receive_data((char* )&response, sizeof(response));

	close_connection();
	return response.activated;
}

void CZapitClient::getAspectRatio(int *ratio)
{
	CZapitMessages::commandInt msg;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_ASPECTRATIO, 0, 0);
	CBasicClient::receive_data((char* )&msg, sizeof(msg));
	* ratio = msg.val;
	close_connection();
}

void CZapitClient::setAspectRatio(int ratio)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;
	msg.val = ratio;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_ASPECTRATIO, (char*)&msg, sizeof(msg));
	close_connection();
}

void CZapitClient::getOSDres(int *mosd)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_OSD_RES, 0, 0);
	CBasicClient::receive_data((char* )&msg, sizeof(msg));
	* mosd = msg.val;
	close_connection();
}

void CZapitClient::setOSDres(int mosd)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;
	msg.val = mosd;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_OSD_RES, (char*)&msg, sizeof(msg));
	close_connection();
}

void CZapitClient::getMode43(int *m43)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_GET_MODE43, 0, 0);
	CBasicClient::receive_data((char* )&msg, sizeof(msg));
	* m43 = msg.val;
	close_connection();
}

void CZapitClient::setMode43(int m43)
{
	CZapitMessages::commandInt msg;
	VALGRIND_PARANOIA;
	msg.val = m43;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_SET_MODE43, (char*)&msg, sizeof(msg));
	close_connection();
}

void CZapitClient::registerEvent(const unsigned int eventID, const unsigned int clientID, const char * const udsName)
{
	CEventServer::commandRegisterEvent msg;
	VALGRIND_PARANOIA;

	msg.eventID = eventID;
	msg.clientID = clientID;

	strcpy(msg.udsName, udsName);

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_REGISTEREVENTS, (char*)&msg, sizeof(msg));

	close_connection();
}

void CZapitClient::unRegisterEvent(const unsigned int eventID, const unsigned int clientID)
{
	CEventServer::commandUnRegisterEvent msg;
	VALGRIND_PARANOIA;

	msg.eventID = eventID;
	msg.clientID = clientID;

	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);
	send(CZapitMessages::CMD_UNREGISTEREVENTS, (char*)&msg, sizeof(msg));

	close_connection();
}
