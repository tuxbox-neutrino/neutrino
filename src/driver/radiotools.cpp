/*
 * radiotools.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * This is a "plugin" for the Video Disk Recorder (VDR).
 *
 * Written by:                  Lars Tegeler <email@host.dom>
 *
 * Project's homepage:          www.math.uni-paderborn.de/~tegeler/vdr
 *
 * Latest version available at: URL
 *
 * See the file COPYING for license information.
 *
 * Description:
 *
 * This Plugin display an background image while the vdr is switcht to radio channels.
 *
 * $Id: radiotools.cpp,v 1.1 2009/08/07 07:22:31 rhabarber1848 Exp $
 
*/

#include "radiotools.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>

/* for timetest */
//#include <time.h>
//#include <sys/time.h>
unsigned short crc16_ccitt(unsigned char *daten, int len, bool skipfirst)
{
/* timetest */
//struct timeval t;
//unsigned long long tstart = 0;
//if (gettimeofday(&t, NULL) == 0)
//    tstart = t.tv_sec*1000000 + t.tv_usec;

	// CRC16-CCITT: x^16 + x^12 + x^5 + 1
	// with start 0xffff and result invers
	register unsigned short crc = 0xffff;

	if (skipfirst) daten++;
	while (len--) {
		crc = (crc >> 8) | (crc << 8);
		crc ^= *daten++;
		crc ^= (crc & 0xff) >> 4;
		crc ^= (crc << 8) << 4;
		crc ^= ((crc & 0xff) << 4) << 1;
	}

/* timetest */
//if (tstart > 0 && gettimeofday(&t, NULL) == 0)
//    printf("vdr-radio: crc-calctime = %d usec\n", (int)((t.tv_sec*1000000 + t.tv_usec) - tstart));

	 return ~(crc);
}

char *rtrim(char *text)
{
    char *s = text + strlen(text) - 1;
    while (s >= text && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
    	*s-- = 0;

    return text;
}

// --- cTimeMs ---------------------------------------------------------------

cTimeMs::cTimeMs(void)
{
	Set();
}

#if 0
uint64_t cTimeMs::Now(void)
{
	struct timeval t;
	if (gettimeofday(&t, NULL) == 0)
		return (uint64_t(t.tv_sec)) * 1000 + t.tv_usec / 1000;
	return 0;
}

void cTimeMs::Set(int Ms)
{
	begin = Now() + Ms;
}

bool cTimeMs::TimedOut(void)
{
	return Now() >= begin;
}

uint64_t cTimeMs::Elapsed(void)
{
	return Now() - begin;
}

#endif
//--------------- End -----------------------------------------------------------------
