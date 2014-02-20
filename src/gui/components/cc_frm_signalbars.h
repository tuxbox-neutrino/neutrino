/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Class for signalbar based up CComponent classes.
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

#ifndef __SIGNALBARS_H__
#define __SIGNALBARS_H__


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <gui/components/cc_frm.h>
#include <gui/components/cc_item_progressbar.h>
#include <gui/components/cc_item_text.h>
#include <zapit/include/zapit/frontend_c.h>
#include <driver/neutrinofonts.h>

/// Basic class for signalbars
/*!
Basic attributes and member functions for items.
These class provides basic attributes and members to show frontend values in signalbars.
CSignalBar() and their sub classes based up CComponentsForm() and are usable like other CComponentsItems()

CSignalBar() is intended to show signal rate.
*/

class CSignalBar : public CComponentsForm
{
	public:
		///refresh current item properties, use this before paintScale().
		void Refresh();

	protected:
		///object: current frontend
		CFrontend	 	*sb_frontend;
		///object: scale bar
		CProgressBar 		*sb_scale;
		///object: value caption
		CComponentsLabel 	*sb_vlbl;
		///object: caption for signal name
		CComponentsLabel 	*sb_lbl;
		///object: current font
		Font			*sb_font;
		///object: dynamic font object handler
		CNeutrinoFonts 		*dy_font;
		///property: text color, see also setTextColor()
		fb_pixel_t 		sb_caption_color;

		///property: item top position
		int sb_item_top;
		///property: height of items
		int sb_item_height;
		///property: height of scale
		int sb_scale_height;
		///property: width of progressbar
		int sb_scale_width;
		///property: width of value caption
		int sb_vlbl_width;
		///property: width of caption
		int sb_lbl_width;
		///property: text mode of value, predefined type = CTextBox::NO_AUTO_LINEBREAK | CTextBox::CENTER
		int sb_val_mode;

		///cache last assingned signal value
		int sb_lastsig;
		///current signal value
		uint16_t sb_signal;

		///initialize all needed basich attributes and objects
		void initVarSigBar();
		///initianlize position and dimensions of signalbar container
		void initDimensions();
		///initialize scale object
		void initSBarScale();
		///initialize value caption object, this contains the value of current frontend data, signal or noise rate
		void initSBarValue();
		///initialize caption object, this contains the unit (e.g %) or name of value (e.g. SIG)
		void initSBarName();

		///initialize all required objects at once, see also Refresh()
		void initSBItems();

		///reinitialize current signal values and paint new values, required after Refresh()
		void paintScale();

		///property: contains the name of signal type in the caption object, see also setName()
		std::string sb_name;

	public:
		CSignalBar();
		///basic component class constructor for signal.
		CSignalBar(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref, const std::string& sb_name = "SIG");

		///assigns the current used frontend, simplified a tuner object, see frontend_c.h
		virtual void setFrontEnd(CFrontend *frontend_ref){sb_frontend = frontend_ref;};
		///assigns font for caption
		virtual void setTextFont(Font* font_text){sb_font = font_text;};
		///sets the caption color, see also property 'sb_caption_color'
		virtual void setTextColor(const fb_pixel_t& caption_color){ sb_caption_color = caption_color;};
		///assigns the height of scale
		virtual void setScaleHeight(const int& scale_height){sb_scale_height = scale_height;};
		///assigns the width of scale
		virtual void setScaleWidth(const int & scale_width){sb_scale_width = scale_width;};
		///assigns the name of signal value in the caption object, see also sb_name
		virtual void setName(const std::string& name){sb_name = name;};

		///returns the scale object, type = CProgressBar*
		virtual CProgressBar* getScaleObject(){return sb_scale;};
		///returns the value label object, type = CComponentsLabel*
		virtual CComponentsLabel* getLabelValObject(){return sb_vlbl;};
		///returns the name label object, type = CComponentsLabel*
		virtual CComponentsLabel* getLabelNameObject(){return sb_lbl;};
		/// return the scale width
		virtual int getScaleWidth(){ return sb_scale_width;};

		///paint this items
		virtual void paint(bool do_save_bg);

		//returns the current signal value
		uint16_t getValue(void) { return sb_signal; }
};

/// Sub class of CSignalBar()
/*!
This class use basic attributes and members from CSignalBar() to show frontend values.

CSignalNoiseRatioBar() is intended to show signal noise ratio value.
*/

class CSignalNoiseRatioBar : public CSignalBar
{
	public:
		///refresh current item properties, use this before paintScale().
		void Refresh();

	public:
		CSignalNoiseRatioBar(){};
		///basic component class constructor for signal noise ratio.
		CSignalNoiseRatioBar(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref)
					: CSignalBar(xpos, ypos, w, h, frontend_ref, "SNR"){};
};

/// Class CSignalBox() provides CSignalBar(), CSignalNoiseRatioBar() scales at once.
/*!
Provides basic attributes and member functions for CComponentItems in
additional of CSignalBar()- and CSignalNoiseRatioBar()-objects.


To add a signalbox object to your code add this to a header file:
#include <gui/components/cc_frm_signalbars.h>

class CSampleClass
{
	private:
		//other stuff;
		CSignalBox * signalbox;

	public:
		CSampleClass();
		~CSampleClass();
		void showSNR();

		//other stuff;

};


//add this to your costructor into the code file:
CSampleClass::CSampleClass()
{
	//other stuff;
	signalbox = NULL;
}

CStreamInfo2::~CStreamInfo2 ()
{
	//other stuff to clean;
	delete signalbox;
	//other stuff to clean;
}

void CSampleClass::showSNR()
{
	if (signalbox == NULL){
		signalbox = new CSignalBox(10, 100, 500, 38, frontend);
//		signalbox->setCornerRadius(0); //optional
// 		signalbox->setColorBody(COL_BLACK); //optional
		signalbox->setColorBody(COL_MENUHEAD_PLUS_0);q
		signalbox->doPaintBg(false);
//if you want to add the object to a CC-Container (e.g. CComponentsWindow()), remove this line:
		signalbox->paint(false);
//and add this lines:
//		if (!ignalbox->isAdded())
//			addCCItem(signalbox);
//Note: signal box objects deallocate together with the CC-Container!
 	}
	else{
		signalbox->paintScale();
 	}
 }

void CSampleClass::hide ()
{
	//other code;

//Note: not required if signalbox is added to a CC-Container!
	signalbox->hide(true);
	delete signalbox;
	signalbox = NULL;

	//other code;
}

*/

class CSignalBox : public CComponentsForm
{
	private:
		///object: current frontend
		CFrontend 		*sbx_frontend;
		///object: current signalbar
		CSignalBar 		*sbar;
		///object: current signal noise ratio bar
		CSignalNoiseRatioBar 	*snrbar;

		///property: height of signalbars
		int 		sbx_bar_height;
		///property: width of signalbars
		int 		sbx_bar_width;
		///property: x position of signalbars
		int 		sbx_bar_x;
		///property: text color, see also setTextColor()
		fb_pixel_t 	sbx_caption_color;

		// true if vertical arrangement, false if horizontal
		bool vertical;

		///initialize all needed basic attributes and objects
		void initVarSigBox();
		///initialize general properties of signal items
		void initSignalItems();

		///paint items with new values, required after Refresh()
		void paintScale();

	public:
		///class constructor for signal noise ratio.
		CSignalBox(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref = NULL, const bool vertical = true);

		///returns the signal object, type = CSignalBar*
		CSignalBar* getScaleObject(){return sbar;};
		///returns the signal noise ratio object, type = CSignalNoiseRatioBar*
		CSignalNoiseRatioBar* getLabelObject(){return snrbar;};

		///sets the caption color of signalbars, see also property 'sbx_caption_color'
		void setTextColor(const fb_pixel_t& caption_color){ sbx_caption_color = caption_color;};
		///get caption color of signalbars, see also property 'sbx_caption_color'
		fb_pixel_t getTextColor(){return sbx_caption_color;};
		
		///paint items
		void paint(bool do_save_bg);

		///return current signal value
		uint16_t getSignalValue(void) { return sbar->getValue();}

		///return current snr value
		uint16_t getSNRValue(void) { return snrbar->getValue();}
		
		///return current snr value
};

#endif
