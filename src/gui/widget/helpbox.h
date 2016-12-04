/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implement CComponent-Windowclass.
	Copyright (C) 2015 Thilo Graf 'dbt'

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


#ifndef __helpbox__
#define __helpbox__

#include <gui/components/cc.h>
#include <global.h>
#include <neutrino.h>

#define HELPBOX_DEFAULT_LINE_HEIGHT 0 // use font height as default
#define HELPBOX_DEFAULT_LINE_INDENT OFFSET_INNER_MID

class Helpbox : public CComponentsWindowMax
{
private:
	uint8_t page;
	//start position of items on page
	int hbox_y;

	int getLineHeight(int line_height, Font* font_text, bool separator = false);

	///default font
	Font* hbox_font;
public:
	Helpbox(	const std::string& Title,
			const std::string& Default_Text = std::string(),
			const int& text_mode = CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
			const int& line_height = HELPBOX_DEFAULT_LINE_HEIGHT,
			Font* font_text = NULL,
			const char* Icon = NEUTRINO_ICON_INFO);

	///show = paint, for compatibility
	void show(bool do_save_bg = true){ paint(do_save_bg) ;}

	void addLine(	const std::string& text,
			const int& text_mode 	= CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);

	void addLine(	const char *text,
			const int& text_mode 	= CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);

	void addLine(	const std::string& icon,
			const std::string& text,
			const int& text_mode 	= CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);

	void addLine(	const char *icon,
			const char *text,
			const int& text_mode 	= CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);
	
	///adds a separator as horizontal line, arg 'line_height' defines the space of full separator height, the separator line itself has a defined height of 2px and is centered within line space.
	///arg 'line_indent' defines begin of line from left border within body object.
	///arg 'enable_gradient' enable/disable gradient, Note: default = true, but gradient is only effected, if global/theme gradient settings are enabled!
	void addSeparatorLine(	const int& line_height = HELPBOX_DEFAULT_LINE_HEIGHT,
				const int& line_indent = HELPBOX_DEFAULT_LINE_INDENT);
	///adds a simple empty separator as horizontal space, arg 'line_height' defines the space of full separator height
	void addSeparator(	const int& line_height = HELPBOX_DEFAULT_LINE_HEIGHT);

	void addPagebreak();
	
	int exec(){return ccw_body->exec();}
};

#endif
