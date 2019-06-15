/***********************************************************************************
 *   UPNPSocket.cpp
 *
 *   Copyright (c) 2007 Jochen Friedrich
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ***********************************************************************************/

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include "upnpclient.h"

CUPnPSocket::CUPnPSocket()
{
	struct sockaddr_in sockudp;
	int opt=1;

	m_socket = socket(PF_INET, SOCK_DGRAM, 0);
	if (!m_socket)
		throw std::runtime_error(std::string("create UDP socket"));

	memset(&sockudp, 0, sizeof(struct sockaddr_in));
	sockudp.sin_family = AF_INET;
	sockudp.sin_addr.s_addr = INADDR_ANY;

	if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
		close(m_socket);
		throw std::runtime_error(std::string("socket option"));
	}

	if (bind(m_socket, (struct sockaddr*) &sockudp, sizeof(struct sockaddr_in)))
	{
		close(m_socket);
		throw std::runtime_error(std::string("bind"));
	}
}

CUPnPSocket::~CUPnPSocket()
{
	close(m_socket);
}

void CUPnPSocket::SetTTL(int ttl)
{
	int result;

	result = setsockopt(m_socket, IPPROTO_IP, IP_TTL, (char *)&ttl, sizeof(ttl));
	if (!result)
		throw std::runtime_error(std::string("set ttl"));
}

std::vector<CUPnPDevice> CUPnPSocket::Discover(std::string service)
{
	struct sockaddr_in sockudp;
	std::stringstream command, reply;
	std::string commandstr, line;
	std::vector<CUPnPDevice> devices;
	int result;
	std::string::size_type pos;
	struct pollfd fds[1];
	char bufr[1536];

	memset(&sockudp, 0, sizeof(struct sockaddr_in));
	sockudp.sin_family = AF_INET;
	sockudp.sin_port = htons(MULTICAST_PORT);
	sockudp.sin_addr.s_addr = inet_addr(MULTICAST_IP);

	command << "M-SEARCH * HTTP/1.1\r\n" <<
	           "HOST: " << MULTICAST_IP << ":" << MULTICAST_PORT << "\r\n" <<
		   "ST: " << service << "\r\n" <<
		   "MAN: \"ssdp:discover\"\r\n" <<
		   "MX: 3\r\n" <<
		   "\r\n";

	commandstr = command.str();
	result = sendto(m_socket, commandstr.c_str(), commandstr.size(), 0, (struct sockaddr*) &sockudp, sizeof(struct sockaddr_in));

	if (result < 0) {
		throw std::runtime_error(std::string(strerror(errno)));
	}
//	result = sendto(m_socket, commandstr.c_str(), commandstr.size(), 0, (struct sockaddr*) &sockudp, sizeof(struct sockaddr_in));

	fds[0].fd = m_socket;
	fds[0].events = POLLIN;

	for(;;)
	{
		result = poll(fds, 1, 4000);
		if (result < 0)
			throw std::runtime_error(std::string("poll"));
		if (result == 0)
			return devices;

		result = recv(m_socket, bufr, sizeof(bufr), 0);
		if (result < 0)
			throw std::runtime_error(std::string("recv"));

		bufr[result]=0;

		reply.clear();
		reply.str(std::string(bufr));

		while (!reply.eof())
		{
			getline(reply, line);
			pos=line.find("\r", 0);
			if (pos!=std::string::npos)
				line.erase(pos);
			std::string location = line.substr(0,9);
			if (!strcasecmp(location.c_str(), "location:"))
			{
				line.erase(0, 9);
				while ((!line.empty()) && ((line[0] == ' ') || (line[0] == '\t')))
					line.erase(0, 1);
				if (line.substr(0,7) == "http://")
				{
					std::vector<CUPnPDevice>::iterator i;
					for (i=devices.begin(); i != devices.end(); ++i)
						if (line == i->descurl)
							goto found;
					try
					{
						devices.push_back(CUPnPDevice(line));
					}
					catch (std::runtime_error& error)
					{
						std::cout << "error " << error.what() << "\n";
					}
				}
				found: ;
			}
		}
	}
}
