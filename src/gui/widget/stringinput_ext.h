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


#ifndef __stringinput_ext__
#define __stringinput_ext__

#include "menue.h"

#include <driver/framebuffer.h>
#include <system/localize.h>

#include <string>
#include <vector>

class CExtendedInput_Item;
class CExtendedInput : public CMenuTarget
{
	protected:
		void Init(void);
		CFrameBuffer	*frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int hintPosY;
		int hheight; // head font height
		int iheight;
		int bheight; // body height
		int input_h; // input field height
		int input_w; // input field width
		int offset;

		std::vector<CExtendedInput_Item*> inputFields;
		int selectedChar;

		neutrino_locale_t name;
		neutrino_locale_t hint_1;
		neutrino_locale_t hint_2;

		char*	value;
		CChangeObserver*   observ;
		bool* cancel;

		virtual void paint();
		virtual void onBeforeExec(){};
		virtual void onAfterExec(){};

	public:

		CExtendedInput(const neutrino_locale_t Name, char* Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ = NULL, bool* cancel = NULL);
		~CExtendedInput();

		void hide();
		int exec( CMenuTarget* parent, const std::string & actionKey );
		void calculateDialog();

		void addInputField( CExtendedInput_Item* );
};


class CExtendedInput_Item
{
	protected:
		CFrameBuffer	*frameBuffer;
		int input_h; // input field height
		int input_w; // input field width
		int ix, iy;
		char* data;

	public:

		virtual void setDataPointer(char* Data){data=Data;};
		virtual void init(int &/*x*/, int &/*y*/){};
		virtual void paint(int /*x*/, int /*y*/, bool /*focusGained*/){};
		virtual bool isSelectable(){return true;};

		virtual void keyPressed( int /*key*/ ){};
		virtual ~CExtendedInput_Item() {};
};

class CExtendedInput_Item_Spacer : public CExtendedInput_Item
{
	protected:
		int mSpacingX;
		int mSpacingY;
	public:
		CExtendedInput_Item_Spacer(){mSpacingY = 0;mSpacingX = 0;};
		CExtendedInput_Item_Spacer(int spaceX, int spaceY=0){mSpacingX=spaceX;mSpacingY=spaceY;};
		virtual void init(int &x, int &y){x+=mSpacingX;y+=mSpacingY;};
		virtual bool isSelectable(){return false;};
};

class CExtendedInput_Item_newLiner : public CExtendedInput_Item
{
	protected:
		int mSpacingY;
	public:
		CExtendedInput_Item_newLiner(){mSpacingY=0;};
		CExtendedInput_Item_newLiner(int spaceY){mSpacingY=spaceY;};
		virtual void init(int &x, int &y){x=0;y+=mSpacingY;};
		virtual bool isSelectable(){return false;};
};


class CExtendedInput_Item_Char : public CExtendedInput_Item
{
	protected:
		std::string allowedChars;
		bool selectable;

		bool isAllowedChar( char );
		int getCharID( char );

	public:
		CExtendedInput_Item_Char(const std::string & Chars="", bool Selectable=true );
		virtual ~CExtendedInput_Item_Char(){};
		void setAllowedChars( const std::string & );
		virtual void init(int &x, int &y);
		virtual void paint(int x, int y, bool focusGained);

		virtual void keyPressed( int key );
		virtual bool isSelectable(){return selectable;};
};

//----------------------------------------------------------------------------------------------------

class CIPInput : public CExtendedInput
{
	char          IP[16];
	std::string * ip;

	protected:
		virtual void onBeforeExec();
		virtual void onAfterExec();

	public:
		CIPInput(const neutrino_locale_t Name, std::string & Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ = NULL);
};

//----------------------------------------------------------------------------------------------------

class CDateInput : public CExtendedInput
{
   private:
		time_t* time;

	protected:
		virtual void onBeforeExec();
		virtual void onAfterExec();

	public:
		CDateInput(const neutrino_locale_t Name, time_t* Time, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ = NULL);
		~CDateInput();
		char* getValue() {return value;}
};

//----------------------------------------------------------------------------------------------------

class CMACInput : public CExtendedInput
{
	char          MAC[32];
	std::string * mac;

	protected:
		virtual void onBeforeExec();
		virtual void onAfterExec();

	public:
		CMACInput(const neutrino_locale_t Name, std::string & Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ = NULL);
};

//----------------------------------------------------------------------------------------------------

class CTimeInput : public CExtendedInput
{
	protected:
		virtual void onBeforeExec();
		virtual void onAfterExec();

	public:
		CTimeInput(const neutrino_locale_t Name, char* Value, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ = NULL, bool* cancel=NULL);
};

//----------------------------------------------------------------------------------------------------

class CIntInput : public CExtendedInput
{
#define MAX_CINTINPUT_SIZE 16

	char myValueStringInput[MAX_CINTINPUT_SIZE];
	char myValueStringOutput[MAX_CINTINPUT_SIZE];

 	int* myValue;
	unsigned int m_size;
	protected:
		virtual void onBeforeExec();
		virtual void onAfterExec();

	public:
		/**
		 *@param Size how many digits can be entered
		 */
		CIntInput(const neutrino_locale_t Name, int& Value, const unsigned int Size, const neutrino_locale_t Hint_1, const neutrino_locale_t Hint_2, CChangeObserver* Observ = NULL);
		char* getValue() {
			return myValueStringOutput;
		}
		void updateValue() { onBeforeExec(); }
};

#endif
