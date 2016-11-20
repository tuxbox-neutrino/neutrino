/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2009,2011,2013,2016 Stefan Seyfried

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

#include <global.h>
#include <neutrino.h>
#include "bouqueteditor_channels.h"
#include "bouqueteditor_bouquets.h"

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/components/cc.h>

#include <gui/widget/hintbox.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <gui/widget/keyboard_input.h>
#include <zapit/client/zapittools.h>

extern CBouquetManager *g_bouquetManager;

CBEBouquetWidget::CBEBouquetWidget()
{
	frameBuffer = CFrameBuffer::getInstance();
	iconoffset = 0;
	origPosition = 0;
	newPosition = 0;
	listmaxshow = 0;
	bouquetsChanged = 0;
	width = 0;
	height = 0;
	x = 0;
	y = 0;
	selected = 0;
	liststart = 0;
	state = beDefault;
	Bouquets = NULL;
	iheight = 0;
	ButtonHeight = footer.getHeight();
	fheight = 0;
	theight = 0;
}

void CBEBouquetWidget::paintItem(int pos)
{
	int ypos = y+ theight+0 + pos*iheight;
	unsigned int current = liststart + pos;

	bool i_selected	= current == selected;
	int i_radius	= RADIUS_NONE;

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, i_selected);

	if (i_selected)
	{
		i_radius = RADIUS_LARGE;
	}
	else
	{
		bool has_channels = true;
		if(current < Bouquets->size())
			has_channels = (!(*Bouquets)[current]->tvChannels.empty() ) || (!(*Bouquets)[current]->radioChannels.empty());

		if (!has_channels)
			color = COL_MENUCONTENTINACTIVE_TEXT;
	}

	if (i_radius)
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor, i_radius);

	if (current < Bouquets->size()) {
		if ((i_selected) && (state == beMoving))
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + 5, ypos, iheight);

		if ((*Bouquets)[current]->bHidden)
			frameBuffer->paintIcon(NEUTRINO_ICON_HIDDEN, x + 26, ypos, iheight);

		if ((*Bouquets)[current]->bLocked != g_settings.parentallock_defaultlocked)
			frameBuffer->paintIcon(NEUTRINO_ICON_LOCK, x + 18 + iconoffset, ypos, iheight);

		if (!(*Bouquets)[current]->tvChannels.empty() ) {
			frameBuffer->paintIcon(NEUTRINO_ICON_VIDEO, x + 20 + 2*iconoffset - 2, ypos, iheight);
		}

		if (!(*Bouquets)[current]->radioChannels.empty()) {
			frameBuffer->paintIcon(NEUTRINO_ICON_AUDIO, x + 20+ 3*iconoffset - 4, ypos, iheight);
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x +20 + 4*iconoffset, ypos + iheight - (iheight-fheight)/2, width-iconoffset-20, (*Bouquets)[current]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : (*Bouquets)[current]->Name, color);
	}
}

void CBEBouquetWidget::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;

	for(unsigned int count=0;count<listmaxshow;count++)
	{
		paintItem(count);
	}

	int ypos = y+ theight;
	int sb = iheight* listmaxshow;
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_SCROLLBAR_PASSIVE_PLUS_0);

	int sbc= ((Bouquets->size()- 1)/ listmaxshow)+ 1;
	int sbs= (selected/listmaxshow);
	if (sbc < 1)
		sbc = 1;

	//scrollbar
	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs * (sb-4)/sbc, 11, (sb-4)/sbc,  COL_SCROLLBAR_ACTIVE_PLUS_0);
}

void CBEBouquetWidget::paintHead()
{
	CComponentsHeaderLocalized header(x, y, width, theight, LOCALE_BOUQUETLIST_HEAD, "" /*no header icon*/, CComponentsHeaderLocalized::CC_BTN_EXIT);
	header.paint(CC_SAVE_SCREEN_NO);
}

const struct button_label CBEBouquetWidgetButtons[6] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_BOUQUETEDITOR_DELETE },
	{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_BOUQUETEDITOR_ADD    },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETEDITOR_MOVE  },
	{ NEUTRINO_ICON_BUTTON_BLUE  , LOCALE_BOUQUETEDITOR_RENAME},
        { NEUTRINO_ICON_BUTTON_PAUSE  , LOCALE_BOUQUETEDITOR_HIDE   },
        { NEUTRINO_ICON_BUTTON_STOP  , LOCALE_BOUQUETEDITOR_LOCK     }
};

void CBEBouquetWidget::paintFoot()
{
	size_t numbuttons = sizeof(CBEBouquetWidgetButtons)/sizeof(CBEBouquetWidgetButtons[0]);
	footer.paintButtons(x, y+height, width, ButtonHeight, numbuttons, CBEBouquetWidgetButtons, width/numbuttons-20);
}

void CBEBouquetWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
	footer.kill();
}

void CBEBouquetWidget::updateSelection(unsigned int newpos)
{
	if (newpos == selected || newpos == (unsigned int)-1)
		return;

	unsigned int prev_selected = selected;
	selected = newpos;

	if (state == beDefault) {
		unsigned int oldliststart = liststart;
		liststart = (selected/listmaxshow)*listmaxshow;
		if(oldliststart!=liststart) {
			paint();
		} else {
			paintItem(prev_selected - liststart);
			paintItem(selected - liststart);
		}
	} else {
		internalMoveBouquet(prev_selected, selected);
	}
}

int CBEBouquetWidget::exec(CMenuTarget* parent, const std::string & /*actionKey*/)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	theight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	fheight     = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();

	int icol_w, icol_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icol_w, &icol_h);
	iheight = std::max(fheight, icol_h+2);
	iconoffset = std::max(iconoffset, icol_w);

	frameBuffer->getIconSize(NEUTRINO_ICON_LOCK, &icol_w, &icol_h);
	iheight = std::max(iheight, icol_h+2);
	iconoffset = std::max(iconoffset, icol_w);

	frameBuffer->getIconSize(NEUTRINO_ICON_HIDDEN, &icol_w, &icol_h);
	iheight = std::max(iheight, icol_h+2);
	iconoffset = std::max(iconoffset, icol_w);

	width  = frameBuffer->getScreenWidthRel();
	height = frameBuffer->getScreenHeightRel() - ButtonHeight;
	listmaxshow = (height-theight-0)/iheight;
	height = theight+0+listmaxshow*iheight; // recalc height
        x = getScreenStartX(width);
        y = getScreenStartY(height + ButtonHeight);

	Bouquets = &g_bouquetManager->Bouquets;
	paintHead();
	paint();
	paintFoot();

	bouquetsChanged = false;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_EPG]);

		if ((msg == CRCInput::RC_timeout) ||
		    (msg == (neutrino_msg_t)g_settings.key_channelList_cancel))
		{
			if (state == beDefault)
			{
				if (bouquetsChanged)
				{
					int result = ShowMsg(LOCALE_BOUQUETEDITOR_NAME, LOCALE_BOUQUETEDITOR_SAVECHANGES, CMsgBox::mbrYes, CMsgBox::mbYesNoCancel, NULL, 600);

					switch( result )
					{
						case CMsgBox::mbrYes :
							loop=false;
							saveChanges();
						break;
						case CMsgBox::mbrNo :
							loop=false;
							discardChanges();
						break;
						case CMsgBox::mbrCancel :
							paintHead();
							paint();
							paintFoot();
						break;
					}
				}
				else
				{
					loop = false;
				}
			}
			else if (state == beMoving)
			{
				cancelMoveBouquet();
			}
		}
		else if (msg == CRCInput::RC_up || msg == (neutrino_msg_t)g_settings.key_pageup ||
			 msg == CRCInput::RC_down || msg == (neutrino_msg_t)g_settings.key_pagedown)
		{
			int new_selected = UpDownKey(*Bouquets, msg, listmaxshow, selected);
			updateSelection(new_selected);
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_start || msg == (neutrino_msg_t) g_settings.key_list_end) {
			if (!(Bouquets->empty())) {
				int new_selected = msg == (neutrino_msg_t) g_settings.key_list_start ? 0 : Bouquets->size() - 1;
				updateSelection(new_selected);
			}
		}
		else if(msg==CRCInput::RC_red)
		{
			if (state == beDefault)
				deleteBouquet();
		}
		else if(msg==CRCInput::RC_green)
		{
			if (state == beDefault)
				addBouquet();
		}
		else if(msg==CRCInput::RC_yellow)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				liststart = (selected/listmaxshow)*listmaxshow;
				if (state == beDefault)
					beginMoveBouquet();
				paintItem(selected - liststart);
			}
		}
		else if(msg==CRCInput::RC_blue)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				if (state == beDefault)
						renameBouquet();
			}
		}

		else if(msg==CRCInput::RC_pause)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				if (state == beDefault)
						switchHideBouquet();
			}
		}
		else if(msg==CRCInput::RC_stop)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				if (state == beDefault)
						switchLockBouquet();
			}
		}
		else if(msg==CRCInput::RC_ok)
		{
			if (state == beDefault)
			{
				if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
				{
					std::string ChannelWidgetCaption=(*Bouquets)[selected]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : (*Bouquets)[selected]->Name;
					if (!(*Bouquets)[selected]->tvChannels.empty() ) {
						ChannelWidgetCaption = ChannelWidgetCaption+ "  =>  TV";
						if (!(*Bouquets)[selected]->radioChannels.empty())
							ChannelWidgetCaption = ChannelWidgetCaption+ "/Radio";
					}
					else if (!(*Bouquets)[selected]->radioChannels.empty()) {
						ChannelWidgetCaption = ChannelWidgetCaption+ "  =>     Radio";
					}

					CBEChannelWidget* channelWidget = new CBEChannelWidget(ChannelWidgetCaption, selected);
					channelWidget->exec( this, "");
					if (channelWidget->hasChanged())
						bouquetsChanged = true;
					delete channelWidget;
					paintHead();
					paint();
					paintFoot();
				}
			}
			else if (state == beMoving)
			{
				finishMoveBouquet();
			}
		}
		else if( CRCInput::isNumeric(msg) )
		{
			if (state == beDefault)
			{
				//kein pushback - wenn man versehentlich wo draufkommt is die edit-arbeit umsonst
				//selected = oldselected;
				//g_RCInput->postMsg( msg, data );
				//loop=false;
			}
			else if (state == beMoving)
			{
				cancelMoveBouquet();
			}
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			// do nothing
		}
		else
		{
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}
	}
	hide();
	return res;
}

void CBEBouquetWidget::deleteBouquet()
{
	if (selected >= Bouquets->size()) /* Bouquets->size() might be 0 */
		return;

	if (ShowMsg(LOCALE_FILEBROWSER_DELETE, (*Bouquets)[selected]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : (*Bouquets)[selected]->Name, CMsgBox::mbrNo, CMsgBox::mbYes|CMsgBox::mbNo)!=CMsgBox::mbrYes)
		return;

	g_bouquetManager->deleteBouquet(selected);
	Bouquets = &g_bouquetManager->Bouquets;
	if (selected >= Bouquets->size())
		selected = Bouquets->empty() ? 0 : (Bouquets->size() - 1);
	bouquetsChanged = true;
	paint();
}

void CBEBouquetWidget::addBouquet()
{
	std::string newName = inputName("", LOCALE_BOUQUETEDITOR_BOUQUETNAME);
	if (!(newName.empty()))
	{
		g_bouquetManager->addBouquet(newName, true);
		Bouquets = &g_bouquetManager->Bouquets;
		selected = Bouquets->empty() ? 0 : (Bouquets->size() - 1);
		bouquetsChanged = true;
	}
	paintHead();
	paint();
	paintFoot();
}

void CBEBouquetWidget::beginMoveBouquet()
{
	state = beMoving;
	origPosition = selected;
	newPosition = selected;
}

void CBEBouquetWidget::finishMoveBouquet()
{
	state = beDefault;
	if (newPosition != origPosition)
	{
		Bouquets = &g_bouquetManager->Bouquets;
		bouquetsChanged = bouquetsChanged | true;
	}
	paint();
}

void CBEBouquetWidget::cancelMoveBouquet()
{
	state = beDefault;
	internalMoveBouquet( newPosition, origPosition);
	bouquetsChanged = bouquetsChanged | false;
}

void CBEBouquetWidget::internalMoveBouquet( unsigned int fromPosition, unsigned int toPosition)
{
	if ( (int) toPosition == -1 ) return;
	if ( toPosition == Bouquets->size()) return;

	g_bouquetManager->moveBouquet(fromPosition, toPosition);
	Bouquets = &g_bouquetManager->Bouquets;
	//bouquetsChanged = true;
	selected = toPosition;
	newPosition = toPosition;
	paint();
}

void CBEBouquetWidget::renameBouquet()
{
	if ((*Bouquets)[selected]->bFav)
		return;

	std::string newName = inputName((*Bouquets)[selected]->Name.c_str(), LOCALE_BOUQUETEDITOR_NEWBOUQUETNAME);
	if (newName != (*Bouquets)[selected]->Name)
	{
		g_bouquetManager->Bouquets[selected]->Name = newName;
		g_bouquetManager->Bouquets[selected]->bUser = true;
		bouquetsChanged = true;
	}
	paintHead();
	paint();
	paintFoot();
}

void CBEBouquetWidget::switchHideBouquet()
{
	bouquetsChanged = true;
	(*Bouquets)[selected]->bHidden = !(*Bouquets)[selected]->bHidden;
	paint();
}

void CBEBouquetWidget::switchLockBouquet()
{
	bouquetsChanged = true;
	g_bouquetManager->setBouquetLock((*Bouquets)[selected], !(*Bouquets)[selected]->bLocked);
	paint();
}

std::string CBEBouquetWidget::inputName(const char * const defaultName, const neutrino_locale_t caption)
{
	std::string Name = defaultName;

	CKeyboardInput * nameInput = new CKeyboardInput(caption, &Name);
	nameInput->exec(this, "");
	delete nameInput;

	return std::string(Name);
}

void CBEBouquetWidget::saveChanges()
{
	CHintBox hintBox(LOCALE_BOUQUETEDITOR_NAME, g_Locale->getText(LOCALE_BOUQUETEDITOR_SAVINGCHANGES), 480); // UTF-8
	hintBox.paint();
	g_Zapit->saveBouquets();
	hintBox.hide();
}

void CBEBouquetWidget::discardChanges()
{
	CHintBox hintBox(LOCALE_BOUQUETEDITOR_NAME, g_Locale->getText(LOCALE_BOUQUETEDITOR_DISCARDINGCHANGES), 480); // UTF-8
	hintBox.paint();
	g_Zapit->restoreBouquets();
	hintBox.hide();
}
