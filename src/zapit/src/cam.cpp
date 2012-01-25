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
#include <zapit/cam.h>
#include <zapit/settings.h> /* CAMD_UDS_NAME         */
#include <messagetools.h>   /* get_length_field_size */
#include <zapit/bouquets.h>
#include <zapit/satconfig.h>
#include <zapit/getservices.h>

CCam::CCam()
{
	camask = 0;
	demuxes[0] = demuxes[1] = demuxes[2] = 0;
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

bool CCam::setCaPmt(CCaPmt * const caPmt, int _demux, int _camask, bool update)
{
	camask = _camask;

	printf("CCam::setCaPmt cam %x source %d camask %d update %s\n", (int) this, _demux, camask, update ? "yes" : "no" );
	if(camask == 0) {
		close_connection();
		return true;
	}
	if (!caPmt)
		return true;

	unsigned int size = caPmt->getLength();
	unsigned char buffer[3 + get_length_field_size(size) + size];
	size_t pos = caPmt->writeToBuffer(buffer, _demux, camask);

	return sendMessage((char *)buffer, pos, update);
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

	mutex.lock();
	if(channel->getCaPmt() == NULL) {
		printf("CCamManager: channel %llx dont have caPmt\n", channel_id);
		mutex.unlock();
		return false;
	}

	sat_iterator_t sit = satellitePositions.find(channel->getSatellitePosition());

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
			source = DEMUX_SOURCE_0;
			demux = RECORD_DEMUX;//FIXME
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

	if((oldmask != newmask) || force_update)
		cam->setCaPmt(channel->getCaPmt(), source, newmask, true);

	if(newmask == 0) {
		/* FIXME: back to live channel from playback dont parse pmt and call setCaPmt
		 * (see CMD_SB_LOCK / UNLOCK PLAYBACK */
		//channel->setCaPmt(NULL);
		channel->setRawPmt(NULL);
		channel_map.erase(channel_id);
		delete cam;
	}
	mutex.unlock();

	return true;
}
