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
#include <driver/display.h>
#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/display.h>
#include <system/debug.h>
#include <gui/color_custom.h>
#include <system/helpers.h>
#include <cs_api.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <xmlinterface.h>

#include <gui/widget/msgbox.h>
#include <gui/widget/progresswindow.h>
#include <gui/scan.h>
#include <gui/scan_setup.h>
#include <zapit/zapit.h>
#include <zapit/scan.h>
#include <zapit/femanager.h>
#include <driver/record.h>
#include <gui/buildinfo.h>
#include <gui/widget/buttons.h>
#include <system/helpers.h>
#include <gui/components/cc_timer.h>

#if HAVE_COOL_HARDWARE
extern int cs_test_card(int unit, char * str);
#endif

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
	width = 50;
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
	timer = NULL;
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

//static int test_pos[4] = { 130, 192, 282, 360 };

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
		int ch = 0x2588;
		std::string tmp = Unicode_Character_to_UTF8(ch);
		size_t len = tmp.size();
		for (int i = 0; i < 12; i++)
		{
			memmove(&text[i*len], tmp.c_str(), len);
		}
		text[12*len] = 0;

		CVFD::getInstance()->ShowText(text);
		ShowMsg(LOCALE_MESSAGEBOX_INFO, "VFD test, Press OK to return", CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
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
		
		ShowMsg(LOCALE_MESSAGEBOX_INFO, str, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
		
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
				ShowMsg(LOCALE_MESSAGEBOX_INFO, str, CMsgBox::mbrBack, CMsgBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 ATR read failed", CMsgBox::mbrBack, CMsgBox::mbBack, "info");
				break;
			case -2:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 reset failed", CMsgBox::mbrBack, CMsgBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 1 open failed", CMsgBox::mbrBack, CMsgBox::mbBack, "info");
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
				ShowMsg(LOCALE_MESSAGEBOX_INFO, str, CMsgBox::mbrBack, CMsgBox::mbBack, "info.raw");
				break;
			case -1:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 ATR read failed", CMsgBox::mbrBack, CMsgBox::mbBack, "info");
				break;
			case -2:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 reset failed", CMsgBox::mbrBack, CMsgBox::mbBack, "info");
				break;
			default:
			case -3:
				ShowMsg(LOCALE_MESSAGEBOX_INFO, "Smardcard 2 open failed", CMsgBox::mbrBack, CMsgBox::mbBack, "info");
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
		
		ShowMsg(LOCALE_MESSAGEBOX_INFO, buffer, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
		
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
		
		ShowMsg(LOCALE_MESSAGEBOX_INFO, buffer, CMsgBox::mbrBack, CMsgBox::mbBack, NEUTRINO_ICON_INFO);
		
		return res;
	}
	else if (actionKey == "buttons")
	{
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;
		CHintBox * khintBox = NULL;
		CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, "Press button, or press EXIT to return");
		hintBox.paint();
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
				hintBox.hide();
				khintBox->paint();
			}
		}
		if (khintBox)
			delete khintBox;

		hintBox.hide();

		return res;
	}
#if 0 //some parts DEPRECATED
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
#endif
	else if (actionKey == "button"){
		if (button == NULL){
			button = new CComponentsButtonRed(100, 100, 100, 50, "Test", NULL, false, true, CC_SHADOW_OFF);
			button->enableShadow();
		}else
			button->disableShadow();
		

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
			circle = new CComponentsShapeCircle (100, 100, 100, NULL, CC_SHADOW_ON, COL_MENUCONTENT_PLUS_6, COL_MENUCONTENT_PLUS_0, COL_RED);

		if (!circle->isPainted())	
			circle->paint();
		else
			circle->hide();			
		return res;
	}
	else if (actionKey == "square"){
		if (sq == NULL){
			sq = new CComponentsShapeSquare (0, 0, 100, 100, NULL, CC_SHADOW_ON, COL_OLIVE, COL_LIGHT_GRAY, COL_RED);
			sq->enableFrame(true,1);
		}

		if (!sq->isPainted())
			sq->paint();
		else
			sq->hide();
		return res;
	}
	else if (actionKey == "picture"){
		if (pic == NULL)
			pic = new CComponentsPicture (100, 100, 200, 100, ICONSDIR "/mp3-5.jpg");

		if (!pic->isPainted() && !pic->isPicPainted())
			pic->paint();
		else
			pic->hide();
		return res;
	}
	else if (actionKey == "blink"){
		if (sq == NULL)
			sq = new CComponentsShapeSquare (0, 0, 100, 100, NULL, CC_SHADOW_ON, COL_OLIVE, COL_LIGHT_GRAY, COL_RED);

		if (sq->paintBlink(500000, true)){
			ShowHint("Testmenu: Blink","Testmenu: Blinking square is running ...", 700, 6);
		}
		if (sq->cancelBlink()){
			ShowHint("Testmenu: Blink","Testmenu: Blinking square stopped ...", 700, 2);
		}

		return res;
	}
	else if (actionKey == "blink_image"){
		if (pic == NULL)
			pic = new CComponentsPicture (100, 100, 200, 100, ICONSDIR "/btn_play.png");

		if (pic->paintBlink(500000, true)){
			ShowHint("Testmenu: Blink","Testmenu: Blinking image is running ...", 700, 10);
		}
		if (pic->cancelBlink()){
			ShowHint("Testmenu: Blink","Testmenu: Blinking image stopped ...", 700, 2);
		}

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
	else if (actionKey == "form_blink_item"){
		if (form == NULL){
			form = new CComponentsForm();
			form->setColorBody(COL_DARK_GRAY);
			form->setDimensionsAll(100, 100, 250, 100);
			form->setFrameThickness(2);
			form->setColorFrame(COL_WHITE);
			//form->doPaintBg(false);

			CComponentsPicture *ptmp = new CComponentsPicture(10, 10, NEUTRINO_ICON_HINT_INFO);
			ptmp->doPaintBg(false);
			ptmp->SetTransparent(CFrameBuffer::TM_BLACK);
			form->addCCItem(ptmp);

			CComponentsText *text = new CComponentsText(80, 30, 100, 50, "Info");
			text->doPaintBg(false);
			form->addCCItem(text);

			form->paint0();
			ptmp->kill();
		}

		if (static_cast<CComponentsPicture*>(form->getCCItem(0))-> paintBlink(500000, true)){
			ShowHint("Testmenu: Blink","Testmenu: Blinking embedded image ...", 700, 10);
		}
		if (form->getCCItem(0)->cancelBlink()){
			ShowHint("Testmenu: Blink","Testmenu: Blinking embedded image stopped ...", 700, 2);
		}

		form->kill();
		delete form; form = NULL;
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
	else if (actionKey == "blinking_text"){
		if (txt == NULL){
			txt = new CComponentsText();
			txt->setDimensionsAll(100, 100, 250, 100);
			txt->setText("This is a text for testing textbox", CTextBox::NO_AUTO_LINEBREAK);
		}

		if (txt->paintBlink(500000, true)){
			ShowHint("Testmenu: Blink","Testmenu: Blinking text is running ...", 700, 10);
		}
		if (txt->cancelBlink()){
			ShowHint("Testmenu: Blink","Testmenu: Blinking text stopped ...", 700, 2);
		}

		return res;
	}
	else if (actionKey == "text_ext"){
		if (text_ext == NULL)
			text_ext = new CComponentsExtTextForm();
		text_ext->setDimensionsAll(10, 20, 300, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()+2*2);
		text_ext->setLabelAndText("Label", "Text for demo", g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
		text_ext->setFrameThickness(2);
// 		text_ext->setLabelWidthPercent(15/*%*/);
		
		if (text_ext->isPainted())
			text_ext->hide();
		else
			text_ext->paint();
		return res;
	}
	else if (actionKey == "blinking_text_ext"){
		if (text_ext == NULL){
			text_ext = new CComponentsExtTextForm();
			text_ext->setDimensionsAll(10, 20, 300, 48);
			text_ext->setLabelAndText("Label", "Text for demo", g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
			text_ext->setFrameThickness(2);
		}

		if (text_ext->paintBlink(500000, true)){
			ShowHint("Testmenu: Blink","Testmenu: Blinking extended text is running ...", 700, 10);
		}
		if (text_ext->cancelBlink()){
			ShowHint("Testmenu: Blink","Testmenu: Blinking extended text stopped ...", 700, 2);
		}
		return res;
	}
	else if (actionKey == "header"){
		int hh = 30;//g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
		if (header == NULL){
			header = new CComponentsHeader (100, 50, 500, hh, "Test-Header"/*, NEUTRINO_ICON_INFO, CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU*/);
			header->addContextButton(NEUTRINO_ICON_BUTTON_RED);
			header->addContextButton(CComponentsHeader::CC_BTN_HELP | CComponentsHeader::CC_BTN_EXIT | CComponentsHeader::CC_BTN_MENU);
		}
		else{	//For existing instances it's recommended to remove old button icons before add new buttons,
			//otherwise icons will be appended to already existant icons, alternatively use the setContextButton() methode
 			header->removeContextButtons();
			//enable clock in header with default format
			header->enableClock(true, "%H:%M", "%H %M", true);
		}

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
// 		CComponentsPicture *logo  = new CComponentsPicture(logo_x, 0, 100, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight(), ICONSDIR "/hint_tvmode.png");
//		set the transparent background for picture item
// 		logo->doPaintBg(false);
//		insert the ne object
// 		header->insertCCItem(1, logo); //replace text with logo

		
		if (!header->isPainted()){
			header->paint();
		}
		else{
			header->hide();
		}

		return res;
	}
	else if (actionKey == "footer"){
		int hh = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
		if (footer == NULL){
			footer = new CComponentsFooter (100, 30, 1000, hh, CComponentsFooter::CC_BTN_HELP | CComponentsFooter::CC_BTN_EXIT | CComponentsFooter::CC_BTN_MENU |CComponentsFooter::CC_BTN_MUTE_ZAP_ACTIVE, NULL, true);
			//int start = 5, btnw =90, btnh = 37;
			footer->setButtonFont(g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]);
			footer->setIcon(NEUTRINO_ICON_INFO);

			//add button labels with conventional button label struct
			footer->setButtonLabels(TestButtons, TestButtonsCount, 0, footer->getWidth()/TestButtonsCount);

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
		if (iconform == NULL){
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
			iconform->paint();
		}

		CComponentsPicture* img = static_cast<CComponentsPicture*>(iconform->getCCItem(2));
		img->kill();

		if (img->paintBlink(500000, true)){
			ShowHint("Testmenu: Blink","Testmenu: Blinking image is running ...", 700, 10);
		}
		if (img->cancelBlink(true)){
			ShowHint("Testmenu: Blink","Testmenu: Blinking image stopped ...", 700, 2);
		}

		iconform->kill();
		delete iconform;
		iconform = NULL;
		return res;
	}
	else if (actionKey == "window"){
		if (window == NULL){
			window = new CComponentsWindow();
			window->setWindowCaption("|........HEADER........|", CCHeaderTypes::CC_TITLE_CENTER);
			window->setDimensionsAll(50, 50, 500, 500);
			window->setWindowIcon(NEUTRINO_ICON_INFO);
			window->enableShadow();
			window->getFooterObject()->setCaption("|........FOOTER........|", CCHeaderTypes::CC_TITLE_CENTER);

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

			CComponentsExtTextForm	*text_1 = new CComponentsExtTextForm();
			text_1->setDimensionsAll(10, CC_CENTERED, 380, 48);
			text_1->setLabelAndText("Page", "Number 1", g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
			text_1->setFrameThickness(2);
			text_1->setPageNumber(0);
			window->addWindowItem(text_1);

			CComponentsExtTextForm	*text_2 = new CComponentsExtTextForm();
			text_2->setDimensionsAll(10, 200, 380, 48);
			text_2->setLabelAndText("Page", "Number 2", g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]);
			text_2->setFrameThickness(2);
			text_2->setPageNumber(1);
			window->addWindowItem(text_2);
		}
#if 0
		if (!window->isPainted()){
			window->paint(); //if no other page has been defined, 1st page always painted
		}
		else{
#endif			//or paint direct a defined page
// 			if (window->getCurrentPage() == 1)
			window->enablePageScroll(CComponentsWindow::PG_SCROLL_M_UP_DOWN_KEY | CComponentsWindow::PG_SCROLL_M_LEFT_RIGHT_KEY);
			window->enableSidebar();
			window->paint();
			window->getBodyObject()->exec();
			window->hide();
#if 0
		}
#endif
		return res;
	}
	else if (actionKey == "running_clock"){	
		if (clock_r == NULL){
			clock_r = new CComponentsFrmClock(100, 50, NULL, "%H.%M:%S", NULL, true);
			clock_r->setClockFont(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]);
			clock_r->setClockInterval(1);
// 			clock_r->doPaintBg(false);
		}
		
		if (!clock_r->isPainted()){
			if (clock_r->Start())
				return menu_return::RETURN_EXIT_ALL;;
		}
		else {
			if (clock_r->Stop()){
				clock_r->kill();
				delete clock_r;
				clock_r = NULL;
				return menu_return::RETURN_EXIT_ALL;
			}
		}
	}
	else if (actionKey == "clock"){
		if (clock == NULL){
			clock = new CComponentsFrmClock(100, 50, NULL, "%d.%m.%Y-%H:%M");
			clock->setClockFont(g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]);
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
	else if (actionKey == "progress_window"){
		//classical
		CProgressWindow pw0("Progress Single Test");
		pw0.paint();
		size_t max = 3;
		for(size_t i = 0; i< max; i++){
			pw0.showStatus(i, max, to_string(i));
			sleep(1);
		}
		pw0.hide();

		CProgressWindow pw1("Progress Local/Global Test");
		pw1.paint();
		for(size_t i = 0; i< max; i++){
			pw1.showGlobalStatus(i, max, to_string(i));
			for(size_t j = 0; j< max; j++){
				pw1.showLocalStatus(j, max, to_string(j));
				sleep(1);
			}
		}
		pw1.hide();

		//with signals
		sigc::signal<void, size_t, size_t, std::string> OnProgress0, OnProgress1;
		CProgressWindow pw2("Progress Single Test -> single Signal", CCW_PERCENT 50, CCW_PERCENT 30, &OnProgress0);
		pw2.paint();

		for(size_t i = 0; i< max; i++){
			OnProgress0(i, max, to_string(i));
			sleep(1);
		}
		pw2.hide();

		OnProgress0.clear();
		OnProgress1.clear();
		CProgressWindow pw3("Progress Single Test -> dub Signal", CCW_PERCENT 50, CCW_PERCENT 20, NULL, &OnProgress0, &OnProgress1);
		pw3.paint();

		for(size_t i = 0; i< max; i++){
			OnProgress1(i, max, to_string(i));
				for(size_t j = 0; j< max; j++){
					OnProgress0(j, max, to_string(j));
					sleep(1);
				}
		}
		pw3.hide();

		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "hintbox_test")
	{
		ShowHint("Testmenu: Hintbox popup test", "Test for HintBox,\nPlease press any key or wait some seconds! ...", 700, 10, NULL, NEUTRINO_ICON_HINT_IMAGEINFO, CComponentsHeader::CC_BTN_EXIT);
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_test_yes_no")
	{
		int msgRet = ShowMsg("Testmenu: MsgBox test", "Test for MsgBox,\nPlease press key! ...", CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NULL, 500);

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgRet);
		ShowHint("MsgBox test returns", msg_txt.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_test_cancel"){
		CMsgBox * msgBox = new CMsgBox("Testmenu: MsgBox exit test", "Please press key");
// 		msgBox->setTimeOut(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR]);
		msgBox->paint();
		res = msgBox->exec();
		msgBox->hide();

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgBox->getResult());
		delete msgBox;

		ShowHint("MsgBox test returns", msg_txt.c_str(), 700, 10, NULL, NULL, CComponentsHeader::CC_BTN_EXIT);

		return res;
	}
	else if (actionKey == "msgbox_test_all"){
		int msgRet = ShowMsg("Testmenu: MsgBox test", "Test for MsgBox,\nPlease press key! ...", CMsgBox::mbrOk, CMsgBox::mbAll, NULL, 700);

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgRet);
		ShowHint("MsgBox test returns", msg_txt.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_test_yes_no_cancel"){
		int msgRet = ShowMsg("Testmenu: MsgBox test", "Test for MsgBox,\nPlease press key! ...", CMsgBox::mbrCancel, CMsgBox::mbYes | CMsgBox::mbNo | CMsgBox::mbCancel, NULL, 500);

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgRet);
		ShowHint("MsgBox test returns", msg_txt.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_test_ok_cancel"){
		int msgRet = ShowMsg("Testmenu: MsgBox test", "Test for MsgBox,\nPlease press key! ...", CMsgBox::mbrOk, CMsgBox::mbOk | CMsgBox::mbCancel, NULL, 500);

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgRet);
		ShowHint("MsgBox test returns", msg_txt.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_test_no_yes"){
		int msgRet = ShowMsg("Testmenu: MsgBox test", "Test for MsgBox,\nPlease press key! ...", CMsgBox::mbrOk, CMsgBox::mbNoYes, NULL, 500);

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgRet);
		ShowHint("MsgBox test returns", msg_txt.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_test_ok"){
		int msgRet = ShowMsg("Testmenu: MsgBox test", "Test for MsgBox,\nPlease press ok key! ...", CMsgBox::mbrOk, CMsgBox::mbOk, NULL, 500);

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgRet);
		ShowHint("MsgBox test returns", msg_txt.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_test_cancel_timeout"){
		int msgRet = ShowMsg("Testmenu: MsgBox test", "Test for MsgBox,\nPlease press ok key or wait! ...", CMsgBox::mbrCancel, CMsgBox::mbOKCancel, NULL, 500, 6, true);

		std::string msg_txt = "Return value of MsgBox test is ";
		msg_txt += to_string(msgRet);
		ShowHint("MsgBox test returns", msg_txt.c_str());
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_error"){
		DisplayErrorMessage("Error Test!");
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "msgbox_info"){
		DisplayInfoMessage("Info Test!");
		return menu_return::RETURN_REPAINT;
	}
	else if (actionKey == "footer_key"){
		CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, "Footer-Key pressed. Press EXIT to return", 350, NULL, NULL, CComponentsHeader::CC_BTN_EXIT);
		hintBox.setTimeOut(15);

		//optional: it is also possible to add more items into the hint box
		//here some examples:
		hintBox.addHintItem(new CComponentsShapeSquare(CC_CENTERED, CC_APPEND, 330, 2, NULL, false, COL_MENUCONTENT_PLUS_6, COL_RED));
		hintBox.addHintItem("- text with left icon", CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH, NEUTRINO_ICON_HINT_INFO);
		hintBox.addHintItem("- text right without an icon", CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::RIGHT);
		hintBox.addHintItem("- text right with an icon", CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH | CTextBox::RIGHT, NEUTRINO_ICON_HINT_INFO);

		hintBox.paint();
		res = hintBox.exec();
		hintBox.hide();

		return res;
	}
	else if (actionKey == "show_records"){
		showRecords();
		return res;
	}
	
	
	return showTestMenu();
}

void CTestMenu::showRecords()
{
	CRecordManager * crm	= CRecordManager::getInstance();

	const int box_posX = 30;
	const int box_posY = 300;
	if (!timer)
		timer = new CComponentsTimer(1);

	if (crm->RecordingStatus())
	{
		CComponentsForm *recordsbox = new CComponentsForm();
		recordsbox->setWidth(0);
		recordsbox->doPaintBg(false);
		recordsbox->setCornerType(CORNER_NONE);
		//recordsbox->setColorBody(COL_BACKGROUND_PLUS_0);

		recmap_t recmap = crm->GetRecordMap();
		std::vector<CComponentsPicture*> images;
		CComponentsForm *recline = NULL;
		CComponentsText *rec_name = NULL;
		int y_recline = 0;
		int h_recline = 0;
		int w_recbox = 0;
		int w_shadow = OFFSET_SHADOW/2;

		for(recmap_iterator_t it = recmap.begin(); it != recmap.end(); it++)
		{
			CRecordInstance * inst = it->second;

			recline = new CComponentsForm(0, y_recline, 0);
			recline->doPaintBg(true);
			recline->setColorBody(COL_INFOBAR_PLUS_0);
			recline->enableShadow(CC_SHADOW_ON, w_shadow);
			recline->setCorner(CORNER_RADIUS_MID);
			recordsbox->addCCItem(recline);

			CComponentsPicture *iconf = new CComponentsPicture(OFFSET_INNER_MID, CC_CENTERED, NEUTRINO_ICON_REC, recline, CC_SHADOW_OFF, COL_RED, COL_INFOBAR_PLUS_0);
			iconf->setCornerType(CORNER_NONE);
			iconf->doPaintBg(true);
			iconf->SetTransparent(CFrameBuffer::TM_BLACK);
			images.push_back(iconf);
			h_recline = iconf->getHeight();
			y_recline += h_recline + w_shadow;
			recline->setHeight(h_recline);

			std::string records_msg = inst->GetEpgTitle();

			rec_name = new CComponentsText(iconf->getWidth()+2*OFFSET_INNER_MID, CC_CENTERED, 0, 0, records_msg, CTextBox::AUTO_WIDTH, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]);
			rec_name->doPaintBg(false);
			rec_name->setTextColor(COL_INFOBAR_TEXT);

			recline->setHeight(std::max(rec_name->getHeight(), iconf->getHeight()));
			recline->setWidth(OFFSET_INNER_MIN+iconf->getWidth()+OFFSET_INNER_MID+rec_name->getWidth()+OFFSET_INNER_MID);
			w_recbox = (std::max(recline->getWidth(), recordsbox->getWidth()));

			recline->addCCItem(rec_name);

			recordsbox->setWidth(std::max(recordsbox->getWidth(), recline->getWidth()));
			y_recline += h_recline;
		}

		int h_rbox = 0;
		for(size_t i = 0; i< recordsbox->size(); i++)
			h_rbox += recordsbox->getCCItem(i)->getHeight()+w_shadow;

		recordsbox->setDimensionsAll(box_posX, box_posY, w_recbox+w_shadow, h_rbox);
		recordsbox->paint0();

		for(size_t j = 0; j< images.size(); j++){
			images[j]->kill();
			images[j]->paintBlink(timer);
		}

		ShowHint("Testmenu: Records", "Record test ...", 200, 30, NULL, NEUTRINO_ICON_HINT_RECORDING, CComponentsHeader::CC_BTN_EXIT);
		recordsbox->kill();
		delete recordsbox; recordsbox = NULL;
	}
	else
		ShowHint("Testmenu: Records", "No records ...", 200, 30, NULL, NEUTRINO_ICON_HINT_RECORDING, CComponentsHeader::CC_BTN_EXIT);
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

	//msg/hintbox
	CMenuWidget * w_msg = new CMenuWidget("MsgBox/Hint-Tests", NEUTRINO_ICON_INFO, width, MN_WIDGET_ID_TESTMENU_HINT_MSG_TESTS);
	w_test.addItem(new CMenuForwarder(w_msg->getName(), true, NULL, w_msg));
	showMsgTests(w_msg);

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
	widget->addItem(new CMenuForwarder("Progress Window", true, NULL, this, "progress_window"));
	widget->addItem(new CMenuForwarder("Running Clock", true, NULL, this, "running_clock"));
	widget->addItem(new CMenuForwarder("Clock", true, NULL, this, "clock"));
	widget->addItem(new CMenuForwarder("Button", true, NULL, this, "button"));
	widget->addItem(new CMenuForwarder("Circle", true, NULL, this, "circle"));
	widget->addItem(new CMenuForwarder("Square", true, NULL, this, "square"));
	widget->addItem(new CMenuForwarder("Blinking-Square", true, NULL, this, "blink"));
	widget->addItem(new CMenuForwarder("Blinking-Image", true, NULL, this, "blink_image"));
	widget->addItem(new CMenuForwarder("Picture", true, NULL, this, "picture"));
	widget->addItem(new CMenuForwarder("Channel-Logo", true, NULL, this, "channellogo"));
	widget->addItem(new CMenuForwarder("Form", true, NULL, this, "form"));
	widget->addItem(new CMenuForwarder("Form with blinking item", true, NULL, this, "form_blink_item"));
	widget->addItem(new CMenuForwarder("Text", true, NULL, this, "text"));
	widget->addItem(new CMenuForwarder("Blinking Text", true, NULL, this, "blinking_text"));
	widget->addItem(new CMenuForwarder("Header", true, NULL, this, "header"));
	widget->addItem(new CMenuForwarder("Footer", true, NULL, this, "footer"));
	widget->addItem(new CMenuForwarder("Icon-Form", true, NULL, this, "iconform"));
	widget->addItem(new CMenuForwarder("Window", true, NULL, this, "window"));
	widget->addItem(new CMenuForwarder("Text-Extended", true, NULL, this, "text_ext"));
	widget->addItem(new CMenuForwarder("Blinking Extended Text", true, NULL, this, "blinking_text_ext"));
	widget->addItem(new CMenuForwarder("Scrollbar", true, NULL, this, "scrollbar"));
	widget->addItem(new CMenuForwarder("Record", true, NULL, this, "show_records"));
}

void CTestMenu::showHWTests(CMenuWidget *widget)
{
	widget->addIntroItems();
	widget->addItem(new CMenuForwarder("VFD", true, NULL, this, "vfd"));
	widget->addItem(new CMenuForwarder("Network", true, NULL, this, "network"));
#if HAVE_COOL_HARDWARE
	widget->addItem(new CMenuForwarder("Smartcard 1", true, NULL, this, "card0"));
	widget->addItem(new CMenuForwarder("Smartcard 2", true, NULL, this, "card1"));
#endif
	widget->addItem(new CMenuForwarder("HDD", true, NULL, this, "hdd"));
	widget->addItem(new CMenuForwarder("SD/MMC", true, NULL, this, "mmc"));
#if 0 //some parts DEPRECATED
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
#endif
}

void CTestMenu::showMsgTests(CMenuWidget *widget)
{
	widget->addIntroItems();
	widget->addItem(new CMenuForwarder("HintBox test!", true, NULL, this, "hintbox_test"));
	widget->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, "MsgBox"));
	widget->addItem(new CMenuForwarder("cancel on timeout", true, NULL, this, 	"msgbox_test_cancel_timeout"));
	widget->addItem(new CMenuForwarder("yes no", true, NULL, this, "msgbox_test_yes_no"));
	widget->addItem(new CMenuForwarder("no yes", true, NULL, this, "msgbox_test_no_yes"));
	widget->addItem(new CMenuForwarder("cancel", true, NULL, this, "msgbox_test_cancel"));
	widget->addItem(new CMenuForwarder("ok", true, NULL, this, "msgbox_test_ok"));
	widget->addItem(new CMenuForwarder("ok cancel", true, NULL, this, "msgbox_test_ok_cancel"));
	widget->addItem(new CMenuForwarder("yes no cancel", true, NULL, this, "msgbox_test_yes_no_cancel"));
	widget->addItem(new CMenuForwarder("all", true, NULL, this, "msgbox_test_all"));
	widget->addItem(new CMenuSeparator(CMenuSeparator::STRING | CMenuSeparator::LINE, "Error/Info"));
	widget->addItem(new CMenuForwarder("Error Message!", true, NULL, this, "msgbox_error"));
	widget->addItem(new CMenuForwarder("Info Message!", true, NULL, this, "msgbox_info"));
}
