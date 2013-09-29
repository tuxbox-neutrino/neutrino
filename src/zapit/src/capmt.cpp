/*
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>,
 *             thegoodguy         <thegoodguy@berlios.de>
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 * Copyright (C) 2012,2013 Stefan Seyfried
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

#include <stdio.h>
/* zapit */
#include <zapit/capmt.h>
#include <zapit/settings.h> /* CAMD_UDS_NAME         */
#include <zapit/getservices.h>
#include <zapit/femanager.h>
#include <zapit/debug.h>

#include <ca_cs.h>
#ifndef HAVE_COOL_HARDWARE
#include <dmx_td.h>
#endif

#include <dvbsi++/program_map_section.h>
#include <dvbsi++/ca_program_map_section.h>

//#define DEBUG_CAPMT

CCam::CCam()
{
	camask = 0;
	for(int i = 0; i < MAX_DMX_UNITS; i++)
		demuxes[i] = 0;
	source_demux = -1;
	calen = 0;
}

unsigned char CCam::getVersion(void) const
{
	return 0x9F;
}

const char *CCam::getSocketName(void) const
{
	return CAMD_UDS_NAME;
}

bool CCam::sendMessage(const char * const data, const size_t length, bool update)
{
	/* send_data return false without trying, if no opened connection */
	if(update) {
		if(!send_data(data, length)) {
			if (!open_connection())
				return false;
			return send_data(data, length);
		}
		return true;
	}

	close_connection();

	if(!data || !length) {
		camask = 1;
		return false;
	}
	if (!open_connection())
		return false;

	return send_data(data, length);
}

bool CCam::makeCaPmt(CZapitChannel * channel, bool add_private, uint8_t list, const CaIdVector &caids)
{
        int len;
        unsigned char * buffer = channel->getRawPmt(len);

	DBG("cam %p source %d camask %d list %02x buffer\n", this, source_demux, camask, list);

	if(!buffer)
		return false;

	ProgramMapSection pmt(buffer);
	CaProgramMapSection capmt(&pmt, list, 0x01, caids);

	if (add_private) {
		uint8_t tmp[10];
		tmp[0] = 0x84;
		tmp[1] = 0x02;
		tmp[2] = channel->getPmtPid() >> 8;
		tmp[3] = channel->getPmtPid() & 0xFF;
		capmt.injectDescriptor(tmp, false);

		tmp[0] = 0x82;
		tmp[1] = 0x02;
		tmp[2] = camask;
		tmp[3] = source_demux;
		capmt.injectDescriptor(tmp, false);

		memset(tmp, 0, sizeof(tmp));
		tmp[0] = 0x81;
		tmp[1] = 0x08;
		tmp[2] = channel->getSatellitePosition() >> 8;
		tmp[3] = channel->getSatellitePosition() & 0xFF;
		tmp[4] = channel->getFreqId() >> 8;
		tmp[5] = channel->getFreqId() & 0xFF;
		tmp[6] = channel->getTransportStreamId() >> 8;
		tmp[7] = channel->getTransportStreamId() & 0xFF;
		tmp[8] = channel->getOriginalNetworkId() >> 8;
		tmp[9] = channel->getOriginalNetworkId() & 0xFF;

		capmt.injectDescriptor(tmp, false);
	}

	calen = capmt.writeToBuffer(cabuf);
#ifdef DEBUG_CAPMT
	printf("CAPMT: ");
	for(int i = 0; i < calen; i++)
		printf("%02X ", cabuf[i]);
	printf("\n");
#endif
	return true;
}

bool CCam::setCaPmt(bool update)
{
	return sendMessage((char *)cabuf, calen, update);
}

bool CCam::sendCaPmt(uint64_t tpid, uint8_t *rawpmt, int rawlen)
{
	return cCA::GetInstance()->SendCAPMT(tpid, source_demux, camask,
			rawpmt ? cabuf : NULL, rawpmt ? calen : 0, rawpmt, rawlen);
}

int CCam::makeMask(int demux, bool add)
{
	int mask = 0;

	if(add)
		demuxes[demux]++;
	else if(demuxes[demux] > 0)
		demuxes[demux]--;

	for(int i = 0; i < MAX_DMX_UNITS; i++) {
		if(demuxes[i] > 0)
			mask |= 1 << i;
	}
	//DBG("demuxes %d:%d:%d:%d old mask %d new mask %d", demuxes[0], demuxes[1], demuxes[2], demuxes[3], camask, mask);
	return mask;
}

CCamManager * CCamManager::manager = NULL;

CCamManager::CCamManager()
{
	channel_map.clear();
}

CCamManager::~CCamManager()
{
	for(cammap_iterator_t it = channel_map.begin(); it != channel_map.end(); it++)
		delete it->second;
	channel_map.clear();
}

CCamManager * CCamManager::getInstance(void)
{
	if(manager == NULL)
		manager = new CCamManager();

	return manager;
}

bool CCamManager::SetMode(t_channel_id channel_id, enum runmode mode, bool start, bool force_update)
{
	CCam * cam;
	int oldmask, newmask;
	int demux = DEMUX_SOURCE_0;
	int source = DEMUX_SOURCE_0;

	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);

	if(channel == NULL) {
		printf("CCamManager: channel %" PRIx64 " not found\n", channel_id);
		return false;
	}
	//INFO("channel %llx [%s] mode %d %s update %d", channel_id, channel->getName().c_str(), mode, start ? "START" : "STOP", force_update);
	mutex.lock();

	cammap_iterator_t it = channel_map.find(channel_id);
	if(it != channel_map.end()) {
		cam = it->second;
	} else if(start) {
		cam = new CCam();
		channel_map.insert(std::pair<t_channel_id, CCam*>(channel_id, cam));
	} else {
		mutex.unlock();
		return false;
	}

	/* FIXME until proper demux management */
	switch(mode) {
		case PLAY:
#if HAVE_COOL_HARDWARE
			source = DEMUX_SOURCE_0;
			demux = LIVE_DEMUX;
#else
			source = cDemux::GetSource(0);
			demux = cDemux::GetSource(0);
			INFO("PLAY: fe_num %d dmx_src %d", CFEManager::getInstance()->allocateFE(channel)->getNumber(), cDemux::GetSource(0));
#endif
			break;
		case STREAM:
		case RECORD:
			INFO("RECORD/STREAM(%d): fe_num %d rec_dmx %d", mode, CFEManager::getInstance()->allocateFE(channel)->getNumber(), channel->getRecordDemux());
			source = channel->getRecordDemux();
			demux = channel->getRecordDemux();
			break;
#if 0
		case STREAM:
			source = DEMUX_SOURCE_0;
			demux = STREAM_DEMUX;
			break;
#endif
		case PIP:
			source = channel->getRecordDemux();
			demux = channel->getPipDemux();
			break;
	}

	oldmask = cam->getCaMask();
	if(force_update)
		newmask = oldmask;
	else
		newmask = cam->makeMask(demux, start);

	if(cam->getSource() > 0)
		source = cam->getSource();

	INFO("channel %" PRIx64 " [%s] mode %d %s src %d mask %d -> %d update %d", channel_id, channel->getName().c_str(),
			mode, start ? "START" : "STOP", source, oldmask, newmask, force_update);
	//INFO("source %d old mask %d new mask %d force update %s", source, oldmask, newmask, force_update ? "yes" : "no");

	/* stop decoding if record stops unless it's the live channel. TODO:PIP? */
	if (mode == RECORD && start == false && source != cDemux::GetSource(0)) {
		INFO("MODE!=record(%d) start=false, src %d getsrc %d", mode, source, cDemux::GetSource(0));
		cam->sendMessage(NULL, 0, false);
		cam->sendCaPmt(channel->getChannelID(), NULL, 0);
	}

	if((oldmask != newmask) || force_update) {
		cam->setCaMask(newmask);
		cam->setSource(source);
		if(newmask == 0) {
			cam->sendMessage(NULL, 0, false);
			cam->sendCaPmt(channel->getChannelID(), NULL, 0);
		} else {
			cam->makeCaPmt(channel, true);
			cam->setCaPmt(true);
		}
	}

	if(newmask == 0) {
		/* FIXME: back to live channel from playback dont parse pmt and call setCaPmt
		 * (see CMD_SB_LOCK / UNLOCK PLAYBACK */
		//channel->setRawPmt(NULL);//FIXME
		channel_map.erase(channel_id);
		delete cam;
	}
	CaIdVector caids;
	cCA::GetInstance()->GetCAIDS(caids);
	//uint8_t list = CCam::CAPMT_FIRST;
	uint8_t list = CCam::CAPMT_ONLY;
	if (channel_map.size() > 1)
		list = CCam::CAPMT_ADD;
	for (it = channel_map.begin(); it != channel_map.end(); /*++it*/)
	{
		cam = it->second;
		channel = CServiceManager::getInstance()->FindChannel(it->first);
		++it;
		if(!channel)
			continue;

#if 0
		if (it == channel_map.end())
			list |= CCam::CAPMT_LAST; // FIRST->ONLY or MORE->LAST
#endif

		cam->makeCaPmt(channel, false, list, caids);
		int len;
		unsigned char * buffer = channel->getRawPmt(len);
		cam->sendCaPmt(channel->getChannelID(), buffer, len);
		//list = CCam::CAPMT_MORE;
	}
	mutex.unlock();

	return true;
}
