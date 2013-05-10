
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

#include <driver/fb_window.h>
#include <gui/color.h>
#include <gui/customcolor.h>

#define TRACE  printf
#define TRACE_1 printf

class CBox
{
	private:

	public:
		/* Constructor */
		inline CBox(){iX=0; iY=0; iWidth=0; iHeight=0;};
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
	public:
		/* Variables */
		enum textbox_modes
		{
			AUTO_WIDTH	= 0x01, 	//auto adapt frame width to max width or max text width, text is painted with auto linebreak
			AUTO_HIGH	= 0x02, 	//auto adapt frame height to max height, text is painted with auto linebreak
			SCROLL		= 0x04, 	//frame box contains scrollbars on long text
			CENTER		= 0x40, 	//paint text centered
			RIGHT		= 0x80, 	//paint text right
			TOP		= 0x100,	//paint text on top of frame
			BOTTOM		= 0x200,	//paint text on bottom of frame
			NO_AUTO_LINEBREAK = 0x400  	//paint text without auto linebreak,  cutting text
		};
		
	private:
		/* Functions */
		void refreshTextLineArray(void);
		void initVar(void);
		void initFramesRel(void);
		void initFramesAndTextArray();
		void refreshScroll(void);
		void refreshText(void);
		void reSizeMainFrameWidth(int maxTextWidth);
		void reSizeMainFrameHeight(int maxTextHeight);
		int  getFontTextHeight();

		/* Variables */
		std::string m_cText;
		std::vector<std::string> m_cLineArray;

		bool m_showTextFrame;

		CBox m_cFrame;
		CBox m_cFrameTextRel;
		CBox m_cFrameScrollRel;

		int m_nMaxHeight;
		int m_nMaxWidth;
		int m_nMinHeight;
		int m_nMinWidth;

		int m_nMaxTextWidth;

		int m_nMode;

		int m_nNrOfPages;
		int m_nNrOfLines;
		int m_nNrOfNewLine;
		int m_nMaxLineWidth;
		int m_nLinesPerPage;
		int m_nCurrentLine;
		int m_nCurrentPage;

		int  m_nBgRadius;
		int  m_nBgRadiusType;
		bool m_nPaintBackground;

		Font* m_pcFontText;
		int m_nFontTextHeight;
		CFBWindow::color_t m_textBackgroundColor;
		fb_pixel_t m_textColor;

		CFrameBuffer * frameBuffer;
/*		int max_width;*/
		
		int text_border_width;
		bool m_FontUseDigitHeight;
		
	public:
		/* Constructor */
		CTextBox();
		CTextBox(	const char * text);
		CTextBox(	const char * text, 
					Font* font_text,
					const int pmode,
					const CBox* position,
					CFBWindow::color_t textBackgroundColor = COL_MENUCONTENT_PLUS_0);

		virtual ~CTextBox();

		/* Functions */
		void    refresh(void);
		void    scrollPageDown(const int pages);
		void    scrollPageUp(const int pages);
		void    enableBackgroundPaint(bool mode = true){m_nPaintBackground = mode;};
		bool	setText(const std::string* newText, int max_width = 0);
		void 	setTextColor(fb_pixel_t color_text){ m_textColor = color_text;};
		void	setBackGroundRadius(const int radius, const int type = CORNER_ALL){m_nBgRadius = radius; m_nBgRadiusType = type;};
		void    setTextBorderWidth(int border);
		void	setTextFont(Font* font_text);
		void	setTextMode(const int text_mode){m_nMode = text_mode;};
		void	setBackGroundColor(CFBWindow::color_t textBackgroundColor){m_textBackgroundColor = textBackgroundColor;};
		void	setWindowPos(const CBox* position){m_cFrame = *position;};
		void 	setWindowMaxDimensions(const int width, const int height);
		void 	setWindowMinDimensions(const int width, const int height);
		void    setFontUseDigitHeight(bool set=true);

		inline	bool 	isPainted(void)			{if( frameBuffer == NULL) return (false); else return (true);};
		inline	CBox	getWindowsPos(void)		{return(m_cFrame);};
		inline	int	getMaxLineWidth(void)		{return(m_nMaxLineWidth);};
		inline  int     getLines(void)			{return(m_nNrOfLines);};
		inline  int     getPages(void)			{return(m_nNrOfPages);};
		inline	void	movePosition(int x, int y)	{m_cFrame.iX = x; m_cFrame.iY = y;};

		void paint (void);
		void hide (void);
};

#endif // !defined(AFX_TEXTBOX_H__208DED01_ABEC_491C_A632_5B21057DC5D8__INCLUDED_)
