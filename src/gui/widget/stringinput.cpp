/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2009-2012 Stefan Seyfried

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

#include <gui/widget/stringinput.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <gui/color.h>

#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/msgbox.h>

#include <system/helpers.h>

#include <global.h>
#include <neutrino.h>

CStringInput::CStringInput(const neutrino_locale_t Name, std::string* Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
{
	name = Name;
	init(g_Locale->getText(name), Value, Size, Hint_1, Hint_2, Valid_Chars, Observ, Icon);
}

CStringInput::CStringInput(const std::string &Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
{
	name = NONEXISTANT_LOCALE;
        init(Name, Value, Size, Hint_1, Hint_2, Valid_Chars, Observ, Icon);
}

CStringInput::~CStringInput()
{
	g_RCInput->killTimer (smstimer);
}

void CStringInput::setMinMax(const int min_value, const int max_value)
{
	lower_bound = min_value - 1;
	upper_bound = max_value + 1;
}

#define CStringInputSMSButtonsCount 2
const struct button_label CStringInputSMSButtons[CStringInputSMSButtonsCount] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_STRINGINPUT_CAPS  },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_STRINGINPUT_CLEAR }
};

void CStringInput::init(const std::string &Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
{
	frameBuffer = CFrameBuffer::getInstance();

        head = Name;
        valueString = Value;
	lower_bound = -1;
	upper_bound = -1;
        size =  Size;

        hint_1 = Hint_1;
        hint_2 = Hint_2;
        validchars = Valid_Chars;
        iconfile = Icon ? Icon : NEUTRINO_ICON_EDIT;

        observ = Observ;

#ifdef DEBUG_STRINGINPUT
	printf("HEAD: %s (len: %d)\n", head, strlen(head));
#endif
	hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	iheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getHeight();
	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_FOOT]->getHeight() + 6;	// init min buttonbar height
	input_h = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight() + 2;		// font height + border
	input_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("M") + 2;	// hack font width + border
	offset  = 20;

	width = std::max(size*input_w + 2*offset, (int) frameBuffer->getScreenWidth() / 100 * 45);

	int tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(head);

	if (!(iconfile.empty()))
	{
		int icol_w, icol_h;
		frameBuffer->getIconSize(iconfile.c_str(), &icol_w, &icol_h);
		hheight = std::max(hheight, icol_h + (offset/4));
		tmp_w += icol_w + (offset/2);
	}
	width = std::max(width, tmp_w + offset);

	bheight = input_h + 2*offset;
	if ((hint_1 != NONEXISTANT_LOCALE) || (hint_2 != NONEXISTANT_LOCALE))
	{
		if (hint_1 != NONEXISTANT_LOCALE)
		{
			tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getRenderWidth(g_Locale->getText(hint_1));
			width = std::max(width, tmp_w + 2*offset);
			bheight += iheight;
		}
		if (hint_2 != NONEXISTANT_LOCALE)
		{
			tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getRenderWidth(g_Locale->getText(hint_2));
			width = std::max(width, tmp_w + 2*offset);
			bheight += iheight;
		}
		bheight += offset;
	}

	height = hheight+ bheight; // space for the possible numpad and the buttonbar will be added in initSMS()
	
	x = getScreenStartX(width);
	y = getScreenStartY(height);
	selected = 0;
	smstimer = 0;
	force_saveScreen = false;
	pixBuf = NULL;
}

void CStringInput::NormalKeyPressed(const neutrino_msg_t key)
{
	if (CRCInput::isNumeric(key))
	{	
		std::string tmp_value = *valueString;
		if (selected >= (int)valueString->length())
			valueString->append(selected - valueString->length() + 1, ' ');
		valueString->at(selected) = validchars[CRCInput::getNumericValue(key)];
		int current_value = atoi(*valueString);
		int tmp = current_value;
		if (lower_bound != -1 || upper_bound != -1)
		{
			if (current_value <= lower_bound)
				current_value = lower_bound + 1;
			else if (current_value >= upper_bound)
				current_value = upper_bound - 1;
			if (tmp != current_value)
				*valueString = to_string(current_value).substr(0, size);
		}
		if( (lower_bound == -1 || upper_bound == -1) || (current_value > 0 && current_value > lower_bound && current_value < upper_bound) ){
			if (selected < (size - 1))
			{
				selected++;
				paintChar(selected - 1);
			}
			if (tmp != current_value)
			{
				for (tmp = 0; tmp < size; tmp++)
					paintChar(tmp);
			}
			else
				paintChar(selected);
		}else{
			*valueString = tmp_value;
		}
	}
}

void CStringInput::keyBackspacePressed(void)
{
	if (selected > 0)
	{
		selected--;
		for (int i = selected; i < size - 1; i++)
		{
			valueString->at(i) = valueString->at(i + 1);
			paintChar(i);
		}
		valueString->at(size - 1) = ' ';
		paintChar(size - 1);
	}
}


void CStringInput::keyRedPressed()
{
	if(lower_bound == -1 || upper_bound == -1){
		if (index(validchars, ' ') != NULL)
		{
			valueString->at(selected) = ' ';

			if (selected < (size - 1))
			{
				selected++;
				paintChar(selected - 1);
			}

			paintChar(selected);
		}
	}
}

void CStringInput::keyYellowPressed()
{
	if(lower_bound == -1 || upper_bound == -1){
		selected=0;
		valueString->assign(valueString->length(), ' ');
		for(int i=0 ; i < size ; i++)
			paintChar(i);
	}
}

void CStringInput::keyBluePressed()
{
	if (((valueString->at(selected) | 32) >= 'a') && ((valueString->at(selected) | 32) <= 'z'))
	{
		char newValue = valueString->at(selected) ^ 32;
		if (index(validchars, newValue) != NULL)
		{
			valueString->at(selected) = newValue;
			paintChar(selected);
		}
	}
}

void CStringInput::keyUpPressed()
{
	int npos = 0;
	std::string tmp_value = *valueString;
	for(int count=0;count<(int)strlen(validchars);count++)
		if(valueString->at(selected)==validchars[count])
			npos = count;
	npos++;
	if(npos>=(int)strlen(validchars))
		npos = 0;
	valueString->at(selected)=validchars[npos];

	int current_value = atoi(*valueString);
	int tmp = current_value;
	if (lower_bound != -1 || upper_bound != -1)
	{
		if (current_value <= lower_bound)
			current_value = lower_bound + 1;
		else if (current_value >= upper_bound)
			current_value = upper_bound - 1;
		if (tmp != current_value)
			*valueString = to_string(current_value).substr(0, size);
	}
	if( (lower_bound == -1 || upper_bound == -1) || (current_value > 0 && current_value > lower_bound && current_value < upper_bound) ){
		if (tmp != current_value)
		{
			for (tmp = 0; tmp < size; tmp++)
				paintChar(tmp);
		}
		else
			paintChar(selected);
	}else{
		*valueString = tmp_value;
	}
}

void CStringInput::keyDownPressed()
{
	int npos = 0;
	std::string tmp_value = *valueString;
	const int validchar_len = (int)strlen(validchars);
	for(int count=0;count<validchar_len;count++)
		if(valueString->at(selected)==validchars[count])
			npos = count;
	npos--;
	if(npos<0){
		if(validchar_len > 0)
			npos = validchar_len-1;
		else
			npos = 0;
	}
	valueString->at(selected)=validchars[npos];

	int current_value = atoi(*valueString);
	int tmp = current_value;
	if (lower_bound != -1 || upper_bound != -1)
	{
		if (current_value <= lower_bound)
			current_value = lower_bound + 1;
		else if (current_value >= upper_bound)
			current_value = upper_bound - 1;
		if (tmp != current_value)
			*valueString = to_string(current_value).substr(0, size);
	}
	if( (lower_bound == -1 || upper_bound == -1) || (current_value > 0 && current_value > lower_bound && current_value < upper_bound) ){
		if (tmp != current_value)
		{
			for (tmp = 0; tmp < size; tmp++)
				paintChar(tmp);
		}
		else
			paintChar(selected);
	}else{
		*valueString = tmp_value;
	}
}

void CStringInput::keyLeftPressed()
{
	int old = selected;
	if(selected>0) {
		selected--;
	} else {
		selected = size - 1;
	}
	paintChar(old);
	paintChar(selected);
}

void CStringInput::keyRightPressed()
{
	int old = selected;
	if (selected < (size - 1)) {
		selected++;
	} else
		selected = 0;
	paintChar(old);
	paintChar(selected);
}

void CStringInput::keyMinusPressed()
{
	if(lower_bound == -1 || upper_bound == -1){
		int item = selected;
		while (item < (size -1))
		{
			valueString->at(item) = valueString->at(item+1);
			paintChar(item);
			item++;
		}
		valueString->at(item) = ' ';
		paintChar(item);
	}
}

void CStringInput::keyPlusPressed()
{
	if(lower_bound == -1 || upper_bound == -1){
		int item = size -1;
		while (item > selected)
		{
			valueString->at(item) = valueString->at(item-1);
			paintChar(item);
			item--;
		}
		valueString->at(item) = ' ';
		paintChar(item);
	}
}

void CStringInput::forceSaveScreen(bool enable)
{
	force_saveScreen = enable;
	if (!enable && pixBuf) {
		delete[] pixBuf;
		pixBuf = NULL;
	}
}

int CStringInput::exec( CMenuTarget* parent, const std::string & )
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	int res = menu_return::RETURN_REPAINT;

	std::string oldval = *valueString;
	std::string dispval = *valueString;

	if (parent)
		parent->hide();

	if (size > (int) valueString->length())
		valueString->append(size - valueString->length(), ' ');

	if (pixBuf)
		delete[] pixBuf;
	pixBuf = NULL;
	if (!parent || force_saveScreen) {
		pixBuf = new fb_pixel_t[(width + OFFSET_SHADOW) * (height + OFFSET_SHADOW)];
		if (pixBuf)
			frameBuffer->SaveScreen(x, y, width + OFFSET_SHADOW, height + OFFSET_SHADOW, pixBuf);
	}

	paint();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		frameBuffer->blit();
		if (*valueString != dispval)
		{
			CVFD::getInstance()->showMenuText(1,valueString->c_str() , selected+1);
			dispval = *valueString;
 		}

		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true );

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if ((msg == NeutrinoMessages::EVT_TIMER) && (data == smstimer))
			msg = CRCInput::RC_right;

		if (msg < CRCInput::RC_nokey)
			g_RCInput->killTimer (smstimer);

		if (msg==CRCInput::RC_left)
		{
			keyLeftPressed();
		}
		else if (msg==CRCInput::RC_right)
		{
			keyRightPressed();
		}
		else if (*CRCInput::getUnicodeValue(msg))
		{
			NormalKeyPressed(msg);
		}
		else if (msg==CRCInput::RC_backspace)
		{
			keyBackspacePressed();
		}
		else if (msg==CRCInput::RC_red)
		{
			keyRedPressed();
		}
		else if (msg==CRCInput::RC_yellow)
		{
			keyYellowPressed();
		}
		else if ( (msg==CRCInput::RC_green) && (index(validchars, '.') != NULL))
		{
			valueString->at(selected) = '.';

			if (selected < (size - 1))
			{
				selected++;
				paintChar(selected - 1);
			}

			paintChar(selected);
		}
		else if (msg== CRCInput::RC_blue)
		{
			keyBluePressed();
		}
		else if (msg==CRCInput::RC_up)
		{
			keyUpPressed();
		}
		else if (msg==CRCInput::RC_down)
		{
			keyDownPressed();
		} else if (msg==(neutrino_msg_t)g_settings.key_volumeup)
		{
			keyPlusPressed();
		} else if (msg==(neutrino_msg_t)g_settings.key_volumedown)
		{
			keyMinusPressed();
		}
		else if (msg==CRCInput::RC_ok)
		{
			loop=false;
		}
		else if ( (msg==CRCInput::RC_home) || (msg==CRCInput::RC_timeout) )
		{
			std::string tmp_name = name == NONEXISTANT_LOCALE ? head : g_Locale->getText(name);
			if ((trim (*valueString) != trim(oldval)) &&
			     (ShowMsg(tmp_name, LOCALE_MESSAGEBOX_DISCARD, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbCancel) == CMsgBox::mbrCancel)) {
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
				continue;
			}

			*valueString = oldval;
			loop=false;
			res = menu_return::RETURN_EXIT_REPAINT;
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			// do nothing
		}
		else
		{
			int r = handleOthers( msg, data );
			if (r & (messages_return::cancel_all | messages_return::cancel_info))
			{
				res = (r & messages_return::cancel_all) ? menu_return::RETURN_EXIT_ALL : menu_return::RETURN_EXIT;
				loop = false;
			}
			else if ( r & messages_return::unhandled )
			{
				if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}
		}
	}

	if (pixBuf)
	{
		frameBuffer->RestoreScreen(x, y, width + OFFSET_SHADOW, height + OFFSET_SHADOW, pixBuf);
		delete[] pixBuf;
		pixBuf = NULL;
		frameBuffer->blit();
	} else
		hide();

	*valueString = trim (*valueString);

        if ( (observ) && (msg==CRCInput::RC_ok) )
        {
                observ->changeNotify(name, (void *) valueString->c_str());
        }
	return res;
}

int CStringInput::handleOthers(const neutrino_msg_t /*msg*/, const neutrino_msg_data_t /*data*/)
{
	return messages_return::unhandled;
}

void CStringInput::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width + OFFSET_SHADOW, height + OFFSET_SHADOW);
	frameBuffer->blit();
}

void CStringInput::paint(bool sms)
{
	frameBuffer->paintBoxRel(x + OFFSET_SHADOW, y + OFFSET_SHADOW, width, height, COL_SHADOW_PLUS_0, RADIUS_LARGE, CORNER_ALL); //round
	frameBuffer->paintBoxRel(x, y + hheight, width, bheight, COL_MENUCONTENT_PLUS_0, sms ? 0 : RADIUS_LARGE, CORNER_BOTTOM);

	CComponentsHeader header(x, y, width, hheight, head, iconfile);
	header.paint(CC_SAVE_SCREEN_NO);

	int tmp_y = y+ hheight+ offset+ input_h+ offset;
	if ((hint_1 != NONEXISTANT_LOCALE) || (hint_2 != NONEXISTANT_LOCALE))
	{
		if (hint_1 != NONEXISTANT_LOCALE)
		{
			tmp_y += iheight;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ offset, tmp_y, width- 2*offset, g_Locale->getText(hint_1), COL_MENUCONTENT_TEXT);
		}
		if (hint_2 != NONEXISTANT_LOCALE)
		{
			tmp_y += iheight;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ offset, tmp_y, width- 2*offset, g_Locale->getText(hint_2), COL_MENUCONTENT_TEXT);
		}
		tmp_y += offset;
	}

	int icol_w = 0;
	int icol_h = 0;
	if (sms)
	{
		frameBuffer->getIconSize(NEUTRINO_ICON_NUMERIC_PAD, &icol_w, &icol_h);
		frameBuffer->paintIcon(NEUTRINO_ICON_NUMERIC_PAD, x + (width/2) - (icol_w/2), tmp_y);

		//buttonbar
		::paintButtons(x, y+ hheight+ bheight, width, CStringInputSMSButtonsCount, CStringInputSMSButtons, width, fheight);
		//::paintButtons(x, y + height, width, 2, CStringInputSMSButtons, width);
	}

	for (int count=0;count<size;count++)
		paintChar(count);
}

void CStringInput::paintChar(int pos)
{
	if(pos<(int)valueString->length())
		paintChar(pos, valueString->at(pos));
}

void CStringInput::paintChar(int pos, const char c)
{
	int xpos = x+ offset+ pos* input_w;
	int ypos = y+ hheight+ offset;

	char ch[2] = {c, 0};

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, pos == selected);

	frameBuffer->paintBoxRel(xpos, ypos, input_w, input_h, bgcolor);
	frameBuffer->paintBoxFrame(xpos, ypos, input_w, input_h, 1, COL_MENUCONTENT_PLUS_2);

	int ch_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(ch);
	int ch_x = xpos + std::max(input_w/2 - ch_w/2, 0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(ch_x, ypos+ input_h, ch_w, ch, color);
}

CStringInputSMS::CStringInputSMS(const neutrino_locale_t Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
		: CStringInput(Name, Value, Size, Hint_1, Hint_2, Valid_Chars, Observ, Icon)
{
	initSMS(Valid_Chars);
}

CStringInputSMS::CStringInputSMS(const std::string &Name, std::string *Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
   		: CStringInput(Name, Value, Size, Hint_1, Hint_2, Valid_Chars, Observ, Icon)
{
	initSMS(Valid_Chars);
}

void CStringInputSMS::initSMS(const char * const Valid_Chars)
{
	last_digit = -1;				// no key pressed yet
	const char CharList[10][11] = { "0 -_/()<>=",	// 10 characters
					"1+.,:!?%\\'",  //' for c't search ;)
					"abc2@ä",
					"def3",
					"ghi4",
					"jkl5",
					"mno6ö",
					"pqrs7ß",
					"tuv8ü",
					"wxyz9" };

	for (int i = 0; i < 10; i++)
	{
		int j = 0;
		for (int k = 0; k < (int) strlen(CharList[i]); k++)
			if (strchr(Valid_Chars, CharList[i][k]) != NULL)
				Chars[i][j++] = CharList[i][k];
		if (j == 0)
			Chars[i][j++] = ' ';	// prevent empty char lists
		arraySizes[i] = j;
	}

	int icol_w = 0, icol_h = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_NUMERIC_PAD, &icol_w, &icol_h);
	bheight += icol_h + offset;

	icol_w = icol_h = 0;
	for (int i = 0; i < CStringInputSMSButtonsCount; i++)
	{
		// get max iconsize in buttonbar
		int w = 0, h = 0;
		frameBuffer->getIconSize(CStringInputSMSButtons[i].button, &w, &h);
		icol_w = std::max(icol_w, w);
		icol_h = std::max(icol_h, h);
	}
	fheight = std::max(fheight, icol_h + offset/5);

	height = hheight + bheight + fheight;
	y = getScreenStartY(height);
}

void CStringInputSMS::NormalKeyPressed(const neutrino_msg_t key)
{
	if (CRCInput::isNumeric(key))
	{
		int numericvalue = CRCInput::getNumericValue(key);
		if (last_digit != numericvalue)
		{
			if ((last_digit != -1) &&	// there is a last key
			    (selected < (size- 1)))	// we can shift the cursor one field to the right
			{
				selected++;
				paintChar(selected - 1);
			}
			keyCounter = 0;
		}
		else
			keyCounter = (keyCounter + 1) % arraySizes[numericvalue];
		valueString->at(selected) = Chars[numericvalue][keyCounter];
		last_digit = numericvalue;
		paintChar(selected);
		g_RCInput->killTimer (smstimer);
		smstimer = g_RCInput->addTimer(2*1000*1000);
	}
	else
	{
		valueString->at(selected) = *CRCInput::getUnicodeValue(key);
		keyRedPressed();   /* to lower, paintChar */
		keyRightPressed(); /* last_digit = -1, move to next position */
	}
}

void CStringInputSMS::keyBackspacePressed(void)
{
	last_digit = -1;
	CStringInput::keyBackspacePressed();
}

void CStringInputSMS::keyRedPressed()		// switch between lower & uppercase
{
	if (((valueString->at(selected) | 32) >= 'a') && ((valueString->at(selected) | 32) <= 'z'))
	valueString->at(selected) ^= 32;

	paintChar(selected);
}

void CStringInputSMS::keyYellowPressed()		// clear all
{
	last_digit = -1;
	CStringInput::keyYellowPressed();
}

void CStringInputSMS::keyUpPressed()
{
	last_digit = -1;

	if (selected > 0)
	{
		int lastselected = selected;
		selected = 0;
		paintChar(lastselected);
		paintChar(selected);
	}
}

void CStringInputSMS::keyDownPressed()
{
	last_digit = -1;

	int lastselected = selected;

	selected = size - 1;

	while (valueString->at(selected) == ' ')
	{
		selected--;
		if (selected < 0)
			break;
	}

	if (selected < (size - 1))
		selected++;

	paintChar(lastselected);
	paintChar(selected);
}

void CStringInputSMS::keyLeftPressed()
{
	last_digit = -1;
	CStringInput::keyLeftPressed();
}

void CStringInputSMS::keyRightPressed()
{
	last_digit = -1;
	CStringInput::keyRightPressed();
}

void CStringInputSMS::paint(bool /*unused*/)
{
	CStringInput::paint(true /* sms */);
}

void CPINInput::paintChar(int pos)
{
	CStringInput::paintChar(pos, (valueString->at(pos) == ' ') ? ' ' : '*');
}

int CPINInput::exec( CMenuTarget* parent, const std::string & )
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	if (size > (int) valueString->length())
		valueString->append(size - valueString->length(), ' ');

	paint();

	bool loop = true;

	while(loop)
	{
		frameBuffer->blit();
		g_RCInput->getMsg( &msg, &data, 300 );

		if (msg==CRCInput::RC_left)
		{
			keyLeftPressed();
		}
		else if (msg==CRCInput::RC_right)
		{
			keyRightPressed();
		}
		else if (CRCInput::isNumeric(msg))
		{
			int old_selected = selected;
			NormalKeyPressed(msg);
			if ( old_selected == ( size- 1 ) )
				loop=false;
		}
		else if ( (msg==CRCInput::RC_up) ||
				  (msg==CRCInput::RC_down) )
		{
			g_RCInput->postMsg( msg, data );
			res = menu_return::RETURN_EXIT;
			loop=false;
		}
		else if ( (msg==CRCInput::RC_home) || (msg==CRCInput::RC_timeout) || (msg==CRCInput::RC_ok) )
		{
			loop=false;
		}
		else
		{
			int r = handleOthers(msg, data);
			if (r & (messages_return::cancel_all | messages_return::cancel_info))
			{
				res = (r & messages_return::cancel_all) ? menu_return::RETURN_EXIT_ALL : menu_return::RETURN_EXIT;
				loop = false;
			}
			else if ( r & messages_return::unhandled )
			{
				if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & ( messages_return::cancel_all | messages_return::cancel_info ) )
				{
					loop = false;
					res = menu_return::RETURN_EXIT_ALL;
				}
			}
		}
	}

	hide();

	*valueString = trim (*valueString);

	if ( (observ) && (msg==CRCInput::RC_ok) )
	{
		observ->changeNotify(name, (void *) valueString->c_str());
	}

	return res;
}

void CEnquiryInput::paintChar(int pos)
{
	if (blind)
		CStringInput::paintChar(pos, (valueString->at(pos) == ' ') ? ' ' : '*');
	else
			CStringInput::paintChar(pos, valueString->at(pos));
}

int CPLPINInput::handleOthers(neutrino_msg_t msg, neutrino_msg_data_t data)
{
	int res = messages_return::unhandled;

	if ( msg == NeutrinoMessages::EVT_PROGRAMLOCKSTATUS )
	{
		// trotzdem handlen
		CNeutrinoApp::getInstance()->handleMsg(msg, data);

		if (data != (neutrino_msg_data_t) fsk)
			res = messages_return::cancel_info;
	}

	return res;
}

const char * CPLPINInput::getHint1(void)
{
	if (fsk == 0x100)
	{
		hint_1 = LOCALE_PARENTALLOCK_LOCKEDCHANNEL;
		return g_Locale->getText(hint_1);
	}
	else
	{
		sprintf(hint, g_Locale->getText(LOCALE_PARENTALLOCK_LOCKEDPROGRAM), fsk);
		return hint;
	}
}

#define borderwidth OFFSET_SHADOW // FIXME: do we need border around ??

int CPLPINInput::exec( CMenuTarget* parent, const std::string & )
{
	fb_pixel_t * pixbuf = new fb_pixel_t[(width+ 2* borderwidth) * (height+ 2* borderwidth)];

	if (pixbuf)
		frameBuffer->SaveScreen(x- borderwidth, y- borderwidth, width+ 2* borderwidth, height+ 2* borderwidth, pixbuf);

	// clear border
	frameBuffer->paintBackgroundBoxRel(x- borderwidth, y- borderwidth, width+ 2* borderwidth, borderwidth);
	frameBuffer->paintBackgroundBoxRel(x- borderwidth, y+ height, width+ 2* borderwidth, borderwidth);
	frameBuffer->paintBackgroundBoxRel(x- borderwidth, y, borderwidth, height);
	frameBuffer->paintBackgroundBoxRel(x+ width, y, borderwidth, height);

	int res = CPINInput::exec ( parent, "" );

	if (pixbuf)
	{
		frameBuffer->RestoreScreen(x - borderwidth, y- borderwidth, width+ 2* borderwidth, height+ 2* borderwidth, pixbuf);
		delete[] pixbuf;//Mismatching allocation and deallocation: pixbuf
		pixBuf = NULL;
	}

	return ( res );
}
