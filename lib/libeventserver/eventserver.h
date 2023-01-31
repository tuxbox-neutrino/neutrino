/*

        $Header: /cvs/tuxbox/apps/misc/libs/libeventserver/eventserver.h,v 1.14 2004/05/06 15:06:30 thegoodguy Exp $

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

#ifndef __libevent__
#define __libevent__

#include <string>
#include <map>


class CEventServer
{
	public:

		enum initiators
		{
			INITID_CONTROLD,
			INITID_SECTIONSD,
			INITID_ZAPIT,
			INITID_TIMERD,
			INITID_HTTPD,
			INITID_NEUTRINO,
			INITID_GENERIC_INPUT_EVENT_PROVIDER
		};


		struct commandRegisterEvent
		{
			unsigned int eventID;
			unsigned int clientID;
			char udsName[50];
		};

		struct commandUnRegisterEvent
		{
			unsigned int eventID;
			unsigned int clientID;
		};

		struct eventHead
		{
			unsigned int eventID;
			unsigned int initiatorID;
			unsigned int dataSize;
		};

		void registerEvent2(const unsigned int eventID, const unsigned int ClientID, const std::string &udsName);
		void registerEvent(const int fd);
		void unRegisterEvent2(const unsigned int eventID, const unsigned int ClientID);
		void unRegisterEvent(const int fd);
		void sendEvent(const unsigned int eventID, const initiators initiatorID, const void *eventbody = NULL, const unsigned int eventbodysize = 0);

	protected:

		struct eventClient
		{
			char udsName[50];
		};

		//key: ClientID                                              // Map is a Sorted Associative Container
		typedef std::map<unsigned int, eventClient> eventClientMap;  // -> clients with lower ClientID receive events first

		//key: eventID
		std::map<unsigned int, eventClientMap> eventData;

		bool sendEvent2Client(const unsigned int eventID, const initiators initiatorID, const eventClient *ClientData, const void *eventbody = NULL, const unsigned int eventbodysize = 0);

};


#endif
