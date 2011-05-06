/*
	$port: menue.cpp,v 1.178 2010/12/08 19:49:30 tuxbox-cvs Exp $

	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	(C) 2008, 2009 Stefan Seyfried

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/menue.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>

#include <gui/widget/stringinput.h>

#include <global.h>
#include <neutrino.h>


#include <cctype>

/* the following generic menu items are integrated into multiple menus at the same time */
CMenuSeparator CGenericMenuSeparator;
CMenuSeparator CGenericMenuSeparatorLine(CMenuSeparator::LINE);
CMenuForwarder CGenericMenuBack(LOCALE_MENU_BACK, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_LEFT);
CMenuForwarder CGenericMenuCancel(LOCALE_MENU_CANCEL, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_HOME);
CMenuForwarder CGenericMenuNext(LOCALE_MENU_NEXT, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_HOME);
CMenuSeparator * const GenericMenuSeparator = &CGenericMenuSeparator;
CMenuSeparator * const GenericMenuSeparatorLine = &CGenericMenuSeparatorLine;
CMenuForwarder * const GenericMenuBack = &CGenericMenuBack;
CMenuForwarder * const GenericMenuCancel = &CGenericMenuCancel;
CMenuForwarder * const GenericMenuNext = &CGenericMenuNext;


CMenuItem::CMenuItem()
{
	x = -1;
	directKey = CRCInput::RC_nokey;
	iconName = "";
	iconName_Info_right = "";
	can_arrow = false;
	used = false;
	icon_frame_w = 10;
}

void CMenuItem::init(const int X, const int Y, const int DX, const int OFFX)
{
	x    = X;
	y    = Y;
	dx   = DX;
	offx = OFFX;
	name_start_x = x + offx + icon_frame_w;
	item_color   = COL_MENUCONTENT;
	item_bgcolor = COL_MENUCONTENT_PLUS_0;
}

void CMenuItem::setActive(const bool Active)
{
	active = Active;
	/* used gets set by the addItem() function. This is for disabling
	   machine-specific options by just not calling the addItem() function.
	   Without this, the changeNotifiers would become machine-dependent. */
	if (used && x != -1)
		paint();
}

void CMenuItem::setItemButton(const std::string& icon_Name, const bool is_select_button)
{
	if (is_select_button)
		selected_iconName = icon_Name;
	else
		iconName = icon_Name;
}

void CMenuItem::initItemColors(const bool select_mode)
{
	if (select_mode)
	{
		item_color   = COL_MENUCONTENTSELECTED;
		item_bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (!active)
	{
		item_color   = COL_MENUCONTENTINACTIVE;
		item_bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}
	else
	{
		item_color   = COL_MENUCONTENT;
		item_bgcolor = COL_MENUCONTENT_PLUS_0;
	}
}


void CMenuItem::paintItemBackground (const bool select_mode, const int &item_height)
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	
	if(select_mode)
		frameBuffer->paintBoxRel(x, y, dx, item_height, item_bgcolor, RADIUS_LARGE);
	//else if(last) ?? Why do we need this?
		//frameBuffer->paintBoxRel(x, y, dx, item_height, i_bgcolor, RADIUS_LARGE, CORNER_BOTTOM); //FIXME 
	else
		frameBuffer->paintBoxRel(x, y, dx, item_height, item_bgcolor, RADIUS_LARGE);
}

void CMenuItem::paintItemCaption(const bool select_mode, const int &item_height, const char * left_text, const char * right_text)
{
	if (select_mode)
	{
		char str[256];

		if (right_text != NULL) 
		{
			snprintf(str, 255, "%s %s", left_text, right_text);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
		} 
		else
			CVFD::getInstance()->showMenuText(0, left_text, -1, true);
	}
	
	//left text
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(name_start_x, y+ item_height, dx- (name_start_x - x), left_text, item_color, 0, true); // UTF-8

	//right text
	if (right_text != NULL)
	{
		int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(right_text, true);
		int stringstartposOption = std::max(name_start_x + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(left_text, true) + icon_frame_w, x + dx - stringwidth - icon_frame_w); //+ offx
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y+item_height,dx- (stringstartposOption- x),  right_text, item_color, 0, true);
	}
}

void CMenuItem::prepareItem(const bool select_mode, const int &item_height)
{
 	//set colors
 	initItemColors(select_mode);

	//paint item background
	paintItemBackground(select_mode, item_height);
}


void CMenuItem::paintItemSlider( const bool select_mode, const int &item_height, const int &optionvalue, const int &factor, const char * left_text, const char * right_text)
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	int slider_lenght = 0, h = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_VOLUMEBODY, &slider_lenght, &h);
	if(slider_lenght == 0 || factor < optionvalue )
		return;
	int stringwidth = 0;
	if (right_text != NULL) {
		stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("U999", true) ;
	}
	int stringwidth2 = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(left_text, true);

	int maxspace = dx - stringwidth - icon_frame_w - stringwidth2 - 10;
	if(maxspace < slider_lenght){
		return ;
	}

	int stringstartposOption = x + dx - stringwidth - slider_lenght;
	int optionV = (optionvalue < 0) ? 0 : optionvalue;
	frameBuffer->paintBoxRel(stringstartposOption, y, slider_lenght, item_height, item_bgcolor);
	frameBuffer->paintIcon(NEUTRINO_ICON_VOLUMEBODY, stringstartposOption, y+2+item_height/4);
	frameBuffer->paintIcon(select_mode ? NEUTRINO_ICON_VOLUMESLIDER2BLUE : NEUTRINO_ICON_VOLUMESLIDER2, (stringstartposOption + (optionV * 100 / factor)), y+item_height/4);
}

void CMenuItem::paintItemButton(const bool select_mode, const int &item_height, const std::string &icon_Name)
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	bool selected = select_mode;
	bool icon_painted = false;
	
	int w = 0;
	int h = 0;

	std::string icon_name = iconName;
	int icon_w = 0;
	int icon_h = 0;

	//define icon name depends of numeric value
	if (icon_name.empty() && CRCInput::isNumeric(directKey))
	{
		char i_name[6]; /* X +'\0' */
		snprintf(i_name, 6, "%d", CRCInput::getNumericValue(directKey));
		i_name[5] = '\0'; /* even if snprintf truncated the string, ensure termination */
		icon_name = i_name;
	}

	//define select icon
	if  (selected && offx > 0)
	{
		if (!selected_iconName.empty())
			icon_name = selected_iconName;
		else if (icon_name.empty() && !CRCInput::isNumeric(directKey))
			icon_name = icon_Name;
	}
	
	int icon_x = 0;
	int icon_start_x = x+icon_frame_w; //start of icon space
	int icon_space_x = name_start_x - icon_frame_w - icon_start_x; //size of space where to paint icon
	int icon_space_mid = icon_start_x + icon_space_x/2;

	//get data of number icon and paint
	if (!icon_name.empty())
	{
		frameBuffer->getIconSize(icon_name.c_str(), &w, &h);
		icon_w = w;
		icon_h = h;
		
		if (active  && icon_w>0 && icon_h>0)
		{
			icon_x = icon_space_mid - (icon_w/2); 

			icon_painted = frameBuffer->paintIcon(icon_name, icon_x, y+ ((item_height/2- icon_h/2)) );
		}
	}

	//paint only number if no icon was painted and keyval is numeric
	if (CRCInput::isNumeric(directKey) && !icon_painted)
	{			
		int number_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(CRCInput::getKeyName(directKey));
		
		int number_x = icon_space_mid - (number_w/2);
		
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(number_x, y+ item_height, item_height, CRCInput::getKeyName(directKey), item_color, item_height);
	}

	//get data of number right info icon and paint
	if (!iconName_Info_right.empty())
	{
		frameBuffer->getIconSize(iconName_Info_right.c_str(), &w, &h);
		icon_w = w;
		icon_h = h;

		if (active  && icon_w>0 && icon_h>0)
		{
			icon_painted = frameBuffer->paintIcon(iconName_Info_right, dx + icon_start_x - (icon_w + 20), y+ ((item_height/2- icon_h/2)) );
		}
	}
}

CMenuWidget::CMenuWidget()
{
        nameString 	= g_Locale->getText(NONEXISTANT_LOCALE);
	name 		= NONEXISTANT_LOCALE;
        iconfile 	= "";
        selected 	= -1;
        iconOffset 	= 0;
	offx = offy 	= 0;
	from_wizard 	= false;
	fade 		= true;
}

CMenuWidget::CMenuWidget(const neutrino_locale_t Name, const std::string & Icon, const int mwidth, const int mheight)
{
	name = Name;
        nameString = g_Locale->getText(Name);

	Init(Icon, mwidth, mheight);
}

CMenuWidget::CMenuWidget(const char* Name, const std::string & Icon, const int mwidth, const int mheight)
{
	name = NONEXISTANT_LOCALE;
        nameString = Name;

	Init(Icon, mwidth, mheight);
}

void CMenuWidget::Init(const std::string & Icon, const int mwidth, const int /*mheight*/)
{
        frameBuffer = CFrameBuffer::getInstance();
        iconfile = Icon;
        preselected = -1;
	selected = preselected;
	min_width = 0;
	width = 0; /* is set in paint() */

	if (mwidth > 100) 
	{
		/* warn about abuse until we found all offenders... */
		fprintf(stderr, "Warning: %s (%s) (%s) mwidth over 100%%: %d\n", __FUNCTION__, nameString.c_str(), Icon.c_str(), mwidth);
		
	}
	else 
	{
		min_width = frameBuffer->getScreenWidth(true) * mwidth / 100;
		if(min_width > (int) frameBuffer->getScreenWidth())
			min_width = frameBuffer->getScreenWidth();
	}

	/* set the max height to 9/10 of usable screen height
	   debatable, if the callers need a possibility to set this */
	height 		= frameBuffer->getScreenHeight() / 20 * 18; /* make sure its a multiple of 2 */
	wanted_height 	= height;
        current_page	= 0;
	offx = offy 	= 0;
	from_wizard 	= false;
	fade 		= true;
}

void CMenuWidget::move(int xoff, int yoff)
{
	offx = xoff;
	offy = yoff;
}

CMenuWidget::~CMenuWidget()
{
	for(unsigned int count=0;count<items.size();count++) {
		CMenuItem * item = items[count];
		if ((item != GenericMenuSeparator) &&
		    (item != GenericMenuSeparatorLine) &&
		    (item != GenericMenuBack) &&
		    (item != GenericMenuCancel))
			delete item;
	}
	items.clear();
	page_start.clear();
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
}

void CMenuWidget::resetWidget()
{
	items.clear();
	page_start.clear();
	selected=-1;
}

bool CMenuWidget::hasItem()
{
	return !items.empty();
}

std::string CMenuWidget::getName()
{
	if (name != NONEXISTANT_LOCALE)
		nameString = g_Locale->getText(name);
	return nameString;
}

int CMenuWidget::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool bAllowRepeatLR = false;

	int pos = 0;
	exit_pressed = false;

	frameBuffer->Lock();

	if (parent)
		parent->hide();

	/* set the max height to 9/10 of usable screen height
	   debatable, if the callers need a possibility to set this */
	//FIXME width ??
	height = frameBuffer->getScreenHeight() / 20 * 18; /* make sure its a multiple of 2 */
	wanted_height = height;

	bool fadeIn = g_settings.widget_fade && fade;
	bool fadeOut = false;
	uint32_t fadeTimer = 0;
	int fadeValue = g_settings.menu_Content_alpha;
        if ( fadeIn ) {
		fadeValue = 100;
		frameBuffer->setBlendMode(2); // Global alpha multiplied with pixel alpha
		frameBuffer->setBlendLevel(fadeValue, fadeValue);
		fadeTimer = g_RCInput->addTimer( FADE_TIME, false );
        }

	if(from_wizard) {
		for (unsigned int count = 0; count < items.size(); count++) {
			if(items[count] == GenericMenuBack) {
				items[count] = GenericMenuNext;
				break;
			}
		}
	}
	paint();
	int retval = menu_return::RETURN_REPAINT;
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);


	do {
		if(hasItem() && selected >= 0)
			bAllowRepeatLR = items[selected]->can_arrow;

		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, bAllowRepeatLR);

		if ( msg <= CRCInput::RC_MaxRC ) {
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
::TIMING_MENU]);
		}
		int handled= false;

		for (unsigned int i= 0; i< items.size(); i++) {
			CMenuItem* titem = items[i];
			if ((titem->directKey != CRCInput::RC_nokey) && (titem->directKey == msg)) {
				if (titem->isSelectable()) {
					items[selected]->paint( false, (i == items.size()-1) );
					selected= i;
					msg= CRCInput::RC_ok;
				} else {
					// swallow-key...
					handled= true;
				}
				break;
			}
		}

		if (!handled) {
			switch (msg) {
				case (NeutrinoMessages::EVT_TIMER):
					if(data == fadeTimer) {
						if (fadeOut) { // disappear
							fadeValue += FADE_STEP;
							if (fadeValue >= 100) {
								fadeValue = g_settings.menu_Content_alpha;
								g_RCInput->killTimer (fadeTimer);
								fadeTimer = 0;
								msg = CRCInput::RC_timeout;
							} else
								frameBuffer->setBlendLevel(fadeValue, fadeValue);
						} else { // appears
							fadeValue -= FADE_STEP;
							if (fadeValue <= g_settings.menu_Content_alpha) {
								fadeValue = g_settings.menu_Content_alpha;
								g_RCInput->killTimer (fadeTimer);
								fadeTimer = 0;
								fadeIn = false;
								//frameBuffer->setBlendLevel(FADE_RESET, g_settings.gtx_alpha2);
								frameBuffer->setBlendMode(1); // Set back to per pixel alpha
							} else
								frameBuffer->setBlendLevel(fadeValue, fadeValue);
						}


					} else {
						if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
							retval = menu_return::RETURN_EXIT_ALL;
							msg = CRCInput::RC_timeout;
						}
					}
					break;
				case (CRCInput::RC_page_up) :
				case (CRCInput::RC_page_down) :
					if(msg==CRCInput::RC_page_up) {
						if(current_page) {
							pos = (int) page_start[current_page] - 1;
							for (unsigned int count=pos ; count > 0; count--) {
								CMenuItem* item = items[pos];
								if ( item->isSelectable() ) {
									if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page])) {
										items[selected]->paint( false, (pos == (int) items.size()-1) );
										item->paint( true , (count == items.size()-1));
										selected = pos;
									} else {
										selected=pos;
										paintItems();
									}
									break;
								}
								pos--;
							}
						} else {
							pos = 0;
							for (unsigned int count=0; count < items.size(); count++) {
								CMenuItem* item = items[pos];
								if ( item->isSelectable() ) {
									if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page])) {
										items[selected]->paint( false, (count == items.size()-1) );
										item->paint( true, (count == items.size()-1) );
										selected = pos;
									} else {
										selected=pos;
										paintItems();
									}
									break;
								}
								pos++;
							}
						}
					}
					else if(msg==CRCInput::RC_page_down) {
						pos = (int) page_start[current_page + 1];// - 1;
						if(pos >= (int) items.size())
							pos = items.size()-1;
						for (unsigned int count=pos ; count < items.size(); count++) {
							CMenuItem* item = items[pos];
							if ( item->isSelectable() ) {
								if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page])) {
									items[selected]->paint( false );
									item->paint( true );
									selected = pos;
								} else {
									selected=pos;
									paintItems();
								}
								break;
							}
							pos++;
						}
					}
					break;
				case (CRCInput::RC_up) :
				case (CRCInput::RC_down) :
					{
						//search next / prev selectable item
						for (unsigned int count=1; count< items.size(); count++) {
							if (msg==CRCInput::RC_up) {
								pos = selected - count;
								if ( pos < 0 )
									pos += items.size();
							}
							else if(msg==CRCInput::RC_down) {
								pos = (selected+ count)%items.size();
							}

							CMenuItem* item = items[pos];

							if ( item->isSelectable() ) {
								if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page]))
								{ // Item is currently on screen
									//clear prev. selected
									items[selected]->paint( false, (selected == (int) (page_start[current_page + 1]-1)) );
									//select new
									item->paint( true );
									selected = pos;
								} else {
									selected=pos;
									paintItems();
								}
								break;
							}
						}
					}
					break;
				case (CRCInput::RC_left):
					{
						CMenuItem* itemX = items[selected];
						int menu_left_exit = (itemX->isMenueOptionChooser() == 1) ? 0 : g_settings.menu_left_exit;
						if ((hasItem() && (selected < 0 || !items[selected]->can_arrow)) ||
							menu_left_exit) {
							msg = CRCInput::RC_timeout;
							break;
						}
					}
				case (CRCInput::RC_right):
				case (CRCInput::RC_ok):
					{
						if(hasItem() && selected > -1) {
							//exec this item...
							CMenuItem* item = items[selected];
							item->msg = msg;
							if ( fadeIn ) {
								g_RCInput->killTimer(fadeTimer);
								fadeTimer = 0;
								fadeIn = false;
								//frameBuffer->setBlendLevel(FADE_RESET, g_settings.gtx_alpha2);
								frameBuffer->setBlendMode(1); // Set back to per pixel alpha
							}
							int rv = item->exec( this );
							switch ( rv ) {
								case menu_return::RETURN_EXIT_ALL:
									retval = menu_return::RETURN_EXIT_ALL;
								case menu_return::RETURN_EXIT:
									msg = CRCInput::RC_timeout;
									break;
								case menu_return::RETURN_REPAINT:
								case menu_return::RETURN_EXIT_REPAINT:
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

				case (CRCInput::RC_sat):
				case (CRCInput::RC_favorites):
					g_RCInput->postMsg (msg, 0);
				//close any menue on dbox-key
				case (CRCInput::RC_setup):
					{
						msg = CRCInput::RC_timeout;
						retval = menu_return::RETURN_EXIT_ALL;
					}
					break;

				default:
					if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
						retval = menu_return::RETURN_EXIT_ALL;
						msg = CRCInput::RC_timeout;
					}
			}
			if(msg == CRCInput::RC_timeout) {
				if ( fadeIn ) {
					g_RCInput->killTimer(fadeTimer);
					fadeTimer = 0;
					fadeIn = false;
				}
				if ((!fadeOut) && g_settings.widget_fade && fade) {
					fadeOut = true;
					fadeTimer = g_RCInput->addTimer( FADE_TIME, false );
					timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
					frameBuffer->setBlendMode(2); // Global alpha multiplied with pixel alpha
					msg = 0;
					continue;
				}
			}

			if ( msg <= CRCInput::RC_MaxRC )
			{
				// recalculate timeout for RC-keys
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
			}
		}
	}
	while ( msg!=CRCInput::RC_timeout );
	hide();

	if ( fadeIn || fadeOut ) {
		g_RCInput->killTimer(fadeTimer);
		fadeTimer = 0;
		//frameBuffer->setBlendLevel(FADE_RESET, g_settings.gtx_alpha2);
		frameBuffer->setBlendMode(1); // Set back to per pixel alpha
	}

	if(!parent)
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);

	for (unsigned int count = 0; count < items.size(); count++) 
	{
		if(items[count] == GenericMenuNext) 
			items[count] = GenericMenuBack;
		else if (items[count] == GenericMenuCancel)
			items[count] = GenericMenuBack;
			
		CMenuItem* item = items[count];
		item->init(-1, 0, 0, 0);
	}

	frameBuffer->Unlock();
	return retval;
}

void CMenuWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width+15+SHADOW_OFFSET,height+10+SHADOW_OFFSET);
}

void CMenuWidget::paint()
{
	if (name != NONEXISTANT_LOCALE)
		nameString = g_Locale->getText(name);
	
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8, nameString.c_str());

	height = wanted_height;
	width = min_width;

	for (unsigned int i= 0; i< items.size(); i++) {
		int tmpw = items[i]->getWidth() + 10 + 10; /* 10 pixels to the left and right of the text */
		if (tmpw > width)
			width = tmpw;
	}

	if(height > ((int)frameBuffer->getScreenHeight() - 10))
		height = frameBuffer->getScreenHeight() - 10;

	int neededWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(nameString.c_str(), true); // UTF-8
	if (neededWidth > width-48) {
		width= neededWidth+ 49;
	}
	int hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	int fw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getWidth();
	int itemHeightTotal=0;
	int heightCurrPage=0;
	page_start.clear();
	page_start.push_back(0);
	total_pages=1;
	for (unsigned int i= 0; i< items.size(); i++) {
		int item_height=items[i]->getHeight();
		itemHeightTotal+=item_height;
		heightCurrPage+=item_height;
		if(heightCurrPage > (height-hheight)) {
			page_start.push_back(i);
			total_pages++;
			heightCurrPage=item_height;
		}
	}
	page_start.push_back(items.size());

	iconOffset= 0;
	for (unsigned int i= 0; i< items.size(); i++) {
		if ((!(items[i]->iconName.empty())) || CRCInput::isNumeric(items[i]->directKey))
		{
			const char *tmp;
			if (items[i]->iconName.empty())
				tmp = CRCInput::getKeyName(items[i]->directKey).c_str();
			else
				tmp = items[i]->iconName.c_str();
			int w, h;
			frameBuffer->getIconSize(tmp, &w, &h);
			if (w > iconOffset)
				iconOffset = w;
		}
	}

	iconOffset += 10;
	width += iconOffset;
	if (width > (int)frameBuffer->getScreenWidth())
		width = frameBuffer->getScreenWidth();

	// shrink menu if less items
	if(hheight+itemHeightTotal < height)
		height=hheight+itemHeightTotal;

	x = offx + frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - width ) >> 1 );
	y = offy + frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - height) >> 1 );

	int sb_width;
	if(total_pages > 1)
		sb_width=15;
	else
		sb_width=0;

	switch(g_settings.menu_pos) {
		case 0: //DEFAULT_CENTER
			break;
		case 1: //TOP_LEFT
			y = offy + frameBuffer->getScreenY() + 10;
			x = offx + frameBuffer->getScreenX() + 10;
			break;
		case 2: //TOP_RIGHT
			y = offy + frameBuffer->getScreenY() + 10;
			x = offx + frameBuffer->getScreenX() + frameBuffer->getScreenWidth() - width - sb_width - 10;
			break;
		case 3: //BOTTOM_LEFT
			y = offy + frameBuffer->getScreenY() + frameBuffer->getScreenHeight() - height - 10;
			x = offx + frameBuffer->getScreenX() + 10;
			break;
		case 4: //BOTTOM_RIGHT
			y = offy + frameBuffer->getScreenY() + frameBuffer->getScreenHeight() - height - 10;
			x = offx + frameBuffer->getScreenX() + frameBuffer->getScreenWidth() - width - sb_width - 10;
			break;
	}

	//paint shadow and backround
	int rad = RADIUS_LARGE-2;
	frameBuffer->paintBoxRel(x+SHADOW_OFFSET ,y + SHADOW_OFFSET ,width + sb_width ,height + rad ,COL_MENUCONTENTDARK_PLUS_0 ,rad);
	frameBuffer->paintBoxRel(x ,y ,width + sb_width ,height + rad ,COL_MENUCONTENT_PLUS_0 ,rad);
	frameBuffer->paintBoxRel(x ,y ,width + sb_width ,hheight ,COL_MENUHEAD_PLUS_0 ,rad, CORNER_TOP);

	//paint menu head
	int HeadiconOffset = 0;
	if(!(iconfile.empty())){
		int w, h;
		frameBuffer->getIconSize(iconfile.c_str(), &w, &h);
		HeadiconOffset = w+6;
	}
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+(fw/3)+HeadiconOffset,y+hheight+1, width-((fw/3)+HeadiconOffset), nameString.c_str(), COL_MENUHEAD, 0, true); // UTF-8
	frameBuffer->paintIcon(iconfile, x + fw/4, y, hheight);

	item_start_y = y+hheight;
	paintItems();
}

void CMenuWidget::paintItems()
{
	int item_height=height-(item_start_y-y);

	//Item not currently on screen
	if (selected >= 0)
	{
		while(selected < (int)page_start[current_page])
			current_page--;
		while(selected >= (int)page_start[current_page + 1])
			current_page++;
	}

	// Scrollbar
	if(total_pages>1)
	{
		frameBuffer->paintBoxRel(x+ width,item_start_y, 15, item_height, COL_MENUCONTENT_PLUS_1, RADIUS_MIN);
		frameBuffer->paintBoxRel(x+ width +2, item_start_y+ 2+ current_page*(item_height-4)/total_pages, 11, (item_height-4)/total_pages, COL_MENUCONTENT_PLUS_3, RADIUS_MIN);
	}
	frameBuffer->paintBoxRel(x,item_start_y, width,item_height, COL_MENUCONTENT_PLUS_0);
	int ypos=item_start_y;
	for (unsigned int count = 0; count < items.size(); count++)
	{
		CMenuItem* item = items[count];

		if ((count >= page_start[current_page]) &&
		    (count < page_start[current_page + 1]))
		{
			item->init(x, ypos, width, iconOffset);
			if( (item->isSelectable()) && (selected==-1) )
			{		
				ypos = item->paint(true);
				selected = count;
			}
			else
			{
				bool sel = selected==((signed int) count) ;
				ypos = item->paint(sel);
			}
		}
		else
		{
			/* x = -1 is a marker which prevents the item from being painted on setActive changes */
			item->init(-1, 0, 0, 0);
		}
	}
}

/*adds the typical menu intro with optional subhead, separator, back or cancel button and separatorline to menu*/
void CMenuWidget::addIntroItems(neutrino_locale_t subhead_text, neutrino_locale_t section_text, int buttontype)
{
	if (subhead_text != NONEXISTANT_LOCALE)
		addItem(new CMenuSeparator(CMenuSeparator::ALIGN_LEFT | CMenuSeparator::SUB_HEAD | CMenuSeparator::STRING, subhead_text));
	addItem(GenericMenuSeparator);
	
	if (buttontype != BTN_TYPE_NO)
	{
		switch (buttontype)
		{
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
	}
	
	if (section_text != NONEXISTANT_LOCALE)
		addItem(new CMenuSeparator(CMenuSeparator::LINE | CMenuSeparator::STRING, section_text));
	else
		addItem(GenericMenuSeparatorLine);
}

//-------------------------------------------------------------------------------------------------------------------------------
CMenuOptionNumberChooser::CMenuOptionNumberChooser(const neutrino_locale_t name, int * const OptionValue, const bool Active, const int min_value, const int max_value, CChangeObserver * const Observ, const int print_offset, const int special_value, const neutrino_locale_t special_value_name, const char * non_localized_name, bool sliderOn)
{
	height               = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionName           = name;
	active               = Active;
	optionValue          = OptionValue;

	lower_bound          = min_value;
	upper_bound          = max_value;

	display_offset       = print_offset;

	localized_value      = special_value;
	localized_value_name = special_value_name;
	
	optionString         = non_localized_name;
	can_arrow	= true;
	observ = Observ;
	slider_on = sliderOn;
}

int CMenuOptionNumberChooser::exec(CMenuTarget*)
{
	if(msg == CRCInput::RC_left) {
		if (((*optionValue) > upper_bound) || ((*optionValue) <= lower_bound))
			*optionValue = upper_bound;
		else
			(*optionValue)--;
	} else {
		if (((*optionValue) >= upper_bound) || ((*optionValue) < lower_bound))
			*optionValue = lower_bound;
		else
			(*optionValue)++;
	}
	paint(true);
	if(observ)
		observ->changeNotify(optionName, optionValue);

	return menu_return::RETURN_NONE;
}

int CMenuOptionNumberChooser::paint(bool selected, bool /*last*/)
{
	const char * l_option;
	char option_value[11];

	if ((localized_value_name == NONEXISTANT_LOCALE) || ((*optionValue) != localized_value))
	{
		sprintf(option_value, "%d", ((*optionValue) + display_offset));
		l_option = option_value;
	}
	else
		l_option = g_Locale->getText(localized_value_name);
	
	const char * l_optionName = (optionString != NULL) ? optionString : g_Locale->getText(optionName);

	//paint item
	prepareItem(selected, height);

	//paint item icon
	paintItemButton(selected, height, NEUTRINO_ICON_BUTTON_OKAY);
	if(slider_on)
		paintItemSlider( selected, height, *optionValue, (upper_bound - lower_bound) , l_optionName, l_option);
	//paint text
	paintItemCaption(selected, height , l_optionName, l_option);

	return y+height;
}

CMenuOptionChooser::CMenuOptionChooser(const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval * const Options, const unsigned Number_Of_Options, const bool Active, CChangeObserver * const Observ, const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	height            = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionNameString  = g_Locale->getText(OptionName);
	optionName        = OptionName;
	active            = Active;
	optionValue       = OptionValue;
	number_of_options = Number_Of_Options;
	observ            = Observ;
	directKey         = DirectKey;
	iconName          = IconName;
	can_arrow	= true;
	pulldown = Pulldown;
	for (unsigned int i = 0; i < number_of_options; i++)
	{
		struct keyval_ext opt;
		opt.key = Options[i].key;
		opt.value = Options[i].value;
		opt.valname = NULL;
		options.push_back(opt);
	}
}

CMenuOptionChooser::CMenuOptionChooser(const char* OptionName, int * const OptionValue, const struct keyval * const Options, const unsigned Number_Of_Options, const bool Active, CChangeObserver * const Observ, const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	height            = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionNameString  = OptionName;
	optionName        = NONEXISTANT_LOCALE;
	active            = Active;
	optionValue       = OptionValue;
	number_of_options = Number_Of_Options;
	observ            = Observ;
	directKey         = DirectKey;
	iconName          = IconName;
	can_arrow	= true;
	pulldown = Pulldown;
	for (unsigned int i = 0; i < number_of_options; i++)
	{
		struct keyval_ext opt;
		opt.key = Options[i].key;
		opt.value = Options[i].value;
		opt.valname = NULL;
		options.push_back(opt);
	}
}

CMenuOptionChooser::CMenuOptionChooser(const neutrino_locale_t OptionName, int * const OptionValue, const struct keyval_ext * const Options,
				       const unsigned Number_Of_Options, const bool Active, CChangeObserver * const Observ,
				       const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	height            = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionNameString  = g_Locale->getText(OptionName);
	optionName        = OptionName;
	active            = Active;
	optionValue       = OptionValue;
	number_of_options = Number_Of_Options;
	observ            = Observ;
	directKey         = DirectKey;
	iconName          = IconName;
	can_arrow	= true;
	pulldown = Pulldown;
	for (unsigned int i = 0; i < number_of_options; i++)
		options.push_back(Options[i]);
}

CMenuOptionChooser::CMenuOptionChooser(const char* OptionName, int * const OptionValue, const struct keyval_ext * const Options,
				       const unsigned Number_Of_Options, const bool Active, CChangeObserver * const Observ,
				       const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	height            = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionNameString  = OptionName;
	optionName        = NONEXISTANT_LOCALE;
	active            = Active;
	optionValue       = OptionValue;
	number_of_options = Number_Of_Options;
	observ            = Observ;
	directKey         = DirectKey;
	iconName          = IconName;
	can_arrow	= true;
	pulldown = Pulldown;
	for (unsigned int i = 0; i < number_of_options; i++)
		options.push_back(Options[i]);
}

CMenuOptionChooser::~CMenuOptionChooser()
{
	options.clear();
}

void CMenuOptionChooser::setOptionValue(const int newvalue)
{
	*optionValue = newvalue;
}

int CMenuOptionChooser::getOptionValue(void) const
{
	return *optionValue;
}


int CMenuOptionChooser::exec(CMenuTarget*)
{
	bool wantsRepaint = false;
	int ret = menu_return::RETURN_NONE;

	if((msg == CRCInput::RC_ok) && pulldown) {
		int select = -1;
		char cnt[5];
		CMenuWidget* menu = new CMenuWidget(optionNameString.c_str(), NEUTRINO_ICON_SETTINGS);
		menu->addIntroItems();
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
			sprintf(cnt, "%d", count);
			CMenuForwarderNonLocalized *mn_option = new CMenuForwarderNonLocalized(l_option, true, NULL, selector, cnt);
			mn_option->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true /*for selected item*/);
			menu->addItem(mn_option, selected);
		}
		menu->exec(NULL, "");
		ret = menu_return::RETURN_REPAINT;;
		if(select >= 0) 
		{
			*optionValue = options[select].key;
		}
		delete menu;
		delete selector;
	} else {
		for(unsigned int count = 0; count < number_of_options; count++) {
			if (options[count].key == (*optionValue)) {
				if(msg == CRCInput::RC_left) {
					if(count > 0)
						*optionValue = options[(count-1) % number_of_options].key;
					else
						*optionValue = options[number_of_options-1].key;
				} else
					*optionValue = options[(count+1) % number_of_options].key;
				break;
			}
		}
	}
	paint(true);
	if(observ)
		wantsRepaint = observ->changeNotify(optionName, optionValue);

	if ( wantsRepaint )
		ret = menu_return::RETURN_REPAINT;

	return ret;
}

int CMenuOptionChooser::paint( bool selected , bool /*last*/)
{
	if(optionName != NONEXISTANT_LOCALE)
		optionNameString  = g_Locale->getText(optionName);

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
	paintItemCaption(selected, height , optionNameString.c_str(), l_option);

	return y+height;
}

int CMenuOptionChooser::getWidth(void)
{
	int ow = 0;
	int tw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(optionNameString, true);
	int width = tw;

	for(unsigned int count = 0; count < options.size(); count++) {
		ow = 0;
		if (options[count].valname)
			ow = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]
				->getRenderWidth(options[count].valname, true);
		else
			ow = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]
				->getRenderWidth(g_Locale->getText(options[count].value), true);

		if (tw + ow > width)
			width = tw + ow;
	}

	return width + 10; /* min 10 pixels between option name and value. enough? */
}

//-------------------------------------------------------------------------------------------------------------------------------

CMenuOptionStringChooser::CMenuOptionStringChooser(const neutrino_locale_t OptionName, char* OptionValue, bool Active, CChangeObserver* Observ, const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	height      = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionName  = OptionName;
	active      = Active;
	optionValue = OptionValue;
	observ      = Observ;

	directKey         = DirectKey;
	iconName          = IconName;
	can_arrow = true;
	pulldown = Pulldown;
}


CMenuOptionStringChooser::~CMenuOptionStringChooser()
{
	options.clear();
}

void CMenuOptionStringChooser::addOption(const char * const value)
{
	options.push_back(std::string(value));
}

void CMenuOptionStringChooser::sortOptions()
{
	sort(options.begin(), options.end());
}

int CMenuOptionStringChooser::exec(CMenuTarget* parent)
{
	bool wantsRepaint = false;
	int ret = menu_return::RETURN_NONE;

	if (parent)
		parent->hide();

	if((!parent || msg == CRCInput::RC_ok) && pulldown) {
		int select = -1;
		char cnt[5];

		CMenuWidget* menu = new CMenuWidget(optionName, NEUTRINO_ICON_SETTINGS);
		menu->addIntroItems();
		//if(parent) menu->move(20, 0);
		CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
		for(unsigned int count = 0; count < options.size(); count++) 
		{
			bool selected = false;
			if (strcmp(options[count].c_str(), optionValue) == 0)
				selected = true;
			sprintf(cnt, "%d", count);
			CMenuForwarderNonLocalized *mn_option = new CMenuForwarderNonLocalized(options[count].c_str(), true, NULL, selector, cnt);
			mn_option->setItemButton(NEUTRINO_ICON_BUTTON_OKAY, true /*for selected item*/);
			menu->addItem(mn_option, selected);
		}
		menu->exec(NULL, "");
		ret = menu_return::RETURN_REPAINT;
		if(select >= 0)
			strcpy(optionValue, options[select].c_str());
		delete menu;
		delete selector;
	} else {
		//select next value
		for(unsigned int count = 0; count < options.size(); count++) {
			if (strcmp(options[count].c_str(), optionValue) == 0) {
				if(msg == CRCInput::RC_left) {
					if(count > 0)
						strcpy(optionValue, options[(count - 1) % options.size()].c_str());
					else
						strcpy(optionValue, options[options.size() - 1].c_str());
				} else
					strcpy(optionValue, options[(count + 1) % options.size()].c_str());
				wantsRepaint = true;
				break;
			}
		}
	}

	if(parent)
		paint(true);
	if(observ) {
		wantsRepaint = observ->changeNotify(optionName, optionValue);
	}
	if (wantsRepaint)
		ret = menu_return::RETURN_REPAINT;

	return ret;
}

int CMenuOptionStringChooser::paint( bool selected, bool /*last*/ )
{
	const char * l_optionName = g_Locale->getText(optionName);
	
	//paint item
	prepareItem(selected, height);

	//paint item icon
	paintItemButton(selected, height, NEUTRINO_ICON_BUTTON_OKAY);
	
	//paint text
	paintItemCaption(selected, height , l_optionName, optionValue);

	return y+height;
}

//-------------------------------------------------------------------------------------------------------------------------------

CMenuOptionLanguageChooser::CMenuOptionLanguageChooser(char* OptionValue, CChangeObserver* Observ, const char * const IconName)
{
	height      = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	optionValue = OptionValue;
	observ      = Observ;

	directKey   = CRCInput::RC_nokey;
	iconName = IconName ? IconName : "";
}


CMenuOptionLanguageChooser::~CMenuOptionLanguageChooser()
{
	options.clear();
}

void CMenuOptionLanguageChooser::addOption(const char * const value)
{
	options.push_back(std::string(value));
}

int CMenuOptionLanguageChooser::exec(CMenuTarget*)
{
	bool wantsRepaint = false;

	//select value
	for(unsigned int count = 0; count < options.size(); count++)
	{
		if (strcmp(options[count].c_str(), optionValue) == 0)
		{
			strcpy(g_settings.language, options[(count + 1) % options.size()].c_str());
			break;
		}
	}

	paint(true);
	if(observ)
	{
		wantsRepaint = observ->changeNotify(LOCALE_LANGUAGESETUP_SELECT, optionValue);
	}
	return menu_return::RETURN_EXIT;
	if ( wantsRepaint )
		return menu_return::RETURN_REPAINT;
	else
		return menu_return::RETURN_NONE;
}

int CMenuOptionLanguageChooser::paint( bool selected, bool /*last*/ )
{
	active = true;
		
	//paint item
	prepareItem(selected, height);

	paintItemButton(selected, height, iconName);
	
	//convert first letter to large
	string s_optionValue = static_cast<std::string> (optionValue);
	string ts = s_optionValue.substr(0, 1);
	string s(ts);
	s = toupper(s_optionValue[0]);
	s_optionValue.replace(0, 1, s);
	
	//paint text
	paintItemCaption(selected, height , s_optionValue.c_str());

	return y+height;
}



//-------------------------------------------------------------------------------------------------------------------------------
CMenuForwarder::CMenuForwarder(const neutrino_locale_t Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right)
{
	option = Option;
	option_string = NULL;
	text=Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
	iconName_Info_right = IconName_Info_right ? IconName_Info_right : "";
}

CMenuForwarder::CMenuForwarder(const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right)
{
	option = NULL;
	option_string = &Option;
	text=Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
	iconName_Info_right = IconName_Info_right ? IconName_Info_right : "";
}

int CMenuForwarder::getHeight(void) const
{
	return g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
}

// used gets set by the addItem() function. This is for set to paint Option string by just not calling the addItem() function.
// Without this, the changeNotifiers would become machine-dependent.
void CMenuForwarder::setOption(const char *Option)
{
	option = Option;

	if (used && x != -1)
		paint();
}

// used gets set by the addItem() function. This is for set to paint Text from locales by just not calling the addItem() function.
// Without this, the changeNotifiers would become machine-dependent.
void CMenuForwarder::setTextLocale(const neutrino_locale_t Text)
{
	text=Text;

	if (used && x != -1)
		paint();
}

int CMenuForwarder::getWidth(void)
{
	int tw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(text), true);
	const char * option_text = NULL;

	if (option)
		option_text = option;
	else if (option_string)
		option_text = option_string->c_str();


        if (option_text != NULL)
                tw += 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(option_text, true);

	return tw;
}

int CMenuForwarder::exec(CMenuTarget* parent)
{
	if(jumpTarget)
	{
		return jumpTarget->exec(parent, actionKey);
	}
	else
	{
		return menu_return::RETURN_EXIT;
	}
}

const char * CMenuForwarder::getOption(void)
{
	if (option)
		return option;
	else
		if (option_string)
			return option_string->c_str();
		else
			return NULL;
}

const char * CMenuForwarder::getName(void)
{
	return g_Locale->getText(text);
}

int CMenuForwarder::paint(bool selected, bool /*last*/)
{
	int height = getHeight();
 	const char * l_text = getName();
 
 	const char * option_text = getOption();
	
	//paint item
	prepareItem(selected, height);
	
	//paint icon
	paintItemButton(selected, height);
	
	//caption
	paintItemCaption(selected, height, l_text, option_text);
	
	return y+ height;
}


//-------------------------------------------------------------------------------------------------------------------------------
const char * CMenuForwarderNonLocalized::getName(void)
{
	return the_text.c_str();
}

CMenuForwarderNonLocalized::CMenuForwarderNonLocalized(const char * const Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right) : CMenuForwarder(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName, IconName_Info_right)
{
	the_text = Text;
}

CMenuForwarderNonLocalized::CMenuForwarderNonLocalized(const char * const Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const IconName_Info_right) : CMenuForwarder(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName, IconName_Info_right)
{
	the_text = Text;
}

// used gets set by the addItem() function. This is for set to paint non localized Text by just not calling the addItem() function.
// Without this, the changeNotifiers would become machine-dependent.
void CMenuForwarderNonLocalized::setText(const char * const Text)
{
	the_text = Text;

	if (used && x != -1)
		paint();
}

int CMenuForwarderNonLocalized::getWidth(void)
{
	int tw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(the_text, true);
	return tw;
}
//-------------------------------------------------------------------------------------------------------------------------------
CMenuSeparator::CMenuSeparator(const int Type, const neutrino_locale_t Text)
{
	directKey = CRCInput::RC_nokey;
	iconName = "";
	type     = Type;
	text     = Text;
}


int CMenuSeparator::getHeight(void) const
{
	if (separator_text.empty() && text == NONEXISTANT_LOCALE)
		return 10;
	else
		return  g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
}

const char * CMenuSeparator::getString(void)
{
	if (!separator_text.empty())
		return separator_text.c_str();
	else	
		return g_Locale->getText(text);
}

void CMenuSeparator::setString(const std::string& s_text)
{
	separator_text = s_text;
}

int CMenuSeparator::getWidth(void)
{
	int w = 0;
	if (type & LINE)
		w = 30; /* 15 pixel left and right */
	if ((type & STRING) && text != NONEXISTANT_LOCALE)
	{
		const char *l_text = getString();
		w += g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true);
	}
	return w;
}

int CMenuSeparator::paint(bool selected, bool /*last*/)
{
	int height;
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	height = getHeight();
	
	if ((type & SUB_HEAD))
	{
		item_color = COL_MENUHEAD;
		item_bgcolor = COL_MENUHEAD_PLUS_0;
	}
	else
	{
		item_color = COL_MENUCONTENTINACTIVE;
		item_bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	frameBuffer->paintBoxRel(x,y, dx, height, item_bgcolor);
	if ((type & LINE))
	{
		frameBuffer->paintHLineRel(x+10,dx-20,y+(height>>1), COL_MENUCONTENT_PLUS_3);
		frameBuffer->paintHLineRel(x+10,dx-20,y+(height>>1)+1, COL_MENUCONTENT_PLUS_1);
	}
	if ((type & STRING))
	{
		const char * l_text;
		l_text = getString();
	
		if (text != NONEXISTANT_LOCALE || strlen(l_text) != 0)
		{
			int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true); // UTF-8

			/* if no alignment is specified, align centered */
			if (type & ALIGN_LEFT)
				name_start_x = x + (!SUB_HEAD ?  name_start_x : 20 +18);
			else if (type & ALIGN_RIGHT)
				name_start_x = x + dx - stringwidth - 20;
			else /* ALIGN_CENTER */
				name_start_x = x + (dx >> 1) - (stringwidth >> 1);
			
 			frameBuffer->paintBoxRel(name_start_x-5, y, stringwidth+10, height, item_bgcolor);
			
			paintItemCaption(selected, height, l_text);
		}
	}
	return y+ height;
}

bool CPINProtection::check()
{
	char cPIN[5];
	neutrino_locale_t hint = NONEXISTANT_LOCALE;
	do
	{
		cPIN[0] = 0;
		CPINInput* PINInput = new CPINInput(LOCALE_PINPROTECTION_HEAD, cPIN, 4, hint);
		PINInput->exec( getParent(), "");
		delete PINInput;
		hint = LOCALE_PINPROTECTION_WRONGCODE;
	} while ((strncmp(cPIN,validPIN,4) != 0) && (cPIN[0] != 0));
	return ( strncmp(cPIN,validPIN,4) == 0);
}


bool CZapProtection::check()
{

	int res;
	char cPIN[5];
	neutrino_locale_t hint2 = NONEXISTANT_LOCALE;
	do
	{
		cPIN[0] = 0;

		CPLPINInput* PINInput = new CPLPINInput(LOCALE_PARENTALLOCK_HEAD, cPIN, 4, hint2, fsk);

		res = PINInput->exec(getParent(), "");
		delete PINInput;

		hint2 = LOCALE_PINPROTECTION_WRONGCODE;
	} while ( (strncmp(cPIN,validPIN,4) != 0) &&
		  (cPIN[0] != 0) &&
		  ( res == menu_return::RETURN_REPAINT ) &&
		  ( fsk >= g_settings.parentallock_lockage ) );
	return ( ( strncmp(cPIN,validPIN,4) == 0 ) ||
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
        if (actionKey != "")
                *m_select = atoi(actionKey.c_str());
        else
                *m_select = -1;
        return menu_return::RETURN_EXIT;
}

