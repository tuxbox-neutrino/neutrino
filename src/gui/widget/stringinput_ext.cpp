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

#include <gui/widget/stringinput_ext.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include <gui/color.h>

#include <gui/widget/messagebox.h>

#include <global.h>
#include <neutrino.h>


CExtendedInput::CExtendedInput(const neutrino_locale_t Name, char* Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ, bool* Cancel)
{
	name = Name;
	value = Value;
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
	mheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	iheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->getHeight();

	width = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(name), true) + 20; // UTF-8
	height = hheight+ mheight+ 20;

	if (hint_1 != NONEXISTANT_LOCALE)
		height += iheight;
	if (hint_2 != NONEXISTANT_LOCALE)
		height += iheight;

	x = frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - width)>>1);
	y = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - height)>>1);
}

CExtendedInput::~CExtendedInput()
{
	for (std::vector<CExtendedInput_Item*>::iterator it = inputFields.begin(); it < inputFields.end(); it++)
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

	selectedChar = -1;
	for(unsigned int i=0; i<inputFields.size();i++)
	{
		inputFields[i]->init( ix, iy);
		inputFields[i]->setDataPointer( &value[i] );
		if ((selectedChar==-1) && (inputFields[i]->isSelectable()))
		{
			selectedChar = i;
		}
		maxX = ix > maxX ? ix : maxX;
		maxY = iy > maxY ? iy : maxY;
	}

	width = width > maxX+40 ? width : maxX+40;
	height = height > maxY + hheight + mheight ? height : maxY + hheight + mheight;

	hintPosY = height -10;

	if (hint_1 != NONEXISTANT_LOCALE)
		height += iheight;
	if (hint_2 != NONEXISTANT_LOCALE)
		height += iheight;

	x = ((frameBuffer->getScreenWidth() - width)>>1);
	y = ((frameBuffer->getScreenHeight() - height)>>1);

	hintPosY += y;
}


int CExtendedInput::exec( CMenuTarget* parent, const std::string & )
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	onBeforeExec();
	int res = menu_return::RETURN_REPAINT;

        char *oldval = new char[inputFields.size()+10];
	if(oldval == NULL)
		return res;
	char  *dispval = new char[inputFields.size()+10];
	if(dispval == NULL){
		delete[] oldval;
		return res;
	}

	if (parent)
	{
		parent->hide();
	}

	strcpy(oldval, value);
	strcpy(dispval, value);
	paint();
	frameBuffer->blit();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings
::TIMING_MENU]);

	bool loop=true;
	while (loop)
	{
		if ( strcmp(value, dispval) != 0)
		{
			std::string tmp = value;
			CVFD::getInstance()->showMenuText(1, tmp.c_str(), selectedChar+1);
			strcpy(dispval, value);
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
				inputFields[oldSelectedChar]->paint( x+20, y+hheight +20, false );
				inputFields[selectedChar]->paint( x+20, y+hheight +20, true );
				std::string tmp = value;
				CVFD::getInstance()->showMenuText(1, tmp.c_str(), selectedChar+1);
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
				inputFields[oldSelectedChar]->paint( x+20, y+hheight +20, false );
				inputFields[selectedChar]->paint( x+20, y+hheight +20, true );
				std::string tmp = value;
				CVFD::getInstance()->showMenuText(1, tmp.c_str(), selectedChar+1);
			}
		}
		else if ( (CRCInput::getUnicodeValue(msg) != -1) || (msg == CRCInput::RC_red) || (msg == CRCInput::RC_green) || (msg == CRCInput::RC_blue) || (msg == CRCInput::RC_yellow)
					|| (msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
		{
			inputFields[selectedChar]->keyPressed(msg);
			inputFields[selectedChar]->paint( x+20, y+hheight +20, true );
		}
		else if (msg==CRCInput::RC_ok)
		{
			loop=false;
			if(cancel != NULL)
				*cancel = false;
		}
		else if ( (msg==CRCInput::RC_home) || (msg==CRCInput::RC_timeout) )
		{
			if(strcmp(value, oldval)!= 0){
				int erg = ShowLocalizedMessage(name, LOCALE_MESSAGEBOX_DISCARD, CMessageBox::mbrYes, CMessageBox::mbNo | CMessageBox::mbYes | CMessageBox::mbCancel);
				 if(erg==CMessageBox::mbrYes){
					strcpy(value, oldval);
					loop=false;
					if(cancel != NULL)
						*cancel = true;
				 }
				 else if(erg==CMessageBox::mbrNo){
					 loop=false;
					 if(cancel != NULL)
						 *cancel = false;
				 }
				 else if(erg==CMessageBox::mbrCancel){
				 }
			} else {
				//keine änderungen - beenden ok
				loop=false;
				if(cancel != NULL)
					*cancel = true;
			}
		}
		else if ((msg ==CRCInput::RC_sat) || (msg == CRCInput::RC_favorites))
		{
		}
		else if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
		{
			loop = false;
			res = menu_return::RETURN_EXIT_ALL;
		}
		frameBuffer->blit();
	}

	hide();

	onAfterExec();

	if ((observ) && (msg == CRCInput::RC_ok))
	{
		observ->changeNotify(name, value);
	}

	delete[] dispval;
	delete[] oldval;

	return res;
}

void CExtendedInput::hide()
{
	frameBuffer->paintBackgroundBoxRel(x, y, width, height);
	frameBuffer->blit();
}

void CExtendedInput::paint()
{
	frameBuffer->paintBoxRel(x, y, width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	frameBuffer->paintBoxRel(x, y + hheight, width, height - hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+ 10, y+ hheight, width- 10, g_Locale->getText(name), COL_MENUHEAD, 0, true); // UTF-8

	if (hint_1 != NONEXISTANT_LOCALE)
	{
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ 20, hintPosY, width- 20, g_Locale->getText(hint_1), COL_MENUCONTENT, 0, true); // UTF-8
		if (hint_2 != NONEXISTANT_LOCALE)
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU_INFO]->RenderString(x+ 20, hintPosY + iheight, width- 20, g_Locale->getText(hint_2), COL_MENUCONTENT, 0, true); // UTF-8
	}

	for(unsigned int i=0; i<inputFields.size();i++)
	{
		inputFields[i]->paint( x+20, y+hheight +20, (i== (unsigned int) selectedChar) );
	}


}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


CExtendedInput_Item_Char::CExtendedInput_Item_Char(const std::string & Chars, bool Selectable )
{
	frameBuffer = CFrameBuffer::getInstance();
	idx = 20;
	idy = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	allowedChars = Chars;
	selectable = Selectable;
}

void CExtendedInput_Item_Char::init(int &x, int &y)
{
	ix = x;
	iy = y;
	x += idx;
}

void CExtendedInput_Item_Char::setAllowedChars( const std::string & ac )
{
	allowedChars = ac;
}

void CExtendedInput_Item_Char::paint(int x, int y, bool focusGained )
{
	int startx = ix + x;
	int starty = iy + y;

	uint8_t    color;
	fb_pixel_t bgcolor;

	if (focusGained)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else
	{
		color   = COL_MENUCONTENT;
		bgcolor = COL_MENUCONTENT_PLUS_0;
	}

	frameBuffer->paintBoxRel( startx, starty, idx, idy, COL_MENUCONTENT_PLUS_4);
	frameBuffer->paintBoxRel( startx+1, starty+1, idx-2, idy-2, bgcolor);

	char text[2];
	text[0] = *data;
	text[1] = 0;
	int xfpos = startx + 1 + ((idx- g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth( text ))>>1);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(xfpos,starty+idy, idx, text, color);
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
	int value = CRCInput::getUnicodeValue(key);
	if (value != -1)
	{
		if (isAllowedChar((char)value))
		{
			*data = (char)value;
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

CIPInput::CIPInput(const neutrino_locale_t Name, std::string & Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CExtendedInput(Name, IP, Hint_1, Hint_2, Observ)
{
	ip = &Value;
	frameBuffer = CFrameBuffer::getInstance();
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
	calculateDialog();
}

void CIPInput::onBeforeExec()
{
	if (ip->empty())
	{
		strcpy(value, "000.000.000.000");
		//printf("[neutrino] value-before(2): %s\n", value);
		return;
	}
	unsigned char _ip[4];
	sscanf(ip->c_str(), "%hhu.%hhu.%hhu.%hhu", &_ip[0], &_ip[1], &_ip[2], &_ip[3]);
	sprintf( value, "%03hhu.%03hhu.%03hhu.%03hhu", _ip[0], _ip[1], _ip[2], _ip[3]);
}

void CIPInput::onAfterExec()
{
	int _ip[4];
	sscanf( value, "%3d.%3d.%3d.%3d", &_ip[0], &_ip[1], &_ip[2], &_ip[3] );
	sprintf( value, "%d.%d.%d.%d", _ip[0], _ip[1], _ip[2], _ip[3]);
	if(strcmp(value,"0.0.0.0")==0)
	{
		(*ip) = "";
	}
	else
		(*ip) = value;
}

//-----------------------------#################################-------------------------------------------------------
CDateInput::CDateInput(const neutrino_locale_t Name, time_t* Time, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CExtendedInput(Name, (char *) "", Hint_1, Hint_2, Observ)
{
	time=Time;
	value= new char[20];
	struct tm *tmTime = localtime(time);
	sprintf( value, "%02d.%02d.%04d %02d:%02d", tmTime->tm_mday, tmTime->tm_mon+1,
				tmTime->tm_year+1900,
				tmTime->tm_hour, tmTime->tm_min);

	frameBuffer = CFrameBuffer::getInstance();
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
	calculateDialog();
}
CDateInput::~CDateInput()
{
	delete value;
}
void CDateInput::onBeforeExec()
{
	struct tm *tmTime = localtime(time);
	sprintf( value, "%02d.%02d.%04d %02d:%02d", tmTime->tm_mday, tmTime->tm_mon+1,
				tmTime->tm_year+1900,
				tmTime->tm_hour, tmTime->tm_min);
	dst = tmTime->tm_isdst;
}

void CDateInput::onAfterExec()
{
	struct tm tmTime;
	sscanf( value, "%02d.%02d.%04d %02d:%02d", &tmTime.tm_mday, &tmTime.tm_mon,
			&tmTime.tm_year,
			&tmTime.tm_hour, &tmTime.tm_min);
	tmTime.tm_mon -= 1;
	tmTime.tm_year -= 1900;
	tmTime.tm_sec = 0;

	//tmTime.tm_isdst = dst;
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
	struct tm *tmTime2 = localtime(time);
	sprintf( value, "%02d.%02d.%04d %02d:%02d", tmTime2->tm_mday, tmTime2->tm_mon+1,
			tmTime2->tm_year+1900,
			tmTime2->tm_hour, tmTime2->tm_min);
}
//-----------------------------#################################-------------------------------------------------------

CMACInput::CMACInput(const neutrino_locale_t Name, char* Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CExtendedInput(Name, Value, Hint_1, Hint_2, Observ)
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
	calculateDialog();
}

void CMACInput::onBeforeExec()
{
	if (value[0] == 0) /* strcmp(value, "") == 0 */
	{
		strcpy(value, "00:00:00:00:00:00");
		printf("[neutrino] value-before(2): %s\n", value);
		return;
	}
	int _mac[6];
	sscanf( value, "%x:%x:%x:%x:%x:%x", &_mac[0], &_mac[1], &_mac[2], &_mac[3], &_mac[4], &_mac[5] );
	sprintf( value, "%02x:%02x:%02x:%02x:%02x:%02x", _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);
}

void CMACInput::onAfterExec()
{
	int _mac[6];
	sscanf( value, "%x:%x:%x:%x:%x:%x", &_mac[0], &_mac[1], &_mac[2], &_mac[3], &_mac[4], &_mac[5] );
	sprintf( value, "%02x:%02x:%02x:%02x:%02x:%02x", _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);
	if(strcmp(value,"00:00:00:00:00:00")==0)
		value[0] = 0; /* strcpy(value, ""); */
}

//-----------------------------#################################-------------------------------------------------------

CTimeInput::CTimeInput(const neutrino_locale_t Name, char* Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ, bool* Cancel)
	: CExtendedInput(Name, Value, Hint_1, Hint_2, Observ, Cancel)
{
	frameBuffer = CFrameBuffer::getInstance();
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
	addInputField( new CExtendedInput_Item_newLiner(30) );
	calculateDialog();
}

void CTimeInput::onBeforeExec()
{
	strcpy(value, "= 00:00:00");
}

void CTimeInput::onAfterExec()
{
	char tmp[10+1];
	strcpy(tmp, value);
	strcpy(value+1, tmp+2);
}
//-----------------------------#################################-------------------------------------------------------

CIntInput::CIntInput(const neutrino_locale_t Name, int& Value, const unsigned int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ)
	: CExtendedInput(Name, myValueStringInput, Hint_1, Hint_2, Observ)
{
	myValue = &Value;

	if (Size<MAX_CINTINPUT_SIZE)
		m_size = Size;
	else
		m_size = MAX_CINTINPUT_SIZE-1;
 	if (*myValue == 0)
 	{
		sprintf(myValueStringInput,"%-7d",0);
		sprintf(myValueStringOutput,"%7d",0);
 	} else {
		sprintf(myValueStringInput,"%-*d",m_size,*myValue);
		sprintf(myValueStringOutput,"%*d",m_size,*myValue);
	}

	frameBuffer = CFrameBuffer::getInstance();
	for (unsigned int i=0;i<Size;i++)
	{
		addInputField( new CExtendedInput_Item_Char("0123456789 ") );
	}
	addInputField( new CExtendedInput_Item_newLiner(30) );
	calculateDialog();
}

void CIntInput::onBeforeExec()
{
 	if (*myValue == 0)
 	{
		sprintf(myValueStringInput,"%-7d",0);
		sprintf(myValueStringOutput,"%7d",0);
 	} else {
		sprintf(myValueStringInput,"%-*d",m_size,*myValue);
		sprintf(myValueStringOutput,"%*d",m_size,*myValue);
	}
}

void CIntInput::onAfterExec()
{
	sscanf(myValueStringInput, "%d", myValue);
	sprintf(myValueStringOutput,"%d",*myValue);
}

//-----------------------------#################################-------------------------------------------------------
