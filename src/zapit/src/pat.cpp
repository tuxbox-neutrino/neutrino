/*
 * $Id: pat.cpp,v 1.44 2003/01/30 17:21:17 obi Exp $
 *
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
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

#include <zapit/debug.h>
#include <zapit/pat.h>
#include <dmx_cs.h>

#define PAT_SIZE 1024

int parse_pat(CZapitChannel * const channel)
{
	if (!channel)
		return -1;

        cDemux * dmx = new cDemux();
	dmx->Open(DMX_PSI_CHANNEL);

	/* buffer for program association table */
	unsigned char buffer[PAT_SIZE];

	/* current positon in buffer */
	unsigned short i;

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	mask[0] = 0xFF;
	mask[4] = 0xFF;

	do {
		/* set filter for program association section */
		/* read section */
		if ((dmx->sectionFilter(0, filter, mask, 5) < 0) || (i = dmx->Read(buffer, PAT_SIZE) < 0))
		{
			delete dmx;
			printf("[pat.cpp] dmx read failed\n");
			return -1;
		}
if(buffer[7]) printf("[PAT] ***************************************************** section %X last %X\n", buffer[6], buffer[7]);
//printf("[pat.cpp] dmx read %d len %d\n", i, ( (buffer[1] & 0x0F) << 8) | buffer[2]);
		/* loop over service id / program map table pid pairs */
		for (i = 8; i < (((buffer[1] & 0x0F) << 8) | buffer[2]) + 3; i += 4) {
			/* compare service id */
			if (channel->getServiceId() == ((buffer[i] << 8) | buffer[i+1])) {
				/* store program map table pid */
				channel->setPmtPid(((buffer[i+2] & 0x1F) << 8) | buffer[i+3]);
				delete dmx;
				return 0;
			}
		}
	} while (filter[4]++ != buffer[7]);
	delete dmx;

	printf("[pat.cpp] sid %X not found..\n", channel->getServiceId());
	return -1;
}

static unsigned char pbuffer[PAT_SIZE];
int parse_pat()
{
	int ret = 0;

	printf("[scan] Parsing pat ...\n");
	cDemux * dmx = new cDemux();
	dmx->Open(DMX_PSI_CHANNEL);

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	mask[0] = 0xFF;
	mask[4] = 0xFF;

	memset(pbuffer, 0x00, PAT_SIZE);
	if ((dmx->sectionFilter(0, filter, mask, 5) < 0) || (dmx->Read(pbuffer, PAT_SIZE) < 0))
	{
		printf("[pat.cpp] dmx read failed\n");
		ret = -1;
	}
	delete dmx;
	return ret;
}

int pat_get_pmt_pid (CZapitChannel * const channel)
{
	unsigned short i;

	for (i = 8; i < (((pbuffer[1] & 0x0F) << 8) | pbuffer[2]) + 3; i += 4) {
		/* compare service id */
		if (channel->getServiceId() == ((pbuffer[i] << 8) | pbuffer[i+1])) {
			/* store program map table pid */
			channel->setPmtPid(((pbuffer[i+2] & 0x1F) << 8) | pbuffer[i+3]);
			return 0;
		}
	}
	printf("[pat.cpp] sid %X not found..\n", channel->getServiceId());
	return -1;
}

