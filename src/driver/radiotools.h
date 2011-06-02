/*
 * radiotools.h: A plugin for the Video Disk Recorder
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
 * $Id: radiotools.h,v 1.1 2009/08/07 07:22:31 rhabarber1848 Exp $
 
*/

#ifndef __RADIO_TOOLS_H
#define __RADIO_TOOLS_H


unsigned short crc16_ccitt(unsigned char *daten, int len, bool skipfirst);

char *rtrim(char *text);

typedef long long unsigned int uint64_t;

class cTimeMs {
	private:
		uint64_t begin;

	public:
		cTimeMs(void);
		static uint64_t Now(void);
		void Set(int Ms = 0);
		bool TimedOut(void);
		uint64_t Elapsed(void);
};

#endif //__RADIO_TOOLS_H
