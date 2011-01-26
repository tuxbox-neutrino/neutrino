/*
	$port: menue.h,v 1.91 2010/12/08 19:49:30 tuxbox-cvs Exp $

	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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


#ifndef __MENU__
#define __MENU__

#include <driver/framebuffer.h>
#include <driver/rcinput.h>
#include <system/localize.h>
#include <gui/widget/icons.h>
#include <gui/color.h>

#include <string>
#include <vector>

struct menu_return
{
	enum
		{
			RETURN_NONE	= 0,
			RETURN_REPAINT 	= 1,
			RETURN_EXIT 	= 2,
			RETURN_EXIT_ALL = 4,
			RETURN_EXIT_REPAINT = 5
		};
};

class CChangeObserver
{
	public:
		virtual ~CChangeObserver(){}
		virtual bool changeNotify(const neutrino_locale_t /*OptionName*/, void */*Data*/)
		{
			return false;
		}
};

class CMenuTarget
{
	public:
		CMenuTarget(){}
		virtual ~CMenuTarget(){}
		virtual void hide(){}
		virtual int exec(CMenuTarget* parent, const std::string & actionKey) = 0;
};

class CMenuItem
{
	protected:
		int x, y, dx, offx, name_start_x;
		bool used;
		
	public:
		bool           	active;
		neutrino_msg_t 	directKey;
		neutrino_msg_t 	msg;
		bool		can_arrow;
		std::string    	iconName;
		std::string    	selected_iconName;
		bool		show_marker;
		fb_pixel_t	item_color;
		fb_pixel_t	item_bgcolor;


		CMenuItem();
		virtual ~CMenuItem(){}
		
		virtual void isUsed(void)
		{
			used = true;
		}

		virtual void init(const int X, const int Y, const int DX, const int OFFX);

		virtual int paint (bool selected = false, bool last = false) = 0;

		virtual int getHeight(void) const = 0;
		virtual int getWidth(void)
		{
			return 0;
		}

		virtual bool isSelectable(void) const
		{
			return false;
		}

		virtual int exec(CMenuTarget* /*parent*/)
		{
			return 0;
		}
		virtual void setActive(const bool Active);

		virtual void paintItemButton(const bool select_mode, const std::string& icon_Name = NEUTRINO_ICON_BUTTON_RIGHT, const bool icon_centered = false);
		
		virtual void setItemColors(const bool select_mode , 	const fb_pixel_t &def_color = COL_MENUCONTENT, 			const fb_pixel_t &def_bgcolor = COL_MENUCONTENT_PLUS_0, 
									const fb_pixel_t &def_sel_color = COL_MENUCONTENTSELECTED, 	const fb_pixel_t &def_sel_bgcolor = COL_MENUCONTENTSELECTED_PLUS_0, 
									const fb_pixel_t &def_inactiv_color = COL_MENUCONTENTINACTIVE,	const fb_pixel_t &def_inactiv_bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0);
									
		virtual void paintItemBackground (const bool select_mode, const int &item_height);
		
		virtual void paintItem(const bool select_mode, const int &item_height);

		virtual void setItemButton(const std::string& icon_Name, const bool is_select_button = false);
		
		virtual void paintItemCaption(const bool select_mode, const int &item_height, const char * left_text=NULL, const char * right_text=NULL);
};

class CMenuSeparator : public CMenuItem
{
		int               type;
		std::string	  separator_text;

	public:
		neutrino_locale_t text;

		enum
		{
			EMPTY =	0,
			LINE =	1,
			STRING =	2,
			ALIGN_CENTER = 4,
			ALIGN_LEFT =   8,
			ALIGN_RIGHT = 16,
			SUB_HEAD = 32
		};


		CMenuSeparator(const int Type = 0, const neutrino_locale_t Text = NONEXISTANT_LOCALE);
		virtual ~CMenuSeparator(){}

		int paint(bool selected=false, bool last = false);
		int getHeight(void) const;
		int getWidth(void);

		virtual const char * getString(void);
		virtual void setString(const std::string& text);
};

class CMenuForwarder : public CMenuItem
{
	const char *        option;
	const std::string * option_string;
	CMenuTarget *       jumpTarget;
	std::string         actionKey;

 protected:
	neutrino_locale_t text;

	virtual const char * getOption(void);
	virtual const char * getName(void);

 public:

	CMenuForwarder(const neutrino_locale_t Text, const bool Active=true, const char * const Option=NULL, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL);
	CMenuForwarder(const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL);
	virtual ~CMenuForwarder(){}

	int paint(bool selected=false, bool last = false);
	int getHeight(void) const;
	int getWidth(void);
	void setOption(const char *Option);
	void setTextLocale(const neutrino_locale_t Text);
	neutrino_locale_t getTextLocale(){return text;};
	CMenuTarget* getTarget(){return jumpTarget;};
	std::string getActionKey(){return actionKey;};

	int exec(CMenuTarget* parent);
	bool isSelectable(void) const
		{
			return active;
		}
};

class CMenuForwarderNonLocalized : public CMenuForwarder
{
 protected:
	std::string the_text;
	virtual const char * getName(void);
 public:
	// Text must be UTF-8 encoded:
	CMenuForwarderNonLocalized(const char * const Text, const bool Active=true, const char * const Option=NULL, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL);
	CMenuForwarderNonLocalized(const char * const Text, const bool Active, const std::string &Option, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL);
	virtual ~CMenuForwarderNonLocalized(){}

	int getWidth(void);

	void setText(const char * const Text);
};

class CAbstractMenuOptionChooser : public CMenuItem
{
 protected:
	neutrino_locale_t optionName;
	int               height;
	int *             optionValue;

	int getHeight(void) const
		{
			return height;
		}

	bool isSelectable(void) const
		{
			return active;
		}
};

class CMenuOptionNumberChooser : public CAbstractMenuOptionChooser
{
	const char *       optionString;

	int                lower_bound;
	int                upper_bound;

	int                display_offset;

	int                localized_value;
	neutrino_locale_t  localized_value_name;
 private:
	CChangeObserver *     observ;

 public:
	CMenuOptionNumberChooser(const neutrino_locale_t name, int * const OptionValue, const bool Active, const int min_value, const int max_value, CChangeObserver * const Observ = NULL, const int print_offset = 0, const int special_value = 0, const neutrino_locale_t special_value_name = NONEXISTANT_LOCALE, const char * non_localized_name = NULL);

	int paint(bool selected, bool last = false);

	int exec(CMenuTarget* parent);
};

class CMenuOptionChooser : public CAbstractMenuOptionChooser
{
 public:
	struct keyval_ext
	{
		int key;
		neutrino_locale_t value;
		const char *valname;
	};

	struct keyval
	{
		const int               key;
		const neutrino_locale_t value;
	};

 private:
	std::vector<keyval_ext> options;
	unsigned              number_of_options;
	CChangeObserver *     observ;
	std::string optionNameString;
	bool			 pulldown;

 public:
	CMenuOptionChooser(const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false);
	CMenuOptionChooser(const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval_ext * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false);
	CMenuOptionChooser(const char* OptionName, int * const OptionValue, const struct keyval * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false);
	CMenuOptionChooser(const char* OptionName, int * const OptionValue, const struct keyval_ext * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false);
	~CMenuOptionChooser();

	void setOptionValue(const int newvalue);
	int getOptionValue(void) const;
	int getWidth(void);

	int paint(bool selected, bool last = 0);
	std::string getOptionName() {return optionNameString;};

	int exec(CMenuTarget* parent);
};

class CMenuOptionStringChooser : public CMenuItem
{
		neutrino_locale_t        optionName;
		int                      height;
		char *                   optionValue;
		std::vector<std::string> options;
		CChangeObserver *        observ;
		bool			 pulldown;

	public:
		CMenuOptionStringChooser(const neutrino_locale_t OptionName, char* OptionValue, bool Active = false, CChangeObserver* Observ = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "", bool Pulldown = false);
		~CMenuOptionStringChooser();

		void addOption(const char * value);
		void removeOptions(void);
		int paint(bool selected, bool last = 0);
		int getHeight(void) const
		{
			return height;
		}
		bool isSelectable(void) const
		{
			return active;
		}

		int exec(CMenuTarget* parent);
};

class CMenuOptionLanguageChooser : public CMenuItem
{
		int                      height;
		char *                   optionValue;
		std::vector<std::string> options;
		CChangeObserver *        observ;

	public:
		CMenuOptionLanguageChooser(char* OptionValue, CChangeObserver* Observ = NULL, const char * const IconName = NULL);
		~CMenuOptionLanguageChooser();

		void addOption(const char * value);
		int paint(bool selected, bool last = 0);
		int getHeight(void) const
		{
			return height;
		}
		bool isSelectable(void) const
		{
			return true;
		}

		int exec(CMenuTarget* parent);
};

class CMenuWidget : public CMenuTarget
{
	protected:
		std::string		nameString;
		neutrino_locale_t       name;
		CFrameBuffer		*frameBuffer;
		std::vector<CMenuItem*>	items;
		std::vector<unsigned int> page_start;
		std::string			iconfile;

		int			min_width;
		int			width;
		int			height;
		int         		wanted_height;
		int			x;
		int			y;
		int			offx, offy;
		int			preselected;
		int			selected;
		int 			iconOffset;
		unsigned int         item_start_y;
		unsigned int         current_page;
		unsigned int         total_pages;
		bool		     exit_pressed;
		bool		     from_wizard;
		bool		     fade;

		void Init(const std::string & Icon, const int mwidth, const int mheight);
		virtual void paintItems();
	public:
		CMenuWidget();
		/* TODO: mheight is not used anymore. remove if nobody misses it */
		/* mwidth (minimum width) in percent of screen width */
		CMenuWidget(const char* Name, const std::string & Icon = "", const int mwidth = 30, const int mheight = 576);
		CMenuWidget(const neutrino_locale_t Name, const std::string & Icon = "", const int mwidth = 30, const int mheight = 576);
		~CMenuWidget();

		virtual void addItem(CMenuItem* menuItem, const bool defaultselected = false);
		
		enum 
		{
			BTN_TYPE_BACK =		0,
			BTN_TYPE_CANCEL =	1,
			BTN_TYPE_NEXT =		3,
			BTN_TYPE_NO =		-1,
		};
		virtual void addIntroItems(neutrino_locale_t subhead_text = NONEXISTANT_LOCALE, neutrino_locale_t section_text = NONEXISTANT_LOCALE, int buttontype = BTN_TYPE_BACK );
		bool hasItem();
		virtual void paint();
		virtual void hide();
		virtual int exec(CMenuTarget* parent, const std::string & actionKey);
		virtual std::string getName(){ return nameString;};
		virtual void setSelected(const int &Preselected){ preselected = Preselected; };
		virtual int getSelected(){ return selected; };
		void move(int xoff, int yoff);
		int getSelectedLine(void){return exit_pressed ? -1 : selected;};
		void setWizardMode(bool _from_wizard) { from_wizard = _from_wizard;};
		void enableFade(bool _enable) { fade = _enable; };
};

class CPINProtection
{
	protected:
		char* validPIN;
		bool check();
		virtual CMenuTarget* getParent() = 0;
	public:
		CPINProtection( char* validpin){ validPIN = validpin;};
		virtual ~CPINProtection(){}
};

class CZapProtection : public CPINProtection
{
	protected:
		virtual CMenuTarget* getParent() { return( NULL);};
	public:
		int	fsk;

		CZapProtection( char* validpin, int	FSK ) : CPINProtection(validpin){ fsk= FSK; };
		bool check();
};

class CLockedMenuForwarder : public CMenuForwarder, public CPINProtection
{
	CMenuTarget* Parent;
	bool Ask;

	protected:
		virtual CMenuTarget* getParent(){ return Parent;};
	public:
		CLockedMenuForwarder(const neutrino_locale_t Text, char* _validPIN, bool ask=true, const bool Active=true, char *Option=NULL,
		                     CMenuTarget* Target=NULL, const char * const ActionKey = NULL,
		                     neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL)

		                     : CMenuForwarder(Text, Active, Option, Target, ActionKey, DirectKey, IconName) ,
		                       CPINProtection(_validPIN){Ask = ask;};

		virtual int exec(CMenuTarget* parent);
};

class CMenuSelectorTarget : public CMenuTarget
{
        public:
                CMenuSelectorTarget(int *select) {m_select = select;};
                int exec(CMenuTarget* parent, const std::string & actionKey);

        private:
                int *m_select;
};

extern CMenuSeparator * const GenericMenuSeparator;
extern CMenuSeparator * const GenericMenuSeparatorLine;
extern CMenuForwarder * const GenericMenuBack;
extern CMenuForwarder * const GenericMenuCancel;



#endif
