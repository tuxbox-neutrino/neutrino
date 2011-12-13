
/***************************************************************************
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

	Module Name: textbox.h .

	Description: interface of the CTextBox class

	Date:	Nov 2005

	Author: Günther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	Revision History:
	Date			Author		Change Description
	   Nov 2005		Günther	initial implementation

****************************************************************************/

#if !defined(TEXTBOX_H)
#define TEXTBOX_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string>
#include <vector>

#include <global.h>
#include <driver/fb_window.h>

#define TRACE  printf
#define TRACE_1 printf

class CBox
{
	private:

	public:
		/* Constructor */
		inline CBox(){;};
		inline CBox( const int _iX, const int _iY, const int _iWidth, const int _iHeight){iX=_iX; iY=_iY; iWidth=_iWidth; iHeight=_iHeight;};
		inline ~CBox(){;};
		/* Functions */
		/* Variables */
		int iX;
		int iY;
		int iWidth;
		int iHeight;
};

class CTextBox  
{
	private:
		/* Functions */
		void refreshTextLineArray(void);
		void initVar(void);
		void initFramesRel(void);
		void refreshScroll(void);
		void refreshText(void);
		void reSizeMainFrameWidth(int maxTextWidth);
		void reSizeMainFrameHeight(int maxTextHeight);

		/* Variables */
		std::string m_cText;
		std::vector<std::string> m_cLineArray;

		bool m_showTextFrame;

		CBox m_cFrame;
		CBox m_cFrameTextRel;
		CBox m_cFrameScrollRel;

		int m_nMaxHeight;
		int m_nMaxWidth;

		int m_nMode;

		int m_nNrOfPages;
		int m_nNrOfLines;
		int m_nNrOfNewLine;
		int m_nMaxLineWidth;
		int m_nLinesPerPage;
		int m_nCurrentLine;
		int m_nCurrentPage;

		Font* m_pcFontText;
		int m_nFontTextHeight;
		CFBWindow::color_t m_textBackgroundColor;

		CFrameBuffer * frameBuffer;
		int max_width;
	public:
		/* Constructor */
		CTextBox();
		CTextBox(	const char * text);
		CTextBox(	const char * text, 
					Font* font_text,
					const int mode, 
					const CBox* position,
					CFBWindow::color_t textBackgroundColor = COL_MENUCONTENT_PLUS_0);

		virtual ~CTextBox();

		/* Functions */
		void    refresh(void);
		void    scrollPageDown(const int pages);
		void    scrollPageUp(const int pages);				
		bool	setText(const std::string* newText, int _max_width = 0);

		inline	bool 	isPainted(void)			{if( frameBuffer == NULL) return (false); else return (true);};
		inline	CBox	getWindowsPos(void)		{return(m_cFrame);};
		inline	int	getMaxLineWidth(void)		{return(m_nMaxLineWidth);};
		inline  int     getLines(void)			{return(m_nNrOfLines);};
		inline  int     getPages(void)			{return(m_nNrOfPages);};
		inline	void	movePosition(int x, int y)	{m_cFrame.iX = x; m_cFrame.iY = y;};

		void paint (void);
		void hide (void);


		/* Variables */
		typedef enum mode_
		{
			AUTO_WIDTH	= 0x01,
			AUTO_HIGH	= 0x02,
			SCROLL		= 0x04,
			CENTER		= 0x40,
			NO_AUTO_LINEBREAK = 0x80
		} mode;
};

#endif // !defined(AFX_TEXTBOX_H__208DED01_ABEC_491C_A632_5B21057DC5D8__INCLUDED_)
