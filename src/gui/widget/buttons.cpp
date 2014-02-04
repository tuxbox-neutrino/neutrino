/*
 * $Id: buttons.cpp,v 1.10 2010/07/12 08:24:55 dbt Exp $
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 * (C) 2011 B1 Systems GmbH, Author: Stefan Seyfried
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
//#include <driver/stacktrace.h>



/* paintButtons usage,
   use this function for painting icons with captions in horizontal or vertical direction.
   Icons automatically use the available maxwidth.
   If not enough space is available, captions are shortened.
   If there is extra space, space between icons is increased.

 * x 		set horizontal startposition
 * y 		set vertical startposition
 * footerwidth	set width of buttonbar as similar to footer, value 0 (default) means: do nothing,
		then paint it extra
 * count	set count of buttons
 * content	set struct buttonlabel with iconfile and locales, for an empty text let locale
		constant empty, so you can paint icons without captions,
 * maxwidth	maximum horizontal space for the buttons
 * footerheight	set height of buttonbar as similar to footer, value 0 (default) means:
		value calculates automaticly depends of maximal height of icon and caption

 * stuff below here was obviously not tested recently
 * vertical_paint	optional, default value is false (horizontal) sets direction of painted buttons
 * fcolor  		optional, default value is COL_INFOBAR_SHADOW_TEXT, use it to render font with other color
 * alt_buttontext	optional, default NULL, overwrites button caption at definied buttonlabel id (see parameter alt_buttontext_id) with this text
 * alt_buttontext_id	optional, default 0, means id from buttonlable struct which text you will change 
 * show			optional, default value is true (show button), if false, then no show and return the height of the button.
 */

int paintButtons(	const int &x,	
			const int &y, 
			const int &footerwidth, 
			const uint &count, 
			const struct button_label * const content,
			const int &maxwidth,
			const int &footerheight,
			std::string /* just to make sure nobody uses anything below */,
			bool vertical_paint,
			const uint32_t fcolor,
			const char * alt_buttontext,
			const uint &buttontext_id,
			bool show,
			const std::vector<neutrino_locale_t>& /*all_buttontext_id*/)
{
	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	Font * font = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];
	uint cnt = count;
	int x_footer = x;
	int y_footer = y;
	int w_footer = footerwidth;
	int h_footer = 0;
	
	int w_space 	= 10; //minimal space between buttons
	int h_space	= 4; //minimal space between caption and/or icon and border
	int x_icon 	= x_footer + w_space;
	int x_caption 	= 0;
	
	int x_button = x_icon;
	int h_button = 0;
	
	//calculate max of h + w
	//icon
	int h_max_icon = 0;
	int w_icons = 0;
	
	//text
	int w_text = 0;
	int h_max_text = font->getHeight();

	int count_icons = 0;
	int count_labels = 0;
	/* more than 16 buttons? noooooo*/
	int iconw[16];
	int iconh[16];
	int fwidth[16];
	const char *buttontext[16];

	/* sanity check... */
	if (count > 16 || count == 0)
	{
		fprintf(stderr, "paintButtons does only support max 16 buttons yet (%d)\n", count);
		//print_stacktrace();
		return 0;
	}
	if (maxwidth < 200 && show)
	{
		fprintf(stderr, "paintButtons: maxwidth very small\n");
		fprintf(stderr, "  x: %d y: %d footw: %d count: %d maxw: %d footh: %d\n ",
				x, y, footerwidth, count, maxwidth, footerheight);
		//print_stacktrace();
	}

	uint i;
	for (i = 0; i < cnt; i++)
	{
		//icon
		int w = 0;
		int h = 0;
		frameBuffer->getIconSize(content[i].button, &w, &h);
		iconw[i] = w;
		iconh[i] = h;
		h_max_icon = std::max(h_max_icon, h);
		w_icons += w;
		if (w)
			count_icons++;

		if (content[i].locale) {
			buttontext[i] = g_Locale->getText(content[i].locale);
			//text width
			if (alt_buttontext != NULL && i == buttontext_id) 
				fwidth[i] = font->getRenderWidth(alt_buttontext, true); //...with an alternate buttontext
			else
				fwidth[i] = font->getRenderWidth(buttontext[i], true);
			w_text += fwidth[i];
			count_labels++;
		} else {
			buttontext[i] = "";
			fwidth[i] = 0;
		}
	}

	//calculate button heigth
	h_button = std::max(h_max_icon, h_max_text); //calculate optimal button height
	
	//calculate footer heigth
	h_footer = footerheight == 0 ? (h_button + 2*h_space) : footerheight;

	if (!show)
		return h_footer;

	//paint footer
	if (w_footer > 0)
		frameBuffer->paintBoxRel(x_footer, y_footer, w_footer, h_footer, COL_INFOBAR_SHADOW_PLUS_1, RADIUS_LARGE, CORNER_BOTTOM); //round

	
	//baseline
	int y_base = y_footer + h_footer/2;
	int spacing = maxwidth - w_space * 2 - w_text - w_icons - (count_icons + count_labels - 1) * h_space;
#if 0
	/* debug */
	fprintf(stderr, "PB: sp %d mw %d w_t %d w_i %d w_s %d c_i %d\n",
		spacing, maxwidth, w_text, w_icons, w_space, count_items);
#endif
	if (fwidth[cnt - 1] == 0) /* divisor needs to be labels+1 unless rightmost icon has a label */
		count_labels++;   /* side effect: we don't try to divide by 0 :-) */
	if (spacing >= 0)
	{				 /* add half of the inter-object space to the */
		spacing /= count_labels; /* left and right (this might break vertical */
		x_button += spacing / 2; /* alignment, but nobody is using this (yet) */
	}				 /* and I'm don't know how it should work.    */
	else
	{
		/* shorten captions relative to their length */
		for (i = 0; i < cnt; i++)
			fwidth[i] = (fwidth[i] * (w_text + spacing)) / w_text; /* spacing is negative...*/
		spacing = 0;
	}

	for (uint j = 0; j < cnt; j++)
	{
		const char * caption = NULL;
		//set caption... 
		if (alt_buttontext != NULL && j == buttontext_id) 
			caption = alt_buttontext; //...with an alternate buttontext
		else
			caption = buttontext[j];

		const char * icon = content[j].button ? content[j].button : "";

		// calculate baseline startposition of icon and text in y
 		int y_caption = y_base + h_max_text/2+1;
		
		// paint icon and text
		frameBuffer->paintIcon(icon, x_button , y_base - iconh[j]/2);
		x_caption = x_button + iconw[j] + h_space;
		font->RenderString(x_caption, y_caption, fwidth[j], caption, fcolor, 0, true);
 		
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
			/* increase x position */
			x_button = x_caption;
			if (fwidth[j])
				x_button += fwidth[j] + spacing + h_space;
		}	
	}

	return h_footer;
}
