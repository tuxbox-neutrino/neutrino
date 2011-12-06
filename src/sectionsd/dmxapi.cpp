/*
 * DMX low level functions (sectionsd) - d-box2 linux project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 * (C) 2009-2011 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include <stdio.h>         /* perror      */
#include <string.h>        /* memset      */
#include <sys/ioctl.h>     /* ioctl       */
#include <fcntl.h>         /* open        */
#include <unistd.h>        /* close, read */
#include <arpa/inet.h>     /* htons */
#include <time.h>          /* ctime */
#include <dmxapi.h>

#if HAVE_TRIPLEDRAGON
#include <dmx_td.h>
#else
#include <dmx_cs.h>
#endif

#include "SIutils.hpp"
#include "debug.h"

#ifndef DO_NOT_INCLUDE_STUFF_NOT_NEEDED_FOR_SECTIONSD
bool setfilter(const int fd, const uint16_t pid, const uint8_t filter, const uint8_t mask, const uint32_t  flags)
{
	struct dmx_sct_filter_params flt;

	memset(&flt, 0, sizeof(struct dmx_sct_filter_params));

	flt.pid              = pid;
	flt.filter.filter[0] = filter;
	flt.filter.mask  [0] = mask;
	flt.timeout          = 0;
	flt.flags            = flags;

	if (::ioctl(fd, DMX_SET_FILTER, &flt) == -1)
	{
		perror("[sectionsd] DMX: DMX_SET_FILTER");
		return false;
	}
	return true;
}
#endif

struct SI_section_TOT_header
{
	unsigned char      table_id                 :  8;
	unsigned char      section_syntax_indicator :  1;
	unsigned char      reserved_future_use      :  1;
	unsigned char      reserved1                :  2;
	unsigned short     section_length           : 12;
	UTC_t              UTC_time; /* :40 */
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned char      reserved2                :  4;
	unsigned char      descr_loop_length_hi     :  4;
#else
	unsigned char      descr_loop_length_hi     :  4;
	unsigned char      reserved2                :  4;
#endif
	unsigned short     descr_loop_length_lo     :  8;
}
__attribute__ ((packed)); /* 10 bytes */

struct SI_section_TDT_header
{
	unsigned char      table_id                 :  8;
	unsigned char      section_syntax_indicator :  1;
	unsigned char      reserved_future_use      :  1;
	unsigned char      reserved1                :  2;
	unsigned short     section_length           : 12;
	/*	uint64_t UTC_time                 : 40;*/
	UTC_t              UTC_time;
}
__attribute__ ((packed)); /* 8 bytes */

struct descrLocalTimeOffset
{
	unsigned char      country_code[3];
#if __BYTE_ORDER == __BIG_ENDIAN
	unsigned char      country_region_id          :  6;
	unsigned char      reserved_1                 :  1;
	unsigned char      local_time_offset_polarity :  1;
#else
	unsigned char      local_time_offset_polarity :  1;
	unsigned char      reserved_1                 :  1;
	unsigned char      country_region_id          :  6;
#endif
	unsigned int       local_time_offset          : 16;
	unsigned int       time_of_change_MJD         : 16;
	unsigned int       time_of_change_UTC         : 24;
	unsigned int       next_time_offset           : 16;
} __attribute__ ((packed)); /* 13 bytes */;


cDemux * dmxUTC = NULL;
bool getUTC(UTC_t * const UTC, const bool TDT)
{
	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];
	int timeout;
	struct SI_section_TOT_header tdt_tot_header;
	char cUTC[5];
	bool ret = true;

	unsigned char buf[1023+3];

	if(dmxUTC == NULL) {
		dmxUTC = new cDemux();
		dmxUTC->Open(DMX_PSI_CHANNEL);
	}

	memset(&filter, 0, DMX_FILTER_SIZE);
	memset(&mask,   0, DMX_FILTER_SIZE);

	filter[0] = TDT ? 0x70 : 0x73;
	mask  [0] = 0xFF;
	timeout   = 31000;
//	flags     = TDT ? (DMX_ONESHOT | DMX_IMMEDIATE_START) : (DMX_ONESHOT | DMX_CHECK_CRC | DMX_IMMEDIATE_START);

	dmxUTC->sectionFilter(0x0014, filter, mask, 5, timeout);

	int size = TDT ? sizeof(struct SI_section_TDT_header) : sizeof(tdt_tot_header);

#if HAVE_COOL_HARDWARE
	int state;
	/* this is stupid, but libcoolstream does not like pthread_cancel */
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &state);
#endif
	int r = dmxUTC->Read(buf, TDT ? size : sizeof(buf));
#if HAVE_COOL_HARDWARE
	pthread_setcancelstate(state, NULL);
#endif
	if (r < size) {
		if (TDT || sections_debug) /* not having TOT is common, no need to log */
			perror("[sectionsd] getUTC: read");
		dmxUTC->Stop();
		return false;
	}
	memset(&tdt_tot_header, 0, sizeof(tdt_tot_header));
	memmove(&tdt_tot_header, buf, size);

	int64_t tmp = tdt_tot_header.UTC_time.time;
	memmove(cUTC, (&tdt_tot_header.UTC_time), 5);
	if ((cUTC[2] > 0x23) || (cUTC[3] > 0x59) || (cUTC[4] > 0x59)) // no valid time
	{
		printf("[sectionsd] getUTC: invalid %s section received: %02x %02x %02x %02x %02x\n",
		       TDT ? "TDT" : "TOT", cUTC[0], cUTC[1], cUTC[2], cUTC[3], cUTC[4]);
		ret = false;
	}

	(*UTC).time = tmp;

#if 1
	short loop_length = tdt_tot_header.descr_loop_length_hi << 8 | tdt_tot_header.descr_loop_length_lo;
	if (loop_length >= 15) {
		int off = sizeof(tdt_tot_header);
		int rem = loop_length;
		while (rem >= 15)
		{
			unsigned char *b2 = &buf[off];
			if (b2[0] == 0x58) {
				struct descrLocalTimeOffset *to;
				to = (struct descrLocalTimeOffset *)&b2[2];
				unsigned char cc[4];
				cc[3] = 0;
				memmove(cc, to->country_code, 3);
				time_t t = changeUTCtoCtime(&b2[2+6],0);
				xprintf("getUTC(TOT): len=%d cc=%s reg_id=%d "
					"pol=%d offs=%04x new=%04x when=%s",
					b2[1], cc, to->country_region_id,
					to->local_time_offset_polarity, htons(to->local_time_offset),
					htons(to->next_time_offset), ctime(&t));
			} else {
				xprintf("getUTC(TOT): descriptor != 0x58: 0x%02x\n", b2[0]);
			}
			off += b2[1] + 2;
			rem -= b2[1] + 2;
			if (off + rem > (int)sizeof(buf))
			{
				xprintf("getUTC(TOT): not enough buffer space? (%d/%d)\n", off+rem, sizeof(buf));
				break;
			}
		}
	}
#endif

	/* TOT without descriptors seems to be not better than a plain TDT, such TOT's are */
	/* found on transponders which also have wrong time in TDT etc, so don't trust it. */
	if (loop_length < 15 && !TDT)
		ret = false;

	//delete dmxUTC;
	dmxUTC->Stop();

	return ret;
}
