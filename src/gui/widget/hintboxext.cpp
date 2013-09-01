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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/hintboxext.h>

#include <global.h>
#include <neutrino.h>
#include <driver/screen_max.h>
#include <driver/framebuffer.h>

#include <iostream>

CHintBoxExt::CHintBoxExt(const neutrino_locale_t Caption, const char * const Text, const int Width, const char * const Icon)
{
	m_message = strdup(Text);

	char *begin   = m_message;

	begin = strtok(m_message, "\n");
	while (begin != NULL)
	{
		std::vector<Drawable*> oneLine;
		std::string s(begin);
		DText *d = new DText(s);
		oneLine.push_back(d);
		m_lines.push_back(oneLine);
		begin = strtok(NULL, "\n");
	}
	m_bbheight = 0;
	init(Caption, Width, Icon);
}


CHintBoxExt::CHintBoxExt(const neutrino_locale_t Caption, ContentLines& lines, const int Width, const char * const Icon)
{
	m_message = NULL;
	m_lines = lines;
	m_bbheight = 0;
	init(Caption, Width, Icon);
}

CHintBoxExt::~CHintBoxExt(void)
{
	if (m_window != NULL)
	{
		delete m_window;
		m_window = NULL;
	}
	if (m_message != NULL) {
		free(m_message);

		// content has been set using "m_message" so we are responsible to 
		// delete it
		for (ContentLines::iterator it = m_lines.begin();
			 it != m_lines.end(); ++it)
		{
			for (std::vector<Drawable*>::iterator it2 = it->begin();
				 it2 != it->end(); ++it2)
			{
				//(*it2)->print();
				delete *it2;
			}
		}
	}
}

void CHintBoxExt::init(const neutrino_locale_t Caption, const int Width, const char * const Icon)
{
	m_width   = Width;
	int nw = 0;
	m_theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	m_fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	m_height  = m_theight + m_fheight;
	m_maxEntriesPerPage = 0;
	int maxLineWidth = 0;
	int scrollWidth = 0;
	textStartX = 0;
	bgPainted = false;

	m_caption = Caption;

	int page = 0;
	int line = 0;
	int maxWidth = m_width > 0 ? m_width : 0;
	int maxOverallHeight = 0;
	int screenheight = CFrameBuffer::getInstance()->getScreenHeight() * 9 / 10 - m_bbheight;
	m_startEntryOfPage.clear();
	m_startEntryOfPage.push_back(0);
	for (ContentLines::iterator it = m_lines.begin(); it!=m_lines.end(); ++it)
	{
		bool pagebreak = false;
		int maxHeight = 0;
		int lineWidth = 0;
		for (std::vector<Drawable*>::iterator item = it->begin();
			 item != it->end(); ++item) {
			if ((*item)->getHeight() > maxHeight)
				maxHeight = (*item)->getHeight();
			lineWidth += (*item)->getWidth();
			if ((*item)->getType() == Drawable::DTYPE_PAGEBREAK)
				pagebreak = true;
		}
                maxLineWidth = std::max(maxLineWidth, lineWidth);
                if (lineWidth > maxWidth)
			maxWidth = lineWidth;
		m_height += maxHeight;
		if (m_height > screenheight || pagebreak) {
			if (m_height-maxHeight > maxOverallHeight)
				maxOverallHeight = m_height - maxHeight;
			m_height = m_theight + m_fheight + maxHeight;
			if (pagebreak)
				m_startEntryOfPage.push_back(line+1);
			else 
				m_startEntryOfPage.push_back(line);
			page++;
			if (m_maxEntriesPerPage < (m_startEntryOfPage[page] - m_startEntryOfPage[page-1]))
			{
				m_maxEntriesPerPage = m_startEntryOfPage[page] - m_startEntryOfPage[page-1];
			}
		}
		else {
			if (m_height > maxOverallHeight)
				maxOverallHeight = m_height;
			m_maxEntriesPerPage = maxOverallHeight / m_fheight;
		}
		line++;
	}

	// if there is only one page m_height is already correct 
    // but m_maxEntries has not been set
	if (m_startEntryOfPage.size() > 1)
	{
		m_height = maxOverallHeight;
	} else {
		m_maxEntriesPerPage = line;
	}

	m_startEntryOfPage.push_back(line+1); // needed to calculate amount of items on last page

// 	for (std::vector<int>::iterator it=m_startEntryOfPage.begin();
// 		 it!=m_startEntryOfPage.end();it++)
// 		printf("startentryofpage: %d\n",*it);
// 	printf("pages: %d, startEntryVec: %d\n",page+1,m_startEntryOfPage.size()-1);
// 	printf("maxEntries: %d\n", m_maxEntriesPerPage);

	m_width = w_max(maxWidth,SHADOW_OFFSET); 
	m_currentPage = 0;
	m_pages = page + 1;
	unsigned int additional_width;

	if (has_scrollbar()) 
		scrollWidth = 15;
	else
		scrollWidth = 0;
	additional_width = 30 + scrollWidth;
	m_width += additional_width;

	if (Icon != NULL)
	{
		m_iconfile = Icon;
		additional_width += 30;
	}
	else
		m_iconfile = "";

	nw = additional_width + g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(m_caption), true); // UTF-8

	if (nw > m_width)
		m_width = nw;

	/* if the output does not fit, make sure we at least
	 * stay inside the screen... */
	m_width = w_max(m_width ,SHADOW_OFFSET);
	if (maxLineWidth + scrollWidth > m_width)
		maxLineWidth = m_width - scrollWidth;

	textStartX = (m_width - scrollWidth - maxLineWidth) / 2;

	m_window = NULL;
}

void CHintBoxExt::paint(bool toround)
{
	if (m_window != NULL)
	{
		/*
		 * do not paint stuff twice:
		 * => thread safety needed by movieplayer.cpp:
		 *    one thread calls our paint method, the other one our hide method
		 * => no memory leaks
		 */
		return;
	}

	bgPainted = false;
	m_window = new CFBWindow(getScreenStartX(m_width + SHADOW_OFFSET),
				 getScreenStartY(m_height + SHADOW_OFFSET),
				 m_width + SHADOW_OFFSET,
				 m_height + SHADOW_OFFSET);

	refresh(toround);
}

void CHintBoxExt::refresh(bool toround)
{
	if (m_window == NULL)
	{
		return;
	}
	
	if (!bgPainted) {
		// bottom, right shadow
		m_window->paintBoxRel(SHADOW_OFFSET, SHADOW_OFFSET, m_width, m_height, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_LARGE, toround ? CORNER_ALL : CORNER_BOTTOM | CORNER_TOP_RIGHT);
		bgPainted = true;
	}
	
	// title bar
	m_window->paintBoxRel(0, 0, m_width, m_theight, (CFBWindow::color_t)COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);//round
	
	// icon
	int x_offset = 6, icon_space = x_offset, x_text;
	std::string title_text = g_Locale->getText(m_caption);
	if (!m_iconfile.empty())
	{
		int w, h;
		CFrameBuffer::getInstance()->getIconSize(m_iconfile.c_str(), &w, &h);
		icon_space = w + 2*x_offset;
		int y_icon = 0+ (m_theight >> 1) - (h >> 1);
 		m_window->paintIcon(m_iconfile.c_str(), x_offset, y_icon);
		x_text = icon_space;
	}
	else
		x_text = x_offset;
	
	// title text
	m_window->RenderString(g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE], x_text, m_theight, m_width, title_text.c_str(), COL_MENUHEAD_TEXT, 0, true); // UTF-8

	// background of text panel
	m_window->paintBoxRel(0, m_theight, m_width, (m_maxEntriesPerPage + 1) * m_fheight, (CFBWindow::color_t)COL_MENUCONTENT_PLUS_0, toround ? RADIUS_LARGE : 0, CORNER_BOTTOM);//round

	int yPos  = m_theight + (m_fheight >> 1);

// 	for(std::vector<int>::iterator it = m_startEntryOfPage.begin();
// 		it != m_startEntryOfPage.end();it++) {
// 		printf(" %d",*it);
// 	}
// 	printf("\n current page: %d lines %d ",m_currentPage, m_lines.size());
// 	printf("start %d bis %d\n",m_startEntryOfPage[m_currentPage],m_startEntryOfPage[m_currentPage+1]-1);

#if 0
	for (ContentLines::iterator it = m_lines.begin() + m_startEntryOfPage[m_currentPage];
		 it != m_lines.begin() + m_startEntryOfPage[m_currentPage+1]
			 && it != m_lines.end(); ++it)
#endif
	for (int count = 0; count < (int) m_lines.size(); count++)
	{
		if ((count >= m_startEntryOfPage[m_currentPage]) &&
				(count < m_startEntryOfPage[m_currentPage+1]))
		{
			int xPos = textStartX;
			int maxHeight = 0;
			for (std::vector<Drawable*>::iterator d = m_lines[count].begin(); d != m_lines[count].end(); ++d)
			{
				// 			(*d)->print();
				// 			printf("\n");
				//(*d)->draw(m_window,xPos,yPos,m_width);
				(*d)->draw(m_window,xPos,yPos,m_width-20);
				xPos += (*d)->getWidth() + 20;
				if ((*d)->getHeight() > maxHeight)
					maxHeight = (*d)->getHeight();
			}
			yPos += maxHeight;
		}
	}

	if (has_scrollbar()) 
	{
//		yPos = m_theight + (m_fheight >> 1);
		yPos = m_theight;
		m_window->paintBoxRel(m_width - 15, yPos, 15, m_maxEntriesPerPage * m_fheight, COL_MENUCONTENT_PLUS_1);
		unsigned int marker_size = (m_maxEntriesPerPage * m_fheight) / m_pages;
		m_window->paintBoxRel(m_width - 13, yPos + m_currentPage * marker_size, 11, marker_size, COL_MENUCONTENT_PLUS_3);
	}
}

bool CHintBoxExt::has_scrollbar(void)
{
	return (m_startEntryOfPage.size() > 2);
}

void CHintBoxExt::scroll_up(void)
{
	if (m_currentPage > 0)
	{
		m_currentPage--;
		refresh();
	}
}

void CHintBoxExt::scroll_down(void)
{
	if (m_currentPage +1 < m_startEntryOfPage.size()-1)
	{
		m_currentPage++;
		refresh();
	}
}

void CHintBoxExt::hide(void)
{
	if (m_window != NULL)
	{
		delete m_window;
		m_window = NULL;
	}
}
