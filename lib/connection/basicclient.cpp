/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicclient.cpp,v 1.17 2004/04/08 07:19:00 thegoodguy Exp $
 *
 * Basic Client Class - The Tuxbox Project
 *
 * (C) 2002-2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * License: GPL
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

#include "basicclient.h"
#include "basicmessage.h"
#include "basicsocket.h"

#include <inttypes.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define TIMEOUT_SEC  60
#define TIMEOUT_USEC 0
#define MAX_TIMEOUT_SEC  300
#define MAX_TIMEOUT_USEC 0

CBasicClient::CBasicClient()
{
	sock_fd = -1;
}

bool CBasicClient::open_connection()
{
	close_connection();

	struct sockaddr_un servaddr;
	int clilen;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, getSocketName());              // no length check !!!
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		printf("[CBasicClient] socket failed.\n");
		perror(getSocketName());
		sock_fd = -1;
		return false;
	}

	if (connect(sock_fd, (struct sockaddr*) &servaddr, clilen) < 0)
	{
		printf("[CBasicClient] connect failed.\n");
		perror(getSocketName());
		close_connection();
		return false;
	}
	return true;
}

void CBasicClient::close_connection()
{
	if (sock_fd != -1)
	{
		close(sock_fd);
		sock_fd = -1;
	}
}

bool CBasicClient::send_data(const char* data, const size_t size)
{
	timeval timeout;

	if (sock_fd == -1)
		return false;
	
	timeout.tv_sec  = TIMEOUT_SEC;
	timeout.tv_usec = TIMEOUT_USEC;
	
	if (::send_data(sock_fd, data, size, timeout) == false)
	{
		printf("[CBasicClient] send failed: %s\n", getSocketName());
		close_connection();
		return false;
	}
	return true;
}

bool CBasicClient::send_string(const char* data)
{
	uint8_t send_length;
	size_t length = strlen(data);
	if (length > 255)
	{
		printf("[CBasicClient] string too long - sending only first 255 characters: %s\n", data);
		send_length = 255;
	}
	else
	{
		send_length = length;
	}
	return (send_data((char *)&send_length, sizeof(send_length)) &&
		send_data(data, send_length));
}

bool CBasicClient::receive_data(char* data, const size_t size, bool use_max_timeout)
{
	timeval timeout;

	if (sock_fd == -1)
		return false;

	if (use_max_timeout)
	{
		timeout.tv_sec  = MAX_TIMEOUT_SEC;
		timeout.tv_usec = MAX_TIMEOUT_USEC;
	}
	else
	{
		timeout.tv_sec  = TIMEOUT_SEC;
		timeout.tv_usec = TIMEOUT_USEC;
	}

	if (::receive_data(sock_fd, data, size, timeout) == false)
	{
		printf("[CBasicClient] receive failed: %s\n", getSocketName());
		close_connection();
		return false;
	}
	return true;
}

bool CBasicClient::send(const unsigned char command, const char* data, const unsigned int size)
{
	CBasicMessage::Header msgHead;
	msgHead.version = getVersion();
	msgHead.cmd     = command;

	open_connection(); // if the return value is false, the next send_data call will return false, too

	if (!send_data((char*)&msgHead, sizeof(msgHead)))
	    return false;
	
	if (size != 0)
	    return send_data(data, size);

	return true;
}

