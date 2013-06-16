/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Experimental Classes for generic GUI-related components. Not really used.
	Copyright (C) 2012, Michael Liebmann 'micha-bbg
	Copyright (C) 2012, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CC_MISC__
#define __CC_MISC__

#include "config.h"
#include <gui/components/cc.h>
#include <vector>
#include <string>


#define FIRST_ELEMENT_INIT 10000
#define LOGO_MAX_WIDTH width/4
class CComponentsItemBox : public CComponentsItem
{
	protected:
		int hSpacer;
		int hOffset;
		int vOffset;
		int digit_offset, digit_h;
		bool paintElements;
		bool onlyOneTextElement;
		fb_pixel_t it_col_text;
		Font* font_text;
		int hMax;
		bool has_TextElement;
		size_t firstElementLeft;
		size_t firstElementRight;
		size_t prevElementLeft;
		size_t prevElementRight;
		std::vector<comp_element_data_t> v_element_data;
		bool isCalculated;

		void clearElements();
		void initVarItemBox();
		void calSizeOfElements();
		void calPositionOfElements();
		void paintItemBox(bool do_save_bg = CC_SAVE_SCREEN_YES);
		void calculateElements();
		bool addElement(int align, int type, const std::string& element="", size_t *index=NULL);
		void paintImage(size_t index, bool newElement);
		void paintText(size_t index, bool newElement);

	public:
		CComponentsItemBox();
		virtual ~CComponentsItemBox();

		inline virtual void setTextFont(Font* font){font_text = font;};
		inline virtual void setTextColor(fb_pixel_t color_text){ it_col_text = color_text;};

		virtual void refreshElement(size_t index, const std::string& element);
		virtual void paintElement(size_t index, bool newElement= false);
		virtual bool addLogoOrText(int align, const std::string& logo, const std::string& text, size_t *index=NULL);
		virtual void clearTitlebar();
		virtual void addText(const std::string& s_text, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addText(neutrino_locale_t locale_text, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addIcon(const std::string& s_icon_name, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addPicture(const std::string& s_picture_path, const int align=CC_ALIGN_LEFT, size_t *index=NULL);
		virtual void addClock(const int align=CC_ALIGN_RIGHT, size_t *index=NULL);
		virtual int  getHeight();
};

class CComponentsTitleBar : public CComponentsItemBox
{
	private:
		const char* tb_c_text;
		std::string tb_s_text, tb_icon_name;
		neutrino_locale_t tb_locale_text;
		int tb_text_align, tb_icon_align;

		void initText();
		void initIcon();
		void initElements();
		void initVarTitleBar();

	public:
		CComponentsTitleBar();
		CComponentsTitleBar(	const int x_pos, const int y_pos, const int w, const int h, const char* c_text = NULL, const std::string& s_icon ="",
					fb_pixel_t color_text = COL_MENUHEAD, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0);
		CComponentsTitleBar(	const int x_pos, const int y_pos, const int w, const int h, const std::string& s_text ="", const std::string& s_icon ="",
					fb_pixel_t color_text = COL_MENUHEAD, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0);
		CComponentsTitleBar(	const int x_pos, const int y_pos, const int w, const int h, neutrino_locale_t locale_text = NONEXISTANT_LOCALE, const std::string& s_icon ="",
					fb_pixel_t color_text = COL_MENUHEAD, fb_pixel_t color_body = COL_MENUHEAD_PLUS_0);

		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);

};

#endif /*__CC_MISC__*/
