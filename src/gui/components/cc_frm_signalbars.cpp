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
#include <zapit/femanager.h>
#include "cc_frm_signalbars.h"
#include <sstream>

#define SB_MIN_HEIGHT 12

using namespace std;

CSignalBar::CSignalBar()
{
	initVarSigBar();
	sb_name 	= "SIG";

	initDimensions();
	initSBItems();
}

CSignalBar::CSignalBar(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref, const string& sbname)
{
	initVarSigBar();
	sb_frontend 	= frontend_ref;
	x 		= xpos;
	y 		= ypos;
	width 		= w;
	height 		= h;
	sb_name		= sbname;

	initDimensions();
	initSBItems();
}

void CSignalBar::initDimensions()
{
	//set current required dimensions and font size
	sb_item_height 	= max(height, SB_MIN_HEIGHT) - 2*fr_thickness - append_h_offset;
	sb_item_top 	= height/2 - sb_item_height/2;
	if (sb_scale_height == -1)
		sb_scale_height = sb_item_height;

	int dx 		= 0;
	int dy          = min(sb_item_height, 100);
	sb_font 	= *dy_font->getDynFont(dx, dy, "100% "+sb_name);
	dx		+= dx/10;
	sb_scale_width	= width - dx;

	sb_vlbl_width = sb_font->getRenderWidth("100% ") + dx/20;
	sb_lbl_width  = dx - sb_vlbl_width;
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

	//init items scale, value and name
	initSBarScale();
	initSBarValue();
	initSBarName();
}

void CSignalBar::initVarSigBar()
{
	corner_rad 	= 0;
	corner_type 	= 0;
	append_h_offset = 2;
	append_v_offset = 2;
	height		= SB_MIN_HEIGHT;

	sb_scale_height = -1;
	dy_font 	= CNeutrinoFonts::getInstance();

	sb_caption_color= COL_INFOBAR_TEXT;
	sb_val_mode 	= CTextBox::NO_AUTO_LINEBREAK | CTextBox::RIGHT;

	sb_lastsig 	= 0;
	sb_signal 	= 0;

	sb_frontend 	= NULL;
	sb_scale 	= NULL;
	sb_vlbl		= NULL;
	sb_lbl		= NULL;
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
	sb_scale->setDimensionsAll(fr_thickness, scale_y, sb_scale_width, sb_scale_height);
	sb_scale->setColorBody(col_body);

	//add scale object to container
	if(!sb_scale->isAdded())
		addCCItem(sb_scale);

}

void CSignalBar::initSBarValue()
{
	//create value label object with basic properties
	if (sb_vlbl == NULL){
		sb_vlbl = new CComponentsLabel();
		sb_vlbl->doPaintBg(false);
		sb_vlbl->setText("  0%", sb_val_mode, sb_font);
	}

	//move and set dimensions
	int vlbl_x = sb_scale->getXPos() + sb_scale_width + append_v_offset;
	int vlbl_h = sb_scale->getHeight();
	int vlbl_y = sb_item_height/2 + sb_item_top - vlbl_h/2 - append_h_offset;
	sb_vlbl->setDimensionsAll(vlbl_x, vlbl_y, sb_vlbl_width, vlbl_h);

	//set current text and body color color
	sb_vlbl->setTextColor(sb_caption_color);
	sb_vlbl->setColorBody(col_body);

	//add value label object to container
	if (!sb_vlbl->isAdded())
		addCCItem(sb_vlbl);
}

void CSignalBar::initSBarName()
{
	//create name label object with basic properties
	if (sb_lbl == NULL){
		sb_lbl = new CComponentsLabel();
		sb_lbl->doPaintBg(false);
		sb_lbl->setText(sb_name, CTextBox::NO_AUTO_LINEBREAK | CTextBox::RIGHT, sb_font);
		sb_lbl->forceTextPaint();
		sb_lbl->doPaintTextBoxBg(true);
	}

	//move and set dimensions
	int lbl_x = sb_vlbl->getXPos()+ sb_vlbl->getWidth();
	int lbl_h = sb_vlbl->getHeight();
	int lbl_y = sb_item_height/2 + sb_item_top - lbl_h/2 - append_h_offset;
	sb_lbl->setDimensionsAll(lbl_x, lbl_y, sb_lbl_width, lbl_h);

	//set current text and body color
	sb_lbl->setTextColor(sb_caption_color);
	sb_lbl->setColorBody(col_body);

	//add name label object to container
	if (!sb_lbl->isAdded())
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
		sb_vlbl->setText(percent, sb_val_mode, sb_font);

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
void CSignalNoiseRatioBar::Refresh()
{
	//get current value from frontend
	sb_signal = sb_frontend->getSignalNoiseRatio();

	//reinit items with current values
	initSBItems();
}


//**********************************************************************************************************************
CSignalBox::CSignalBox(const int& xpos, const int& ypos, const int& w, const int& h, CFrontend *frontend_ref, const bool vert)
{
	initVarSigBox();
	vertical = vert;

	sbx_frontend 	= (frontend_ref == NULL) ? CFEManager::getInstance()->getLiveFE() : frontend_ref;
	x 		= xpos;
	y 		= ypos;
	width 		= w;
	height 		= h;

	if (vertical) {
		sbx_bar_height	= height/2;
		sbx_bar_width 	= width-2*corner_rad;
	} else {
		sbx_bar_height	= height;
		sbx_bar_width	= width/2-2*corner_rad;
	}

	sbar = new CSignalBar(sbx_bar_x, 0, sbx_bar_width, sbx_bar_height, sbx_frontend);
	sbar->doPaintBg(false);
	addCCItem(sbar);

	snrbar = new CSignalNoiseRatioBar(vertical ? sbx_bar_x : CC_APPEND, vertical ? CC_APPEND : 0, sbx_bar_width, sbx_bar_height, sbx_frontend);
	snrbar->doPaintBg(false);
	addCCItem(snrbar);

	initSignalItems();
}

void CSignalBox::initVarSigBox()
{
	corner_rad	= 0;

	sbx_frontend 	= NULL;
	sbar		= NULL;
	snrbar		= NULL;
	height 		= 3* SB_MIN_HEIGHT;
	sbx_bar_height	= height/2;
	sbx_bar_x	= corner_rad;
	sbx_caption_color = COL_INFOBAR_TEXT;
	vertical = true;
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

	int sbar_sw = sbar->getScaleWidth();
	int snrbar_sw = snrbar->getScaleWidth();
	if (sbar_sw < snrbar_sw)
		snrbar->setScaleWidth(sbar_sw);
	else if (snrbar_sw < sbar_sw)
		sbar->setScaleWidth(snrbar_sw);

	sbar->setDimensionsAll(sbar_x, fr_thickness, sbar_w, sbar_h);
	sbar->setFrontEnd(sbx_frontend);
	sbar->setCorner(0);
	sbar->setScaleHeight(scale_h);

	snrbar->setDimensionsAll(vertical ? sbar_x : CC_APPEND, vertical ? CC_APPEND : fr_thickness, sbar_w, sbar_h);
	snrbar->setFrontEnd(sbx_frontend);
	snrbar->setCorner(0);
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
