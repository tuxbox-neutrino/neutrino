/*
$Id: genpsi.h,v 1.1 2005/08/15 14:47:52 metallica Exp $

 Copyright (c) 2004 gmo18t, Germany. All rights reserved.

 aktuelle Versionen gibt es hier:
 $Source: /cvs/tuxbox/apps/tuxbox/neutrino/src/driver/genpsi.h,v $

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published
 by the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 675 Mass Ave, Cambridge MA 02139, USA.

 Mit diesem Programm koennen Neutrino TS Streams für das Abspielen unter Enigma gepatched werden 
 */
#ifndef __genpsi_h__
#define __genpsi_h__
#include <inttypes.h>

int genpsi(int fd2);
void transfer_pids(uint16_t pid,uint16_t pidart,short isAC3);

#define EN_TYPE_VIDEO           0x00
#define EN_TYPE_AUDIO           0x01
#define EN_TYPE_TELTEX          0x02
#define EN_TYPE_PCR             0x03
#define EN_TYPE_AVC           0x04

#endif
