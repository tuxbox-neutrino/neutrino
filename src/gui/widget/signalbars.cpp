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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "signalbars.h"
#include <sstream>

#define SB_MIN_HEIGHT 12

using namespace std;

CSignalBar::CSignalBar()
{
	initVarSigBar();
	initSBItems();
}

CSignalBar::CSignalBar(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref)
{
	initVarSigBar();
	sb_frontend 	= frontend_ref;
	x 		= xpos;
	y 		= ypos;
	width 		= w;
	height 		= h;

	initSBItems();
}

void CSignalBar::initDimensions()
{
	//set current required dimensions and font size
	sb_item_height 	= max(height, SB_MIN_HEIGHT) - 2*fr_thickness;
	sb_item_top 	= height/2 - sb_item_height/2;
	if (sb_scale_height == -1)
		sb_scale_height = sb_item_height;

	int dx 		= 0;
	int dy 		= sb_item_height;
	sb_font 	= *dy_font->getDynFont(dx, dy);

	sb_vlbl_width 	= sb_font->getRenderWidth ("00%", true);
	sb_lbl_width 	= sb_font->getRenderWidth ("XXXX"/*sb_name*/, true);
	sb_scale_width	= width - sb_vlbl_width - sb_lbl_width - 2*fr_thickness;
}

void CSignalBar::initSBItems()
{
	if (cc_parent){
		//use backround color of parent form if signalbar is embedded
		col_body = cc_parent->getColorBody();

		//and set required color for text to name label
		CSignalBox *sbx = static_cast<CSignalBox*>(cc_parent);
		sb_caption_color = sbx->getTextColor();
	}

	//reinit dimensions
	initDimensions();

	//init items scale, value and name
	initSBarScale();
	initSBarValue();
	initSBarName();
}

void CSignalBar::initVarSigBar()
{
	initVarForm();
	corner_rad 	= 0;
	corner_type 	= 0;
	append_h_offset = 2;
	append_v_offset = 0;
	height		= SB_MIN_HEIGHT;

	sb_scale_height = -1;

	dy_font 	= CNeutrinoFonts::getInstance();

	sb_caption_color= COL_INFOBAR_TEXT;

	sb_lastsig 	= 0;
	sb_signal 	= 0;

	sb_frontend 	= NULL;
	sb_scale 	= NULL;
	sb_vlbl		= NULL;
	sb_lbl		= NULL;
	sb_name		= "SIG";
}

void CSignalBar::initSBarScale()
{
	//create scale object if required
	if (sb_scale == NULL){
		sb_scale = new CProgressBar();
		//we want colored scale!
		sb_scale->setBlink();
	}

	//move and set dimensions
	int scale_y = (sb_item_height/2 - sb_scale_height/2);
	sb_scale->setDimensionsAll(fr_thickness, scale_y, sb_scale_width, sb_scale_height/*sb_item_height*/);
	sb_scale->setColorBody(col_body);

	//add scale object to container
	if(!isAdded(sb_scale))
		addCCItem(sb_scale);

}

void CSignalBar::initSBarValue()
{
	//create value label object with basic properties
	if (sb_vlbl == NULL){
		sb_vlbl = new CComponentsLabel();
		sb_vlbl->doPaintBg(false);
		sb_vlbl->setText("0%", CTextBox::NO_AUTO_LINEBREAK, sb_font);
	}

	//move and set dimensions
	int vlbl_x = sb_scale->getXPos() + sb_scale_width + append_h_offset;
	sb_vlbl->setDimensionsAll(vlbl_x/*CC_APPEND*/, sb_item_top, sb_vlbl_width, sb_item_height);

	//set current text and body color color
	sb_vlbl->setTextColor(sb_caption_color);
	sb_vlbl->setColorBody(col_body);

	//add value label object to container
	if (!isAdded(sb_vlbl))
		addCCItem(sb_vlbl);
}

void CSignalBar::initSBarName()
{
	//create name label object with basic properties
	if (sb_lbl == NULL){
		sb_lbl = new CComponentsLabel();
		//paint no backround
		sb_lbl->doPaintBg(true);
		sb_lbl->forceTextPaint();
	}

	//move and set dimensions
	int lbl_x = width - sb_lbl_width - fr_thickness;
	sb_lbl->setDimensionsAll(lbl_x, sb_item_top, sb_lbl_width, sb_item_height);
	sb_lbl->setText(sb_name, CTextBox::NO_AUTO_LINEBREAK | CTextBox::RIGHT, sb_font);

	//set current text and body color
	sb_lbl->setTextColor(sb_caption_color);
	sb_lbl->setColorBody(col_body);

	//add name label object to container
	if (!isAdded(sb_lbl))
		addCCItem(sb_lbl);
}


void CSignalBar::Refresh()
{
	//get current value from frontend
	sb_signal = sb_frontend->getSignalStrength();

	//reinit items with current values
	initSBItems();
}

void CSignalBar::paintScale()
{
	//format signal values
	int sig;
	sig = (sb_signal & 0xFFFF) * 100 / 65535;

	//exit if value too small
	if(sig < 5)
		return;

	//assign current value to scale object and paint new value
	if (sb_lastsig != sig) {
		sb_lastsig = sig;
		sb_scale->setValues(sig, 100);
		//string is required
		ostringstream i_str;
		i_str << sig;
		string percent(i_str.str());
		percent += "%";
		sb_vlbl->setText(percent, CTextBox::NO_AUTO_LINEBREAK | CTextBox::CENTER, sb_font);

		//we must force paint backround, because of changing values
		sb_vlbl->doPaintBg(true);
		sb_vlbl->forceTextPaint();
		sb_vlbl->doPaintTextBoxBg(true);
		sb_vlbl->setColorBody(col_body);

		//repaint labels
		for(size_t i=0; i<this->v_cc_items.size(); i++)
			v_cc_items[i]->paint(false);
	}
}

void CSignalBar::paint(bool do_save_bg)
{
	//initialize before and paint frame and body
	if (!is_painted){
		initSBItems();
		paintForm(do_save_bg);
	}

	//paint current sig value
	paintScale();
}


//*******************************************************************************************************************************
CSignalNoiseRatioBar::CSignalNoiseRatioBar()
{
	initVarSnrBar();
	initSBItems();
}

CSignalNoiseRatioBar::CSignalNoiseRatioBar(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref)
{
	initVarSnrBar();
	sb_frontend 	= frontend_ref;
	x 		= xpos;
	y 		= ypos;
	width 		= w;
	height 		= h;

	initSBItems();
}

void CSignalNoiseRatioBar::initVarSnrBar()
{
	initVarSigBar();
	sb_name	= "SNR";
}

void CSignalNoiseRatioBar::Refresh()
{
	//get current value from frontend
	sb_signal = sb_frontend->getSignalNoiseRatio();

	//reinit items with current values
	initSBItems();
}


//**********************************************************************************************************************
CSignalBox::CSignalBox(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref)
{
	initVarSigBox();

	sbx_frontend 	= frontend_ref;
	x 		= xpos;
	y 		= ypos;
	width 		= w;
	height 		= h;

	sbx_bar_height	= height/2;
	sbx_bar_width 	= width-2*corner_rad;

	sbar = new CSignalBar(sbx_bar_x, 0, sbx_bar_width, sbx_bar_height, sbx_frontend);
	sbar->doPaintBg(false);
	addCCItem(sbar);

	snrbar = new CSignalNoiseRatioBar(sbx_bar_x, CC_APPEND, sbx_bar_width, sbx_bar_height, sbx_frontend);
	snrbar->doPaintBg(false);
	addCCItem(snrbar);

	initSignalItems();
}

void CSignalBox::initVarSigBox()
{
	initVarForm();
	corner_rad	= 0;

	sbx_frontend 	= NULL;
	sbar		= NULL;
	snrbar		= NULL;
	height 		= 3* SB_MIN_HEIGHT;
	sbx_bar_height	= height/2;
	sbx_bar_x	= corner_rad;
	sbx_caption_color = COL_INFOBAR_TEXT;
}

void CSignalBox::initSignalItems()
{
	//set current properties for items
// 	int cor_rad = corner_rad/2-fr_thickness;

// 	int corr_y = sbx_bar_height%2;
// 	int sb_h = sbx_bar_height - corr_y;

	int sbar_h = sbx_bar_height - fr_thickness - append_v_offset/2;
	int sbar_w = sbx_bar_width - 2*fr_thickness;
	int sbar_x = sbx_bar_x + fr_thickness;
	int scale_h = sbar_h * 76 / 100;

	sbar->setDimensionsAll(sbar_x, fr_thickness, sbar_w, sbar_h);
	sbar->setFrontEnd(sbx_frontend);
	sbar->setCornerRadius(0);
	sbar->setScaleHeight(scale_h);

	snrbar->setDimensionsAll(sbar_x, CC_APPEND, sbar_w, sbar_h);
	snrbar->setFrontEnd(sbx_frontend);
	snrbar->setCornerRadius(0);
	snrbar->setScaleHeight(scale_h);
}

void CSignalBox::paintScale()
{
	initSignalItems();

	//repaint items
	sbar->Refresh();
	sbar->paint/*Scale*/(false);

	snrbar->Refresh();
	snrbar->paint/*Scale*/(false);
}

void CSignalBox::paint(bool do_save_bg)
{
	//paint frame and body
	if (!is_painted){
		initSignalItems();
		paintForm(do_save_bg);
	}

	//paint current signal value
	paintScale();
}
