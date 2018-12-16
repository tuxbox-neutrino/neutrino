/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/


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

#include <gui/widget/stringinput_ext.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <driver/display.h>

#include <gui/color.h>

#include <gui/widget/msgbox.h>

#include <system/helpers.h>
#include <global.h>
#include <neutrino.h>


CExtendedInput::CExtendedInput(const neutrino_locale_t Name, std::string *Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ, bool* Cancel)
{
	name = Name;
	valueString = Value;
	cancel = Cancel;

	hint_1 = Hint_1;
	hint_2 = Hint_2;

	observ = Observ;
	Init();
}

void CExtendedInput::Init(void)
{
	frameBuffer = CFrameBuffer::getInstance();

	hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	iheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getHeight();
	input_h = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight() + 2;		// font height + border
	input_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("M") + 2;	// hack font width + border
	offset  = 20;

	width = frameBuffer->getScreenWidth() / 100 * 45;

	int tmp_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(name)); // UTF-8
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

	height = hheight+ bheight;

	x = getScreenStartX(width);
	y = getScreenStartY(height);

	savescreen	= false;
	background	= NULL;
}

CExtendedInput::~CExtendedInput()
{
	for (std::vector<CExtendedInput_Item*>::iterator it = inputFields.begin(); it < inputFields.end(); ++it)
		delete *it;
}

void CExtendedInput::addInputField( CExtendedInput_Item* fld )
{
	inputFields.push_back(fld);
}


void CExtendedInput::calculateDialog()
{
	int ix = 0;
	int iy = 0;
	int maxX = 0;
	int maxY = 0;

	if (valueString->size() < inputFields.size())
		valueString->append(inputFields.size() - valueString->size(), ' ');

	selectedChar = -1;
	for(unsigned int i=0; i<inputFields.size();i++)
	{
		inputFields[i]->init( ix, iy);
		inputFields[i]->setDataPointer( &valueString->at(i) );
		if ((selectedChar==-1) && (inputFields[i]->isSelectable()))
		{
			selectedChar = i;
		}
		maxX = std::max(ix, maxX);
		maxY = std::max(iy, maxY);
	}

	width = std::max(width, maxX + 2*offset);
	height = std::max(height, maxY + bheight);

	x = getScreenStartX(width);
	y = getScreenStartY(height);
}

void CExtendedInput::saveScreen()
{
	if(!savescreen)
		return;

	delete[] background;

	background = new fb_pixel_t [width * height];
	if(background)
		frameBuffer->SaveScreen(x, y, width, height, background);
}

void CExtendedInput::restoreScreen()
{
	if(background) {
		if(savescreen)
			frameBuffer->RestoreScreen(x, y, width, height, background);
	}
}

void CExtendedInput::enableSaveScreen(bool enable)
{
	savescreen = enable;
	if (!enable && background) {
		delete[] background;
		background = NULL;
	}
}

int CExtendedInput::exec( CMenuTarget* parent, const std::string & )
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	onBeforeExec();
	calculateDialog();
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	std::string oldval = *valueString;
	std::string dispval = *valueString;
	if (savescreen)
		saveScreen();
	paint();
	frameBuffer->blit();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		if (*valueString != dispval)
		{
			CVFD::getInstance()->showMenuText(1, valueString->c_str(), selectedChar+1);
			dispval = *valueString;
		}

		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true );

		if (msg==CRCInput::RC_left) {
			bool found = false;
			int oldSelectedChar = selectedChar;
			if(selectedChar > 0) {
				for(int i=selectedChar-1; i>=0;i--) {
					if (inputFields[i]->isSelectable()) {
						found = true;
						selectedChar = i;
						break;
					}
				}
			} else {
				for(int i = inputFields.size() - 1; i >= 0; i--) {
					if(inputFields[i]->isSelectable()) {
						found = true;
						selectedChar = i;
						break;
					}
				}
			}
			if(found) {
				inputFields[oldSelectedChar]->paint(x+ offset, y+ hheight+ offset, false );
				inputFields[selectedChar]->paint(x+ offset, y+ hheight+ offset, true );
				CVFD::getInstance()->showMenuText(1, valueString->c_str(), selectedChar+1);
			}
		} else if (msg==CRCInput::RC_right) {
			bool found = false;
			int oldSelectedChar = selectedChar;
			if(selectedChar < (int) inputFields.size()-1) {
				for(unsigned int i = selectedChar+1; i < inputFields.size();i++) {
					if (inputFields[i]->isSelectable()) {
						found = true;
						selectedChar = i;
						break;
					}
				}
			}
			if(!found) {
				for(int i = 0; i < (int) inputFields.size(); i++) {
//printf("old %d sel %d size %d i %d\n", oldSelectedChar, selectedChar, inputFields.size(), i);
					if(inputFields[i]->isSelectable()) {
						found = true;
						selectedChar = i;
						break;
					}
				}
			}
			if(found) {
				inputFields[oldSelectedChar]->paint(x+ offset, y+ hheight+ offset, false );
				inputFields[selectedChar]->paint(x+ offset, y+ hheight+ offset, true );
				CVFD::getInstance()->showMenuText(1, valueString->c_str(), selectedChar+1);
			}
		}
		else if ( (*CRCInput::getUnicodeValue(msg)) || (msg == CRCInput::RC_red) || (msg == CRCInput::RC_green) || (msg == CRCInput::RC_blue) || (msg == CRCInput::RC_yellow)
					|| (msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
		{
			inputFields[selectedChar]->keyPressed(msg);
			inputFields[selectedChar]->paint(x+ offset, y+ hheight+ offset, true );
		}
		else if (msg==CRCInput::RC_ok)
		{
			loop=false;
			if(cancel != NULL)
				*cancel = false;
		}
		else if ( (msg==CRCInput::RC_home) || (msg==CRCInput::RC_timeout) )
		{
			if (trim (*valueString) != trim(oldval)){
				int erg = ShowMsg(name, LOCALE_MESSAGEBOX_DISCARD, CMsgBox::mbrYes, CMsgBox::mbNo | CMsgBox::mbYes | CMsgBox::mbCancel);
				 if(erg==CMsgBox::mbrYes){
					*valueString = oldval;
					loop=false;
					if(cancel != NULL)
						*cancel = true;
				 }
				 else if(erg==CMsgBox::mbrNo){
					 loop=false;
					 if(cancel != NULL)
						 *cancel = false;
				 }
				 else if(erg==CMsgBox::mbrCancel){
					 timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
				 }
			} else {
				loop=false;
				if(cancel != NULL)
					*cancel = true;
			}
		}
		else if (CNeutrinoApp::getInstance()->listModeKey(msg))
		{
			// do nothing
		}
		else if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
		{
			*valueString = oldval;
			loop=false;
			if(cancel != NULL)
				*cancel = true;
			res = menu_return::RETURN_EXIT_ALL;
		}
		frameBuffer->blit();
	}

	hide();

	*valueString = trim(*valueString);

	onAfterExec();

	if ((observ) && (msg == CRCInput::RC_ok))
		observ->changeNotify(name, (void *)valueString->c_str());

	return res;
}

void CExtendedInput::hide()
{
	if (savescreen && background)
		restoreScreen();
	else
		frameBuffer->paintBackgroundBoxRel(x, y, width, height);
	frameBuffer->blit();
}

void CExtendedInput::paint()
{
	CComponentsHeader header(x, y, width, hheight, g_Locale->getText(name), NEUTRINO_ICON_EDIT);
	header.paint(CC_SAVE_SCREEN_NO);

	frameBuffer->paintBoxRel(x, y + hheight, width, bheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

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
	}

	for(unsigned int i=0; i<inputFields.size();i++)
		inputFields[i]->paint(x+ offset, y+ hheight+ offset, (i== (unsigned int) selectedChar) );
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


CExtendedInput_Item_Char::CExtendedInput_Item_Char(const std::string & Chars, bool Selectable )
{
	frameBuffer = CFrameBuffer::getInstance();
	input_h = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight() + 2;		// font height + border
	input_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("M") + 2;	// hack font width + border
	allowedChars = Chars;
	selectable = Selectable;
}

void CExtendedInput_Item_Char::init(int &x, int &y)
{
	ix = x;
	iy = y;
	x += input_w;
}

void CExtendedInput_Item_Char::setAllowedChars( const std::string & ac )
{
	allowedChars = ac;
}

void CExtendedInput_Item_Char::paint(int x, int y, bool focusGained )
{
	int xpos = ix + x;
	int ypos = iy + y;

	char ch[2] = {*data, 0};

	fb_pixel_t color;
	fb_pixel_t bgcolor;

	getItemColors(color, bgcolor, focusGained);

	frameBuffer->paintBoxRel(xpos, ypos, input_w, input_h, bgcolor);
	frameBuffer->paintBoxFrame(xpos, ypos, input_w, input_h, 1, COL_MENUCONTENT_PLUS_2);

	int ch_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(ch);
	int ch_x = xpos + std::max(input_w/2 - ch_w/2, 0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(ch_x, ypos+ input_h, ch_w, ch, color);
}

bool CExtendedInput_Item_Char::isAllowedChar( char ch )
{
	return ( (int) allowedChars.find(ch) != -1);
}

int CExtendedInput_Item_Char::getCharID( char ch )
{
	return allowedChars.find(ch);
}

void CExtendedInput_Item_Char::keyPressed(const int key)
{
	const char *value = CRCInput::getUnicodeValue(key);
	if (*value)
	{
		if (isAllowedChar(*value))
		{
			*data = *value;
			g_RCInput->postMsg( CRCInput::RC_right, 0 );
		}
	}
	else
	{
		unsigned int pos = getCharID( *data );
		if (key==CRCInput::RC_up)
		{
			if(pos<allowedChars.size()-1)
			{
				*data = allowedChars[pos+1];
			}
			else
			{
				*data = allowedChars[0];
			}
		}
		else if (key==CRCInput::RC_down)
		{
			if(pos>0)
			{
				*data = allowedChars[pos-1];
			}
			else
			{
				*data = allowedChars[allowedChars.size()-1];
			}
		}
	}
}

//-----------------------------#################################-------------------------------------------------------

CIPInput::CIPInput(const neutrino_locale_t Name, std::string *Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ) : CExtendedInput(Name, Value, Hint_1, Hint_2, Observ)
{
	addInputField( new CExtendedInput_Item_Char("012") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("012") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("012") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("012") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_newLiner(30) );
}

void CIPInput::onBeforeExec()
{
	if (valueString->empty())
	{
		*valueString = "000.000.000.000";
		return;
	}
	unsigned char ip[4];
	sscanf(valueString->c_str(), "%hhu.%hhu.%hhu.%hhu", &ip[0], &ip[1], &ip[2], &ip[3]);
	char s[20];
	snprintf(s, sizeof(s), "%03hhu.%03hhu.%03hhu.%03hhu", ip[0], ip[1], ip[2], ip[3]);
	*valueString = std::string(s);
}

void CIPInput::onAfterExec()
{
	int ip[4];
	sscanf(valueString->c_str(), "%3d.%3d.%3d.%3d", &ip[0], &ip[1], &ip[2], &ip[3] );
	char s[20];
	snprintf(s, sizeof(s), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	*valueString = std::string(s);
	if(*valueString == "0.0.0.0")
		*valueString = "";
}

//-----------------------------#################################-------------------------------------------------------
CDateInput::CDateInput(const neutrino_locale_t Name, time_t* Time, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CExtendedInput(Name, &valueStringTmp, Hint_1, Hint_2, Observ)
{
	time=Time;
	char value[40];
	struct tm *tmTime = localtime(time);
	snprintf(value, sizeof(value), "%02d.%02d.%04d %02d:%02d", tmTime->tm_mday, tmTime->tm_mon+1,
				tmTime->tm_year+1900,
				tmTime->tm_hour, tmTime->tm_min);
	*valueString = std::string(value);

	addInputField( new CExtendedInput_Item_Char("0123") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char(".",false) );
	addInputField( new CExtendedInput_Item_Char("01") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char(".",false) );
	addInputField( new CExtendedInput_Item_Char("2",false) );
	addInputField( new CExtendedInput_Item_Char("0",false) );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("012") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char(":",false) );
	addInputField( new CExtendedInput_Item_Char("012345") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_newLiner(30) );
}

void CDateInput::onBeforeExec()
{
	char value[40];
	struct tm *tmTime = localtime(time);
	snprintf(value, sizeof(value), "%02d.%02d.%04d %02d:%02d", tmTime->tm_mday, tmTime->tm_mon+1,
				tmTime->tm_year+1900,
				tmTime->tm_hour, tmTime->tm_min);
	*valueString = std::string(value);
}

void CDateInput::onAfterExec()
{
	struct tm tmTime;
	sscanf(valueString->c_str(), "%02d.%02d.%04d %02d:%02d", &tmTime.tm_mday, &tmTime.tm_mon,
			&tmTime.tm_year,
			&tmTime.tm_hour, &tmTime.tm_min);
	tmTime.tm_mon -= 1;
	tmTime.tm_year -= 1900;
	tmTime.tm_sec = 0;

	tmTime.tm_isdst = -1;

	if(tmTime.tm_year>129)
		tmTime.tm_year=129;
	if(tmTime.tm_year<0)
		tmTime.tm_year=0;
	if(tmTime.tm_mon>11)
		tmTime.tm_mon=11;
	if(tmTime.tm_mon<0)
		tmTime.tm_mon=0;
	if(tmTime.tm_mday>31) //-> eine etwas laxe pruefung, aber mktime biegt das wieder grade
		tmTime.tm_mday=31;
	if(tmTime.tm_mday<1)
		tmTime.tm_mday=1;
	if(tmTime.tm_hour>23)
		tmTime.tm_hour=23;
	if(tmTime.tm_hour<0)
		tmTime.tm_hour=0;
	if(tmTime.tm_min>59)
		tmTime.tm_min=59;
	if(tmTime.tm_min<0)
		tmTime.tm_min=0;
	if(tmTime.tm_sec>59)
		tmTime.tm_sec=59;
	if(tmTime.tm_sec<0)
		tmTime.tm_sec=0;

	*time=mktime(&tmTime);
	char value[40];
	struct tm *tmTime2 = localtime(time);
	snprintf(value, sizeof(value), "%02d.%02d.%04d %02d:%02d", tmTime2->tm_mday, tmTime2->tm_mon+1,
				tmTime2->tm_year+1900,
				tmTime2->tm_hour, tmTime2->tm_min);
	*valueString = std::string(value);
}
//-----------------------------#################################-------------------------------------------------------

CMACInput::CMACInput(const neutrino_locale_t Name, std::string * Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ) : CExtendedInput(Name, Value, Hint_1, Hint_2, Observ)
{
	frameBuffer = CFrameBuffer::getInstance();
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_Char("0123456789ABCDEF") );
	addInputField( new CExtendedInput_Item_newLiner(30) );
}

void CMACInput::onBeforeExec()
{
	if (valueString->empty())
	{
		*valueString = "00:00:00:00:00:00";
		return;
	}
	int mac[6];
	sscanf(valueString->c_str(), "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] );
	char s[20];
	snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	*valueString = std::string(s);
}

void CMACInput::onAfterExec()
{
	int mac[6];
	sscanf(valueString->c_str(), "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] );
	char s[20];
	snprintf(s, sizeof(s), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	*valueString = std::string(s);
	if(*valueString == "00:00:00:00:00:00")
		*valueString = "";
}

//-----------------------------#################################-------------------------------------------------------

CTimeInput::CTimeInput(const neutrino_locale_t Name, std::string* Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ, bool* Cancel)
	: CExtendedInput(Name, &valueStringTmp, Hint_1, Hint_2, Observ, Cancel)
{
	valueString = Value;
	frameBuffer = CFrameBuffer::getInstance();
#if 1
	// As nobody else seems to use this class I feel free to make some minor (and mostly backwards-compatible)
	// adjustments to make it suitable for movieplayer playtime selection ... --martii

	const char *v = valueString->c_str();
	if (!isdigit(*v)) {
		addInputField( new CExtendedInput_Item_Char("=+-") );
		addInputField( new CExtendedInput_Item_Spacer(20) );
	}
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	v = strstr(v, ":");
	if (v) {
		v++;
		addInputField( new CExtendedInput_Item_Char(":",false) );
		addInputField( new CExtendedInput_Item_Char("0123456789") );
		addInputField( new CExtendedInput_Item_Char("0123456789") );
		v = strstr(v, ":");
		if (v) {
			addInputField( new CExtendedInput_Item_Char(":",false) );
			addInputField( new CExtendedInput_Item_Char("0123456789") );
			addInputField( new CExtendedInput_Item_Char("0123456789") );
		}
	}
#else
	addInputField( new CExtendedInput_Item_Char("=+-") );
	addInputField( new CExtendedInput_Item_Spacer(20) );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char(":",false) );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char(":",false) );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
	addInputField( new CExtendedInput_Item_Char("0123456789") );
#endif
	addInputField( new CExtendedInput_Item_newLiner(30) );
}

void CTimeInput::onBeforeExec()
{
#if 0 //--martii
	strcpy(value, "= 00:00:00");
#endif
}

void CTimeInput::onAfterExec()
{
#if 0 //--martii
	char tmp[10+1];
	strcpy(tmp, value);
	strcpy(value+1, tmp+2);
#endif
}
//-----------------------------#################################-------------------------------------------------------

CIntInput::CIntInput(const neutrino_locale_t Name, int *Value, const unsigned int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CExtendedInput(Name, &valueStringTmp, Hint_1, Hint_2, Observ)
{
	myValue = Value;

	if (Size<MAX_CINTINPUT_SIZE)
		m_size = Size;
	else
		m_size = MAX_CINTINPUT_SIZE-1;

	onBeforeExec();

	frameBuffer = CFrameBuffer::getInstance();
	for (unsigned int i=0;i<Size;i++)
		addInputField( new CExtendedInput_Item_Char("0123456789 ") );
	addInputField( new CExtendedInput_Item_newLiner(30) );
}

void CIntInput::onBeforeExec()
{
	char tmp[MAX_CINTINPUT_SIZE];
	snprintf(tmp, sizeof(tmp) - 1,"%*d", m_size, *myValue);
	tmp[sizeof(tmp) - 1] = 0;
	*valueString = std::string(tmp);
}

void CIntInput::onAfterExec()
{
	*myValue = atoi(*valueString);
}

//-----------------------------#################################-------------------------------------------------------
