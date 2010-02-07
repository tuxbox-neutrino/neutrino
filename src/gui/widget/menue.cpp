/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/menue.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include <gui/color.h>

#include <gui/widget/stringinput.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/icons.h>


#include <cctype>

#define ROUND_RADIUS 9

/* the following generic menu items are integrated into multiple menus at the same time */
CMenuSeparator CGenericMenuSeparator;
CMenuSeparator CGenericMenuSeparatorLine(CMenuSeparator::LINE);
CMenuForwarder CGenericMenuBack(LOCALE_MENU_BACK);
CMenuSeparator * const GenericMenuSeparator = &CGenericMenuSeparator;
CMenuSeparator * const GenericMenuSeparatorLine = &CGenericMenuSeparatorLine;
CMenuForwarder * const GenericMenuBack = &CGenericMenuBack;

void CMenuItem::init(const int X, const int Y, const int DX, const int OFFX)
{
	x    = X;
	y    = Y;
	dx   = DX;
	offx = OFFX;
}

void CMenuItem::setActive(const bool Active)
{
	active = Active;
	if (x != -1)
		paint();
}

CMenuWidget::CMenuWidget()
{
        nameString = g_Locale->getText(NONEXISTANT_LOCALE);
	name = NONEXISTANT_LOCALE;
        iconfile = "";
        selected = -1;
        iconOffset = 0;
	offx = offy = 0;
}

CMenuWidget::CMenuWidget(const neutrino_locale_t Name, const std::string & Icon, const int mwidth, const int mheight)
{
	name = Name;
        nameString = g_Locale->getText(NONEXISTANT_LOCALE);

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
        selected = -1;
	needed_width = 0; /* is set in addItem() */
	width = 0; /* is set in paint() */

	if (mwidth > 100) /* warn about abuse until we found all offenders... */
		fprintf(stderr, "CMenuWidget::Init (%s) (%s) mwidth over 100%%: %d\n", nameString.c_str(), Icon.c_str(), mwidth);
	else
		needed_width = frameBuffer->getScreenWidth() * mwidth / 100;

	/* set the max height to 9/10 of usable screen height
	   debatable, if the callers need a possibility to set this */
	height = frameBuffer->getScreenHeight() / 20 * 18; /* make sure its a multiple of 2 */
	wanted_height = height;
        current_page=0;
	offx = offy = 0;
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
		    (item != GenericMenuBack))
			delete item;
	}
	items.clear();
	page_start.clear();
}

void CMenuWidget::addItem(CMenuItem* menuItem, const bool defaultselected)
{
	if (defaultselected)
		selected = items.size();
	int tmpw = menuItem->getWidth() + 10 + 10; /* 10 pixels to the left and right of the text */
	if (tmpw > needed_width) {
		//fprintf(stderr, "CMenuWidget::addItem: increase width from %d to %d '%s' offx: %d\n", needed_width, tmpw, menuItem->iconName.c_str(), offx);
		needed_width = tmpw;
	}
	items.push_back(menuItem);
}

bool CMenuWidget::hasItem()
{
	return !items.empty();
}

#define FADE_TIME 40000
int CMenuWidget::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int pos = 0;
	exit_pressed = false;

	frameBuffer->Lock();

	if (parent)
		parent->hide();

	bool fadeIn = g_settings.widget_fade;
	bool fadeOut = false;
	int fadeValue;
	uint32_t fadeTimer = 0;
        if ( fadeIn ) {
		fadeValue = 0x10;
		frameBuffer->setBlendLevel(fadeValue, fadeValue);
        }
        else
		fadeValue = g_settings.gtx_alpha1;

	paint();
	int retval = menu_return::RETURN_REPAINT;
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	if ( fadeIn )
		fadeTimer = g_RCInput->addTimer( FADE_TIME, false );
	do {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

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
							fadeValue -= 0x10;
							if (fadeValue <= 0x10) {
								fadeValue = g_settings.gtx_alpha1;
								g_RCInput->killTimer (fadeTimer);
								msg = CRCInput::RC_timeout;
							} else
								frameBuffer->setBlendLevel(fadeValue, fadeValue);
						} else { // appears
							fadeValue += 0x10;
							if (fadeValue >= g_settings.gtx_alpha1) {
								fadeValue = g_settings.gtx_alpha1;
								g_RCInput->killTimer (fadeTimer);
								fadeIn = false;
								frameBuffer->setBlendLevel(g_settings.gtx_alpha1, g_settings.gtx_alpha2);
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
					if(!(items[selected]->can_arrow) || g_settings.menu_left_exit) {
						msg = CRCInput::RC_timeout;
						break;
					}
				case (CRCInput::RC_right):
				case (CRCInput::RC_ok):
					{
						if(hasItem()) {
							//exec this item...
							CMenuItem* item = items[selected];
							item->msg = msg;
							if ( fadeIn ) {
								g_RCInput->killTimer(fadeTimer);
								frameBuffer->setBlendLevel(g_settings.gtx_alpha1, g_settings.gtx_alpha2);
								fadeIn = false;
							}
							int rv = item->exec( this );
							switch ( rv ) {
								case menu_return::RETURN_EXIT_ALL:
									retval = menu_return::RETURN_EXIT_ALL;
								case menu_return::RETURN_EXIT:
									msg = CRCInput::RC_timeout;
									break;
								case menu_return::RETURN_REPAINT:
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
					fadeIn = false;
				}
				if ((!fadeOut) && g_settings.widget_fade) {
					fadeOut = true;
					fadeTimer = g_RCInput->addTimer( FADE_TIME, false );
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
		}
	}
	while ( msg!=CRCInput::RC_timeout );
	hide();

	if ( fadeIn || fadeOut ) {
		g_RCInput->killTimer(fadeTimer);
		frameBuffer->setBlendLevel(g_settings.gtx_alpha1, g_settings.gtx_alpha2);
	}

	if(!parent)
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);

	for (unsigned int count = 0; count < items.size(); count++) {
		CMenuItem* item = items[count];
		item->init(-1, 0, 0, 0);
	}

	frameBuffer->Unlock();
	return retval;
}

void CMenuWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width+15,height+10 );
}

void CMenuWidget::paint()
{
	const char * l_name;
	if(name == NONEXISTANT_LOCALE)
		l_name = nameString.c_str();
	else
        	l_name = g_Locale->getText(name);

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8 /*, l_name*/); //FIXME menu name

	height=wanted_height;
	width = needed_width;

	if(height > ((int)frameBuffer->getScreenHeight() - 10))
		height = frameBuffer->getScreenHeight() - 10;

	int neededWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(l_name, true); // UTF-8
	if (neededWidth > width-48) {
		width= neededWidth+ 49;
	}
	int hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
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

	//frameBuffer->paintBoxRel(x,y, width+sb_width,hheight, COL_MENUHEAD_PLUS_0);
	frameBuffer->paintBoxRel(x, y, width+sb_width, hheight, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, CORNER_TOP); //FIXME rounded

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+38,y+hheight+1, width-40, l_name, COL_MENUHEAD, 0, true); // UTF-8
	frameBuffer->paintIcon(iconfile, x + 8, y, hheight);

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
		int sbh= ((item_height-4) / total_pages);
		// items box
		frameBuffer->paintBoxRel(x, item_start_y, width+15, item_height+10, COL_MENUCONTENT_PLUS_0, ROUND_RADIUS, CORNER_BOTTOM);
		// scrollbar
		frameBuffer->paintBoxRel(x+ width,item_start_y, 15, item_height, COL_MENUCONTENT_PLUS_1);
		frameBuffer->paintBoxRel(x+ width +2, item_start_y+ 2+ current_page* sbh, 11, sbh, COL_MENUCONTENT_PLUS_3);
	} else
		frameBuffer->paintBoxRel(x, item_start_y, width,item_height, COL_MENUCONTENT_PLUS_0, ROUND_RADIUS, CORNER_BOTTOM);//FIXME round

	int ypos=item_start_y;
	for (unsigned int count = 0; count < items.size(); count++) {
		CMenuItem* item = items[count];

		if ((count >= page_start[current_page]) && (count < page_start[current_page + 1])) {
			item->init(x, ypos, width, iconOffset);
			if( (item->isSelectable()) && (selected==-1) ) {
				ypos = item->paint(true);
				selected = count;
			} else {
				//ypos = item->paint(selected==((signed int) count), (count == items.size()-1));
				ypos = item->paint(selected==((signed int) count), (count == (page_start[current_page + 1]-1)));
			}
		} else {
			/* x = -1 is a marker which prevents the item from being painted on setActive changes */
			item->init(-1, 0, 0, 0);
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------------------
CMenuOptionNumberChooser::CMenuOptionNumberChooser(const neutrino_locale_t name, int * const OptionValue, const bool Active, const int min_value, const int max_value, CChangeObserver * const Observ, const int print_offset, const int special_value, const neutrino_locale_t special_value_name, const char * non_localized_name)
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

int CMenuOptionNumberChooser::paint(bool selected, bool last)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();

	unsigned char color   = COL_MENUCONTENT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;
	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}

	if(selected)
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS); //FIXME
	else if(last)
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS, CORNER_BOTTOM); //FIXME
	else
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor);

	const char * l_option;
	char option_value[11];

	if ((localized_value_name == NONEXISTANT_LOCALE) || ((*optionValue) != localized_value))
	{
		sprintf(option_value, "%d", ((*optionValue) + display_offset));
		l_option = option_value;
	}
	else
		l_option = g_Locale->getText(localized_value_name);

	int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_option, true); // UTF-8
	int stringstartposName = x + offx + 10;
	int stringstartposOption = x + dx - stringwidth - 10; //+ offx

	const char * l_optionName = (optionString != NULL) ? optionString : g_Locale->getText(optionName);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName,   y+height,dx- (stringstartposName - x), l_optionName, color, 0, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y+height,dx- (stringstartposOption - x), l_option, color, 0, true); // UTF-8

	if (selected)
	{
		char str[256];
		snprintf(str, 255, "%s %s", l_optionName, l_option);
		CVFD::getInstance()->showMenuText(0, str, -1, true);
		//CVFD::getInstance()->showMenuText(0, l_optionName, -1, true); // UTF-8
		//CVFD::getInstance()->showMenuText(1, l_option, -1, true); // UTF-8
	}

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
		menu->move(20, 0);
		CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
		for(unsigned int count = 0; count < number_of_options; count++) {
			bool selected = false;
			const char * l_option;
			if (options[count].key == (*optionValue))
				selected = true;

			if(options[count].valname != 0)
				l_option = options[count].valname;
			else
				l_option = g_Locale->getText(options[count].value);
			sprintf(cnt, "%d", count);
			menu->addItem(new CMenuForwarderNonLocalized(l_option, true, NULL, selector, cnt), selected);
		}
		menu->exec(NULL, "");
		ret = menu_return::RETURN_REPAINT;;
		if(select >= 0) {
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

int CMenuOptionChooser::paint( bool selected , bool last)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();

	unsigned char color   = COL_MENUCONTENT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;
	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}

	if(selected)
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS); //FIXME
	else if(last)
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS, CORNER_BOTTOM); //FIXME
	else
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor);

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
	if(l_option == NULL) {
		*optionValue = options[0].key;
		option = options[0].value;
		if(options[0].valname != 0)
			l_option = options[0].valname;
		else
			l_option = g_Locale->getText(option);
	}

	if (!(iconName.empty()))
	{
		frameBuffer->paintIcon(iconName, x + 10, y, height);
	}
	else if (CRCInput::isNumeric(directKey))
	{
		std::string number = CRCInput::getKeyName(directKey);
		if (! frameBuffer->paintIcon(number, x + 10, y, height))
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]
				->RenderString(x + 15, y + height, height, number, color, height);
	}

	int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_option, true); // UTF-8
	int stringstartposName = x + offx + 10;
	int stringstartposOption = x + dx - stringwidth - 10; //+ offx

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName,   y+height,dx- (stringstartposName - x), optionNameString.c_str(), color, 0, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y+height,dx- (stringstartposOption - x), l_option, color, 0, true); // UTF-8

	if (selected)
	{
		char str[256];
		snprintf(str, 255, "%s %s", optionNameString.c_str(), l_option);
		CVFD::getInstance()->showMenuText(0, str, -1, true);
		//CVFD::getInstance()->showMenuText(0, optionNameString.c_str(), -1, true); // UTF-8
		//CVFD::getInstance()->showMenuText(1, l_option, -1, true); // UTF-8
	}

	return y+height;
}

int CMenuOptionChooser::getWidth(void) const
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
		//if(parent) menu->move(20, 0);
		CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
		for(unsigned int count = 0; count < options.size(); count++) {
			bool selected = false;
			if (strcmp(options[count].c_str(), optionValue) == 0)
				selected = true;
			sprintf(cnt, "%d", count);
			menu->addItem(new CMenuForwarderNonLocalized(options[count].c_str(), true, NULL, selector, cnt), selected);
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

int CMenuOptionStringChooser::paint( bool selected, bool last )
{
	CFrameBuffer *fb = CFrameBuffer::getInstance();
	unsigned char color   = COL_MENUCONTENT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;
	if (selected) {
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	if (!active) {
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}

	if(selected)
		fb->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS); //FIXME
	else if(last)
		fb->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS, CORNER_BOTTOM); //FIXME
	else
		fb->paintBoxRel(x, y, dx, height, bgcolor);


	const char * l_optionName = g_Locale->getText(optionName);
	int optionwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_optionName, true);
	//int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(optionValue, true);
	int stringstartposName = x + offx + 10;
	//int stringstartposOption = x + dx - stringwidth - 10; //+ offx
	int stringstartposOption = x + offx + 10 + 10 + optionwidth;

	if (!(iconName.empty()))
	{
		fb->paintIcon(iconName, x + 10, y, height);
	}
	else if (CRCInput::isNumeric(directKey))
	{
		std::string number = CRCInput::getKeyName(directKey);
		if (! fb->paintIcon(number, x + 10, y, height))
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]
				->RenderString(x + 15, y + height, height, number, color, height);
        }

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName,   y+height, dx- (stringstartposName - x),  l_optionName, color, 0, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y+height, dx- (stringstartposOption - x), optionValue, color, 0, true);

	if (selected)
	{
		char str[256];
		snprintf(str, 255, "%s %s", l_optionName, optionValue);
		CVFD::getInstance()->showMenuText(0, str, -1, true);
		//CVFD::getInstance()->showMenuText(0, l_optionName, -1, true); // UTF-8
		//CVFD::getInstance()->showMenuText(1, optionValue);
	}

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

int CMenuOptionLanguageChooser::paint( bool selected, bool last )
{
	unsigned char color   = COL_MENUCONTENT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;
	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}

	if(selected)
		CFrameBuffer::getInstance()->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS); //FIXME
	else if(last)
		CFrameBuffer::getInstance()->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS, CORNER_BOTTOM); //FIXME
	else
		CFrameBuffer::getInstance()->paintBoxRel(x, y, dx, height, bgcolor);

	if (!(iconName.empty()))
	{
		CFrameBuffer::getInstance()->paintIcon(iconName, x + 10, y, height);
	}

	//int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(optionValue);
	int stringstartposOption = x + offx + 10;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y+height,dx- (stringstartposOption - x), optionValue, color);

	if (selected)
	{
		CVFD::getInstance()->showMenuText(1, optionValue);
	}

	return y+height;
}



//-------------------------------------------------------------------------------------------------------------------------------
CMenuForwarder::CMenuForwarder(const neutrino_locale_t Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName)
{
	option = Option;
	option_string = NULL;
	text=Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
}

CMenuForwarder::CMenuForwarder(const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName)
{
	option = NULL;
	option_string = &Option;
	text=Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
}

int CMenuForwarder::getHeight(void) const
{
	return g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
}

int CMenuForwarder::getWidth(void) const
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

int CMenuForwarder::paint(bool selected, bool last)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	int height = getHeight();
	const char * l_text = getName();
	int stringstartposX = x + offx + 10;

	const char * option_text = getOption();

	if (selected)
	{
		char str[256];

		if (option_text != NULL) {
			snprintf(str, 255, "%s %s", l_text, option_text);
			CVFD::getInstance()->showMenuText(0, str, -1, true);
		} else
			CVFD::getInstance()->showMenuText(0, l_text, -1, true);
	}

	unsigned char color   = COL_MENUCONTENT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;
	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}

	if(selected)
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS); //FIXME
	else if(last)
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor, ROUND_RADIUS, CORNER_BOTTOM); //FIXME
	else
		frameBuffer->paintBoxRel(x, y, dx, height, bgcolor);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposX, y+ height, dx- (stringstartposX - x), l_text, color, 0, true); // UTF-8

	if (!iconName.empty())
	{
		frameBuffer->paintIcon(iconName, x + 10, y, height);
	}
	else if (CRCInput::isNumeric(directKey))
	{
		std::string number = CRCInput::getKeyName(directKey);
		if (! frameBuffer->paintIcon(number, x + 10, y, height))
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]
				->RenderString(x + 15, y + height, height, number, color, height);
	}

	if (option_text != NULL)
	{
		int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(option_text, true);
		int stringstartposOption = std::max(stringstartposX + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true) + 10,
											x + dx - stringwidth - 10); //+ offx
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y+height,dx- (stringstartposOption- x),  option_text, color, 0, true);
	}

	return y+ height;
}


//-------------------------------------------------------------------------------------------------------------------------------
const char * CMenuForwarderNonLocalized::getName(void)
{
	return the_text.c_str();
}

CMenuForwarderNonLocalized::CMenuForwarderNonLocalized(const char * const Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName) : CMenuForwarder(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName)
{
	the_text = Text;
}

CMenuForwarderNonLocalized::CMenuForwarderNonLocalized(const char * const Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName) : CMenuForwarder(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName)
{
    the_text = Text;
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
	return (text == NONEXISTANT_LOCALE) ? 10 : g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
}

int CMenuSeparator::getWidth(void) const
{
	int w = 0;
	if (type & LINE)
		w = 30; /* 15 pixel left and right */
	if ((type & STRING) && text != NONEXISTANT_LOCALE)
	{
		const char *l_text = g_Locale->getText(text);
		w += g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true);
	}
	return w;
}

int CMenuSeparator::paint(bool selected, bool /*last*/)
{
	int height;
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	height = getHeight();

	frameBuffer->paintBoxRel(x,y, dx, height, COL_MENUCONTENT_PLUS_0);
	if ((type & LINE))
	{
		frameBuffer->paintHLineRel(x+10,dx-20,y+(height>>1), COL_MENUCONTENT_PLUS_3);
		frameBuffer->paintHLineRel(x+10,dx-20,y+(height>>1)+1, COL_MENUCONTENT_PLUS_1);
	}
	if ((type & STRING))
	{

		if (text != NONEXISTANT_LOCALE)
		{
			int stringstartposX;

			const char *l_text = g_Locale->getText(text);
			int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true); // UTF-8

			/* if no alignment is specified, align centered */
			if (type & ALIGN_LEFT)
				stringstartposX = x + 20;
			else if (type & ALIGN_RIGHT)
				stringstartposX = x + dx - stringwidth - 20;
			else /* ALIGN_CENTER */
				stringstartposX = x + (dx >> 1) - (stringwidth >> 1);

			frameBuffer->paintBoxRel(stringstartposX-5, y, stringwidth+10, height, COL_MENUCONTENT_PLUS_0);

			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposX, y+height,dx- (stringstartposX- x) , l_text, COL_MENUCONTENTINACTIVE, 0, true); // UTF-8

			if (selected)
			{
				CVFD::getInstance()->showMenuText(0, l_text, -1, true); // UTF-8
				//CVFD::getInstance()->showMenuText(1, "", -1, true); // UTF-8
			}
		}
	}
	return y+ height;
}

bool CPINProtection::check()
{
	char cPIN[4];
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
	if( (g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_NEVER) || AlwaysAsk )
		if (!check())
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

