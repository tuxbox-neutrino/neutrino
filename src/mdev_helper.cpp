/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2014 CoolStream International Ltd

        License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

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
#include <stdlib.h>

#include "config.h"
#include <neutrinoMessages.h>
#include <eventserver.h>

#include <string>

const char *mdev_env[] = 
{
	"ACTION" ,
	"MDEV" ,
	"DEVPATH" ,
	"INTERFACE",
};

#define ENV_SIZE (sizeof(mdev_env)/sizeof(char *))

int main (int /*argc*/, char **argv)
{
	struct sockaddr_un servaddr;
	int clilen, sock_fd;
	std::string data;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	strncpy(servaddr.sun_path, NEUTRINO_UDS_NAME, sizeof(servaddr.sun_path) - 1);
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		perror("error open socket " NEUTRINO_UDS_NAME);
		exit(1);
	}

	if (connect(sock_fd, (struct sockaddr*) &servaddr, clilen) < 0)
	{
		perror("error connect socket " NEUTRINO_UDS_NAME);
		goto _error;
	}

	for (unsigned i = 0; i < ENV_SIZE; i++) {
		char * s = getenv(mdev_env[i]);
		if (s)
			data += std::string(mdev_env[i]) + "=" + s + " ";
	}

	if (data.empty()) {
		printf("%s: env data empty\n", argv[0]);
		goto _error;
	}

	CEventServer::eventHead head;

	head.eventID = NeutrinoMessages::EVT_HOTPLUG;
	head.initiatorID = CEventServer::INITID_NEUTRINO;
	head.dataSize = data.size() + 1;

	if (write(sock_fd, &head, sizeof(head)) != sizeof(head)) {
		perror("write event");
		goto _error;
	}

	if (write(sock_fd, data.c_str(), head.dataSize) != (ssize_t) head.dataSize) {
		perror("write data");
		goto _error;
	}
	close(sock_fd);
	exit(0);
_error:
	close(sock_fd);
	exit(1);
}
