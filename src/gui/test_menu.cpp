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
#include <gui/buildinfo.h>
#include <gui/widget/buttons.h>
#include <system/helpers.h>

extern int cs_test_card(int unit, char * str);

#define TestButtonsCount 4
const struct button_label TestButtons[/*TestButtonsCount*/] =
{
	{ NEUTRINO_ICON_BUTTON_RED   	, LOCALE_STRINGINPUT_CAPS  	},
	{ NEUTRINO_ICON_BUTTON_GREEN	, LOCALE_STRINGINPUT_CLEAR 	},
	{ NEUTRINO_ICON_BUTTON_YELLOW	, LOCALE_MESSAGEBOX_INFO	},
	{ NEUTRINO_ICON_BUTTON_BLUE	, LOCALE_STRINGINPUT_CLEAR	}
};


CTestMenu::CTestMenu()
{
	width = w_max (50, 10);
	circle = NULL;
	sq = NULL;
	pic = chnl_pic = NULL;
	form = NULL;
	txt = NULL;
	header = NULL;
	footer = NULL;
	iconform = NULL;
	window = NULL;
	button = NULL;
	clock = clock_r = NULL;
	text_ext = NULL;
	scrollbar = NULL;
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
	delete chnl_pic;
	delete text_ext;
	delete scrollbar;
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
		ShowMsg(LOCALE_MESSAGEBOX_INFO, "VFD test, Press OK to return", CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
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
		
		ShowMsg(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		
		return res;
	}
	else if (actionKey == "card0")
	{
		char str[255];
		int ret = cs_test_card(0, str);
		switch(ret)
		{
			case 0:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 ATR read failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			case -2:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 reset failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 open failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
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
				ShowMsg(LOCALE_MESSAGEBOX_INFO, str, CMessageBox::mbrBack, CMessageBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 ATR read failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			case -2:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 reset failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 open failed", CMessageBox::mbrBack, CMessageBox::mbBack, "info");
				break;
		}
		
		return res;
	}
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
		
		ShowMsg(LOCALE_MESSAGEBOX_INFO, buffer, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		
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
		
		ShowMsg(LOCALE_MESSAGEBOX_INFO, buffer, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO);
		
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
		scansettings.sat_TP_freq = "12000000";
		scansettings.satName =  CServiceManager::getInstance()->GetSatelliteName(test_pos[fnum]);
		CScanTs scanTs(ALL_SAT);
		scanTs.exec(NULL, "test");
		return res;
	}
	else if (actionKey.find("22koff") != std::string::npos)
	{
		int fnum = atoi(actionKey.substr(6, 1).c_str());
		printf("22koff: fe %d sat pos %d\n", fnum, test_pos[fnum]);
		scansettings.sat_TP_freq = "11000000";
		scansettings.satName = CServiceManager::getInstance()->GetSatelliteName(test_pos[fnum]);
		CScanTs scanTs(ALL_SAT);
		scanTs.exec(NULL, "test");
		return res;
	}
	else if (actionKey.find("scan") != std::string::npos)
	{
		int fnum = atoi(actionKey.substr(4, 1).c_str());
		printf("scan: fe %d sat pos %d\n", fnum, test_pos[fnum]);
		delivery_system_t delsys = ALL_SAT;

		CFrontend *frontend = CFEManager::getInstance()->getFE(fnum);
		if (frontend->hasSat()) {
			scansettings.satName = CServiceManager::getInstance()->GetSatelliteName(test_pos[fnum]);
			scansettings.sat_TP_freq = to_string((fnum & 1) ? /*12439000*/ 3951000 : 4000000);
			scansettings.sat_TP_rate = to_string((fnum & 1) ? /*2500*1000*/ 9520*1000 : 27500*1000);
			scansettings.sat_TP_fec = FEC_3_4; //(fnum & 1) ? FEC_3_4 : FEC_1_2;
			scansettings.sat_TP_pol = (fnum & 1) ? 1 : 0;
		} else if (frontend->hasCable()) {
			unsigned count = CFEManager::getInstance()->getFrontendCount();
			for (unsigned i = 0; i < count; i++) {
				CFrontend * fe = CFEManager::getInstance()->getFE(i);
				if (fe->hasCable())
					fe->setMode(CFrontend::FE_MODE_UNUSED);
			}
			frontend->setMode(CFrontend::FE_MODE_INDEPENDENT);
			scansettings.cableName     = "CST Berlin";
			scansettings.cable_TP_freq = "474000";
			scansettings.cable_TP_rate = "6875000";
			scansettings.cable_TP_fec  = 1;
			scansettings.cable_TP_mod  = 5;
			delsys = ALL_CABLE;
		} else {
			return res;
		}

		CScanTs scanTs(delsys);
		scanTs.exec(NULL, "manual");
		return res;
	}
	else if (actionKey == "button"){
		if (button == NULL)
			button = new CComponentsButtonRed(100, 100, 100, 50, "Test");

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
			circle = new CComponentsShapeCircle (100, 100, 100, NULL, false);

		if (!circle->isPainted())	
			circle->paint();
		else
			circle->hide();			
		return res;
	}
	else if (actionKey == "square"){
		if (sq == NULL)
			sq = new CComponentsShapeSquare (100, 220, 100, 100, NULL, false);

		if (!sq->isPainted())
			sq->paint();
		else
			sq->hide();
		return res;
	}
	else if (actionKey == "picture"){
		if (pic == NULL)
			pic = new CComponentsPicture (100, 100, 200, 100, DATADIR "/neutrino/icons/mp3-5.jpg");

		if (!pic->isPainted() && !pic->isPicPainted())
			pic->paint();
		else
			pic->hide();
		return res;
	}
	else if (actionKey == "channellogo"){
		if (chnl_pic == NULL)
			chnl_pic = new CComponentsChannelLogo(100, 100, "ProSieben", 0);

		if (!chnl_pic->isPainted() && !chnl_pic->isPicPainted())
			chnl_pic->paint();
		else
			chnl_pic->hide();
		return res;
	}
	else if (actionKey == "form"){
		if (form == NULL)
			form = new CComponentsForm();
		form->setColorBody(COL_LIGHT_GRAY);
		form->setDimensionsAll(100, 100, 250, 100);
		form->setFrameThickness(2);
		form->setColorFrame(COL_WHITE);

		CComponentsPicture *ptmp = new CComponentsPicture(0, 0, NEUTRINO_ICON_BUTTON_YELLOW);
		ptmp->setWidth(28);
		ptmp->setHeight(28);
		ptmp->setColorBody(COL_BLUE);
		ptmp->setCorner(RADIUS_MID, CORNER_TOP_LEFT);
		form->addCCItem(ptmp);

		CComponentsText *t1 = new CComponentsText(28, 0, 100, 28, "Text1", CTextBox::NO_AUTO_LINEBREAK);
		form->addCCItem(t1);
		
		CComponentsText *t2 = new CComponentsText(t1->getXPos()+t1->getWidth(), 0, 200, 50, "Text2", CTextBox::NO_AUTO_LINEBREAK | CTextBox::RIGHT);
		t2->setCorner(RADIUS_MID, CORNER_TOP_RIGHT);
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
	else if (actionKey == "text_ext"){
		if (text_ext == NULL)
			text_ext = new CComponentsExtTextForm();
		text_ext->setDimensionsAll(10, 20, 300, 48);
		text_ext->setLabelAndText("Label", "Text for demo", g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
		text_ext->setFrameThickness(2);
// 		text_ext->setLabelWidthPercent(15/*%*/);
		
		if (text_ext->isPainted())
			text_ext->hide();
		else
			text_ext->paint();
		return res;
	}
	else if (actionKey == "header"){
		int hh = 30;//g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
		if (header == NULL){
			header = new CComponentsHeader (100, 50, 500, hh, "Test-Header"/*, NEUTRINO_ICON_INFO, CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU*/);
			header->addContextButton(NEUTRINO_ICON_BUTTON_RED);
			header->addContextButton(CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU);
		}
		else	//For existing instances it's recommended to remove old button icons before add new buttons,
			//otherwise icons will be appended to already existant icons, alternatively use the setContextButton() methode
 			header->removeContextButtons();

//		example to manipulate header items
// 		header->setFrameThickness(5);
// 		header->setColorFrame(COL_WHITE);
// 		header->setCornerType(CORNER_TOP);

//		change text of header
		header->setCaption("Test");

		//add context buttons via vector
// 		vector<string> v_buttons;
// 		v_buttons.push_back(NEUTRINO_ICON_BUTTON_YELLOW);
// 		v_buttons.push_back(NEUTRINO_ICON_BUTTON_RED);
// 		header->addContextButton(v_buttons);
// 
// //		add any other button icon via string
//   		header->addContextButton(NEUTRINO_ICON_BUTTON_BLUE);
// 		header->addContextButton(NEUTRINO_ICON_BUTTON_GREEN);
// 		header->addContextButton(CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU);

// 		set a single button, this will also remove all existant context button icons from header
//		header->setContextButton(NEUTRINO_ICON_HINT_AUDIO);

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
			footer = new CComponentsFooter (100, 30, 1000, hh, CComponentsFooter::CC_BTN_HELP | CComponentsFooter::CC_BTN_EXIT | CComponentsFooter::CC_BTN_MENU |CComponentsFooter::CC_BTN_MUTE_ZAP_ACTIVE, NULL, true);
			//int start = 5, btnw =90, btnh = 37;
			footer->setButtonFont(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]);
			footer->setIcon(NEUTRINO_ICON_INFO);

			//add button labels with conventional button label struct
			footer->setButtonLabels(TestButtons, TestButtonsCount, 0, footer->getWidth()/6);

			//also possible: use directly button name and as 2nd parameter string or locale as text
//			footer->setButtonLabel(NULL, "Test", 0, 250);

			//also possible: use Button objects
// 			footer->addCCItem(new CComponentsButtonRed(start, 0, btnw, btnh, "Button1"));
// 			footer->addCCItem(new CComponentsButtonGreen(start+=btnw, 0, btnw, btnh, "Button2"));
// 			footer->addCCItem(new CComponentsButtonYellow(start+=btnw, 0, btnw, btnh, "Button3"));
// 			footer->addCCItem(new CComponentsButtonBlue(start+=btnw, 0, btnw, btnh, "Button4"));
		}

		if (!footer->isPainted())
			footer->paint();
		else
			footer->hide();
		return res;
	}
	else if (actionKey == "scrollbar"){
		if (scrollbar == NULL)
			scrollbar = new CComponentsScrollBar(50, 100, 20, 400, 1);
		
		if (scrollbar->isPainted()){
			if (scrollbar->getMarkID() == scrollbar->getSegmentCount()){
				scrollbar->hide();
				scrollbar->setSegmentCount(scrollbar->getSegmentCount()+1);
			}
			else{
				scrollbar->setMarkID(scrollbar->getMarkID()+1);
				scrollbar->paint();
			}
		}
		else
			scrollbar->paint();

		return res;
	}
	else if (actionKey == "iconform"){
		if (iconform == NULL)
			iconform = new CComponentsIconForm();
		iconform->setColorBody(COL_LIGHT_GRAY);
		iconform->setDimensionsAll(100, 100,80/*480*/, 80);
		iconform->setFrameThickness(2);
		iconform->setColorFrame(COL_WHITE);
		iconform->setDirection(CC_DIR_X);
		iconform->setAppendOffset(5, 5);

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
			window->setDimensionsAll(50, 50, 500, 500);
			window->setWindowIcon(NEUTRINO_ICON_INFO);
			window->setShadowOnOff(true);

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

			CComponentsShapeCircle *c14 = new CComponentsShapeCircle(20, 20, 100);
			c14->setColorBody(COL_RED);
			c14->setPageNumber(1);
			window->addWindowItem(c14);
		}
		else{
			window->setWindowIcon(NEUTRINO_ICON_LOCK);
			window->setWindowCaption("Test");
		}
#if 0
		if (!window->isPainted()){
			window->paint(); //if no other page has been defined, 1st page always painted
		}
		else{
#endif			//or paint direct a defined page
			if (window->getCurrentPage() == 1)
				window->paintPage(0);
			else
				window->paintPage(1);
#if 0
		}
#endif
		return res;
	}
	else if (actionKey == "running_clock"){	
		if (clock_r == NULL){
			clock_r = new CComponentsFrmClock(100, 50, 0, 50, "%H.%M:%S", true);
			clock_r->setClockFont(SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME);
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
			clock->setClockFont(SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME);
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
	else if (actionKey == "footer_key"){
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;
		CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, "Footer-Key pressed. Press EXIT to return");
		hintBox->paint();
		while (1)
		{
			g_RCInput->getMsg(&msg, &data, 100);
			if (msg == CRCInput::RC_home)
				break;
		}
		delete hintBox;

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
	w_test.addItem(new CMenuForwarder(w_hw->getName(), true, NULL, w_hw));
	showHWTests(w_hw);
	
	//buttons
	w_test.addItem(new CMenuForwarder("Buttons", true, NULL, this, "buttons"));
	
	//components
	CMenuWidget * w_cc = new CMenuWidget("OSD-Components Demo", NEUTRINO_ICON_INFO, width, MN_WIDGET_ID_TESTMENU_COMPONENTS);
	w_test.addItem(new CMenuForwarder(w_cc->getName(), true, NULL, w_cc));
	showCCTests(w_cc);
	
	//buildinfo
	CMenuForwarder *f_bi = new CMenuForwarder(LOCALE_BUILDINFO_MENU,  true, NULL, new CBuildInfo());
	f_bi->setHint(NEUTRINO_ICON_HINT_IMAGEINFO, LOCALE_MENU_HINT_BUILDINFO);
	w_test.addItem(f_bi);

	//footer buttons
	static const struct button_label footerButtons[2] = {
		{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_COLORCHOOSER_RED	},
		{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_COLORCHOOSER_GREEN }
	};

	w_test.setFooter(footerButtons, 2);
	w_test.addKey(CRCInput::RC_red, this, "footer_key");
	w_test.addKey(CRCInput::RC_green, this, "footer_key");

	//exit
	return w_test.exec(NULL, "");;
}

void CTestMenu::showCCTests(CMenuWidget *widget)
{
	widget->addIntroItems();
	widget->addItem(new CMenuForwarder("Running Clock", true, NULL, this, "running_clock"));
	widget->addItem(new CMenuForwarder("Clock", true, NULL, this, "clock"));
	widget->addItem(new CMenuForwarder("Button", true, NULL, this, "button"));
	widget->addItem(new CMenuForwarder("Circle", true, NULL, this, "circle"));
	widget->addItem(new CMenuForwarder("Square", true, NULL, this, "square"));
	widget->addItem(new CMenuForwarder("Picture", true, NULL, this, "picture"));
	widget->addItem(new CMenuForwarder("Channel-Logo", true, NULL, this, "channellogo"));
	widget->addItem(new CMenuForwarder("Form", true, NULL, this, "form"));
	widget->addItem(new CMenuForwarder("Text", true, NULL, this, "text"));
	widget->addItem(new CMenuForwarder("Header", true, NULL, this, "header"));
	widget->addItem(new CMenuForwarder("Footer", true, NULL, this, "footer"));
	widget->addItem(new CMenuForwarder("Icon-Form", true, NULL, this, "iconform"));
	widget->addItem(new CMenuForwarder("Window", true, NULL, this, "window"));
	widget->addItem(new CMenuForwarder("Text-Extended", true, NULL, this, "text_ext"));
	widget->addItem(new CMenuForwarder("Scrollbar", true, NULL, this, "scrollbar"));
}

void CTestMenu::showHWTests(CMenuWidget *widget)
{
	widget->addIntroItems();
	widget->addItem(new CMenuForwarder("VFD", true, NULL, this, "vfd"));
	widget->addItem(new CMenuForwarder("Network", true, NULL, this, "network"));
	widget->addItem(new CMenuForwarder("Smartcard 1", true, NULL, this, "card0"));
	widget->addItem(new CMenuForwarder("Smartcard 2", true, NULL, this, "card1"));
	widget->addItem(new CMenuForwarder("HDD", true, NULL, this, "hdd"));
	widget->addItem(new CMenuForwarder("SD/MMC", true, NULL, this, "mmc"));

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
		if (frontend->hasSat()) {
			sprintf(title, "Satellite tuner %d: Scan %s", i+1, (i & 1) ? /*"12439-02500-H-5/6"*/"3951-9520-V-3/4" : "4000-27500-V-3/4");
		} else if (frontend->hasCable()) {
			sprintf(title, "Cable tuner %d: Scan 474-6875-QAM-256", i+1);
		} else
			continue;

		widget->addItem(new CMenuForwarder(title, true, NULL, this, scan));
		if (frontend->hasSat()) {
			frontend->setMode(CFrontend::FE_MODE_INDEPENDENT);

			satellite_map_t satmap = CServiceManager::getInstance()->SatelliteList();
			satmap[test_pos[i]].configured = 1;
			satmap[test_pos[i]].lnbOffsetLow = 5150;
			satmap[test_pos[i]].lnbOffsetHigh = 5150;
			frontend->setSatellites(satmap);
			if (i == 0) {
				widget->addItem(new CMenuForwarder("Tuner 1: 22 Khz ON", true, NULL, this, "22kon0"));
				widget->addItem(new CMenuForwarder("Tuner 1: 22 Khz OFF", true, NULL, this, "22koff0"));
			}
			if (i == 1) {
				widget->addItem(new CMenuForwarder("Tuner 2: 22 Khz ON", true, NULL, this, "22kon1"));
				widget->addItem(new CMenuForwarder("Tuner 2: 22 Khz OFF", true, NULL, this, "22koff1"));
			}
			if (i == 2) {
				widget->addItem(new CMenuForwarder("Tuner 3: 22 Khz ON", true, NULL, this, "22kon2"));
				widget->addItem(new CMenuForwarder("Tuner 3: 22 Khz OFF", true, NULL, this, "22koff2"));
			}
			if (i == 3) {
				widget->addItem(new CMenuForwarder("Tuner 4: 22 Khz ON", true, NULL, this, "22kon3"));
				widget->addItem(new CMenuForwarder("Tuner 4: 22 Khz OFF", true, NULL, this, "22koff3"));
			}
		}
	}
	CFEManager::getInstance()->linkFrontends(true);
}
