/*
	$port: network_setup.cpp,v 1.14 2010/07/01 11:44:19 tuxbox-cvs Exp $

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "gui/network_setup.h"
#include "gui/proxyserver_setup.h"
#include "gui/nfs.h"

#include <gui/widget/icons.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/stringinput_ext.h>
#include <gui/widget/hintbox.h>

#include <global.h>
#include <neutrino.h>
#include <mymenu.h>

#include <driver/screen_max.h>

#include <system/debug.h>


CNetworkSetup::CNetworkSetup(bool wizard_mode)
{
	frameBuffer = CFrameBuffer::getInstance();
	networkConfig = CNetworkConfig::getInstance();
	
	is_wizard = wizard_mode;
	
	width = w_max (30, 10);
	hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	height = hheight+13*mheight+ 10;
	x	= getScreenStartX (width);
	y	= getScreenStartY (height);
	
	readNetworkSettings();
}

CNetworkSetup::~CNetworkSetup()
{
	//delete networkConfig;
}


int CNetworkSetup::exec(CMenuTarget* parent, const std::string &actionKey)
{
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
	{
		parent->hide();
	}
	

	if(actionKey=="networkapply")
	{
		applyNetworkSettings();
		return res;
	}
	else if(actionKey=="networktest")
	{
		printf("[network setup] doing network test...\n");
		testNetworkSettings(	networkConfig->address.c_str(), 
					networkConfig->netmask.c_str(), 
					networkConfig->broadcast.c_str(), 
					networkConfig->gateway.c_str(), 
					networkConfig->nameserver.c_str(), 
					networkConfig->inet_static);
		return res;
	}
	else if(actionKey=="networkshow")
	{
		dprintf(DEBUG_INFO, "show current network settings...\n");
		showCurrentNetworkSettings();
		return res;
	}
	else if(actionKey=="restore")
	{
		int result =  	ShowMsgUTF(LOCALE_MAINSETTINGS_NETWORK, g_Locale->getText(LOCALE_NETWORKMENU_RESET_SETTINGS_NOW), CMessageBox::mbrNo, 
				CMessageBox::mbYes | 
				CMessageBox::mbNo , 
				NEUTRINO_ICON_QUESTION, 
				width);

		if (result == 0) //yes
			restoreNetworkSettings();
	}
	

	printf("[neutrino] init network setup...\n");
	showNetworkSetup();
	
	return res;
}

void CNetworkSetup::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width, height);
}


void CNetworkSetup::readNetworkSettings()
{
	network_automatic_start = networkConfig->automatic_start;
	network_dhcp 		= networkConfig->inet_static ? NETWORK_DHCP_OFF : NETWORK_DHCP_ON;
	
	network_address		= networkConfig->address;
	network_netmask		= networkConfig->netmask;
	network_broadcast	= networkConfig->broadcast;
	network_nameserver	= networkConfig->nameserver;
	network_gateway		= networkConfig->gateway;
	network_hostname	= networkConfig->hostname;

	old_network_automatic_start 	= networkConfig->automatic_start;
	old_network_dhcp 		= networkConfig->inet_static ? NETWORK_DHCP_OFF : NETWORK_DHCP_ON;
	
	old_network_address		= networkConfig->address;
	old_network_netmask		= networkConfig->netmask;
	old_network_broadcast		= networkConfig->broadcast;
	old_network_nameserver		= networkConfig->nameserver;
	old_network_gateway		= networkConfig->gateway;
	old_network_hostname		= networkConfig->hostname;
}

#define OPTIONS_NTPENABLE_OPTION_COUNT 2
const CMenuOptionChooser::keyval OPTIONS_NTPENABLE_OPTIONS[OPTIONS_NTPENABLE_OPTION_COUNT] =
{
	{ CNetworkSetup::NETWORK_NTP_OFF, LOCALE_OPTIONS_NTP_OFF },
	{ CNetworkSetup::NETWORK_NTP_ON, LOCALE_OPTIONS_NTP_ON }
};



void CNetworkSetup::showNetworkSetup()
{
	//menue init
	CMenuWidget* networkSettings = new CMenuWidget(LOCALE_MAINSETTINGS_HEAD, NEUTRINO_ICON_SETTINGS, width);
	networkSettings->setWizardMode(is_wizard);

	//apply button
	CMenuForwarder *m0 = new CMenuForwarder(LOCALE_NETWORKMENU_SETUPNOW, true, NULL, this, "networkapply", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
	
	//eth id
	static CNetAdapter netadapter; 
	std::string eth_id = netadapter.getMacAddr();
	CMenuForwarder *mac = new CMenuForwarderNonLocalized("MAC", false, eth_id.c_str());

	//prepare input entries
	CIPInput * networkSettings_NetworkIP  = new CIPInput(LOCALE_NETWORKMENU_IPADDRESS , network_address   , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2, this);
	CIPInput * networkSettings_NetMask    = new CIPInput(LOCALE_NETWORKMENU_NETMASK   , network_netmask   , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CIPInput * networkSettings_Broadcast  = new CIPInput(LOCALE_NETWORKMENU_BROADCAST , network_broadcast , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CIPInput * networkSettings_Gateway    = new CIPInput(LOCALE_NETWORKMENU_GATEWAY   , network_gateway   , LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CIPInput * networkSettings_NameServer = new CIPInput(LOCALE_NETWORKMENU_NAMESERVER, network_nameserver, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	
	//hostname
	CStringInputSMS * networkSettings_Hostname = new CStringInputSMS(LOCALE_NETWORKMENU_HOSTNAME, &network_hostname, 30, LOCALE_NETWORKMENU_NTPSERVER_HINT1, LOCALE_NETWORKMENU_NTPSERVER_HINT2, "abcdefghijklmnopqrstuvwxyz0123456789-. ");

	//auto start
	CMenuOptionChooser* o1 = new CMenuOptionChooser(LOCALE_NETWORKMENU_SETUPONSTARTUP, &network_automatic_start, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);

	//dhcp
	network_dhcp 	= networkConfig->inet_static ? NETWORK_DHCP_OFF : NETWORK_DHCP_ON;

	CMenuForwarder *m1 = new CMenuForwarder(LOCALE_NETWORKMENU_IPADDRESS , networkConfig->inet_static, network_address   , networkSettings_NetworkIP );
	CMenuForwarder *m2 = new CMenuForwarder(LOCALE_NETWORKMENU_NETMASK   , networkConfig->inet_static, network_netmask   , networkSettings_NetMask   );
	CMenuForwarder *m3 = new CMenuForwarder(LOCALE_NETWORKMENU_BROADCAST , networkConfig->inet_static, network_broadcast , networkSettings_Broadcast );
	CMenuForwarder *m4 = new CMenuForwarder(LOCALE_NETWORKMENU_GATEWAY   , networkConfig->inet_static, network_gateway   , networkSettings_Gateway   );
	CMenuForwarder *m5 = new CMenuForwarder(LOCALE_NETWORKMENU_NAMESERVER, networkConfig->inet_static, network_nameserver, networkSettings_NameServer);
	CMenuForwarder *m8 = new CMenuForwarder(LOCALE_NETWORKMENU_HOSTNAME  , !networkConfig->inet_static, network_hostname , networkSettings_Hostname  );
	
	CDHCPNotifier* dhcpNotifier = new CDHCPNotifier(m1,m2,m3,m4,m5,m8);
	CMenuOptionChooser* o2 = new CMenuOptionChooser(LOCALE_NETWORKMENU_DHCP, &network_dhcp, OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, dhcpNotifier);		
	
	//paint menu items
	networkSettings->addIntroItems(LOCALE_MAINSETTINGS_NETWORK); //intros
	//-------------------------------------------------
	networkSettings->addItem( m0 ); //apply
	networkSettings->addItem(new CMenuForwarder(LOCALE_NETWORKMENU_TEST, true, NULL, this, "networktest", CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN)); //test
	networkSettings->addItem(new CMenuForwarder(LOCALE_NETWORKMENU_SHOW, true, NULL, this, "networkshow", CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP));	//show settings
	networkSettings->addItem(GenericMenuSeparatorLine);
	//-------------------------------------------------
	networkSettings->addItem(o1);	//set on start
	networkSettings->addItem(GenericMenuSeparatorLine);
	//------------------------------------------------
	networkSettings->addItem(mac);	//eth id
	networkSettings->addItem(GenericMenuSeparatorLine);
	//-------------------------------------------------
	networkSettings->addItem(o2);	//dhcp on/off
	networkSettings->addItem( m8);	//hostname
	networkSettings->addItem(GenericMenuSeparatorLine);
	//-------------------------------------------------
	networkSettings->addItem( m1);	//adress
	networkSettings->addItem( m2);	//mask
	networkSettings->addItem( m3);	//broadcast
	networkSettings->addItem(GenericMenuSeparatorLine);
	//------------------------------------------------
	networkSettings->addItem( m4);	//gateway
	networkSettings->addItem( m5);	//nameserver
	networkSettings->addItem(GenericMenuSeparatorLine);
	//------------------------------------------------
	//ntp submenu
	CMenuWidget* ntp = new CMenuWidget(LOCALE_MAINSETTINGS_NETWORK, NEUTRINO_ICON_SETTINGS, width);
	networkSettings->addItem(new CMenuForwarder(LOCALE_NETWORKMENU_NTPTITLE, true, NULL, ntp, NULL, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW));
	showNetworkNTPSetup(ntp);
	
	//nfs mount submenu
	CMenuWidget* networkmounts = new CMenuWidget(LOCALE_MAINSETTINGS_NETWORK, NEUTRINO_ICON_SETTINGS, width);
	networkSettings->addItem(new CMenuForwarder(LOCALE_NETWORKMENU_MOUNT, true, NULL, networkmounts, NULL, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE));
	showNetworkNFSMounts(networkmounts);

	//proxyserver submenu
	networkSettings->addItem(new CMenuForwarder(LOCALE_FLASHUPDATE_PROXYSERVER_SEP, true, NULL, new CProxySetup(LOCALE_MAINSETTINGS_NETWORK), NULL, CRCInput::RC_0, NEUTRINO_ICON_BUTTON_0));

	
	networkSettings->exec(NULL, "");
	networkSettings->hide();
	delete networkSettings;
				
	// Check for changes
	if (settingsChanged())
		saveChangesDialog();

}

void CNetworkSetup::showNetworkNTPSetup(CMenuWidget *menu_ntp)
{
	//prepare ntp input
	CSectionsdConfigNotifier* sectionsdConfigNotifier = new CSectionsdConfigNotifier;
	CStringInputSMS * networkSettings_NtpServer = new CStringInputSMS(LOCALE_NETWORKMENU_NTPSERVER, &g_settings.network_ntpserver, 30, LOCALE_NETWORKMENU_NTPSERVER_HINT1, LOCALE_NETWORKMENU_NTPSERVER_HINT2, "abcdefghijklmnopqrstuvwxyz0123456789-. ", sectionsdConfigNotifier);

	CStringInput * networkSettings_NtpRefresh = new CStringInput(LOCALE_NETWORKMENU_NTPREFRESH, &g_settings.network_ntprefresh, 3,LOCALE_NETWORKMENU_NTPREFRESH_HINT1, LOCALE_NETWORKMENU_NTPREFRESH_HINT2 , "0123456789 ", sectionsdConfigNotifier);

	CMenuOptionChooser *ntp1 = new CMenuOptionChooser(LOCALE_NETWORKMENU_NTPENABLE, &g_settings.network_ntpenable, OPTIONS_NTPENABLE_OPTIONS, OPTIONS_NTPENABLE_OPTION_COUNT, true, sectionsdConfigNotifier);
	CMenuForwarder *ntp2 = new CMenuForwarder( LOCALE_NETWORKMENU_NTPSERVER, true , g_settings.network_ntpserver, networkSettings_NtpServer );
	CMenuForwarder *ntp3 = new CMenuForwarder( LOCALE_NETWORKMENU_NTPREFRESH, true , g_settings.network_ntprefresh, networkSettings_NtpRefresh );
		
	menu_ntp->addIntroItems(LOCALE_NETWORKMENU_NTPTITLE);
	menu_ntp->addItem( ntp1);
	menu_ntp->addItem( ntp2);
	menu_ntp->addItem( ntp3);
}

void CNetworkSetup::showNetworkNFSMounts(CMenuWidget *menu_nfs)
{
	menu_nfs->addIntroItems(LOCALE_NETWORKMENU_MOUNT);
	menu_nfs->addItem(new CMenuForwarder(LOCALE_NFS_MOUNT , true, NULL, new CNFSMountGui(), NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED));
	menu_nfs->addItem(new CMenuForwarder(LOCALE_NFS_UMOUNT, true, NULL, new CNFSUmountGui(), NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
}


typedef struct n_isettings_t
{
	int old_network_setting;
	int network_setting;
}n_isettings_struct_t;

//checks settings changes for int settings, returns true on changes 
bool CNetworkSetup::checkIntSettings()
{
	n_isettings_t n_isettings[]	= 
	{	
		{old_network_automatic_start, 	network_automatic_start}, 
		{old_network_dhcp, 		network_dhcp		}
	};
	for (uint i = 0; i < (sizeof(n_isettings) / sizeof(n_isettings[0])); i++)
		if (n_isettings[i].old_network_setting != n_isettings[i].network_setting)
			return true;
	
	return false;
}


typedef struct n_ssettings_t
{
	std::string old_network_setting;
	std::string network_setting;
}n_ssettings_struct_t;

//checks settings changes for int settings, returns true on changes
bool CNetworkSetup::checkStringSettings()
{ 						
	n_ssettings_t n_ssettings[]	= 
	{
		{old_network_address,	 	network_address		},
		{old_network_netmask, 		network_netmask		},
		{old_network_broadcast, 	network_broadcast	},
		{old_network_gateway, 		network_gateway		},
		{old_network_nameserver, 	network_nameserver	},
		{old_network_hostname, 		network_hostname	}
	};
	for (uint i = 0; i < (sizeof(n_ssettings) / sizeof(n_ssettings[0])); i++)
		if (n_ssettings[i].old_network_setting != n_ssettings[i].network_setting)
			return true; 
		
	return false;
}

//returns true, if any settings were changed 
bool CNetworkSetup::settingsChanged()
{
	if (networkConfig->modified_from_orig() || checkStringSettings() || checkIntSettings())
		return true;
	
	return false;
}

//prepares internal settings before commit
void CNetworkSetup::prepareSettings()
{
	networkConfig->automatic_start 	= (network_automatic_start == 1);
	networkConfig->inet_static 	= (network_dhcp ? false : true);
	networkConfig->address 		= network_address;
	networkConfig->netmask 		= network_netmask;
	networkConfig->broadcast 	= network_broadcast;
	networkConfig->gateway 		= network_gateway;
	networkConfig->nameserver 	= network_nameserver;
	
	readNetworkSettings();
}

typedef struct n_settings_t
{
	neutrino_locale_t addr_name;
	std::string network_settings;
}n_settings_struct_t;

//check for addresses, if dhcp disabled, returns false if any address no definied and shows a message
bool CNetworkSetup::checkForIP()
{
	n_settings_t n_settings[]	=
	{
		{LOCALE_NETWORKMENU_IPADDRESS, 	network_address		},
		{LOCALE_NETWORKMENU_NETMASK, 	network_netmask		},
		{LOCALE_NETWORKMENU_BROADCAST, 	network_broadcast	},
		{LOCALE_NETWORKMENU_GATEWAY, 	network_gateway		},
		{LOCALE_NETWORKMENU_NAMESERVER, network_nameserver	}
	};
	
	if (!network_dhcp)
	{
		for (uint i = 0; i < (sizeof(n_settings) / sizeof(n_settings[0])); i++)
		{
			if (n_settings[i].network_settings.empty()) //no definied setting
			{	
				printf("[network setup] empty address ...\n");
				char msg[64]; 
				snprintf(msg, 64, g_Locale->getText(LOCALE_NETWORKMENU_ERROR_NO_ADDRESS), g_Locale->getText(n_settings[i].addr_name));
				
				int res = ShowMsgUTF(LOCALE_MAINSETTINGS_NETWORK, msg, CMessageBox::mbrOk, CMessageBox::mbOk, NEUTRINO_ICON_ERROR, width);
				return false;
			}
		}
	}
	
	return true;
}

//saves settings without apply, reboot is required 
void CNetworkSetup::saveNetworkSettings()
{
	printf("[network setup] saving current network settings...\n");

	prepareSettings();
	networkConfig->commitConfig();
}

//saves settings and apply
void CNetworkSetup::applyNetworkSettings()
{
	printf("[network setup] apply network settings...\n");

	CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_NETWORKMENU_APPLY_SETTINGS)); // UTF-8
	hintBox->paint();

 	networkConfig->stopNetwork();
	saveNetworkSettings();
 	networkConfig->startNetwork();

	hintBox->hide();
	delete hintBox;
}

//open a message dialog with buttons,
//yes:		applies networksettings and exit network setup
//no:		saves networksettings and exit network setup
void  CNetworkSetup::saveChangesDialog()
{
	// Save the settings after changes, if user wants to!
	int result = 	ShowMsgUTF(LOCALE_MAINSETTINGS_NETWORK, g_Locale->getText(LOCALE_NETWORKMENU_APPLY_SETTINGS_NOW), CMessageBox::mbrYes, 
			CMessageBox::mbYes | 
			CMessageBox::mbNo , 
			NEUTRINO_ICON_QUESTION, 
			width);
			
	//check for missing ip settings
	if (!checkForIP())
		result = CMessageBox::mbrNo; //restore
	
	switch(result)
	{
		case CMessageBox::mbrYes:
			exec(NULL, "networkapply");
			break;
	
		case CMessageBox::mbrNo: //no
			exec(NULL, "restore");
			break;
	}
}

//restores settings 
void CNetworkSetup::restoreNetworkSettings()
{
	network_automatic_start		= old_network_automatic_start;
	network_dhcp			= old_network_dhcp;
	network_address			= old_network_address;
	network_netmask			= old_network_netmask;
	network_broadcast		= old_network_broadcast;
	network_nameserver		= old_network_nameserver;
	network_gateway			= old_network_gateway;

	networkConfig->automatic_start 	= network_automatic_start;
	networkConfig->inet_static 	= (network_dhcp ? false : true);
	networkConfig->address 		= network_address;
	networkConfig->netmask 		= network_netmask;
	networkConfig->broadcast 	= network_broadcast;
	networkConfig->gateway 		= network_gateway;
	networkConfig->nameserver 	= network_nameserver;

	networkConfig->commitConfig();
}

bool CNetworkSetup::changeNotify(const neutrino_locale_t, void * Data)
{
	char ip[16];
	unsigned char _ip[4];
	sscanf((char*) Data, "%hhu.%hhu.%hhu.%hhu", &_ip[0], &_ip[1], &_ip[2], &_ip[3]);

	sprintf(ip, "%hhu.%hhu.%hhu.255", _ip[0], _ip[1], _ip[2]);
	networkConfig->broadcast = ip;
	network_broadcast = networkConfig->broadcast;

	networkConfig->netmask = (_ip[0] == 10) ? "255.0.0.0" : "255.255.255.0";
	network_netmask = networkConfig->netmask;
	
	return true;
}

//sets menu mode to "wizard" or "default"
void CNetworkSetup::setWizardMode(bool mode)
{
	printf("[neutrino network setup] %s set network settings menu to mode %d...\n", __FUNCTION__, mode);
	is_wizard = mode;
}