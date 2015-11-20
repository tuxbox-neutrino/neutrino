/***********************************************************************************
 *   UPNPService.cpp
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <xmlinterface.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <poll.h>
#include "upnpclient.h"

CUPnPService::CUPnPService(CUPnPDevice* dev, std::string curl, std::string eurl, std::string name)
{
	device = dev;
	controlurl = curl;
	eventurl = eurl;
	servicename = name;
}

CUPnPService::~CUPnPService()
{
}

std::list<UPnPAttribute> CUPnPService::SendSOAP(std::string action, std::list<UPnPAttribute> attribs)
{
	std::stringstream post;
	std::string soapaction;
	std::string result, head, body, charset, envelope, soapbody, soapresponse, rcode;
	std::list<UPnPAttribute>::iterator i;
	std::list<UPnPAttribute> results;
	xmlNodePtr root, node, snode;
	std::string::size_type pos;

	post << "<?xml version=\"1.0\"?>\r\n"
	        "<SOAP-ENV:Envelope "
		"xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<SOAP-ENV:Body>";
		post << "<m:" << action << " xmlns:m=\"" << servicename << "\"";

	if (attribs.empty())
	{
		post << "/>";
	} else {
		post << ">";
		for (i = attribs.begin(); i != attribs.end(); ++i)
		{
			if (i->second.empty())
				post << "<" << i->first << "/>";
			else
			{
				post << "<" << i->first << ">";
				post << i->second;
				post << "</" << i->first << ">";
			}
		}
		post << "</m:" << action << ">";
	}
	post << "</SOAP-ENV:Body></SOAP-ENV:Envelope>\r\n";
	soapaction = servicename + "#" + action;
	result = device->HTTP(controlurl, post.str(), soapaction);

	pos = result.find("\r\n\r\n");

	if (pos == std::string::npos)
		throw std::runtime_error(std::string("no soap body"));

	head = result.substr(0,pos);
	body = result.substr(pos+4);

	if (!device->check_response(head, charset, rcode))
		throw std::runtime_error(std::string("protocol error"));

	xmlDocPtr parser = parseXml(body.c_str(),charset.c_str());
	if(!parser)
		throw std::runtime_error(std::string("XML: parser error"));

	root = xmlDocGetRootElement(parser);
	if (!root)
		throw std::runtime_error(std::string("XML: no root node"));

	envelope=std::string(xmlGetName(root));
	pos = envelope.find(":");
	if (pos !=std::string::npos)
		envelope.erase(0,pos+1);

	if (envelope != "Envelope")
		throw std::runtime_error(std::string("XML: no envelope"));

	node = xmlChildrenNode(root);
	if (!node)
		throw std::runtime_error(std::string("XML: no envelope child"));

	soapbody=std::string(xmlGetName(node));
	pos = soapbody.find(":");
	if (pos !=std::string::npos)
		soapbody.erase(0,pos+1);

	if (soapbody != "Body")
		throw std::runtime_error(std::string("XML: no soap body"));

	node = xmlChildrenNode(node);
	if (!node)
		throw std::runtime_error(std::string("XML: no soap body child"));

	soapresponse=std::string(xmlGetName(node));
	pos = soapresponse.find(":");
	if (pos !=std::string::npos)
		soapresponse.erase(0,pos+1);

	if (rcode != "200")
	{
		std::string faultstring, upnpcode, upnpdesc, errstr;
		if (soapresponse != "Fault")
			throw std::runtime_error(std::string("XML: http error without soap fault: ")+rcode);
		for (node=xmlChildrenNode(node); node; node=xmlNextNode(node))
		{
			if (!strcmp(xmlGetName(node),"detail"))
			{
				snode=xmlChildrenNode(node);
				if (snode)
					for (snode=xmlChildrenNode(snode); snode; snode=xmlNextNode(snode))
					{
						errstr=xmlGetName(snode);
						pos = errstr.find(":");
						if (pos !=std::string::npos)
							errstr.erase(0,pos+1);
						if (errstr=="errorCode")
							upnpcode=std::string(xmlGetData(snode)?xmlGetData(snode):"");
						if (errstr=="errorDescription")
							upnpdesc=std::string(xmlGetData(snode)?xmlGetData(snode):"");
					}

			}
			if (!strcmp(xmlGetName(node),"faultstring"))
				faultstring=std::string(xmlGetData(node)?xmlGetData(node):"");
		}
		if (!faultstring.empty())
			throw std::runtime_error(faultstring + " " + upnpcode + " " + upnpdesc);
		else
			throw std::runtime_error(std::string("XML: http error with unknown soap: ")+rcode);
	}
	if (soapresponse != action + "Response")
		throw std::runtime_error(std::string("XML: no soap response"));

	for (node=xmlChildrenNode(node); node; node=xmlNextNode(node))
		results.push_back(UPnPAttribute(xmlGetName(node), xmlGetData(node)));

	xmlFreeDoc(parser);
	return results;
}
