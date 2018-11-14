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

//#define VISUAL_DEBUG

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <system/debug.h>
#include <gui/color.h>
#include "textbox.h"
#include <gui/components/cc.h>
#include <gui/widget/icons.h>
#include <driver/fontrenderer.h>
#ifdef VISUAL_DEBUG
#include <gui/color_custom.h>
#endif
#include <sstream>

#define	SCROLL_FRAME_WIDTH	SCROLLBAR_WIDTH
#define	SCROLL_MARKER_BORDER	OFFSET_INNER_MIN

#define MAX_WINDOW_WIDTH  (g_settings.screen_EndX - g_settings.screen_StartX - CFrameBuffer::getInstance()->scale2Res(40))
#define MAX_WINDOW_HEIGHT (g_settings.screen_EndY - g_settings.screen_StartY - CFrameBuffer::getInstance()->scale2Res(40))

#define MIN_WINDOW_WIDTH  ((g_settings.screen_EndX - g_settings.screen_StartX)>>1)
#define MIN_WINDOW_HEIGHT CFrameBuffer::getInstance()->scale2Res(40)

CTextBox::CTextBox(const char * text, Font* font_text, const int pmode,
		   const CBox* position, CFBWindow::color_t textBackgroundColor)
{
	//TRACE("[CTextBox] new %d\n", __LINE__);
	initVar();

	if(text != NULL)
		m_cText = m_old_cText = text;
	
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

	m_textBackgroundColor 	= m_old_textBackgroundColor = textBackgroundColor;
	m_nFontTextHeight 	= getFontTextHeight();

	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	//TRACE(" CTextBox::m_nFontTextHeight: %d\t\r\n",m_nFontTextHeight);

	//Initialise the window frames first and than refresh text line array
	initFramesAndTextArray();
}

CTextBox::CTextBox(const char * text)
{
	//TRACE("[CTextBox] new %d\r", __LINE__);
	initVar();

	if(text != NULL)
		m_cText = m_old_cText = *text;

	//TRACE_1("[CTextBox] %s Line %d text: %s\r\n", __FUNCTION__, __LINE__, text);

	//Initialise the window frames first and than refresh text line array
	initFramesAndTextArray();
}

CTextBox::CTextBox()
{
	//TRACE("[CTextBox] new %d\n", __LINE__);
	initVar();

	//Initialise the window frames first and than refresh text line array
	initFramesAndTextArray();
}

CTextBox::~CTextBox()
{
	//TRACE("[CTextBox] del\r\n");
	m_cLineArray.clear();
	//hide();
	clearScreenBuffer();
}

void CTextBox::initVar(void)
{
	//TRACE("[CTextBox]->InitVar\r\n");
	frameBuffer 	= NULL;
	m_bgpixbuf	= NULL;

	m_showTextFrame = 0;
	m_nNrOfNewLine = 0;

	m_cText	= "";
	m_nMode = m_old_nMode 	= SCROLL;

	m_FontUseDigitHeight	= false;
	m_pcFontText  		= g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1];
	m_nFontTextHeight 	= getFontTextHeight();
	m_nMaxTextWidth		= 0;
	m_bg_painted		= false;

	m_nNrOfPages 		= 1;
	m_nNrOfLines 		= 0;
	m_nLinesPerPage 	= 0;
	m_nCurrentLine 		= 0;
	m_nCurrentPage 		= 0;
	text_Hborder_width 	= OFFSET_INNER_MID; //border left and right
	text_Vborder_width	= OFFSET_INNER_MID; //border top and buttom

	m_cFrame.iX		= m_old_x = g_settings.screen_StartX + ((g_settings.screen_EndX - g_settings.screen_StartX - MIN_WINDOW_WIDTH) >>1);
	m_cFrame.iWidth		= m_old_dx = MIN_WINDOW_WIDTH;
	m_cFrame.iY		= m_old_y = g_settings.screen_StartY + ((g_settings.screen_EndY - g_settings.screen_StartY - MIN_WINDOW_HEIGHT) >>1);
	m_cFrame.iHeight	= m_old_dy = MIN_WINDOW_HEIGHT;
	
	m_has_scrolled		= false;

	m_nMaxHeight 		= MAX_WINDOW_HEIGHT;
	m_nMaxWidth 		= MAX_WINDOW_WIDTH;
	m_nMinHeight		= MIN_WINDOW_HEIGHT;
	m_nMinWidth		= MIN_WINDOW_WIDTH;

	m_textBackgroundColor 	= m_old_textBackgroundColor = COL_MENUCONTENT_PLUS_0;
	m_textColor		= COL_MENUCONTENT_TEXT;
	m_old_textColor 	= 0;
	m_nPaintBackground 	= true;
	m_SaveScreen		= false;
	m_nBgRadius		= m_old_nBgRadius = 0;
	m_nBgRadiusType 	= m_old_nBgRadiusType = CORNER_NONE;

	m_cLineArray.clear();

	m_renderMode		= 0;
	m_utf8_encoded		= true;
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

void CTextBox::setTextBorderWidth(int Hborder, int Vborder)
{
	text_Hborder_width = Hborder;
	text_Vborder_width = Vborder;
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

	int iNewWindowWidth = textWidth + m_cFrameScrollRel.iWidth + 2*text_Hborder_width;

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

	int iNewWindowHeight =	textHeight + 2*text_Vborder_width;

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
	m_cFrameTextRel.iHeight		= m_cFrame.iHeight ;

	if(m_nMode & SCROLL)
	{
		m_cFrameScrollRel.iWidth	= SCROLL_FRAME_WIDTH;
		m_cFrameScrollRel.iX		= m_cFrame.iWidth - m_cFrameScrollRel.iWidth;
		m_cFrameScrollRel.iY		= m_cFrameTextRel.iY + m_nBgRadius;
		m_cFrameScrollRel.iHeight	= m_cFrameTextRel.iHeight - 2*m_nBgRadius;
	}
	else
	{
		m_cFrameScrollRel.iX		= 0;
		m_cFrameScrollRel.iY		= 0;
		m_cFrameScrollRel.iHeight	= 0;
		m_cFrameScrollRel.iWidth	= 0;
	}

	m_cFrameTextRel.iWidth	= m_cFrame.iWidth - m_cFrameScrollRel.iWidth;

	m_nLinesPerPage = std::max(1, (m_cFrameTextRel.iHeight - (2*text_Vborder_width)) / m_nFontTextHeight);

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

	int MaxWidth = m_nMaxWidth - m_cFrameScrollRel.iWidth - 2*text_Hborder_width;
	if( m_nMode & AUTO_WIDTH){
		/* In case of autowidth, we calculate the max allowed width of the textbox */
		lineBreakWidth = MaxWidth;
	}else{
		/* If not autowidth, we just take the actuall textframe width */
		lineBreakWidth = std::max(MaxWidth, m_cFrameTextRel.iWidth - 2*text_Hborder_width);
	}

	if(m_nMaxTextWidth)
		lineBreakWidth = m_nMaxTextWidth - 2*text_Hborder_width;

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
				pos = m_cText.find_first_of("\n", pos_prev);
			else if(m_nMode & AUTO_LINEBREAK_NO_BREAKCHARS)
				pos = m_cText.find_first_of("\n/ ", pos_prev);
			else
				pos = m_cText.find_first_of("\n/-. ", pos_prev);

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
			aktWordWidth = m_pcFontText->getRenderWidth(aktWord, m_utf8_encoded);

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

		m_nLinesPerPage = std::max(1, (m_cFrameTextRel.iHeight - (2*text_Vborder_width)) / m_nFontTextHeight);
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

	/*
	   FIXME: Find right conditions.
	   So long let's paint scrollbar background in every case
	   to avoid transparent spaces in scrollbar corners.
	*/
	if (1)
	{
		/*
		   Why we paint scrollbar background seperately?
		   So we have to reduce roundings from the left side of background.
		*/
		int BgRadiusType = CORNER_NONE;
		if (m_nBgRadiusType == CORNER_ALL)
			BgRadiusType = CORNER_RIGHT;
		else if (m_nBgRadiusType == CORNER_TOP)
			BgRadiusType = CORNER_TOP_RIGHT;
		else if (m_nBgRadiusType == CORNER_BOTTOM)
			BgRadiusType = CORNER_BOTTOM_RIGHT;

		frameBuffer->paintBoxRel(m_cFrameScrollRel.iX+m_cFrame.iX, m_cFrame.iY,
				m_cFrameScrollRel.iWidth, m_cFrame.iHeight,
				m_textBackgroundColor, m_nBgRadius, BgRadiusType);
	}

	if (m_nNrOfPages > 1)
	{
		paintScrollBar(m_cFrameScrollRel.iX+m_cFrame.iX, m_cFrameScrollRel.iY+m_cFrame.iY,
				m_cFrameScrollRel.iWidth, m_cFrameScrollRel.iHeight,
				m_nNrOfPages, m_nCurrentPage);
		m_has_scrolled = true;
	}
	else
		m_has_scrolled = false;
}

//evaluate comparsion between old and current properties WITHOUT text contents, return true if found changes
//first init is done in initVar() and reinit done in reInitToCompareVar()
bool CTextBox::hasChanged(int* x, int* y, int* dx, int* dy)
{
	if (	   hasChangedPos(x, y)
		|| hasChangedDim(dx, dy)
		|| m_old_textBackgroundColor != m_textBackgroundColor
		|| m_old_textColor != m_textColor
		|| m_old_nBgRadius != m_nBgRadius
		|| m_old_nBgRadiusType != m_nBgRadiusType
		|| m_old_nMode != m_nMode){
			return true;
	}
	return false;
}

bool CTextBox::hasChangedPos(int* x, int* y)
{
	return  (m_old_x != *x || m_old_y != *y);
}

bool CTextBox::hasChangedDim(int* dx, int* dy)
{
	return (m_old_dx != *dx || m_old_dy != *dy);
}

void CTextBox::reInitToCompareVar(int* x, int* y, int* dx, int* dy)
{
	m_old_x = *x;
	m_old_y = *y;
	m_old_dx = *dx;
	m_old_dy = *dy;
	m_old_textBackgroundColor = m_textBackgroundColor;
	m_old_textColor = m_textColor;
	m_old_nBgRadius = m_nBgRadius;
	m_old_nBgRadiusType = m_nBgRadiusType;
	m_old_nMode = m_nMode;
}

void CTextBox::refreshText(void)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);

	if( frameBuffer == NULL)
		return;

	//TRACE("[CTextBox] m_nCurrentLine: %d, m_nNrOfLines %d, m_cLineArray[m_nCurrentLine]: %s m_nBgRadius: %d\r\n",m_nCurrentLine, m_nNrOfLines, m_cLineArray[m_nCurrentLine].c_str(), m_nBgRadius, m_nBgRadiusType);

	//bg variables
	int ax = m_cFrameTextRel.iX+m_cFrame.iX;
	int ay = m_cFrameTextRel.iY+m_cFrame.iY;
	int dx = m_old_cText != m_cText || m_nNrOfPages>1 ? m_cFrameTextRel.iWidth : m_nMaxTextWidth - (m_nMode & SCROLL ? m_nBgRadius : 0);
	int dy = m_cFrameTextRel.iHeight;
	
	//avoid artefacts in transparent cornes
	/*
	   This happens, when text width is smaller then the radius width.
	*/
	dx = std::max(dx, 2*m_nBgRadius);

	//find changes
	bool has_changed = hasChanged(&ax, &ay, &dx, &dy);

	//clean up possible screen on any changes
	if (has_changed || m_bgpixbuf) {
		/*TODO/FIXME: in some cases could be required, that we must restore old saved screen. eg. if a text without bg was painted
		 * and another text should be painted as next on the same position like current text, but new text will be overpaint and is
		 * not visible. It's currently solvable only with appropriate order of text items
		*/
		if (has_changed)
			clearScreenBuffer();

		if(m_bgpixbuf && (m_old_cText != m_cText))
			frameBuffer->RestoreScreen(m_old_x, m_old_y, m_old_dx, m_old_dy, m_bgpixbuf);

		if (m_bgpixbuf){
			frameBuffer->RestoreScreen(m_old_x, m_old_y, m_old_dx, m_old_dy, m_bgpixbuf);
			clearScreenBuffer();
		}
	}

	//detect corrupt position values
	if ((ax<0) || (ay<0)){
		dprintf(DEBUG_NORMAL, "\033[33m[CTextBox] [%s - %d] ERROR! position out of range: ax = %d, ay = %d, dx = %d, dy = %d\033[0m\n", __func__, __LINE__, ax, ay, dx, dy);
		return;
	}

	//save screen only if no paint of background required
	if (!m_nPaintBackground && m_SaveScreen) {
		if (m_bgpixbuf == NULL){
			if ((dx * dy) >0){
// 				TRACE("[CTextBox]  [%s - %d] save bg for use as transparent background [%s]\n", __func__, __LINE__, m_cText.c_str());
				m_bgpixbuf= new fb_pixel_t[dx * dy];
				frameBuffer->SaveScreen(ax, ay, dx, dy, m_bgpixbuf);
			}
		}
	}

	//Paint Text Background
	bool allow_paint_bg = (m_old_cText != m_cText || has_changed || m_has_scrolled);
	if (m_nPaintBackground  && !m_SaveScreen){
		clearScreenBuffer();
		if (allow_paint_bg)
		{
			/*
			   Why we paint scrollbar background seperately?
			   So we have to reduce roundings from the right side of background.
			*/
			int BgRadiusType = CORNER_NONE;
			if (m_nBgRadiusType == CORNER_ALL)
				BgRadiusType = CORNER_LEFT;
			else if (m_nBgRadiusType == CORNER_TOP)
				BgRadiusType = CORNER_TOP_LEFT;
			else if (m_nBgRadiusType == CORNER_BOTTOM)
				BgRadiusType = CORNER_BOTTOM_LEFT;

			//TRACE("[CTextBox] %s paint bg %d\r\n", __FUNCTION__, __LINE__);
			//paint full background only on new text, otherwise paint required background
			frameBuffer->paintBoxRel(ax, ay, dx, dy, m_textBackgroundColor, m_nBgRadius, BgRadiusType);
			m_bg_painted = true;
		}
	}
	else{
		if (m_bgpixbuf){
			if (allow_paint_bg){
				//TRACE("[CTextBox] %s restore bg %d\r\n", __FUNCTION__, __LINE__);
				frameBuffer->RestoreScreen(ax, ay, dx, dy, m_bgpixbuf);
				m_bg_painted = true;
			}
		}
	}

	//save and reinit current comparsion properties
	if (has_changed){
		//TRACE("[CTextBox] %s set current values %d\r\n", __FUNCTION__, __LINE__);
		reInitToCompareVar(&ax, &ay, &dx, &dy);
	}
	m_has_scrolled = false;

	if( m_nNrOfLines <= 0)
		return;

	int y = m_cFrameTextRel.iY;
	int i;
	int x_center = 0;

	int lines = std::min(m_nLinesPerPage, m_nNrOfLines);

	// set text y position
	if (m_nMode & TOP)
		// move to top of frame
		y += m_nFontTextHeight + ((m_cFrameTextRel.iHeight - m_nFontTextHeight * m_nLinesPerPage) >> 1);
	else if (m_nMode & BOTTOM) {
		/* if BOTTOM && !SCROLL, show the last lines if more than one page worth of text is in cLineArray */
		if (!(m_nMode & SCROLL) && (m_nNrOfLines > m_nLinesPerPage))
			m_nCurrentLine = m_nNrOfLines - m_nLinesPerPage;
		// move to bottom of frame
		y += m_cFrameTextRel.iHeight - (lines > 1 ? (lines - 1)*m_nFontTextHeight : 0) - text_Vborder_width;
		//m_nFontTextHeight + text_Vborder_width /*- ((m_cFrameTextRel.iHeight + m_nFontTextHeight*/ * m_nLinesPerPage/*) >> 1)*/;
	} else
		// fit into mid of frame space
		y += m_nFontTextHeight + ((m_cFrameTextRel.iHeight - m_nFontTextHeight * lines) >> 1);

#ifdef VISUAL_DEBUG
	frameBuffer->paintBoxRel(m_cFrame.iX, m_cFrame.iY, m_cFrame.iWidth, m_cFrame.iHeight, COL_GREEN, m_nBgRadius, m_nBgRadiusType);
#endif

	for(i = m_nCurrentLine; i < m_nNrOfLines && i < m_nCurrentLine + m_nLinesPerPage; i++)
	{
		//calculate xpos
		if ((m_nMode & CENTER) || (m_nMode & RIGHT))
		{
			x_center = m_cFrameTextRel.iWidth - m_cFrameTextRel.iX - 2*text_Hborder_width - m_pcFontText->getRenderWidth(m_cLineArray[i], m_utf8_encoded);
			if (m_nMode & CENTER)
				x_center /= 2;
			if (m_nMode & SCROLL)
				x_center -= m_cFrameScrollRel.iWidth;
		}
		x_center = std::max(x_center, 0);

		int tx = m_cFrame.iX + m_cFrameTextRel.iX + text_Hborder_width + x_center;
		int ty = m_cFrame.iY + y;
		int tw = m_cFrameTextRel.iWidth - m_cFrameTextRel.iX - 2*text_Hborder_width - x_center;

#ifdef VISUAL_DEBUG
		int th = m_nFontTextHeight;
		frameBuffer->paintBoxRel(tx, ty-th, tw, th, COL_RED, m_nBgRadius, m_nBgRadiusType);
#endif
		//TRACE("[CTextBox] %s Line %d m_cFrame.iX %d m_cFrameTextRel.iX %d\r\n", __FUNCTION__, __LINE__, m_cFrame.iX, m_cFrameTextRel.iX);
		if (m_bg_painted || (m_old_cText != m_cText))
			m_pcFontText->RenderString(tx, ty, tw, m_cLineArray[i].c_str(), m_textColor, 0, m_renderMode | ((m_utf8_encoded) ? Font::IS_UTF8 : 0));
		y += m_nFontTextHeight;
	}
	m_old_cText = m_cText;
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
	OnAfterScrollPage();
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
	OnAfterScrollPage();
}

void CTextBox::refresh(void)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	if( frameBuffer == NULL)
		return;

	//Paint text
	refreshScroll();
	refreshText();
	OnAfterRefresh();
}


bool CTextBox::setText(const std::string* newText, int max_width, bool force_repaint)
{
	//TRACE("[CTextBox]->SetText \r\n");
	m_nMaxTextWidth = max_width;
	//reset text to force repaint the text, managed in hasChanged()
	if (force_repaint)
		m_old_cText = "";
//printf("setText: _max_width %d max_width %d\n", _max_width, max_width);
	if (newText){
		m_cText = *newText;
		if (m_old_cText != m_cText){
			//m_cText = *newText + "\n"; //FIXME test
			reSizeMainFrameHeight(m_cFrame.iHeight);
			//refresh text line array
			refreshTextLineArray();
			refresh();
			return true;
		}
		return false;
	}else
		return false;

	return true;
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

	frameBuffer = NULL;
}

void CTextBox::clear(void)
{
	//TRACE("[CTextBox] %s Line %d\r\n", __FUNCTION__, __LINE__);
	if (frameBuffer == NULL)
		return;

	std::string empty(" ");
	setText(&empty);
}

bool CTextBox::clearScreenBuffer()
{
	if(m_bgpixbuf){
		//TRACE("[CTextBox] %s destroy bg %d\r\n", __FUNCTION__, __LINE__);
		delete[] m_bgpixbuf;
		m_bgpixbuf = NULL;
		return true;
	}
	return false;
}

bool CTextBox::enableSaveScreen(bool mode)
{
	if (m_SaveScreen == mode)
		return false;

	if (!m_SaveScreen || m_SaveScreen != mode)
		clearScreenBuffer();

	m_SaveScreen = mode;

	return true;
}

int CTextBox::getLines(const std::string& text)
{
	if (text.empty())
		return 0;

	std::stringstream s (text);
	if (!s)
		return 0;

	int count = 0;
	std::string line;
	while(getline(s, line))
		count++;

	return count;
}

int CTextBox::getLines()
{
	if (m_cText.empty())
		return 0;

	refreshTextLineArray();

	return m_nNrOfLines;
}

int CTextBox::getMaxLineWidth(const std::string& text, Font* font)
{
	std::string txt = text;
	if (txt.find('\n', 0) == std::string::npos){
		/* If found no linebreak, return pure size with additional space char.
		* Space char simulates a line termination as a workaround to get
		* largest possible width.
		*/
		txt += ' ';
		return font->getRenderWidth(txt.c_str());
	}

	std::stringstream in (txt);
	if (!in)
		return 0;

	int len = 0;
	std::string line;
	while(getline(in, line))
		len = std::max(len, font->getRenderWidth(line.c_str()));

	return len;
}
