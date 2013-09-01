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

    Module Name: msgbox.cpp: .

	Description: Implementation of the CMsgBox class
				 This class provides a  message box using CTextBox.

  	Date:	Nov 2005

	Author: Gnther@tuxbox.berlios.org
		based on code of Steffen Hehn 'McClean'

	Revision History:
	Date			Author		Change Description
	   Nov 2005		Gnther	initial implementation
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>

#include "msgbox.h"

#include <gui/widget/icons.h>
#include <neutrino.h>

#define WINDOW_FRAME_BORDER_WIDTH	 4
#define ADD_FOOT_HEIGHT	 			20
#define	TEXT_BORDER_WIDTH			 8
#define	TITLE_ICON_WIDTH			(40 - TEXT_BORDER_WIDTH)

#define MAX_WINDOW_WIDTH  (g_settings.screen_EndX - g_settings.screen_StartX )
#define MAX_WINDOW_HEIGHT (g_settings.screen_EndY - g_settings.screen_StartY - 40)

#define MIN_WINDOW_WIDTH  (MAX_WINDOW_WIDTH>>1)
#define MIN_WINDOW_HEIGHT 40

#define DEFAULT_TITLE_FONT	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]
#define DEFAULT_FOOT_FONT	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Function Name:	CMsgBox
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
CMsgBox::CMsgBox(  const char * text,
				   Font* fontText,
				   const int _mode,
				   const CBox* position,
				   const char * title,
				   Font* fontTitle,
				   const char * icon,
				   int return_button ,
				   const result_ default_result)
{
	//TRACE("->CMsgBox::CMsgBox\r\n");
	initVar();

	if(title != NULL)		m_cTitle = title;
	if(fontTitle != NULL)	m_pcFontTitle = fontTitle;
	if(icon != NULL)		m_cIcon = icon;
	if(position != NULL)	m_cBoxFrame	= *position;
	m_nMode	= _mode;
	//TRACE(" CMsgBox::cText: %d ,m_cTitle %d,m_nMode %d\t\r\n",strlen(text),m_cTitle.size(),m_nMode);

	if(m_nMode & BORDER)
		m_nWindowFrameBorderWidth = WINDOW_FRAME_BORDER_WIDTH;
	else
		m_nWindowFrameBorderWidth = 0;

	//TRACE("  Mode: ");
	//if(_mode & BORDER) TRACE("BORDER ");
	//if(_mode & TITLE) TRACE("TITLE ");
	//if(_mode & FOOT) TRACE("FOOT ");
	//if(_mode & CENTER) TRACE("CENTER");
	//TRACE("\r\n");

	//TRACE_1(" m_nWindowFrameBorderWidth: \t%d\r\n",m_nWindowFrameBorderWidth);


	/* Initialise the window frames first */
	initFramesRel();

	m_pcTextBox = new CTextBox(	text,
								fontText,
								_mode,
								&m_cBoxFrameText);

	if(_mode & AUTO_WIDTH || _mode & AUTO_HIGH)
	{
		/* window might changed in size ...*/
		m_cBoxFrameText = m_pcTextBox->getWindowsPos();

		m_cBoxFrame.iWidth = m_cBoxFrameText.iWidth + m_nWindowFrameBorderWidth;
		m_cBoxFrame.iHeight = m_cBoxFrameText.iHeight + m_cBoxFrameFootRel.iHeight +  m_cBoxFrameTitleRel.iHeight + m_nWindowFrameBorderWidth;

		initFramesRel();
	}

	if(_mode & CENTER)
	{
		m_cBoxFrame.iX		= g_settings.screen_StartX + ((g_settings.screen_EndX - g_settings.screen_StartX - m_cBoxFrame.iWidth) >>1);
		m_cBoxFrame.iY		= g_settings.screen_StartY + ((g_settings.screen_EndY - g_settings.screen_StartY - m_cBoxFrame.iHeight) >>2);
	}

	m_nResult = default_result;
	m_nFootButtons = return_button;
}

//////////////////////////////////////////////////////////////////////
// Function Name:	CMsgBox
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
CMsgBox::CMsgBox(const char * text)
{
	initVar();

	m_pcTextBox = new CTextBox(	text);
	/* Initialise the window frames first */
	initFramesRel();
}

//////////////////////////////////////////////////////////////////////
// Function Name:	CMsgBox
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
CMsgBox::CMsgBox()
{
	m_pcTextBox = NULL;

	initVar();
	initFramesRel();
}

//////////////////////////////////////////////////////////////////////
// Function Name:	~CMsgBox
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
CMsgBox::~CMsgBox()
{
	if (m_pcWindow != NULL)
	{
		delete m_pcWindow;
		m_pcWindow = NULL;
	}

	if (m_pcTextBox != NULL)
	{
		delete m_pcTextBox;
		m_pcTextBox = NULL;
	}

}

//////////////////////////////////////////////////////////////////////
// Function Name:	InitVar
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void CMsgBox::initVar(void)
{
	//TRACE("->CMsgBox::InitVar\r\n");
	m_nResult = mbrYes;
	m_cTitle = "";
	m_cIcon = "";
	m_nMode = SCROLL | TITLE | BORDER ;

	// set the title varianles
	m_pcFontTitle  =  DEFAULT_TITLE_FONT;
	m_nFontTitleHeight = m_pcFontTitle->getHeight();

	// set the foot variables
	m_pcFontFoot  =  DEFAULT_FOOT_FONT;
	m_nFontFootHeight = m_pcFontFoot->getHeight();
	m_nFootButtons = 0;

	// set the main frame border width
	if(m_nMode & BORDER)
		m_nWindowFrameBorderWidth = WINDOW_FRAME_BORDER_WIDTH;
	else
		m_nWindowFrameBorderWidth = 0;

	// set the main frame to default
	m_cBoxFrame.iX		= g_settings.screen_StartX + ((g_settings.screen_EndX - g_settings.screen_StartX - MIN_WINDOW_WIDTH) >>1);
	m_cBoxFrame.iWidth	= MIN_WINDOW_WIDTH;
	m_cBoxFrame.iY		= g_settings.screen_StartY + ((g_settings.screen_EndY - g_settings.screen_StartY - MIN_WINDOW_HEIGHT) >>2);
	m_cBoxFrame.iHeight	= MIN_WINDOW_HEIGHT;

	m_pcWindow = NULL;

	//TRACE_1(" m_nWindowFrameBorderWidth: \t%d\r\n",m_nWindowFrameBorderWidth);
}

//////////////////////////////////////////////////////////////////////
// Function Name:	InitFramesRel
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void CMsgBox::initFramesRel(void)
{
	//TRACE("->CMsgBox::InitFramesRel\r\n");
	// init the title frame
	if(m_nMode & TITLE)
	{
		m_cBoxFrameTitleRel.iX		= 0;
		m_cBoxFrameTitleRel.iY		= 0;
		m_cBoxFrameTitleRel.iWidth	= m_cBoxFrame.iWidth - m_nWindowFrameBorderWidth;
		m_cBoxFrameTitleRel.iHeight	= m_nFontTitleHeight + 2;
	}
	else
	{
		m_cBoxFrameTitleRel.iX		= 0;
		m_cBoxFrameTitleRel.iY		= 0;
		m_cBoxFrameTitleRel.iHeight	= 0;
		m_cBoxFrameTitleRel.iWidth	= 0;
	}

	// init the foot frame
	if(m_nMode & FOOT)
	{
		m_cBoxFrameFootRel.iX		= 0;
		m_cBoxFrameFootRel.iY		= m_cBoxFrame.iHeight - m_nFontFootHeight - m_nWindowFrameBorderWidth - ADD_FOOT_HEIGHT;
		m_cBoxFrameFootRel.iWidth	= m_cBoxFrame.iWidth - m_nWindowFrameBorderWidth;
		m_cBoxFrameFootRel.iHeight	= m_nFontFootHeight + ADD_FOOT_HEIGHT;
	}
	else
	{
		m_cBoxFrameFootRel.iX		= 0;
		m_cBoxFrameFootRel.iY		= 0;
		m_cBoxFrameFootRel.iHeight   = 0;
		m_cBoxFrameFootRel.iWidth    = 0;
	}

	// init the text frame
	m_cBoxFrameText.iY		= m_cBoxFrame.iY + m_cBoxFrameTitleRel.iY + m_cBoxFrameTitleRel.iHeight;
	m_cBoxFrameText.iX		= m_cBoxFrame.iX + m_cBoxFrameTitleRel.iX;
	m_cBoxFrameText.iHeight	= m_cBoxFrame.iHeight - m_cBoxFrameTitleRel.iHeight - m_cBoxFrameFootRel.iHeight - m_nWindowFrameBorderWidth;
	m_cBoxFrameText.iWidth	= m_cBoxFrame.iWidth  - m_nWindowFrameBorderWidth;
#if 0
	TRACE_1("Frames\r\n\tScren:\t%3d,%3d,%3d,%3d\r\n\tMain:\t%3d,%3d,%3d,%3d\r\n\tText:\t%3d,%3d,%3d,%3d \r\n\tTitle:\t%3d,%3d,%3d,%3d \r\n\tFoot:\t%3d,%3d,%3d,%3d\r\n\r\n",
		g_settings.screen_StartX,
		g_settings.screen_StartY,
		g_settings.screen_EndX,
		g_settings.screen_EndY,
		m_cBoxFrame.iX,
		m_cBoxFrame.iY,
		m_cBoxFrame.iWidth,
		m_cBoxFrame.iHeight,
		m_cBoxFrameText.iX,
		m_cBoxFrameText.iY,
		m_cBoxFrameText.iWidth,
		m_cBoxFrameText.iHeight,
		m_cBoxFrameTitleRel.iX,
		m_cBoxFrameTitleRel.iY,
		m_cBoxFrameTitleRel.iWidth,
		m_cBoxFrameTitleRel.iHeight,
		m_cBoxFrameFootRel.iX,
		m_cBoxFrameFootRel.iY,
		m_cBoxFrameFootRel.iWidth,
		m_cBoxFrameFootRel.iHeight
		);
#endif
}

//////////////////////////////////////////////////////////////////////
// Function Name:	RefreshFoot
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void CMsgBox::refreshFoot(void)
{
	uint32_t color, bgcolor;
	if(	!(m_nMode & FOOT)) return;

	// draw the background first
	m_pcWindow->paintBoxRel(	m_cBoxFrameFootRel.iX+m_cBoxFrame.iX,
								m_cBoxFrameFootRel.iY+m_cBoxFrame.iY,
								m_cBoxFrameFootRel.iWidth,
								m_cBoxFrameFootRel.iHeight,
								(CFBWindow::color_t)COL_MENUHEAD_PLUS_0,
								RADIUS_LARGE, CORNER_BOTTOM);

	//const char* text;

	int MaxButtonTextWidth = m_pcFontFoot->getRenderWidth(g_Locale->getText(LOCALE_MESSAGEBOX_CANCEL), true); // UTF-8
	int ButtonWidth = 20 + 33 + MaxButtonTextWidth;
	int ButtonSpacing = (m_cBoxFrameFootRel.iWidth - 20- (ButtonWidth*3) ) / 2;
	int xpos = m_cBoxFrameFootRel.iX;

	// draw Button mbYes
	if (m_nFootButtons & mbYes)
	{
		if (m_nResult == mbrYes)
		{
			color   = COL_MENUCONTENTSELECTED_TEXT;
			bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		}
		else
		{
			color   = COL_INFOBAR_SHADOW_TEXT;
			bgcolor = COL_INFOBAR_SHADOW_PLUS_0;
		}
		m_pcWindow->paintBoxRel(xpos+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY, ButtonWidth, m_nFontFootHeight + 4, (CFBWindow::color_t)bgcolor, RADIUS_MID);
		m_pcWindow->paintIcon(NEUTRINO_ICON_BUTTON_RED, xpos + 14+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY);
		/*m_pcWindow->RenderString(*/
		m_pcFontFoot->RenderString(xpos + 43+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + m_nFontFootHeight + 4 + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY, ButtonWidth - 53, g_Locale->getText(LOCALE_MESSAGEBOX_YES), (CFBWindow::color_t)color, 0, true); // UTF-8
	}

	xpos += ButtonWidth + ButtonSpacing;

	// draw Button mbNo
	if (m_nFootButtons & mbNo)
	{
		if (m_nResult == mbrNo)
		{
			color   = COL_MENUCONTENTSELECTED_TEXT;
			bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		}
		else
		{
			color   = COL_INFOBAR_SHADOW_TEXT;
			bgcolor = COL_INFOBAR_SHADOW_PLUS_0;
		}

		m_pcWindow->paintBoxRel(xpos+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY, ButtonWidth, m_nFontFootHeight + 4, (CFBWindow::color_t)bgcolor, RADIUS_MID);
		m_pcWindow->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, xpos + 14+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY);
		/*m_pcWindow->RenderString(*/
		m_pcFontFoot->RenderString(xpos + 43+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + m_nFontFootHeight + 4 + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY, ButtonWidth- 53, g_Locale->getText(LOCALE_MESSAGEBOX_NO), (CFBWindow::color_t)color, 0, true); // UTF-8
	}

	xpos += ButtonWidth + ButtonSpacing;

	// draw Button mbCancel
	if (m_nFootButtons & (mbCancel | mbBack))
	{
		if (m_nResult >= mbrCancel)
		{
			color   = COL_MENUCONTENTSELECTED_TEXT;
			bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		}
		else
		{
			color   = COL_INFOBAR_SHADOW_TEXT;
			bgcolor = COL_INFOBAR_SHADOW_PLUS_0;
		}

		m_pcWindow->paintBoxRel(xpos+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY, ButtonWidth, m_nFontFootHeight + 4, (CFBWindow::color_t)bgcolor, RADIUS_MID);
		m_pcWindow->paintIcon(NEUTRINO_ICON_BUTTON_HOME, xpos+10+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY);
		/*m_pcWindow->RenderString(*/
		m_pcFontFoot->RenderString(xpos + 43+m_cBoxFrame.iX, m_cBoxFrameFootRel.iY + m_nFontFootHeight + 2 + (ADD_FOOT_HEIGHT>>1)+m_cBoxFrame.iY, ButtonWidth- 53, g_Locale->getText((m_nFootButtons & mbCancel) ? LOCALE_MESSAGEBOX_CANCEL : LOCALE_MESSAGEBOX_BACK), (CFBWindow::color_t)color, 0, true); // UTF-8
	}
}

//////////////////////////////////////////////////////////////////////
// Function Name:	RefreshTitle
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void CMsgBox::refreshTitle(void)
{
	// first check if title is configured
	if(	!(m_nMode & TITLE)) return;

	// draw the background
	m_pcWindow->paintBoxRel(	m_cBoxFrameTitleRel.iX+m_cBoxFrame.iX,
							m_cBoxFrameTitleRel.iY+m_cBoxFrame.iY,
							m_cBoxFrameTitleRel.iWidth,
							m_cBoxFrameTitleRel.iHeight,
							(CFBWindow::color_t)COL_MENUHEAD_PLUS_0,
							RADIUS_LARGE, CORNER_TOP);

	if (!m_cIcon.empty())
	{
		// draw icon and title text
		m_pcWindow->paintIcon(m_cIcon.c_str(), m_cBoxFrameTitleRel.iX + 8+m_cBoxFrame.iX, m_cBoxFrameTitleRel.iY + 5+m_cBoxFrame.iY);
		/*m_pcWindow->RenderString(*/
		m_pcFontTitle->RenderString(
								m_cBoxFrameTitleRel.iX + TITLE_ICON_WIDTH + TEXT_BORDER_WIDTH+m_cBoxFrame.iX,
								m_cBoxFrameTitleRel.iHeight+3+m_cBoxFrame.iY,
								m_cBoxFrameTitleRel.iWidth - TITLE_ICON_WIDTH + TEXT_BORDER_WIDTH,
								m_cTitle.c_str(),
								COL_MENUHEAD_TEXT,
								0,
								true); // UTF-8
	}
	else
	{
		// no icon available, just draw the title text
		/*m_pcWindow->RenderString(*/
		m_pcFontTitle->RenderString(
								m_cBoxFrameTitleRel.iX + TEXT_BORDER_WIDTH+m_cBoxFrame.iX,
								m_cBoxFrameTitleRel.iHeight+3+m_cBoxFrame.iY,
								m_cBoxFrameTitleRel.iWidth - TEXT_BORDER_WIDTH,
								m_cTitle.c_str(),
								COL_MENUHEAD_TEXT,
								0,
								true); // UTF-8
	}
}

//////////////////////////////////////////////////////////////////////
// Function Name:	RefreshBorder
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void CMsgBox::refreshBorder(void)
{
	if(	!(m_nMode & BORDER && m_nWindowFrameBorderWidth > 0)) return;

	//draw bottom shadow
	m_pcWindow->paintBoxRel(	m_nWindowFrameBorderWidth+m_cBoxFrame.iX,
							m_cBoxFrame.iHeight - m_nWindowFrameBorderWidth+m_cBoxFrame.iY - RADIUS_LARGE,
							m_cBoxFrame.iWidth - m_nWindowFrameBorderWidth - RADIUS_LARGE,
							m_nWindowFrameBorderWidth + RADIUS_LARGE,
							COL_INFOBAR_SHADOW_PLUS_0,
							RADIUS_LARGE, CORNER_BOTTOM_LEFT);

	//draw right shadow
	m_pcWindow->paintBoxRel(	m_cBoxFrame.iWidth - m_nWindowFrameBorderWidth+m_cBoxFrame.iX - RADIUS_LARGE,
							m_nWindowFrameBorderWidth+m_cBoxFrame.iY,
							m_nWindowFrameBorderWidth + RADIUS_LARGE,
							m_cBoxFrame.iHeight - m_nWindowFrameBorderWidth,
							COL_INFOBAR_SHADOW_PLUS_0,
							RADIUS_LARGE, CORNER_RIGHT);
}

//////////////////////////////////////////////////////////////////////
// global Functions
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Function Name:	Hide
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
bool CMsgBox::hide(void)
{
	//TRACE("->CMsgBox::Hide\r\n");

	if (m_pcWindow == NULL)
	{
		TRACE("  return -> window does not exists\r\n");
		return (false);
	}
	if(m_pcTextBox != NULL)
	{
		m_pcTextBox->hide();
	}

	// delete window
	// delete m_pcWindow;
	m_pcWindow->paintBackgroundBoxRel(m_cBoxFrame.iX, m_cBoxFrame.iY, m_cBoxFrame.iWidth, m_cBoxFrame.iHeight);
	m_pcWindow->blit();
	m_pcWindow = NULL;
	return (true);
}

//////////////////////////////////////////////////////////////////////
// Function Name:	ScrollPageDown
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void    CMsgBox::scrollPageDown(const int pages)
{
	// send scroll up event to text box if there is one
	if(m_pcTextBox != NULL)
	{
		m_pcTextBox->scrollPageDown(pages);
	}

}

//////////////////////////////////////////////////////////////////////
// Function Name:	ScrollPageUp
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void    CMsgBox::scrollPageUp(const int pages)
{
	// send scroll up event to text box if there is one
	if(m_pcTextBox != NULL)
	{
		m_pcTextBox->scrollPageUp(pages);
	}
}

//////////////////////////////////////////////////////////////////////
// Function Name:	Paint
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
bool CMsgBox::paint(void)
{
	/*
	 * do not paint stuff twice:
	 * => thread safety needed by movieplayer.cpp:
	 *    one thread calls our paint method, the other one our hide method
	 * => no memory leaks
	 */
	//TRACE("->CMsgBox::Paint\r\n");

	if (m_pcWindow != NULL)
	{
		TRACE("  return -> window already exists\r\n");
		return (false);
	}

	// create new window
	//m_pcWindow = new CFBWindow( m_cBoxFrame.iX, m_cBoxFrame.iY, m_cBoxFrame.iWidth, m_cBoxFrame.iHeight);
	m_pcWindow = CFrameBuffer::getInstance();
	refresh();
	if(m_pcTextBox != NULL)
	{
		m_pcTextBox->paint();
	}
	return (true);
}

//////////////////////////////////////////////////////////////////////
// Function Name:	Refresh
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
void CMsgBox::refresh(void)
{
	//TRACE("->CMsgBox::Refresh\r\n");

	if (m_pcWindow == NULL)
	{
		TRACE("  return -> no window\r\n");
		return;
	}

	//re-draw message box window
	refreshBorder();
	refreshTitle();
	refreshFoot();

	// rep-draw textbox if there is one
	if(m_pcTextBox != NULL)
	{
		//m_pcTextBox->refresh();
	}
}

//////////////////////////////////////////////////////////////////////
// Function Name:	Exec
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
int CMsgBox::exec( int timeout, int returnDefaultOnTimeout)
{
	//TRACE("->CMsgBox::exec\r\n");
#ifdef VC
	int res = 1;

#else
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	int return_button = m_nFootButtons;
	int res = menu_return::RETURN_REPAINT;

	// show message box
	paint();
	m_pcWindow->blit();
	if (m_pcWindow == NULL)
	{
		return res; /* out of memory */
	}

	if ( timeout == -1 )
		timeout = g_settings.timing[SNeutrinoSettings::TIMING_EPG];

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd( timeout );

	bool loop=true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if (msg == CRCInput::RC_timeout && returnDefaultOnTimeout)
		{
			// return default
			loop = false;
		}
		else if (	((msg == CRCInput::RC_timeout) ||
					(msg  == (neutrino_msg_t)g_settings.key_channelList_cancel)) &&
					(return_button & (mbCancel | mbBack)))
		{
			m_nResult = (return_button & mbCancel) ? mbrCancel : mbrBack;
			loop   = false;
		}
		else if ((msg == CRCInput::RC_green) && (return_button & mbNo))
		{
			m_nResult = mbrNo;
			loop   = false;
		}
		else if ((msg == CRCInput::RC_red) && (return_button & mbYes))
		{
			m_nResult = mbrYes;
			loop   = false;
		}
		else if(msg==CRCInput::RC_right)
		{
			bool ok = false;
			while (!ok)
			{
				m_nResult = (CMsgBox::result_)((m_nResult + 1) & 3);
				ok = m_nFootButtons & (1 << m_nResult);
			}

			refreshFoot();
		}
		else if (msg == CRCInput::RC_up )
		{
				scrollPageUp(1);
		}
		else if (msg == CRCInput::RC_down)
		{
				scrollPageDown(1);
		}
		else if(msg==CRCInput::RC_left)
		{
			bool ok = false;
			while (!ok)
			{
				m_nResult = (CMsgBox::result_)((m_nResult - 1) & 3);
				ok = return_button & (1 << m_nResult);
			}

			refreshFoot();
		}
		else if(msg == CRCInput::RC_ok)
		{
			loop = false;
		}
		else if ((msg ==CRCInput::RC_sat) || (msg == CRCInput::RC_favorites))
		{
		}
		else if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
		{
			res  = menu_return::RETURN_EXIT_ALL;
			loop = false;
		}
		m_pcWindow->blit();
	}

	hide();
#endif //VC

	return res;
}

//////////////////////////////////////////////////////////////////////
// Function Name:	SetText
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
bool CMsgBox::setText(const std::string* newText)
{
	bool lresult = false;
	// update text in textbox if there is one
	if(m_pcTextBox != NULL && newText != NULL)
	{
		lresult = m_pcTextBox->setText(newText);
		if(m_nMode & AUTO_WIDTH || m_nMode & AUTO_HIGH)
		{
			/* window might changed in size ...*/
			m_cBoxFrameText = m_pcTextBox->getWindowsPos();

			m_cBoxFrame.iWidth = m_cBoxFrameText.iWidth + m_nWindowFrameBorderWidth;
			m_cBoxFrame.iHeight = m_cBoxFrameText.iHeight + m_cBoxFrameFootRel.iHeight +  m_cBoxFrameTitleRel.iHeight + m_nWindowFrameBorderWidth;

			initFramesRel();

			// since the frames size has changed, we have to recenter the window again */
			if(m_nMode & CENTER)
			{
				m_cBoxFrame.iX		= g_settings.screen_StartX + ((g_settings.screen_EndX - g_settings.screen_StartX - m_cBoxFrame.iWidth) >>1);
				m_cBoxFrame.iY		= g_settings.screen_StartY + ((g_settings.screen_EndY - g_settings.screen_StartY - m_cBoxFrame.iHeight) >>1);
			}
		}
	}

	return(lresult);
}


//////////////////////////////////////////////////////////////////////
// Function Name:	SetText
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
int CMsgBox::result(void)
{
	return m_nResult;
}

//////////////////////////////////////////////////////////////////////
// Function Name:	ShowMsg2UTF
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
int ShowMsg2UTF(	const neutrino_locale_t Caption,
					const char * const Text,
					const CMsgBox::result_ Default,
					const uint32_t ShowButtons,
					const char * const Icon,
					const int Width,
					const int timeout,
					bool returnDefaultOnTimeout)
{
	//TRACE("->CMsgBox::ShowTextUTF \r\n");

	int result = ShowMsg2UTF(	g_Locale->getText(Caption),
								Text,
								Default,
								ShowButtons,
								Icon,
								Width,
								timeout,
								returnDefaultOnTimeout);

	return (result);

}

//////////////////////////////////////////////////////////////////////
// Function Name:	ShowMsg2UTF
// Description:
// Parameters:
// Data IN/OUT:
// Return:
// Notes:
//////////////////////////////////////////////////////////////////////
int ShowMsg2UTF(	const char * const Title,
					const char * const Text,
					const CMsgBox::result_ Default,
					const uint32_t ShowButtons,
					const char * const Icon,
					const int /*Width*/,
					const int timeout,
					bool returnDefaultOnTimeout)
{
	int mode =  CMsgBox::SCROLL |
				CMsgBox::TITLE |
				CMsgBox::FOOT |
				CMsgBox::BORDER |
				CMsgBox::AUTO_HIGH;
				//CMsgBox::NO_AUTO_LINEBREAK |
				//CMsgBox::CENTER |
				//CMsgBox::AUTO_WIDTH |
	CBox position (	g_settings.screen_StartX+30,
					g_settings.screen_StartY+30,
					g_settings.screen_EndX - g_settings.screen_StartX-60,
					g_settings.screen_EndY - g_settings.screen_StartY-60);

	//TRACE("\r\n->ShowTextUTF %s\r\n",Text);
   	CMsgBox* msgBox = new CMsgBox(		Text,
   										g_Font[SNeutrinoSettings::FONT_TYPE_MENU],
   										mode,
   										&position,
   										Title,
   										g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE],
   										Icon,
										ShowButtons,
										Default);

	msgBox->exec( timeout, returnDefaultOnTimeout);

	int res = msgBox->result();

	delete msgBox;

	return res;
}
