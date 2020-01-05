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


#ifndef __C_GUI_RADIO_TEXT_H__
#define __C_GUI_RADIO_TEXT_H__

#include <gui/components/cc.h>
#include <sigc++/signal.h>
#include <string>

class CRadioTextGUI : public CComponentsWindow
{
	private:
		std::vector<std::string> v_lines_title;
		std::vector<std::string> v_lines_desc;
		Font *font1, *font2;
		typedef std::pair <t_channel_id, t_channel_id> channel_id_pairs; //pair = current id, old id
		channel_id_pairs channel_id_pair;

		bool hasDescription();
		bool hasTitle();
		bool isDubLine(std::vector<std::string>& vec, const std::string& to_compare_str);
		void addLine(std::vector<std::string>& vec, std::string str);
		bool GetData();
		void initSlots();
		void resetPos();
		void Init();
		void InitInfoItems();
		void _clearSavedScreen() {clearSavedScreen();}


	public:
		CRadioTextGUI();
		virtual~CRadioTextGUI();

		void kill(const fb_pixel_t& bg_color = COL_BACKGROUND_PLUS_0, const int& corner_radius = -1, const int& fblayer_type = ~CC_FBDATA_TYPES);
		void clearLines();
		bool hasLines(){ return (hasTitle() || hasDescription());}
		void paint(const bool &do_save_bg);

		sigc::slot<void>sl_after_decode_line,
				sl_on_start_screensaver,
				sl_on_stop_screensaver,
				sl_on_after_kill_infobar,
				sl_on_show_radiotext;
};
#endif
