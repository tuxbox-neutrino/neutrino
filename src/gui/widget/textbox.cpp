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

#include <global.h>

#include "textbox.h"
#include <gui/widget/icons.h>

#define	SCROLL_FRAME_WIDTH	10
#define	SCROLL_MARKER_BORDER	 2

#define MAX_WINDOW_WIDTH  (g_settings.screen_EndX - g_settings.screen_StartX - 40)
#define MAX_WINDOW_HEIGHT (g_settings.screen_EndY - g_settings.screen_StartY - 40)

#define MIN_WINDOW_WIDTH  ((g_settings.screen_EndX - g_settings.screen_StartX)>>1)
#define MIN_WINDOW_HEIGHT 40


CTextBox::CTextBox(const char * text, Font* font_text, const int pmode,
		   const CBox* position, CFBWindow::color_t textBackgroundColor)
{
	//TRACE("[CTextBox] new\r\n");
	initVar();

	if(text != NULL)
		m_cText = text;
	
	if(font_text != NULL)
		m_pcFontText = font_text;
	
	if(position != NULL)
	{
		m_cFrame 	= *position;
		m_nMaxHeight 	= m_cFrame.iHeight;
		m_nMaxWidth 	= m_cFrame.iWidth;
	}

	m_nMode		= pmode;

	/* in case of auto line break, we do no support auto width  yet */
	if( !(pmode & NO_AUTO_LINEBREAK))
		m_nMode = m_nMode & ~AUTO_WIDTH; /* delete any AUTO_WIDTH*/

	//TRACE(" CTextBox::m_cText: %d, m_nMode %d\t\r\n",m_cText.size(),m_nMode);

	m_textBackgroundColor 	= textBackgroundColor;
	m_nFontTextHeight 	= getFontTextHeight();

	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	//TRACE(" CTextBox::m_nFontTextHeight: %d\t\r\n",m_nFontTextHeight);
	
	//Initialise the window frames first and than refresh text line array
	initFramesAndTextArray();
}

CTextBox::CTextBox(const char * text)
{
	//TRACE("[CTextBox] new\r\n");
	initVar();

	if(text != NULL)
		m_cText = *text;
	
	//TRACE_1("[CTextBox] %s Line %d text: %s\r\n", __FUNCTION__, __LINE__, text);

	//Initialise the window frames first and than refresh text line array
	initFramesAndTextArray();
}

CTextBox::CTextBox()
{
	//TRACE("[CTextBox] new\r\n");
	initVar();
	
	//Initialise the window frames first and than refresh text line array
	initFramesAndTextArray();
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
	frameBuffer 	= NULL;
	
	m_showTextFrame = 0;
	m_nNrOfNewLine = 0;
	m_nMaxLineWidth = 0;

	m_cText	= "";
	m_nMode = SCROLL;

	m_FontUseDigitHeight	= false;
	m_pcFontText  		= g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1];
	m_nFontTextHeight 	= getFontTextHeight();
	m_nMaxTextWidth		= 0;

	m_nNrOfPages 		= 1;
	m_nNrOfLines 		= 0;
	m_nLinesPerPage 	= 0;
	m_nCurrentLine 		= 0;
	m_nCurrentPage 		= 0;
	text_border_width 	= 8;

	m_cFrame.iX		= g_settings.screen_StartX + ((g_settings.screen_EndX - g_settings.screen_StartX - MIN_WINDOW_WIDTH) >>1);
	m_cFrame.iWidth		= MIN_WINDOW_WIDTH;
	m_cFrame.iY		= g_settings.screen_StartY + ((g_settings.screen_EndY - g_settings.screen_StartY - MIN_WINDOW_HEIGHT) >>1);
	m_cFrame.iHeight	= MIN_WINDOW_HEIGHT;

	m_nMaxHeight 		= MAX_WINDOW_HEIGHT;
	m_nMaxWidth 		= MAX_WINDOW_WIDTH;
	m_nMinHeight		= MIN_WINDOW_HEIGHT;
	m_nMinWidth		= MIN_WINDOW_WIDTH;

	m_textBackgroundColor 	= COL_MENUCONTENT_PLUS_0;
	m_textColor		= COL_MENUCONTENT;
	m_nPaintBackground 	= true;
	m_nBgRadius		= 0;
	m_nBgRadiusType 	= CORNER_ALL;
	
	m_cLineArray.clear();

// 	max_width 		= 0;
}

void CTextBox::initFramesAndTextArray()
{
	/* Initialise the window frames first */
	initFramesRel();
	// than refresh text line array
	refreshTextLineArray();
}

int CTextBox::getFontTextHeight()
{
		if (m_FontUseDigitHeight)
			return m_pcFontText->getDigitHeight() + (m_pcFontText->getDigitOffset() * 18) / 10;
		else
			return m_pcFontText->getHeight();
}

void CTextBox::setFontUseDigitHeight(bool set/*=true*/)
{
	if (m_FontUseDigitHeight != set) {
		m_FontUseDigitHeight = set;
		m_nFontTextHeight = getFontTextHeight();
		initFramesAndTextArray();
	}
}

void CTextBox::setTextFont(Font* font_text)
{
	if ((m_pcFontText != font_text) && (font_text != NULL)) {
		m_pcFontText = font_text;
		m_nFontTextHeight = getFontTextHeight();
		//Initialise the window frames first and than refresh text line array
		initFramesAndTextArray();
	}
}

void CTextBox::setTextBorderWidth(int border)
{
	/* we need a minimal borderwith of 1px because the edge-smoothing
	(or fontrenderer?) otherwise will paint single pixels outside the
	defined area. e.g. 'j' is leaving such residues */
	text_border_width = (border > 0) ? border : 1;
	//Initialise the window frames first and than refresh text line array
	initFramesAndTextArray();
}

void CTextBox::setWindowMaxDimensions(const int width, const int height)
{
	m_nMaxHeight = height;
	m_nMaxWidth = width;
}

void CTextBox::setWindowMinDimensions(const int width, const int height)
{
	m_nMinHeight = height;
	m_nMinWidth = width;
}

void CTextBox::reSizeMainFrameWidth(int textWidth)
{
	//TRACE("[CTextBox]->%s: \ntext width: %d\n m_cFrame.iWidth: %d\n m_cFrameTextRel.iWidth: %d\n m_nMaxWidth: %d\n  m_nMinWidth: %d\n",__FUNCTION__, textWidth, m_cFrame.iWidth, m_cFrameTextRel.iWidth, m_nMaxWidth, m_nMinWidth);

	int iNewWindowWidth = textWidth  + m_cFrameScrollRel.iWidth   + 2*text_border_width;

	if( iNewWindowWidth > m_nMaxWidth)
		iNewWindowWidth = m_nMaxWidth;	
	
	if( iNewWindowWidth < m_nMinWidth)
		iNewWindowWidth = m_nMinWidth;
	

	m_cFrame.iWidth	= iNewWindowWidth;

	/* Re-Init the children frames due to new main window */
	initFramesRel();
}

void CTextBox::reSizeMainFrameHeight(int textHeight)
{
	//TRACE("[CTextBox]->ReSizeMainFrameHeight: %d, current: %d\r\n",textHeight,m_cFrameTextRel.iHeight);

	int iNewWindowHeight =	textHeight + 2*text_border_width;

	if( iNewWindowHeight > m_nMaxHeight)
		iNewWindowHeight = m_nMaxHeight;
	
	if( iNewWindowHeight < m_nMinHeight)
		iNewWindowHeight = m_nMinHeight;

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

	m_nLinesPerPage = std::max(1, (m_cFrameTextRel.iHeight - (2*text_border_width)) / m_nFontTextHeight);

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
	int loop 		= true;
	int pos_prev 		= 0;
	int pos 		= 0;
	int aktWidth 		= 0;
	int aktWordWidth 	= 0;
	int lineBreakWidth 	= 0;

	m_nNrOfNewLine 		= 0;

	std::string	aktLine = "";
	std::string	aktWord = "";

	/* clear current line vector */
	m_cLineArray.clear();
	m_nNrOfLines = 0;

	if( m_nMode & AUTO_WIDTH){
		/* In case of autowidth, we calculate the max allowed width of the textbox */
		lineBreakWidth = m_nMaxWidth - m_cFrameScrollRel.iWidth - 2*text_border_width;
	}else{
		/* If not autowidth, we just take the actuall textframe width */
		lineBreakWidth = std::max(m_nMaxWidth, m_cFrameTextRel.iWidth - 2*text_border_width);
	}
	
 	if(m_nMaxTextWidth)
 		lineBreakWidth = m_nMaxTextWidth;

	//TRACE("[CTextBox] line %d: lineBreakWidth %d\n", __LINE__, lineBreakWidth);

	int TextChars = m_cText.size();
	// do not parse, if text is empty
	if(TextChars == 0)
	{
		m_nNrOfPages = 0;
		m_nNrOfLines = 0;
		m_nCurrentPage = 0;
		m_nCurrentLine = 0;
		m_nLinesPerPage = 1;
	}
	else
	{
		while(loop)
		{
			//manage auto linebreak,
			if(m_nMode & NO_AUTO_LINEBREAK)
				pos = m_cText.find_first_of("\n",pos_prev);
			else
				pos = m_cText.find_first_of("\n/-. ",pos_prev);

			//TRACE_1("     pos: %d pos_prev: %d\r\n",pos,pos_prev);

			if(pos == -1)
			{
				pos = TextChars+1;
				loop = false; // note, this is not 100% correct. if the last characters does not fit in one line, the characters after are cut
				//TRACE_1(" Textend found\r\n");
			}

			//find current word between start pos and next pos (next possible \n)
			aktWord = m_cText.substr(pos_prev, pos - pos_prev + 1);

			//calculate length of current found word
			aktWordWidth = m_pcFontText->getRenderWidth(aktWord, true);

			//set next start pos
			pos_prev = pos + 1;
			//if(aktWord.find("&quot;") == )
			if(1)
			{
				//TRACE_1("     aktWord: >%s< pos:%d\r\n",aktWord.c_str(),pos);

				if( (aktWidth + aktWordWidth) > lineBreakWidth && !(m_nMode & NO_AUTO_LINEBREAK))
				{
					/* we need a new line before we can continue */
					m_cLineArray.push_back(aktLine);
					//TRACE_1("  end line: %s\r\n", aktLine.c_str());
					m_nNrOfLines++;
					aktLine = "";
					aktWidth = 0;

					if(pos_prev >= TextChars)
						loop = false;
				}

				//add current word to current line
				aktLine  += aktWord;
				//set current line width 
				aktWidth += aktWordWidth;
				
				//set max text width, if required
				if (aktWidth > m_nMaxTextWidth)
					m_nMaxTextWidth = aktWidth;
				
				//TRACE_1("     aktLine : %s\r\n",aktLine.c_str());
				//TRACE_1("     aktWidth: %d aktWordWidth:%d\r\n",aktWidth,aktWordWidth);

				if( ((pos < TextChars) && (m_cText[pos] == '\n')) || loop == false)
				{
					// current line ends with an carriage return, make new line
					if ((pos < TextChars) && (m_cText[pos] == '\n'))
						aktLine.erase(aktLine.size() - 1,1);
					m_cLineArray.push_back(aktLine);
					m_nNrOfLines++;
					aktLine = "";
					aktWidth = 0;
					m_nNrOfNewLine++;
					
					if(pos_prev >= TextChars)
						loop = false;
				}
			}
		}


		/* check if we have to recalculate the window frame size, due to auto width and auto height */
		if( m_nMode & AUTO_WIDTH)
		{
 			reSizeMainFrameWidth(m_nMaxTextWidth);
		}

		if(m_nMode & AUTO_HIGH)
		{
			reSizeMainFrameHeight(m_nNrOfLines * m_nFontTextHeight);
		}

		m_nLinesPerPage = std::max(1, (m_cFrameTextRel.iHeight - (2*text_border_width)) / m_nFontTextHeight);
		m_nNrOfPages =	((m_nNrOfLines-1) / m_nLinesPerPage) + 1;

		if(m_nCurrentPage >= m_nNrOfPages)
		{
			m_nCurrentPage = m_nNrOfPages - 1;
			m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage;
		}
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
	if(!(m_nMode & SCROLL))
		return;
	
	if( frameBuffer == NULL)
		return;

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
	frameBuffer->blit();
}

void CTextBox::refreshText(void)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);

	if( frameBuffer == NULL)
		return;

	//TRACE("[CTextBox] m_nCurrentLine: %d, m_nNrOfLines %d, m_cLineArray[m_nCurrentLine]: %s\r\n",m_nCurrentLine, m_nNrOfLines, m_cLineArray[m_nCurrentLine].c_str());

	//Paint Text Background
	if (m_nPaintBackground)
		frameBuffer->paintBoxRel(m_cFrameTextRel.iX+m_cFrame.iX, /*m_cFrameTextRel.iY+*/m_cFrame.iY,
			m_cFrameTextRel.iWidth, m_cFrameTextRel.iHeight,  m_textBackgroundColor, m_nBgRadius, m_nBgRadiusType);
			
	if( m_nNrOfLines <= 0)
		return;

	
	int y = m_cFrameTextRel.iY + text_border_width;
	int i;
	int x_center = 0;

	// set text y position
	if (m_nMode & TOP)
		// move to top of frame
		y += m_nFontTextHeight + ((m_cFrameTextRel.iHeight - m_nFontTextHeight * m_nLinesPerPage) >> 1) - text_border_width;
	else if (m_nMode & BOTTOM)
		// move to bottom of frame
 		y += m_cFrameTextRel.iHeight - text_border_width - (m_nNrOfLines > 1 ? (m_nNrOfLines-1)*m_nFontTextHeight : 0) ;
		//m_nFontTextHeight + text_border_width /*- ((m_cFrameTextRel.iHeight + m_nFontTextHeight*/ * m_nLinesPerPage/*) >> 1)*/;
	else
		// fit into mid of frame space
		y += m_nFontTextHeight + ((m_cFrameTextRel.iHeight - m_nFontTextHeight * std::min(m_nLinesPerPage, m_nNrOfLines)) >> 1) - text_border_width;

	for(i = m_nCurrentLine; i < m_nNrOfLines && i < m_nCurrentLine + m_nLinesPerPage; i++)
	{
		
		//calculate centered xpos
		if( m_nMode & CENTER ){
			x_center = (m_cFrameTextRel.iWidth - m_pcFontText->getRenderWidth(m_cLineArray[i], true))>>1;
		}
		else if ( m_nMode & RIGHT ){
			x_center = (m_cFrameTextRel.iWidth - m_pcFontText->getRenderWidth(m_cLineArray[i], true));
			if ( m_nMode & SCROLL )
				x_center -= SCROLL_FRAME_WIDTH;
		}
		
		//TRACE("[CTextBox] %s Line %d m_cFrame.iX %d m_cFrameTextRel.iX %d\r\n", __FUNCTION__, __LINE__, m_cFrame.iX, m_cFrameTextRel.iX);
		m_pcFontText->RenderString(m_cFrame.iX + m_cFrameTextRel.iX + text_border_width + x_center,
				y+m_cFrame.iY, m_cFrameTextRel.iWidth, m_cLineArray[i].c_str(),
				m_textColor, 0, true); // UTF-8
		y += m_nFontTextHeight;
	}
	frameBuffer->blit();
}

void CTextBox::scrollPageDown(const int pages)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	if( !(m_nMode & SCROLL))
		return;
	
	if( m_nNrOfLines <= 0)
		return;

	if(m_nCurrentPage + pages < m_nNrOfPages)
	{
		m_nCurrentPage += pages;
	}
	else
	{
		m_nCurrentPage = m_nNrOfPages - 1;
	}
	int oldCurrentLine = m_nCurrentLine;
	m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage;
	if (oldCurrentLine != m_nCurrentLine)
		refresh();
}

void CTextBox::scrollPageUp(const int pages)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	if( !(m_nMode & SCROLL))
		return;
	
	if( m_nNrOfLines <= 0)
		return;

	if(m_nCurrentPage - pages > 0)
	{
		m_nCurrentPage -= pages;
	}
	else
	{
		m_nCurrentPage = 0;
	}
	int oldCurrentLine = m_nCurrentLine;
	m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage;
	if (oldCurrentLine != m_nCurrentLine)
		refresh();
}

void CTextBox::refresh(void)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	if( frameBuffer == NULL)
		return;

	//Paint text
	refreshScroll();
	refreshText();
}


bool CTextBox::setText(const std::string* newText, int max_width)
{
	//TRACE("[CTextBox]->SetText \r\n");
	bool result = false;
	m_nMaxTextWidth = max_width;
	
//printf("setText: _max_width %d max_width %d\n", _max_width, max_width);
	if (newText != NULL)
	{
		m_cText = *newText;
		//m_cText = *newText + "\n"; //FIXME test
		reSizeMainFrameHeight(m_cFrame.iHeight);
		//refresh text line array
		refreshTextLineArray();
		refresh();
		result = true;
	}
	return(result);
}

void CTextBox::paint (void)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
#if 0
	TRACE("  Mode: ");
	if(m_nMode & SCROLL)		TRACE("SCROLL ");
	if(m_nMode & NO_AUTO_LINEBREAK)	TRACE("NO_AUTO_LINEBREAK ");
	if(m_nMode & AUTO_WIDTH)	TRACE("AUTO_WIDTH ");
	if(m_nMode & AUTO_HIGH)		TRACE("AUTO_HIGH ");
	if(m_nMode & CENTER)		TRACE("CENTER ");
	if(m_nMode & RIGHT)		TRACE("RIGHT ");
	if(m_nMode & TOP)		TRACE("TOP ");
	if(m_nMode & BOTTOM)		TRACE("BOTTOM ");
	TRACE("\r\n");

#endif
	if(frameBuffer != NULL)
		return;

	frameBuffer = CFrameBuffer::getInstance();
	refresh();
}

void CTextBox::hide (void)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	if(frameBuffer == NULL)
		return;
	
	if (m_nPaintBackground)
		frameBuffer->paintBackgroundBoxRel(m_cFrame.iX, m_cFrame.iY, m_cFrame.iWidth, m_cFrame.iHeight);
	frameBuffer->blit();
	
	frameBuffer = NULL;
}
