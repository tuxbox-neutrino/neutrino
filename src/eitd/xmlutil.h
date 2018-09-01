/*
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de),
 *     2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
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

#ifndef __eitd_xmlutil_h__
#define __eitd_xmlutil_h__

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <xmltree/xmlinterface.h>
#include <zapit/client/zapittools.h>
#include <zapit/types.h>

void *insertEventsfromFile(void * data);
void *insertEventsfromXMLTV(void * data);
bool readEventsFromFile(std::string &epgname, int &ev_count);
bool readEventsFromXMLTV(std::string &epgname, int &ev_count);
bool readEventsFromDir(std::string &epgdir, int &ev_count);
void writeEventsToFile(const char *epgdir);
t_channel_id getepgid(std::string epg_name);

bool readEPGFilter(void);
void readDVBTimeFilter(void);
bool checkEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid);
bool checkBlacklist(t_channel_id channel_id);
bool checkNoDVBTimelist(t_channel_id channel_id);
void addEPGFilter(t_original_network_id onid, t_transport_stream_id tsid, t_service_id sid);
void clearEPGFilter();

#endif /* __sectionsd__debug_h__ */

