/*
        $Id: personalize.cpp,1.43 2011/04/26 19:45:31 tuxbox-cvs Exp $

        Customization Menu - Neutrino-GUI

        Copyright (C) 2007 Speed2206
        and some other guys
        
        Reworked by dbt (Thilo Graf)
        Copyright (C) 2010, 2011 dbt

        Comment:

        This is the customization menu, as originally showcased in
        Oxygen. It is a more advanced version of the 'user levels'
        patch currently available.
        
        The reworked version >1.24 works more dynamicly with input objects
        and their parameters and it's more code reduced. It's also independent
        from #ifdefs of items. 
        The personalize-object collects all incomming forwarder item objects.
        These will be handled here and will be shown after evaluation.


        License: GPL

        This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.

		
	NOTE for ignorant distributors:
	It's not allowed to distribute any compiled parts of this code, if you don't accept the terms of GPL.
	Please read it and understand it right!
	This means for you: Hold it, if not, leave it! You could face legal action! 
	Otherwise ask the copyright owners, anything else would be theft!
        
        
        Parameters:
	addItem(CMenuWidget *widget, CMenuItem *menuItem, const int *personalize_mode, const bool defaultselected, const bool item_mode),
	addItem(const int& widget_id, CMenuItem *menuItem, const int *personalize_mode, const bool defaultselected, const bool item_mode),
	
	CMenuWidget *widget 		= pointer to menue widget object, also to get with 'getWidget(const int& id)'
	const int& widget_id		= index of widget (overloaded), this index is definied in vector 'v_widget', to get with 'getWidgetId()' 
	CMenuItem *menuItem		= pointer to a menuitem object, can be forwarders, locked forwarders and separators...NO CHOOSERS!
	const int *personalize_mode	= optional, default NULL, pointer to a specified personalize setting look at: PERSONALIZE_MODE, this regulates the personalize mode
	const bool item_mode		= optional, default true, if you don't want to see this item in personalize menue, then set it to false
	CMenuItem *observer_Item	= optional, default NULL, if you want to observe this item with another item (observer), then use this prameter.
					  Effect: this observed item will be deactivated, if observer is set to 'visible' or 'pin-protected'
	menu_item_disable_cond_t& disable_condition = optional, default DCOND_MODE_NONE
					  Effect: on used condition eg. DCOND_MODE_TS, this item is disabled in mode_ts
					  Also usable are combined conditions: eg: DCOND_MODE_TV | DCOND_MODE_TS, this item is disabled in mode_tv and mode_ts
					  More possible conditions are defined in menue.h.
	
	Icon handling:
	If you define an icon in the item object, this will be shown in the personalized menu but not the personilazitions menue itself, otherwise a shortcut will be create
	
	Shortcuts (optional): default is set to '1':
	A default start-shortcut you can create with personalize.setShortcut(), 
	this sets default value '1', e.g.personalize.setShortcut(0) sets value '0'
	Only values 0-9 are possible, others will be ignored!
	
	Separators:
	Add separators with
	addSeparator(CMenuWidget &widget, const neutrino_locale_t locale_text, const bool item_mode)
	OR
	addSeparator(const int& widget_id, const neutrino_locale_t locale_text, const bool item_mode)
	
		Parameters:
		CMenuWidget &widget 			= rev to menue widget object
		const int& widget_id			= index of widget (overloaded), this index is definied in vector 'v_widget', to get with 'getWidgetId(widget_object)'
		
		const neutrino_locale_t locale_text	= optional, default NONEXISTANT_LOCALE, adds a line separator, is defined a locale then adds a text separator
		const bool item_mode			= optional, default true, if you don't want to see this sparator also in personalize menue, then set it to false, usefull for to much separtors ;-)
		
	Usage:
	It's possible to personalize only forwarders, locked forwarders and separators!

	Example:
	//we need an instance of CPersonalizeGUI()
	CPersonalizeGui personalize;

	//do you need a start shortcut !=1 then set a start number for shortcuts with
	personalize.setShortcut(0...9);

	//create a menue widget object, this will be automaticly shown as menu item in your peronalize menu
	CMenuWidget * mn =  new CMenuWidget(LOCALE_MAINMENU_HEAD, ICON    ,width);
	OR
	create a widget struct:
	const mn_widget_struct_t menu_widgets[count of available widgets] =
	{
		{LOCALE_1, 	NEUTRINO_ICON_1, 	width1},
		{LOCALE_2, 	NEUTRINO_ICON_2, 	width2},
		{LOCALE_3,	NEUTRINO_ICON_3, 	width3}, 
	};
	
	//add it to widget collection as single
	personalize.addWidget(mn);
	OR as struct
	personalize.addWidgets(widget_struct, count of available widgets);

	//create a forwarder object:
	CMenuItem *item = new CMenuForwarder(LOCALE_MAINMENU_TVMODE, true, NULL, this, "tv", CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);

	//now you can add this to personalization
	personalize.addItem(&mn, tvswitch, &g_settings.personalize_tvmode);
	OR with widget id
	personalize.addItem(0, tvswitch, &g_settings.personalize_tvmode);
	
	//if you want to add a non personalized separator use this function, you must use this anstead addItem(GenericMenuSeparatorLine)  
	personalize.addSeparator(mn);
	OR with widget id
	personalize.addSeparator(0);
	//otherwise you can add a separator at this kind:
	personalize.addItem(&mn, GenericMenuSeparatorLine);
	OR with widget id
	personalize.addItem(0, GenericMenuSeparatorLine);

	//finally add the menue items
	personalize.addPersonalizedItems();
	//this member makes the same like mn->addItem(...) known from CMenuWidget()-class for all collected and evaluated objects
	
	//reset shortcuts:
	personalize.setShortcut();
	
	Enums:
	PERSONALIZE_MODE: use as parameter 'personalize_mode'
		PERSONALIZE_MODE_NOTVISIBLE 	: not visible in your personalized menue
		PERSONALIZE_MODE_VISIBLE	: visible in your personalized menue
		PERSONALIZE_MODE_PIN		: visible in your personalized menue with PIN access
		
	PERSONALIZE_PROTECT_MODE: used also as parameter 'personalize_mode'
		PERSONALIZE_MODE_NOT_PROTECTED	: visible in personalize settings menue with PIN setup, option 'no'
		PERSONALIZE_MODE_PIN_PROTECTED	: visible in personalize settings menue with PIN setup, option 'yes'
		
	PERSONALIZE_ITEM_MODE: use as as parameter 'item_mode items in personalize settings menu 
		PERSONALIZE_SHOW_NO		: dont'show this item in personalize settings menu
		PERSONALIZE_SHOW_AS_ITEM_OPTION	: show as item with options 'visible, not visible or PIN'
		PERSONALIZE_SHOW_AS_ACCESS_OPTION: show as item with options 'PIN' with 'yes' or 'no'
		PERSONALIZE_SHOW_ONLY_IN_PERSONALIZE_MENU :usefull to hide separators in menu, but visible only in personalizing menu
	

*/

#include <global.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <mymenu.h>
#include <sigc++/bind.h>
#include <stdio.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <daemonc/remotecontrol.h>
#include <gui/widget/helpbox.h>
#include "widget/msgbox.h"
#include "widget/hintbox.h"
#include "widget/keychooser.h"
#include "color.h"
#include "personalize.h"
#include "plugins.h"

#include <system/settings.h>
#include <system/helpers.h>

using namespace std;

const CMenuOptionChooser::keyval PERSONALIZE_MODE_OPTIONS[CPersonalizeGui::PERSONALIZE_MODE_MAX] =
{
	{ CPersonalizeGui::PERSONALIZE_MODE_NOTVISIBLE	, LOCALE_PERSONALIZE_NOTVISIBLE      	},// The option is NOT visible on the menu's
	{ CPersonalizeGui::PERSONALIZE_MODE_VISIBLE	, LOCALE_PERSONALIZE_VISIBLE         	},// The option is visible on the menu's
	{ CPersonalizeGui::PERSONALIZE_MODE_PIN		, LOCALE_PERSONALIZE_PIN      		},// PIN Protect the item on the menu
};

const CMenuOptionChooser::keyval PERSONALIZE_ACTIVE_MODE_OPTIONS[CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_MAX] =
{
	{ CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_DISABLED	, LOCALE_PERSONALIZE_DISABLED        	},// The option is NOT enabled / accessible
	{ CPersonalizeGui::PERSONALIZE_ACTIVE_MODE_ENABLED	, LOCALE_PERSONALIZE_ENABLED         	},// The option is enabled / accessible
};

const CMenuOptionChooser::keyval PERSONALIZE_PROTECT_MODE_OPTIONS[CPersonalizeGui::PERSONALIZE_PROTECT_MODE_MAX] =
{
	{ CPersonalizeGui::PERSONALIZE_PROTECT_MODE_NOT_PROTECTED	, LOCALE_PERSONALIZE_NOTPROTECTED    	},// The menu/option is NOT protected
	{ CPersonalizeGui::PERSONALIZE_PROTECT_MODE_PIN_PROTECTED	, LOCALE_PERSONALIZE_PINPROTECT      	},// The menu/option is protected by a PIN
};

CPersonalizeGui::CPersonalizeGui() : CPINProtection(g_settings.personalize_pincode)
{
	width 	= 0;
	widget_count = 0;
	shortcut = 1;
	show_usermenu = false;
	show_pluginmenu = false;
	show_pin_setup = false;
	user_menu_notifier = NULL;
	pin_setup_notifier = NULL;
	tmpW = NULL;
	v_observ.clear();
	options_count = 0;
}

CPersonalizeGui::~CPersonalizeGui()
{
	v_widget.clear();
	v_observ.clear();
}

int CPersonalizeGui::exec(CMenuTarget* parent, const string & actionKey)
{
	int res = menu_return::RETURN_REPAINT;

	if (actionKey == ">d") {
		int selected = uMenu->getSelected();
		if (selected >= customkey_offset) {
			if(parent)
				parent->hide();
			const char *ak = (static_cast<CMenuForwarder*>(uMenu->getItem(selected)))->getActionKey();
			if (ak && *ak && std::string(ak).find_first_not_of("0123456789") == std::string::npos) {
				int i = atoi(ak);
				if (i > -1) {
					for (unsigned int j = 4; j < g_settings.usermenu.size(); j++) {
						std::string name = to_string(j);
						std::string usermenu_key("usermenu_key_");
						usermenu_key += name;
						CNeutrinoApp::getInstance()->getConfigFile()->deleteKey(usermenu_key);
						std::string txt1("usermenu_tv_");
						txt1 += name;
						CNeutrinoApp::getInstance()->getConfigFile()->deleteKey(txt1);
						txt1 += "_text";
						CNeutrinoApp::getInstance()->getConfigFile()->deleteKey(txt1);
					}
					delete g_settings.usermenu[i];
					g_settings.usermenu[i] = NULL;
				}
			}
			uMenu->removeItem(selected);
			uMenu->hide();
			return menu_return::RETURN_REPAINT;
		}
		return menu_return::RETURN_NONE;
	}

	if (parent)
		parent->hide();

	if (actionKey == ">a") {
		unsigned int i = g_settings.usermenu.size();
		CUserMenuSetup *cms = new CUserMenuSetup(LOCALE_USERMENU_HEAD, i);
		SNeutrinoSettings::usermenu_t *um = new SNeutrinoSettings::usermenu_t;
		um->key = CRCInput::RC_nokey;
		g_settings.usermenu.push_back(um);
		CMenuDForwarder *fw = new CMenuDForwarder(CRCInput::getKeyName(um->key), true, um->title, cms, to_string(i).c_str());
		cms->setCaller(fw);

#if 0
		int selected = uMenu->getSelected();
		if (selected >= customkey_offset)
			uMenu->insertItem(selected, fw);
		else
#endif
			uMenu->addItem(fw, true);
		uMenu->hide();
		return menu_return::RETURN_REPAINT;
	}

	for (int i = 0; i<(widget_count); i++)
	{
		ostringstream i_str;
		i_str << i;
		string s(i_str.str());
		string a_key = s;

		if(actionKey == a_key)
		{
			res = ShowMenuOptions(i);
			return res;
		}
	}

	if (actionKey=="personalize_help") {
		ShowHelpPersonalize();
		return res;
	}

	if (actionKey=="restore") {
		ShowPersonalizationMenu	();
		return menu_return::RETURN_EXIT_ALL;
	}

	//also handle pin access
	handleSetting(&g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS]);

	//pin protected access to personalize menu also if found any pin protected items
	bool is_pin_protected = g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS];

	if (is_pin_protected || hasPinItems()) {
		setHint(LOCALE_PERSONALIZE_PINHINT); //from CPINProtection
		is_pin_protected = true;
		if (check())
			is_pin_protected = false;
	}
	if (!is_pin_protected){
		res = ShowPersonalizationMenu();
		SaveAndExit();
	}

	return res;
}

//This is the main personalization menu. From here we can go to the other sub-menu's and enable/disable
//the PIN code feature, as well as determine whether or not the EPG menu/Features menu is accessible.
int CPersonalizeGui::ShowPersonalizationMenu()
{
	width = 40;

	CMenuWidget* pMenu = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width, MN_WIDGET_ID_PERSONALIZE);
	pMenu->addIntroItems(NONEXISTANT_LOCALE, LOCALE_PERSONALIZE_ACCESS);

	//pin
	CPINChangeWidget *pinChangeWidget = NULL;
	if (show_pin_setup)
		ShowPinSetup(pMenu, pinChangeWidget);

	//personalized menues
	CMenuForwarder *p_mn[widget_count];
	for (int i = 0; i<(widget_count); i++)
	{
		ostringstream i_str;
		i_str << i;
		string s(i_str.str());
		string action_key = s;
		string mn_name = v_widget[i]->getName();
		p_mn[i] = new CMenuForwarder(mn_name.c_str(), true, NULL, this, action_key.c_str(), CRCInput::convertDigitToKey(i+1));
		pMenu->addItem(p_mn[i]);
	}

	//usermenu
	uMenu = NULL;
	if (show_usermenu)
	{
		pMenu->addItem(GenericMenuSeparatorLine);
		uMenu = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width, MN_WIDGET_ID_PERSONALIZE_USERMENU);
		pMenu->addItem(new CMenuForwarder(LOCALE_USERMENUS_HEAD, true, NULL, uMenu, NULL, CRCInput::RC_green));

		ShowUserMenu();
	}
	CMenuWidget* plMenu = NULL;
	int pcount = g_Plugins->getNumberOfPlugins();
	std::string pldesc[pcount];
	int pltype[pcount];
	if (show_pluginmenu)
	{
		if (!show_usermenu)
			pMenu->addItem(GenericMenuSeparatorLine);
		plMenu = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width, MN_WIDGET_ID_PERSONALIZE_PLUGINS);
		pMenu->addItem(new CMenuForwarder(LOCALE_PERSONALIZE_PLUGINS, true, NULL, plMenu, NULL, CRCInput::RC_blue));

		ShowPluginMenu(plMenu, pldesc, pltype);
	}

	//help
	pMenu->addItem(GenericMenuSeparatorLine);
	pMenu->addItem(new CMenuForwarder(LOCALE_PERSONALIZE_HELP, true, NULL, this, "personalize_help", CRCInput::RC_help));

	int res = pMenu->exec(NULL, "");
	if (show_pluginmenu) {
		g_settings.plugins_disabled = "";
		g_settings.plugins_game = "";
		g_settings.plugins_tool = "";
		g_settings.plugins_script = "";
		g_settings.plugins_lua = "";
		for (int i = 0; i < pcount; i++) {
			if (pltype[i] & CPlugins::P_TYPE_DISABLED) {
				if (!g_settings.plugins_disabled.empty())
					g_settings.plugins_disabled += ",";
				g_settings.plugins_disabled +=  g_Plugins->getFileName(i);
				g_Plugins->setType(i, CPlugins::P_TYPE_DISABLED);
			} else if (pltype[i] & CPlugins::P_TYPE_GAME) {
				if (!g_settings.plugins_game.empty())
					g_settings.plugins_game += ",";
				g_settings.plugins_game +=  g_Plugins->getFileName(i);
				g_Plugins->setType(i, CPlugins::P_TYPE_GAME);
			} else if (pltype[i] & CPlugins::P_TYPE_TOOL) {
				if (!g_settings.plugins_tool.empty())
					g_settings.plugins_tool += ",";
				g_settings.plugins_tool +=  g_Plugins->getFileName(i);
				g_Plugins->setType(i, CPlugins::P_TYPE_TOOL);
			} else if (pltype[i] & CPlugins::P_TYPE_SCRIPT) {
				if (!g_settings.plugins_script.empty())
					g_settings.plugins_script += ",";
				g_settings.plugins_script +=  g_Plugins->getFileName(i);
				g_Plugins->setType(i, CPlugins::P_TYPE_SCRIPT);
			} else if (pltype[i] & CPlugins::P_TYPE_LUA) {
				if (!g_settings.plugins_lua.empty())
					g_settings.plugins_lua += ",";
				g_settings.plugins_lua +=  g_Plugins->getFileName(i);
				g_Plugins->setType(i, CPlugins::P_TYPE_LUA);
			}
		}
		g_Plugins->loadPlugins();
	}

	if (show_usermenu)
		for(std::vector<SNeutrinoSettings::usermenu_t *>::iterator it = g_settings.usermenu.begin(); it != g_settings.usermenu.end();) {
			if (!*it)
				it = g_settings.usermenu.erase(it);
			else
				++it;
		}

	delete pMenu;
	delete uMenu;
	delete pinChangeWidget;
	delete plMenu;
	delete user_menu_notifier;
	delete pin_setup_notifier;

	return res;
}

//init pin setup dialog
void CPersonalizeGui::ShowPinSetup(CMenuWidget* p_widget, CPINChangeWidget * &pin_widget)
{
	pin_widget = new CPINChangeWidget(LOCALE_PERSONALIZE_PINCODE, &g_settings.personalize_pincode, 4, LOCALE_PERSONALIZE_PINHINT);

	CMenuForwarder * fw_pin_setup = new CMenuForwarder(LOCALE_PERSONALIZE_PINCODE, true, g_settings.personalize_pincode, pin_widget, NULL, CRCInput::RC_red);
 	pin_setup_notifier = new CPinSetupNotifier(fw_pin_setup);
 	p_widget->addItem(new CMenuOptionChooser(LOCALE_PERSONALIZE_PIN_IN_USE, &g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, pin_setup_notifier));

	pin_setup_notifier->changeNotify();
	p_widget->addItem(fw_pin_setup);

	p_widget->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_PERSONALIZE_MENUCONFIGURATION));
}

static const struct button_label footerButtons[2] = {
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_BOUQUETEDITOR_DELETE },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_BOUQUETEDITOR_ADD }
};

//init usermenu items
void CPersonalizeGui::ShowUserMenu()
{
	int uMenu_shortcut = 1;

	uMenu->addIntroItems(LOCALE_USERMENUS_HEAD);

	uMenu->addItem(new CMenuOptionChooser(LOCALE_PERSONALIZE_USERMENU_SHOW_CANCEL, &g_settings.personalize[SNeutrinoSettings::P_UMENU_SHOW_CANCEL], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));

	uMenu->addItem(GenericMenuSeparatorLine);
	//define usermenu items
	std::vector<CMenuForwarder*> v_umenu_fw;
	for (uint i = 0; i<USERMENU_ITEMS_COUNT; i++)
	{
		CUserMenuSetup *cms = new CUserMenuSetup(usermenu[i].menue_title, usermenu[i].menue_button);
		CMenuDForwarder *fw = new CMenuDForwarder(usermenu[i].menue_title, true, g_settings.usermenu[i]->title, cms, g_settings.usermenu[i]->name.c_str(), /*usermenu[i].DirectKey*/CRCInput::RC_nokey /*, usermenu[i].IconName*/);
		cms->setCaller(fw);
		v_umenu_fw.push_back(fw);
	}
	user_menu_notifier = new CUserMenuNotifier(v_umenu_fw[0], v_umenu_fw[1], v_umenu_fw[2], v_umenu_fw[3]);

	int  buttons[USERMENU_ITEMS_COUNT] = { SNeutrinoSettings::P_MAIN_RED_BUTTON, SNeutrinoSettings::P_MAIN_GREEN_BUTTON, SNeutrinoSettings::P_MAIN_YELLOW_BUTTON, SNeutrinoSettings::P_MAIN_BLUE_BUTTON };
	for (int i = 0; i < USERMENU_ITEMS_COUNT; i++)
		uMenu->addItem(new CMenuOptionChooser(usermenu[i].menue_title, &g_settings.personalize[buttons[i]], PERSONALIZE_ACTIVE_MODE_OPTIONS, PERSONALIZE_ACTIVE_MODE_MAX, true, user_menu_notifier));

	//add usermenu items
	uMenu->addItem(new CMenuSeparator(CMenuSeparator::ALIGN_RIGHT | CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_USERMENU_NAME));
	user_menu_notifier->changeNotify();
	for (uint j = 0; j<USERMENU_ITEMS_COUNT; j++)
		uMenu->addItem(v_umenu_fw[j]);

	uMenu->addItem(GenericMenuSeparatorLine);
	//add plugin selection menu
	CMenuWidget *pluginSelectionMenu = new CMenuWidget(LOCALE_PERSONALIZE_USERMENU_PLUGIN_TYPES, NEUTRINO_ICON_SETTINGS);
	pluginSelectionMenu->addIntroItems(LOCALE_MAINMENU_SETTINGS);

	CMenuOptionChooser * /*oc = NULL;*/
	oc = new CMenuOptionChooser(LOCALE_MAINMENU_GAMES, &g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_GAMES], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	oc->setHint(NEUTRINO_ICON_HINT_PERSONALIZE, LOCALE_MENU_HINT_PLUGIN_TYPE_GAMES);
	pluginSelectionMenu->addItem(oc);

	oc = new CMenuOptionChooser(LOCALE_MAINMENU_TOOLS, &g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_TOOLS], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	oc->setHint(NEUTRINO_ICON_HINT_PERSONALIZE, LOCALE_MENU_HINT_PLUGIN_TYPE_TOOLS);
	pluginSelectionMenu->addItem(oc);

	oc = new CMenuOptionChooser(LOCALE_MAINMENU_SCRIPTS, &g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_SCRIPTS], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	oc->setHint(NEUTRINO_ICON_HINT_PERSONALIZE, LOCALE_MENU_HINT_PLUGIN_TYPE_SCRIPTS);
	pluginSelectionMenu->addItem(oc);

	oc = new CMenuOptionChooser(LOCALE_MAINMENU_LUA, &g_settings.personalize[SNeutrinoSettings::P_UMENU_PLUGIN_TYPE_LUA], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true);
	oc->setHint(NEUTRINO_ICON_HINT_PERSONALIZE, LOCALE_MENU_HINT_PLUGIN_TYPE_LUA);
	pluginSelectionMenu->addItem(oc);

	uMenu->addItem(new CMenuDForwarder(LOCALE_PERSONALIZE_USERMENU_PLUGIN_TYPES, true, NULL, pluginSelectionMenu, NULL, CRCInput::convertDigitToKey(uMenu_shortcut++)));

	uMenu->addItem(GenericMenuSeparatorLine);
	//non-standard usermenu keys
	customkey_offset = uMenu->getItemsCount();
	unsigned int ums = g_settings.usermenu.size();
	for (unsigned int i = USERMENU_ITEMS_COUNT; i < ums; i++)
		if (g_settings.usermenu[i]->key != CRCInput::RC_nokey) {
			CUserMenuSetup *cms = new CUserMenuSetup(LOCALE_USERMENU_HEAD, i);
			CMenuDForwarder *fw = new CMenuDForwarder(CRCInput::getKeyName(g_settings.usermenu[i]->key), true, g_settings.usermenu[i]->title, cms, to_string(i).c_str());
			cms->setCaller(fw);
			uMenu->addItem(fw);
		}

	uMenu->setFooter(footerButtons, 2);
	uMenu->addKey(CRCInput::RC_red, this, ">d");
	uMenu->addKey(CRCInput::RC_green, this, ">a");
}

#define PERSONALIZE_PLUGINTYPE_MAX 5
const CMenuOptionChooser::keyval PERSONALIZE_PLUGINTYPE_OPTIONS[PERSONALIZE_PLUGINTYPE_MAX] =
{
	{ CPlugins::P_TYPE_DISABLED, LOCALE_PLUGINTYPE_DISABLED },
	{ CPlugins::P_TYPE_GAME, LOCALE_PLUGINTYPE_GAME },
	{ CPlugins::P_TYPE_TOOL, LOCALE_PLUGINTYPE_TOOL },
	{ CPlugins::P_TYPE_SCRIPT, LOCALE_PLUGINTYPE_SCRIPT },
	{ CPlugins::P_TYPE_LUA, LOCALE_PLUGINTYPE_LUA }
};

//init plugin settings
void CPersonalizeGui::ShowPluginMenu(CMenuWidget* p_widget, std::string da[], int ia[])
{
	p_widget->addIntroItems(LOCALE_PERSONALIZE_PLUGINS);

	uint  d_key = 1;
	int pcount = g_Plugins->getNumberOfPlugins();
	for (int i = 0; i < pcount; i++)
	{
		ia[i] = g_Plugins->getType(i);
		da[i] = g_Plugins->getName(i);
		if (da[i].empty())
			continue;
		std::string pluginDesc = g_Plugins->getDescription(i);
		if (!pluginDesc.empty())
			da[i] += " (" + pluginDesc + ")";
		p_widget->addItem(new CMenuOptionChooser(da[i].c_str(), &ia[i], PERSONALIZE_PLUGINTYPE_OPTIONS, PERSONALIZE_PLUGINTYPE_MAX,
					!g_Plugins->isHidden(i), NULL, getShortcut(d_key++)));
	}
}

//Here we give the user the option to enable, disable, or PIN protect items on the Main Menu.
//We also provide a means of PIN protecting the menu itself.
int CPersonalizeGui::ShowMenuOptions(const int& widget)
{
	string mn_name = v_widget[widget]->getName();
	printf("[neutrino-personalize] exec %s for [%s]...\n", __FUNCTION__, mn_name.c_str());

	mn_widget_id_t w_index = widget+MN_WIDGET_ID_PERSONALIZE_MAIN;
	CMenuWidget* pm = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width, w_index);
	//reuqired in changeNotify()
	options_count = 0;
	tmpW = pm;
	//*************************

	//subhead
	CMenuSeparator * pm_subhead = new CMenuSeparator(CMenuSeparator::ALIGN_LEFT | CMenuSeparator::SUB_HEAD | CMenuSeparator::STRING);
	string 	s_sh = g_Locale->getText(LOCALE_PERSONALIZE_ACCESS);
	s_sh += ": " + mn_name;
	pm_subhead->setName(s_sh);

	pm->addItem(pm_subhead);
	pm->addIntroItems();

	//add all needed items
	for (uint i = 0; i<v_item.size(); i++)
	{
		if (mn_name == v_item[i].widget->getName())
		{
			int i_mode = v_item[i].item_mode;

			if (i_mode != PERSONALIZE_SHOW_NO)
			{
				//add items to the options menu
				if (i_mode == PERSONALIZE_SHOW_AS_ITEM_OPTION)
				{
					if (v_item[i].personalize_mode != NULL) //option chooser
					{
						//get locale name and personalize mode
						neutrino_locale_t name = v_item[i].locale_name;
						int* p_mode = v_item[i].personalize_mode;

						//found observer item and if found, then define 'this' as observer for current option chooser and run changeNotify
						bool is_observer = isObserver(v_item[i].widget, v_item[i].menuItem) ? true : false;
						CChangeObserver* observer = is_observer ? this : NULL;
						CMenuOptionChooser * opt = new CMenuOptionChooser(name, p_mode, PERSONALIZE_MODE_OPTIONS, PERSONALIZE_MODE_MAX, v_item[i].menuItem->current_active || is_observer, observer);
						if (is_observer)
							changeNotify(name, (void*)p_mode);

						//required for first view: check active mode of option chooser and disable if it's an observed item and item mode is set to 'not visible'
						for (uint j = 0; j < v_observ.size(); j++)
							if (opt->getName()== g_Locale->getText(v_observ[j].to_observ_locale) && *p_mode == PERSONALIZE_MODE_NOTVISIBLE)
								opt->setActive(false);

						pm->addItem(opt); //add option chooser
					}
					else
						pm->addItem(v_item[i].menuItem); //separator
				}

				//pin protected items only
				if (i_mode == PERSONALIZE_SHOW_AS_ACCESS_OPTION)
				{
					string 	itm_name = g_Locale->getText(v_item[i].locale_name);
					itm_name += " ";
					itm_name += g_Locale->getText(LOCALE_PERSONALIZE_PINSTATUS);

					if (v_item[i].personalize_mode != NULL)
						pm->addItem(new CMenuOptionChooser(itm_name.c_str(), v_item[i].personalize_mode, PERSONALIZE_PROTECT_MODE_OPTIONS, PERSONALIZE_PROTECT_MODE_MAX, v_item[i].menuItem->active));
				}

				//only show in personalize menu, usefull to hide separators in menu, but visible only in personalizing menu
				if (i_mode == PERSONALIZE_SHOW_ONLY_IN_PERSONALIZE_MENU)
					pm->addItem(v_item[i].menuItem);
			}
		}

	}
	options_count = pm->getItemsCount();
	int res = pm->exec (NULL, "");
	delete pm;
	return res;
}

//returns true, if found an observer item
bool CPersonalizeGui::isObserver(CMenuWidget* widget, CMenuItem *item)
{
	for (uint i = 0; i < v_observ.size(); i++)
	{
		if (v_observ[i].widget == widget)
		{
			CMenuForwarder* fw = static_cast <CMenuForwarder*> (item);
			if (fw->getTextLocale() == v_observ[i].observer_locale)
				return true;
		}
	}
	return false;
}


bool CPersonalizeGui::changeNotify(const neutrino_locale_t locale, void *data)
{
	int opt_val = *(int*) data;

	//exit if no options found
	int opt_count = options_count;
	if (opt_count == 0 && locale == NONEXISTANT_LOCALE)
		return true;

	//if found an option and handle
	for (int i = 0; i < opt_count; i++){

		//get current item
		CMenuItem* item = tmpW->getItem(i);
		if (item->isMenueOptionChooser())
		{
			//if found an optionchooser, then extract option name
			CMenuOptionChooser* chooser = static_cast <CMenuOptionChooser*> (item);
			string opt_name = chooser->getName();

			for (uint j = 0; j < v_observ.size(); j++)
			{
				//if found the same option name for an observer item then...
				if (locale == v_observ[j].observer_locale)
				{
					//...compare for observed item
					if (opt_name == g_Locale->getText(v_observ[j].to_observ_locale))
					{
						//and if found an observed item, then set properties
						if (opt_val == PERSONALIZE_MODE_VISIBLE || opt_val == PERSONALIZE_MODE_PIN)
						{
							chooser->setActive(false);
							chooser->setOption(PERSONALIZE_MODE_NOTVISIBLE);
						}else{
							chooser->setActive(true);
							chooser->setOption(PERSONALIZE_MODE_VISIBLE);
						}
						chooser->paint(false);
					}
				}
			}
		}
	}
	return false;
}

//shows a short help message
void CPersonalizeGui::ShowHelpPersonalize()
{
	Helpbox helpbox(g_Locale->getText(LOCALE_PERSONALIZE_HELP));

	for (int i = (int)LOCALE_PERSONALIZE_HELP_LINE1; i<= (int)LOCALE_PERSONALIZE_HELP_LINE8; i++)
		helpbox.addLine(g_Locale->getText((neutrino_locale_t)i));

	helpbox.addExitKey(CRCInput::RC_ok);
	helpbox.show();
	helpbox.exec();
	helpbox.hide();
}

void CPersonalizeGui::ApplySettings()
{
	// replace old settings with new settings
	for (uint i = 0; i < v_int_settings.size(); i++)
		v_int_settings[i].old_val = *v_int_settings[i].p_val;
	for (int i = 0; i<(widget_count); i++)
		v_widget[i]->resetWidget();

	addPersonalizedItems();
}

void CPersonalizeGui::SaveAndExit()
{
	// Save the settings and left menu, if user wants to!
	if (haveChangedSettings())
	{
		if (ShowMsg(LOCALE_PERSONALIZE_HEAD, g_Locale->getText(LOCALE_PERSONALIZE_APPLY_SETTINGS), CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_QUESTION) == CMsgBox::mbrYes)
		{
			CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MAINSETTINGS_SAVESETTINGSNOW_HINT)); // UTF-8
			hintBox.paint();
			ApplySettings();
			hintBox.hide();
		}
		else
		{
			if (ShowMsg(LOCALE_PERSONALIZE_HEAD, g_Locale->getText(LOCALE_MESSAGEBOX_DISCARD), CMsgBox::mbrNo, CMsgBox::mbYes | CMsgBox::mbNo, NEUTRINO_ICON_QUESTION) == CMsgBox::mbrYes)
				exec(NULL, "restore");
		}
	}
}



//adds a menu widget to v_widget and sets the count of available widgets
void CPersonalizeGui::addWidget(CMenuWidget *widget)
{
	v_widget.push_back(widget);
	widget_count = v_widget.size();
}

//adds a group of menu widgets to v_widget and sets the count of available widgets
void CPersonalizeGui::addWidgets(const struct mn_widget_t * const widget, const int& widgetCount)
{
	for (int i = 0; i<(widgetCount); i++)
		addWidget(new CMenuWidget(widget[i].locale_text, widget[i].icon, widget[i].width));
}

//returns a menu widget from v_widget
CMenuWidget& CPersonalizeGui::getWidget(const int& id)
{
	return *v_widget[id];
}

//returns index of menu widget from 'v_widget'
int CPersonalizeGui::getWidgetId(CMenuWidget *widget)
{
	for (int i = 0; i<widget_count; i++)
		if (v_widget[i] == widget)
			return i;

	return -1;
}

void CPersonalizeGui::addObservedItem(CMenuWidget *widget, CMenuItem* observer_Item, CMenuItem* to_observ_Item)
{
	CMenuForwarder *fw[2] = {	static_cast <CMenuForwarder*> (observer_Item),
					static_cast <CMenuForwarder*> (to_observ_Item)};
	observ_menu_item_t item = {widget, fw[0]->getTextLocale(), fw[1]->getTextLocale()};
	v_observ.push_back(item);
}


//adds non personalized menu intro items objects with separator, back button and separator line to menu without personalizing parameters
void CPersonalizeGui::addIntroItems(const int& widget_id)
{
	addIntroItems(v_widget[widget_id]);
}

void CPersonalizeGui::addIntroItems(CMenuWidget *widget)
{
	addItem(widget, GenericMenuSeparator, 		NULL, false, PERSONALIZE_SHOW_NO);
	addItem(widget, GenericMenuBack, 		NULL, false, PERSONALIZE_SHOW_NO);
	addItem(widget, GenericMenuSeparatorLine, 	NULL, false, PERSONALIZE_SHOW_NO);
}


//overloaded version from 'addItem', first parameter is an id from widget collection 'v_widget'
void CPersonalizeGui::addItem(const int& widget_id, CMenuItem *menu_Item, const int *personalize_mode, const bool defaultselected, const int& item_mode, CMenuItem *observer_Item, const menu_item_disable_cond_t& disable_condition)
{
	addItem(v_widget[widget_id], menu_Item, personalize_mode, defaultselected, item_mode, observer_Item, disable_condition);
}

//adds a personalized menu item object to menu with personalizing parameters
void CPersonalizeGui::addItem(CMenuWidget *widget, CMenuItem *menu_Item, const int *personalize_mode, const bool defaultselected, const int& item_mode, CMenuItem *observer_Item, const menu_item_disable_cond_t& disable_condition)
{
	if (observer_Item != NULL)
		addObservedItem(widget, observer_Item, menu_Item);

	CMenuForwarder *fw = static_cast <CMenuForwarder*> (menu_Item);

	menu_item_t item = {widget, menu_Item, defaultselected, fw->getTextLocale(), (int*)personalize_mode, item_mode, observer_Item, disable_condition};

	if (item_mode == PERSONALIZE_SHOW_AS_ACCESS_OPTION)
	{
		v_item.push_back(item);
		handleSetting((int*)personalize_mode);
	}
	else if (personalize_mode != NULL)
	{
		v_item.push_back(item);
		if (item_mode != PERSONALIZE_SHOW_NO) //handle only relevant items
			handleSetting((int*)personalize_mode);
	}
	else if (personalize_mode == NULL)
		v_item.push_back(item);
}

//overloaded version from 'addSeparator', first parameter is an id from widget collection 'v_widget',
void CPersonalizeGui::addSeparator(const int& widget_id, const neutrino_locale_t locale_text, const int& item_mode)
{
	addSeparator(*v_widget[widget_id], locale_text, item_mode);
}

//adds a menu separator to menue, based upon GenericMenuSeparatorLine or CMenuSeparator objects with locale
//expands with parameter within you can show or hide this item in personalize options
void CPersonalizeGui::addSeparator(CMenuWidget &widget, const neutrino_locale_t locale_text, const int& item_mode)
{
	if (locale_text == NONEXISTANT_LOCALE) {
		menu_item_t to_add_sep = {&widget, GenericMenuSeparatorLine, false, locale_text, NULL, item_mode, NULL, DCOND_MODE_NONE };
		v_item.push_back(to_add_sep);
	} else {
		menu_item_t to_add_sep = {&widget, new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, locale_text), false, locale_text, NULL, item_mode, NULL, DCOND_MODE_NONE };
		v_item.push_back(to_add_sep);
	}
}

//returns available count of personalized item for definied widget
int CPersonalizeGui::getItemsCount(CMenuWidget *widget)
{
	int ret = 0;

	for (uint i = 0; i < v_item.size(); i++)
		if (v_item[i].widget == widget)
			ret++;

	return ret;
}

int CPersonalizeGui::getItemsCount(const int& widget_id)
{
	return getItemsCount(v_widget[widget_id]);
}

//paint all available personalized menu items and separators to menu
//this replaces all collected actual and handled "widget->addItem()" tasks at once
void CPersonalizeGui::addPersonalizedItems()
{
	bool allow_sep = true;
	int old_w_id = 0;
	int widget_id = 0;
	int short_cut = shortcut;
	int old_p_mode = PERSONALIZE_MODE_NOTVISIBLE;

	//get current mode of personlize itself
	int in_pinmode = hasPinItems() ? PERSONALIZE_PROTECT_MODE_PIN_PROTECTED : PERSONALIZE_PROTECT_MODE_NOT_PROTECTED;

	for (uint i = 0; i < v_item.size(); i++)
	{
		int i_mode = v_item[i].item_mode;

		if (i_mode != PERSONALIZE_SHOW_ONLY_IN_PERSONALIZE_MENU) //skip if item only used in personalize settings
		{
			widget_id = getWidgetId(v_item[i].widget);

			if (old_w_id != widget_id)
			{
				//reset shortcut if widget has changed
				short_cut = shortcut;

				//normalize previous widget: remove last item, if it is a separator line
				uint items_count = v_item[old_w_id].widget->getItemsCount();
				if (v_item[old_w_id].widget->getItem(items_count-1) == GenericMenuSeparatorLine)
					v_item[old_w_id].widget->removeItem(items_count-1);

				allow_sep = true;
			}

			if (v_item[i].personalize_mode != NULL) //handle personalized item and non separator
			{
				CMenuForwarder *fw = static_cast <CMenuForwarder*> (v_item[i].menuItem);

				bool use_pin 		= false;
				int p_mode 		= *v_item[i].personalize_mode;
				neutrino_msg_t d_key 	= fw->directKey;
				bool add_shortcut 	= false;


				//get shortcut
				if (fw->active && (d_key == CRCInput::RC_nokey || CRCInput::isNumeric(d_key))) //if RC_nokey  or RC_key is digi and item is active, allow to generate a shortcut,
				{
					add_shortcut = true;
					d_key = getShortcut(short_cut);
				}

				//set pin mode if required
				const char* lock_icon = NULL;
				if (p_mode == PERSONALIZE_MODE_PIN || (p_mode == PERSONALIZE_PROTECT_MODE_PIN_PROTECTED && i_mode == PERSONALIZE_SHOW_AS_ACCESS_OPTION))
					use_pin = true;

				//set pinmode for personalize menu or for settings manager menu and if any item is pin protected
				if ((in_pinmode && !use_pin)){
					if (v_item[i].personalize_mode == &g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS] || v_item[i].personalize_mode == &g_settings.personalize[SNeutrinoSettings::P_MSET_SETTINGS_MANAGER])
					{
						use_pin = true;
						lock_icon = NEUTRINO_ICON_LOCK_PASSIVE;
					}
				}

				//convert item to locked forwarder and use generated pin mode for usage as ask parameter
				v_item[i].menuItem = new CLockedMenuForwarder(fw->getTextLocale(),
						g_settings.personalize_pincode,
						use_pin, fw->active, NULL, fw->getTarget(), fw->getActionKey(), d_key, NULL, lock_icon);
				v_item[i].menuItem->hintIcon = fw->hintIcon;
				v_item[i].menuItem->hint = fw->hint;

				/*
				 * assign of visualized lock mode with lock icons for 'personalize menu' itself,
				 * required menu item is identified with relatetd locale that is used inside the settings menu
				*/
				if (fw->getTextLocale() == LOCALE_PERSONALIZE_HEAD){
					lock_icon = g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS] ? NEUTRINO_ICON_LOCK : (in_pinmode ? NEUTRINO_ICON_LOCK_PASSIVE : NULL);
					v_item[i].menuItem->setInfoIconRight(lock_icon);
				}

				//assign slot for items, causes disable/enable by condition eg: receiver mode
				if (v_item[i].condition != DCOND_MODE_NONE ){
					sigc::slot0<void> sl = sigc::bind<0>(sigc::mem_fun1(v_item[i].menuItem, &CMenuForwarder::disableByCondition), v_item[i].condition);
					v_item[i].menuItem->OnPaintItem.connect(sl);
					v_item[i].default_selected = false;

					//value of current_active for personalized item must have value of current settings state here!
					v_item[i].menuItem->current_active = (p_mode || i_mode);
				}

				//add item if it's set to visible or pin protected and allow to add an forwarder as next
				if (p_mode != PERSONALIZE_MODE_NOTVISIBLE || i_mode == PERSONALIZE_SHOW_AS_ACCESS_OPTION)
				{
					//add item
					v_item[i].widget->addItem(v_item[i].menuItem, v_item[i].default_selected); //forwarders...
					allow_sep = true;

					//generate shortcut for next item
					if (add_shortcut) {
						short_cut++;
						v_item[i].widget->setNextShortcut(short_cut);
					}
				}
				else if (p_mode == PERSONALIZE_MODE_NOTVISIBLE)
				{
					//allow separator as next if personalize mode was changed
					if (p_mode != old_p_mode)
					{
						old_p_mode = p_mode;
						allow_sep = true;
					}
 				}

				delete fw;
			}
			else //handle and add separator as non personalized item and don't allow to add a separator as next but allow back button as next
			{
				if (allow_sep || v_item[i].menuItem == GenericMenuBack)
				{
					v_item[i].widget->addItem(v_item[i].menuItem, v_item[i].default_selected); //separators
					allow_sep = v_item[i].menuItem == GenericMenuBack ? true : false;
				}
			}
		}
		old_w_id = widget_id;
	}
}


// returns RC_key depends of shortcut between key number 1 to 0, 10 returns 0, >10 returns no key
// parameter alternate_rc_key allows using an alternate key, default key is RC_nokey
neutrino_msg_t CPersonalizeGui::getShortcut(const int & shortcut_num, neutrino_msg_t alternate_rc_key)
{
	if (shortcut_num < 10)
		return CRCInput::convertDigitToKey(shortcut_num);
	else if (shortcut_num == 10)
		return CRCInput::RC_0;
	else
		return alternate_rc_key;
}

//handle/collects old int settings
void  CPersonalizeGui::handleSetting(int *setting)
{
	settings_int_t val	= {*setting, setting};
	v_int_settings.push_back(val);
}

//check for setup changes
bool  CPersonalizeGui::haveChangedSettings()
{
	//compare old settings with current settings
	for (uint i = 0; i < v_int_settings.size(); i++)
		if (v_int_settings[i].old_val != *v_int_settings[i].p_val)
			return true;

	return false;
}

//checks for pin protected item
bool  CPersonalizeGui::hasPinItems()
{
	for (uint i = 0; i < v_int_settings.size(); i++)
		if (*v_int_settings[i].p_val == PERSONALIZE_MODE_PIN || *v_int_settings[i].p_val == PERSONALIZE_PROTECT_MODE_PIN_PROTECTED)
		{
			printf("[neutrino personalize] %s found pin protected item...[item %d]\n", __FUNCTION__, i);
			return true;
		}

	return false;
}

//restore old settings
void CPersonalizeGui::restoreSettings()
{
	//restore settings with current settings
	for (uint i = 0; i < v_int_settings.size(); i++)
		*v_int_settings[i].p_val = v_int_settings[i].old_val;

	exec(NULL, "restore");
}


//helper class to enable/disable some items in usermenu setup
CUserMenuNotifier::CUserMenuNotifier( CMenuItem* i1, CMenuItem* i2, CMenuItem *i3, CMenuItem *i4)
{
	toDisable[0]=i1;
	toDisable[1]=i2;
	toDisable[2]=i3;
	toDisable[3]=i4;
}

bool CUserMenuNotifier::changeNotify(const neutrino_locale_t, void *)
{
	toDisable[0]->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_RED_BUTTON]);
	toDisable[1]->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_GREEN_BUTTON]);
	toDisable[2]->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_YELLOW_BUTTON]);
	toDisable[3]->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_BLUE_BUTTON]);

	return false;
}

//helper class to enable/disable pin setup
CPinSetupNotifier::CPinSetupNotifier( CMenuItem* item)
{
	toDisable=item;
}

bool CPinSetupNotifier::changeNotify(const neutrino_locale_t, void *)
{
	toDisable->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS]);

	//   return g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS];
	return false;
}
