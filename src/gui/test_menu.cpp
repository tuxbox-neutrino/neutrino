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
#include <neutrino_menue.h>

#include <driver/screen_max.h>
#include <driver/display.h>
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


#if HAVE_COOL_HARDWARE
extern int cs_test_card(int unit, char * str);
#endif

#ifdef TEST_MENU
CTestMenu::CTestMenu()
{
	width = w_max (50, 10);
	circle = NULL;
	sq = NULL;
	pic= NULL;
	form = NULL;
	txt = NULL;
	header = NULL;
	footer = NULL;
	iconform = NULL;
	window = NULL;
	button = NULL;
	clock = clock_r = NULL;
}

CTestMenu::~CTestMenu()
{
	delete sq;
	delete circle;
	delete pic;
	delete form;
	delete txt;
	delete header;
	delete footer;
	delete iconform;
	delete window;
	delete button;
	delete clock;
	delete clock_r;
}

static int test_pos[4] = { 130, 192, 282, 360 };

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
			CVFD::getInstance()->ShowIcon((fp_icon) icon, true);
			icon /= 2;
		}
		for (int i = 0x01000001; i <= 0x0C000001; i+= 0x01000000)
		{
			CVFD::getInstance()->ShowIcon((fp_icon) i, true);
		}
		CVFD::getInstance()->ShowIcon((fp_icon) 0x09000002, true);
		CVFD::getInstance()->ShowIcon((fp_icon) 0x0B000002, true);
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
#if HAVE_COOL_HARDWARE
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
#endif
	else if (actionKey == "hdd")
	{
		char buffer[255];
		FILE *f = fopen("/proc/mounts", "r");
		bool mounted = false;
		if (f != NULL) {
			while (fgets (buffer, 255, f) != NULL) {
				if (strstr(buffer, "/dev/sda1")) {
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
	else if (actionKey == "mmc")
	{
		char buffer[255];
		FILE *f = fopen("/proc/mounts", "r");
		bool mounted = false;
		if (f != NULL) {
			while (fgets (buffer, 255, f) != NULL) {
				if (strstr(buffer, "/dev/mmcblk0p1")) {
					mounted = true;
					break;
				}
			}
			fclose(f);
		}
		sprintf(buffer, "MMC: /dev/mmcblk0p1 is %s", mounted ? "mounted" : "NOT mounted");
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
	else if (actionKey.find("22kon") != std::string::npos)
	{
		int fnum = atoi(actionKey.substr(5, 1).c_str());
		printf("22kon: fe %d sat pos %d\n", fnum, test_pos[fnum]);
		sprintf(scansettings.sat_TP_freq, "%d", 12000*1000);
		strncpy(scansettings.satName,
			CServiceManager::getInstance()->GetSatelliteName(test_pos[fnum]).c_str(), 50);
		CScanTs scanTs(FE_QPSK);
		scanTs.exec(NULL, "test");
		return res;
	}
	else if (actionKey.find("22koff") != std::string::npos)
	{
		int fnum = atoi(actionKey.substr(6, 1).c_str());
		printf("22koff: fe %d sat pos %d\n", fnum, test_pos[fnum]);
		sprintf(scansettings.sat_TP_freq, "%d", 11000*1000);
		strncpy(scansettings.satName,
			CServiceManager::getInstance()->GetSatelliteName(test_pos[fnum]).c_str(), 50);
		CScanTs scanTs(FE_QPSK);
		scanTs.exec(NULL, "test");
		return res;
	}
	else if (actionKey.find("scan") != std::string::npos)
	{
		int fnum = atoi(actionKey.substr(4, 1).c_str());
		printf("scan: fe %d sat pos %d\n", fnum, test_pos[fnum]);

		CFrontend *frontend = CFEManager::getInstance()->getFE(fnum);
		switch (frontend->getInfo()->type) {
			case FE_QPSK:
				strncpy(scansettings.satName,
						CServiceManager::getInstance()->GetSatelliteName(test_pos[fnum]).c_str(), 50);
				sprintf(scansettings.sat_TP_freq, "%d", (fnum & 1) ? 12439000: 12538000);
				sprintf(scansettings.sat_TP_rate, "%d", (fnum & 1) ? 2500*1000 : 41250*1000);
				scansettings.sat_TP_fec = (fnum & 1) ? FEC_3_4 : FEC_1_2;
				scansettings.sat_TP_pol = (fnum & 1) ? 0 : 1;
				break;
			case FE_QAM:
				{
					unsigned count = CFEManager::getInstance()->getFrontendCount();
					for (unsigned i = 0; i < count; i++) {
						CFrontend * fe = CFEManager::getInstance()->getFE(i);
						if (fe->isCable())
							fe->setMode(CFrontend::FE_MODE_UNUSED);
					}
					frontend->setMode(CFrontend::FE_MODE_INDEPENDENT);
					strncpy(scansettings.cableName, "CST Berlin", 50);
					sprintf(scansettings.cable_TP_freq, "%d", 474*1000);
					sprintf(scansettings.cable_TP_rate, "%d", 6875*1000);
					scansettings.cable_TP_fec = 1;
					scansettings.cable_TP_mod = 5;
				}
				break;
			case FE_OFDM:
			case FE_ATSC:
				return res;
		}

		CScanTs scanTs(frontend->getInfo()->type);
		scanTs.exec(NULL, "manual");
		return res;
	}
	else if (actionKey == "button"){
		if (button == NULL)
			button = new CComponentsButtonRed(100, 100, 100, 40, "Test");

		if (!button->isPainted()){
			if (button->isSelected())
				button->setSelected(false);
			else
				button->setSelected(true);
			button->paint();
		}else			
			button->hide();
		
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
			pic = new CComponentsPicture (100, 100, 200, 200, DATADIR "/neutrino/icons/mp3-5.jpg");

		if (!pic->isPainted() && !pic->isPicPainted())
			pic->paint();
		else
			pic->hide();
		return res;
	}
	else if (actionKey == "form"){
		if (form == NULL)
			form = new CComponentsForm();
		form->setColorBody(COL_LIGHT_GRAY);
		form->setDimensionsAll(100, 100, 250, 100);
		form->setFrameThickness(2);
		form->setColorFrame(COL_WHITE);

		CComponentsPicture *ptmp = new CComponentsPicture(0, 0, 0, 0, NEUTRINO_ICON_BUTTON_YELLOW);
		ptmp->setWidth(28);
		ptmp->setHeight(28);
		ptmp->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
		ptmp->setColorBody(COL_BLUE);
 		ptmp->setCornerRadius(RADIUS_MID);
 		ptmp->setCornerType(CORNER_TOP_LEFT);
		form->addCCItem(ptmp);

		CComponentsText *t1 = new CComponentsText(28, 0, 100, 28, "Text1", CTextBox::NO_AUTO_LINEBREAK);
		form->addCCItem(t1);
		
		CComponentsText *t2 = new CComponentsText(t1->getXPos()+t1->getWidth(), 0, 200, 50, "Text2", CTextBox::NO_AUTO_LINEBREAK | CTextBox::RIGHT);
		t2->setCornerRadius(RADIUS_MID);
 		t2->setCornerType(CORNER_TOP_RIGHT);
 		form->addCCItem(t2);

		CComponentsShapeCircle *c1 = new CComponentsShapeCircle(28, 40, 28);
		c1->setColorBody(COL_RED);
		form->addCCItem(c1);

// 		form->removeCCItem(form->getCCItemId(t1));
//  		form->insertCCItem(1,  new CComponentsPicture(28, 0, 0, 0, NEUTRINO_ICON_BUTTON_RED));
		
		
		if (form->isPainted()) {
			form->hide();
			delete form;
			form = NULL;
		}
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
	else if (actionKey == "header"){
		int hh = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
		if (header == NULL){
			header = new CComponentsHeader (100, 50, 500, hh, "Test-Header"/*, NEUTRINO_ICON_INFO, CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU*/);
// 			header->addHeaderButton(NEUTRINO_ICON_BUTTON_RED);
			header->setDefaultButtons(CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU);
		}
// 		else	//For existing instances it's recommended
// 			//to remove old button icons before add new buttons, otherwise icons will be appended.
//  			header->removeHeaderButtons();

//		example to manipulate header items
// 		header->setFrameThickness(5);
// 		header->setColorFrame(COL_WHITE);
// 		header->setCornerType(CORNER_TOP);

//		change text of header
		header->setCaption("Test");

//		add any other button icon
//   		header->addButton(NEUTRINO_ICON_BUTTON_BLUE);
// 		header->addButton(NEUTRINO_ICON_BUTTON_GREEN);

//		example to replace the text item with an image item
//		get text x position
// 		int logo_x = header->getCCItem(CComponentsHeader::CC_HEADER_ITEM_TEXT)->getXPos();
//		remove text item
// 		header->removeCCItem(CComponentsHeader::CC_HEADER_ITEM_TEXT); //then remove text item
//		create picture object with the last x position of text
// 		CComponentsPicture *logo  = new CComponentsPicture(logo_x, 0, 100, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight(), "/share/tuxbox/neutrino/icons/hint_tvmode.png");
//		set the transparent background for picture item
// 		logo->doPaintBg(false);
//		insert the ne object
// 		header->insertCCItem(1, logo); //replace text with logo

		
		if (!header->isPainted())
			header->paint();
		else
			header->hide();
		return res;
	}
	else if (actionKey == "footer"){
		int hh = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
		if (footer == NULL){
			footer = new CComponentsFooter (100, 50, 500, hh, CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU, true);
			int start = 5, btnw =90, btnh = 37;
			footer->addCCItem(new CComponentsButtonRed(start, 0, btnw, btnh, "Button1"));
			footer->addCCItem(new CComponentsButtonGreen(start+=btnw, 0, btnw, btnh, "Button2"));
			footer->addCCItem(new CComponentsButtonYellow(start+=btnw, 0, btnw, btnh, "Button3"));
			footer->addCCItem(new CComponentsButtonBlue(start+=btnw, 0, btnw, btnh, "Button4"));
		}
		if (!footer->isPainted())
			footer->paint();
		else
			footer->hide();
		return res;
	}
	else if (actionKey == "iconform"){
		if (iconform == NULL)
			iconform = new CComponentsIconForm();
		iconform->setColorBody(COL_LIGHT_GRAY);
		iconform->setDimensionsAll(100, 100, 480, 60);
		iconform->setFrameThickness(2);
		iconform->setColorFrame(COL_WHITE);
		iconform->setIconOffset(5);
		iconform->setIconAlign(CComponentsIconForm::CC_ICONS_FRM_ALIGN_RIGHT);
		//For existing instances it's recommended
		//to remove old items before add new icons, otherwise icons will be appended.
		iconform->removeAllIcons();

		//you can...
		//add icons step by step
		iconform->addIcon(NEUTRINO_ICON_INFO);
		iconform->addIcon(NEUTRINO_ICON_INFO);
		iconform->addIcon(NEUTRINO_ICON_HINT_MEDIA);
		//...or
		//add icons with vector
		std::vector<std::string> v_icons;
		v_icons.push_back(NEUTRINO_ICON_HINT_VIDEO);
		v_icons.push_back(NEUTRINO_ICON_HINT_AUDIO);
		iconform->addIcon(v_icons);

		//insert any icon, here as first (index = 0...n)
		iconform->insertIcon(0, NEUTRINO_ICON_HINT_APLAY);
// 		iconform->setIconAlign(CComponentsIconForm::CC_ICONS_FRM_ALIGN_RIGHT);
		
		if (iconform->isPainted())
			iconform->hide();
		else{
			iconform->paint();
		}
		return res;
	}
	else if (actionKey == "window"){
		if (window == NULL){
			window = new CComponentsWindow();
			window->setWindowCaption("|.....................|");
			window->setDimensionsAll(50, 50, 1000, 500);
			window->setWindowIcon(NEUTRINO_ICON_INFO);

			CComponentsShapeCircle *c10 = new CComponentsShapeCircle(0, 0, 28);
			CComponentsShapeCircle *c11 = new CComponentsShapeCircle(0, CC_APPEND, 28);
			CComponentsShapeCircle *c12 = new CComponentsShapeCircle(50, 0, 28);
			CComponentsShapeCircle *c13 = new CComponentsShapeCircle(50, CC_APPEND, 28);
			c10->setColorBody(COL_RED);
			c11->setColorBody(COL_GREEN);
			c12->setColorBody(COL_YELLOW);
			c13->setColorBody(COL_BLUE);
			
			window->getBodyObject()->setAppendOffset(0,50);
			window->addWindowItem(c10);
			window->addWindowItem(c11);
			window->addWindowItem(c12);
			window->addWindowItem(c13);
		}
		else{
			window->setWindowIcon(NEUTRINO_ICON_LOCK);
			window->setWindowCaption("Test");
		}


		
		if (!window->isPainted())
			window->paint();
		else
			window->hide();

		return res;
	}
	else if (actionKey == "running_clock"){	
		if (clock_r == NULL){
			clock_r = new CComponentsFrmClock(100, 50, 0, 50, "%H.%M:%S");
			clock_r->setClockFontType(SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME);
			clock_r->setClockIntervall(1);
// 			clock_r->doPaintBg(false);
		}
		
		if (!clock_r->isPainted()){
			if (clock_r->Start())
				return menu_return::RETURN_EXIT_ALL;;
		}
		else {
			if (clock_r->Stop()){
				clock_r->hide();
				delete clock_r;
				clock_r = NULL;
				return menu_return::RETURN_EXIT_ALL;
			}
		}
	}
	else if (actionKey == "clock"){
		if (clock == NULL){
			clock = new CComponentsFrmClock(100, 50, 0, 50, "%H:%M", false);
			clock->setClockFontType(SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME);
		}

		if (!clock->isPainted())
			clock->paint();
		else {
			clock->hide();
			delete clock;
			clock = NULL;
		}
		return res;
	}
	
	
	return showTestMenu();
}

/* shows entries for proxy settings */
int CTestMenu::showTestMenu()
{
	unsigned int system_rev = cs_get_revision();
	
	//init
	char rev[255];
	sprintf(rev, "Test menu, System revision %d %s", system_rev, system_rev == 0 ? "WARNING - INVALID" : "");
	CMenuWidget w_test(rev /*"Test menu"*/, NEUTRINO_ICON_INFO, width, MN_WIDGET_ID_TESTMENU);
	w_test.addIntroItems();
	
	//hardware
	CMenuWidget * w_hw = new CMenuWidget("Hardware Test", NEUTRINO_ICON_INFO, width, MN_WIDGET_ID_TESTMENU_HARDWARE);
	w_test.addItem(new CMenuForwarderNonLocalized(w_hw->getName().c_str(), true, NULL, w_hw));
	showHWTests(w_hw);
	
	//buttons
	w_test.addItem(new CMenuForwarderNonLocalized("Buttons", true, NULL, this, "buttons"));
	
	//components
	CMenuWidget * w_cc = new CMenuWidget("OSD-Components Demo", NEUTRINO_ICON_INFO, width, MN_WIDGET_ID_TESTMENU_COMPONENTS);
	w_test.addItem(new CMenuForwarderNonLocalized(w_cc->getName().c_str(), true, NULL, w_cc));
	showCCTests(w_cc);

	//exit
	return w_test.exec(NULL, "");;
}

void CTestMenu::showCCTests(CMenuWidget *widget)
{
	widget->addIntroItems();
	widget->addItem(new CMenuForwarderNonLocalized("Running Clock", true, NULL, this, "running_clock"));
	widget->addItem(new CMenuForwarderNonLocalized("Clock", true, NULL, this, "clock"));
	widget->addItem(new CMenuForwarderNonLocalized("Button", true, NULL, this, "button"));
	widget->addItem(new CMenuForwarderNonLocalized("Circle", true, NULL, this, "circle"));
	widget->addItem(new CMenuForwarderNonLocalized("Square", true, NULL, this, "square"));
	widget->addItem(new CMenuForwarderNonLocalized("Picture", true, NULL, this, "picture"));
	widget->addItem(new CMenuForwarderNonLocalized("Form", true, NULL, this, "form"));
	widget->addItem(new CMenuForwarderNonLocalized("Text", true, NULL, this, "text"));
	widget->addItem(new CMenuForwarderNonLocalized("Header", true, NULL, this, "header"));
	widget->addItem(new CMenuForwarderNonLocalized("Footer", true, NULL, this, "footer"));
	widget->addItem(new CMenuForwarderNonLocalized("Icon-Form", true, NULL, this, "iconform"));
	widget->addItem(new CMenuForwarderNonLocalized("Window", true, NULL, this, "window"));
}

void CTestMenu::showHWTests(CMenuWidget *widget)
{
	widget->addIntroItems();
	widget->addItem(new CMenuForwarderNonLocalized("VFD", true, NULL, this, "vfd"));
	widget->addItem(new CMenuForwarderNonLocalized("Network", true, NULL, this, "network"));
#if HAVE_COOL_HARDWARE
	widget->addItem(new CMenuForwarderNonLocalized("Smartcard 1", true, NULL, this, "card0"));
	widget->addItem(new CMenuForwarderNonLocalized("Smartcard 2", true, NULL, this, "card1"));
#endif
	widget->addItem(new CMenuForwarderNonLocalized("HDD", true, NULL, this, "hdd"));
	widget->addItem(new CMenuForwarderNonLocalized("SD/MMC", true, NULL, this, "mmc"));

	for (unsigned i = 0; i < sizeof(test_pos)/sizeof(int); i++) {
		CServiceManager::getInstance()->InitSatPosition(test_pos[i], NULL, true);
	}

	unsigned count = CFEManager::getInstance()->getFrontendCount();
	for (unsigned i = 0; i < count; i++) {
		widget->addItem(GenericMenuSeparatorLine);
		CFrontend * frontend = CFEManager::getInstance()->getFE(i);
		char title[100];
		char scan[100];
		sprintf(scan, "scan%d", i);
		if (frontend->getInfo()->type == FE_QPSK) {
			sprintf(title, "Satellite tuner %d: Scan %s", i+1, (i & 1) ? "12439-02500-H-5/6" : "12538-41250-V-1/2");
		} else if (frontend->getInfo()->type == FE_QAM) {
			sprintf(title, "Cable tuner %d: Scan 474-6875-QAM-256", i+1);
		} else
			continue;

		widget->addItem(new CMenuForwarderNonLocalized(title, true, NULL, this, scan));
		if (frontend->getInfo()->type == FE_QPSK) {
			frontend->setMode(CFrontend::FE_MODE_INDEPENDENT);

			satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
			satmap[test_pos[i]].configured = 1;
			frontend->setSatellites(satmap);
			if (i == 0) {
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 1: 22 Khz ON", true, NULL, this, "22kon0"));
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 1: 22 Khz OFF", true, NULL, this, "22koff0"));
			}
			if (i == 1) {
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 2: 22 Khz ON", true, NULL, this, "22kon1"));
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 2: 22 Khz OFF", true, NULL, this, "22koff1"));
			}
			if (i == 2) {
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 3: 22 Khz ON", true, NULL, this, "22kon2"));
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 3: 22 Khz OFF", true, NULL, this, "22koff2"));
			}
			if (i == 3) {
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 4: 22 Khz ON", true, NULL, this, "22kon3"));
				widget->addItem(new CMenuForwarderNonLocalized("Tuner 4: 22 Khz OFF", true, NULL, this, "22koff3"));
			}
		}
	}
	CFEManager::getInstance()->linkFrontends(true);
}
#endif
