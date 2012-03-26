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

        This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
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
#include "widget/messagebox.h"
#include "widget/hintbox.h"
#include "widget/lcdcontroler.h"
#include "widget/keychooser.h"
#include "color.h"
#include "personalize.h"

#include <system/settings.h>

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
	show_pin_setup = false;
	user_menu_notifier = NULL;
	pin_setup_notifier = NULL;
	fkeyMenu = NULL;
	plMenu = NULL;
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

	if (parent)
		parent->hide();

	for (int i = 0; i<(widget_count); i++)
	{
		ostringstream i_str;
		i_str << i;
		string s(i_str.str());
		string a_key = s;
		
		if(actionKey == a_key) 
		{                                     				// Personalize options menu
			ShowMenuOptions(i);
			return res;
		}
	}
		
	if (actionKey=="personalize_help") {                                    // Personalize help
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
	
	if ( is_pin_protected || hasPinItems()){
		setHint(LOCALE_PERSONALIZE_PINHINT); //from CPINProtection
		if (check())
			is_pin_protected = false;
	}
	if (!is_pin_protected){
		res = ShowPersonalizationMenu();                                 // Show main Personalization Menu
		SaveAndExit();
	}
	
	return res;
}

const CMenuOptionChooser::keyval OPTIONS_FEAT_KEY_OPTIONS[CPersonalizeGui::PERSONALIZE_FEAT_KEY_MAX] =
{
	{ CPersonalizeGui::PERSONALIZE_FEAT_KEY_RED		, LOCALE_PERSONALIZE_BUTTON_RED 	},
	{ CPersonalizeGui::PERSONALIZE_FEAT_KEY_GREEN		, LOCALE_PERSONALIZE_BUTTON_GREEN 	},
	{ CPersonalizeGui::PERSONALIZE_FEAT_KEY_YELLOW		, LOCALE_PERSONALIZE_BUTTON_YELLOW 	},
	{ CPersonalizeGui::PERSONALIZE_FEAT_KEY_BLUE		, LOCALE_PERSONALIZE_BUTTON_BLUE 	},
	{ CPersonalizeGui::PERSONALIZE_FEAT_KEY_AUTO		, LOCALE_PERSONALIZE_BUTTON_AUTO	}
};
//This is the main personalization menu. From here we can go to the other sub-menu's and enable/disable
//the PIN code feature, as well as determine whether or not the EPG menu/Features menu is accessible.
int CPersonalizeGui::ShowPersonalizationMenu()
{
	width = w_max (40, 10);
	
	CMenuWidget* pMenu = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width, MN_WIDGET_ID_PERSONALIZE);
	pMenu->addIntroItems(NONEXISTANT_LOCALE, LOCALE_PERSONALIZE_ACCESS);
	
	//pin
	CPINChangeWidget *pinChangeWidget = NULL;
	if (show_pin_setup)
		ShowPinSetup(pMenu, pinChangeWidget);

	//personalized menues
	CMenuForwarderNonLocalized *p_mn[widget_count];
	for (int i = 0; i<(widget_count); i++)
	{
		ostringstream i_str;
		i_str << i;
		string s(i_str.str());
		string action_key = s;
		string mn_name = v_widget[i]->getName();
		p_mn[i] = new CMenuForwarderNonLocalized(mn_name.c_str(), true, NULL, this, action_key.c_str(), CRCInput::convertDigitToKey(i+1));
		pMenu->addItem(p_mn[i]);
	}
	

	//usermenu
	CMenuWidget* uMenu = NULL;
	vector<CUserMenuSetup*> v_userMenuSetup;
	if (show_usermenu)
	{
		pMenu->addItem(GenericMenuSeparatorLine);
		uMenu = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width, MN_WIDGET_ID_PERSONALIZE_USERMENU);
		pMenu->addItem(new CMenuForwarder(LOCALE_USERMENU_HEAD, true, NULL, uMenu, NULL, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN));
	
		ShowUserMenu(uMenu, v_userMenuSetup);
	}
	
	
	//help
	pMenu->addItem(GenericMenuSeparatorLine);
	pMenu->addItem(new CMenuForwarder(LOCALE_PERSONALIZE_HELP, true, NULL, this, "personalize_help", CRCInput::RC_help, NEUTRINO_ICON_BUTTON_HELP));
	
	int res = pMenu->exec(NULL, "");
	pMenu->hide();
	delete pMenu;
	delete uMenu;
	delete pinChangeWidget;
	delete fkeyMenu;
	delete plMenu;
	for(vector<CUserMenuSetup*>::iterator it = v_userMenuSetup.begin(); it != v_userMenuSetup.end(); ++it)
		delete *it;
	v_userMenuSetup.clear();
	delete user_menu_notifier;
	delete pin_setup_notifier;
	
	return res;
}

//init pin setup dialog
void CPersonalizeGui::ShowPinSetup(CMenuWidget* p_widget, CPINChangeWidget *pin_widget)
{
	pin_widget = new CPINChangeWidget(LOCALE_PERSONALIZE_PINCODE, g_settings.personalize_pincode, 4, LOCALE_PERSONALIZE_PINHINT);
	
	CMenuForwarder * fw_pin_setup = new CMenuForwarder(LOCALE_PERSONALIZE_PINCODE, true, g_settings.personalize_pincode, pin_widget, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
 	pin_setup_notifier = new CPinSetupNotifier(fw_pin_setup);
 	p_widget->addItem(new CMenuOptionChooser(LOCALE_PERSONALIZE_PIN_IN_USE, &g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, pin_setup_notifier));
	
	fw_pin_setup->setActive(pin_setup_notifier->changeNotify());
	p_widget->addItem(fw_pin_setup);
	
	p_widget->addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_PERSONALIZE_MENUCONFIGURATION));
}



//init preverred key setup 
void CPersonalizeGui::ShowPreverredKeySetup(CMenuWidget* p_widget)
{
	p_widget->addIntroItems(LOCALE_PERSONALIZE_USERMENU_PREFERRED_BUTTONS);
	
	p_widget->addItem(new CMenuOptionChooser(LOCALE_FAVORITES_MENUEADD, 	&g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_FAVORIT], OPTIONS_FEAT_KEY_OPTIONS, CPersonalizeGui::PERSONALIZE_FEAT_KEY_MAX, true));
	p_widget->addItem(new CMenuOptionChooser(LOCALE_TIMERLIST_NAME,		&g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_TIMERLIST], OPTIONS_FEAT_KEY_OPTIONS, CPersonalizeGui::PERSONALIZE_FEAT_KEY_MAX, true));
	p_widget->addItem(new CMenuOptionChooser(LOCALE_USERMENU_ITEM_VTXT, 	&g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_VTXT], OPTIONS_FEAT_KEY_OPTIONS, CPersonalizeGui::PERSONALIZE_FEAT_KEY_MAX, true));
	p_widget->addItem(new CMenuOptionChooser(LOCALE_RCLOCK_MENUEADD, 	&g_settings.personalize[SNeutrinoSettings::P_FEAT_KEY_RC_LOCK], OPTIONS_FEAT_KEY_OPTIONS, CPersonalizeGui::PERSONALIZE_FEAT_KEY_MAX, true));
	
// 	//plugins
// 	plMenu = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width);
// 	CMenuForwarder *fw_plugins = new CMenuForwarder(LOCALE_PERSONALIZE_PLUGINS, true, NULL, plMenu, NULL, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED);
// 	p_widget->addItem(GenericMenuSeparatorLine);
// 	p_widget->addItem(fw_plugins);
// 	ShowPluginMenu(plMenu);
}


//init usermenu items
void CPersonalizeGui::ShowUserMenu(CMenuWidget* p_widget, vector<CUserMenuSetup*> &v_umenu)
{
	p_widget->addIntroItems(LOCALE_USERMENU_HEAD);
	
	//define usermenu items
	vector<CMenuForwarder*> v_umenu_fw;
	for (uint i = 0; i<USERMENU_ITEMS_COUNT; i++)
	{
		v_umenu.push_back(new CUserMenuSetup(usermenu[i].menue_title, usermenu[i].menue_button));
		
		//ensure correct default string, e.g: required after settings reset
		if (v_umenu[i]->getUsedItemsCount() > 0)
			g_settings.usermenu_text[i] = g_settings.usermenu_text[i].empty() ? g_Locale->getText(usermenu[i].def_name) : g_settings.usermenu_text[i].c_str();
		
		v_umenu_fw.push_back(new CMenuForwarder(usermenu[i].menue_title, true, g_settings.usermenu_text[i], v_umenu[i], NULL, usermenu[i].DirectKey, usermenu[i].IconName));
	}
#if 0		
	//feature keys
	fkeyMenu = new CMenuWidget(LOCALE_PERSONALIZE_HEAD, NEUTRINO_ICON_PERSONALIZE, width, MN_WIDGET_ID_PERSONALIZE_FEATUREKEYS);
	CMenuForwarder *fw_fkeys = new CMenuForwarder(LOCALE_PERSONALIZE_USERMENU_PREFERRED_BUTTONS, true, NULL, fkeyMenu, NULL, CRCInput::RC_1);
#endif	
	//enable/disable epg/features
	user_menu_notifier = new CUserMenuNotifier(v_umenu_fw[0], v_umenu_fw[3]);
	//red 
	p_widget->addItem(new CMenuOptionChooser(usermenu[0].menue_title, &g_settings.personalize[SNeutrinoSettings::P_MAIN_RED_BUTTON], PERSONALIZE_ACTIVE_MODE_OPTIONS, PERSONALIZE_ACTIVE_MODE_MAX, true, user_menu_notifier));/*LOCALE_INFOVIEWER_EVENTLIST*/
	//blue 
	p_widget->addItem(new CMenuOptionChooser(usermenu[3].menue_title, &g_settings.personalize[SNeutrinoSettings::P_MAIN_BLUE_BUTTON], PERSONALIZE_ACTIVE_MODE_OPTIONS, PERSONALIZE_ACTIVE_MODE_MAX, true, user_menu_notifier));/*LOCALE_INFOVIEWER_STREAMINFO*/
	
	//add usermenu items
	p_widget->addItem(new CMenuSeparator(CMenuSeparator::ALIGN_RIGHT | CMenuSeparator::LINE | CMenuSeparator::STRING, LOCALE_USERMENU_NAME));
	user_menu_notifier->changeNotify();
	for (uint j = 0; j<USERMENU_ITEMS_COUNT; j++)
		p_widget->addItem(v_umenu_fw[j]);
	
	p_widget->addItem(GenericMenuSeparator);
#if 0	
	//preverred keys
	p_widget->addItem(GenericMenuSeparatorLine);
	p_widget->addItem(fw_fkeys);
	ShowPreverredKeySetup(fkeyMenu);
#endif	
	p_widget->addItem(GenericMenuSeparatorLine);
	p_widget->addItem(new CMenuOptionChooser(LOCALE_PERSONALIZE_USERMENU_SHOW_CANCEL, &g_settings.personalize[SNeutrinoSettings::P_UMENU_SHOW_CANCEL], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true));
}


//init plugin settings
void CPersonalizeGui::ShowPluginMenu(CMenuWidget* p_widget)
{
	p_widget->addIntroItems(LOCALE_PERSONALIZE_PLUGINS);
	
	uint  d_key = 1;	
	for (int i = 0; i<g_PluginList->getNumberOfPlugins(); i++)
	{
		if( g_PluginList->getType(i)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(i)) //don't show hidden plugins an games
		{
			p_widget->addItem(new CMenuForwarderNonLocalized(g_PluginList->getName(i), true, g_PluginList->getDescription(i), NULL, NULL, getShortcut(d_key)));
			d_key++;
		}
	}
}

//Here we give the user the option to enable, disable, or PIN protect items on the Main Menu.
//We also provide a means of PIN protecting the menu itself.
void CPersonalizeGui::ShowMenuOptions(const int& widget)
{
	string mn_name = v_widget[widget]->getName();
	printf("[neutrino-personalize] exec %s...\n", __FUNCTION__);
	
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
	pm_subhead->setString(s_sh);
	
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
						CMenuOptionChooser * opt = new CMenuOptionChooser(name, p_mode, PERSONALIZE_MODE_OPTIONS, PERSONALIZE_MODE_MAX, v_item[i].menuItem->active, observer);
						if (is_observer)
							changeNotify(name, (void*)p_mode);
										
						//required for first view: check active mode of option chooser and disable if it's an observed item and item mode is set to 'not visible'
						for (uint j = 0; j < v_observ.size(); j++)		
							if (opt->getOptionName()== g_Locale->getText(v_observ[j].to_observ_locale) && *p_mode == PERSONALIZE_MODE_NOTVISIBLE)
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
	pm->exec (NULL, "");
	pm->hide ();
	delete pm;

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
			string opt_name = chooser->getOptionName();

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
							chooser->setOptionValue(PERSONALIZE_MODE_NOTVISIBLE);
						}else{
							chooser->setActive(true);
							chooser->setOptionValue(PERSONALIZE_MODE_VISIBLE);
						}
					}
				}
			}
		}
	}
	return true;
}

//shows a short help message
void CPersonalizeGui::ShowHelpPersonalize()
{
	Helpbox helpbox;
	
	for (int i = (int)LOCALE_PERSONALIZE_HELP_LINE1; i<= (int)LOCALE_PERSONALIZE_HELP_LINE8; i++)
		helpbox.addLine(g_Locale->getText((neutrino_locale_t)i));


	helpbox.show(LOCALE_PERSONALIZE_HELP);
}

void CPersonalizeGui::SaveAndExit()
{
	// Save the settings and left menu, if user wants to!
	if (haveChangedSettings())
	{
		if (ShowMsgUTF(LOCALE_PERSONALIZE_HEAD, g_Locale->getText(LOCALE_PERSONALIZE_APPLY_SETTINGS), CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_QUESTION) == CMessageBox::mbrYes)
		{
			CHintBox hintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MAINSETTINGS_SAVESETTINGSNOW_HINT)); // UTF-8
			hintBox.paint();
			// replace old settings with new settings
			for (uint i = 0; i < v_int_settings.size(); i++)
				v_int_settings[i].old_val = *v_int_settings[i].p_val;
			//CNeutrinoApp::getInstance()->saveSetup();

			for (int i = 0; i<(widget_count); i++)
				v_widget[i]->resetWidget();
			
			addPersonalizedItems();
			hintBox.hide();
		}
		else
		{
			if (ShowMsgUTF(LOCALE_PERSONALIZE_HEAD, g_Locale->getText(LOCALE_MESSAGEBOX_DISCARD), CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo, NEUTRINO_ICON_QUESTION) == CMessageBox::mbrYes)
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
void CPersonalizeGui::addItem(const int& widget_id, CMenuItem *menu_Item, const int *personalize_mode, const bool defaultselected, const int& item_mode, CMenuItem *observer_Item)
{
	addItem(v_widget[widget_id], menu_Item, personalize_mode, defaultselected, item_mode, observer_Item);
}

//adds a personalized menu item object to menu with personalizing parameters
void CPersonalizeGui::addItem(CMenuWidget *widget, CMenuItem *menu_Item, const int *personalize_mode, const bool defaultselected, const int& item_mode, CMenuItem *observer_Item)
{
	if (observer_Item != NULL)
		addObservedItem(widget, observer_Item, menu_Item);
	
 	CMenuForwarder *fw = static_cast <CMenuForwarder*> (menu_Item);
	
	menu_item_t item = {widget, menu_Item, defaultselected, fw->getTextLocale(), (int*)personalize_mode, item_mode, observer_Item};

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
	menu_item_t to_add_sep[2] = {	{&widget, GenericMenuSeparatorLine, false, locale_text, NULL, item_mode, NULL}, 
					{&widget, new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, locale_text), false, locale_text, NULL, item_mode, NULL}};
	
	if (locale_text == NONEXISTANT_LOCALE)
		v_item.push_back(to_add_sep[0]);
	else
		v_item.push_back(to_add_sep[1]);
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
				if (fw->iconName.empty() && fw->active ) //if no icon is defined and item is active, allow to generate a shortcut, 
				{
					add_shortcut = true;
					d_key = getShortcut(short_cut);
				}					
				
				//set pin mode if required
				const char* lock_icon = NULL;
				if (p_mode == PERSONALIZE_MODE_PIN || (p_mode == PERSONALIZE_PROTECT_MODE_PIN_PROTECTED && i_mode == PERSONALIZE_SHOW_AS_ACCESS_OPTION))
					use_pin = true;
				
				//set pinmode for personalize menu or for settings manager menu and if any item is pin protected 
				if (in_pinmode && !use_pin)		
					if (v_item[i].personalize_mode == &g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS] || v_item[i].personalize_mode == &g_settings.personalize[SNeutrinoSettings::P_MSET_SETTINGS_MANAGER])
					{
						use_pin = true;
						lock_icon = NEUTRINO_ICON_LOCK_PASSIVE;
					}
											
				//convert item to locked forwarder and use generated pin mode for usage as ask parameter 
				v_item[i].menuItem = new CLockedMenuForwarder(fw->getTextLocale(), g_settings.personalize_pincode, use_pin, fw->active, NULL, fw->getTarget(), fw->getActionKey().c_str(), d_key, fw->iconName.c_str(), lock_icon);
				
				//add item if it's set to visible or pin protected and allow to add an forwarder as next
				if (v_item[i].menuItem->active && (p_mode != PERSONALIZE_MODE_NOTVISIBLE || i_mode == PERSONALIZE_SHOW_AS_ACCESS_OPTION))
				{
					//add item
					v_item[i].widget->addItem(v_item[i].menuItem, v_item[i].default_selected); //forwarders...
					allow_sep = true;
									
					//generate shortcut for next item
					if (add_shortcut)
						short_cut++;
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
CUserMenuNotifier::CUserMenuNotifier( CMenuItem* i1, CMenuItem* i2)
{
   toDisable[0]=i1;
   toDisable[1]=i2;
}

bool CUserMenuNotifier::changeNotify(const neutrino_locale_t, void *)
{
	toDisable[0]->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_RED_BUTTON]);
	toDisable[1]->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_BLUE_BUTTON]);
   
   return true;
}

//helper class to enable/disable pin setup
CPinSetupNotifier::CPinSetupNotifier( CMenuItem* item)
{
   toDisable=item;
}

bool CPinSetupNotifier::changeNotify(const neutrino_locale_t, void *)
{
	toDisable->setActive(g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS]);
   
   return g_settings.personalize[SNeutrinoSettings::P_MAIN_PINSTATUS];
}
