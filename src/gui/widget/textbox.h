
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
#include <sigc++/signal.h>
#define TRACE  printf
#define TRACE_1 printf

class Font;
class CBox
{
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

class CTextBox : public sigc::trackable
{
	public:
		/* Variables */
		enum textbox_modes
		{
			AUTO_WIDTH			= 0x01, 	//auto adapt frame width to max width or max text width, text is painted with auto linebreak
			AUTO_HIGH			= 0x02, 	//auto adapt frame height to max height, text is painted with auto linebreak
			SCROLL				= 0x04, 	//frame box contains scrollbars on long text
			CENTER				= 0x40, 	//paint text centered
			RIGHT				= 0x80, 	//paint text right
			TOP				= 0x100,	//paint text on top of frame
			BOTTOM				= 0x200,	//paint text on bottom of frame
			NO_AUTO_LINEBREAK 		= 0x400,  	//paint text without auto linebreak,  cutting text
			AUTO_LINEBREAK_NO_BREAKCHARS	= 0x800		//no linbreak an char '-' and '.'
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
		
		bool hasChanged(int* x, int* y, int* dx, int* dy);
		bool hasChangedPos(int* x, int* y);
		bool hasChangedDim(int* dx, int* dy);
		void reInitToCompareVar(int* x, int* y, int* dx, int* dy);

		/* Variables */
		std::string m_cText, m_old_cText;
		std::vector<std::string> m_cLineArray;

		int m_old_x, m_old_y, m_old_dx, m_old_dy, m_old_nBgRadius, m_old_nBgRadiusType, m_old_nMode;
		bool m_has_scrolled;
		fb_pixel_t m_old_textBackgroundColor, m_old_textColor;

		bool m_showTextFrame;
		bool m_bg_painted;

		CBox m_cFrame;
		CBox m_cFrameTextRel;
		CBox m_cFrameScrollRel;

		int m_nMaxHeight;
		int m_nMaxWidth;
		int m_nMinHeight;
		int m_nMinWidth;

		int m_nMaxTextWidth;

		int m_nMode;
		int m_renderMode;

		int m_nNrOfPages;
		int m_nNrOfLines;
		int m_nNrOfNewLine;

		int m_nLinesPerPage;
		int m_nCurrentLine;
		int m_nCurrentPage;

		int  m_nBgRadius;
		int  m_nBgRadiusType;
		bool m_nPaintBackground;
		bool m_SaveScreen;
		bool m_utf8_encoded;

		Font* m_pcFontText;
		int m_nFontTextHeight;
		CFBWindow::color_t m_textBackgroundColor;
		fb_pixel_t m_textColor;
		fb_pixel_t* m_bgpixbuf;

		CFrameBuffer * frameBuffer;
/*		int max_width;*/
		
		int text_Hborder_width;
		int text_Vborder_width;
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
		void    enableBackgroundPaint(bool mode = true);
		void    disableBackgroundPaint();
		//enable screen saving behind chars, is required for transparent text paint, returns true if mode was changed
		bool    enableSaveScreen(bool mode = true);
		bool	setText(const std::string* newText, int max_width = 0, bool force_repaint = true);
		void 	setTextColor(fb_pixel_t color_text){ m_textColor = color_text;};
		void	setBackGroundRadius(const int radius, const int type = CORNER_ALL){m_nBgRadius = radius; m_nBgRadiusType = type;};
		void    setTextBorderWidth(int Hborder, int Vborder);
		void	setTextFont(Font* font_text);
		void	setTextMode(const int text_mode){m_nMode = text_mode;};
		void	setTextRenderModeFullBG(bool mode){ m_renderMode = (mode) ? 2 /*Font::FULLBG*/ : 0; };
		void	setBackGroundColor(CFBWindow::color_t textBackgroundColor){m_textBackgroundColor = textBackgroundColor;};
		void	setWindowPos(const CBox* position){m_cFrame = *position;};
		void 	setWindowMaxDimensions(const int width, const int height);
		void 	setWindowMinDimensions(const int width, const int height);
		void    setFontUseDigitHeight(bool set=true);
		void	enableUTF8(bool enable = true){m_utf8_encoded = enable;}
		void	disableUTF8(bool enable = false){enableUTF8(enable);}

		bool 	isPainted(void)			{if( frameBuffer == NULL) return (false); else return (true);};
		CBox	getWindowsPos(void)		{return(m_cFrame);};

		int     getLinesPerPage(void)		{return m_nLinesPerPage;};
		int     getPages(void)			{return(m_nNrOfPages);};
		int	getBackGroundRadius(void)	{return(m_nBgRadius);};

		/**
		* Returns count of lines of a passed text.
		* @param[in]	text
		* 	@li 	expects type std::string
		* 	@return	count of lines as int
		* 	@see	getLines()
		*/
		static int getLines(const std::string& text);

		/**
		* Returns count of calculated lines from an existing CTextBox instance.
		* 	@return	count of lines as int
		* 	@see	static version getLines()
		* 	@note	Real count of lines will be only returned if CTextBox object is initialized with a valid CBox instance, \n
		* 		otherwise count of 0 lines will be returned!
		*/
		int     getLines();

		/**
		* Returns width of largest line from passed text
		* @param[in]	text
		* 	@li 	expects type std::string
		* @param[in]	font
		* 	@li 	expects font type object
		* 	@return	width of largest line as int
		* 	@see	getMaxLineWidth(void)
		*/
		static int	getMaxLineWidth(const std::string& text, Font* font);

		/**
		* Returns internal defined largest line width of an existant CTextBox instance.
		* 	@return	width of largest line as int
		* 	@see	static version getMaxLineWidth()
		* 		setText(), parameter: max_width
		*/
		int		getMaxLineWidth()		{return(m_nMaxTextWidth);}

		inline	void	movePosition(int x, int y)	{m_cFrame.iX = x; m_cFrame.iY = y;};
		int  getFontTextHeight();
		inline int	getTextMode()			{return m_nMode;};
		void paint (void);
		void hide (void);
		void clear(void);
		bool clearScreenBuffer();
		sigc::signal<void> OnAfterRefresh;
		sigc::signal<void> OnAfterScrollPage;
};

#endif // !defined(AFX_TEXTBOX_H__208DED01_ABEC_491C_A632_5B21057DC5D8__INCLUDED_)
