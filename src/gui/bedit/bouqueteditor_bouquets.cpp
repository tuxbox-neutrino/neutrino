/*
	neutrino bouquet editor - bouquets editor

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2009,2011,2013,2016 Stefan Seyfried
	Copyright (C) 2017 Sven Hoefer

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <gui/components/cc.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/keyboard_input.h>
#include <gui/widget/msgbox.h>
#include <gui/widget/stringinput.h>
#include <zapit/client/zapittools.h>

#include "bouqueteditor_bouquets.h"
#include "bouqueteditor_channels.h"

extern CBouquetManager *g_bouquetManager;

CBEBouquetWidget::CBEBouquetWidget()
{
	origPosition = 0;
	newPosition = 0;
	bouquetsChanged = 0;
	selected = 0;
	liststart = 0;
	state = beDefault;
	Bouquets = NULL;

	int iw, ih;
	action_icon_width = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_DUMMY_SMALL, &action_icon_width, &ih);

	status_icon_width = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_MARKER_HIDDEN, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
	frameBuffer->getIconSize(NEUTRINO_ICON_MARKER_LOCK, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
	frameBuffer->getIconSize(NEUTRINO_ICON_MARKER_RADIO, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
	frameBuffer->getIconSize(NEUTRINO_ICON_MARKER_TV, &iw, &ih);
	status_icon_width = std::max(status_icon_width, iw);
}

void CBEBouquetWidget::paintItem(int pos)
{
	int ypos = y + header_height + pos*item_height;
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
		if (current < Bouquets->size())
			has_channels = (!(*Bouquets)[current]->tvChannels.empty()) || (!(*Bouquets)[current]->radioChannels.empty());

		if (!has_channels)
			color = COL_MENUCONTENTINACTIVE_TEXT;
	}

	if (i_radius)
		frameBuffer->paintBoxRel(x,ypos, width - SCROLLBAR_WIDTH, item_height, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x,ypos, width - SCROLLBAR_WIDTH, item_height, bgcolor, i_radius);

	if (current < Bouquets->size())
	{
		if ((i_selected) && (state == beMoving))
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + OFFSET_INNER_MID, ypos, item_height);
		else
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_DUMMY_SMALL, x + OFFSET_INNER_MID, ypos, item_height);

		int text_offset = 2*OFFSET_INNER_MID + action_icon_width;
		item_font->RenderString(x + text_offset, ypos + item_height, width - text_offset - SCROLLBAR_WIDTH - 5*OFFSET_INNER_MID - 4*status_icon_width, (*Bouquets)[current]->bName, color);

		if ((*Bouquets)[current]->bHidden)
			frameBuffer->paintIcon(NEUTRINO_ICON_MARKER_HIDDEN, x + width - SCROLLBAR_WIDTH - OFFSET_INNER_MID - status_icon_width, ypos, item_height);

		if ((*Bouquets)[current]->bLocked != g_settings.parentallock_defaultlocked)
			frameBuffer->paintIcon(NEUTRINO_ICON_MARKER_LOCK, x + width - SCROLLBAR_WIDTH - 2*OFFSET_INNER_MID - 2*status_icon_width, ypos, item_height);

		if (!(*Bouquets)[current]->radioChannels.empty())
			frameBuffer->paintIcon(NEUTRINO_ICON_MARKER_RADIO, x + width - SCROLLBAR_WIDTH - 3*OFFSET_INNER_MID - 3*status_icon_width, ypos, item_height);

		if (!(*Bouquets)[current]->tvChannels.empty())
			frameBuffer->paintIcon(NEUTRINO_ICON_MARKER_TV, x + width - SCROLLBAR_WIDTH - 4*OFFSET_INNER_MID - 4*status_icon_width, ypos, item_height);
	}
}

void CBEBouquetWidget::paintItems()
{
	liststart = (selected/items_count)*items_count;

	for(unsigned int count = 0; count < items_count; count++)
		paintItem(count);

	int total_pages;
	int current_page;
	getScrollBarData(&total_pages, &current_page, Bouquets->size(), items_count, selected);
	paintScrollBar(x + width - SCROLLBAR_WIDTH, y + header_height, SCROLLBAR_WIDTH, body_height, total_pages, current_page);
}

void CBEBouquetWidget::paintHead()
{
	CBEGlobals::paintHead(g_Locale->getText(LOCALE_BOUQUETLIST_HEAD), NEUTRINO_ICON_EDIT);
}

const struct button_label CBEBouquetWidgetButtons[] =
{
	{ NEUTRINO_ICON_BUTTON_RED,	LOCALE_BOUQUETEDITOR_DELETE	},
	{ NEUTRINO_ICON_BUTTON_GREEN,	LOCALE_BOUQUETEDITOR_ADD	},
	{ NEUTRINO_ICON_BUTTON_YELLOW,	LOCALE_BOUQUETEDITOR_MOVE	},
	{ NEUTRINO_ICON_BUTTON_BLUE,	LOCALE_BOUQUETEDITOR_RENAME	},
	{ NEUTRINO_ICON_BUTTON_PAUSE,	LOCALE_BOUQUETEDITOR_HIDE	},
	{ NEUTRINO_ICON_BUTTON_STOP,	LOCALE_BOUQUETEDITOR_LOCK	}
};

void CBEBouquetWidget::paintFoot()
{
	size_t numbuttons = sizeof(CBEBouquetWidgetButtons)/sizeof(CBEBouquetWidgetButtons[0]);

	CBEGlobals::paintFoot(numbuttons, CBEBouquetWidgetButtons);
}

void CBEBouquetWidget::updateSelection(unsigned int newpos)
{
	if (newpos == selected || newpos == (unsigned int)-1)
		return;

	unsigned int prev_selected = selected;
	selected = newpos;

	if (state == beDefault)
	{
		unsigned int oldliststart = liststart;
		liststart = (selected/items_count)*items_count;
		if (oldliststart != liststart)
		{
			paintItems();
		}
		else
		{
			paintItem(prev_selected - liststart);
			paintItem(selected - liststart);
		}
	}
	else
	{
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

	Bouquets = &g_bouquetManager->Bouquets;
	init();

	paintHead();
	paintBody();
	paintFoot();
	paintItems();

	bouquetsChanged = false;

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);

	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);

		if ((msg == CRCInput::RC_timeout) || (msg == (neutrino_msg_t)g_settings.key_channelList_cancel))
		{
			if (state == beDefault)
			{
				if (bouquetsChanged)
				{
					int result = ShowMsg(LOCALE_BOUQUETEDITOR_NAME, LOCALE_BOUQUETEDITOR_SAVECHANGES, CMsgBox::mbrYes, CMsgBox::mbYesNoCancel, NULL, 600);

					switch(result)
					{
						case CMsgBox::mbrYes:
						{
							loop=false;
							saveChanges();
							break;
						}
						case CMsgBox::mbrNo:
						{
							loop=false;
							discardChanges();
							break;
						}
						case CMsgBox::mbrCancel:
						{
							paintHead();
							paintBody();
							paintFoot();
							paintItems();
							break;
						}
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
			int new_selected = UpDownKey(*Bouquets, msg, items_count, selected);
			updateSelection(new_selected);
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_start || msg == (neutrino_msg_t) g_settings.key_list_end)
		{
			if (!(Bouquets->empty()))
			{
				int new_selected = msg == (neutrino_msg_t) g_settings.key_list_start ? 0 : Bouquets->size() - 1;
				updateSelection(new_selected);
			}
		}
		else if (msg == CRCInput::RC_red)
		{
			if (state == beDefault)
				deleteBouquet();
		}
		else if (msg == CRCInput::RC_green)
		{
			if (state == beDefault)
				addBouquet();
		}
		else if (msg == CRCInput::RC_yellow)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				liststart = (selected/items_count)*items_count;
				if (state == beDefault)
					beginMoveBouquet();
				else if (state == beMoving)
					finishMoveBouquet();
				paintItem(selected - liststart);
			}
		}
		else if (msg == CRCInput::RC_blue)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				if (state == beDefault)
					renameBouquet();
			}
		}
		else if (msg == CRCInput::RC_pause || msg == CRCInput::RC_playpause)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				if (state == beDefault)
					switchHideBouquet();
			}
		}
		else if (msg == CRCInput::RC_stop)
		{
			if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
			{
				if (state == beDefault)
					switchLockBouquet();
			}
		}
		else if (msg == CRCInput::RC_ok)
		{
			if (state == beDefault)
			{
				if (selected < Bouquets->size()) /* Bouquets->size() might be 0 */
				{
					std::string ChannelWidgetCaption = (*Bouquets)[selected]->bName;
#if 0
					if (!(*Bouquets)[selected]->tvChannels.empty())
					{
						ChannelWidgetCaption = ChannelWidgetCaption + " => TV";
						if (!(*Bouquets)[selected]->radioChannels.empty())
							ChannelWidgetCaption = ChannelWidgetCaption + " / Radio";
					}
					else if (!(*Bouquets)[selected]->radioChannels.empty())
					{
						ChannelWidgetCaption = ChannelWidgetCaption + " => Radio";
					}
#endif

					CBEChannelWidget* channelWidget = new CBEChannelWidget(ChannelWidgetCaption, selected);
					channelWidget->exec(this, "");
					selected = channelWidget->getBouquet();
					if (channelWidget->hasChanged())
						bouquetsChanged = true;
					delete channelWidget;
					paintHead();
					paintBody();
					paintFoot();
					paintItems();

					timeoutEnd = CRCInput::calcTimeoutEnd(*timeout_ptr);
				}
			}
			else if (state == beMoving)
			{
				finishMoveBouquet();
			}
		}
		else if (CRCInput::isNumeric(msg))
		{
			if (state == beDefault)
			{
				//kein pushback - wenn man versehentlich wo draufkommt is die edit-arbeit umsonst
				//selected = oldselected;
				//g_RCInput->postMsg(msg, data);
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
			CNeutrinoApp::getInstance()->handleMsg(msg, data);
		}
	}
	hide();
	ResetModules();
	return res;
}

void CBEBouquetWidget::deleteBouquet()
{
	if (selected >= Bouquets->size()) /* Bouquets->size() might be 0 */
		return;

	if (ShowMsg(LOCALE_FILEBROWSER_DELETE, (*Bouquets)[selected]->bName, CMsgBox::mbrNo, CMsgBox::mbYes|CMsgBox::mbNo)!=CMsgBox::mbrYes)
		return;

	g_bouquetManager->deleteBouquet(selected);
	Bouquets = &g_bouquetManager->Bouquets;
	if (selected >= Bouquets->size())
		selected = Bouquets->empty() ? 0 : (Bouquets->size() - 1);
	bouquetsChanged = true;
	paintItems();
}

void CBEBouquetWidget::addBouquet()
{
	std::string newName = inputName("", LOCALE_BOUQUETEDITOR_BOUQUETNAME);
	if (!newName.empty())
	{
		g_bouquetManager->addBouquet(newName, true);
		Bouquets = &g_bouquetManager->Bouquets;
		selected = Bouquets->empty() ? 0 : (Bouquets->size() - 1);
		bouquetsChanged = true;
	}
	paintHead();
	paintBody();
	paintFoot();
	paintItems();
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
	paintItems();
}

void CBEBouquetWidget::cancelMoveBouquet()
{
	state = beDefault;
	internalMoveBouquet(newPosition, origPosition);
	bouquetsChanged = bouquetsChanged | false;
}

void CBEBouquetWidget::internalMoveBouquet(unsigned int fromPosition, unsigned int toPosition)
{
	if ((int) toPosition == -1)
		return;
	if (toPosition == Bouquets->size())
		return;

	g_bouquetManager->moveBouquet(fromPosition, toPosition);
	Bouquets = &g_bouquetManager->Bouquets;
	//bouquetsChanged = true;
	selected = toPosition;
	newPosition = toPosition;
	paintItems();
}

void CBEBouquetWidget::renameBouquet()
{
	if ((*Bouquets)[selected]->bFav)
		return;

	std::string newName = inputName((*Bouquets)[selected]->Name.c_str(), LOCALE_BOUQUETEDITOR_NEWBOUQUETNAME);
	if (newName != (*Bouquets)[selected]->Name)
	{
		g_bouquetManager->Bouquets[selected]->Name = newName;
		g_bouquetManager->Bouquets[selected]->bName = newName;
		g_bouquetManager->Bouquets[selected]->bUser = true;
		bouquetsChanged = true;
	}
	paintHead();
	paintBody();
	paintFoot();
	paintItems();
}

void CBEBouquetWidget::switchHideBouquet()
{
	bouquetsChanged = true;
	(*Bouquets)[selected]->bHidden = !(*Bouquets)[selected]->bHidden;
	paintItems();
}

void CBEBouquetWidget::switchLockBouquet()
{
	bouquetsChanged = true;
	g_bouquetManager->setBouquetLock((*Bouquets)[selected], !(*Bouquets)[selected]->bLocked);
	paintItems();
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
