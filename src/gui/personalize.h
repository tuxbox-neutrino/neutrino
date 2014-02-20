/*
	$Id: personalize.h,v 1.19 2011/04/25 19:32:46 tuxbox-cvs Exp $

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
*/


#ifndef __personalize__
#define __personalize__

#include <gui/widget/menue.h>
#include <gui/plugins.h>
#include <gui/user_menue_setup.h>
#include "widget/stringinput.h"
#include "widget/stringinput_ext.h"
#include <string>
#include <vector>
#include <configfile.h>
#include <system/lastchannel.h>
#include <system/setting_helpers.h>

extern CPlugins       * g_PluginList;    /* neutrino.cpp */

//helper class to enable/disable some items in usermenu setup
class CUserMenuNotifier : public CChangeObserver
{
	private:

		CMenuItem* toDisable[4];
	
	public:
		CUserMenuNotifier( CMenuItem*, CMenuItem*, CMenuItem*, CMenuItem*);

		bool changeNotify(const neutrino_locale_t = NONEXISTANT_LOCALE, void *data = NULL);
};

//helper class to enable/disable pin setup
class CPinSetupNotifier : public CChangeObserver
{
	private:
		CMenuItem* toDisable;
	public:
		CPinSetupNotifier( CMenuItem*);
		bool changeNotify(const neutrino_locale_t = NONEXISTANT_LOCALE, void *data = NULL);
};


//some required typedefs
typedef struct mn_widget_t
{
	const neutrino_locale_t locale_text;
	const std::string icon;
	const int width;
} mn_widget_struct_t;

typedef struct settings_int_t
{
	int old_val;
	int *p_val;
}settings_int_struct_t;

typedef struct menu_item_t
{
	CMenuWidget *widget;
	CMenuItem* menuItem;
	bool default_selected;
	neutrino_locale_t locale_name;
	int* personalize_mode;
	int item_mode;
	CMenuItem *observer_Item;
}menu_item_struct_t;

typedef struct observ_menu_item_t
{
	CMenuWidget *widget;
	neutrino_locale_t observer_locale;
	neutrino_locale_t to_observ_locale;
}observ_menu_item_struct_t;

typedef struct raw_item_t
{
	CMenuItem* menuItem;
	bool default_selected;
	int* personalize_mode;
	int item_mode;
}raw_item_struct_t;

typedef struct personalize_settings_t
{
	const char* personalize_settings_name;
	int personalize_default_val;
} personalize_settings_struct_t;


class CPersonalizeGui : public CMenuTarget, public CChangeObserver, public CPINProtection
{
	protected:
		virtual CMenuTarget* getParent() { return( NULL);};
		
	private:
		CUserMenuNotifier *user_menu_notifier;
		CPinSetupNotifier *pin_setup_notifier;
		CMenuWidget* fkeyMenu;
		CMenuWidget* plMenu;
		CMenuWidget* tmpW;
		CMenuWidget *pluginSelectionMenu;
		
		int width, widget_count, shortcut, options_count;
		bool show_usermenu, show_pin_setup;
				
		//stuff for settings handlers
		void	handleSetting(int *setting);
		void	restoreSettings();
		bool	haveChangedSettings();

		std::vector<settings_int_t> v_int_settings;
		
		std::vector<menu_item_t> v_item;
		std::vector<CMenuWidget *> v_widget;
		std::vector<observ_menu_item_t> v_observ;
				
		int 	ShowPersonalizationMenu();
		int 	ShowMenuOptions(const int& menu);
		void 	ShowHelpPersonalize();
		void 	ShowPinSetup(CMenuWidget* p_widget, CPINChangeWidget * &pin_widget);
		void 	ShowUserMenu(CMenuWidget* p_widget, std::vector<CUserMenuSetup*> &v_umenu);
		void 	ShowPluginMenu(CMenuWidget* p_widget);
		void 	ShowPreverredKeySetup(CMenuWidget* p_widget);
		void 	SaveAndExit();
		void	ApplySettings();
		
		bool	hasPinItems();
		
		neutrino_msg_t	getShortcut(const int & shortcut_num, neutrino_msg_t alternate_rc_key = CRCInput::RC_nokey);
		void 	addObservedItem(CMenuWidget *widget, CMenuItem *observer_Item, CMenuItem *to_observ_Item);
		bool 	changeNotify(const neutrino_locale_t locale= NONEXISTANT_LOCALE, void *data = NULL);
		bool 	isObserver(CMenuWidget* widget, CMenuItem * item);
	public:	
		//general options for personalized items
		enum PERSONALIZE_MODE 
		{
			PERSONALIZE_MODE_NOTVISIBLE =  0,
			PERSONALIZE_MODE_VISIBLE  =  1,
			PERSONALIZE_MODE_PIN  = 2,
			
			PERSONALIZE_MODE_MAX
		};

		//options for personalized items with pin protection
		enum PERSONALIZE_PROTECT_MODE 
		{
			PERSONALIZE_PROTECT_MODE_NOT_PROTECTED =  0,
			PERSONALIZE_PROTECT_MODE_PIN_PROTECTED  =  2,
			
			PERSONALIZE_PROTECT_MODE_MAX = 2
		};

		//options for show_epg_feat options
		enum PERSONALIZE_ACTIVE_MODE 
		{
			PERSONALIZE_ACTIVE_MODE_DISABLED =  0,
			PERSONALIZE_ACTIVE_MODE_ENABLED  =  1,
			
			PERSONALIZE_ACTIVE_MODE_MAX
		};
		
		//internal display modes for items in personalize settings menue
		enum PERSONALIZE_ITEM_MODE 
		{
			PERSONALIZE_SHOW_NO =  0,
			PERSONALIZE_SHOW_AS_ITEM_OPTION  =  1,
			PERSONALIZE_SHOW_AS_ACCESS_OPTION  =  2,
			PERSONALIZE_SHOW_ONLY_IN_PERSONALIZE_MENU  =  3 //usefull to hide separators in menu, but visible only in personalizing menu
		};
		
		//options for features key
		enum PERSONALIZE_FEAT_KEY 
		{
			PERSONALIZE_FEAT_KEY_RED,
			PERSONALIZE_FEAT_KEY_GREEN,
			PERSONALIZE_FEAT_KEY_YELLOW,
			PERSONALIZE_FEAT_KEY_BLUE,
			PERSONALIZE_FEAT_KEY_AUTO,
			
			PERSONALIZE_FEAT_KEY_MAX
		};
		
		CPersonalizeGui();
		~CPersonalizeGui();
				
		int 	exec(CMenuTarget* parent, const std::string & actionKey);
		
		CMenuWidget& getWidget(const int& id);
		
		void 	addWidget(CMenuWidget *widget);
		void 	addWidgets(const struct mn_widget_t * const widget, const int& widget_count);
		int 	getWidgetCount() {return widget_count;};
		int 	getWidgetId(CMenuWidget *widget);
		int	getItemsCount(CMenuWidget *widget);
		int	getItemsCount(const int& widget_id);
		void	setShortcut(const int& short_cut = 1) {shortcut = short_cut;};
		void 	addItem(CMenuWidget *widget, CMenuItem *menu_Item, const int *personalize_mode = NULL, const bool defaultselected = false, const int& item_mode = PERSONALIZE_SHOW_AS_ITEM_OPTION, CMenuItem *observer_Item = NULL);
		void 	addItem(const int& widget_id, CMenuItem *menu_Item, const int *personalize_mode = NULL, const bool defaultselected = false, const int& item_mode = PERSONALIZE_SHOW_AS_ITEM_OPTION, CMenuItem *observer_Item = NULL);
		void	addIntroItems(CMenuWidget *widget);
		void	addIntroItems(const int& widget_id);
		void 	addSeparator(CMenuWidget &menu, const neutrino_locale_t locale_text = NONEXISTANT_LOCALE, const int& item_mode = PERSONALIZE_SHOW_AS_ITEM_OPTION);
		void 	addSeparator(const int& widget_id, const neutrino_locale_t locale_text = NONEXISTANT_LOCALE, const int& item_mode = PERSONALIZE_SHOW_AS_ITEM_OPTION);
		void 	addPersonalizedItems();
		void	enableUsermenu(bool show = true){show_usermenu = show;};
		void	enablePinSetup(bool show = true){show_pin_setup = show;};
};

typedef struct feat_keys_t
{
	neutrino_locale_t locale_name;
	neutrino_msg_t key;
}feat_keys_struct_t;

const struct feat_keys_t feat_key[CPersonalizeGui::PERSONALIZE_FEAT_KEY_MAX] =
{
	{LOCALE_PERSONALIZE_BUTTON_RED		, CRCInput::RC_red	},
	{LOCALE_PERSONALIZE_BUTTON_GREEN	, CRCInput::RC_green	},
	{LOCALE_PERSONALIZE_BUTTON_YELLOW	, CRCInput::RC_yellow	},
	{LOCALE_PERSONALIZE_BUTTON_BLUE		, CRCInput::RC_blue	},
	{LOCALE_PERSONALIZE_BUTTON_AUTO		, CRCInput::RC_nokey	}
};

#endif

