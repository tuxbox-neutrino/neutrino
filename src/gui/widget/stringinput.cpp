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
#include <gui/widget/messagebox.h>

#include <global.h>
#include <neutrino.h>

CStringInput::CStringInput(const neutrino_locale_t Name, char* Value, const int min_value, const int max_value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
{
	name =  Name;
	head = NULL;
	value = Value;
	valueString = NULL;
	lower_bound = min_value - 1;
	upper_bound = max_value + 1;
	size =  Size;

	hint_1 = Hint_1;
	hint_2 = Hint_2;
	validchars = Valid_Chars;
	iconfile = Icon ? Icon : "";

	observ = Observ;
	init();
}

CStringInput::CStringInput(const neutrino_locale_t Name, char* Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
{
	name =  Name;
	head = NULL;
	value = Value;
	valueString = NULL;
	lower_bound = -1;
	upper_bound = -1;

	size =  Size;

	hint_1 = Hint_1;
	hint_2 = Hint_2;
	validchars = Valid_Chars;
	iconfile = Icon ? Icon : "";

	observ = Observ;
	init();
}

CStringInput::CStringInput(const neutrino_locale_t Name, std::string* Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
{
        name =  Name;
	head = NULL;
        value = new char[Size+1];
        value[Size] = '\0';
        strncpy(value,Value->c_str(),Size);
        valueString = Value;
	lower_bound = -1;
	upper_bound = -1;
        size = Size;

        hint_1 = Hint_1;
        hint_2 = Hint_2;
        validchars = Valid_Chars;
        iconfile = Icon ? Icon : "";

        observ = Observ;
        init();
}

CStringInput::CStringInput(char * Head, char* Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
{
        head = strdup(Head);
        value = Value;
        valueString = NULL;
	lower_bound = -1;
	upper_bound = -1;
        size =  Size;

        hint_1 = Hint_1;
        hint_2 = Hint_2;
        validchars = Valid_Chars;
        iconfile = Icon ? Icon : "";

        observ = Observ;
        init();
}

CStringInput::~CStringInput()
{
	if (valueString != NULL)
	{
		delete[] value;
	}
	if(head) {
		free(head);
	}
}

#define CStringInputSMSButtonsCount 2
const struct button_label CStringInputSMSButtons[CStringInputSMSButtonsCount] =
{
	{ NEUTRINO_ICON_BUTTON_RED   , LOCALE_STRINGINPUT_CAPS  },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_STRINGINPUT_CLEAR }
};

void CStringInput::init()
{
	frameBuffer = CFrameBuffer::getInstance();

#ifdef DEBUG_STRINGINPUT
	printf("HEAD: %s (len: %d)\n", head, strlen(head));
#endif
	hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	iheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getHeight();
	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight() + 6;	// init min buttonbar height
	input_h = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight() + 2;		// font height + border
	input_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("M") + 2;	// hack font width + border
	offset  = 20;

	width = std::max(size*input_w + 2*offset, (int) frameBuffer->getScreenWidth() / 100 * 45);

	int tmp_w = 0;
	if (head)
		tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(head, true); // UTF-8
	else
		tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(name), true); // UTF-8

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
			tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getRenderWidth(g_Locale->getText(hint_1), true);
			width = std::max(width, tmp_w + 2*offset);
			bheight += iheight;
		}
		if (hint_2 != NONEXISTANT_LOCALE)
		{
			tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getRenderWidth(g_Locale->getText(hint_2), true);
			width = std::max(width, tmp_w + 2*offset);
			bheight += iheight;
		}
		bheight += offset;
	}

	height = hheight+ bheight; // space for the possible numpad and the buttonbar will be added in initSMS()
	
	x = getScreenStartX(width);
	y = getScreenStartY(height);
	selected = 0;
}

void CStringInput::NormalKeyPressed(const neutrino_msg_t key)
{
	if (CRCInput::isNumeric(key))
	{	
		std::string tmp_value = value;
		value[selected] = validchars[CRCInput::getNumericValue(key)];
		int current_value = atoi(value);
		int tmp = current_value;
		if (lower_bound != -1 || upper_bound != -1)
		{
			if (current_value <= lower_bound)
				current_value = lower_bound + 1;
			else if (current_value >= upper_bound)
				current_value = upper_bound - 1;
			if (tmp != current_value) /* size + 1 is correct, snprintf includes always '\0' */
				snprintf(value, size + 1, "%*d", size, current_value);
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
			snprintf(value, size + 1, "%s", tmp_value.c_str());
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
			value[i] = value[i + 1];
			paintChar(i);
		}
		value[size - 1] = ' ';
		paintChar(size - 1);
	}
}


void CStringInput::keyRedPressed()
{
	if(lower_bound == -1 || upper_bound == -1){
		if (index(validchars, ' ') != NULL)
		{
			value[selected] = ' ';

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
		for(int i=0 ; i < size ; i++)
		{
			value[i]=' ';
			paintChar(i);
		}
	}
}

void CStringInput::keyBluePressed()
{
	if (((value[selected] | 32) >= 'a') && ((value[selected] | 32) <= 'z'))
	{
		char newValue = value[selected] ^ 32;
		if (index(validchars, newValue) != NULL)
		{
			value[selected] = newValue;
			paintChar(selected);
		}
	}
}

void CStringInput::keyUpPressed()
{
	int npos = 0;
	std::string tmp_value = value;
	for(int count=0;count<(int)strlen(validchars);count++)
		if(value[selected]==validchars[count])
			npos = count;
	npos++;
	if(npos>=(int)strlen(validchars))
		npos = 0;
	value[selected]=validchars[npos];

	int current_value = atoi(value);
	int tmp = current_value;
	if (lower_bound != -1 || upper_bound != -1)
	{
		if (current_value <= lower_bound)
			current_value = lower_bound + 1;
		else if (current_value >= upper_bound)
			current_value = upper_bound - 1;
		if (tmp != current_value) /* size + 1 is correct, snprintf includes always '\0' */
			snprintf(value, size + 1, "%*d", size, current_value);
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
		snprintf(value, size + 1, "%s", tmp_value.c_str());
	}
}

void CStringInput::keyDownPressed()
{
	int npos = 0;
	std::string tmp_value = value;
	for(int count=0;count<(int)strlen(validchars);count++)
		if(value[selected]==validchars[count])
			npos = count;
	npos--;
	if(npos<0)
		npos = strlen(validchars)-1;
	value[selected]=validchars[npos];

	int current_value = atoi(value);
	int tmp = current_value;
	if (lower_bound != -1 || upper_bound != -1)
	{
		if (current_value <= lower_bound)
			current_value = lower_bound + 1;
		else if (current_value >= upper_bound)
			current_value = upper_bound - 1;
		if (tmp != current_value) /* size + 1 is correct, snprintf includes always '\0' */
			snprintf(value, size + 1, "%*d", size, current_value);
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
		snprintf(value, size + 1, "%s", tmp_value.c_str());
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
			value[item] = value[item+1];
			paintChar(item);
			item++;
		}
		value[item] = ' ';
		paintChar(item);
	}
}

void CStringInput::keyPlusPressed()
{
	if(lower_bound == -1 || upper_bound == -1){
		int item = size -1;
		while (item > selected)
		{
			value[item] = value[item-1];
			paintChar(item);
			item--;
		}
		value[item] = ' ';
		paintChar(item);
	}
}

int CStringInput::exec( CMenuTarget* parent, const std::string & )
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	int res = menu_return::RETURN_REPAINT;

        char *oldval = new char[size+1];
	if(oldval == NULL)
		return res;
	char  *dispval = new char[size+1];
	if(dispval == NULL){
		delete[] oldval;
		return res;
	}
        oldval[size] = 0;
	memset(dispval, 0, size + 1);

	if (parent)
		parent->hide();

	for(int count=strlen(value)-1;count<size-1;count++)
		strcat(value, " ");
	strncpy(oldval, value, size);

	fb_pixel_t * pixbuf = new fb_pixel_t[(width + SHADOW_OFFSET) * (hheight + bheight + SHADOW_OFFSET)];

	if (pixbuf != NULL)
		frameBuffer->SaveScreen(x, y, width + SHADOW_OFFSET, hheight + bheight + SHADOW_OFFSET, pixbuf);

	paint();
	frameBuffer->blit();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		if ( strncmp(value, dispval, size) != 0)
		{
			std::string tmp = value;
			CVFD::getInstance()->showMenuText(1,tmp.c_str() , selected+1);
			strncpy(dispval, value, size);
 		}

		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true );

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if (msg==CRCInput::RC_left)
		{
			keyLeftPressed();
		}
		else if (msg==CRCInput::RC_right)
		{
			keyRightPressed();
		}
		else if (CRCInput::getUnicodeValue(msg) != -1)
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
			value[selected] = '.';

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
		} else if (msg==CRCInput::RC_plus)
		{
			keyPlusPressed();
		} else if (msg==CRCInput::RC_minus)
		{
			keyMinusPressed();
		}
		else if (msg==CRCInput::RC_ok)
		{
			loop=false;
		}
		else if ( (msg==CRCInput::RC_home) || (msg==CRCInput::RC_timeout) )
		{
			if ( ( strcmp(value, oldval) != 0) &&
			     (ShowLocalizedMessage(name, LOCALE_MESSAGEBOX_DISCARD, CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbCancel) == CMessageBox::mbrCancel))
				continue;

			strncpy(value, oldval, size);
			loop=false;
			res = menu_return::RETURN_EXIT_REPAINT;
		}
		else if ((msg ==CRCInput::RC_sat) || (msg == CRCInput::RC_favorites))
		{
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
		frameBuffer->blit();
	}

	hide();

	if (pixbuf != NULL)
	{
		frameBuffer->RestoreScreen(x, y, width + SHADOW_OFFSET, hheight + bheight + SHADOW_OFFSET, pixbuf);
		delete[] pixbuf;//Mismatching allocation and deallocation: pixbuf
	}

	for(int count=size-1;count>=0;count--)
	{
		if((value[count]==' ') || (value[count]==0))
		{
			value[count]=0;
		}
		else
			break;
	}
	value[size]=0;

	if (valueString != NULL)
        {
                *valueString = value;
        }

        if ( (observ) && (msg==CRCInput::RC_ok) )
        {
                observ->changeNotify(name, value);
        }
	delete[] dispval;
	delete[] oldval;
	return res;
}

int CStringInput::handleOthers(const neutrino_msg_t /*msg*/, const neutrino_msg_data_t /*data*/)
{
	return messages_return::unhandled;
}

void CStringInput::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width + SHADOW_OFFSET, height + SHADOW_OFFSET);
	frameBuffer->blit();
}

void CStringInput::paint(bool sms)
{
	frameBuffer->paintBoxRel(x + SHADOW_OFFSET, y + SHADOW_OFFSET, width, hheight + bheight, COL_MENUCONTENTDARK_PLUS_0, RADIUS_LARGE, CORNER_ALL); //round
	frameBuffer->paintBoxRel(x, y, width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP); //round
	frameBuffer->paintBoxRel(x, y + hheight, width, bheight, COL_MENUCONTENT_PLUS_0, sms ? 0 : RADIUS_LARGE, CORNER_BOTTOM);

	int icol_w = 0, icol_h = 0, icol_o = 0;
	if (!(iconfile.empty()))
	{
		frameBuffer->getIconSize(iconfile.c_str(), &icol_w, &icol_h);
		frameBuffer->paintIcon(iconfile, x + (offset/2), y, hheight);
		icol_o = icol_w + (offset/2);
	}

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+ (offset/2)+ icol_o, y+ hheight, width- offset- icol_o, head ? head : g_Locale->getText(name), COL_MENUHEAD, 0, true); // UTF-8

	int tmp_y = y+ hheight+ offset+ input_h+ offset;
	if ((hint_1 != NONEXISTANT_LOCALE) || (hint_2 != NONEXISTANT_LOCALE))
	{
		if (hint_1 != NONEXISTANT_LOCALE)
		{
			tmp_y += iheight;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ offset, tmp_y, width- 2*offset, g_Locale->getText(hint_1), COL_MENUCONTENT, 0, true); // UTF-8
		}
		if (hint_2 != NONEXISTANT_LOCALE)
		{
			tmp_y += iheight;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ offset, tmp_y, width- 2*offset, g_Locale->getText(hint_2), COL_MENUCONTENT, 0, true); // UTF-8
		}
		tmp_y += offset;
	}

	icol_w = icol_h = 0;
	if (sms)
	{
		frameBuffer->getIconSize(NEUTRINO_ICON_NUMERIC_PAD, &icol_w, &icol_h);
		frameBuffer->paintIcon(NEUTRINO_ICON_NUMERIC_PAD, x + (width/2) - (icol_w/2), tmp_y, 0, COL_MENUCONTENT);

		//buttonbar
		::paintButtons(x, y+ hheight+ bheight, width, CStringInputSMSButtonsCount, CStringInputSMSButtons, width, fheight);
		//::paintButtons(x, y + height, width, 2, CStringInputSMSButtons, width);
	}

	for (int count=0;count<size;count++)
		paintChar(count);
}

void CStringInput::paintChar(int pos)
{
	if(pos<(int)strlen(value))
		paintChar(pos, value[pos]);
}

void CStringInput::paintChar(int pos, const char c)
{
	int xpos = x+ offset+ pos* input_w;
	int ypos = y+ hheight+ offset;

	char ch[2] = {c, 0};

	uint8_t    color;
	fb_pixel_t bgcolor;

	if (pos == selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else
	{
		color   = COL_MENUCONTENT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	frameBuffer->paintBoxRel(xpos, ypos, input_w, input_h, COL_MENUCONTENT_PLUS_2);
	frameBuffer->paintBoxRel(xpos+ 1, ypos+ 1, input_w- 2, input_h- 2, bgcolor);

	int ch_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(ch);
	int ch_x = xpos + std::max(input_w/2 - ch_w/2, 0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(ch_x, ypos+ input_h, ch_w, ch, color);
}

CStringInputSMS::CStringInputSMS(const neutrino_locale_t Name, std::string* Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
		: CStringInput(Name, Value, Size, Hint_1, Hint_2, Valid_Chars, Observ, Icon)
{
	initSMS(Valid_Chars);
}

CStringInputSMS::CStringInputSMS(const neutrino_locale_t Name, char* Value, int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, const char * const Valid_Chars, CChangeObserver* Observ, const char * const Icon)
   		: CStringInput(Name, Value, Size, Hint_1, Hint_2, Valid_Chars, Observ, Icon)
{
	initSMS(Valid_Chars);
}

void CStringInputSMS::initSMS(const char * const Valid_Chars)
{
	last_digit = -1;				// no key pressed yet
	const char CharList[10][11] = { "0 -_/()<>=",	// 10 characters
					"1+.,:!?\\'",//' for c't search ;)
					"abc2ä",
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
		value[selected] = Chars[numericvalue][keyCounter];
		last_digit = numericvalue;
		paintChar(selected);
	}
	else
	{
		value[selected] = (char)CRCInput::getUnicodeValue(key);
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
	if (((value[selected] | 32) >= 'a') && ((value[selected] | 32) <= 'z'))
	value[selected] ^= 32;

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

	while (value[selected] == ' ')
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
	CStringInput::paintChar(pos, (value[pos] == ' ') ? ' ' : '*');
}

int CPINInput::exec( CMenuTarget* parent, const std::string & )
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	for(int count=strlen(value)-1;count<size-1;count++)
		strcat(value, " ");

	paint();
	frameBuffer->blit();

	bool loop = true;

	while(loop)
	{
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
		frameBuffer->blit();
	}

	hide();

	for(int count=size-1;count>=0;count--)
	{
		if((value[count]==' ') || (value[count]==0))
		{
			value[count]=0;
		}
		else
			break;
	}
	value[size]=0;

	if ( (observ) && (msg==CRCInput::RC_ok) )
	{
		observ->changeNotify(name, value);
	}

	return res;
}

void CEnquiryInput::paintChar(int pos)
{
	if (blind)
		CStringInput::paintChar(pos, (value[pos] == ' ') ? ' ' : '*');
	else
			CStringInput::paintChar(pos, value[pos]);
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

#define borderwidth 4

int CPLPINInput::exec( CMenuTarget* parent, const std::string & )
{
	fb_pixel_t * pixbuf = new fb_pixel_t[(width+ 2* borderwidth) * (height+ 2* borderwidth)];

	if (pixbuf != NULL)
		frameBuffer->SaveScreen(x- borderwidth, y- borderwidth, width+ 2* borderwidth, height+ 2* borderwidth, pixbuf);

	// clear border
	frameBuffer->paintBackgroundBoxRel(x- borderwidth, y- borderwidth, width+ 2* borderwidth, borderwidth);
	frameBuffer->paintBackgroundBoxRel(x- borderwidth, y+ height, width+ 2* borderwidth, borderwidth);
	frameBuffer->paintBackgroundBoxRel(x- borderwidth, y, borderwidth, height);
	frameBuffer->paintBackgroundBoxRel(x+ width, y, borderwidth, height);

	int res = CPINInput::exec ( parent, "" );

	if (pixbuf != NULL)
	{
		frameBuffer->RestoreScreen(x- borderwidth, y- borderwidth, width+ 2* borderwidth, height+ 2* borderwidth, pixbuf);
		delete[] pixbuf;//Mismatching allocation and deallocation: pixbuf
	}

	return ( res );
}
