/*
 * $Header: /cvs/tuxbox/apps/dvb/zapit/include/zapit/settings.h,v 1.8.2.6 2003/06/14 06:46:22 digi_casi Exp $
 *
 * zapit's settings - d-box2 linux project
 *
 * (C) 2002 by thegoodguy <thegoodguy@berlios.de>
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

#ifndef __zapit__settings_h__
#define __zapit__settings_h__


#ifdef HAVE_CONFIG_H
#include                "config.h"
#else
#define CONFIGDIR       "/var/tuxbox/config"
#endif

#define CONFIGFILE      CONFIGDIR "/zapit/zapit.conf"
#define MOTORCONFIGFILE CONFIGDIR "/zapit/motor.conf"
#define SATCONFIG CONFIGDIR "/zapit/sat.conf"
#define SERVICES_XML    CONFIGDIR "/zapit/services.xml"
#define SERVICES_TMP    "/tmp/services.tmp"
#define BOUQUETS_XML    CONFIGDIR "/zapit/bouquets.xml"
#define UBOUQUETS_XML    CONFIGDIR "/zapit/ubouquets.xml"
#define BOUQUETS_TMP    "/tmp/bouquets.tmp"

#define CURRENTSERVICES_XML     "/tmp/currentservices.xml"
#define CURRENTSERVICES_TMP     "/tmp/currentservices.tmp"

#define MYSERVICES_XML	CONFIGDIR "/zapit/myservices.xml"

#define CABLES_XML      CONFIGDIR "/cables.xml"
#define SATELLITES_XML  CONFIGDIR "/satellites.xml" 
#define TERRESTRIAL_XML CONFIGDIR "/terrestrial.xml"

#if HAVE_TRIPLEDRAGON == 0
#define AUDIO_DEVICE          "/dev/dvb/adapter0/audio0"
#define DEMUX_DEVICE          "/dev/dvb/adapter0/demux0"
#define FRONTEND_DEVICE       "/dev/dvb/adapter0/frontend0"
#define VIDEO_DEVICE          "/dev/dvb/adapter0/video0"
#endif

#define CAMD_UDS_NAME  "/tmp/camd.socket"


#endif /* __zapit__settings_h__ */
