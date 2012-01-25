/*
 * $Id: cam.cpp,v 1.33 2004/04/04 20:20:45 obi Exp $
 *
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>,
 *             thegoodguy         <thegoodguy@berlios.de>
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

#include <dvbsi++/program_map_section.h>
#include <dvbsi++/ca_program_map_section.h>

#define DEBUG_CAPMT

CCam::CCam()
{
	camask = 0;
	demuxes[0] = demuxes[1] = demuxes[2] = 0;
	source_demux = -1;
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

bool CCam::setCaPmt(CZapitChannel * channel, int _demux, int _camask, bool update)
{
        int len;
        unsigned char * buffer = channel->getRawPmt(len);

	camask = _camask;
	source_demux = _demux;

	printf("CCam::setCaPmt cam %x source %d camask %d update %s\n", (int) this, _demux, camask, update ? "yes" : "no" );

	if(camask == 0) {
		close_connection();
		return true;
	}
	if(!buffer)
		return false;

	ProgramMapSection pmt(buffer);
	CaProgramMapSection capmt(&pmt, 0x03, 0x01);

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

	unsigned char cabuf[2048];
	int calen = capmt.writeToBuffer(cabuf);
#ifdef DEBUG_CAPMT
	printf("CAPMT: ");
	for(int i = 0; i < calen; i++)
		printf("%02X ", cabuf[i]);
	printf("\n");
#endif
	return sendMessage((char *)cabuf, calen, update);
}

int CCam::makeMask(int demux, bool add)
{
	int mask = 0;

	if(add)
		demuxes[demux]++;
	else if(demuxes[demux] > 0)
		demuxes[demux]--;

	for(int i = 0; i < 3; i++) {
		if(demuxes[i] > 0)
			mask |= 1 << i;
	}
	printf("CCam::MakeMask: demuxes %d:%d:%d old mask %d new mask %d\n", demuxes[0], demuxes[1], demuxes[2], camask, mask);
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
		printf("CCamManager: channel %llx not found\n", channel_id);
		return false;
	}
printf("CCam::SetMode: channel %llx [%s] mode %d %s update %d\n", channel_id, channel->getName().c_str(), mode, start ? "START" : "STOP", force_update);
	mutex.lock();
#if 0
	if(channel->getCaPmt() == NULL) {
		printf("CCamManager: channel %llx dont have caPmt\n", channel_id);
		mutex.unlock();
		return false;
	}
#endif
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

	switch(mode) {
		case PLAY:
			source = DEMUX_SOURCE_0;
			demux = LIVE_DEMUX;
			break;
		case RECORD:
			source = channel->getRecordDemux(); //DEMUX_SOURCE_0;//FIXME
			demux = channel->getRecordDemux(); //RECORD_DEMUX;//FIXME
			break;
		case STREAM:
			source = DEMUX_SOURCE_0;
			demux = STREAM_DEMUX;//FIXME
			break;
	}

	oldmask = cam->getCaMask();
	if(force_update)
		newmask = oldmask;
	else
		newmask = cam->makeMask(demux, start);

	if(cam->getSource() > 0)
		source = cam->getSource();

printf("CCam::SetMode:  source %d old mask %d new mask %d force update %s\n", source, oldmask, newmask, force_update ? "yes" : "no");
	if((oldmask != newmask) || force_update) {
		cam->setCaPmt(channel, source, newmask, true);
	}

	if(newmask == 0) {
		/* FIXME: back to live channel from playback dont parse pmt and call setCaPmt
		 * (see CMD_SB_LOCK / UNLOCK PLAYBACK */
		//channel->setCaPmt(NULL);
		//channel->setRawPmt(NULL);//FIXME
		channel_map.erase(channel_id);
		delete cam;
	}
	mutex.unlock();

	return true;
}
