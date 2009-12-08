/*
	Neutrino-GUI  -   DBoxII-Project

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

	***********************************************************
  
	Module Name: msgbox.h .

	Description: interface of the CMsgBox class

	Date:	  Nov 2005

	Author: Günther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	Revision History:
	Date			Author		Change Description
	   Nov 2005		Günther	initial implementation

*/

#if !defined(MSGBOX_H)
#define MSGBOX_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include "textbox.h"

#include <gui/widget/icons.h>
#include <driver/fb_window.h>

class CMsgBox  
{
	public:
		/* enum definition */
		enum result_
		{
			mbrYes    = 0,
			mbrNo     = 1,
			mbrCancel = 2,
			mbrBack   = 3
		};
		enum buttons_
		{
			mbYes =		0x01,
			mbNo =		0x02,
			mbCancel =	0x04,
			mbAll =		0x07,
			mbBack =	0x08
		};
		 enum mode_
		{
			AUTO_WIDTH	= 0x01,
			AUTO_HIGH	= 0x02,
			SCROLL		= 0x04,
			TITLE		= 0x08,
			FOOT		= 0x10,
			BORDER		= 0x20,
			CENTER		= 0x40,
			NO_AUTO_LINEBREAK= 0x80
		}mode;

	private:
		/* Functions */
		void initVar(void);
		void initFramesRel(void);
		void refreshFoot(void);
		void refreshTitle(void);
		void refreshText(void);
		void refreshBorder(void);

		/* Variables */
		std::string m_cIcon;
		std::string m_cTitle;

		CBox	m_cBoxFrame;
		CBox	m_cBoxFrameText;
		CBox	m_cBoxFrameTitleRel;
		CBox	m_cBoxFrameFootRel;

		int		m_nMode;

		int		m_nWindowFrameBorderWidth;

		Font*	m_pcFontTitle;
		int		m_nFontTitleHeight;

		Font*	m_pcFontFoot;
		int		m_nFontFootHeight;
		int		m_nFootButtons;

		CTextBox* m_pcTextBox;
		//CFBWindow* m_pcWindow;
		CFrameBuffer * m_pcWindow;

		result_	m_nResult;
	public:
		/* Constructor */
		virtual ~CMsgBox();
		CMsgBox();
		CMsgBox(const char * text);
		CMsgBox(  const char * text, 
				   Font* fontText,
				   const int mode, 
				   const CBox* position, 
				   const char * title,
				   Font* fontTitle,
				   const char * icon,
				   int return_button = mbCancel, 
				   const result_ default_result = mbrCancel);

		/* Functions */
		bool    paint(void);
		bool    hide(void);
		int     exec(int timeout,int returnDefaultOnTimeout = false);
		void    refresh(void);
		void    scrollPageDown(const int pages);
		void    scrollPageUp(const int pages);
		int		result(void);

		bool	setText(const std::string* newText);
};

extern int ShowMsg2UTF(	const neutrino_locale_t Caption, 
						const char * const Text, 
						const CMsgBox::result_ Default, 
						const uint32_t ShowButtons, 
						const char * const Icon = NULL, 
						const int Width = 450, 
						const int timeout = -1, 
						bool returnDefaultOnTimeout = false); // UTF-8

extern int ShowMsg2UTF(	const char * const Title, 
						const char * const Text, 
						const CMsgBox::result_ Default, 
						const uint32_t ShowButtons, 
						const char * const Icon = NULL, 
						const int Width = 450, 
						const int timeout = -1, 
						bool returnDefaultOnTimeout = false); // UTF-8
						

#endif
