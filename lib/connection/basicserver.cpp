/*
 * $Header: /cvs/tuxbox/apps/misc/libs/libconnection/basicserver.cpp,v 1.11 2004/04/08 08:00:55 thegoodguy Exp $
 *
 * Basic Server Class Class - The Tuxbox Project
 *
 * (C) 2002 - 2003 by thegoodguy <thegoodguy@berlios.de>
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

#include "basicserver.h"
#include "basicsocket.h"

#include <cstdio>
#include <cstdlib>

#include <inttypes.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define RECEIVE_TIMEOUT_IN_SECONDS 60
#define SEND_TIMEOUT_IN_SECONDS 60

bool CBasicServer::receive_data(int fd, void * data, const size_t size)
{
        timeval timeout;
        timeout.tv_sec  = RECEIVE_TIMEOUT_IN_SECONDS;
        timeout.tv_usec = 0;
        return ::receive_data(fd, data, size, timeout);
}

bool CBasicServer::send_data(int fd, const void * data, const size_t size)
{
        timeval timeout;
        timeout.tv_sec  = SEND_TIMEOUT_IN_SECONDS;
        timeout.tv_usec = 0;
        return ::send_data(fd, data, size, timeout);
}

void CBasicServer::delete_string(char * data)
{
	free((void *)data);
}

char * CBasicServer::receive_string(int fd)
{
	uint8_t length;

	if (receive_data(fd, &length, sizeof(length)))
	{
		char * data = (char *)malloc(((size_t)length) + 1);
		if (receive_data(fd, data, length))
		{
			data[length] = 0; /* add terminating 0 */
			return data;
		}
		else
		{
			delete_string(data);
			return NULL;
		}
	}
	return NULL;
}

bool CBasicServer::prepare(const char* socketname)
{
	struct sockaddr_un servaddr;
	int clilen;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, socketname);
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	unlink(socketname);

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		printf("[CBasicServer] socket failed.\n");
		perror(socketname);
		return false;
	}
	if (bind(sock_fd, (struct sockaddr*) &servaddr, clilen) < 0)
	{
		printf("[CBasicServer] bind failed.\n");
		perror(socketname);
		return false;
	}

#define N_connection_requests_queued 128

	if (listen(sock_fd, N_connection_requests_queued) != 0)
	{
		printf("[CBasicServer] listen failed.\n");
		perror(socketname);
		return false;
	}

	name = socketname;

	return true;
}

bool CBasicServer::parse(bool (parse_command)(CBasicMessage::Header &rmsg, int connfd), const CBasicMessage::t_version version)
{
	int conn_fd;
	struct sockaddr_un servaddr;
	int clilen = sizeof(servaddr);
	bool parse_another_command = true;

	CBasicMessage::Header rmsg;
	conn_fd = accept(sock_fd, (struct sockaddr*) &servaddr, (socklen_t*) &clilen);
	memset(&rmsg, 0, sizeof(rmsg));
	ssize_t r = read(conn_fd, &rmsg, sizeof(rmsg));

	if (r && rmsg.version == version)
		parse_another_command = parse_command(rmsg, conn_fd);
	else
		printf("[%s] Command ignored: cmd %x version %d received - server cmd version is %d\n", name.c_str(), rmsg.cmd, rmsg.version, version);

	close(conn_fd);

	return parse_another_command;
}

bool CBasicServer::run(bool (parse_command)(CBasicMessage::Header &rmsg, int connfd), const CBasicMessage::t_version version, bool non_blocking)
{
	if (non_blocking) {
		struct pollfd pfd;

		pfd.fd = sock_fd;
		pfd.events = (POLLIN | POLLPRI);

		if (poll(&pfd, 1, 0) > 0)
			return parse(parse_command, version);
		else
			return true;
	}
	else {
		while(parse(parse_command, version))
		{};

		stop();

		return false;
	}
}

void CBasicServer::stop(void)
{
	close(sock_fd);
        unlink(name.c_str());
}

