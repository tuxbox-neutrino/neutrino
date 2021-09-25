/*
 * Neutrino-GUI  -   DBoxII-Project
 *
 * Copyright (C) 2011 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
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

#ifndef __zapit_scan_ait_h__
#define __zapit_scan_ait_h__

#include <OpenThreads/Thread>
#include <hardware/dmx.h>

#include <dvbsi++/application_information_section.h>
#include <dvbsi++/application_name_descriptor.h>
#include <dvbsi++/application_profile.h>
#include <dvbsi++/application_descriptor.h>

#define AIT_SECTION_SIZE 4096
#define PACK_VERSION(major,minor,micro) (((major) << 16) + ((minor) << 8) + (micro))
#define UNPACK_VERSION(version,major,minor,micro) { \
        major = (version)&0xff; \
        minor = (version>>8)&0xff; \
        micro = (version>>16)&0xff; \
}

class CAit : public OpenThreads::Thread
{
private:
	int dmxnum;
	unsigned short pid;
	std::string name;
	ApplicationInformationSectionList sections;
	void run();
	bool Read();

public:
	CAit();
	~CAit();
	void setDemux(int dnum = 0);
	bool Start();
	bool Stop();
	bool Parse();
	bool Parse(CZapitChannel * const channel);
};

#endif
