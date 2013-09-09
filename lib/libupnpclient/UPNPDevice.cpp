/***********************************************************************************
 *   UPNPDevice.cpp
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
#include <xmltree.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include "upnpclient.h"
#include <algorithm>
#include <errno.h>
#include <stdio.h>

struct ToLower
{
	char operator() (char c) const { return std::tolower(c); }
};

static void trim(std::string &buf)
{
	std::string::size_type pos1, pos2;

	pos1 = buf.find_first_not_of(" \t\n\r");
	pos2 = buf.find_last_not_of(" \t\n\r");
	if ((pos1 == std::string::npos) || (pos2 == std::string::npos))
		buf="";
	else
		buf = buf.substr(pos1, pos2 - pos1 + 1);
}

bool CUPnPDevice::check_response(std::string header, std::string& charset, std::string& rcode)
{
	std::string::size_type pos;
	std::string content, attrib, chars;

	charset="ISO-8859-1";

	pos = header.find("\r");
	if (pos == std::string::npos)
		return false;

	rcode = header.substr(0, pos);
	if (rcode.substr(0,5) != "HTTP/")
		return false;

	pos = rcode.find(" ");
	if (pos == std::string::npos)
		return false;

	rcode.erase(0, pos);
	trim(rcode);

	pos = rcode.find(" ");
	if (pos != std::string::npos)
	{
		rcode.erase(pos);
		trim(rcode);
	}

	std::transform(header.begin(), header.end(), header.begin(), ToLower());

	pos = header.find("content-type:");
	if (pos == std::string::npos)
		return false;

	content = header.substr(pos);
	content.erase(0,13);
	pos = content.find("\n");
	if (pos != std::string::npos)
		content.erase(pos);

	trim(content);

	pos=content.find(";");
	if (pos!=std::string::npos)
	{
		attrib=content.substr(pos+1);
		content.erase(pos);
		trim(content);
	}
	if (content != "text/xml")
		return false;

	trim(attrib);
	if (attrib.substr(0,8) == "charset=")
	{
		attrib.erase(0,8);
		pos = attrib.find("\"");
		if (pos!=std::string::npos)
		{
			attrib.erase(0,pos+1);
			pos = attrib.find("\"");
			if (pos==std::string::npos)
				return false;
			chars=attrib.substr(0,pos);
		}
		else
			chars=attrib;
		if (!strcasecmp(chars.c_str(), "utf-8"))
			charset = "UTF-8";
	}
	return true;
}

CUPnPDevice::CUPnPDevice(std::string url)
{
	std::string result, head, body, charset, urlbase;
	std::string curl, eurl, name, mimetype, iurl, rcode;
	std::string::size_type pos;
	XMLTreeNode *root, *device, *service, *node, *snode, *icon;
	int width = 0;
	int height = 0;
	int depth = 0;
	bool servicefound = false;

	descurl = url;
	urlbase = url.substr(7);
	pos = urlbase.find("/");
	if (pos != std::string::npos)
		urlbase=url.substr(0,pos+7);
	else
		urlbase=url;

	result = HTTP(url);

	pos = result.find("\r\n\r\n");

	if (pos == std::string::npos)
		throw std::runtime_error(std::string("no desc body"));

	head = result.substr(0,pos);
	body = result.substr(pos+4);

	if (body == "")
		throw std::runtime_error(std::string("desc body empty"));

	if (!check_response(head, charset, rcode))
		throw std::runtime_error(std::string("protocol error"));

	if (rcode != "200")
		throw std::runtime_error(std::string("description url returned ") + rcode);
	XMLTreeParser parser(charset.c_str());
	parser.Parse(body.c_str(), body.size(), 1);
	root = parser.RootNode();
	if (!root)
		throw std::runtime_error(std::string("XML: no root node"));

	if (strcmp(root->GetType(),"root"))
		throw std::runtime_error(std::string("XML: no root"));

	for (node = root->GetChild(); node; node=node->GetNext())
	{
		if (!strcmp(node->GetType(),"URLBase"))
		{
			urlbase = std::string(node->GetData());
			if ((urlbase.length() > 0) && (urlbase[urlbase.length()-1] == '/'))
				urlbase.erase(urlbase.length()-1);
		}

	}

	node = root->GetChild();
	if (!node)
		throw std::runtime_error(std::string("XML: no root child"));

	while (strcmp(node->GetType(),"device"))
	{
		node = node->GetNext();
		if (!node)
			throw std::runtime_error(std::string("XML: no device"));
	}
	device = node;

	for (node=device->GetChild(); node; node=node->GetNext())
	{
		if (!strcmp(node->GetType(),"deviceType"))
			devicetype = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"friendlyName"))
			friendlyname = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"manufacturer"))
			manufacturer = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"manufacturerURL"))
			manufacturerurl = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"modelDescription"))
			modeldescription = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"modelName"))
			modelname = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"modelNumber"))
			modelnumber = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"modelURL"))
			modelurl = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"serialNumber"))
			serialnumber = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"UDN"))
			udn = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"UPC"))
			upc = std::string(node->GetData()?node->GetData():"");

		if (!strcmp(node->GetType(),"iconList"))
		{
			for (icon=node->GetChild(); icon; icon=icon->GetNext())
			{
				bool foundm = false;
				bool foundw = false;
				bool foundh = false;
				bool foundd = false;
				bool foundu = false;

				if (strcmp(icon->GetType(),"icon"))
					throw std::runtime_error(std::string("XML: no icon"));
				for (snode=icon->GetChild(); snode; snode=snode->GetNext())
				{
					if (!strcmp(snode->GetType(),"mimetype"))
					{
						mimetype=std::string(snode->GetData()?snode->GetData():"");
						foundm = true;
					}
					if (!strcmp(snode->GetType(),"width"))
					{
						width=snode->GetData()?atoi(snode->GetData()):0;
						foundw = true;
					}
					if (!strcmp(snode->GetType(),"height"))
					{
						height=snode->GetData()?atoi(snode->GetData()):0;
						foundh = true;
					}
					if (!strcmp(snode->GetType(),"depth"))
					{
						depth=snode->GetData()?atoi(snode->GetData()):0;
						foundd = true;
					}
					if (!strcmp(snode->GetType(),"url"))
					{
						url=std::string(snode->GetData()?snode->GetData():"");
						foundu = true;
					}
				}
				if (!foundm)
					throw std::runtime_error(std::string("XML: icon without mime"));
				if (!foundw)
					throw std::runtime_error(std::string("XML: icon without width"));
				if (!foundh)
					throw std::runtime_error(std::string("XML: icon without height"));
				if (!foundd)
					throw std::runtime_error(std::string("XML: icon without depth"));
				if (!foundu)
					throw std::runtime_error(std::string("XML: icon without url"));
				UPnPIcon e={mimetype, url, width, height, depth};
				icons.push_back(e);
			}
		}
		if (!strcmp(node->GetType(),"serviceList"))
		{
			servicefound = true;
			for (service=node->GetChild(); service; service=service->GetNext())
			{
				bool foundc = false;
				bool founde = false;
				bool foundn = false;

				if (strcmp(service->GetType(),"service"))
					throw std::runtime_error(std::string("XML: no service"));
				for (snode=service->GetChild(); snode; snode=snode->GetNext())
				{
					if (!strcmp(snode->GetType(),"serviceType"))
					{
						name=std::string(snode->GetData()?snode->GetData():"");
						foundn = true;
					}
					if (!strcmp(snode->GetType(),"eventSubURL"))
					{
						char *p;
						p = snode->GetData();
						if (!p)
							eurl=urlbase + "/";
						else if (p[0]=='/')
							eurl=urlbase + std::string(p);
						else
							eurl=urlbase + "/" + std::string(p);
						founde = true;
					}
					if (!strcmp(snode->GetType(),"controlURL"))
					{
						char *p;
						p = snode->GetData();
						if (!p)
							curl=urlbase + "/";
						else if (p[0]=='/')
							curl=urlbase + std::string(p);
						else
							curl=urlbase + "/" + std::string(p);
						foundc = true;
					}
				}
				if (!foundn)
					throw std::runtime_error(std::string("XML: no service type"));
				if (!founde)
					throw std::runtime_error(std::string("XML: no event url"));
				if (!foundc)
					throw std::runtime_error(std::string("XML: no control url"));
				//try
				{
					services.push_back(CUPnPService(this, curl, eurl, name));
				}
				//catch (std::runtime_error error)
				//{
				//	std::cout << "error " << error.what() << "\n";
				//}
			}
		}
	}
	if (!servicefound)
		throw std::runtime_error(std::string("XML: no service list"));
}

CUPnPDevice::~CUPnPDevice()
{
}

std::string CUPnPDevice::HTTP(std::string url, std::string post, std::string action)
{
	std::string portname;
	std::string hostname;
	std::string path;
	int port, t_socket, received;
	std::stringstream command, reply;
	std::string commandstr, line;
	struct sockaddr_in socktcp;
	struct hostent *hp;
	std::string::size_type pos1, pos2;
	char buf[4096];

	if (url.substr(0,7) != "http://")
		return "";

	url.erase(0,7);

	pos1 = url.find(":", 0);
	pos2 = url.find("/", 0);

	if (pos2 == std::string::npos)
		return "";
	if ((pos1 == std::string::npos) || (pos1 > pos2))
	{
		hostname = url.substr(0,pos2);
		port=80;
	}
	else
	{
		hostname = url.substr(0,pos1);
		portname = url.substr(pos1+1, pos2-pos1-1);
		port = atoi(portname.c_str());
	}
	path = url.substr(pos2);

	if (!post.empty())
		command << "POST " << path << " HTTP/1.0\r\n";
	else
		command << "GET " << path << " HTTP/1.0\r\n";

	command << "Host: " << hostname << ":" << port << "\r\n";
	command << "User-Agent: TuxBox\r\n";
	command << "Accept: text/xml\r\n";
	command << "Connection: Close\r\n";

	if (!post.empty())
	{
		command << "Content-Length: " << post.size() << "\r\n";
		command << "Content-Type: text/xml\r\n";
	}

	if (!action.empty())
		command << "SOAPAction: \"" << action << "\"\r\n";

	command << "\r\n";

	if (!post.empty())
		command << post;

	t_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (!t_socket)
		throw std::runtime_error(std::string("create TCP socket"));

	hp = gethostbyname(hostname.c_str());
	if (!hp)
		throw std::runtime_error(std::string("resolve name"));

	memset(&socktcp, 0, sizeof(struct sockaddr_in));
	socktcp.sin_family = AF_INET;
	socktcp.sin_port = htons(port);
	memmove((char *)&socktcp.sin_addr, hp->h_addr, hp->h_length);

	if (connect(t_socket, (struct sockaddr*) &socktcp, sizeof(struct sockaddr_in)))
	{
		close(t_socket);
		throw std::runtime_error(std::string("connect"));
	}

	commandstr = command.str();
	send(t_socket, commandstr.c_str(), commandstr.size(), 0);
#if 0
	while ((received = recv(t_socket, buf, sizeof(buf)-1, 0)) > 0)
	{
		buf[received] = 0;
		reply << buf;
	}
#endif
	struct pollfd fds[1];
	fds[0].fd = t_socket;
	fds[0].events = POLLIN;
	while (true) {
		int result = poll(fds, 1, 4000);
		if (result < 0) {
			printf("CUPnPDevice::HTTP: poll error %s\n", strerror(errno));
			break;
		}
		if (result == 0) {
			printf("CUPnPDevice::HTTP: poll timeout\n");
			break;
		}
		received = recv(t_socket, buf, sizeof(buf)-1, 0);
		if (received <= 0)
			break;
		buf[received] = 0;
		reply << buf;
	}
	close(t_socket);
	return reply.str();
}

std::list<UPnPAttribute> CUPnPDevice::SendSOAP(std::string servicename, std::string action, std::list<UPnPAttribute> attribs)
{
	std::list<CUPnPService>::iterator i;
	std::list<UPnPAttribute> empty;

	for (i=services.begin(); i != services.end(); ++i)
	{
		if (i->servicename == servicename)
			return i->SendSOAP(action, attribs);
	}
	return empty;
}

std::list<std::string> CUPnPDevice::GetServices(void)
{
	std::list<CUPnPService>::iterator i;
	std::list<std::string> result;

	for (i=services.begin(); i != services.end(); ++i)
		result.push_back(i->servicename);
	return result;
}

