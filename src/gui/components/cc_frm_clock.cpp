/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Generic GUI-related component.
	Copyright (C) 2013-2015 Thilo Graf 'dbt'

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

#include "cc_frm_clock.h"

#include <time.h>

#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <system/helpers.h>
#include <system/debug.h>
#include <driver/fontrenderer.h>

using namespace std;

CComponentsFrmClock::CComponentsFrmClock( 	const int& x_pos,
						const int& y_pos,
						Font * font,
						const char* prformat_str,
						const char* secformat_str,
						bool activ,
						const int& interval_seconds,
						CComponentsForm* parent,
						int shadow_mode,
						fb_pixel_t color_frame,
						fb_pixel_t color_body,
						fb_pixel_t color_shadow,
						int font_style
					)

{
	cc_item_type.id 	= CC_ITEMTYPE_FRM_CLOCK;
	cc_item_type.name 	= "cc_clock";

	x = cc_xr = x_old = x_pos;
	y = cc_yr = y_old = y_pos;

	shadow		= shadow_mode;
	shadow_w	= OFFSET_SHADOW;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	corner_rad	= RADIUS_SMALL;

	//init default format and  text color
	setClockFormat(prformat_str, secformat_str);
	cl_col_text	= COL_MENUCONTENT_TEXT;

	//enable refresh of all segments on each interval as default
	cl_force_repaint = true;

	//init default font
	cl_font 	= font;
	cl_font_style	= font_style;
	if (cl_font == NULL)
		initClockFont(0, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight());

	//init general clock dimensions
	height 	= height_old= cl_font->getHeight();
	width 	= width_old = cl_font->getRenderWidth(cl_format_str);

	//set default text background behavior
	cc_txt_save_screen = false;

	//set default running clock properties
	cl_interval	= interval_seconds;
	cl_timer 	= NULL;
	cl_blocked	= true;
#if 0
	may_blit		= true;
#endif

	//general init
	initCCLockItems();
	initParent(parent);

	//init slot for running clock
	cl_sl_show = sigc::mem_fun(*this, &CComponentsFrmClock::ShowTime);

	//run clock already if required
	if (activ)
		startClock();
}

CComponentsFrmClock::~CComponentsFrmClock()
{
	if (cl_timer){
		delete cl_timer; cl_timer = NULL;
	}
}

void CComponentsFrmClock::initClockFont(int dx, int dy)
{
	setClockFont(*CNeutrinoFonts::getInstance()->getDynFont(dx, dy, cl_format_str, cl_font_style));
}


void CComponentsFrmClock::initTimeString()
{
	struct tm t;
	time_t ltime;
	ltime=time(&ltime);

	toggleFormat();

	strftime(cl_timestr, sizeof(cl_timestr), cl_format.c_str(), localtime_r(&ltime, &t));
}

//formating time string with possible blink string
void CComponentsFrmClock::toggleFormat()
{
	if (cl_format_str.length() != cl_blink_str.length())
		kill();

	if (cl_format == cl_blink_str)
		cl_format = cl_format_str;
	else
		cl_format = cl_blink_str;
}

//set current time format string
void CComponentsFrmClock::setClockFormat(const char* prformat_str, const char* secformat_str)
{
	cl_format_str = prformat_str;

	if (secformat_str == NULL)
		cl_blink_str = cl_format_str;
	else
		cl_blink_str = secformat_str;
}

// How does it works?
// We don't paint complete date or time string at once, because of individual possible formats,
// so we split timestring and assign only one char to one lable (segment)

//	x/y	    width
//	+--------------------------+
//	||lbl0|lbl1|lbl2|lbl3|lbl4||
//	||    |    |    |    |    ||height
//	||    |    |    |    |    ||
//	+--------------------------+

// So every item (lable or segment) contains only one char depending on specified format. eg: format %H:%M gives 5 items.
// The effort is slightly greater and it's necessary to avoid flicker effect, but it's more flexible.

void CComponentsFrmClock::initCCLockItems()
{
	//prepare and set current time string
	initTimeString();
	string s_time = cl_timestr;

	/* create label objects and add to container, ensure that count of items = count of chars (one char = one segment)
	 * this is required for the case, if any time string format was changed
	*/
	if (v_cc_items.empty() || (v_cc_items.size() != s_time.size())){
		//exit on empty time string
		if (s_time.empty()){
			clear();
			return;
		}

		//clean up possible old items before add new items
		clear();

		//create new empty label objects, set some general properties and add to container
		for (size_t i = 0; i < s_time.size(); i++){
			CComponentsLabel * lbl = new CComponentsLabel();
			addCCItem(lbl);

			//background paint of item is not required
			lbl->doPaintBg(false);

			//preset corner properties of label item
			lbl->setCorner(max(0, corner_rad-fr_thickness), corner_type);

			//set text border to 0
			lbl->setTextBorderWidth(0,0);
		}
	}

	/*calculate minimal separator width, we use char size of some possible chars
	 * TODO: it's not really generic at the moment
	*/
	int minSepWidth = 0;
	string sep[] ={" ", ".", ":"};
	for (size_t i = 0; i < sizeof(sep)/sizeof(sep[0]); i++)
		minSepWidth = max(cl_font->getRenderWidth(sep[i]), minSepWidth);

	//get minimal required dimensions for segements from current format string
	int w_text_min = max(cl_font->getRenderWidth(s_time), width);
	int h_text_min = max(cl_font->getHeight(), height);

	//init some temporary variables
	int x_tmp = x;
	int h_tmp = h_text_min;
	int y_tmp = y;

	//summary of all segments (labels)
	int w_segments = 0;

	const bool force_repaint = cl_force_repaint;

	/* modify available label items with current segment chars
	 * we are using segments with only one char per segment,
	 * these chars are predefined via format string
	*/
	for (size_t i = 0; i < v_cc_items.size(); i++)
	{
		//v_cc_items are only available as CComponent-items here, so we must cast them before
		CComponentsLabel *lbl = static_cast <CComponentsLabel*> (v_cc_items[i]);

		//add rounded corners only to 1st and last segment
		if (corner_type) {
			if (i == 0)				
				lbl->setCornerType(corner_type & CORNER_LEFT);// 1st label item
			else if (i == v_cc_items.size()-1)	
				lbl->setCornerType(corner_type & CORNER_RIGHT);// last label item
			else			
				lbl->setCorner(0,CORNER_NONE);// inner items don't need round corners
		}

		//extract timestring segment (char)
		string stmp = s_time.substr(i, 1);

		int w_tmp = 0;
		//get width of current segment
		if (isdigit(stmp.at(0)) ) //check for digits, if true, we use digit width
			w_tmp = cl_font->getMaxDigitWidth();
		else //not digit found, we use render width or minimal width
			w_tmp = max(cl_font->getRenderWidth(stmp), minSepWidth);
		//lbl->enablePaintCache();

		//set size, text, color of current item
		lbl->setDimensionsAll(x_tmp, y_tmp, w_tmp, h_tmp);
		lbl->setColorAll(col_frame, col_body, col_shadow);
		lbl->forceTextPaint(force_repaint);
		lbl->setText(stmp, CTextBox::CENTER, cl_font, cl_col_text, cl_font_style);

		//init background behavior of segment
		//printf("[CComponentsFrmClock]   [%s - %d] paint_bg: [%d] gradient_mode = [%d], text save screen mode = [%d]\n", __func__, __LINE__, paint_bg, cc_body_gradient_enable, cc_txt_save_screen);
		lbl->doPaintBg(false);
		lbl->doPaintTextBoxBg(paint_bg);
		bool save_txt_screen = cc_txt_save_screen || (!paint_bg || cc_body_gradient_enable);
		lbl->enableTboxSaveScreen(save_txt_screen);
#if 0
		//use matching height for digits for better vertical centerring into form
		CTextBox* ctb = lbl->getCTextBoxObject();
		if (ctb)
			ctb->setFontUseDigitHeight();

		//ensure paint of text and label bg on changed text or painted form background
		bool force_txt_and_bg = (lbl->textChanged() || this->paint_bg);
		lbl->forceTextPaint(force_txt_and_bg);
#endif
		//set xpos and width of item (segment)
		lbl->setWidth(w_tmp);
		x_tmp += w_tmp;

		//sum required width for clock (this)
		w_segments += w_tmp;
		h_text_min = max(lbl->getHeight(), height);
		height = max(lbl->getHeight(), height);
	}

	//set required width for clock (this)
	width = max(w_text_min, w_segments);

	//use first item as reference and set x and y position to the 1st segement item with definied alignment
	int x_lbl = width/2-w_segments/2;
	v_cc_items[0]->setXPos(x_lbl);

	int y_lbl = height/2-h_text_min/2;
	v_cc_items[0]->setYPos(y_lbl);

	//set all evaluated position values to all other segement items
	for (size_t i = 1; i < v_cc_items.size(); i++){
		x_lbl += v_cc_items[i-1]->getWidth();
		v_cc_items[i]->setPos(x_lbl, y_lbl);
	}
}


//this member is provided for slot with timer event "OnTimer"
void CComponentsFrmClock::ShowTime()
{
	if (!cl_blocked) {
		//paint segements, but wihtout saved backgrounds
		paint(CC_SAVE_SCREEN_NO);
	}
}

//start up ticking clock controled by timer with signal/slot, return true on succses
bool CComponentsFrmClock::startClock()
{
	if (cl_interval <= 0){
		dprintf(DEBUG_NORMAL, "[CComponentsFrmClock]    [%s]  clock is set to active, but interval is initialized with value %d ...\n", __func__, cl_interval);
		return false;
	}

	if (cl_timer == NULL)
		cl_timer = new CComponentsTimer(0);

	cl_timer->setThreadName(getItemName());

	if (cl_timer->OnTimer.empty()){
		dprintf(DEBUG_INFO,"\033[33m[CComponentsFrmClock]\t[%s] init slot...\033[0m\n", __func__);
		cl_timer->OnTimer.connect(cl_sl_show);
		force_paint_bg = true;
	}

	cl_timer->setTimerInterval(cl_interval);

	if (cl_timer->startTimer())
		return true;
	
	return  false;
}

//stop ticking clock and internal timer, return true on succses
bool CComponentsFrmClock::stopClock()
{
	if (cl_timer){
		if (cl_timer->stopTimer()){
			dprintf(DEBUG_INFO, "[CComponentsFrmClock]    [%s]  stopping clock...\n", __func__);
			clear();
			delete cl_timer;
			cl_timer = NULL;
			return true;
		}
		else
			dprintf(DEBUG_NORMAL, "[CComponentsFrmClock]    [%s]  stopping timer failed...\n", __func__);
	}
	return false;
}

bool CComponentsFrmClock::Start()
{
	if (startClock()) {
		cl_blocked = !cc_allow_paint;
		return true;
	}
	return false;
}

bool CComponentsFrmClock::Stop()
{
	if (stopClock()){
		cl_blocked = true;
		return true;
	}

	return false;
}

void CComponentsFrmClock::paint(const bool &do_save_bg)
{
	//prepare items before paint
	initCCLockItems();

	if (!is_painted)
		force_paint_bg = false;

	//paint form contents
	CComponentsForm::paint(do_save_bg);

}

void CComponentsFrmClock::setClockFont(Font *font, const int& style)
{
	if (cl_font != font || cl_font_style != style){
		if (cl_font != font)
			cl_font = font;
		if (style != -1)
			cl_font_style = style;
	}
	initCCLockItems();
}

Font* CComponentsFrmClock::getClockFont()
{
	return cl_font;
}

void CComponentsFrmClock::kill(const fb_pixel_t& bg_color, bool ignore_parent)
{
	Stop();
	CComponentsForm::kill(bg_color, ignore_parent);
}

void CComponentsFrmClock::enableTboxSaveScreen(bool mode)
{
	if (cc_txt_save_screen == mode || v_cc_items.empty())
		return;
	cc_txt_save_screen = mode;
	for (size_t i = 0; i < v_cc_items.size(); i++){
		CComponentsLabel *seg = static_cast <CComponentsLabel*>(v_cc_items[i]);
		seg->enableTboxSaveScreen(cc_txt_save_screen);
	}
}

void CComponentsFrmClock::setHeight(const int& h)
{
	if (h == height)
		return;

	int f_height = cl_font->getHeight();
	if (h != f_height){
		dprintf(DEBUG_DEBUG, "\033[33m[CComponentsFrmClock]\t[%s - %d], font height is different than current height [%d], using [%d]  ...\033[0m\n", __func__, __LINE__, h, f_height);
		CComponentsItem::setHeight(f_height);
	}else
		CComponentsItem::setHeight(h);
	initCCLockItems();
}

void CComponentsFrmClock::setWidth(const int& w)
{
	if (w == width)
		return;

	int f_width = cl_font->getRenderWidth(cl_format_str);
	if (w != f_width){
		dprintf(DEBUG_NORMAL, "\033[33m[CComponentsFrmClock]\t[%s - %d], font width is different than current width [%d], using [%d]  ...\033[0m\n", __func__, __LINE__, w, f_width);
		CComponentsItem::setWidth(f_width);
	}else
		CComponentsItem::setWidth(w);
	initCCLockItems();
}

bool CComponentsFrmClock::enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_color)
{
	if (CCDraw::enableColBodyGradient(enable_mode, sec_color)){
		for (size_t i = 0; i < v_cc_items.size(); i++)
			static_cast <CComponentsLabel*>(v_cc_items[i])->getCTextBoxObject()->clearScreenBuffer();
		return true;
	}
	return false;
}
