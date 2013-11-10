/*
        Copyright (C) 2013 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _IWSCAN_H_
#define _IWSCAN_H_

#include <vector>
#include <string>

struct wlan_network
{
	std::string ssid;
	int encrypted;
	std::string channel;
	std::string qual;
	wlan_network(): encrypted(0) {}
};

bool get_wlan_list(std::string dev, std::vector<wlan_network> &networks);
#endif
