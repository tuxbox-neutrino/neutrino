/*
   Copyright (c) 2003,2004 Harald Maiss
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 

*/

#ifndef _UDPSTREAMPES_H
#define _UDPSTREAMPES_H

typedef struct {
     unsigned char Packet;
     unsigned char Status;
     unsigned char SPktBuf;
     unsigned char Stream;
     unsigned StreamPacket;
} PacketHeaderType;
         

// MTU betraegt standartmaessig 1500 und kann mit "ifconfig" nicht erhoeht 
// werden. Abzueglich 8 Byte UDP-Header und 24/20 Byte IP-Header => 1468
// Im Experiment wurde 1472 festgestellt.
#define DATA_PER_PACKET 1472
#define NET_DATA_PER_PACKET (DATA_PER_PACKET-sizeof(PacketHeaderType))


#define MAX_PID_NUM 9   /* 1 Video + 8 Audio */
#define MAX_SPKT_BUF_NUM 36
#define SPKT_BUF_PACKET_NUM 256  
#define SPKT_BUF_SIZE (SPKT_BUF_PACKET_NUM * DATA_PER_PACKET)


#define AUDIO_BUF_PACKET_NUM 20
#define AV_BUF_FACTOR 7
#define DMX_BUF_FACTOR 5

#define STRING_SIZE 1000

#endif /* _UDPSTREAMPES_H */
