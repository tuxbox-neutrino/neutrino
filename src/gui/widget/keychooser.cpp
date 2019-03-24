/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'

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

#include <gui/widget/keychooser.h>

#include <global.h>
#include <neutrino.h>

#include <gui/widget/hintbox.h>

CKeyChooser::CKeyChooser(unsigned int * const Key, const neutrino_locale_t title, const std::string & Icon) : CMenuWidget(title, Icon)
{
	key = Key;
	keyName = CRCInput::getKeyName(*key);

	addIntroItems();
	addItem(new CMenuDForwarder(LOCALE_KEYCHOOSERMENU_SETNEW,  true, NULL, new CKeyChooserItem(key, LOCALE_KEYCHOOSER_HEAD)));
	addItem(new CMenuDForwarder(LOCALE_KEYCHOOSERMENU_SETNONE, true, NULL, new CKeyRemoverItem(key)));
	addItem(GenericMenuSeparatorLine);
	addItem(new CMenuForwarder(LOCALE_KEYCHOOSERMENU_CURRENTKEY, false, keyName));
}

CKeyChooser::~CKeyChooser()
{
}

void CKeyChooser::paint()
{
	keyName = CRCInput::getKeyName(*key);
	CMenuWidget::paint();
}

void CKeyChooser::reinitName()
{
	keyName = CRCInput::getKeyName(*key);
}

int CKeyChooserItem::exec(CMenuTarget* parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	uint64_t timeoutEnd;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	// 10 seconds to choose a new key
	int timeout = 10;

	CHintBox hintbox(name, LOCALE_KEYCHOOSER_TEXT, HINTBOX_MIN_WIDTH, NEUTRINO_ICON_SETTINGS, NEUTRINO_ICON_HINT_KEYS);
	hintbox.setTimeOut(timeout);
	hintbox.paint();

	CFrameBuffer::getInstance()->blit();

	g_RCInput->clearRCMsg();
	g_RCInput->setLongPressAny(true);

	timeoutEnd = CRCInput::calcTimeoutEnd(timeout);

	get_Message:
	hintbox.enableTimeOutBar();
	g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

	if (msg != CRCInput::RC_timeout)
	{
		if ((msg >0 ) && (msg <= CRCInput::RC_MaxRC))
			*key = msg;
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			res = menu_return::RETURN_EXIT_ALL;
		else
			goto get_Message;
	}

	g_RCInput->setLongPressAny(false);
	hintbox.disableTimeOutBar();
	hintbox.hide();
	return res;
}

int CKeyRemoverItem::exec(CMenuTarget* /*parent*/, const std::string &)
{
	*key = (unsigned int)CRCInput::RC_nokey;
	return menu_return::RETURN_REPAINT;
}
