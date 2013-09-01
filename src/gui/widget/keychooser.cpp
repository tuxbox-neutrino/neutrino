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

#include <gui/widget/keychooser.h>

#include <global.h>
#include <neutrino.h>

#include <gui/color.h>
#include <driver/screen_max.h>


class CKeyValue : public CMenuSeparator
{
	std::string the_text;
public:
	int         keyvalue;

	CKeyValue(int k) : CMenuSeparator(CMenuSeparator::STRING, LOCALE_KEYCHOOSERMENU_CURRENTKEY)
	{
		keyvalue = k;
	};

	virtual const char * getString(void)
		{
			the_text  = g_Locale->getText(LOCALE_KEYCHOOSERMENU_CURRENTKEY);
			the_text += ": ";
			the_text += CRCInput::getKeyName(keyvalue);
			return the_text.c_str();
		};
};



CKeyChooser::CKeyChooser(int * const Key, const neutrino_locale_t title, const std::string & Icon) : CMenuWidget(title, Icon)
{
	frameBuffer = CFrameBuffer::getInstance();
	key = Key;
	keyName = CRCInput::getKeyName(*key);
	keyChooser = new CKeyChooserItem(LOCALE_KEYCHOOSER_HEAD, key);
	keyDeleter = new CKeyChooserItemNoKey(key);

	addItem(new CKeyValue(*key));
	addItem(GenericMenuSeparatorLine);
	addItem(GenericMenuBack);
	addItem(GenericMenuSeparatorLine);
	addItem(new CMenuForwarder(LOCALE_KEYCHOOSERMENU_SETNEW , true, NULL, keyChooser));
	addItem(new CMenuForwarder(LOCALE_KEYCHOOSERMENU_SETNONE, true, NULL, keyDeleter));
}


CKeyChooser::~CKeyChooser()
{
	delete keyChooser;
	delete keyDeleter;
}


void CKeyChooser::paint()
{
	(((CKeyValue *)(items[0]))->keyvalue) = *key;
	keyName = CRCInput::getKeyName(*key);
	CMenuWidget::paint();
}

//*****************************
CKeyChooserItem::CKeyChooserItem(const neutrino_locale_t Name, int * Key)
{
	name = Name;
	key = Key;
	x = y = width = height = 0;
}


int CKeyChooserItem::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	uint64_t timeoutEnd;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	paint();
	CFrameBuffer::getInstance()->blit();
	g_RCInput->clearRCMsg();

	timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
::TIMING_MENU]);

 get_Message:
	g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

	if (msg != CRCInput::RC_timeout)
	{
//		comparing an unsigned int against >= 0 is senseless!
// 		if ((msg >= 0) && (msg <= CRCInput::RC_MaxRC))
		if ((msg >0 ) && (msg <= CRCInput::RC_MaxRC))
			*key = msg;
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			res = menu_return::RETURN_EXIT_ALL;
		else
			goto get_Message;
	}

	hide();
	return res;
}

void CKeyChooserItem::hide()
{
	CFrameBuffer::getInstance()->paintBackgroundBoxRel(x, y, width, height);
	CFrameBuffer::getInstance()->blit();
}

void CKeyChooserItem::paint()
{
	int hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	int mheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();

	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();

	int tmp;
	width = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(name), true);
	tmp = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_KEYCHOOSER_TEXT1), true);
	if (tmp > width)
		width = tmp;
	tmp = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(LOCALE_KEYCHOOSER_TEXT2), true);
	if (tmp > width)
		width = tmp;
	width += 20;
	width       = w_max(width, 0);
	height      = h_max(hheight + 2 * mheight, 0);
	x           = frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth()-width) >> 1);
	y           = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight()-height) >> 1);

	//frameBuffer->paintBoxRel(x, y          , width, hheight         , COL_MENUHEAD_PLUS_0   );
	//frameBuffer->paintBoxRel(x, y + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0);
	frameBuffer->paintBoxRel(x, y          , width, hheight         , COL_MENUHEAD_PLUS_0   , RADIUS_LARGE, CORNER_TOP);//round
	frameBuffer->paintBoxRel(x, y + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);//round

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+ 10, y+ hheight, width, g_Locale->getText(name), COL_MENUHEAD_TEXT, 0, true); // UTF-8

	//paint msg...
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, y+ hheight+ mheight, width, g_Locale->getText(LOCALE_KEYCHOOSER_TEXT1), COL_MENUCONTENT_TEXT, 0, true); // UTF-8
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, y+ hheight+ mheight* 2, width, g_Locale->getText(LOCALE_KEYCHOOSER_TEXT2), COL_MENUCONTENT_TEXT, 0, true); // UTF-8
}
