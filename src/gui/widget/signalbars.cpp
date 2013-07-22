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

#define SB_MIN_HEIGHT 10

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
	height 		= max(h, height);

	initSBItems();
}

void CSignalBar::initVarSigBar()
{
	initVarForm();
	corner_rad 	= 0;
	corner_type 	= 0;
	append_h_offset = 4;
	append_v_offset = 0;

	sb_font 	= g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];
	sb_item_height 	= sb_font->getHeight();
	height		= sb_item_height;
	sb_scale_height = SB_MIN_HEIGHT;
	sb_caption_color= COL_INFOBAR_TEXT;

	initDimensions();

	sb_lastsig 	= 0;
	sb_signal 	= 0;

	sb_frontend 	= NULL;
	sb_scale 	= NULL;
	sb_vlbl		= NULL;
	sb_lbl		= NULL;
	sb_name		= "SIG";
}

void CSignalBar::initDimensions()
{
	//set current required dimensions
	sb_vlbl_width 	= sb_font->getRenderWidth ("100%", true);
	sb_lbl_width 	= sb_font->getRenderWidth ("XXXXX", true);
	sb_scale_width	= width-sb_vlbl_width-sb_lbl_width-corner_rad;
	sb_item_height 	= max(sb_scale_height, sb_font->getHeight());
	height 		= max(height, sb_item_height);
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

void CSignalBar::Refresh()
{
	//get current value from frontend
	sb_signal = sb_frontend->getSignalStrength();

	//reinit items with current values
	initSBItems();
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
	int sb_y = height/2 - sb_scale_height/2 ;
	sb_scale->setDimensionsAll(append_v_offset, sb_y, sb_scale_width, sb_scale_height);

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
	int sb_y = height/2 - sb_item_height/2 ;
	sb_vlbl->setDimensionsAll(CC_APPEND, sb_y, sb_vlbl_width, sb_item_height);

	//set current text color
	sb_vlbl->setTextColor(sb_caption_color);

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
		sb_lbl->doPaintBg(false);
	}

	//move and set dimensions
	int sb_y = sb_vlbl->getYPos() ;
	sb_lbl->setDimensionsAll(CC_APPEND, sb_y, sb_lbl_width, sb_item_height);
	sb_lbl->setText(sb_name, CTextBox::NO_AUTO_LINEBREAK, sb_font);

	//set current text color
	sb_lbl->setTextColor(sb_caption_color);

	//add name label object to container
	if (!isAdded(sb_lbl))
		addCCItem(sb_lbl);
}


void CSignalBar::Repaint()
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
	height 		= max(h, height);

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
	height 		= max(h, height);

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
	height 		= 5* SB_MIN_HEIGHT;
	sbx_bar_height	= height/2;
	sbx_bar_x	= corner_rad;
	sbx_caption_color = COL_INFOBAR_TEXT;
}

void CSignalBox::initSignalItems()
{
	//set current properties for items
	int cor_rad = corner_rad/2-fr_thickness;

	sbar->setDimensionsAll(sbx_bar_x, 0, sbx_bar_width, sbx_bar_height);
	sbar->setCornerRadius(cor_rad);
	sbar->setFrontEnd(sbx_frontend);

	snrbar->setDimensionsAll(sbx_bar_x, CC_APPEND, sbx_bar_width, sbx_bar_height);
	snrbar->setCornerRadius(cor_rad);
	snrbar->setFrontEnd(sbx_frontend);
}

void CSignalBox::Refresh()
{
	//reinit properties of items
	initSignalItems();

	//refresh properties of items
	sbar->Refresh();
	snrbar->Refresh();
}

void CSignalBox::Repaint()
{
	//repaint items
	sbar->Repaint();
	snrbar->Repaint();
}
