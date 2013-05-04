/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Volumebar class for gui.
	Copyright (C) 2013, Thilo Graf 'dbt'
	Copyright (C) 2013, M. Liebmann (micha-bbg)

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


#include "gui/volumebar.h"

#include <global.h>
#include <neutrino.h>
#include <gui/infoclock.h>

using namespace std;

CVolumeBar::CVolumeBar()
{
	initVarVolumeBar();
}

void CVolumeBar::initVarVolumeBar()
{
	//init inherited variables
	initVarForm();
	col_body 	= COL_MENUCONTENT_PLUS_0;
	height 		= 36; //default height

	//init variables this

	//assume volume value as pointer to global setting
	vb_vol		= &g_settings.current_volume;

	//items
	//icon object
	vb_icon 	= NULL;
	// icon whith / height
	int tmp_h = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_VOLUME, &icon_w, &tmp_h);
	icon_w		+= 12;
	tmp_h		+= 4;
	height 		= max(height, tmp_h);

	//progressbar object
	vb_pb 		= NULL;
	vb_pbx 		= 0;
	vb_pbw 		= 0;
	vb_pbh 		= 0;
	vb_pby 		= 0;
	// progressbar whith
	pb_w 		= 200;

	//digit
	vb_digit 	= NULL;
	vb_digit_mode	= CTextBox::CENTER;
	VolumeFont	= SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO;
	vb_font		= g_Font[VolumeFont];

	initVolumeBarSize();
	initVolumeBarPosition();
	initVolumeBarItems();
}

//calculates size referred for possible activated clock or/and mute icon
void CVolumeBar::initVolumeBarSize()
{
	int tmp_h = height;

	// digit whith / height
	if (g_settings.volume_digits) {
		tmp_h		= g_Font[VolumeFont]->getDigitHeight() + (g_Font[VolumeFont]->getDigitOffset() * 18) / 10;
		height 		= max(height, tmp_h);
		digit_w		= g_Font[VolumeFont]->getRenderWidth("100 ");
	}
	else
		digit_w		= 0;

	//adapt x-pos
	icon_x 		= corner_rad + 2;
	pb_x 		= icon_x + icon_w + 4;
	digit_x		= pb_x + pb_w + 5;

	//width
	if (g_settings.volume_digits)
		width  	= digit_x + digit_w + corner_rad;
	else
		width  	= pb_x + pb_w + corner_rad + 12;

	CVolumeHelper *cvh = CVolumeHelper::getInstance();
	cvh->getSpacer(&h_spacer, &v_spacer);
	cvh->getDimensions(&x, &y, &sw, &sh);

	// mute icon
	cvh->getMuteIconDimensions(&mute_ax, &mute_ay, &mute_dx, &mute_dy);

	clock_height = 0;
	int clock_width  = 0;
	int mute_corrY = 0;
	if ((g_settings.mode_clock) && (g_settings.volume_pos == 0)) {
		// Clock and MuteIcon in a line.
		clock_height = CInfoClock::getInstance(true)->time_height;
		clock_width = CInfoClock::getInstance(true)->time_width;
		mute_corrY = (clock_height - mute_dy) / 2;
	}
	else {
		// Volume level and MuteIcon in a line.
		if (mute_dy > height)
			y += (mute_dy - height) / 2;
		else
			mute_corrY = (height - mute_dy) / 2;
	}
	cvh->setMuteIconCorrY(mute_corrY);

	if ((g_settings.mode_clock) && (!CNeutrinoApp::getInstance()->isMuted()))
		frameBuffer->paintBackgroundBoxRel(sw - clock_width, y, clock_width, clock_height);

	vb_pbh 		= height-8;
	vb_pby 		= height/2-vb_pbh/2;
}

//init current position of form
void CVolumeBar::initVolumeBarPosition()
{
	CNeutrinoApp* neutrino = CNeutrinoApp::getInstance();

	switch (g_settings.volume_pos)
	{
		case 0:{// upper right
			int x_corr 	= 0;
			if (( neutrino->getMode() != CNeutrinoApp::mode_scart ) && ( neutrino->getMode() != CNeutrinoApp::mode_audio) && ( neutrino->getMode() != CNeutrinoApp::mode_pic)) {
				if ((neutrino->isMuted()) && (!g_settings.mode_clock))
					x_corr = mute_dx + h_spacer;
				if (g_settings.mode_clock)
					y += clock_height + v_spacer / 2;
			}
			x = sw - width - x_corr;
			break;
		}
		case 1:// upper left
			break;
		case 2:// bottom left
			y = (sh + frameBuffer->getScreenY()) - height - v_spacer;
			break;
		case 3:// bottom right
			x = sw - width;
			y = (sh + frameBuffer->getScreenY()) - height - v_spacer;
			break;
		case 4:// upper center
			x = ((sw - width) / 2) + x;
			break;
		case 5:// bottom center
			x = ((sw - width) / 2) + x;
			y = (sh + frameBuffer->getScreenY()) - height - v_spacer;
			break;
	}
}

void CVolumeBar::initVolumeBarItems()
{
	//icon
	initVolumeBarIcon();

	//scale
	initVolumeBarScale();

	//digits
	if (g_settings.volume_digits)
		initVolumeBarDigit();
}

//init current icon object
void CVolumeBar::initVolumeBarIcon()
{
	vb_icon = new CComponentsPicture(icon_x, 0, icon_w, height, NEUTRINO_ICON_VOLUME);

	vb_icon->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
	vb_icon->setColorBody(col_body);
	vb_icon->setCornerRadius(corner_rad);
	vb_icon->setCornerType(CORNER_LEFT);

	//add icon to container
	addCCItem(vb_icon);
}

//create new scale
void CVolumeBar::initVolumeBarScale()
{
	vb_pb = new CProgressBar();

	vb_pbx = pb_x;
	vb_pbw = pb_w;

	vb_pb->setInvert();
	vb_pb->setBlink();
	vb_pb->setRgb(85, 75, 100);
	vb_pb->setFrameThickness(2);
	vb_pb->setProgress(vb_pbx, vb_pby, vb_pbw, vb_pbh, *vb_vol, 100);

	//add progressbar to container
	addCCItem(vb_pb);
}

//set digit text with current volume value
void CVolumeBar::initVolumeBarDigitValue()
{
	vb_digit->setText(*vb_vol ,vb_digit_mode, vb_font);
}

//create digit
void CVolumeBar::initVolumeBarDigit()
{
	vb_digit = new CComponentsLabel();

	vb_digit->setDimensionsAll(digit_x, 0, digit_w, height);
	vb_digit->setTextColor(COL_MENUCONTENT);
	initVolumeBarDigitValue();

	//add digit label to container
	addCCItem(vb_digit);
}

//refresh and paint digit
void CVolumeBar::paintVolumeBarDigit()
{
	// digits
	CTextBox* ctb = vb_digit->getCTextBoxObject();
	if (ctb)
		ctb->setFontUseDigitHeight();
	int dx = vb_digit->getRealXPos();
	int dy = vb_digit->getRealYPos();
	vb_digit->setDimensionsAll(dx, dy, digit_w, height);
	vb_digit->paint(CC_SAVE_SCREEN_NO);
}


//refresh progressbar and digit
void CVolumeBar::repaintVolScale()
{
	paintVolScale();

	if (g_settings.volume_digits) {
		initVolumeBarDigitValue();
		paintVolumeBarDigit();
	}
}

//set current volume value and paint form
void CVolumeBar::paintVolScale()
{
	vb_pb->setValue(*vb_vol);
	vb_pb->paint(CC_SAVE_SCREEN_NO);
}


//final paint
void CVolumeBar::paint(bool do_save_bg)
{
	//paint form
	paintForm(do_save_bg);
}


// CVolumeHelper ####################################################################################################

CVolumeHelper::CVolumeHelper()
{
	h_spacer	= 11;
	v_spacer	= 6;

	CFrameBuffer *frameBuffer = CFrameBuffer::getInstance();
	x		= frameBuffer->getScreenX() + h_spacer;
	y		= frameBuffer->getScreenY() + v_spacer;
	sw		= g_settings.screen_EndX - h_spacer;
	sh		= frameBuffer->getScreenHeight();

	int mute_icon_dx = 0;
	int mute_icon_dy = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MUTE, &mute_icon_dx, &mute_icon_dy);
	mute_dx 	= mute_icon_dx;
	mute_dy 	= mute_icon_dy;
	mute_dx 	+= mute_icon_dx / 4;
	mute_dy 	+= mute_icon_dx / 4;
	mute_ax 	= sw - mute_dx;
	mute_ay 	= y;
	mute_corrY	= 0;
}

CVolumeHelper* CVolumeHelper::getInstance()
{
	static CVolumeHelper* Helper = NULL;
	if(!Helper)
		Helper = new CVolumeHelper();
	return Helper;
}
