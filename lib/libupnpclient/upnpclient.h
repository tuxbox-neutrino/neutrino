/***********************************************************************************
 *   upnpclient.h
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

#ifndef __UPNP_H
#define __UPNP_H

#include <utility>
#include <list>
#include <vector>
#include <string>

#define MULTICAST_PORT 1900
#define MULTICAST_IP   "239.255.255.250"

typedef std::pair<std::string, std::string> UPnPAttribute;

struct UPnPIcon
{
	std::string mimetype;
	std::string url;
	int         width;
	int         heigth;
	int         depth;
};

class CUPnPService;

/******************************************
 * Class: CUPnPDevice
 ******************************************/

class CUPnPDevice
{
  friend class CUPnPService;
  friend class CUPnPSocket;

private:
  std::string  descurl;
  std::list<CUPnPService> services;
  std::string HTTP(std::string url,std::string post="", std::string action="");
  bool check_response(std::string header, std::string& charset, std::string& rcode);
  std::list<UPnPIcon> icons;

public:
  std::string friendlyname;
  std::string devicetype;
  std::string manufacturer;
  std::string manufacturerurl;
  std::string modeldescription;
  std::string modelname;
  std::string modelnumber;
  std::string modelurl;
  std::string serialnumber;
  std::string udn;
  std::string upc;
  std::list<UPnPAttribute> SendSOAP(std::string service, std::string action, std::list<UPnPAttribute> attribs);
  std::list<std::string> GetServices(void);
  std::list<UPnPIcon> GetIcons() const { return icons; };

  CUPnPDevice(std::string url);
  ~CUPnPDevice();
};

/******************************************
 * Class: CUPnPService
 ******************************************/

class CUPnPService
{

public:
  std::string  controlurl;
  std::string  eventurl;
  std::string  servicename;
  CUPnPDevice *device;

  CUPnPService(CUPnPDevice *dev, std::string curl, std::string eurl, std::string service);
  ~CUPnPService();
  std::list<UPnPAttribute> SendSOAP(std::string action, std::list<UPnPAttribute> attribs);
};

/******************************************
 * Class: CUPnPSocket
 ******************************************/

class CUPnPSocket
{

public:
  CUPnPSocket();
  ~CUPnPSocket();

  void SetTTL(int ttl);
  std::vector<CUPnPDevice> Discover(std::string service);

private:
  int m_socket;

};

#endif
