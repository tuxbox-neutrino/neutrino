/*
	$port: network_setup.h,v 1.3 2009/11/22 15:36:52 tuxbox-cvs Exp $

	network setup implementation - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

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

#ifndef __network_setup__
#define __network_setup__

#include <gui/widget/menue.h>

#include <system/setting_helpers.h>
#include <system/configure_network.h>

#include <string>


class CNetworkSetup : public CMenuTarget, CChangeObserver
{
	private:
 		CNetworkConfig  *networkConfig;
						
		int width;
		
		bool is_wizard;

		int network_dhcp;
		int network_automatic_start;
		std::string network_address;
		std::string network_netmask;
		std::string network_broadcast;
		std::string network_nameserver;
		std::string network_gateway;
		std::string network_hostname;
		std::string network_ssid;
		std::string network_key;
		std::string mac_addr;

		int old_network_dhcp;
		int old_network_automatic_start;
		std::string old_network_address;
		std::string old_network_netmask;
		std::string old_network_broadcast;
		std::string old_network_nameserver;
		std::string old_network_gateway;
		std::string old_network_hostname;
		std::string old_ifname;
		std::string old_network_ssid;
		std::string old_network_key;
		std::string old_mac_addr;


		CMenuForwarder* dhcpDisable[5];
		CMenuForwarder* wlanEnable[2];

		CSectionsdConfigNotifier* sectionsdConfigNotifier;
			
		void restoreNetworkSettings();
		void prepareSettings();
		void readNetworkSettings();
		void backupNetworkSettings();
		int showNetworkSetup();
		void showNetworkNTPSetup(CMenuWidget *menu_ntp);
		void showNetworkNFSMounts(CMenuWidget *menu_nfs);
		int saveChangesDialog();
		void applyNetworkSettings();
		void saveNetworkSettings();
		
		bool checkIntSettings();
		bool checkStringSettings();
		bool checkForIP();
		bool settingsChanged();
		const char * mypinghost(const char * const host);
				
	public:	
		enum NETWORK_DHCP_MODE
		{
			NETWORK_DHCP_OFF =  0, //static
			NETWORK_DHCP_ON  =  1
		};

		enum NETWORK_START_MODE
		{
			NETWORK_AUTOSTART_OFF =  0,
			NETWORK_AUTOSTART_ON  =  1
		};

		enum NETWORK_NTP_MODE
		{
			NETWORK_NTP_OFF =  0,
			NETWORK_NTP_ON  =  1
		};
		
		enum NETWORK_SETUP_MODE
		{
			N_SETUP_MODE_WIZARD_NO   = 0,
			N_SETUP_MODE_WIZARD   = 1
		};

		CNetworkSetup(bool wizard_mode = N_SETUP_MODE_WIZARD_NO);
		~CNetworkSetup();
		
		static CNetworkSetup* getInstance();
		
		bool getWizardMode() {return is_wizard;};
		void setWizardMode(bool mode);
		
		int exec(CMenuTarget* parent, const std::string & actionKey);
 		virtual bool changeNotify(const neutrino_locale_t, void * Data);
		void showCurrentNetworkSettings();
		void testNetworkSettings();
};


#endif
