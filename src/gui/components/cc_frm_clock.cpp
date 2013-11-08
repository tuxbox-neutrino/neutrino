/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Generic GUI-related component.
	Copyright (C) 2013, Thilo Graf 'dbt'

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>

#include "cc_frm_clock.h"
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

using namespace std;

CComponentsFrmClock::CComponentsFrmClock()
{
	initVarClock();
}

CComponentsFrmClock::CComponentsFrmClock( const int x_pos, const int y_pos, const int w, const int h,
						const char* format_str, bool activ, bool has_shadow,
						fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow)

{
	initVarClock();

	x 		= x_pos;
	y 		= y_pos;
	width 		= w;
	height	 	= h;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_body;
	col_shadow	= color_shadow;

	paintClock	= false;
	activeClock	= activ;
	if (activeClock)
		startThread();

	cl_format_str	= format_str;
}

void CComponentsFrmClock::initVarClock()
{
	initVarForm();
	cc_item_type 	= CC_ITEMTYPE_FRM_CLOCK;
	corner_rad	= RADIUS_SMALL;

	cl_font		= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO];
	cl_col_text	= COL_MENUCONTENT_TEXT;
	cl_format_str	= "%H:%M";
	cl_align	= CC_ALIGN_VER_CENTER | CC_ALIGN_HOR_CENTER;

	cl_thread 	= 0;
	cl_interval	= 1;
}

CComponentsFrmClock::~CComponentsFrmClock()
{
	cleanCCForm();
	if (activeClock)
		stopThread();
}

void CComponentsFrmClock::initTimeString()
{
	struct tm t;
	time_t ltime;
	ltime=time(&ltime);
	strftime((char*) &cl_timestr, sizeof(cl_timestr), cl_format_str, localtime_r(&ltime, &t));
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
	initTimeString();
	string s_time = cl_timestr;
	
	//get minimal required height, width from raw text
	int min_text_w = cl_font->getRenderWidth(s_time, true);;
	int min_text_h = cl_font->getHeight();
	height = max(height, min_text_h);
	width = max(width, min_text_w);

	int cl_x = 0;
	int cl_h = min_text_h;
	int cl_y = 0;
	int w_lbl_tmp = 0;
	
	//create label objects and add to container, ensure count of items = count of chars (one char = one segment)
	if (v_cc_items.size() != s_time.size()){
		
		//clean up possible old items before add new items
		clearCCItems();

		//create new empty label objects, set some general properties and add to container
		for (size_t i = 0; i < s_time.size(); i++){
			CComponentsLabel * lbl = new CComponentsLabel();
			addCCItem(lbl);
			
			//background paint of item is not required
			lbl->doPaintBg(false);
			
			//set corner properties of label item
			lbl->setCorner(corner_rad-fr_thickness, corner_type);

			//set text border to 0
			lbl->setTextBorderWidth(0,0);
		}
	}
	
	//modifie available lable items with current segment chars
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
				lbl->setCornerType(0);// inner items
		}

		//extract timestring segment (char)
		string stmp = s_time.substr(i, 1);

		//get width of current segment
		int wtmp = cl_font->getRenderWidth(stmp, true);

		//set size, text, color of current item
		lbl->setDimensionsAll(cl_x, cl_y, wtmp, cl_h);
		lbl->setTextColor(cl_col_text);
		lbl->setColorAll(col_frame, col_body, col_shadow);
		lbl->setText(stmp, CTextBox::CENTER, cl_font);

		//use matching height for digits for better vertical centerring into form
		CTextBox* ctb = lbl->getCTextBoxObject();
		if (ctb)
			ctb->setFontUseDigitHeight();

		//ensure paint of text and label bg on changed text or painted form background
		bool force_txt_and_bg = (lbl->textChanged() || this->paint_bg);
		lbl->forceTextPaint(force_txt_and_bg);

		//set xpos of item
		cl_x += wtmp;

		lbl->setWidth(wtmp);

		//set current width for form
		w_lbl_tmp += wtmp;
	}

	//set required width
	width = max(width, w_lbl_tmp);

	initSegmentAlign(&w_lbl_tmp, &min_text_h);
}

//handle alignment
void CComponentsFrmClock::initSegmentAlign(int* segment_width, int* segment_height)
{	
	int wadd = 0;
	int hadd = 0;
	int* w_lbl_tmp = segment_width;
	int* min_text_h = segment_height;

	//use first item as reference and set x and y position to the 1st segement item with definied alignment
	if (cl_align & CC_ALIGN_RIGHT){
		wadd = width-*w_lbl_tmp;
		v_cc_items[0]->setXPos(wadd);
	}
	else if (cl_align & CC_ALIGN_LEFT){
		v_cc_items[0]->setXPos(wadd);
	}
	else if  (cl_align & CC_ALIGN_HOR_CENTER){
		hadd = height/2-*min_text_h/2;
		v_cc_items[0]->setYPos(hadd);
	}

	if (cl_align & CC_ALIGN_TOP){
		v_cc_items[0]->setYPos(hadd);
	}
	else if  (cl_align & CC_ALIGN_BOTTOM){
		hadd = height-*min_text_h;
		v_cc_items[0]->setYPos(hadd);
	}
	else if  (cl_align & CC_ALIGN_VER_CENTER){
		wadd = width/2-*w_lbl_tmp/2;
		v_cc_items[0]->setXPos(wadd);
	}

	//set all evaluated position values to all other segement items
	for (size_t i = 1; i < v_cc_items.size(); i++){
		wadd += v_cc_items[i-1]->getWidth();
		v_cc_items[i]->setPos(wadd, hadd);
	}
}

//thread handle
void* CComponentsFrmClock::initClockThread(void *arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,0);
 	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS,0);

	CComponentsFrmClock *clock = static_cast<CComponentsFrmClock*>(arg);
	time_t count = time(0);
	//start loop for paint
	while(1) {
		sleep(clock->cl_interval);

		if (clock->paintClock) {
			//refresh item property values
			clock->refresh();
			
			//paint segements, but wihtout saved backgrounds
			clock->paint(CC_SAVE_SCREEN_NO);
			count = time(0);
		}
		if (time(0) >= count+30) {
			clock->cl_thread = 0;
			break;
		}
	}
	return 0;
}

//start up ticking clock with own thread, return true on succses
bool CComponentsFrmClock::startThread()
{
	void *ptr = static_cast<void*>(this);
	
	if(!cl_thread) {
		int res = pthread_create (&cl_thread, NULL, initClockThread, ptr) ;
		if (res != 0){
			printf("[CComponentsFrmClock]    [%s]  pthread_create  %s\n", __FUNCTION__, strerror(errno));
			return false;
		}
	}
	return  true;
}

//stop ticking clock and kill thread, return true on succses
bool CComponentsFrmClock::stopThread()
{
	if(cl_thread) {
		int res = pthread_cancel(cl_thread);
		if (res != 0){
			printf("[CComponentsFrmClock]    [%s] pthread_cancel  %s\n", __FUNCTION__, strerror(errno));
			return false;
		}

		res = pthread_join(cl_thread, NULL);
		if (res != 0){
			printf("[CComponentsFrmClock]    [%s] pthread_join  %s\n", __FUNCTION__, strerror(errno));
			return false;
		}
	}
	cl_thread = 0;
	return true;
}

bool CComponentsFrmClock::Start()
{
	if (!activeClock)
		return false;
	if (!cl_thread)
		startThread();
	if (cl_thread) {
		//ensure paint of segements on first paint
		paint();
		paintClock = true;
	}
	return cl_thread == 0 ? false : true;
}

bool CComponentsFrmClock::Stop()
{
	if (!activeClock)
		return false;
	paintClock = false;
	return cl_thread == 0 ? false : true;
}

void CComponentsFrmClock::paint(bool do_save_bg)
{
	//prepare items before paint
	initCCLockItems();

	//paint form contents
	paintForm(do_save_bg);
}
