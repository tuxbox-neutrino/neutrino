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

#include "test_menu.h"

#include <global.h>
#include <neutrino.h>

#include <driver/screen_max.h>
#include <system/debug.h>

#include <cs_api.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <xmlinterface.h>

#include <gui/widget/hintbox.h>
#include <gui/scan.h>
#include <gui/scan_setup.h>
#include <zapit/zapit.h>
#include <zapit/scan.h>
#include <zapit/femanager.h>
#include <gui/widget/messagebox.h>


extern int cs_test_card(int unit, char * str);

#ifdef TEST_MENU
CTestMenu::CTestMenu()
{
	width = w_max (50, 10);
	selected = -1;
	circle = NULL;
	sq = NULL;
	pic= NULL;
	pip = NULL;
	form = NULL;
	txt = NULL;
}

CTestMenu::~CTestMenu()
{
	delete sq;
	delete circle;
	delete pic;
	delete pip;
	delete form;
	delete txt;
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
	else if (actionKey == "22kon1" || actionKey == "22koff1") 
	{
		CScanTs * scanTs = new CScanTs();
		int freq = (actionKey == "22kon1") ? 12000*1000: 11000*1000;
		sprintf(scansettings.TP_freq, "%d", freq);
                strncpy(scansettings.satNameNoDiseqc,
                        CServiceManager::getInstance()->GetSatelliteName(130).c_str(), 50);

		scanTs->exec(NULL, "test");
		delete scanTs;
		return res;
	}
	else if (actionKey == "22kon2" || actionKey == "22koff2") 
	{
		int freq = (actionKey == "22kon2") ? 12000*1000: 11000*1000;
		sprintf(scansettings.TP_freq, "%d", freq);
                strncpy(scansettings.satNameNoDiseqc,
                        CServiceManager::getInstance()->GetSatelliteName(192).c_str(), 50);

		CScanTs * scanTs = new CScanTs();
		scanTs->exec(NULL, "test");
		delete scanTs;
		return res;
	}
	else if (actionKey == "scan1" || actionKey == "scan2") 
	{
		int fnum = actionKey == "scan1" ? 0 : 1;
                strncpy(scansettings.satNameNoDiseqc, actionKey == "scan1" ?
                        CServiceManager::getInstance()->GetSatelliteName(130).c_str() :
                        CServiceManager::getInstance()->GetSatelliteName(192).c_str(), 50);

		CFrontend *frontend = CFEManager::getInstance()->getFE(fnum);
		CServiceScan::getInstance()->SetFrontend(fnum);

		int freq = 12538000;
		sprintf(scansettings.TP_freq, "%d", freq);
		//CFrontend * frontend = CFEManager::getInstance()->getFE(0);
		switch (frontend->getInfo()->type) 
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
		CScanTs * scanTs = new CScanTs();
		scanTs->exec(NULL, "manual");
		delete scanTs;
		return res;
	}
	else if (actionKey == "circle"){
		if (circle == NULL)
			circle = new CComponentsShapeCircle (100, 100, 100, false);

		if (!circle->isPainted())	
			circle->paint();
		else
			circle->hide();			
		return res;
	}
	else if (actionKey == "square"){
		if (sq == NULL)
			sq = new CComponentsShapeSquare (100, 220, 100, 100, false);

		if (!sq->isPainted())
			sq->paint();
		else
			sq->hide();
		return res;
	}
	else if (actionKey == "picture"){
		if (pic == NULL)
			pic = new CComponentsPicture (100, 100, 200, 200, "/share/tuxbox/neutrino/icons/mp3-5.jpg");

		if (!pic->isPainted() && !pic->isPicPainted())
			pic->paint();
		else
			pic->hide();
		return res;
	}
	else if (actionKey == "pip"){
		if (pip == NULL)
			pip = new CComponentsPIP (100, 100, 25);

		if (!pip->isPainted())
			pip->paint();
		else
			pip->hide();
		return res;
	}
	else if (actionKey == "form"){
		if (form == NULL)
			form = new CComponentsForm();
		form->setDimensionsAll(100, 100, 250, 300);
		form->setCaption(NONEXISTANT_LOCALE);
		form->setIcon(NEUTRINO_ICON_INFO);
		
		if (form->isPainted())
			form->hide();
		else
			form->paint();
		return res;
	}
	else if (actionKey == "text"){
		if (txt == NULL)
			txt = new CComponentsText();
		txt->setDimensionsAll(100, 100, 250, 100);
		txt->setText("This is a text for testing textbox", CTextBox::NO_AUTO_LINEBREAK);

		if (txt->isPainted())
			txt->hide();
		else
			txt->paint();
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
	CMenuWidget w_test(rev /*"Test menu"*/, NEUTRINO_ICON_INFO, width);
	w_test.addIntroItems();
	
	//hardware
	CMenuWidget * w_hw = new CMenuWidget("Hardware Test", NEUTRINO_ICON_INFO, width);
	w_test.addItem(new CMenuForwarderNonLocalized(w_hw->getName().c_str(), true, NULL, w_hw));
	showHWTests(w_hw);
	
	//buttons
	w_test.addItem(new CMenuForwarderNonLocalized("Buttons", true, NULL, this, "buttons"));
	
	//components
	CMenuWidget * w_cc = new CMenuWidget("OSD-Components Demo", NEUTRINO_ICON_INFO, width);
	w_test.addItem(new CMenuForwarderNonLocalized(w_cc->getName().c_str(), true, NULL, w_cc));
	showCCTests(w_cc);

	//exit
	w_test.exec(NULL, "");
	selected = w_test.getSelected();
}

void CTestMenu::showCCTests(CMenuWidget *widget)
{
	widget->setSelected(selected);
	widget->addIntroItems();
	widget->addItem(new CMenuForwarderNonLocalized("Circle", true, NULL, this, "circle"));
	widget->addItem(new CMenuForwarderNonLocalized("Square", true, NULL, this, "square"));
	widget->addItem(new CMenuForwarderNonLocalized("Picture", true, NULL, this, "picture"));
	widget->addItem(new CMenuForwarderNonLocalized("PiP", true, NULL, this, "pip"));
	widget->addItem(new CMenuForwarderNonLocalized("Form", true, NULL, this, "form"));
	widget->addItem(new CMenuForwarderNonLocalized("Text", true, NULL, this, "text"));
}

void CTestMenu::showHWTests(CMenuWidget *widget)
{
	widget->setSelected(selected);
	widget->addIntroItems();
	widget->addItem(new CMenuForwarderNonLocalized("VFD", true, NULL, this, "vfd"));
	widget->addItem(new CMenuForwarderNonLocalized("Network", true, NULL, this, "network"));
	widget->addItem(new CMenuForwarderNonLocalized("Smartcard 1", true, NULL, this, "card0"));
	widget->addItem(new CMenuForwarderNonLocalized("Smartcard 2", true, NULL, this, "card1"));
	widget->addItem(new CMenuForwarderNonLocalized("HDD", true, NULL, this, "hdd"));
	
	CFEManager::getInstance()->setMode(CFEManager::FE_MODE_ALONE);
	
	CServiceManager::getInstance()->InitSatPosition(130, NULL, true);
	CServiceManager::getInstance()->InitSatPosition(192, NULL, true);
	
	satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
	satmap[130].configured = 1;
	
	CFrontend * frontend = CFEManager::getInstance()->getFE(0);
	frontend->setSatellites(satmap);
	
	int count = CFEManager::getInstance()->getFrontendCount();
	if (frontend->getInfo()->type == FE_QPSK) {
		widget->addItem(new CMenuForwarderNonLocalized("Tuner 1: Scan 12538000", true, NULL, this, "scan1"));
		widget->addItem(new CMenuForwarderNonLocalized("Tuner 1: 22 Khz ON", true, NULL, this, "22kon1"));
		widget->addItem(new CMenuForwarderNonLocalized("Tuner 1: 22 Khz OFF", true, NULL, this, "22koff1"));
		if(count > 1) {
			satmap = CServiceManager::getInstance()->SatelliteList();
			satmap[192].configured = 1;
			frontend = CFEManager::getInstance()->getFE(1);
			frontend->setSatellites(satmap);
			
			widget->addItem(new CMenuForwarderNonLocalized("Tuner 2: Scan 12538000", true, NULL, this, "scan2"));
			widget->addItem(new CMenuForwarderNonLocalized("Tuner 2: 22 Khz ON", true, NULL, this, "22kon2"));
			widget->addItem(new CMenuForwarderNonLocalized("Tuner 2: 22 Khz OFF", true, NULL, this, "22koff2"));
		}
	}
}
#endif
