/*

        $Header: /cvs/tuxbox/apps/misc/libs/libeventserver/eventserver.cpp,v 1.12 2003/03/14 06:25:49 obi Exp $

	Event-Server  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "eventserver.h"

void CEventServer::registerEvent2(const unsigned int eventID, const unsigned int ClientID, const std::string &udsName)
{
	strcpy(eventData[eventID][ClientID].udsName, udsName.c_str());
}

void CEventServer::registerEvent(const int fd)
{
	commandRegisterEvent msg;

	int readresult= read(fd, &msg, sizeof(msg));
	if ( readresult<= 0 )
		perror("[eventserver]: read");
//	printf("[eventserver]: read from %d %x bytes  %d/%d\n", fd, errno, readresult,  sizeof(msg));
//	printf("[eventserver]: registered event (%d) to: %d - %s\n", msg.eventID, msg.clientID, msg.udsName);
	registerEvent2(msg.eventID, msg.clientID, msg.udsName);
}

void CEventServer::unRegisterEvent2(const unsigned int eventID, const unsigned int ClientID)
{
	eventData[eventID].erase( ClientID );
}

void CEventServer::unRegisterEvent(const int fd)
{
	commandUnRegisterEvent msg;
	ssize_t ignored __attribute__((unused)) = read(fd, &msg, sizeof(msg));
	unRegisterEvent2(msg.eventID, msg.clientID);
}

void CEventServer::sendEvent(const unsigned int eventID, const initiators initiatorID, const void* eventbody, const unsigned int eventbodysize)
{
	eventClientMap notifyClients = eventData[eventID];

	for(eventClientMap::iterator pos = notifyClients.begin(); pos != notifyClients.end(); ++pos)
	{
		//allen clients ein event schicken
		eventClient client = pos->second;
		sendEvent2Client(eventID, initiatorID, &client, eventbody, eventbodysize);
	}
}


bool CEventServer::sendEvent2Client(const unsigned int eventID, const initiators initiatorID, const eventClient* ClientData, const void* eventbody, const unsigned int eventbodysize)
{
	struct sockaddr_un servaddr;
	int clilen, sock_fd;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, ClientData->udsName);
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		perror("[eventserver]: socket");
		return false;
	}

	if(connect(sock_fd, (struct sockaddr*) &servaddr, clilen) <0 )
	{
		char errmsg[128];
		snprintf(errmsg, 128, "[eventserver]: connect (%s)", ClientData->udsName);
		perror(errmsg);
		close(sock_fd);
		return false;
	}

	eventHead head;
	head.eventID = eventID;
	head.initiatorID = initiatorID;
	head.dataSize = eventbodysize;
	/*int written = */
	ssize_t ignored __attribute__((unused)) = write(sock_fd, &head, sizeof(head));
//	printf ("[eventserver]: sent 0x%x - following eventbody= %d\n", written, eventbodysize );

	if(eventbodysize!=0)
	{
		/*written = */
		ignored = write(sock_fd, eventbody, eventbodysize);
//		printf ("[eventserver]: eventbody sent 0x%x - peventbody= %x eventbody= %x\n", written, (unsigned)eventbody, *(unsigned*)eventbody );
	}
	close(sock_fd);
	return true;
}
