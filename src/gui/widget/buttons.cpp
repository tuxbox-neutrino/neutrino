/*
 * $Id: buttons.cpp,v 1.10 2010/07/12 08:24:55 dbt Exp $
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include <gui/widget/buttons.h>
#include <gui/customcolor.h>
#include <system/settings.h>



/* paintButtons usage, 
   use this fucntion for painting icons with captions in horizontal or vertical direction. 

 * x 			set horizontal startposition
 * y 			set vertical startposition
 * footerwidth		set width of buttonbar as similar to footer, value 0 (default) means: do nothing, then paint it extra
 * count		set count of buttons
 * content		set struct buttonlabel with iconfile and locales, for an empty text let locale constant empty, so you can paint icons without captions, 
 * width		width of footer, contains the buttons 
 * footerheigth		set height of buttonbar as similar to footer, value 0 (default) means: value calculates automaticly depends of maximal height of icon and caption
 * buttonwidth		set width of buttonlabel include icon, caption and offsets, value 0 (default), calculates automaticly the buttonwidth, buttons will be paint like a chain
 * vertical_paint	optional, default value is false (horizontal) sets direction of painted buttons
 * fcolor  		optional, default value is COL_INFOBAR_SHADOW, use it to render font with other color
 * alt_buttontext	optional, default NULL, overwrites button caption at definied buttonlabel id (see parameter alt_buttontext_id) with this text
 * alt_buttontext_id	optional, default 0, means id from buttonlable struct which text you will change 
 */

// 		y-------+										+-----------+
// 			|	ID0				 ID1					|
// 			| +-----buttonwidth------+---------+-----buttonwidth------+			|
// 			| [icon][w_space][caption][w_space][icon][w_space][caption]			|	footerheight
// 			| 										|
// 		rounded	+----------------------------------footerwidth----------------------------------+rounded----+								|
// 			|
// 			x
// 	
int paintButtons(	const int &x,	
			const int &y, 
			const int &footerwidth, 
			const uint &count, 
			const struct button_label * const content,
			const int &footerheight,
			const int &buttonwidth,
			bool vertical_paint,
			const unsigned char fcolor,
			const char * alt_buttontext,
			const uint &buttontext_id)
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	Font * font = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];
	uint cnt = count;
	int x_footer = x;
	int y_footer = y;
	int w_footer = footerwidth;
	int h_footer = 0;
	
	int w_space 	= 4; //minimal space between caption to icon and between buttons and between border to caption
	int x_icon 	= x_footer+20;
	int x_caption 	= 0;
	
	int x_button = x_icon;
	int w_button = 0;
	int h_button = 0;
	
	//calculate max of h + w
	//icon
	int h_max_icon = 0;
	int w_max_icon = 0;
	
	//text
	int w_max_text = 0;
	int h_max_text = font->getHeight();

	for (uint i = 0; i < cnt; i++)
	{
		//icon
		int w = 0;
		int h = 0;
		frameBuffer->getIconSize(content[i].button, &w, &h);
		h_max_icon = std::max(h_max_icon, h);
		w_max_icon = std::max(w_max_icon, w);

		//text
		const char * buttontext =  content[i].locale ? g_Locale->getText(content[i].locale) : "";
		
		//text width
		int fwidth = font->getRenderWidth(buttontext, true);
		w_max_text = std::max(w_max_text, fwidth);
	}
	
	//calculate button width
	w_button = buttonwidth == 0 ? (w_max_icon + w_space + w_max_text) : buttonwidth;
	
	//calculate button heigth
	h_button = std::max(h_max_icon, h_max_text); //calculate optimal button height
	
	//calculate footer heigth
	h_footer = footerheight == 0 ? (h_button + 2*w_space) : footerheight;

	//paint footer
	if (w_footer > 0)
		frameBuffer->paintBoxRel(x_footer, y_footer, w_footer, h_footer, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_LARGE, CORNER_BOTTOM); //round

	
	//baseline
	int y_base = y_footer + h_footer/2;
	
	
	for (uint j = 0; j < cnt; j++)
	{
		const char * caption = NULL;
		//set caption... 
		if (alt_buttontext != NULL && j == buttontext_id) 
			caption = alt_buttontext; //...with an alternate buttontext
		else
			caption =  content[j].locale ? g_Locale->getText(content[j].locale) : ""; 
		
		const char * icon = content[j].button ? content[j].button : "";

		//get height/width of icon
		int iconw, iconh;
		frameBuffer->getIconSize(content[j].button, &iconw, &iconh);
					
		// calculate baseline startposition of icon and text in y
 		int y_caption = y_base + h_max_text/2+1;
		
		// paint icon and text
		frameBuffer->paintIcon(icon, x_button , y_base-iconh/2);
		x_caption = x_button + iconw + w_space;
		font->RenderString(x_caption, y_caption, w_max_text, caption, fcolor, 0, true); // UTF-8
 		
 		/* 	set next startposition x, if text is length=0 then offset is =renderwidth of icon, 
  		* 	for generating buttons without captions, 
  		*/		
 		
 		int lentext = strlen(caption);	
		if (vertical_paint) 
		// set x_icon for painting buttons with vertical arrangement 
		{
				if (lentext !=0)
				{
					x_button = x;	
					y_base += h_footer;				
				}
				else
				{
					x_button = x_caption;		
				}
		}
		else
		{
			//set x_icon for painting buttons with horizontal arrangement as default
			x_button = lentext !=0 ? (x_button + w_button + 2*w_space) : x_button;
		}	
	}

	return h_footer;
}
