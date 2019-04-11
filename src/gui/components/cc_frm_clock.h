/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2015, Thilo Graf 'dbt'

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

#ifndef __CC_CLOCK__
#define __CC_CLOCK__


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/neutrinofonts.h>
#include "cc_base.h"
#include "cc_frm.h"
#include "cc_text_screen.h"
#include "cc_timer.h"
//! Sub class of CComponents. Show clock with digits on screen. 
/*!
Usable as simple fixed display or as ticking clock.
*/

class CComponentsTimer;
class CComponentsFrmClock : public CComponentsForm, public CCTextScreen
{
	private:
		CComponentsTimer *cl_timer;
		void ShowTime();
#if 0
		bool may_blit;
#endif

	protected:
		///slot for timer event, reserved for ShowTime()
		sigc::slot0<void> cl_sl_show;

		///refresh interval in seconds
		int cl_interval;

		///raw time chars
		char cl_timestr[32];

		///handle paint clock within thread and is not similar to cc_allow_paint
		bool paintClock;

		///object: font render object
		Font *cl_font;
		int cl_font_style;

		///text color
		int cl_col_text;

		///refresh mode
		bool cl_force_repaint;

		///current time format
		std::string cl_format;
		///primary time format
		std::string cl_format_str;
		///secondary time format for blink
		std::string cl_blink_str;

		///initialize clock contents  
		void initCCLockItems();
		///initialize timestring, called in initCCLockItems()
		void initTimeString();

		///start ticking clock, returns true on success, if false causes log output
		bool startClock();
		///stop ticking clock, returns true on success, if false causes log output
		bool stopClock();
		///switch between primary and secondary format
		void toggleFormat();
		///init internal font
		void initClockFont(int dx, int dy);

	public:

		CComponentsFrmClock( 	const int& x_pos = 1, const int& y_pos = 1,
					Font * font = NULL,
					const char* format_str = "%H:%M",
					const char* secformat_str = NULL,
					bool activ=false,
					const int& interval_seconds = 1,
					CComponentsForm *parent = NULL,
					int shadow_mode = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_FRAME_PLUS_0,
					fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
					fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
					int font_style = CNeutrinoFonts::FONT_STYLE_BOLD
				);
		virtual ~CComponentsFrmClock();

		/*! Sets font type for clock segments.
		 * 1st parameter expects a pointer to font type, usually a type from the global g_Font collection, but also possible
		 * are dynamic font.
		 * The use of NULL pointer enforces dynamic font.
		 * 2nd paramter is relevant for dynamic fonts only, you can use the enum types
		 * - FONT_STYLE_REGULAR
		 * - FONT_STYLE_BOLD
		 * - FONT_STYLE_ITALIC
		 * (see /.src/driver/neutrinofonts.h)
		*/
		void setClockFont(Font * font, const int& style = -1);

		///return pointer of font object
		Font* getClockFont();

		///set text color
		void setTextColor(fb_pixel_t color_text){ cl_col_text = color_text;}

		///set height of clock on screen
		void setHeight(const int& h);
		///set width of clock on screen
		void setWidth(const int& w);

		///use string expession: "%H:%M" = 12:22, "%H:%M:%S" = 12:22:12
		///set current time format string, 1st parameter set the default format, 2nd parameter sets an alternatively format for use as blink effect
		void setClockFormat(const char* prformat_str, const char* secformat_str = NULL);
		///get current time format string,
		std::string getClockFormat(){return cl_format;}

		///start and paint ticking clock
		bool Start();
		///same like Start() but for usage as simple call without return value
		void unblock(){Start();}
		///stop ticking clock, but don't hide, use kill() or hide() to remove from screen
		bool Stop();
		///same like Stop() but for usage as simple call without return value
		void block(){Stop();}
		///return true on blocked status, blocked means clock can be initalized but would be not paint, to unblock use unblock()
		bool isBlocked(void) {return !paintClock;}

		///returns true, if clock is running
		bool isRun() const {return cl_timer ? cl_timer->isRun() : false;}
		///set refresh interval in seconds, default value=1 (=1 sec)
		void setClockInterval(const int& seconds){cl_interval = seconds;}

		///show clock on screen
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		///hide clock on screen
		void hide(){Stop(); CComponentsForm::hide();}
		///does the same like kill() from base class, but stopping clock before kill
		void kill(const fb_pixel_t& bg_color = COL_BACKGROUND_PLUS_0, bool ignore_parent = false);

		///reinitialize clock contents
		void refresh() { initCCLockItems(); }

		///enables force to repaint of all segments on each interval, Note: repaint of all segemts is default enabled.
		void enableForceSegmentPaint(bool enable = true){cl_force_repaint = enable;}
		///disables repaint of all segments on each interval, repaint happens only on changed segment value
		void disableForceSegmentPaint(){enableForceSegmentPaint(false);}

		/**Member to modify background behavior of embeded segment objects
		* @param[in]  mode
		* 	@li bool, default = true
		* @return
		*	void
		* @see
		* 	Parent member: CCTextScreen::enableTboxSaveScreen()
		* 	CTextBox::enableSaveScreen()
		* 	disableTboxSaveScreen()
		*/
		void enableTboxSaveScreen(bool mode);

		///set color gradient on/off, returns true if gradient mode was changed
		bool enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_color = 255 /*=COL_BACKGROUND*/);
#if 0
		///enable/disable automatic blitting
		void setBlit(bool _may_blit = true) { may_blit = _may_blit; }
#endif
};

#endif
