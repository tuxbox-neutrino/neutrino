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
#define HELPBOX_DEFAULT_TEXT_MODE CTextBox::AUTO_WIDTH | CTextBox::AUTO_HIGH

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
	/**
	* Creates a window with pre defined items added with addLine() or
	* inhertited addWindowItem() methods. This allows to add any compatible cc-items types
	* e.g. Text, label, infobox, images ...
	* NOTE: addLine() members are used only for compatibilty with older implementation
	* of lines.
	* @param[in]	Title
	* 	@li 	expects type std::string, defines caption of window
	* @param[in]	Default_Text
	* 	@li 	optional: expects type std::string, defines a pre defined help text
	* @param[in]	text_mode
	* 	@li 	optional: expects type int, defines text modes, see /gui/widget/textbox.h for possible modes
	* @param[in]	line_height
	* 	@li 	optional: expects type int, defines height of line
	* @param[in]	line_indent
	* 	@li 	optional: expects type int, defines lenght of indent from left
	* @param[in]	font_text
	* 	@li 	optional: expects type Font*, default = NULL, this means SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO is used
	* @param[in]	Icon
	* 	@li 	expects type const char*, defins the title bar icon and can be name (see /gui/icons.h) or path to an image file
	*/
	Helpbox(	const std::string& Title,
			const std::string& Default_Text = std::string(),
			const int& text_mode = HELPBOX_DEFAULT_TEXT_MODE,
			const int& line_height = HELPBOX_DEFAULT_LINE_HEIGHT,
			Font* font_text = NULL,
			const char* Icon = NEUTRINO_ICON_INFO);

	///show = paint, for compatibility
	void show(bool do_save_bg = true){ paint(do_save_bg) ;}


	/**Adds an item with pre defined text
	* @param[in]	text
	* 	@li 	expects type std::string
	* @param[in]	text_mode
	* 	@li 	optional: expects type int, defines text modes, see /gui/widget/textbox.h for possible modes
	* @param[in]	line_height
	* 	@li 	optional: expects type int, defines height of line
	* @param[in]	line_indent
	* 	@li 	optional: expects type int, defines lenght of indent from left
	* @param[in]	font_text
	* 	@li 	optional: expects type Font*, default = NULL, this means SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO is used
	*/
	void addLine(	const std::string& text,
			const int& text_mode 	= HELPBOX_DEFAULT_TEXT_MODE,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);

	/**Adds an item with pre defined text
	* @param[in]	text
	* 	@li 	expects type const char *
	* @param[in]	text_mode
	* 	@li 	optional: expects type int, defines text modes, see /gui/widget/textbox.h for possible modes
	* @param[in]	line_height
	* 	@li 	optional: expects type int, defines height of line
	* @param[in]	line_indent
	* 	@li 	optional: expects type int, defines lenght of indent from left
	* @param[in]	font_text
	* 	@li 	optional: expects type Font*, default = NULL, this means SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO is used
	*/
	void addLine(	const char *text,
			const int& text_mode 	= HELPBOX_DEFAULT_TEXT_MODE,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);

	/**Adds an item with pre defined icon and text
	* @param[in]	icon
	* 	@li 	expects type std::string, icon can be name (see /gui/icons.h) or path to an image file
	* @param[in]	text
	* 	@li 	expects type std::string
	* @param[in]	text_mode
	* 	@li 	optional: expects type int, defines text modes, see /gui/widget/textbox.h for possible modes
	* @param[in]	line_height
	* 	@li 	optional: expects type int, defines height of line
	* @param[in]	line_indent
	* 	@li 	optional: expects type int, defines lenght of indent from left
	* @param[in]	font_text
	* 	@li 	optional: expects type Font*, default = NULL, this means SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO is used
	*/
	void addLine(	const std::string& icon,
			const std::string& text,
			const int& text_mode 	= HELPBOX_DEFAULT_TEXT_MODE,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);

	/**Adds an item with pre defined icon and text
	* @param[in]	icon
	* 	@li 	expects type const char *, icon can be name (see /gui/icons.h) or path to an image file
	* @param[in]	text
	* 	@li 	expects type const char *
	* @param[in]	text_mode
	* 	@li 	optional: expects type int, defines text modes, see /gui/widget/textbox.h for possible modes
	* @param[in]	line_height
	* 	@li 	optional: expects type int, defines height of line
	* @param[in]	line_indent
	* 	@li 	optional: expects type int, defines lenght of indent from left
	* @param[in]	font_text
	* 	@li 	optional: expects type Font*, default = NULL, this means SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO is used
	*/
	void addLine(	const char *icon,
			const char *text,
			const int& text_mode 	= HELPBOX_DEFAULT_TEXT_MODE,
			const int& line_height 	= HELPBOX_DEFAULT_LINE_HEIGHT,
			const int& line_indent 	= HELPBOX_DEFAULT_LINE_INDENT,
			Font* font_text = NULL);

	///adds a separator as horizontal line, arg 'line_height' defines the space of full separator height, the separator line itself has a defined height of 2px and is centered within line space.
	///arg 'line_indent' defines begin of line from left border within body object.
	///gradient is only effected, if global/theme gradient settings are enabled!
	void addSeparatorLine(	const int& line_height = HELPBOX_DEFAULT_LINE_HEIGHT,
				const int& line_indent = HELPBOX_DEFAULT_LINE_INDENT);
	///adds a simple empty separator as horizontal space, arg 'line_height' defines the space of full separator height
	void addSeparator(	const int& line_height = HELPBOX_DEFAULT_LINE_HEIGHT,
				const int& line_indent = HELPBOX_DEFAULT_LINE_INDENT);

	void addPagebreak();
	
	int exec(){return ccw_body->exec();}
};

#endif
