/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Class for radio text handling
	Copyright (C) 2019, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "radiotext_window.h"
#include "infoclock.h"
#include "screensaver.h"
#include "infoviewer.h"

#include <driver/radiotext.h>
#include <driver/fontrenderer.h>
#include <zapit/zapit.h>
#include <system/helpers.h>
#include <system/debug.h>
#include <mutex>
#include <stdexcept>

#define MAX_TITLE_LINES 1
#define MAX_DESC_LINES 2

using namespace std;

CRadioTextGUI::CRadioTextGUI() : CComponentsWindow(0, 0, CFrameBuffer::getInstance()->getScreenWidth() - 2*OFFSET_INNER_MID, 0, "")
{
	Init();
}


CRadioTextGUI::~CRadioTextGUI()
{
	dprintf(DEBUG_DEBUG, "\033[32m[CRadioTextGUI] %s - %d delete radiotext window instance\033[0m\n", __func__, __LINE__);
}


void CRadioTextGUI::Init()
{
	cc_item_type.name 	= "radiotext_window";

	if (!g_Radiotext)
		return;

	dprintf(DEBUG_DEBUG, "\033[32m[CRadioTextGUI] %s - %d init radiotext window handler\033[0m\n", __func__, __LINE__);

	initSlots();

	font1 = g_Font[SNeutrinoSettings::FONT_TYPE_WINDOW_RADIOTEXT_TITLE];
	font2 = g_Font[SNeutrinoSettings::FONT_TYPE_WINDOW_RADIOTEXT_DESC];
	height = (MAX_DESC_LINES * font1->getHeight()) + (ccw_head->getHeight() + 2*OFFSET_INNER_MID);
	shadow = CC_SHADOW_ON;
	ccw_show_footer = false;
	shadow_force = true;
	channel_id_pair.first = CZapit::getInstance()->GetCurrentChannelID();
	channel_id_pair.second = 0;

#if 0	// gradient mode for header with infobar properties
	ccw_head->setColorBody(g_settings.theme.infobar_gradient_top ? COL_MENUHEAD_PLUS_0 : COL_INFOBAR_PLUS_0);
	ccw_head->enableColBodyGradient(g_settings.theme.infobar_gradient_top, COL_INFOBAR_PLUS_0, g_settings.theme.infobar_gradient_top_direction);
#endif
	ccw_body->setColorBody(g_settings.theme.infobar_gradient_body ? COL_MENUHEAD_PLUS_0 : COL_INFOBAR_PLUS_0);
	ccw_body->enableColBodyGradient(g_settings.theme.infobar_gradient_body, COL_INFOBAR_PLUS_0, g_settings.theme.infobar_gradient_body_direction);

}


void CRadioTextGUI::initSlots()
{
	// paint rt window if any decoded lines are available
	sl_after_decode_line = sigc::bind(sigc::mem_fun(*this, &CRadioTextGUI::paint), false);
	g_Radiotext->OnAfterDecodeLine.connect(sl_after_decode_line);

	// unblock paint of rt window if screensaver is started
	sl_on_start_screensaver = sigc::bind(sigc::mem_fun(*this, &CRadioTextGUI::allowPaint), true);
	CScreenSaver::getInstance()->OnAfterStart.connect(sl_on_start_screensaver);

	// block paint of screensaver if screensaver was sopped
	sl_on_stop_screensaver = sigc::bind(sigc::mem_fun(*this, &CRadioTextGUI::allowPaint), false);
	CScreenSaver::getInstance()->OnAfterStop.connect(sl_on_stop_screensaver);

	// reset position of rt window if screensaver was stopped, because rt window position would be changed inside screensaver
	sl_on_stop_screensaver = sigc::mem_fun(*this, &CRadioTextGUI::resetPos);
	CScreenSaver::getInstance()->OnAfterStop.connect(sl_on_stop_screensaver);

	// cleanup saved background while stopping screensaver
	sl_on_stop_screensaver = sigc::mem_fun(*this, (&CRadioTextGUI::_clearSavedScreen));
	CScreenSaver::getInstance()->OnAfterStop.connect(sl_on_stop_screensaver);

	// remove rt window from screen if infoviewer was closed
	sl_on_after_kill_infobar = sigc::bind(sigc::mem_fun(*this, &CRadioTextGUI::kill), COL_BACKGROUND_PLUS_0, -1,  ~CC_FBDATA_TYPES);
	g_InfoViewer->OnAfterKillTitle.connect(sl_on_after_kill_infobar);

	// allow paint of rt window if infoviewer gives commands
	sl_on_show_radiotext = sigc::bind(sigc::mem_fun(*this, &CRadioTextGUI::allowPaint), true);
	g_InfoViewer->OnEnableRadiotext.connect(sl_on_show_radiotext);
}


bool CRadioTextGUI::GetData()
{
	if (g_Radiotext == NULL)
		return false;
	
// 	if (showButtonBar)
// 		infoViewerBB->showIcon_RadioText(g_Radiotext->haveRadiotext()); ///callback???
// 
	std::mutex mutex;
	std::lock_guard<std::mutex> lg(mutex);
	char stext[3][100];

	// cleanup lines on changed channel and clean up cached lines
	channel_id_pair.first = CZapit::getInstance()->GetCurrentChannelID();
	if (channel_id_pair.first != channel_id_pair.second){
		clearLines();
		channel_id_pair.second = CZapit::getInstance()->GetCurrentChannelID();
	}

	if (g_Radiotext->S_RtOsd)
	{
		int lines = 0;
		for (int i = 0; i < g_Radiotext->S_RtOsdRows; i++)
		{
			if (g_Radiotext->RT_Text[i][0] != '\0')
				lines++;
		}

// 		if (lines)
// 		{
// 			clean up title before generate new lines
// 			clearLines()
// 		}

		if (g_Radiotext->RT_MsgShow) // TODO: remove unused desc lines from vector, maybe more than one lines are not really sensefully
		{
			// title line(s)
			if (g_Radiotext->S_RtOsdTitle == 1)
			{
				if (lines || g_Radiotext->RT_PTY != 0)
				{
					snprintf(stext[0], sizeof(stext[0]),g_Radiotext->RT_PTY == 0 ? "%s %s%s" : "%s (%s)%s", tr("Radiotext"), g_Radiotext->RT_PTY == 0 ? g_Radiotext->RDS_PTYN : g_Radiotext->ptynr2string(g_Radiotext->RT_PTY), ":");
					if (!isDubLine(v_lines_title, stext[0]))
						addLine(v_lines_title, stext[0]);
				}
			}
			// collecting...
			if (lines)
			{
				// RT-Text roundloop
				int index = (g_Radiotext->RT_Index == 0) ? g_Radiotext->S_RtOsdRows - 1 : g_Radiotext->RT_Index - 1;
				
				if (g_Radiotext->S_RtOsdLoop == 1) // ...description lines, latest bottom
				{
					for (int i = index + 1; i < g_Radiotext->S_RtOsdRows; i++)
					{
						if (!string(g_Radiotext->RT_Text[i]).empty())
							if (!isDubLine(v_lines_desc, g_Radiotext->RT_Text[i]))
								addLine(v_lines_desc, g_Radiotext->RT_Text[i]);
					}

					for (int i = 0; i <= index; i++)
					{
						if (!string(g_Radiotext->RT_Text[i]).empty())
							if (!isDubLine(v_lines_desc, g_Radiotext->RT_Text[i]))
								addLine(v_lines_desc, g_Radiotext->RT_Text[i]);
					}
				}
				else // ...title line(s), latest top
				{
					for (int i = index; i >= 0; i--)
					{	
						if (!string(g_Radiotext->RT_Text[i]).empty())
							if (!isDubLine(v_lines_desc, g_Radiotext->RT_Text[i]))
								addLine(v_lines_desc, g_Radiotext->RT_Text[i]);
					}

					for (int i = g_Radiotext->S_RtOsdRows - 1; i > index; i--)
					{
						if (!string(g_Radiotext->RT_Text[i]).empty())
							if (!isDubLine(v_lines_desc, g_Radiotext->RT_Text[i]))
								addLine(v_lines_desc, g_Radiotext->RT_Text[i]);
					}
				}
			}
		}
	}

	// clean up last description line that not to use
	if (v_lines_desc.size() > MAX_DESC_LINES)
		v_lines_desc.erase(v_lines_desc.begin());

	g_Radiotext->RT_MsgShow = false;

	if (hasDescription() && hasTitle())
		return true;

	return false;
}


bool CRadioTextGUI::isDubLine(vector<string>& vec, const string& to_compare_str)
{
	for (size_t i = 0; i < vec.size(); i++)
		if (vec.at(i) == to_compare_str)
			return true;

	return false;
}

void CRadioTextGUI::addLine(std::vector<std::string>& vec, std::string str)
{
	vec.push_back(iso_8859_1_to_utf8(str));
}


void CRadioTextGUI::clearLines()
{
	if (hasTitle())
		v_lines_title.clear();
	if (hasDescription())
		v_lines_desc.clear();
	ccw_body->clear();
}

bool CRadioTextGUI::hasDescription()
{
	return !v_lines_desc.empty();
}


bool CRadioTextGUI::hasTitle()
{
	return !v_lines_title.empty();
}

void CRadioTextGUI::InitInfoItems()
{
	//get and checkup required informations
	if ((!GetData() && !hasLines()) || !cc_allow_paint)
		return;
	
	//define size and position
	int x_info = OFFSET_INNER_MID;
	int y_info = OFFSET_INNER_MID;
	int h_info = font1->getHeight(); //default height
	int w_info = width-2*x_info;

	//init text items
	if (ccw_body->empty())
	{
		// add required text objects
		for (int i = 0; i < MAX_DESC_LINES; i++)
		{
			static CComponentsTextTransp* item = NULL;
			item = new CComponentsTextTransp(NULL, OFFSET_INNER_MID, y_info, w_info, h_info, "");
			item->setItemName("desc" + to_string(i));
			addWindowItem(item);
			y_info += h_info;
		}
	}

	// fill text items with text
	if (!v_lines_desc.empty())
	{
		reverse(v_lines_desc.begin(),v_lines_desc.end()); // we need synchron order
		for (size_t i = 0; i < v_lines_desc.size(); i++)
		{
			if (i < MAX_DESC_LINES) // use only required entries
			{
				if (ccw_body->getCCItem(i))
				{
					if (ccw_body->getCCItem(i)->getItemType() == CC_ITEMTYPE_TEXT)
					{
						try
						{
							string s_tmp = v_lines_desc.at(i);
							if (i > 0)
							{
								if (s_tmp == v_lines_desc.at(i-1))
									s_tmp = "";
							}
							dprintf(DEBUG_INFO, "\033[36m[CRadioTextGUI] %s - %d: set line: %d text:  %s\033[0m\n", __func__, __LINE__, (int)i, s_tmp.c_str());
							static CComponentsTextTransp* item = NULL;
							item = static_cast<CComponentsTextTransp*> (ccw_body->getCCItem(i));
							item->setText(	s_tmp,
									CTextBox::AUTO_HIGH | CTextBox::TOP | CTextBox::AUTO_LINEBREAK_NO_BREAKCHARS,
									item->getItemName() == "desc0" ? font1 : font2);
						}
						catch (const std::exception &e)
						{
							dprintf(DEBUG_NORMAL, "\033[31m[CRadioTextGUI]\[%s - %d], exeption error %s\033[0m\n", __func__, __LINE__, e.what());
							return;
						}
					}
				}
			}
		}
		reverse(v_lines_desc.begin(),v_lines_desc.end()); // restore origin order
	}
}

void CRadioTextGUI::paint(const bool &do_save_bg)
{
	hide();
	clearSavedScreen();

	if (!g_Radiotext)
		return;

	if (!g_Radiotext->haveRadiotext())
		return;
#if 0
	g_Radiotext->S_RtOsd = g_Radiotext->haveRadiotext() ? 1 : 0;

	if (g_settings.radiotext_enable)
		g_Radiotext->RT_MsgShow = true;
#endif
	if (ccw_body->isPainted()){
		ccw_body->hideCCItems();
	}

	InitInfoItems();
#if 0
	for (size_t i = 0; i < v_lines_title.size(); i++)
		dprintf(DEBUG_NORMAL, "\033[32m[CRadioTextGUI] %s - %d: Title: %s\033[0m\n", __func__, __LINE__, v_lines_title[i].c_str());

	size_t n = v_lines_desc.size()-1;
	for (int i = n; i > -1; i--)
		dprintf(DEBUG_NORMAL, "\033[32m[CRadioTextGUI] %s - %d: Description line:  %s\033[0m\n", __func__, __LINE__, v_lines_desc[i].c_str());
#endif

	if (!v_lines_title.empty())
		setWindowCaption(v_lines_title.back());


	if (cc_allow_paint)
		CInfoClock::getInstance()->block();

	CComponentsWindow::paint(do_save_bg);
}

void CRadioTextGUI::kill(const fb_pixel_t& bg_color, const int& corner_radius, const int& fblayer_type)
{
	int cr = getCornerRadius() != corner_radius ? corner_radius : getCornerRadius();
	CComponentsWindow::kill(bg_color, cr, fblayer_type);
	allowPaint(false);
	ccw_body->clear();
}

void CRadioTextGUI::resetPos()
{
	setPos(0, 0);
	initWindowSize();
	initWindowPos();
}
