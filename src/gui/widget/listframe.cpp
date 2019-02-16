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

	Module Name: listframe.cpp .

	Description: Implementation of the CListFrame class
				 This class provides a plain Listbox with numerous rows and lines.

	Date:	 Nov 2005

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

#include <stdlib.h>
#include <gui/color.h>
#include "listframe.h"
#include <gui/components/cc.h>
#include <gui/widget/icons.h>
#include <driver/fontrenderer.h>

#define MAX_WINDOW_WIDTH  (frameBuffer ? frameBuffer->getScreenWidth() - 40:0)
#define MAX_WINDOW_HEIGHT (frameBuffer ? frameBuffer->getScreenHeight() - 40:0)

#define MIN_WINDOW_WIDTH  (frameBuffer ? frameBuffer->getScreenWidth() >> 1:0)
#define MIN_WINDOW_HEIGHT 40

#define TITLE_BACKGROUND_COLOR COL_MENUHEAD_PLUS_0
#define TITLE_FONT_COLOR COL_MENUHEAD_TEXT

#define HEADER_LIST_BACKGROUND_COLOR COL_MENUCONTENT_PLUS_0
#define HEADER_LIST_FONT_COLOR COL_MENUCONTENTDARK_TEXT_PLUS_2

#define FONT_LIST g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]
#define FONT_HEADER_LIST g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]
#define FONT_TITLE g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CListFrame::CListFrame(	LF_LINES* lines, Font* font_text, const int pmode,
		const CBox* position, const char* textTitle, Font* font_title)
{
	//TRACE("[CListFrame] new\r\n");
	initVar();

	if(lines != NULL) {
		m_pLines = lines;
		m_nNrOfRows = lines->rows;
		if(m_nNrOfRows > LF_MAX_ROWS)
			m_nNrOfRows = LF_MAX_ROWS;
	}
	if(font_text != NULL)	m_pcFontList = font_text;
	if(font_text != NULL)	m_pcFontHeaderList = font_text;
	if(position != NULL) {
		m_cFrame	= *position;
		m_nMaxHeight = m_cFrame.iHeight;
		m_nMaxWidth = m_cFrame.iWidth;
	}

	m_nMode	= pmode;

#if 0
	TRACE("  Mode: ");
	if(pmode & SCROLL) TRACE("SCROLL ");
	if(pmode & AUTO_WIDTH) TRACE("AUTO_WIDTH ");
	if(pmode & AUTO_HIGH) TRACE("AUTO_HIGH");
	TRACE("\r\n");
#endif

	if(font_title != NULL)
		m_pcFontTitle = font_title;

	if( textTitle != NULL)
		m_textTitle = textTitle;

	m_nFontListHeight  = m_pcFontList->getHeight();
	m_nFontHeaderListHeight = m_pcFontHeaderList->getHeight();
	m_nFontTitleHeight = m_pcFontTitle->getHeight();

	//TRACE(" CListFrame::m_nFontTextHeight: %d\t\r\n",m_nFontListHeight);

	/* Initialise the window frames first */
	initFramesRel();

	// than refresh text line array
	onNewLineArray();
}

CListFrame::CListFrame(	LF_LINES* lines)
{
	//TRACE("[CListFrame] new\r\n");
	initVar();

	if(lines != NULL)
	{
		m_pLines = lines;
		m_nNrOfRows = lines->rows;
		if(m_nNrOfRows > LF_MAX_ROWS)
			m_nNrOfRows = LF_MAX_ROWS;
	}

	/* Initialise the window frames first */
	initFramesRel();

	// than refresh text line array
	onNewLineArray();
}

CListFrame::CListFrame()
{
	//TRACE("[CListFrame] new\r\n");
	initVar();
	initFramesRel();

	m_pLines = NULL;
}

CListFrame::~CListFrame()
{
	//TRACE("[CListFrame] del\r\n");
	hide();
}

void CListFrame::initVar(void)
{
	frameBuffer = CFrameBuffer::getInstance();
	//TRACE("[CListFrame]->InitVar\r\n");
	m_nMode = SCROLL;

	m_showSelection = true;

	m_pcFontList  =  FONT_LIST ;
	m_nFontListHeight = m_pcFontList->getHeight();

	m_pcFontHeaderList  =  FONT_HEADER_LIST ;
	m_nFontHeaderListHeight = m_pcFontHeaderList->getHeight();

	m_pcFontTitle = FONT_TITLE;
	m_textTitle = "MovieBrowser";
	m_nFontTitleHeight = m_pcFontTitle->getHeight();

	m_nNrOfPages = 1;
	m_nNrOfLines = 0;
	m_nNrOfRows = 1;
	m_nLinesPerPage = 0;
	m_nCurrentLine = 0;
	m_nCurrentPage = 0;
	m_nSelectedLine = 0;

	m_nBgRadius = RADIUS_NONE;

	m_cFrame.iX		= frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - MIN_WINDOW_WIDTH) >>1);
	m_cFrame.iY		= frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - MIN_WINDOW_HEIGHT) >>1);
	m_cFrame.iWidth		= MIN_WINDOW_WIDTH;
	m_cFrame.iHeight	= MIN_WINDOW_HEIGHT;

	m_nMaxHeight = MAX_WINDOW_HEIGHT;
	m_nMaxWidth = MAX_WINDOW_WIDTH;
	frameBuffer = NULL;
}

void CListFrame::reSizeMainFrameWidth(int textWidth)
{
	//TRACE("[CListFrame]->ReSizeMainFrameWidth: %d, current: %d\r\n",textWidth,m_cFrameListRel.iWidth);

	int iNewWindowWidth =	textWidth  + m_cFrameScrollRel.iWidth   + 2*OFFSET_INNER_MID;

	if(iNewWindowWidth > m_nMaxWidth) iNewWindowWidth = m_nMaxWidth;
	if((unsigned int) iNewWindowWidth < MIN_WINDOW_WIDTH) iNewWindowWidth = MIN_WINDOW_WIDTH;

	m_cFrame.iWidth	= iNewWindowWidth;

	/* Re-Init the children frames due to new main window */
	initFramesRel();
}

void CListFrame::reSizeMainFrameHeight(int textHeight)
{
	//TRACE("[CListFrame]->ReSizeMainFrameHeight: %d, current: %d\r\n",textHeight,m_cFrameListRel.iHeight);

	int iNewWindowHeight =	textHeight + 2*OFFSET_INNER_MID;

	if( iNewWindowHeight > m_nMaxHeight) iNewWindowHeight = m_nMaxHeight;
	if( iNewWindowHeight < MIN_WINDOW_HEIGHT) iNewWindowHeight = MIN_WINDOW_HEIGHT;

	m_cFrame.iHeight	= iNewWindowHeight;

	/* Re-Init the children frames due to new main window */
	initFramesRel();
}

void CListFrame::initFramesRel(void)
{
	//TRACE("-[CListFrame]->InitFramesRel\r\n");

	if(m_nMode & TITLE)
	{
		m_cFrameTitleRel.iX		= 0;
		m_cFrameTitleRel.iY		= 0;
		m_cFrameTitleRel.iHeight	= m_nFontTitleHeight + 2*OFFSET_INNER_MIN;
		m_cFrameTitleRel.iWidth		= m_cFrame.iWidth;
	}
	else
	{
		m_cFrameTitleRel.iX		= 0;
		m_cFrameTitleRel.iY		= 0;
		m_cFrameTitleRel.iHeight	= 0;
		m_cFrameTitleRel.iWidth		= 0;
	}

	if(m_nMode & HEADER_LINE)
	{
		m_cFrameHeaderListRel.iX	= 0;
		m_cFrameHeaderListRel.iY	= m_cFrameTitleRel.iHeight;
		m_cFrameHeaderListRel.iHeight	= m_nFontHeaderListHeight + 2*OFFSET_INNER_MIN;
	}
	else
	{
		m_cFrameHeaderListRel.iX	= 0;
		m_cFrameHeaderListRel.iY	= 0;
		m_cFrameHeaderListRel.iHeight	= 0;
		m_cFrameHeaderListRel.iWidth	= 0;
	}

	m_cFrameListRel.iX		= 0;
	m_cFrameListRel.iY		= m_cFrameHeaderListRel.iHeight + m_cFrameTitleRel.iHeight;
	m_cFrameListRel.iHeight		= m_cFrame.iHeight - m_cFrameHeaderListRel.iHeight - m_cFrameTitleRel.iHeight;

	if(m_nMode & SCROLL)
	{
		m_cFrameScrollRel.iX		= m_cFrame.iWidth - SCROLLBAR_WIDTH;
		m_cFrameScrollRel.iY		= m_cFrameTitleRel.iHeight + m_cFrameHeaderListRel.iHeight;
		m_cFrameScrollRel.iWidth	= SCROLLBAR_WIDTH;
		m_cFrameScrollRel.iHeight	= m_cFrameListRel.iHeight - m_nBgRadius;
	}
	else
	{
		m_cFrameScrollRel.iX		= 0;
		m_cFrameScrollRel.iY		= 0;
		m_cFrameScrollRel.iWidth	= 0;
		m_cFrameScrollRel.iHeight	= 0;
	}

	m_cFrameListRel.iWidth		= m_cFrame.iWidth - m_cFrameScrollRel.iWidth;

	if(m_nMode & HEADER_LINE)
	{
		m_cFrameHeaderListRel.iWidth	= m_cFrame.iWidth;
	}

	m_nLinesPerPage = m_cFrameListRel.iHeight / m_nFontListHeight;
}

void CListFrame::onNewLineArray(void)
{
	//TRACE("[CListFrame]->onNewLineArray \r\n");
	int maxTextWidth = 0;

	maxTextWidth = 300; // TODO
	m_nNrOfLines = m_pLines->lineArray[0].size();
	if(m_nNrOfLines > 0 )
	{
		/* check if we have to recalculate the window frame size, due to auto width and auto height */
		if( m_nMode & AUTO_WIDTH)
		{
			reSizeMainFrameWidth(maxTextWidth);
		}

		if(m_nMode & AUTO_HIGH)
		{
			reSizeMainFrameHeight(m_nNrOfLines * m_nFontListHeight);
		}
		m_nLinesPerPage = (m_cFrameListRel.iHeight - (2*OFFSET_INNER_MID)) / m_nFontListHeight;

		if(m_nLinesPerPage <= 0)
			m_nLinesPerPage = 1;

		m_nNrOfPages =	((m_nNrOfLines-1) / m_nLinesPerPage) + 1;

		if(m_nCurrentLine >= m_nNrOfLines)
		{
			m_nCurrentPage = m_nNrOfPages - 1;
			if (m_nCurrentPage < 0)
				m_nCurrentPage = 0;
			m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage;
		}
		if(m_nSelectedLine >= m_nNrOfLines)
		{
			m_nSelectedLine = m_nCurrentLine;
		}
	}
	else
	{
		m_nNrOfLines = 0;
		m_nCurrentLine = 0;
		m_nSelectedLine = 0;
		m_nLinesPerPage = 1;
		m_nNrOfPages = 1;
	}

//	TRACE_1(" m_nNrOfPages:     %d\r\n",m_nNrOfPages);
//	TRACE_1(" m_nNrOfLines:     %d\r\n",m_nNrOfLines);
//	TRACE_1(" maxTextWidth:  %d\r\n",maxTextWidth);
//	TRACE_1(" m_nLinesPerPage:  %d\r\n",m_nLinesPerPage);
//	TRACE_1(" m_nFontTextHeight:%d\r\n",m_nFontListHeight);
	//TRACE_1(" m_nCurrentPage:   %d\r\n",m_nCurrentPage);
	//TRACE_1(" m_nCurrentLine:   %d\r\n",m_nCurrentLine);
}

void CListFrame::refreshTitle(void)
{
	//TRACE("[CListFrame]->refreshHeaderList \r\n");
	if( frameBuffer == NULL) return;

	frameBuffer->paintBoxRel(m_cFrameTitleRel.iX+m_cFrame.iX, m_cFrameTitleRel.iY+m_cFrame.iY,
			m_cFrameTitleRel.iWidth, m_cFrameTitleRel.iHeight, TITLE_BACKGROUND_COLOR,
			m_nBgRadius, CORNER_TOP);

	m_pcFontTitle->RenderString(m_cFrameTitleRel.iX + OFFSET_INNER_MID + m_cFrame.iX,
			m_cFrameTitleRel.iY + m_cFrameTitleRel.iHeight - OFFSET_INNER_MIN + m_cFrame.iY,
			m_cFrameTitleRel.iWidth - 2*OFFSET_INNER_MID,
			m_textTitle.c_str(), TITLE_FONT_COLOR);
}

void CListFrame::refreshScroll(void)
{
	//TRACE("[CListFrame]->refreshScroll\r\n");
	if( frameBuffer == NULL) return;
	if(!(m_nMode & SCROLL)) return;

	/*
	   FIXME: Find right conditions.
	   So long let's paint scrollbar background in every case
	   to avoid transparent spaces in scrollbar corners.
	*/
	if (1)
	{
		frameBuffer->paintBoxRel(m_cFrameScrollRel.iX+m_cFrame.iX, m_cFrameScrollRel.iY+m_cFrame.iY,
				m_cFrameScrollRel.iWidth, m_cFrameHeaderListRel.iHeight, COL_MENUCONTENT_PLUS_0,
				m_nBgRadius, CORNER_BOTTOM_RIGHT);
	}

	if (m_nNrOfPages > 1)
	{
		paintScrollBar(m_cFrameScrollRel.iX+m_cFrame.iX, m_cFrameScrollRel.iY+m_cFrame.iY,
				m_cFrameScrollRel.iWidth, m_cFrameScrollRel.iHeight,
				m_nNrOfPages, m_nCurrentPage);
	}
}

int CListFrame::paintListIcon(int x, int y, int line)
{
	int xDiff = 0;
	if ((!m_pLines->Icon.empty()) && (!m_pLines->Icon[line].empty())) {
		int icol_w, icol_h;
		frameBuffer->getIconSize(m_pLines->Icon[line].c_str(), &icol_w, &icol_h);
		if ((icol_w > 0) && (icol_h > 0)) {
			frameBuffer->paintIcon(m_pLines->Icon[line], x+m_cFrame.iX, y+m_cFrame.iY-m_nFontListHeight, m_nFontListHeight);
			xDiff = icol_w + OFFSET_INNER_MID;
		}
	}
	return xDiff;
}

void CListFrame::refreshList(void)
{
	//TRACE("[CListFrame]->refreshList: %d\r\n",m_nCurrentLine);
	if (frameBuffer == NULL)
		return;

	frameBuffer->paintBoxRel(m_cFrameListRel.iX+m_cFrame.iX, m_cFrameListRel.iY+m_cFrame.iY,
			m_cFrameListRel.iWidth, m_cFrameListRel.iHeight, COL_MENUCONTENT_PLUS_0,
			m_nBgRadius, CORNER_BOTTOM_LEFT);

	if (m_nNrOfLines <= 0)
		return;

	for (int line = m_nCurrentLine; line < m_nNrOfLines && line < m_nCurrentLine + m_nLinesPerPage; line++)
	{
		refreshLine(line);
	}
}

void CListFrame::refreshLine(int line)
{
	if( frameBuffer == NULL) return;
	if( m_nNrOfLines <= 0) return;

	//TRACE("[CListFrame]->refreshLine: %d\r\n", line);
	if((line < m_nCurrentLine) && (line > m_nCurrentLine + m_nLinesPerPage))
		return;

	fb_pixel_t color, bgcolor;
	int rel_line = line - m_nCurrentLine;
	int y = m_cFrameListRel.iY + (rel_line*m_nFontListHeight);
	int radius = 0;

	bool selected = (line == m_nSelectedLine && m_showSelection == true);
	bool marked = (!m_pLines->marked.empty() && m_pLines->marked[line]);

	getItemColors(color, bgcolor, selected, marked);

	if (selected || marked)
		radius = RADIUS_LARGE;

	frameBuffer->paintBoxRel(m_cFrameListRel.iX+m_cFrame.iX, y+m_cFrame.iY,
			m_cFrameListRel.iWidth, m_nFontListHeight, bgcolor, radius);

	int width;
	int x = m_cFrameListRel.iX + OFFSET_INNER_MID;
	y += m_nFontListHeight;

	int xDiff = paintListIcon(x, y, line);

	int net_width = m_cFrameListRel.iWidth - OFFSET_INNER_SMALL * (m_pLines->rows - 1);
	for(int row = 0; row < m_pLines->rows; row++)
	{
		width = std::min(m_pLines->rowWidth[row] * net_width / 100,
				m_cFrameListRel.iWidth - x + m_cFrameListRel.iX - OFFSET_INNER_MID);
		if (row > 0)
			xDiff = 0;
		m_pcFontList->RenderString(x+m_cFrame.iX+xDiff, y+m_cFrame.iY,
				width-xDiff, m_pLines->lineArray[row][line].c_str(),
				color);
		x += width + OFFSET_INNER_SMALL;
	}
}

void CListFrame::refreshHeaderList(void)
{
	//TRACE("[CListFrame]->refreshHeaderList \r\n");
	if( frameBuffer == NULL) return;
	if(!(m_nMode & HEADER_LINE))return;

	frameBuffer->paintBoxRel(m_cFrameHeaderListRel.iX+m_cFrame.iX, m_cFrameHeaderListRel.iY+m_cFrame.iY,
			m_cFrameHeaderListRel.iWidth, m_cFrameHeaderListRel.iHeight, HEADER_LIST_BACKGROUND_COLOR);

	int width;
	int x = m_cFrameHeaderListRel.iX + OFFSET_INNER_MID;
	int y = m_cFrameHeaderListRel.iY + m_nFontHeaderListHeight + OFFSET_INNER_MIN;
	int net_width = m_cFrameHeaderListRel.iWidth - OFFSET_INNER_SMALL * (m_pLines->rows - 1);
	net_width -= m_cFrameScrollRel.iWidth;
	bool loop = true;
	for(int row = 0; row < m_pLines->rows && loop == true; row++)
	{
		width = m_pLines->rowWidth[row] * net_width / 100;
		if (width > m_cFrameHeaderListRel.iWidth - x + m_cFrameHeaderListRel.iX - OFFSET_INNER_MID)
		{
			width = m_cFrameHeaderListRel.iWidth - x + m_cFrameHeaderListRel.iX - OFFSET_INNER_MID;
			//TRACE("   normalize width to %d , x:%d \r\n",width,x);
			loop = false;
		}
		m_pcFontHeaderList->RenderString(x+m_cFrame.iX, y+m_cFrame.iY,
				width, m_pLines->lineHeader[row].c_str(),
				HEADER_LIST_FONT_COLOR);
		x += width + OFFSET_INNER_SMALL;
	}
}

void CListFrame::scrollLineDown(const int lines)
{
	//TRACE("[CListFrame]->scrollLineDown \r\n");

	if( !(m_nMode & SCROLL)) return;
	if( m_nNrOfLines <= 1) return;

	setSelectedLine((m_nSelectedLine + lines) % m_nNrOfLines);
}

void CListFrame::scrollLineUp(const int lines)
{
	//TRACE("[CListFrame]->scrollLineUp \r\n");
	if( !(m_nMode & SCROLL)) return;
	if( m_nNrOfLines <= 1) return;

	setSelectedLine((m_nSelectedLine - lines + m_nNrOfLines) % m_nNrOfLines);
}

void CListFrame::scrollPageDown(const int pages)
{
	//TRACE("[CListFrame]->ScrollPageDown \r\n");

	if( !(m_nMode & SCROLL)) return;
	if( m_nNrOfLines <= 1) return;

	if(m_nCurrentPage + pages < m_nNrOfPages)
		setSelectedLine(m_nSelectedLine + pages * m_nLinesPerPage);
	else if (m_nSelectedLine == m_nNrOfLines - 1)
		setSelectedLine(0);
	else
		setSelectedLine(m_nNrOfLines - 1);
	//TRACE("[CListFrame]  m_nCurrentLine: %d, m_nCurrentPage: %d \r\n",m_nCurrentLine,m_nCurrentPage);
}

void CListFrame::scrollPageUp(const int pages)
{
	//TRACE("[CListFrame]->ScrollPageUp \r\n");

	if( !(m_nMode & SCROLL)) return;
	if( m_nNrOfLines <= 1) return;

	if(m_nCurrentPage - pages >= 0)
		setSelectedLine(m_nSelectedLine - pages * m_nLinesPerPage);
	else if (m_nSelectedLine == 0)
		setSelectedLine(m_nNrOfLines - 1);
	else
		setSelectedLine(0);

	//TRACE("[CListFrame]  m_nCurrentLine: %d, m_nCurrentPage: %d \r\n",m_nCurrentLine,m_nCurrentPage);
}

void CListFrame::refresh(void)
{
	//TRACE("[CListFrame]->Refresh\r\n");
	if( frameBuffer == NULL) return;

	refreshTitle();
	refreshScroll();
	refreshHeaderList();
	refreshList();
}

bool CListFrame::setLines(LF_LINES* lines)
{
	if(lines == NULL)
		return(false);
	//TRACE("[CListFrame]->setLines \r\n");

	m_pLines = lines;
	m_nNrOfRows = lines->rows;

	if(m_nNrOfRows > LF_MAX_ROWS)
		m_nNrOfRows = LF_MAX_ROWS;
	onNewLineArray();
	refresh();

	return(true);
}

bool CListFrame::setTitle(char* title)
{
	//TRACE("[CListFrame]->setTitle \r\n");
	if(!(m_nMode & TITLE)) return(false);
	if(title == NULL) return(false);

	m_textTitle = title;
	refreshTitle();

	return(true);
}

bool CListFrame::setSelectedLine(int selection)
{
	//TRACE("[CListFrame]->setSelectedLine %d \r\n",selection);
	bool result = false;
	if (selection >= m_nNrOfLines)
		selection = m_nNrOfLines - 1;
	if (selection < 0)
		selection = 0;

	int old_page = m_nCurrentPage;
	int old_selected = m_nSelectedLine;
	m_nSelectedLine = selection;
	m_nCurrentPage =  selection / m_nLinesPerPage;
	m_nCurrentLine = m_nCurrentPage * m_nLinesPerPage;
	if (m_nCurrentPage != old_page) {
		refreshList();
		refreshScroll();
	} else {
		refreshLine(old_selected);
		refreshLine(m_nSelectedLine);
	}

	result = true;
	//TRACE(" selected line: %d,%d,%d \r\n",m_nSelectedLine,m_nCurrentPage,m_nCurrentLine);

	return (result);
}

void CListFrame::setSelectedMarked(bool enable)
{
	if (!m_pLines->marked.empty() && m_nSelectedLine < (int) m_pLines->marked.size()) {
		m_pLines->marked[m_nSelectedLine] = enable;
		refreshLine(m_nSelectedLine);
	}
}

void CListFrame::hide(void)
{
	if(frameBuffer == NULL) return;
	TRACE("[CListFrame]->hide %s\n", m_textTitle.c_str());

	frameBuffer->paintBackgroundBoxRel(m_cFrame.iX, m_cFrame.iY, m_cFrame.iWidth, m_cFrame.iHeight);
	frameBuffer = NULL;
}

void CListFrame::paint(void)
{
	TRACE("[CListFrame]->paint %s\n", m_textTitle.c_str());
	frameBuffer = CFrameBuffer::getInstance();
	refresh();
}
