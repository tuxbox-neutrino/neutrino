/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	(C) 2008, 2009 Stefan Seyfried
	Copyright (C) 2012 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the 
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <global.h>
#include <neutrino.h>
#include <gui/widget/menue.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/pluginlist.h>
#include <gui/widget/stringinput.h>
#include <gui/infoclock.h>


#include <driver/fade.h>
#include <driver/display.h>
#include <system/helpers.h>

#include <cctype>

/* the following generic menu items are integrated into multiple menus at the same time */
CMenuSeparator CGenericMenuSeparator(0, NONEXISTANT_LOCALE, true);
CMenuSeparator CGenericMenuSeparatorLine(CMenuSeparator::LINE, NONEXISTANT_LOCALE, true);
CMenuForwarder CGenericMenuBack(LOCALE_MENU_BACK, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_LEFT, NULL, true);
CMenuForwarder CGenericMenuCancel(LOCALE_MENU_CANCEL, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_HOME, NULL, true);
CMenuForwarder CGenericMenuNext(LOCALE_MENU_NEXT, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_RIGHT, NULL, true);
CMenuSeparator * const GenericMenuSeparator = &CGenericMenuSeparator;
CMenuSeparator * const GenericMenuSeparatorLine = &CGenericMenuSeparatorLine;
CMenuForwarder * const GenericMenuBack = &CGenericMenuBack;
CMenuForwarder * const GenericMenuCancel = &CGenericMenuCancel;
CMenuForwarder * const GenericMenuNext = &CGenericMenuNext;

CMenuItem::CMenuItem(bool Active, neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right, bool IsStatic)
{
	active		= current_active = Active;
	directKey	= DirectKey;
	isStatic	= IsStatic;

	if (IconName && *IconName)
		iconName = IconName;
	else
		setIconName();

	setInfoIconRight(IconName_Info_right);

	hintIcon	= NULL;

	x		= -1;
	used		= false;
	icon_frame_w	= OFFSET_INNER_MID;
	hint		= NONEXISTANT_LOCALE;
	name		= NONEXISTANT_LOCALE;
	nameString	= "";
	desc		= NONEXISTANT_LOCALE;
	descString	= "";
	marked		= false;
	inert		= false;
	directKeyOK	= true;
	selected_iconName = NULL;
	height = 0;
	actObserv	= NULL;
	parent_widget	= NULL;
}

void CMenuItem::init(const int X, const int Y, const int DX, const int OFFX)
{
	x		= X;
	y		= Y;
	dx		= DX;
	offx		= OFFX;
	name_start_x	= x + offx + icon_frame_w;

	getItemColors(item_color, item_bgcolor);
}

void CMenuItem::setActive(const bool Active)
{
	active	= current_active = Active;
	/* used gets set by the addItem() function. This is for disabling
	   machine-specific options by just not calling the addItem() function.
	   Without this, the changeNotifiers would become machine-dependent. */
	if (used && x != -1)
		paint();
}

bool CMenuItem::initModeCondition(const int& stb_mode)
{
	if (CNeutrinoApp::getInstance()->getMode() == stb_mode){
		active 	= false;
		marked 	= false;
		if (parent_widget)
			if (!isSelectable())
				parent_widget->initSelectable();
		return true;
	}
	printf("\033[33m[CMenuItem] [%s - %d] missmatching stb mode condition %d\033[0m\n", __func__, __LINE__, stb_mode);
	return false;
}

void CMenuItem::disableByCondition(const menu_item_disable_cond_t& condition)
{
	int stb_mode = CNeutrinoApp::getInstance()->getMode();

	if (condition & DCOND_MODE_TS){
		if (stb_mode == CNeutrinoApp::mode_ts)
			if (initModeCondition(stb_mode))
				return;
	}
	if (condition & DCOND_MODE_RADIO){
		if (stb_mode == CNeutrinoApp::mode_radio)
			if (initModeCondition(stb_mode))
				return;
	}
	if (condition & DCOND_MODE_TV){
		if (stb_mode == CNeutrinoApp::mode_tv)
			if (initModeCondition(stb_mode))
				return;
	}

	active = current_active;

	if (parent_widget){
		if (!isSelectable())
			parent_widget->initSelectable();
	}
}

void CMenuItem::setInfoIconRight(const char * const IconName_Info_right){
	if (IconName_Info_right && *IconName_Info_right)
		iconName_Info_right = IconName_Info_right;
	else
		iconName_Info_right = NULL;
}

void CMenuItem::setMarked(const bool Marked)
{
	marked = Marked;
	if (used && x != -1)
		paint();
}

void CMenuItem::setInert(const bool Inert)
{
	inert = Inert;
	if (used && x != -1)
		paint();
}

void CMenuItem::setItemButton(const char * const icon_Name, const bool is_select_button)
{
	if (is_select_button)
		selected_iconName = icon_Name;
	else
		iconName = icon_Name;
}

void CMenuItem::initItemColors(const bool select_mode)
{
	getItemColors(item_color, item_bgcolor, select_mode, marked);

	if (!active || inert)
	{
		item_color   = COL_MENUCONTENTINACTIVE_TEXT;
		item_bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}
}

void CMenuItem::paintItemCaption(const bool select_mode, const char * right_text, const fb_pixel_t right_bgcol)
{
	int item_height = height;
	const char *left_text = getName();
	const char *desc_text = getDescription();

	if (select_mode)
	{
		if (right_text && *right_text) 
		{
			ssize_t len = strlen(left_text) + strlen(right_text) + 2;
			char str[len];
			snprintf(str, len, "%s %s", left_text, right_text);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
		} 
		else
			CVFD::getInstance()->showMenuText(0, left_text, -1, true);
	}
	
	//left text
	int _dx = dx;
	int icon_w = 0;
	int icon_h = 0;
	if (iconName_Info_right) {
		CFrameBuffer::getInstance()->getIconSize(iconName_Info_right, &icon_w, &icon_h);
		if (icon_w)
			_dx -= icon_frame_w + icon_w;
	}

	int desc_height = 0;
	if (desc_text && *desc_text)
		desc_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]->getHeight();

	if (*left_text)
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(name_start_x, y+ item_height - desc_height, _dx- (name_start_x - x), left_text, item_color);

	//right text
	if (right_text && (*right_text || right_bgcol))
	{
		int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(right_text);
		int stringstartposOption;
		if (*left_text)
			stringstartposOption = std::max(name_start_x + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(left_text) + icon_frame_w, x + dx - stringwidth - icon_frame_w); //+ offx
		else
			stringstartposOption = name_start_x;
		if (right_bgcol) {
			if (!*right_text)
				stringstartposOption -= CFrameBuffer::getInstance()->scale2Res(60);
			fb_pixel_t right_frame_col, right_bg_col;
			if (active) {
				right_bg_col = right_bgcol;
				right_frame_col = COL_FRAME_PLUS_0;
			}
			else {
				right_bg_col = COL_MENUCONTENTINACTIVE_TEXT;
				right_frame_col = COL_MENUCONTENTINACTIVE_TEXT;
			}
			CComponentsShapeSquare col(stringstartposOption, y + OFFSET_INNER_SMALL, dx - stringstartposOption + x - OFFSET_INNER_MID, item_height - 2*OFFSET_INNER_SMALL, NULL, false, right_frame_col, right_bg_col);
			col.setFrameThickness(FRAME_WIDTH_MIN);
			col.setCorner(RADIUS_SMALL);
			col.paint(false);
		}
		if (*right_text) {
			stringstartposOption -= (icon_w == 0 ? 0 : icon_w + icon_frame_w);
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y+item_height - desc_height, dx- (stringstartposOption- x),  right_text, item_color);
		}
	}
	if (desc_text && *desc_text)
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]->RenderString(name_start_x + OFFSET_INNER_MID, y+ item_height, _dx- OFFSET_INNER_MID - (name_start_x - x), desc_text, item_color);
}

void CMenuItem::prepareItem(const bool select_mode, const int &item_height)
{
 	//set colors
 	initItemColors(select_mode);

	//paint item background
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	frameBuffer->paintBoxRel(x, y, dx, item_height, item_bgcolor, RADIUS_LARGE);
}

void CMenuItem::paintItemSlider( const bool select_mode, const int &item_height, const int &optionvalue, const int &factor, const char * left_text, const char * right_text)
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();

	// assuming all sliders have same dimensions
	int slider_width, dummy;
	frameBuffer->getIconSize(NEUTRINO_ICON_SLIDER_ALPHA, &slider_width, &dummy);

	int bar_width = frameBuffer->scale2Res(100);
	/*
	   We have a half slider_width before and after the bar
	   to get the middle of the slider at the point of choise
	*/
	int bar_offset = slider_width/2;
	int bar_full = bar_width + slider_width;

	// avoid division by zero
	if (factor < optionvalue || factor < 1)
		return;

	int right_needed = 0;
	if (right_text != NULL)
	{
		right_needed = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("U999");
	}
	int left_needed = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(left_text);

	int space = dx - right_needed - icon_frame_w - left_needed - OFFSET_INNER_MID;
	if (space < bar_full)
		return ;

	int bar_x = x + dx - right_needed - bar_full;

	// FIXME: negative optionvalues falsifies the slider on the right side
	int optionV = (optionvalue < 0) ? 0 : optionvalue;

	// clear area
	frameBuffer->paintBoxRel(bar_x, y, bar_full, item_height, item_bgcolor);
	// paint bar
	frameBuffer->paintBoxFrame(bar_x + bar_offset, y + item_height/3, bar_width, item_height/3, 1, COL_MENUCONTENT_TEXT);
	// paint slider
	frameBuffer->paintIcon(select_mode ? NEUTRINO_ICON_SLIDER_ALPHA : NEUTRINO_ICON_SLIDER_INACTIVE, bar_x + (optionV*bar_width / factor), y, item_height);
}

void CMenuItem::paintItemButton(const bool select_mode, int item_height, const char * const icon_Name)
{
	item_height -= getDescriptionHeight();

	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	bool selected = select_mode;
	bool icon_painted = false;
	
	const char *icon_name = iconName;
	int icon_w = 0;
	int icon_h = 0;

	//define icon name depends of numeric value
	bool isNumeric = CRCInput::isNumeric(directKey);
#if 0
	if (isNumeric && !g_settings.menu_numbers_as_icons)
		icon_name = NULL;
#endif
	//define select icon
	if (selected && offx > 0)
	{
		if (selected_iconName)
			icon_name = selected_iconName;
		else if (!(icon_name && *icon_name) && !isNumeric)
			icon_name = icon_Name;
	}
	
	int icon_start_x = x+icon_frame_w; //start of icon space
	int icon_space_x = name_start_x - icon_frame_w - icon_start_x; //size of space where to paint icon
	int icon_space_mid = icon_start_x + icon_space_x/2;

	//get data of number icon and paint
	if (icon_name && *icon_name)
	{
               if (!active)
			icon_name = NEUTRINO_ICON_BUTTON_DUMMY_SMALL;

		frameBuffer->getIconSize(icon_name, &icon_w, &icon_h);

		if (/*active  &&*/ icon_w>0 && icon_h>0 && icon_space_x >= icon_w)
		{
			int icon_x = icon_space_mid - icon_w/2;
			int icon_y = y + item_height/2 - icon_h/2;
			icon_painted = frameBuffer->paintIcon(icon_name, icon_x, icon_y);
			if (icon_painted && (directKey != CRCInput::RC_nokey) && (directKey & CRCInput::RC_Repeat)) {
				static int longpress_icon_w = 0, longpress_icon_h = 0;
				if (!longpress_icon_w)
					frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_LONGPRESS, &longpress_icon_w, &longpress_icon_h);
				frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_LONGPRESS,
					std::min(icon_x + icon_w - longpress_icon_w/2, name_start_x - longpress_icon_w),
					std::min(icon_y + icon_h - longpress_icon_h/2, y + item_height - longpress_icon_h));
			}
		}
	}

	//paint only number if no icon was painted and keyval is numeric
	if (active && isNumeric && !icon_painted)
	{			
		int number_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(CRCInput::getKeyName(directKey));
		
		int number_x = icon_space_mid - (number_w/2);
		
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(number_x, y+ item_height, item_height, CRCInput::getKeyName(directKey), item_color, item_height);
	}

	//get data of number right info icon and paint
	if (iconName_Info_right)
	{
		frameBuffer->getIconSize(iconName_Info_right, &icon_w, &icon_h);

		if (icon_w>0 && icon_h>0)
		{
			frameBuffer->paintIcon(iconName_Info_right, dx + icon_start_x - (icon_w + 2*OFFSET_INNER_MID), y+ ((item_height/2- icon_h/2)) );
		}
	}
}

void CMenuItem::setIconName()
{
	iconName = NULL;

	switch (directKey & ~CRCInput::RC_Repeat) {
		case CRCInput::RC_red:
			iconName = NEUTRINO_ICON_BUTTON_RED;
			break;
		case CRCInput::RC_green:
			iconName = NEUTRINO_ICON_BUTTON_GREEN;
			break;
		case CRCInput::RC_yellow:
			iconName = NEUTRINO_ICON_BUTTON_YELLOW;
			break;
		case CRCInput::RC_blue:
			iconName = NEUTRINO_ICON_BUTTON_BLUE;
			break;
		case CRCInput::RC_standby:
			iconName = NEUTRINO_ICON_BUTTON_POWER;
			break;
		case CRCInput::RC_setup:
			iconName = NEUTRINO_ICON_BUTTON_MENU_SMALL;
			break;
		case CRCInput::RC_help:
			iconName = NEUTRINO_ICON_BUTTON_HELP_SMALL;
			break;
		case CRCInput::RC_info:
			iconName = NEUTRINO_ICON_BUTTON_INFO_SMALL;
			break;
		case CRCInput::RC_stop:
			iconName = NEUTRINO_ICON_BUTTON_STOP;
			break;
		case CRCInput::RC_0:
			iconName = NEUTRINO_ICON_BUTTON_0;
			break;
		case CRCInput::RC_1:
			iconName = NEUTRINO_ICON_BUTTON_1;
			break;
		case CRCInput::RC_2:
			iconName = NEUTRINO_ICON_BUTTON_2;
			break;
		case CRCInput::RC_3:
			iconName = NEUTRINO_ICON_BUTTON_3;
			break;
		case CRCInput::RC_4:
			iconName = NEUTRINO_ICON_BUTTON_4;
			break;
		case CRCInput::RC_5:
			iconName = NEUTRINO_ICON_BUTTON_5;
			break;
		case CRCInput::RC_6:
			iconName = NEUTRINO_ICON_BUTTON_6;
			break;
		case CRCInput::RC_7:
			iconName = NEUTRINO_ICON_BUTTON_7;
			break;
		case CRCInput::RC_8:
			iconName = NEUTRINO_ICON_BUTTON_8;
			break;
		case CRCInput::RC_9:
			iconName = NEUTRINO_ICON_BUTTON_9;
			break;
	}
}

void CMenuItem::setName(const std::string& t)
{
	name = NONEXISTANT_LOCALE;
	nameString = t;
}

void CMenuItem::setName(const neutrino_locale_t t)
{
	name = t;
	nameString = "";
}

const char *CMenuItem::getName(void)
{
	if (name != NONEXISTANT_LOCALE)
		return g_Locale->getText(name);
	return nameString.c_str();
}

void CMenuItem::setDescription(const std::string& t)
{
	desc = NONEXISTANT_LOCALE;
	descString = t;
	getHeight();
}

void CMenuItem::setDescription(const neutrino_locale_t t)
{
	desc = t;
	descString = "";
	getHeight();
}

const char *CMenuItem::getDescription(void)
{
	if (desc != NONEXISTANT_LOCALE)
		return g_Locale->getText(desc);
	return descString.c_str();
}

int CMenuItem::getDescriptionHeight(void)
{
	if (*getDescription())
		return g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]->getHeight();
	return 0;
}

int CMenuItem::getHeight(void)
{
	height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight() + getDescriptionHeight();
	return height;
}

void CMenuItem::activateNotify()
{
	if (actObserv)
		actObserv->activateNotify(name);
}

//small helper class to manage values e.g.: handling needed but deallocated widget objects
CMenuGlobal::CMenuGlobal()
{
	//creates needed select values with default value NO_WIDGET_ID = -1
	for (uint i=0; i<MN_WIDGET_ID_MAX; ++i)
		v_selected.push_back(NO_WIDGET_ID);
}

CMenuGlobal::~CMenuGlobal()
{
	v_selected.clear();
}

//Note: use only singleton to create an instance in the constructor or init handler of menu widget
CMenuGlobal* CMenuGlobal::getInstance()
{
	static CMenuGlobal* m = NULL;

	if(!m) 
		m = new CMenuGlobal();
	return m;
}
//****************************************************************************************

CMenuWidget::CMenuWidget()
{
	Init("", "", 0, 0);
}

CMenuWidget::CMenuWidget(const neutrino_locale_t Name, const std::string & Icon, const int mwidth, const mn_widget_id_t &w_index)
{
	Init(g_Locale->getText(Name), Icon, mwidth, w_index);
}

CMenuWidget::CMenuWidget(const std::string &Name, const std::string & Icon, const int mwidth, const mn_widget_id_t &w_index)
{
	Init(Name, Icon, mwidth, w_index);
}

void CMenuWidget::Init(const std::string &NameString, const std::string &Icon, const int mwidth, const mn_widget_id_t &w_index)
{
	//pos
	x = y		= 0;

	//caption and icon
	nameString 	= NameString;
	iconfile 	= Icon;

	//basic attributes
	iconOffset 	= 0;
	offx = offy 	= 0;
	from_wizard 	= SNeutrinoSettings::WIZARD_OFF;
	fade 		= true;
	scrollbar_width	= 0;
	savescreen	= false;
	preselected 	= -1;
	nextShortcut	= 1;
	current_page	= 0;
	has_hints	= false;
	brief_hints	= BRIEF_HINT_NO;
	hint_painted	= false;
	hint_height	= 0;
	fbutton_count	= 0;
	fbutton_labels	= NULL;
	footer_width	= 0;
	footer_height	= 0;
	saveScreen_width = 0;
	saveScreen_height = 0;

	//objects
	background	= NULL;
	details_line	= NULL;
	info_box	= NULL;
	header 		= NULL;
	footer		= NULL;
	frameBuffer 	= CFrameBuffer::getInstance();
	mglobal 	= CMenuGlobal::getInstance(); //create CMenuGlobal instance only here

	//handle select values
	if(w_index > MN_WIDGET_ID_MAX){
		//error
		fprintf(stderr, "Warning: %s Index ID value (%i) is bigger than MN_WIDGET_ID_MAX (%i)  \n", __FUNCTION__,w_index,MN_WIDGET_ID_MAX );
		widget_index  = NO_WIDGET_ID;
	}
	else{
		//ok
		widget_index = w_index;
	}
	
	//overwrite preselected value with global select value
	selected = (widget_index == NO_WIDGET_ID ? preselected : mglobal->v_selected[widget_index]);

	//dimension
	mwidth_save = mwidth;
	min_width = 0;
	width = 0; /* is set in paint() */
	if (mwidth > 100){
		/* warn about abuse until we found all offenders... */
		fprintf(stderr, "Warning: %s (%s) (%s) mwidth over 100%%: %d\n", __FUNCTION__, nameString.c_str(), Icon.c_str(), mwidth);
	}
	else{
		min_width = frameBuffer->getScreenWidth(true) * mwidth / 100;
		if(min_width > (int) frameBuffer->getScreenWidth())
			min_width = frameBuffer->getScreenWidth();
	}
}

void CMenuWidget::move(int xoff, int yoff)
{
	offx = xoff;
	offy = yoff;
}

CMenuWidget::~CMenuWidget()
{
	resetWidget(true);
	ResetModules();
}

void CMenuWidget::ResetModules()
{
	if (header){
		header->hide();
		delete header;
		header = NULL;
	}
	if (details_line){
		details_line->hide();
		delete details_line;
		details_line = NULL;
	}
	if (info_box){
		info_box->kill();
		delete info_box;
		info_box = NULL;
	}
	if (footer){
		footer->kill();
		delete footer;
		footer = NULL;
	}
}

void CMenuWidget::addItem(CMenuItem* menuItem, const bool defaultselected)
{
	if (menuItem->isSelectable())
	{
		bool isSelected = defaultselected;

		if (preselected != -1)
			isSelected = (preselected == (int)items.size());

		if (isSelected)
			selected = items.size();
	}

	menuItem->isUsed();
	items.push_back(menuItem);
	menuItem->setParentWidget(this);
}

void CMenuWidget::resetWidget(bool delete_items)
{
	for(unsigned int count=0;count<items.size();count++) {
		CMenuItem * item = items[count];
		if (delete_items && !item->isStatic){
			delete item;
			item = NULL;
		}
	}
	
	items.clear();
	page_start.clear();
	selected=-1;
}

void CMenuWidget::insertItem(const uint& item_id, CMenuItem* menuItem)
{
	items.insert(items.begin()+item_id, menuItem);
}

void CMenuWidget::removeItem(const uint& item_id)
{
	items.erase(items.begin()+item_id);
	if ((unsigned int) selected >= items.size())
		selected = items.size() - 1;
	while (selected > 0 && !items[selected]->active)
		selected--;
}

bool CMenuWidget::hasItem()
{
	return !items.empty();
}

int CMenuWidget::getItemId(CMenuItem* menuItem)
{
	for (uint i= 0; i< items.size(); i++)
		if (items[i] == menuItem)
			return i;
	return -1;
}

CMenuItem* CMenuWidget::getItem(const uint& item_id)
{
	for (uint i= 0; i< items.size(); i++)
		if (i == item_id)
			return items[i];
	return NULL;
}

const char *CMenuWidget::getName()
{
	return nameString.c_str();
}

int CMenuWidget::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	bool bAllowRepeatLR = false;
	CVFD::MODES oldLcdMode = CVFD::getInstance()->getMode();

	exit_pressed = false;

	frameBuffer->Lock();

	if (parent)
		parent->hide();

	COSDFader fader(g_settings.theme.menu_Content_alpha);
	if(fade)
		fader.StartFadeIn();

	if(from_wizard) {
		for (unsigned int count = 0; count < items.size(); count++) {
			if(items[count] == GenericMenuBack) {
				items[count] = GenericMenuNext;
				break;
			}
		}
	}

	checkHints();

	if(savescreen) {
		calcSize();
		saveScreen();
	}

	/* make sure we start with a selectable item... */
	initSelectable();

	paint();
	frameBuffer->blit();

	int pos = selected;

	int retval = menu_return::RETURN_REPAINT;
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
		
	bool bAllowRepeatLR_override = keyActionMap.find(CRCInput::RC_left) != keyActionMap.end() || keyActionMap.find(CRCInput::RC_right) != keyActionMap.end();
	do {
		if(hasItem() && selected >= 0 && (int)items.size() > selected )
			bAllowRepeatLR = items[selected]->isMenueOptionChooser();
		bAllowRepeatLR |= bAllowRepeatLR_override;

		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, bAllowRepeatLR);

		int handled= false;
		if ( msg <= CRCInput::RC_MaxRC ) {
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

			std::map<neutrino_msg_t, keyAction>::iterator it = keyActionMap.find(msg);
			if (it != keyActionMap.end()) {
				fader.StopFade();
				int rv = it->second.menue->exec(this, it->second.action);
				switch ( rv ) {
					case menu_return::RETURN_EXIT_ALL:
						retval = menu_return::RETURN_EXIT_ALL;
						/* fall through */
					case menu_return::RETURN_EXIT:
						msg = CRCInput::RC_timeout;
						break;
					case menu_return::RETURN_REPAINT:
					case menu_return::RETURN_EXIT_REPAINT:
						if (fade && washidden)
							fader.StartFadeIn();
						checkHints();
						paint();
						break;
				}
				continue;
			}
			for (unsigned int i= 0; i< items.size(); i++) {
				CMenuItem* titem = items[i];
				if ((titem->directKey != CRCInput::RC_nokey) && (titem->directKey == msg)) {
					if (titem->isSelectable()) {
						items[selected]->paint( false );
						selected= i;
						if (selected > page_start[current_page + 1] || selected < page_start[current_page]) {
							/* different page */
							paintItems();
						}
						paintHint(selected);
						pos = selected;
						if (titem->directKeyOK)
							msg = CRCInput::RC_ok;
						else
							msg = CRCInput::RC_nokey;
					} else {
						// swallow-key...
						handled= true;
					}
					break;
				}
			}
#if 0
			if (msg == (uint32_t) g_settings.key_pageup)
				msg = CRCInput::RC_page_up;
			else if (msg == (uint32_t) g_settings.key_pagedown)
				msg = CRCInput::RC_page_down;
#endif
		}

		if (handled)
			continue;

		switch (msg) {
			case (NeutrinoMessages::EVT_TIMER):
				if(data == fader.GetFadeTimer()) {
					if(fader.FadeDone())
						msg = CRCInput::RC_timeout;
				} else {
					if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
						retval = menu_return::RETURN_EXIT_ALL;
						msg = CRCInput::RC_timeout;
					}
				}
				break;
			case CRCInput::RC_page_up:
			case CRCInput::RC_page_down:
			case CRCInput::RC_up:
			case CRCInput::RC_down:
			case CRCInput::RC_nokey:
			{
				/* dir and wrap are used when searching for a selectable item:
				 * if the item is not selectable, try the previous (dir = -1) or
				 * next (dir = 1) item, until a selectale item is found.
				 * if wrap = true, allows to wrap around while searching */
				int dir = 1;
				bool wrap = false;
				switch (msg) {
					case CRCInput::RC_nokey:
						break;
					case CRCInput::RC_page_up:
						if (current_page) {
							pos = page_start[current_page] - 1;
							dir = -1; /* pg_up: search upwards */
						} else
							pos = 0; /* ...but not if already at top */
						break;
					case CRCInput::RC_page_down:
						pos = page_start[current_page + 1];
						if (pos >= (int)items.size()) {
							pos = items.size() - 1;
							dir = -1; /* if last item is not selectable, go up */
						}
						break;
					case CRCInput::RC_up:
						dir = -1;
						/* fall through */
					default: /* fallthrough or RC_down => dir = 1 */
						pos += dir;
						if (pos < 0 || pos >= (int)items.size())
							pos -= dir * items.size();
						wrap = true;
				}
				if (!items.empty() && pos >= (int)items.size())
					pos = (int)items.size() - 1;
				do {
					if(items.empty())
						break;
					CMenuItem* item = items[pos];
					if (item->isSelectable()) {
						if (pos < page_start[current_page + 1] && pos >= page_start[current_page]) {
							/* in current page */
							items[selected]->paint(false);
							item->activateNotify();
							item->paint(true);
							paintHint(pos);
							selected = pos;
						} else {
							/* different page */
							selected = pos;
							paintItems();
						}
						break;
					}
					pos += dir;
					if (wrap && (pos >= (int)items.size() || pos < 0)) {
						pos -= dir * items.size();
						wrap = false; /* wrap only once, avoids endless loop */
					}
				} while (pos >= 0 && pos < (int)items.size());
				break;
			}
			case (CRCInput::RC_left):
				if (hasItem() && selected > -1 && (int)items.size() > selected) {
					CMenuItem* itemX = items[selected];
					if (!itemX->isMenueOptionChooser() && g_settings.menu_left_exit)
						msg = CRCInput::RC_timeout;
				}
				break;
			case (CRCInput::RC_right):
			case (CRCInput::RC_ok):
				{
					if(hasItem() && selected > -1 && (int)items.size() > selected) {
						//exec this item...
						CMenuItem* item = items[selected];
						if (!item->isSelectable())
							break;
						item->msg = msg;
						fader.StopFade();
						int rv = item->exec( this );
						switch ( rv ) {
							case menu_return::RETURN_EXIT_ALL:
								retval = menu_return::RETURN_EXIT_ALL;
								/* fall through */
							case menu_return::RETURN_EXIT:
								msg = CRCInput::RC_timeout;
								break;
							case menu_return::RETURN_REPAINT:
							case menu_return::RETURN_EXIT_REPAINT:
								if (fade && washidden)
									fader.StartFadeIn();
								checkHints();
								pos = selected;
								paint();
								break;
						}
					} else
						msg = CRCInput::RC_timeout;
				}
				break;

			case (CRCInput::RC_home):
				exit_pressed = true;
				msg = CRCInput::RC_timeout;
				break;
			case (CRCInput::RC_timeout):
				break;
			case (CRCInput::RC_setup):
				//close any menu on menu-key
				{
					msg = CRCInput::RC_timeout;
					retval = menu_return::RETURN_EXIT_ALL;
				}
				break;
			case (CRCInput::RC_help):
				// FIXME should we switch hints in menu without hints ?
				checkHints();
				if (has_hints)
					hide();
				g_settings.show_menu_hints = !g_settings.show_menu_hints;
				if (has_hints)
					paint();
				break;

			default:
				if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
					g_RCInput->postMsg (msg, 0);
					retval = menu_return::RETURN_EXIT_ALL;
					msg = CRCInput::RC_timeout;
				}
				else if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
					retval = menu_return::RETURN_EXIT_ALL;
					msg = CRCInput::RC_timeout;
				}
		}
		if(msg == CRCInput::RC_timeout) {
			if(fade && fader.StartFadeOut()) {
				timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
				msg = 0;
				continue;
			}
		}

		if ( msg <= CRCInput::RC_MaxRC )
		{
			// recalculate timeout for RC-keys
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
		}
		frameBuffer->blit();
	}
	while ( msg!=CRCInput::RC_timeout );
	hide();
	if (background) {
		delete[] background;
		background = NULL;
	}

	fader.StopFade();

	if(!parent)
		if(oldLcdMode != CVFD::getInstance()->getMode())
			CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);

	for (unsigned int count = 0; count < items.size(); count++) 
	{
		if(items[count] == GenericMenuNext) 
			items[count] = GenericMenuBack;
		else if (items[count] == GenericMenuCancel)
			items[count] = GenericMenuBack;
	}
	
 	if (widget_index > -1)
 		mglobal->v_selected[widget_index] = selected;

	frameBuffer->Unlock();
	return retval;
}

void CMenuWidget::integratePlugins(int integration, const unsigned int shortcut, bool enabled)
{
	bool separatorline = false;
	unsigned int number_of_plugins = (unsigned int) g_Plugins->getNumberOfPlugins();
	unsigned int sc = shortcut;
	for (unsigned int count = 0; count < number_of_plugins; count++)
	{
		if ((g_Plugins->getIntegration(count) == integration) && !g_Plugins->isHidden(count))
		{
			if (!separatorline)
			{
				addItem(GenericMenuSeparatorLine);
				separatorline = true;
			}
			printf("[neutrino] integratePlugins: add %s\n", g_Plugins->getName(count));
			neutrino_msg_t dk = (shortcut != CRCInput::RC_nokey) ? CRCInput::convertDigitToKey(sc++) : CRCInput::RC_nokey;
			CMenuForwarder *fw_plugin = new CMenuForwarder(g_Plugins->getName(count), enabled, NULL, CPluginsExec::getInstance(), to_string(count).c_str(), dk);
			fw_plugin->setHint(g_Plugins->getHintIcon(count), g_Plugins->getDescription(count));
			addItem(fw_plugin);
		}
	}
}

void CMenuWidget::hide()
{
	if(savescreen && background) {
		ResetModules();
		restoreScreen();//FIXME
	} else {
		if (header)
			header->kill();
		if (info_box)
			info_box->kill();
		if (details_line)
			details_line->hide();
		frameBuffer->paintBackgroundBoxRel(x, y, full_width, full_height/* + footer_height*/);	// full_height includes footer_height : see calcSize
		//paintHint(-1);
	}
	paintHint(-1);
	frameBuffer->blit();

	/* setActive() paints item for hidden parent menu, if called from child menu */
	for (unsigned int count = 0; count < items.size(); count++) 
		items[count]->init(-1, 0, 0, 0);
	hint_painted	= false;
	washidden = true;
	if (CInfoClock::getInstance()->isRun())
		CInfoClock::getInstance()->enableInfoClock(!CInfoClock::getInstance()->isBlocked());
	OnAfterHide();
}

void CMenuWidget::checkHints()
{
	brief_hints = (brief_hints || (from_wizard == SNeutrinoSettings::WIZARD_START));

	GenericMenuBack->setHint("", NONEXISTANT_LOCALE);
	GenericMenuNext->setHint("", NONEXISTANT_LOCALE);
	for (unsigned int i= 0; i< items.size(); i++) {
		if(items[i]->hintIcon || items[i]->hint != NONEXISTANT_LOCALE || !items[i]->hintText.empty()) {
			has_hints = true;
			break;
		}
	}
	if (has_hints) {
		GenericMenuBack->setHint(NEUTRINO_ICON_HINT_BACK, brief_hints ? LOCALE_MENU_HINT_BACK_BRIEF : LOCALE_MENU_HINT_BACK);
		GenericMenuNext->setHint(NEUTRINO_ICON_HINT_NEXT, brief_hints ? LOCALE_MENU_HINT_NEXT_BRIEF : LOCALE_MENU_HINT_NEXT);
	}
}

void CMenuWidget::calcSize()
{
	// recalc min_width
	min_width = 0;
	int mwidth = std::min(mwidth_save, 100);
	min_width = frameBuffer->getScreenWidth(true) * mwidth / 100;
	if (min_width > (int)frameBuffer->getScreenWidth())
		min_width = frameBuffer->getScreenWidth();

	width = min_width;

	int wi, hi;
	for (unsigned int i= 0; i< items.size(); i++) {
		wi = 0;
		if (items[i]->iconName_Info_right) {
			frameBuffer->getIconSize(items[i]->iconName_Info_right, &wi, &hi);
			if ((wi > 0) && (hi > 0))
				wi += OFFSET_INNER_MID;
			else
				wi = 0;
		}
		int tmpw = items[i]->getWidth() + 2*OFFSET_INNER_MID + wi; /* 10 pixels to the left and right of the text */
		if (tmpw > width)
			width = tmpw;
	}
	hint_height = 0;
	if(g_settings.show_menu_hints && has_hints) {
		int lines = 2;
		int text_height = 2*OFFSET_INNER_SMALL + lines*g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT]->getHeight();
		/* assuming all hint icons has the same size ! */
		int icon_width, icon_height;
		frameBuffer->getIconSize(NEUTRINO_ICON_HINT_TVMODE, &icon_width, &icon_height);
		icon_height += 2*OFFSET_INNER_SMALL;
		hint_height = std::max(icon_height, text_height);
	}
	/* set the max height to 9/10 of usable screen height
	   debatable, if the callers need a possibility to set this */
	height = (frameBuffer->getScreenHeight() - footer_height - hint_height) / 20 * 18; /* make sure its a multiple of 2 */

	if (height > ((int)frameBuffer->getScreenHeight() - 2*OFFSET_INNER_MID))
		height = frameBuffer->getScreenHeight() - 2*OFFSET_INNER_MID;

	int neededWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(getName());
	if (neededWidth > width - frameBuffer->scale2Res(48)) {
		width = neededWidth + frameBuffer->scale2Res(48)+1;
	}
	hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();

	int heightCurrPage=0;
	page_start.clear();
	page_start.push_back(0);
	total_pages=1;

	int maxItemHeight = 0;

	for (unsigned int i= 0; i< items.size(); i++) {
		int item_height=items[i]->getHeight();
		heightCurrPage+=item_height;
		if(heightCurrPage > (height-hheight)) {
			page_start.push_back(i);
			total_pages++;
			maxItemHeight = std::max(heightCurrPage - item_height, maxItemHeight);
			heightCurrPage=item_height;
		}
	}
	maxItemHeight = std::max(heightCurrPage, maxItemHeight);
	page_start.push_back(items.size());

	iconOffset= 0;
	for (unsigned int i= 0; i< items.size(); i++)
		if (items[i]->iconName /*&& !g_settings.menu_numbers_as_icons*/)
		{
			int w, h;
			frameBuffer->getIconSize(items[i]->iconName, &w, &h);
			if (w > iconOffset)
				iconOffset = w;
		}

	iconOffset += OFFSET_INNER_MID;
	width += iconOffset;

	if (fbutton_count)
		width = std::max(width, footer_width);

	if (width > (int)frameBuffer->getScreenWidth())
		width = frameBuffer->getScreenWidth();

	// shrink menu if less items
	height = std::min(height, hheight + maxItemHeight);
	/*
	   Always add a bottom separator offset.
	   Most menus has an upper offset too,
	   which is added with the intro-items
	*/
	CMenuItem *separator = new CMenuSeparator();
	height += separator->getHeight();
	
	//scrollbar width
	scrollbar_width=0;
	if (total_pages > 1)
		scrollbar_width = SCROLLBAR_WIDTH;

	full_width = width + scrollbar_width + OFFSET_SHADOW;
	full_height = height + footer_height + OFFSET_SHADOW/* + OFFSET_INTER*/; // hintbox is handled separately

	/* + DETAILSLINE_WIDTH for the hintbox connection line
	 * + center_offset for symmetry
	 * + 20 for setMenuPos calculates 10 pixels border left and right */
	int center_offset = (g_settings.menu_pos == MENU_POS_CENTER) ? DETAILSLINE_WIDTH : 0;
	int max_possible = (int)frameBuffer->getScreenWidth() - DETAILSLINE_WIDTH - center_offset - 2*OFFSET_INNER_MID;
	if (full_width > max_possible)
	{
		width = max_possible - scrollbar_width - OFFSET_SHADOW;
		full_width = max_possible + center_offset; /* symmetry in MENU_POS_CENTER case */
	}

	setMenuPos(full_width);
}

void CMenuWidget::initSelectable()
{
	int pos = 0;
	if (selected > 0 && selected < (int)items.size())
		pos = selected;
	else
		selected = -1;

	while (pos < (int)items.size()) {
		if (items[pos]->isSelectable())
			break;
		pos++;
	}
	selected = pos;
}

void CMenuWidget::paint()
{
	if (header){
		if ((bool)header->getCornerRadius() != (bool)g_settings.rounded_corners) //ensure reset if corner mode was changed
			ResetModules();
	}

	if (CInfoClock::getInstance()->isRun())
		CInfoClock::getInstance()->disableInfoClock();
	calcSize();
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8 /*, nameString.c_str()*/);

	/* prepare footer:
	 * We must prepare footer, to get current footer dimensions,
	 * otherwise footer will paint on wrong position
	*/
	setFooter(fbutton_labels, fbutton_count);

	OnBeforePaint();

	// paint head
	if (header == NULL){
		header = new CComponentsHeader(x, y, width + scrollbar_width, hheight, getName(), iconfile);
		header->enableShadow(CC_SHADOW_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT | CC_SHADOW_CORNER_BOTTOM_RIGHT);
		header->setOffset(OFFSET_INNER_MID);
	}
	header->setCaption(getName());
	header->setColorAll(COL_FRAME_PLUS_0, COL_MENUHEAD_PLUS_0, COL_SHADOW_PLUS_0);
	header->setCaptionColor(COL_MENUHEAD_TEXT);
	header->enableColBodyGradient(g_settings.theme.menu_Head_gradient, COL_MENUCONTENT_PLUS_0);
	header->enableGradientBgCleanUp(savescreen);
	header->paint(CC_SAVE_SCREEN_NO);

	// paint body background
	PaintBoxRel(x, y+hheight, width + scrollbar_width, height-hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE,
				(fbutton_count ? CORNER_NONE : CORNER_BOTTOM), footer && fbutton_count ? CC_SHADOW_RIGHT_CORNER_ALL : CC_SHADOW_ON);

	item_start_y = y+hheight;
	paintItems();
	washidden = false;

	// Finally paint footer if buttons are defined.
	if (footer && fbutton_count)
		footer->paint(CC_SAVE_SCREEN_NO);
}

void CMenuWidget::setMenuPos(const int& menu_width)
{
	int scr_x = frameBuffer->getScreenX();
	int scr_y = frameBuffer->getScreenY();
	int scr_w = frameBuffer->getScreenWidth();
	int scr_h = frameBuffer->getScreenHeight();
	int real_h = full_height/* + footer_height*/ + hint_height;		// full_height includes footer_height : see calcSize
	int x_old = x;
	int y_old = y;
	//configured positions 
	switch(g_settings.menu_pos) 
	{
		case MENU_POS_CENTER:
			x = offx + scr_x + ((scr_w - menu_width ) >> 1 );
			y = offy + scr_y + ((scr_h - real_h) >> 1 );
			x += DETAILSLINE_WIDTH;
			break;
			
		case MENU_POS_TOP_LEFT: 
			y = offy + scr_y + OFFSET_INNER_MID;
			x = offx + scr_x + OFFSET_INNER_MID;
			x += DETAILSLINE_WIDTH;
			break;
			
		case MENU_POS_TOP_RIGHT: 
			y = offy + scr_y + OFFSET_INNER_MID;
			x = /*offx +*/ scr_x + scr_w - menu_width - OFFSET_INNER_MID;
			break;
			
		case MENU_POS_BOTTOM_LEFT: 
			y = /*offy +*/ scr_y + scr_h - real_h - OFFSET_INNER_MID;
			x = offx + scr_x + OFFSET_INNER_MID;
			x += DETAILSLINE_WIDTH;
			break;
			
		case MENU_POS_BOTTOM_RIGHT: 
			y = /*offy +*/ scr_y + scr_h - real_h - OFFSET_INNER_MID;
			x = /*offx +*/ scr_x + scr_w - menu_width - OFFSET_INNER_MID;
			break;
	}
	if (x_old != x || y_old != y)
		ResetModules();
}

void CMenuWidget::paintItems()
{	
	//Item not currently on screen
	if (selected >= 0)
	{
		while (current_page > 0 && selected < page_start[current_page])
			current_page--;
		while (current_page+1 < page_start.size() && selected >= page_start[current_page + 1])
			current_page++;
	}

	// Scrollbar
	if(total_pages>1)
	{
		int scrollbar_height = height - hheight - OFFSET_INNER_MID;
		paintScrollBar(x + width, item_start_y, scrollbar_width, scrollbar_height, total_pages, current_page);
		/* background of menu items, paint every time because different items can have
		 * different height and this might leave artifacts otherwise after changing pages */
		frameBuffer->paintBoxRel(x, item_start_y, width, scrollbar_height, COL_MENUCONTENT_PLUS_0);
	}

	int ypos=item_start_y;
	for (int count = 0; count < (int)items.size(); count++)
	{
		CMenuItem* item = items[count];
		item->OnPaintItem();
		if ((count >= page_start[current_page]) &&
		    (count < page_start[current_page + 1]))
		{
			item->init(x, ypos, width, iconOffset);
			if (item->isSelectable() && selected == -1)
				selected = count;

			if (selected == count) {
				item->activateNotify();
				paintHint(count);
			}
			ypos = item->paint(selected == count);
		}
		else
		{
			/* x = -1 is a marker which prevents the item from being painted on setActive changes */
			item->init(-1, 0, 0, 0);
		}
	}
}

/*adds the typical menu intro with optional subhead, separator, back or cancel button and separatorline to menu*/
void CMenuWidget::addIntroItems(neutrino_locale_t subhead_text, neutrino_locale_t section_text, int buttontype, bool brief_hint)
{
	brief_hints = brief_hint;

	if (subhead_text != NONEXISTANT_LOCALE)
		addItem(new CMenuSeparator(CMenuSeparator::ALIGN_LEFT | CMenuSeparator::SUB_HEAD | CMenuSeparator::STRING, subhead_text));

	addItem(GenericMenuSeparator);
	
	switch (buttontype) {
		case BTN_TYPE_BACK:
			GenericMenuBack->setItemButton(!g_settings.menu_left_exit ? NEUTRINO_ICON_BUTTON_HOME : NEUTRINO_ICON_BUTTON_LEFT); 
			addItem(GenericMenuBack);
			break;
		case BTN_TYPE_CANCEL:
			addItem(GenericMenuCancel);
			break;
		case BTN_TYPE_NEXT:
			addItem(GenericMenuNext);
			break;
	}
	
	if (section_text != NONEXISTANT_LOCALE)
		addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, section_text));
	else if (buttontype != BTN_TYPE_NO)
		addItem(GenericMenuSeparatorLine);
}

void CMenuWidget::saveScreen()
{
	if(!savescreen)
		return;

	delete[] background;
	saveScreen_height = full_height/* + footer_height*/;	// full_height includes footer_height : see calcSize
	saveScreen_width = full_width;
	saveScreen_y = y;
	saveScreen_x = x;
	background = new fb_pixel_t [saveScreen_height * saveScreen_width];
	if(background)
		frameBuffer->SaveScreen(saveScreen_x /*-DETAILSLINE_WIDTH*/, saveScreen_y, saveScreen_width, saveScreen_height, background);
}

void CMenuWidget::restoreScreen()
{
	if(background) {
		if(savescreen)
			frameBuffer->RestoreScreen(saveScreen_x /*-DETAILSLINE_WIDTH*/, saveScreen_y, saveScreen_width, saveScreen_height, background);
	}
}

void CMenuWidget::enableSaveScreen(bool enable)
{
	savescreen = enable;
	if (!enable && background) {
		delete[] background;
		background = NULL;
		saveScreen_width = 0;
		saveScreen_height = 0;
		saveScreen_y = 0;
		saveScreen_x = 0;
	}
}

void CMenuWidget::paintHint(int pos)
{
	if (!g_settings.show_menu_hints){
		//ResetModules(); //ensure clean up on changed setting
		return;
	}

	if (pos < 0 && !hint_painted)
		return;

	if (hint_painted) {
		/* clear detailsline line */
		if (details_line)
			details_line->hide();
		/* clear info box */
		if ((info_box) && ((pos < 0) || savescreen))
			savescreen ? info_box->hide() : info_box->kill();
		if (info_box)
			hint_painted = info_box->isPainted();
	}
	if (pos < 0)
		return;

	CMenuItem* item = items[pos];
	
	if (!item->hintIcon && item->hint == NONEXISTANT_LOCALE && item->hintText.empty()) {
		if (info_box) {
			savescreen ? info_box->hide() : info_box->kill();
			hint_painted = info_box->isPainted();
			if (details_line)
				details_line->hide();
		}
		return;
	}
	
	if (item->hint == NONEXISTANT_LOCALE && item->hintText.empty())
		item->hintText = " ";

	int iheight = item->getHeight();
	int xpos  = x - DETAILSLINE_WIDTH;
	int ypos2 = y + height + footer_height + OFFSET_SHADOW + OFFSET_INTER;
	int iwidth = width + scrollbar_width;
	
	//init details line and infobox dimensions
	int ypos1 = item->getYPosition();
	int ypos1a = ypos1 + (iheight/2);
	int ypos2a = ypos2 + (hint_height/2);
	int rad = RADIUS_LARGE;
	int markh = hint_height > rad*2 ? hint_height - rad*2 : hint_height;
	int imarkh = iheight/2;
	
	//init details line
	if (details_line == NULL)
		details_line = new CComponentsDetailsLine();

	details_line->setXPos(xpos);
	details_line->setYPos(ypos1a);
	details_line->setYPosDown(ypos2a);
	details_line->setHMarkTop(imarkh);
	details_line->setHMarkDown(markh);
	details_line->syncSysColors();

	//init infobox
	std::string str = item->hintText.empty() ? g_Locale->getText(item->hint) : item->hintText;
	if (info_box == NULL)
		info_box = new CComponentsInfoBox();

	info_box->setDimensionsAll(x, ypos2, iwidth, hint_height);
	info_box->setFrameThickness(FRAME_WIDTH_MIN);
	info_box->removeLineBreaks(str);
	info_box->setText(str, CTextBox::AUTO_WIDTH, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_HINT], COL_MENUCONTENT_TEXT);
	info_box->setCorner(rad);
	info_box->setColorAll(COL_FRAME_PLUS_0, COL_MENUCONTENTDARK_PLUS_0);
	info_box->setTextColor(COL_MENUCONTENTDARK_TEXT);
	info_box->enableShadow();
	info_box->setPicture(item->hintIcon ? item->hintIcon : "");
	info_box->enableColBodyGradient(g_settings.theme.menu_Hint_gradient, COL_MENUFOOT_PLUS_0, g_settings.theme.menu_Hint_gradient_direction);// COL_MENUFOOT_PLUS_0 is default footer color

	//paint result
	if (details_line)
		details_line->paint();
	if (info_box)
		info_box->paint(savescreen);
	
	hint_painted = info_box ? info_box->isPainted() : false;
}

void CMenuWidget::addKey(neutrino_msg_t key, CMenuTarget *menue, const std::string & action)
{
	keyActionMap[key].menue = menue;
	keyActionMap[key].action = action;
}

void CMenuWidget::setFooter(const struct button_label *_fbutton_labels, const int _fbutton_count, bool repaint)
{
	fbutton_count = _fbutton_count;
	fbutton_labels = _fbutton_labels;

	if (fbutton_count){
		if (!footer)
			footer = new CComponentsFooter(x, y + height, width + scrollbar_width, 0, 0, NULL, CC_SHADOW_ON);
		footer->setWidth(width + scrollbar_width);
		footer->setButtonLabels(fbutton_labels, fbutton_count, 0, width/fbutton_count);
		footer_height = footer->getHeight();
		footer_width = footer->getWidth();
	}else{
		if (footer){
			delete footer; footer = NULL;
		}
		footer_width = 0;
		footer_height = 0;
	}

	if (repaint)
		paint();
}


//-------------------------------------------------------------------------------------------------------------------------------
CMenuOptionNumberChooser::CMenuOptionNumberChooser(	const neutrino_locale_t Name, int * const OptionValue, const bool Active,
							const int min_value, const int max_value,
							CChangeObserver * const Observ, const neutrino_msg_t DirectKey, const char * const IconName,
							const int print_offset, const int special_value, const neutrino_locale_t special_value_name, bool sliderOn)
								: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init(Name, "", OptionValue, min_value, max_value, print_offset, special_value, special_value_name, Observ, sliderOn);
}

CMenuOptionNumberChooser::CMenuOptionNumberChooser(	const std::string &Name, int * const OptionValue, const bool Active,
							const int min_value, const int max_value,
							CChangeObserver * const Observ, const neutrino_msg_t DirectKey, const char * const IconName,
							const int print_offset, const int special_value, const neutrino_locale_t special_value_name, bool sliderOn)
								: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init(NONEXISTANT_LOCALE, Name, OptionValue, min_value, max_value, print_offset, special_value, special_value_name, Observ, sliderOn);
}

void CMenuOptionNumberChooser::init(	const neutrino_locale_t& lName,
					const std::string &sName,
					int* const Option_Value,
					const int& min_value,
					const int& max_value,
					const int& print_offset,
					const int& special_value,
					const neutrino_locale_t& special_value_name,
					CChangeObserver * const Observ,
					bool sliderOn)
{
	name			= lName;
	nameString		= sName;
	optionValue		= Option_Value;
	lower_bound		= min_value;
	upper_bound		= max_value;
	display_offset		= print_offset;
	localized_value		= special_value;
	localized_value_name 	= special_value_name;
	observ			= Observ;
	slider_on		= sliderOn;

	numberFormat		= "%d";
	numberFormatFunction 	= NULL;
	directKeyOK		= false;
	numeric_input		= false;
}

int CMenuOptionNumberChooser::exec(CMenuTarget*)
{
	int res = menu_return::RETURN_NONE;

	if(msg == CRCInput::RC_left) {
		if (((*optionValue) > upper_bound) || ((*optionValue) <= lower_bound))
			*optionValue = upper_bound;
		else
			(*optionValue)--;
	} else if (numeric_input && msg == CRCInput::RC_ok) {
		int size = 0;
		int b = lower_bound;
		if (b < 0) {
			size++,
			b = -b;
		}
		if (b < upper_bound)
			b = upper_bound;
		for (; b; b /= 10, size++);
		CIntInput cii(name, optionValue, size, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
		cii.exec(NULL, "");
		if (*optionValue > upper_bound)
			*optionValue = upper_bound;
		else if (*optionValue < lower_bound)
			*optionValue = lower_bound;
		res = menu_return::RETURN_REPAINT;
	} else {
		if (((*optionValue) >= upper_bound) || ((*optionValue) < lower_bound))
			*optionValue = lower_bound;
		else
			(*optionValue)++;
	}

	bool wantsRepaint = false;
	if(observ && !luaAction.empty()) {
		// optionValue is int*
		wantsRepaint = observ->changeNotify(luaState, luaAction, luaId, (void *) to_string(*optionValue).c_str());
	} else if(observ)
		wantsRepaint = observ->changeNotify(name, optionValue);

	// give the observer a chance to modify the value
	paint(true);
	OnAfterChangeOption();
	if (wantsRepaint)
		res = menu_return::RETURN_REPAINT;

	return res;
}

int CMenuOptionNumberChooser::paint(bool selected)
{
	const char * l_option;
	char option_value[40];

	if ((localized_value_name == NONEXISTANT_LOCALE) || ((*optionValue) != localized_value))
	{
		if (numberFormatFunction) {
			std::string s = numberFormatFunction(*optionValue + display_offset);
			strncpy(option_value, s.c_str(), sizeof(option_value));
		} else
			sprintf(option_value, numberFormat.c_str(), *optionValue + display_offset);
		l_option = option_value;
	}
	else
		l_option = g_Locale->getText(localized_value_name);
	
	//paint item
	prepareItem(selected, height);

	//paint item icon
	paintItemButton(selected, height, NEUTRINO_ICON_BUTTON_OKAY);
	if(slider_on)
		paintItemSlider(selected, height, *optionValue, (upper_bound - lower_bound), getName(), l_option);
	//paint text
	paintItemCaption(selected, l_option);

	return y+height;
}

int CMenuOptionNumberChooser::getWidth(void)
{
	int width = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(getName());
	int _lower_bound = lower_bound;
	int _upper_bound = upper_bound;
	int m = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getMaxDigitWidth();

	int w1 = 0;
	if (_lower_bound < 0)
		w1 += g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("-");
	while (_lower_bound) {
		w1 += m;
		_lower_bound /= 10;
	}

	int w2 = 0;
	if (_upper_bound < 0)
		w2 += g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("-");
	while (_upper_bound) {
		w1 += m;
		_upper_bound /= 10;
	}

	width += (w1 > w2) ? w1 : w2;

	if (numberFormatFunction) {
		std::string s = numberFormatFunction(0);
		width += g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(s) - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("0"); // arbitrary
	} else if (numberFormat != "%d") {
		char format[numberFormat.length()];
		snprintf(format, numberFormat.length(), numberFormat.c_str(), 0);
		width += g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(format) - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("0");
	}

	width += OFFSET_INNER_MID; /* min 10 pixels between option name and value. enough? */

	const char *desc_text = getDescription();
	if (*desc_text)
		width = std::max(width, OFFSET_INNER_MID + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(desc_text));
	return width;
}

CMenuOptionChooser::CMenuOptionChooser(	const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval * const Options, const size_t Number_Of_Options,
					const bool Active, CChangeObserver * const Observ,
					const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown, bool OptionsSort)
						: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init("", OptionName, OptionValue, Options, NULL, NULL, Number_Of_Options, Observ, Pulldown, OptionsSort);
}

CMenuOptionChooser::CMenuOptionChooser(	const std::string &OptionName, int * const OptionValue, const struct keyval * const Options, const size_t Number_Of_Options,
					const bool Active, CChangeObserver * const Observ,
					const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown, bool OptionsSort)
						: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init(OptionName, NONEXISTANT_LOCALE, OptionValue, Options, NULL, NULL, Number_Of_Options, Observ, Pulldown, OptionsSort);
}

CMenuOptionChooser::CMenuOptionChooser(	const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval_ext * const Options, const size_t Number_Of_Options, 
					const bool Active, CChangeObserver * const Observ,
					const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown, bool OptionsSort)
						: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init("", OptionName, OptionValue, NULL, Options,  NULL, Number_Of_Options, Observ, Pulldown, OptionsSort);
}

CMenuOptionChooser::CMenuOptionChooser(	const std::string &OptionName, int * const OptionValue, const struct keyval_ext * const Options, const size_t Number_Of_Options,
					const bool Active, CChangeObserver * const Observ,
					const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown, bool OptionsSort)
						: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init(OptionName, NONEXISTANT_LOCALE, OptionValue, NULL, Options, NULL, Number_Of_Options, Observ, Pulldown, OptionsSort);
}

CMenuOptionChooser::CMenuOptionChooser(	const neutrino_locale_t OptionName, int * const OptionValue, std::vector<keyval_ext> &Options,
					const bool Active, CChangeObserver * const Observ,
					const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown, bool OptionsSort)
						: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init("", OptionName, OptionValue, NULL, NULL, &Options, Options.size(), Observ, Pulldown, OptionsSort);
}

CMenuOptionChooser::CMenuOptionChooser(const std::string &OptionName, int * const OptionValue, std::vector<keyval_ext> &Options,
				       const bool Active, CChangeObserver * const Observ,
				       const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown, bool OptionsSort)
						: CAbstractMenuOptionChooser(Active, DirectKey, IconName)
{
	init(OptionName, NONEXISTANT_LOCALE, OptionValue, NULL, NULL, &Options, Options.size(), Observ, Pulldown, OptionsSort);
}

CMenuOptionChooser::~CMenuOptionChooser()
{
	clearChooserOptions();
}

void CMenuOptionChooser::init(  const std::string &OptionName,
				const neutrino_locale_t Name,
				int * const OptionValue,
				const struct keyval * const Options,
				const struct keyval_ext * const OptionsExt,
				std::vector<keyval_ext> * v_Options,
				const size_t Number_Of_Options,
				CChangeObserver * const Observ,
				bool Pulldown,
				bool OptionsSort)
{
	height                  = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	nameString              = OptionName;
	name                    = Name;
	optionValue             = OptionValue;
	number_of_options 	= Number_Of_Options;
	observ                  = Observ;
	pulldown                = Pulldown;
	optionsSort             = OptionsSort;

	if (Options || OptionsExt)
	{
		if (Options)
		{
			for (unsigned int i = 0; i < number_of_options; i++)
			{
				struct keyval_ext opt;
				opt = Options[i];
				options.push_back(opt);
			}
		}

		if (OptionsExt)
		{
			for (unsigned int i = 0; i < number_of_options; i++)
				options.push_back(OptionsExt[i]);
		}
	}
	else{
		if (v_Options)
			options = *v_Options;
	}
}

void CMenuOptionChooser::clearChooserOptions()
{
	for (std::vector<CMenuOptionChooserOptions*>::iterator it = option_chooser_options_v.begin(); it != option_chooser_options_v.end(); ++it)
		delete *it;

	option_chooser_options_v.clear();
}

void CMenuOptionChooser::setOptions(const struct keyval * const Options, const unsigned Number_Of_Options)
{
	options.clear();
	number_of_options = Number_Of_Options;
	for (unsigned int i = 0; i < number_of_options; i++)
	{
		struct keyval_ext opt;
		opt = Options[i];
		options.push_back(opt);
	}
	if (used && x != -1)
		paint(false);
}

void CMenuOptionChooser::setOptions(const struct keyval_ext * const Options, const unsigned Number_Of_Options)
{
	options.clear();
	number_of_options = Number_Of_Options;
	for (unsigned int i = 0; i < number_of_options; i++)
		options.push_back(Options[i]);
	if (used && x != -1)
		paint(false);
}

void CMenuOptionChooser::setOption(const int newvalue)
{
	*optionValue = newvalue;
}

int CMenuOptionChooser::getOption(void) const
{
	return *optionValue;
}


int CMenuOptionChooser::exec(CMenuTarget*)
{
	bool wantsRepaint = false;
	int ret = menu_return::RETURN_NONE;
	char *optionValname = NULL;

	if (optionsSort) {
		optionsSort = false;
		clearChooserOptions();
		unsigned int i1;
		for (i1 = 0; i1 < number_of_options; i1++)
		{
			CMenuOptionChooserOptions* co = new CMenuOptionChooserOptions();
			co->key     = options[i1].key;
			co->valname = (options[i1].valname != 0) ? options[i1].valname : g_Locale->getText(options[i1].value);
			option_chooser_options_v.push_back(co);
		}

		sort(option_chooser_options_v.begin(), option_chooser_options_v.end(), CMenuOptionChooserCompareItem());

		options.clear();
		for (std::vector<CMenuOptionChooserOptions*>::iterator it = option_chooser_options_v.begin(); it != option_chooser_options_v.end(); ++it) {
			struct keyval_ext opt;
			opt.key     = (*it)->key;
			opt.value   = NONEXISTANT_LOCALE;
			opt.valname = (*it)->valname.c_str();
			options.push_back(opt);
		}
	}

	if((msg == CRCInput::RC_ok) && pulldown) {
		int select = -1;
		CMenuWidget* menu = new CMenuWidget(getName(), NEUTRINO_ICON_SETTINGS);
		menu->enableFade(false);
		/* FIXME: BACK button with hints enabled - parent menu getting holes, possible solution
		 * to hide parent, or add hints ? */
		menu->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);
		// 		menu->move(20, 0);
		CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
		for(unsigned int count = 0; count < number_of_options; count++) 
		{
			bool selected = false;
			const char * l_option;
			if (options[count].key == (*optionValue))
				selected = true;

			if(options[count].valname != 0)
				l_option = options[count].valname;
			else
				l_option = g_Locale->getText(options[count].value);
			CMenuForwarder *mn_option = new CMenuForwarder(l_option, true, NULL, selector, to_string(count).c_str());
			mn_option->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true /*for selected item*/);
			menu->addItem(mn_option, selected);
		}
		menu->exec(NULL, "");
		ret = menu_return::RETURN_REPAINT;;
		if(select >= 0) 
		{
			*optionValue = options[select].key;
			optionValname = (char *) options[select].valname;
		}
		delete menu;
		delete selector;
	} else {
		for(unsigned int count = 0; count < number_of_options; count++) {
			if (options[count].key == (*optionValue)) {
				if(msg == CRCInput::RC_left) {
					if(count > 0)
						optionValname = (char *) options[(count-1) % number_of_options].valname,
							      *optionValue = options[(count-1) % number_of_options].key;
					else
						optionValname = (char *) options[number_of_options-1].valname,
							      *optionValue = options[number_of_options-1].key;
				} else
					optionValname = (char *) options[(count+1) % number_of_options].valname,
						      *optionValue = options[(count+1) % number_of_options].key;
				break;
			}
		}
	}
	paint(true);
	OnAfterChangeOption();
	if(observ && !luaAction.empty()) {
		if (optionValname)
			wantsRepaint = observ->changeNotify(luaState, luaAction, luaId, optionValname);
	} else if(observ)
		wantsRepaint = observ->changeNotify(name, optionValue);

	if (wantsRepaint)
		ret = menu_return::RETURN_REPAINT;

	return ret;
}

int CMenuOptionChooser::paint( bool selected)
{
	neutrino_locale_t option = NONEXISTANT_LOCALE;
	const char * l_option = NULL;

	for(unsigned int count = 0 ; count < number_of_options; count++) {
		if (options[count].key == *optionValue) {
			option = options[count].value;
			if(options[count].valname != 0)
				l_option = options[count].valname;
			else
				l_option = g_Locale->getText(option);
			break;
		}
	}
	
	if(l_option == NULL) 
	{
		*optionValue = options[0].key;
		option = options[0].value;
		if(options[0].valname != 0)
			l_option = options[0].valname;
		else
			l_option = g_Locale->getText(option);
	}

	//paint item
	prepareItem(selected, height);
	
	//paint item icon
	paintItemButton(selected, height, NEUTRINO_ICON_BUTTON_OKAY);
	
	//paint text
	paintItemCaption(selected, l_option);

	return y+height;
}

int CMenuOptionChooser::getWidth(void)
{
	int tw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(getName());
	int width = tw;

	for(unsigned int count = 0; count < options.size(); count++) {
		int ow = 0;
		if (options[count].valname)
			ow = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(options[count].valname);
		else
			ow = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(options[count].value));

		if (tw + ow > width)
			width = tw + ow;
	}

	width += OFFSET_INNER_MID; /* min 10 pixels between option name and value. enough? */
	const char *desc_text = getDescription();
	if (*desc_text)
		width = std::max(width, OFFSET_INNER_MID + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(desc_text));
	return width;
}

//-------------------------------------------------------------------------------------------------------------------------------

CMenuOptionStringChooser::CMenuOptionStringChooser(	const neutrino_locale_t OptionName, std::string* OptionValue, bool Active, CChangeObserver* Observ,
							const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown)
								: CMenuItem(Active, DirectKey, IconName)
{
	init("", OptionName, OptionValue, Observ, Pulldown);
}

CMenuOptionStringChooser::CMenuOptionStringChooser(	const std::string &OptionName, std::string* OptionValue, bool Active, CChangeObserver* Observ,
							const neutrino_msg_t DirectKey, const char * const IconName, bool Pulldown)
								: CMenuItem(Active, DirectKey, IconName)
{
	init(OptionName, NONEXISTANT_LOCALE, OptionValue, Observ, Pulldown);
}

void CMenuOptionStringChooser::init(	const std::string &OptionName,
					const neutrino_locale_t Name,
					std::string* pOptionValue,
					CChangeObserver * const Observ,
					bool Pulldown)
{
	nameString	= OptionName;
	name		= Name;
	optionValuePtr	= pOptionValue ? pOptionValue : &optionValue;
	observ		= Observ;
	pulldown	= Pulldown;
}


CMenuOptionStringChooser::~CMenuOptionStringChooser()
{
	options.clear();
}

void CMenuOptionStringChooser::setTitle(std::string &Title)
{
	title = Title;
}

void CMenuOptionStringChooser::setTitle(const neutrino_locale_t Title)
{
	title = g_Locale->getText(Title);
}

void CMenuOptionStringChooser::addOption(const std::string &value)
{
	options.push_back(value);
}

void CMenuOptionStringChooser::sortOptions()
{
	sort(options.begin(), options.end());
}

int CMenuOptionStringChooser::exec(CMenuTarget* parent)
{
	bool wantsRepaint = false;
	int ret = menu_return::RETURN_NONE;

	if((!parent || msg == CRCInput::RC_ok) && pulldown) {
		int select = -1;

		if (parent)
			parent->hide();

		std::string title_str = title.empty() ? getName() : title;

		CMenuWidget* menu = new CMenuWidget(title_str, NEUTRINO_ICON_SETTINGS);
		menu->addIntroItems(NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, CMenuWidget::BTN_TYPE_CANCEL);
		//if(parent) menu->move(20, 0);
		CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
		for(unsigned int count = 0; count < options.size(); count++) 
		{
			bool selected = optionValuePtr && (options[count] == *optionValuePtr);
			CMenuForwarder *mn_option = new CMenuForwarder(options[count], true, NULL, selector, to_string(count).c_str());
			mn_option->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true /*for selected item*/);
			menu->addItem(mn_option, selected);
		}
		menu->exec(NULL, "");
		ret = menu_return::RETURN_REPAINT;
		if(select >= 0 && optionValuePtr)
			*optionValuePtr = options[select];
		delete menu;
		delete selector;
	} else {
		//select next value
		for(unsigned int count = 0; count < options.size(); count++) {
			if (optionValuePtr && (options[count] == *optionValuePtr)) {
				if(msg == CRCInput::RC_left) {
					if(count > 0)
						*optionValuePtr = options[(count - 1) % options.size()];
					else
						*optionValuePtr = options[options.size() - 1];
				} else
					*optionValuePtr = options[(count + 1) % options.size()];
				//wantsRepaint = true;
				break;
			}
		}

		paint(true);
	}
	if(observ && !luaAction.empty())
		wantsRepaint = observ->changeNotify(luaState, luaAction, luaId, (void *)(optionValuePtr ? optionValuePtr->c_str() : ""));
	else if(observ) {
		wantsRepaint = observ->changeNotify(name, (void *)(optionValuePtr ? optionValuePtr->c_str() : ""));
	}
	if (wantsRepaint)
		ret = menu_return::RETURN_REPAINT;

	return ret;
}

int CMenuOptionStringChooser::paint( bool selected )
{
	//paint item
	prepareItem(selected, height);

	//paint item icon
	paintItemButton(selected, height, NEUTRINO_ICON_BUTTON_OKAY);

	//paint text
	paintItemCaption(selected, optionValuePtr->c_str());

	return y+height;
}

//-------------------------------------------------------------------------------------------------------------------------------
CMenuForwarder::CMenuForwarder(	const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey,
					neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right, bool IsStatic)
					: CMenuItem(Active, DirectKey, IconName, IconName_Info_right, IsStatic)
{
	init("", Text, Option, NULL, Target, ActionKey);
}

CMenuForwarder::CMenuForwarder(	const std::string& Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey,
					neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right, bool IsStatic)
					: CMenuItem(Active, DirectKey, IconName, IconName_Info_right, IsStatic)
{
	init(Text, NONEXISTANT_LOCALE, Option, NULL, Target, ActionKey);
}

CMenuForwarder::CMenuForwarder(	const neutrino_locale_t Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey,
					neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right, bool IsStatic)
					: CMenuItem(Active, DirectKey, IconName, IconName_Info_right, IsStatic)
{
	init("", Text, "", Option, Target, ActionKey);
}

CMenuForwarder::CMenuForwarder(	const std::string& Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey,
					neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right, bool IsStatic)
					: CMenuItem(Active, DirectKey, IconName, IconName_Info_right, IsStatic)
{
	init(Text, NONEXISTANT_LOCALE, "", Option, Target, ActionKey);
}

void CMenuForwarder::init(	const std::string &Text,
				const neutrino_locale_t Name,
				const std::string &sOption,
				const char * const cOption,
				CMenuTarget* Target,
				const char * const ActionKey)
{
	nameString = Text;
	name = Name;

	if (sOption.empty())
	{
		option_string = cOption ? cOption : "";
		option_string_ptr = &option_string;
	}
	else
		option_string_ptr = &sOption;

	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
}

void CMenuForwarder::setOption(const std::string &Option)
{
	option_string = Option;
	option_string_ptr = &option_string;
}

int CMenuForwarder::getWidth(void)
{
	int tw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(getName());

	fb_pixel_t bgcol = 0;
	std::string option_name = getOption();
	if (jumpTarget)
		bgcol = jumpTarget->getColor();

	if (!option_name.empty())
		tw += OFFSET_INNER_MID + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(option_name);
	else if (bgcol)
		tw += OFFSET_INNER_MID + CFrameBuffer::getInstance()->scale2Res(60);

	const char *desc_text = getDescription();
	if (*desc_text)
		tw = std::max(tw, OFFSET_INNER_MID + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(desc_text));
	return tw;
}

int CMenuForwarder::exec(CMenuTarget* parent)
{
	if(jumpTarget)
		return jumpTarget->exec(parent, actionKey);
	return menu_return::RETURN_EXIT;
}

std::string CMenuForwarder::getOption(void)
{
	if (!option_string_ptr->empty())
		return *option_string_ptr;
	if (jumpTarget)
		return jumpTarget->getValue();
	return "";
}

int CMenuForwarder::paint(bool selected)
{
	std::string option_name = getOption();

	fb_pixel_t bgcol = 0;
	if (jumpTarget)
		bgcol = jumpTarget->getColor();
	
	//paint item
	prepareItem(selected, height);
	
	//paint icon
	paintItemButton(selected, height);
	
	//caption
	paintItemCaption(selected, option_name.c_str(), bgcol);

	return y+ height;
}

//-------------------------------------------------------------------------------------------------------------------------------
CMenuSeparator::CMenuSeparator(const int Type, const neutrino_locale_t Text, bool IsStatic) : CMenuItem(false, CRCInput::RC_nokey, NULL, NULL, IsStatic)
{
	init(Type, Text, "");
}

CMenuSeparator::CMenuSeparator(const int Type, const std::string &Text, bool IsStatic) : CMenuItem(false, CRCInput::RC_nokey, NULL, NULL, IsStatic)
{
	init(Type, NONEXISTANT_LOCALE, Text);
}

void CMenuSeparator::init(const int& Type, const neutrino_locale_t& lText, const std::string &sText)
{
	type		= Type;
	name		= lText;
	nameString	= sText;
}

int CMenuSeparator::getHeight(void)
{
	if (nameString.empty() && name == NONEXISTANT_LOCALE)
		return OFFSET_INNER_MID;
	return  g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
}

int CMenuSeparator::getWidth(void)
{
	int w = 0;
	if (type & LINE)
		w = 2*OFFSET_INNER_MID; /* offset left and right */
	const char *l_name = getName();
	if ((type & STRING) && *l_name)
		w += g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_name);
	return w;
}

int CMenuSeparator::paint(bool selected)
{
	height = getHeight();
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	
	if ((type & SUB_HEAD))
	{
		item_color = COL_MENUHEAD_TEXT;
		item_bgcolor = g_settings.theme.menu_Head_gradient ? COL_MENUCONTENT_PLUS_0 : COL_MENUHEAD_PLUS_0;
	}
	else
	{
		item_color = COL_MENUCONTENTINACTIVE_TEXT;
		item_bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	frameBuffer->paintBoxRel(x, y, dx, height, item_bgcolor);
	if ((type & LINE))
	{
		int grad = g_settings.theme.menu_Separator_gradient_enable ? CC_COLGRAD_COL_DARK_LIGHT_DARK : CC_COLGRAD_OFF;
		paintBoxRel(x+OFFSET_INNER_MID, y+(height>>1), dx-2*OFFSET_INNER_MID, 1, COL_MENUCONTENT_PLUS_1, 0, CORNER_NONE, grad, COL_MENUCONTENT_PLUS_0, CFrameBuffer::gradientHorizontal, CColorGradient::light);
	}
	if ((type & STRING))
	{
		const char * l_name = getName();
	
		if (*l_name)
		{
			int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_name);
			int iconwidth, iconheight;
			CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_HOME, &iconwidth, &iconheight);

			/* if no alignment is specified, align centered */
			if (type & ALIGN_LEFT)
				name_start_x = x + (!(type & SUB_HEAD) ? name_start_x : 2*OFFSET_INNER_MID + iconwidth);
			else if (type & ALIGN_RIGHT)
				name_start_x = x + dx - stringwidth - 2*OFFSET_INNER_MID;
			else /* ALIGN_CENTER */
				name_start_x = x + (dx >> 1) - (stringwidth >> 1);
			
			frameBuffer->paintBoxRel(name_start_x-OFFSET_INNER_SMALL, y, stringwidth+2*OFFSET_INNER_SMALL, height, item_bgcolor);
			
			paintItemCaption(selected);
		}
	}
	return y+ height;
}

bool CPINProtection::check()
{
	hint = NONEXISTANT_LOCALE;
	std::string cPIN;
	do
	{
		cPIN = "";
		CPINInput* PINInput = new CPINInput(title, &cPIN, 4, hint);
		PINInput->exec( getParent(), "");
		delete PINInput;
		hint = LOCALE_PINPROTECTION_WRONGCODE;
	} while ((cPIN != *validPIN) && !cPIN.empty());
	return (cPIN == *validPIN);
}


bool CZapProtection::check()
{
	hint = NONEXISTANT_LOCALE;
	int res;
	std::string cPIN;
	char systemstr[128];
	do
	{
		cPIN = "";

		CPLPINInput* PINInput = new CPLPINInput(title, &cPIN, 4, hint, fsk);

		res = PINInput->exec(getParent(), "");
		delete PINInput;
		cPIN[4] = 0;
		strcpy(systemstr, CONFIGDIR "/pinentered.sh ");
		strcat(systemstr, cPIN.c_str());
		system(systemstr);

		hint = LOCALE_PINPROTECTION_WRONGCODE;
	} while ( (cPIN != *validPIN) && !cPIN.empty() &&
		  ( res == menu_return::RETURN_REPAINT ) &&
		  ( fsk >= g_settings.parentallock_lockage ) );
	return ( (cPIN == *validPIN) ||
			 ( fsk < g_settings.parentallock_lockage ) );
}

int CLockedMenuForwarder::exec(CMenuTarget* parent)
{
	Parent = parent;

	if (Ask && !check())
	{
		Parent = NULL;
		return menu_return::RETURN_REPAINT;
	}

	Parent = NULL;
	return CMenuForwarder::exec(parent);
}

int CMenuSelectorTarget::exec(CMenuTarget* /*parent*/, const std::string & actionKey)
{
	if (!actionKey.empty())
		*m_select = atoi(actionKey);
	else
		*m_select = -1;
	return menu_return::RETURN_EXIT;
}

CMenuProgressbar::CMenuProgressbar(const neutrino_locale_t Text) : CMenuItem(true, CRCInput::RC_nokey, NULL, NULL, false)
{
	init(Text, "");
}

CMenuProgressbar::CMenuProgressbar(const std::string &Text) : CMenuItem(true, CRCInput::RC_nokey, NULL, NULL, false)
{
	init(NONEXISTANT_LOCALE, Text);
}

void CMenuProgressbar::init(const neutrino_locale_t Loc, const std::string &Text)
{
	name = Loc;
	nameString = Text;
	scale.setDimensionsAll(0, 0, 100, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()/2);
	scale.setValues(100, 100);
}

int CMenuProgressbar::paint(bool selected)
{
	//paint item
	prepareItem(selected, height);

	//left text
	const char *left_text = getName();
	int _dx = dx;

	if (*left_text)
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(name_start_x, y + height, _dx- (name_start_x - x), left_text, item_color);

	//progress bar
	int pb_x;
	if (*left_text)
		pb_x = std::max(name_start_x + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(left_text) + icon_frame_w, x + dx - scale.getWidth() - icon_frame_w);
	else
		pb_x = name_start_x;

	scale.setPos(pb_x, y + (height - scale.getHeight())/2);
	scale.reset();
	scale.paint();

	return y + height;
}

int CMenuProgressbar::getHeight(void)
{
	return std::max(CMenuItem::getHeight(), scale.getHeight());
}

int CMenuProgressbar::getWidth(void)
{
	int width = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(getName());
	if (width)
		width += OFFSET_INNER_MID;
	return width + scale.getWidth();
}

int CMenuProgressbar::exec(CMenuTarget*)
{
	int val = scale.getValue() + 25;
	if (val > 100)
		val = 0;
	scale.setValue(val);
	scale.reset();
	scale.paint();
	return menu_return::RETURN_NONE;
}
