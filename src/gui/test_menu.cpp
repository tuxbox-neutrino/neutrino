/*
	test menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
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

#include "gui/test_menu.h"

#include <global.h>
#include <neutrino.h>

#include <driver/screen_max.h>
#include <system/debug.h>

#include <cs_api.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <xmlinterface.h>

#include "gui/widget/hintbox.h"
#include "gui/scan.h"
#include "gui/scan_setup.h"
#include <zapit/frontend_c.h>
#include <gui/widget/messagebox.h>

extern int cs_test_card(int unit, char * str);

CTestMenu::CTestMenu()
{
	width = w_max (50, 10);
	selected = -1;
}

CTestMenu::~CTestMenu()
{
}

int CTestMenu::exec(CMenuTarget* parent, const std::string &actionKey)
{
	dprintf(DEBUG_DEBUG, "init test menu\n");
	
	int   res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	printf("CTestMenu::exec: %s\n", actionKey.c_str());
	
	if (actionKey == "vfd") 
	{
		CVFD::getInstance()->Clear();
		int icon = 0x00040000;
		while (icon > 0x2) {
			CVFD::getInstance()->ShowIcon((vfd_icon) icon, true);
			icon /= 2;
		}
		for (int i = 0x01000001; i <= 0x0C000001; i+= 0x01000000) 
		{
			CVFD::getInstance()->ShowIcon((vfd_icon) i, true);
		}
		CVFD::getInstance()->ShowIcon((vfd_icon) 0x09000002, true);
		CVFD::getInstance()->ShowIcon((vfd_icon) 0x0B000002, true);
		char text[255];
		char buf[XML_UTF8_ENCODE_MAX];
		int ch = 0x2588;
		int len = XmlUtf8Encode(ch, buf);

		for (int i = 0; i < 12; i++) 
		{
			memmove(&text[i*len], buf, len);
		}
		text[12*len] = 0;

		CVFD::getInstance()->ShowText(text);
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "VFD test, Press OK to return", CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		CVFD::getInstance()->Clear();
		
		return res;
	}
	else if (actionKey == "network") 
	{
		int fd, ret;
		struct ifreq ifr;
		char * ip = NULL, str[255];
		struct sockaddr_in *addrp=NULL;

		fd = socket(AF_INET, SOCK_DGRAM, 0);

		ifr.ifr_addr.sa_family = AF_INET;
		strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

		ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
		if (ret < 0)
			perror("SIOCGIFHWADDR");

		ret = ioctl(fd, SIOCGIFADDR, &ifr );
		if (ret < 0)
			perror("SIOCGIFADDR");
		else 
		{
			addrp = (struct sockaddr_in *)&(ifr.ifr_addr);
			ip = inet_ntoa(addrp->sin_addr);
		}

		sprintf(str, "MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\nIP: %s",
			(unsigned char)ifr.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr.ifr_hwaddr.sa_data[5], ip == NULL ? "Unknown" : ip);

		close(fd);
		
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		
		return res;
	}
	else if (actionKey == "card0") 
	{
		char str[255];
		int ret = cs_test_card(0, str);
		switch(ret) 
		{
			case 0:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 ATR read failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			case -2:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 reset failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 open failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
		}
		
		return res;
	}
	else if (actionKey == "card1") 
	{
		char str[255];
		int ret = cs_test_card(1, str);
		switch(ret) 
		{
			case 0:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 ATR read failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			case -2:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 reset failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 open failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
		}
		
		return res;
	}
	else if (actionKey == "hdd") 
	{
		char buffer[255];
		FILE *f = fopen("/proc/mounts", "r");
		bool mounted = false;
		if (f != NULL) 
		{
			while (fgets (buffer, 255, f) != NULL) 
			{
				if (strstr(buffer, "/dev/sda1")) 
				{
					mounted = true;
					break;
				}
			}
			fclose(f);
		}
		sprintf(buffer, "HDD: /dev/sda1 is %s", mounted ? "mounted" : "NOT mounted");
		printf("%s\n", buffer);
		
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, buffer, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		
		return res;
	}
	else if (actionKey == "buttons") 
	{
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;
		CHintBox * khintBox = NULL;
		CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, "Press button, or press EXIT to return");
		hintBox->paint();
		while (1) 
		{
			g_RCInput->getMsg(&msg, &data, 100);
			if (msg == CRCInput::RC_home)
				break;

			if (msg != CRCInput::RC_timeout && msg <= CRCInput::RC_MaxRC) 
			{
				char keyname[50];
				sprintf(keyname, "Button [%s] pressed (EXIT to return)", g_RCInput->getKeyName(msg).c_str());
				if (khintBox) 
				{
					delete khintBox;
				}
				khintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, keyname);
				hintBox->hide();
				khintBox->paint();
			}
		}
		if (khintBox)
			delete khintBox;
		
		delete hintBox;
		
		return res;
	}
	else if (actionKey == "22kon" || actionKey == "22koff") 
	{
		CScanTs * scanTs = new CScanTs();

		int freq = (actionKey == "22kon") ? 12000*1000: 11000*1000;

		sprintf(scansettings.TP_freq, "%d", freq);
#if 0 // not needed ?
		switch (CFrontend::getInstance()->getInfo()->type) 
		{
		case FE_QPSK:
			sprintf(scansettings.TP_rate, "%d", tmpI->second.feparams.u.qpsk.symbol_rate);
			scansettings.TP_fec = tmpI->second.feparams.u.qpsk.fec_inner;
			scansettings.TP_pol = tmpI->second.polarization;
			break;
		case FE_QAM:
			sprintf(scansettings.TP_rate, "%d", tmpI->second.feparams.u.qam.symbol_rate);
			scansettings.TP_fec = tmpI->second.feparams.u.qam.fec_inner;
			scansettings.TP_mod = tmpI->second.feparams.u.qam.modulation;
			break;
		}
#endif
		scanTs->exec(NULL, "test");
		delete scanTs;
		
		return res;
	}
	else if (actionKey == "scan") 
	{
		CScanTs * scanTs = new CScanTs();

		int freq = 12538000;
		sprintf(scansettings.TP_freq, "%d", freq);
		switch (CFrontend::getInstance()->getInfo()->type) 
		{
			case FE_QPSK:
				sprintf(scansettings.TP_rate, "%d", 41250*1000);
				scansettings.TP_fec = 1;
				scansettings.TP_pol = 1;
				break;
			case FE_QAM:
#if 0
			sprintf(scansettings.TP_rate, "%d", tmpI->second.feparams.u.qam.symbol_rate);
			scansettings.TP_fec = tmpI->second.feparams.u.qam.fec_inner;
			scansettings.TP_mod = tmpI->second.feparams.u.qam.modulation;
#endif
				break;
			case FE_OFDM:
			case FE_ATSC:
				break;
		}
		scanTs->exec(NULL, "manual");
		delete scanTs;
		
		return res;
	}


	showTestMenu();

	return res;
}

/* shows entries for proxy settings */
void CTestMenu::showTestMenu()
{
	unsigned int system_rev = cs_get_revision();
	
	//init
	char rev[255];
	sprintf(rev, "Test menu, System revision %d %s", system_rev, system_rev == 0 ? "WARNING - INVALID" : "");
	CMenuWidget * TestMenu = new CMenuWidget(rev /*"Test menu"*/);
	TestMenu->setSelected(selected);
	TestMenu->addIntroItems();
	TestMenu->addItem(new CMenuForwarderNonLocalized("VFD", true, NULL, this, "vfd"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Network", true, NULL, this, "network"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Smartcard 1", true, NULL, this, "card0"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Smartcard 2", true, NULL, this, "card1"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("HDD", true, NULL, this, "hdd"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Buttons", true, NULL, this, "buttons"));
	TestMenu->addItem(new CMenuForwarderNonLocalized("Scan 12538000", true, NULL, this, "scan"));
	//TestMenu->addItem(new CMenuForwarderNonLocalized("22 Khz ON", true, NULL, this, "22kon"));
	//TestMenu->addItem(new CMenuForwarderNonLocalized("22 Khz OFF", true, NULL, this, "22koff"));
	
	TestMenu->exec(NULL, "");
	TestMenu->hide();
	selected = TestMenu->getSelected();
	delete TestMenu;
}

