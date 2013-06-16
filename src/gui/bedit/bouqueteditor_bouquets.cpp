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

#include <global.h>
#include <neutrino.h>
#include "bouqueteditor_channels.h"
#include "bouqueteditor_bouquets.h"

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/components/cc_frm.h>
#include <gui/widget/buttons.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>
#include <gui/widget/stringinput.h>
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
	blueFunction = beRename;
	Bouquets = NULL;
	iheight = 0;
	ButtonHeight = 0;
	fheight = 0;
	theight = 0;
}

void CBEBouquetWidget::paintItem(int pos)
{
	uint8_t    color;
	fb_pixel_t bgcolor;
	int ypos = y+ theight+0 + pos*iheight;
	unsigned int current = liststart + pos;

	if (current == selected) {
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, COL_MENUCONTENT_PLUS_0);
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor, RADIUS_LARGE);
	} else {
		bool has_channels = true;
		if(current < Bouquets->size())
			has_channels = (!(*Bouquets)[current]->tvChannels.empty() ) || (!(*Bouquets)[current]->radioChannels.empty());
		color   = has_channels ? COL_MENUCONTENT : COL_MENUCONTENTINACTIVE;
		bgcolor = has_channels ? COL_MENUCONTENT_PLUS_0 : COL_MENUCONTENTINACTIVE_PLUS_0;
		frameBuffer->paintBoxRel(x,ypos, width- 15, iheight, bgcolor);
	}

	if(current < Bouquets->size()) {
		if ((current == selected) && (state == beMoving))
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + 10, ypos, iheight);

		if ((*Bouquets)[current]->bLocked != g_settings.parentallock_defaultlocked)
			frameBuffer->paintIcon(NEUTRINO_ICON_LOCK, x + 10, ypos, iheight);

		if ((*Bouquets)[current]->bHidden)
			frameBuffer->paintIcon(NEUTRINO_ICON_HIDDEN, x + 10, ypos, iheight);

		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+iconoffset+20, ypos + iheight - (iheight-fheight)/2, width-iconoffset-20, (*Bouquets)[current]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : (*Bouquets)[current]->Name, color, 0, true);
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
	frameBuffer->paintBoxRel(x+ width- 15,ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc= ((Bouquets->size()- 1)/ listmaxshow)+ 1;
	int sbs= (selected/listmaxshow);
	if (sbc < 1)
		sbc = 1;

	//scrollbar
	frameBuffer->paintBoxRel(x+ width- 13, ypos+ 2+ sbs * (sb-4)/sbc, 11, (sb-4)/sbc,  COL_MENUCONTENT_PLUS_3);
}

void CBEBouquetWidget::paintHead()
{
	CComponentsHeader header(x, y, width, theight, LOCALE_BOUQUETLIST_HEAD, NULL /*no header icon*/, CComponentsHeader::CC_BTN_MENU);
	header.paint(CC_SAVE_SCREEN_NO);
}

const struct button_label CBEBouquetWidgetButtons[4] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_BOUQUETEDITOR_DELETE },
	{ NEUTRINO_ICON_BUTTON_GREEN , LOCALE_BOUQUETEDITOR_ADD    },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETEDITOR_MOVE   },
	{ NEUTRINO_ICON_BUTTON_BLUE  , NONEXISTANT_LOCALE /*dummy*/}
};

void CBEBouquetWidget::paintFoot()
{
	struct button_label Button[4];

	Button[0] = CBEBouquetWidgetButtons[0];
	Button[1] = CBEBouquetWidgetButtons[1];
	Button[2] = CBEBouquetWidgetButtons[2];
	Button[3] = CBEBouquetWidgetButtons[3];

	switch( blueFunction)
	{
		case beRename:
			Button[3].locale = LOCALE_BOUQUETEDITOR_RENAME;
		break;
		case beHide:
			Button[3].locale = LOCALE_BOUQUETEDITOR_HIDE;
		break;
		case beLock:
			Button[3].locale = LOCALE_BOUQUETEDITOR_LOCK;
		break;
	}

	::paintButtons(x, y+height, width, 4, Button, width, ButtonHeight);
}

void CBEBouquetWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height+ButtonHeight);
	frameBuffer->blit();
}

void CBEBouquetWidget::updateSelection(unsigned int newpos)
{
	if(newpos == selected)
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

	ButtonHeight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight()+8;
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
	frameBuffer->blit();

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
					int result = ShowLocalizedMessage(LOCALE_BOUQUETEDITOR_NAME, LOCALE_BOUQUETEDITOR_SAVECHANGES, CMessageBox::mbrYes, CMessageBox::mbAll);

					switch( result )
					{
						case CMessageBox::mbrYes :
							loop=false;
							saveChanges();
						break;
						case CMessageBox::mbrNo :
							loop=false;
							discardChanges();
						break;
						case CMessageBox::mbrCancel :
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
		else if (msg==CRCInput::RC_up || msg==(neutrino_msg_t)g_settings.key_channelList_pageup)
		{
			if (!(Bouquets->empty())) {
				int step = (msg == (neutrino_msg_t)g_settings.key_channelList_pageup) ? listmaxshow : 1;  // browse or step 1
				int new_selected = selected - step;

				if (new_selected < 0) {
					if (selected != 0 && step != 1)
						new_selected = 0;
					else
						new_selected = Bouquets->size() - 1;
				}
				updateSelection(new_selected);
			}
		}
		else if (msg==CRCInput::RC_down || msg==(neutrino_msg_t)g_settings.key_channelList_pagedown)
		{
			if (!(Bouquets->empty())) {
				int step =  ((int) msg == g_settings.key_channelList_pagedown) ? listmaxshow : 1;  // browse or step 1
				int new_selected = selected + step;
				if (new_selected >= (int) Bouquets->size()) {
					if ((Bouquets->size() - listmaxshow -1 < selected) && (selected != (Bouquets->size() - 1)) && (step != 1))
						new_selected = Bouquets->size() - 1;
					else if (((Bouquets->size() / listmaxshow) + 1) * listmaxshow == Bouquets->size() + listmaxshow) // last page has full entries
						new_selected = 0;
					else
						new_selected = ((step == (int) listmaxshow) && (new_selected < (int) (((Bouquets->size() / listmaxshow)+1) * listmaxshow))) ? (Bouquets->size() - 1) : 0;
				}
				updateSelection(new_selected);
			}
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
					switch (blueFunction)
					{
					case beRename:
						renameBouquet();
						break;
					case beHide:
						switchHideBouquet();
						break;
					case beLock:
						switchLockBouquet();
						break;
					}
			}
		}
		else if(msg==CRCInput::RC_setup)
		{
			if (state == beDefault)
			switch (blueFunction)
			{
				case beRename:
					blueFunction = beHide;
				break;
				case beHide:
					blueFunction = beLock;
				break;
				case beLock:
					blueFunction = beRename;
				break;
			}
			paintFoot();
		}
		else if(msg==CRCInput::RC_ok)
		{
			if (state == beDefault)
			{
				if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
				{
					CBEChannelWidget* channelWidget = new CBEChannelWidget((*Bouquets)[selected]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : (*Bouquets)[selected]->Name, selected);
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
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
		}
		else
		{
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
			// kein canceling...
		}
		frameBuffer->blit();
	}
	hide();
	return res;
}

void CBEBouquetWidget::deleteBouquet()
{
	if (selected >= Bouquets->size()) /* Bouquets->size() might be 0 */
		return;

	if (ShowMsgUTF(LOCALE_FILEBROWSER_DELETE, (*Bouquets)[selected]->bFav ? g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME) : (*Bouquets)[selected]->Name, CMessageBox::mbrNo, CMessageBox::mbYes|CMessageBox::mbNo)!=CMessageBox::mbrYes)
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
		bouquetsChanged = true;
	}
	paint();
}

void CBEBouquetWidget::cancelMoveBouquet()
{
	state = beDefault;
	internalMoveBouquet( newPosition, origPosition);
	bouquetsChanged = false;
}

void CBEBouquetWidget::internalMoveBouquet( unsigned int fromPosition, unsigned int toPosition)
{
	if ( (int) toPosition == -1 ) return;
	if ( toPosition == Bouquets->size()) return;

	g_bouquetManager->moveBouquet(fromPosition, toPosition);
	Bouquets = &g_bouquetManager->Bouquets;
	bouquetsChanged = true;
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
	(*Bouquets)[selected]->bLocked = !(*Bouquets)[selected]->bLocked;
	paint();
}

std::string CBEBouquetWidget::inputName(const char * const defaultName, const neutrino_locale_t caption)
{
	char Name[30];

	strncpy(Name, defaultName, sizeof(Name)-1);

	CStringInputSMS * nameInput = new CStringInputSMS(caption, Name, 29, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789-.,:|!?/ ");
	nameInput->exec(this, "");
	delete nameInput;

	return std::string(Name);
}

void CBEBouquetWidget::saveChanges()
{
	CHintBox* hintBox= new CHintBox(LOCALE_BOUQUETEDITOR_NAME, g_Locale->getText(LOCALE_BOUQUETEDITOR_SAVINGCHANGES), 480); // UTF-8
	hintBox->paint();
	g_Zapit->saveBouquets();
	hintBox->hide();
	delete hintBox;
}

void CBEBouquetWidget::discardChanges()
{
	CHintBox* hintBox= new CHintBox(LOCALE_BOUQUETEDITOR_NAME, g_Locale->getText(LOCALE_BOUQUETEDITOR_DISCARDINGCHANGES), 480); // UTF-8
	hintBox->paint();
	g_Zapit->restoreBouquets();
	hintBox->hide();
	delete hintBox;
}
