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
	along with this program; if not, write to the 
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifndef __MENU__
#define __MENU__

#include <driver/framebuffer.h>
#include <driver/rcinput.h>
#include <system/localize.h>
#include <gui/widget/icons.h>
#include <gui/color.h>
#include <gui/components/cc.h>
#include <string>
#include <vector>

#define NO_WIDGET_ID -1

typedef int mn_widget_id_t;

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
		virtual fb_pixel_t getColor(void) { return 0; }
		virtual const char * getTargetValue() { return NULL; }
};

class CMenuItem
{
	protected:
		int x, y, dx, offx, name_start_x, icon_frame_w;
		bool used;
		fb_pixel_t item_color, item_bgcolor;
		
		void initItemColors(const bool select_mode);
			
	public:
		bool           	active;
		bool		isStatic;
		neutrino_msg_t 	directKey;
		neutrino_msg_t 	msg;
		std::string    	iconName;
		std::string    	selected_iconName;
		std::string    	iconName_Info_right;
		std::string	hintIcon;
		neutrino_locale_t hint;

		CMenuItem();
		virtual ~CMenuItem(){}
		
		virtual void isUsed(void)
		{
			used = true;
		}
		
		virtual void init(const int X, const int Y, const int DX, const int OFFX);

		virtual int paint (bool selected = false) = 0;
		virtual int getHeight(void) const = 0;
		virtual int getWidth(void)
		{
			return 0;
		}
		virtual int getYPosition(void) const { return y; }

		virtual bool isSelectable(void) const
		{
			return false;
		}

		virtual int exec(CMenuTarget* /*parent*/)
		{
			return 0;
		}
		virtual void setActive(const bool Active);
		
		virtual void paintItemButton(const bool select_mode, const int &item_height, const std::string& icon_Name = NEUTRINO_ICON_BUTTON_RIGHT);
		
		virtual void paintItemBackground (const bool select_mode, const int &item_height);
		
		virtual void prepareItem(const bool select_mode, const int &item_height);

		virtual void setItemButton(const std::string& icon_Name, const bool is_select_button = false);
		
		virtual void paintItemCaption(const bool select_mode, const int &item_height, const char * left_text=NULL, const char * right_text=NULL, const fb_pixel_t right_bgcol=0);

		virtual void paintItemSlider( const bool select_mode, const int &item_height, const int &optionvalue, const int &factor, const char * left_text=NULL, const char * right_text=NULL);

		virtual int isMenueOptionChooser(void) const{return 0;}
		void setHint(std::string icon, neutrino_locale_t text) { hintIcon = icon; hint = text; }
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


		CMenuSeparator(const int Type = 0, const neutrino_locale_t Text = NONEXISTANT_LOCALE, bool IsStatic = false);
		virtual ~CMenuSeparator(){}

		int paint(bool selected=false);
		int getHeight(void) const;
		int getWidth(void);

		virtual const char * getString(void);
		void setString(const std::string& text);
};

class CNonLocalizedMenuSeparator : public CMenuSeparator
{
	const char * the_text;

public:
	CNonLocalizedMenuSeparator(const char * ptext, const neutrino_locale_t Text1) : CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, Text1)
	{
		the_text = ptext;
	}

	virtual const char * getString(void)
	{
		return the_text;
	}
};

class CMenuForwarder : public CMenuItem
{
	std::string         actionKey;

 protected:
	const char *        option;
	const std::string * option_string;
	CMenuTarget *       jumpTarget;
	neutrino_locale_t text;

	virtual const char * getOption(void);
	virtual const char * getName(void);

 public:

	CMenuForwarder(const neutrino_locale_t Text, const bool Active=true, const char * const Option=NULL, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL, bool IsStatic = false);
	CMenuForwarder(const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL, bool IsStatic = false);

	virtual ~CMenuForwarder(){}

	int paint(bool selected=false);
	int getHeight(void) const;
	int getWidth(void);
	void setTextLocale(const neutrino_locale_t Text);
	neutrino_locale_t getTextLocale() const {return text;};
	CMenuTarget* getTarget() const {return jumpTarget;};
	std::string getActionKey(){return actionKey;};

	int exec(CMenuTarget* parent);
	bool isSelectable(void) const
		{
			return active;
		}
	void setOption(const char * const Option);
	void setOption(const std::string &Option);
};

class CMenuDForwarder : public CMenuForwarder
{
 public:
	CMenuDForwarder(const neutrino_locale_t Text, const bool Active=true, const char * const Option=NULL, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL);
	CMenuDForwarder(const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL);

	~CMenuDForwarder();
};

class CMenuForwarderNonLocalized : public CMenuForwarder
{
 protected:
	std::string the_text;
	virtual const char * getName(void);
 public:
	// Text must be UTF-8 encoded:
	CMenuForwarderNonLocalized(const char * const Text, const bool Active=true, const char * const Option=NULL, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL);
	CMenuForwarderNonLocalized(const char * const Text, const bool Active, const std::string &Option, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL);
	virtual ~CMenuForwarderNonLocalized(){}

	int getWidth(void);

	void setText(const char * const Text);
};

class CMenuDForwarderNonLocalized : public CMenuForwarderNonLocalized
{
 public:
	CMenuDForwarderNonLocalized(const char * const Text, const bool Active=true, const char * const Option=NULL, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL);
	CMenuDForwarderNonLocalized(const char * const Text, const bool Active, const std::string &Option, CMenuTarget* Target=NULL, const char * const ActionKey = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL);
	~CMenuDForwarderNonLocalized();
};

class CAbstractMenuOptionChooser : public CMenuItem
{
	protected:
		neutrino_locale_t optionName;
		int               height;
		int *             optionValue;

		int getHeight(void) const{return height;}

		bool isSelectable(void) const{return active;}
	public:
		CAbstractMenuOptionChooser()
		{
			optionName = NONEXISTANT_LOCALE;
			height = 0;
			optionValue = NULL;
		};
		~CAbstractMenuOptionChooser(){}
		
};

class CMenuOptionNumberChooser : public CAbstractMenuOptionChooser
{
private:
	const char *       optionString;

	int                lower_bound;
	int                upper_bound;

	int                display_offset;

	int                localized_value;
	neutrino_locale_t  localized_value_name;
	bool  		slider_on;
	CChangeObserver *     observ;
	std::string	numberFormat;
	std::string	(*numberFormatFunction)(int num);
 public:
	CMenuOptionNumberChooser(const neutrino_locale_t name, int * const OptionValue, const bool Active, const int min_value, const int max_value, CChangeObserver * const Observ = NULL, const int print_offset = 0, const int special_value = 0, const neutrino_locale_t special_value_name = NONEXISTANT_LOCALE, const char * non_localized_name = NULL, bool sliderOn = false );

	int paint(bool selected);

	int exec(CMenuTarget* parent);
	int isMenueOptionChooser(void) const{return 1;}
	int getWidth(void);
	void setNumberFormat(std::string format) { numberFormat = format; }
	void setNumberFormat(std::string (*fun)(int)) { numberFormatFunction = fun; }
};

class CMenuOptionChooserOptions
{	
	public:
		int               key;
		std::string       valname;
};

struct CMenuOptionChooserCompareItem: public std::binary_function <const CMenuOptionChooserOptions * const, const CMenuOptionChooserOptions * const, bool>
{
	static bool cmpToLower(const char a, const char b)
	{
		return tolower(a) < tolower(b);
	};

	bool operator() (const CMenuOptionChooserOptions * const c1, CMenuOptionChooserOptions * const c2)
	{
		return std::lexicographical_compare(c1->valname.begin(), c1->valname.end(), c2->valname.begin(), c2->valname.end(), cmpToLower);
	};
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
	std::vector<CMenuOptionChooserOptions*> option_chooser_options_v;
	unsigned		number_of_options;
	CChangeObserver *	observ;
	std::string		optionNameString;
	bool			pulldown;
	bool			optionsSort;

	void clearChooserOptions();

 public:
	CMenuOptionChooser(const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false, bool OptionsSort = false);
	CMenuOptionChooser(const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval_ext * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false, bool OptionsSort = false);
	CMenuOptionChooser(const char* OptionName, int * const OptionValue, const struct keyval * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false, bool OptionsSort = false);
	CMenuOptionChooser(const char* OptionName, int * const OptionValue, const struct keyval_ext * const Options,
			   const unsigned Number_Of_Options, const bool Active = false, CChangeObserver * const Observ = NULL,
			   const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "",
			   bool Pulldown = false, bool OptionsSort = false);
	~CMenuOptionChooser();

	void setOptionValue(const int newvalue);
	int getOptionValue(void) const;
	int getWidth(void);

	int paint(bool selected);
	std::string getOptionName()const {return optionNameString;};

	int exec(CMenuTarget* parent);
	int isMenueOptionChooser(void) const{return 1;}
};

class CMenuOptionStringChooser : public CMenuItem
{
		neutrino_locale_t        optionName;
		std::string 		 optionNameString;
		int                      height;
		char *                   optionValue;
		std::string *		 optionValueString;
		std::vector<std::string> options;
		CChangeObserver *        observ;
		bool			 pulldown;

	public:
		CMenuOptionStringChooser(const neutrino_locale_t OptionName, char* OptionValue, bool Active = false, CChangeObserver* Observ = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "", bool Pulldown = false);
		CMenuOptionStringChooser(const neutrino_locale_t OptionName, std::string* OptionValue, bool Active = false, CChangeObserver* Observ = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "", bool Pulldown = false);
		CMenuOptionStringChooser(const char* OptionName, char* OptionValue, bool Active = false, CChangeObserver* Observ = NULL, const neutrino_msg_t DirectKey = CRCInput::RC_nokey, const std::string & IconName= "", bool Pulldown = false);

		~CMenuOptionStringChooser();

		void addOption(const char * value);
		void removeOptions(){options.clear();};
		int paint(bool selected);
		int getHeight(void) const
		{
			return height;
		}
		bool isSelectable(void) const
		{
			return active;
		}
		void sortOptions();
		int exec(CMenuTarget* parent);
		int isMenueOptionChooser(void) const{return 1;}
};

class CMenuOptionLanguageChooser : public CMenuItem
{
		int                      height;
		std::string		 optionValue;
		CChangeObserver *        observ;

	public:
		CMenuOptionLanguageChooser(char* OptionValue, CChangeObserver* Observ = NULL, const char * const IconName = NULL);
		~CMenuOptionLanguageChooser();

		int paint(bool selected);
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

class CMenuGlobal
{
	public:
		std::vector<int> v_selected;
		
		CMenuGlobal();	
		~CMenuGlobal();
		
		static CMenuGlobal* getInstance();
};

class CMenuWidget : public CMenuTarget
{
	private: 
		mn_widget_id_t 		widget_index;
		CMenuGlobal		*mglobal;
		CComponentsDetailLine	*details_line;
		CComponentsInfoBox	*info_box;
		int			hint_height;

	protected:
		std::string		nameString;
		neutrino_locale_t       name;
		CFrameBuffer		*frameBuffer;
		std::vector<CMenuItem*>	items;
		std::vector<int>	page_start;
		struct keyAction { std::string action; CMenuTarget *menue; };
		std::map<neutrino_msg_t, keyAction> keyActionMap;
		std::string		iconfile;

		int			min_width;
		int			width;
		int			height;
		int			hheight; // header
		int			x;
		int			y;
		int			offx, offy;
		int			preselected;
		int			selected;
		int 			iconOffset;
		int			sb_width;
		fb_pixel_t		*background;
		int			full_width, full_height;
		bool			savescreen;
		bool			has_hints; // is any items has hints
		bool			hint_painted; // is hint painted

		unsigned int         item_start_y;
		unsigned int         current_page;
		unsigned int         total_pages;
		bool		     exit_pressed;
		bool		     from_wizard;
		bool		     fade;
		bool		     washidden;

		void Init(const std::string & Icon, const int mwidth, const mn_widget_id_t &w_index);
		virtual void paintItems();
		void checkHints();
		void calcSize();
		void saveScreen();
		void restoreScreen();
		void setMenuPos(const int& menu_width);

	public:
		CMenuWidget();
		/* mwidth (minimum width) in percent of screen width */
		CMenuWidget(const char* Name, const std::string & Icon = "", const int mwidth = 30, const mn_widget_id_t &w_index = NO_WIDGET_ID);
		CMenuWidget(const neutrino_locale_t Name, const std::string & Icon = "", const int mwidth = 30, const mn_widget_id_t &w_index = NO_WIDGET_ID);
		~CMenuWidget();
		
		virtual void addItem(CMenuItem* menuItem, const bool defaultselected = false);
		
		enum 
		{
			BTN_TYPE_BACK =		0,
			BTN_TYPE_CANCEL =	1,
			BTN_TYPE_NEXT =		3,
			BTN_TYPE_NO =		-1
		};
		virtual void addIntroItems(neutrino_locale_t subhead_text = NONEXISTANT_LOCALE, neutrino_locale_t section_text = NONEXISTANT_LOCALE, int buttontype = BTN_TYPE_BACK );
		bool hasItem();
		void resetWidget(bool delete_items = false);
		void insertItem(const uint& item_id, CMenuItem* menuItem);
		void removeItem(const uint& item_id);
		int getItemId(CMenuItem* menuItem);
		int getItemsCount()const{return items.size();};
		CMenuItem* getItem(const uint& item_id);
		virtual void paint();
		virtual void hide();
		virtual int exec(CMenuTarget* parent, const std::string & actionKey);
		virtual std::string getName();
		void setSelected(const int &Preselected){ preselected = Preselected; };
		int getSelected()const { return selected; };
		void move(int xoff, int yoff);
		int getSelectedLine(void)const {return exit_pressed ? -1 : selected;};
		void setWizardMode(bool _from_wizard) { from_wizard = _from_wizard;};		
		void enableFade(bool _enable) { fade = _enable; };
		void enableSaveScreen(bool enable);
		void paintHint(int num);
		enum 
		{
			MENU_POS_CENTER 	,
			MENU_POS_TOP_LEFT	,
			MENU_POS_TOP_RIGHT	,
			MENU_POS_BOTTOM_LEFT	,
			MENU_POS_BOTTOM_RIGHT
		};
		void addKey(neutrino_msg_t key, CMenuTarget *menue, const std::string &action);
};

class CPINProtection
{
	protected:
		char* validPIN;
		bool check();
		virtual CMenuTarget* getParent() = 0;
		neutrino_locale_t title, hint;
	public:
		CPINProtection( char* validpin)
		{ 
			validPIN = validpin;
			hint = NONEXISTANT_LOCALE;
			title = LOCALE_PINPROTECTION_HEAD;
		};
		virtual ~CPINProtection(){}
		virtual void setTitle(neutrino_locale_t Title){title = Title;};
		virtual void setHint(neutrino_locale_t Hint){ hint = Hint;};
};

class CZapProtection : public CPINProtection
{
	protected:
		virtual CMenuTarget* getParent() { return( NULL);};
	public:
		int	fsk;

		CZapProtection( char* validpin, int	FSK ) : CPINProtection(validpin)
		{
			fsk = FSK;
			title = LOCALE_PARENTALLOCK_HEAD;
		};
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
		                     neutrino_msg_t DirectKey = CRCInput::RC_nokey, const char * const IconName = NULL, const char * const IconName_Info_right = NULL)

					: CMenuForwarder(Text, Active, Option, Target, ActionKey, DirectKey, IconName, IconName_Info_right) ,CPINProtection(_validPIN)
					{
						Ask = ask;
						
						//if we in ask mode then show NEUTRINO_ICON_SCRAMBLED as default info icon or no icon, 
						//but use always an info icon if defined in parameter 'IconName_Info_right'
						if (IconName_Info_right || ask)
							iconName_Info_right = IconName_Info_right ? IconName_Info_right : NEUTRINO_ICON_LOCK; 
						else
							iconName_Info_right = "";
				       };

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
