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

	Module Name: textbox.cpp: .

	Description: implementation of the CTextBox class
				 This class provides a plain textbox with selectable features:
				 	- Foot, Title
				 	- Scroll bar
				 	- Frame shadow
				 	- Auto line break
				 	- fixed position or auto width and auto height (later not tested yet)
				 	- Center Text

	Date:	Nov 2005

	Author: Günther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	Revision History:
	Date			Author		Change Description
	   Nov 2005		Günther	initial implementation

****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "textbox.h"
#include <gui/widget/icons.h>

#define	TEXT_BORDER_WIDTH			 8
#define	SCROLL_FRAME_WIDTH			10
#define	SCROLL_MARKER_BORDER		 2

#define MAX_WINDOW_WIDTH  (g_settings.screen_EndX - g_settings.screen_StartX - 40)
#define MAX_WINDOW_HEIGHT (g_settings.screen_EndY - g_settings.screen_StartY - 40)	

#define MIN_WINDOW_WIDTH  ((g_settings.screen_EndX - g_settings.screen_StartX)>>1)
#define MIN_WINDOW_HEIGHT 40	

CTextBox::CTextBox(const char * text, Font* font_text, const int mode, 
		   const CBox* position, CFBWindow::color_t textBackgroundColor)
{
	//TRACE("[CTextBox] new\r\n");
	initVar();

	frameBuffer = NULL;
	max_width = 0;

 	if(text != NULL)		m_cText = text;
	if(font_text != NULL)	m_pcFontText = font_text;
	if(position != NULL)
	{
		m_cFrame	= *position;
		m_nMaxHeight = m_cFrame.iHeight;
		m_nMaxWidth = m_cFrame.iWidth;
	}

	m_nMode	= mode;

	/* in case of auto line break, we do no support auto width  yet */
	if( !(mode & NO_AUTO_LINEBREAK))
	{
		m_nMode = m_nMode & ~AUTO_WIDTH; /* delete any AUTO_WIDTH*/
	}

#if 0
	TRACE("  Mode: ");
	if(mode & SCROLL) TRACE("SCROLL ");
	if(mode & NO_AUTO_LINEBREAK) TRACE("NO_AUTO_LINEBREAK ");
	if(mode & AUTO_WIDTH) TRACE("AUTO_WIDTH ");
	if(mode & AUTO_HIGH) TRACE("AUTO_HIGH");
	TRACE("\r\n");

#endif
	//TRACE(" CTextBox::m_cText: %d, m_nMode %d\t\r\n",m_cText.size(),m_nMode);

	m_textBackgroundColor = textBackgroundColor;
	m_nFontTextHeight  = m_pcFontText->getHeight();
	//TRACE(" CTextBox::m_nFontTextHeight: %d\t\r\n",m_nFontTextHeight);

	/* Initialise the window frames first */
	initFramesRel();

	// than refresh text line array 
	refreshTextLineArray();
}

CTextBox::CTextBox(const char * text)
{
	//TRACE("[CTextBox] new\r\n");
	initVar();

	frameBuffer = NULL;
	if(text != NULL)		m_cText = *text;

	/* Initialise the window frames first */
	initFramesRel();

	// than refresh text line array 
	refreshTextLineArray();
}

CTextBox::CTextBox()
{
	//TRACE("[CTextBox] new\r\n");
	initVar();
	initFramesRel();

	frameBuffer = NULL;
}

CTextBox::~CTextBox()
{
	//TRACE("[CTextBox] del\r\n");
	m_cLineArray.clear();
	hide();

}

void CTextBox::initVar(void)
{
	//TRACE("[CTextBox]->InitVar\r\n");
	m_cText	= "";
	m_nMode = SCROLL;

	m_pcFontText = NULL;
	m_pcFontText  =  g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1];
	m_nFontTextHeight = m_pcFontText->getHeight();

	m_nNrOfPages = 1;
	m_nNrOfLines = 0;
	m_nLinesPerPage = 0;
	m_nCurrentLine = 0;
	m_nCurrentPage = 0;

	m_cFrame.iX		= g_settings.screen_StartX + ((g_settings.screen_EndX - g_settings.screen_StartX - MIN_WINDOW_WIDTH) >>1);
	m_cFrame.iWidth	= MIN_WINDOW_WIDTH;
	m_cFrame.iY		= g_settings.screen_StartY + ((g_settings.screen_EndY - g_settings.screen_StartY - MIN_WINDOW_HEIGHT) >>1);
	m_cFrame.iHeight	= MIN_WINDOW_HEIGHT;

	m_nMaxHeight = MAX_WINDOW_HEIGHT;
	m_nMaxWidth = MAX_WINDOW_WIDTH;
	
	m_textBackgroundColor = COL_MENUCONTENT_PLUS_0;

	m_cLineArray.clear();
}

void CTextBox::reSizeMainFrameWidth(int textWidth)
{
	//TRACE("[CTextBox]->ReSizeMainFrameWidth: %d, current: %d\r\n",textWidth,m_cFrameTextRel.iWidth);

	int iNewWindowWidth = textWidth  + m_cFrameScrollRel.iWidth   + 2*TEXT_BORDER_WIDTH;

	if( iNewWindowWidth > m_nMaxWidth) iNewWindowWidth = m_nMaxWidth;
	if( iNewWindowWidth < MIN_WINDOW_WIDTH) iNewWindowWidth = MIN_WINDOW_WIDTH;

	m_cFrame.iWidth	= iNewWindowWidth;

	/* Re-Init the children frames due to new main window */
	initFramesRel();
}

void CTextBox::reSizeMainFrameHeight(int textHeight)
{
	TRACE("[CTextBox]->ReSizeMainFrameHeight: %d, current: %d\r\n",textHeight,m_cFrameTextRel.iHeight);

	int iNewWindowHeight =	textHeight 
							+ 2*TEXT_BORDER_WIDTH;

	if( iNewWindowHeight > m_nMaxHeight) iNewWindowHeight = m_nMaxHeight;
	if( iNewWindowHeight < MIN_WINDOW_HEIGHT) iNewWindowHeight = MIN_WINDOW_HEIGHT;

	m_cFrame.iHeight	= iNewWindowHeight;

	/* Re-Init the children frames due to new main window */
	initFramesRel();
}

void CTextBox::initFramesRel(void)
{
	//TRACE("[CTextBox]->InitFramesRel\r\n");

	m_cFrameTextRel.iX		= 0;
	m_cFrameTextRel.iY		= 0;
	m_cFrameTextRel.iHeight	= m_cFrame.iHeight ;
	
	if(m_nMode & SCROLL)
	{
		m_cFrameScrollRel.iX		= m_cFrame.iWidth - SCROLL_FRAME_WIDTH;
		m_cFrameScrollRel.iY		= m_cFrameTextRel.iY;
		m_cFrameScrollRel.iWidth	= SCROLL_FRAME_WIDTH;
		m_cFrameScrollRel.iHeight	= m_cFrameTextRel.iHeight;
	}
	else
	{
		m_cFrameScrollRel.iX		= 0;
		m_cFrameScrollRel.iY		= 0;
		m_cFrameScrollRel.iHeight	= 0;
		m_cFrameScrollRel.iWidth	= 0;
	}

	m_cFrameTextRel.iWidth	= m_cFrame.iWidth - m_cFrameScrollRel.iWidth;

	m_nLinesPerPage = (m_cFrameTextRel.iHeight - (2*TEXT_BORDER_WIDTH)) / m_nFontTextHeight;

#if 0
	TRACE_1("Frames\r\n\tScren:\t%3d,%3d,%3d,%3d\r\n\tMain:\t%3d,%3d,%3d,%3d\r\n\tText:\t%3d,%3d,%3d,%3d \r\n\tScroll:\t%3d,%3d,%3d,%3d \r\n",
		g_settings.screen_StartX,
		g_settings.screen_StartY,
		g_settings.screen_EndX,
		g_settings.screen_EndY,
		m_cFrame.iX,
		m_cFrame.iY,
		m_cFrame.iWidth,
		m_cFrame.iHeight,
		m_cFrameTextRel.iX,
		m_cFrameTextRel.iY,
		m_cFrameTextRel.iWidth,
		m_cFrameTextRel.iHeight,
		m_cFrameScrollRel.iX,
		m_cFrameScrollRel.iY,
		m_cFrameScrollRel.iWidth,
		m_cFrameScrollRel.iHeight
		);
#endif
}

void CTextBox::refreshTextLineArray(void)
{      
	//TRACE("[CTextBox]->RefreshLineArray \r\n");
	int loop = true;
	int pos_prev = 0;
	int pos = 0;
	int	aktWidth = 0;
	int aktWordWidth = 0;
	int lineBreakWidth;
	int maxTextWidth = 0;

	m_nNrOfNewLine = 0;

	std::string	aktLine = "";
	std::string	aktWord = "";

	/* clear current line vector */
	m_cLineArray.clear();
	m_nNrOfLines = 0;

	if( m_nMode & AUTO_WIDTH)
	{
		/* In case of autowidth, we calculate the max allowed width of the textbox */
		lineBreakWidth = MAX_WINDOW_WIDTH - m_cFrameScrollRel.iWidth - 2*TEXT_BORDER_WIDTH;
	}
	else
	{
		/* If not autowidth, we just take the actuall textframe width */
		lineBreakWidth = m_cFrameTextRel.iWidth - 2*TEXT_BORDER_WIDTH;
	}
	if(max_width)
		lineBreakWidth = max_width;
	//printf("TextBox: lineBreakWidth %d\n", lineBreakWidth);
	int TextChars = m_cText.size();
	// do not parse, if text is empty 
	if(TextChars > 0)
	{
		while(loop)
		{
			if(m_nMode & NO_AUTO_LINEBREAK)
			{
				pos = m_cText.find_first_of("\n",pos_prev);
			}
			else
			{
				pos = m_cText.find_first_of("\n-. ",pos_prev);
			}

			//TRACE_1("     pos: %d pos_prev: %d\r\n",pos,pos_prev);

			if(pos == -1)
			{
				pos = TextChars+1;
				loop = false; // note, this is not 100% correct. if the last characters does not fit in one line, the characters after are cut
				//TRACE_1(" Textend found\r\n");
			}

			aktWord = m_cText.substr(pos_prev, pos - pos_prev + 1);
			aktWordWidth = m_pcFontText->getRenderWidth(aktWord, true);
			pos_prev = pos + 1;
			//if(aktWord.find("&quot;") == )
			if(1)
			{
				//TRACE_1("     aktWord: >%s< pos:%d\r\n",aktWord.c_str(),pos);

				if( aktWidth + aktWordWidth > lineBreakWidth &&
						!(m_nMode & NO_AUTO_LINEBREAK))
				{
					/* we need a new line before we can continue */
					m_cLineArray.push_back(aktLine);
					//TRACE_1("  end line: %s\r\n", aktLine.c_str());
					m_nNrOfLines++;
					aktLine = "";
					aktWidth = 0;

					if(pos_prev >= TextChars) loop = false;
				}

				aktLine  += aktWord;
				aktWidth += aktWordWidth;
				if (aktWidth > maxTextWidth) maxTextWidth = aktWidth;
				//TRACE_1("     aktLine : %s\r\n",aktLine.c_str());
				//TRACE_1("     aktWidth: %d aktWordWidth:%d\r\n",aktWidth,aktWordWidth);

				if( m_cText[pos] == '\n' ||
						loop == false)
				{
					// current line ends with an carriage return, make new line
					if (m_cText[pos] == '\n')
						aktLine.erase(aktLine.size() - 1,1);
					m_cLineArray.push_back(aktLine);
					m_nNrOfLines++;
					aktLine = "";
					aktWidth = 0;
					m_nNrOfNewLine++;
					if(pos_prev >= TextChars) loop = false;
				}
			}
		}


		/* check if we have to recalculate the window frame size, due to auto width and auto height */
		if( m_nMode & AUTO_WIDTH)
		{
			reSizeMainFrameWidth(maxTextWidth);
		}

		if(m_nMode & AUTO_HIGH)
		{
			reSizeMainFrameHeight(m_nNrOfLines * m_nFontTextHeight);
		}

		m_nLinesPerPage = (m_cFrameTextRel.iHeight - (2*TEXT_BORDER_WIDTH)) / m_nFontTextHeight;
		m_nNrOfPages =	((m_nNrOfLines-1) / m_nLinesPerPage) + 1;

		if(m_nCurrentPage >= m_nNrOfPages)
		{
			m_nCurrentPage = m_nNrOfPages - 1;
			m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage;
		}
	}
	else
	{
		m_nNrOfPages = 0;
		m_nNrOfLines = 0;
		m_nCurrentPage = 0;
		m_nCurrentLine = 0;
		m_nLinesPerPage = 1;
	}
#if 0	
	TRACE_1(" m_nNrOfPages:     %d\r\n",m_nNrOfPages);
	TRACE_1(" m_nNrOfLines:     %d\r\n",m_nNrOfLines);
	TRACE_1(" m_nNrOfNewLine:   %d\r\n",m_nNrOfNewLine);
	TRACE_1(" maxTextWidth:  %d\r\n",maxTextWidth);
	TRACE_1(" m_nLinesPerPage:  %d\r\n",m_nLinesPerPage);
	TRACE_1(" m_nFontTextHeight:%d\r\n",m_nFontTextHeight);
	TRACE_1(" m_nCurrentPage:   %d\r\n",m_nCurrentPage);
	TRACE_1(" m_nCurrentLine:   %d\r\n",m_nCurrentLine);
#endif
}

void CTextBox::refreshScroll(void)
{
	if(	!(m_nMode & SCROLL)) return;
	if( frameBuffer == NULL) return;

	if (m_nNrOfPages > 1) 
	{
		frameBuffer->paintBoxRel(m_cFrameScrollRel.iX+m_cFrame.iX, m_cFrameScrollRel.iY+m_cFrame.iY, 
				m_cFrameScrollRel.iWidth, m_cFrameScrollRel.iHeight, 
				COL_MENUCONTENT_PLUS_1);
		unsigned int marker_size = m_cFrameScrollRel.iHeight / m_nNrOfPages;
		frameBuffer->paintBoxRel(m_cFrameScrollRel.iX + SCROLL_MARKER_BORDER+m_cFrame.iX, 
				m_cFrameScrollRel.iY + m_nCurrentPage * marker_size+m_cFrame.iY, 
				m_cFrameScrollRel.iWidth - 2*SCROLL_MARKER_BORDER, 
				marker_size, COL_MENUCONTENT_PLUS_3);
	}
	else
	{
		frameBuffer->paintBoxRel(m_cFrameScrollRel.iX+m_cFrame.iX, m_cFrameScrollRel.iY+m_cFrame.iY, 
				m_cFrameScrollRel.iWidth, m_cFrameScrollRel.iHeight, 
				m_textBackgroundColor);
	}
}

void CTextBox::refreshText(void)
{
	if( frameBuffer == NULL) return;
	//TRACE("   CTextBox::refreshText: %d,%s\r\n",m_nCurrentLine,m_cLineArray[m_nCurrentLine].c_str());
	//Paint Text Background
	frameBuffer->paintBoxRel(m_cFrameTextRel.iX+m_cFrame.iX, m_cFrameTextRel.iY+m_cFrame.iY, 
			m_cFrameTextRel.iWidth, m_cFrameTextRel.iHeight,  m_textBackgroundColor);

	if( m_nNrOfLines <= 0) return;
	int y = m_cFrameTextRel.iY + TEXT_BORDER_WIDTH;
	int i;
	int x_center = 0;

	for(i = m_nCurrentLine; i < m_nNrOfLines && i < m_nCurrentLine + m_nLinesPerPage; i++)
	{
		y += m_nFontTextHeight;

		if( m_nMode & CENTER )
		{
			x_center = (m_cFrameTextRel.iWidth - m_pcFontText->getRenderWidth(m_cLineArray[i], true))>>1;
		}

		m_pcFontText->RenderString(m_cFrameTextRel.iX + TEXT_BORDER_WIDTH + x_center+m_cFrame.iX, 
				y+m_cFrame.iY, m_cFrameTextRel.iWidth, m_cLineArray[i].c_str(), 
				COL_MENUCONTENT, 0, true); // UTF-8
	}
}

void CTextBox::scrollPageDown(const int pages)
{
	if( !(m_nMode & SCROLL)) return;
	if( m_nNrOfLines <= 0) return;
	TRACE("[CTextBox]->ScrollPageDown \r\n");


	if(m_nCurrentPage + pages < m_nNrOfPages)
	{
		m_nCurrentPage += pages; 
	}
	else 
	{
		m_nCurrentPage = m_nNrOfPages - 1;
	}
	m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage; 
	refresh();
}

void CTextBox::scrollPageUp(const int pages)
{
	if( !(m_nMode & SCROLL)) return;
	if( m_nNrOfLines <= 0) return;
	TRACE("[CTextBox]->ScrollPageUp \r\n");


	if(m_nCurrentPage - pages > 0)
	{
		m_nCurrentPage -= pages; 
	}
	else 
	{
		m_nCurrentPage = 0;
	}
	m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage; 
	refresh();
}

void CTextBox::refresh(void)
{
	if( frameBuffer == NULL) return;
	//TRACE("[CTextBox]->Refresh\r\n");
//printf("setText::refresh!\n");

	//Paint text
	refreshScroll();
	refreshText();
}

bool CTextBox::setText(const std::string* newText, int _max_width)
{
	//TRACE("[CTextBox]->SetText \r\n");
	bool result = false;
	max_width = _max_width;
//printf("setText: _max_width %d max_width %d\n", _max_width, max_width);
	if (newText != NULL)
	{
		m_cText = *newText;
		//m_cText = *newText + "\n"; //FIXME test
		refreshTextLineArray();
		refresh();
		result = true;
	}
	return(result);
};

void CTextBox::paint (void)
{
	if(frameBuffer != NULL) return;
	//TRACE("[CTextBox]->paint \r\n");
	frameBuffer = CFrameBuffer::getInstance();
	refresh();
}

void CTextBox::hide (void)
{
	if(frameBuffer == NULL) return;
	//TRACE("[CTextBox]->hide \r\n");
	frameBuffer->paintBackgroundBoxRel(m_cFrame.iX, m_cFrame.iY, m_cFrame.iWidth, m_cFrame.iHeight);
	frameBuffer = NULL;
}
