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

	vb_item_offset 	= 4;
	height 		= 4*vb_item_offset; //default height

	//assume volume value as pointer to global setting
	vb_vol		= &g_settings.current_volume;

	//items
	//icon object
	vb_icon 	= NULL;

	//progressbar object
	vb_pb 		= NULL;
	vb_pbx 		= 0;
	vb_pbw 		= 0;
	vb_pbh 		= 0;
	vb_pby 		= 0;

	//digit
	vb_digit 	= NULL;
	vb_digit_mode	= CTextBox::CENTER ;
	VolumeFont	= SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO;
	vb_font		= g_Font[VolumeFont];

	initVolumeBarSize();
	initVolumeBarPosition();
	initVolumeBarItems();
}

//calculates size referred for possible activated clock or/and mute icon
void CVolumeBar::initVolumeBarSize()
{
	CVolumeHelper *cvh = CVolumeHelper::getInstance();
	cvh->getSpacer(&h_spacer, &v_spacer);
	cvh->getDimensions(&x, &y, &sw, &sh, &vb_icon_w, &vb_digit_w);
	cvh->getVolBarDimensions(&y, &height);

	vb_digit_w += cornerRad()/2;

	//scale
	vb_pbw 		= 200;
	vb_pbh 		= height-4*vb_item_offset;

	//adapt x-pos
	vb_icon_x 	= vb_item_offset;
	vb_pbx 		= vb_icon_x + vb_icon_w + vb_item_offset;
	vb_digit_x	= vb_pbx + vb_pbw + vb_item_offset;

	//result for width
	width = (vb_icon_w + vb_pbw + vb_digit_w) + 4*vb_item_offset;

	// mute icon
	cvh->getMuteIconDimensions(&mute_ax, &mute_ay, &mute_dx, &mute_dy);
	// info clock
	int dummy;
	cvh->getInfoClockDimensions(&dummy, &clock_y, &clock_width, &clock_height, &dummy, &dummy);
	int mute_corrY = 0;
	if (mute_dy < height)
		mute_corrY = (height - mute_dy) / 2;
	cvh->setMuteIconCorrY(mute_corrY);

	if ((g_settings.mode_clock) && (!CNeutrinoApp::getInstance()->isMuted()))
		CInfoClock::getInstance()->ClearDisplay();

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
					y += max(clock_y + clock_height, mute_ay + mute_dy);
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
			x = ((sw - width) / 2) + x - h_spacer/2;
			break;
		case 5:// bottom center
			x = ((sw - width) / 2) + x - h_spacer/2;
			y = (sh + frameBuffer->getScreenY()) - height - v_spacer;
			break;
		case 6:// higher center
			x = ((sw - width) / 2) + x - h_spacer/2;
			y = (sh + frameBuffer->getScreenY()) - sh/10;
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
	vb_icon = new CComponentsPicture(vb_icon_x, 0, vb_icon_w, height, NEUTRINO_ICON_VOLUME);

	vb_icon->setPictureAlign(CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER);
	vb_icon->setColorBody(col_body);
	vb_icon->setCornerRadius(cornerRad());
	vb_icon->setCornerType(CORNER_LEFT);

	//add icon to container
	addCCItem(vb_icon);
}

//create new scale
void CVolumeBar::initVolumeBarScale()
{
	vb_pb = new CProgressBar();

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

	vb_digit->setDimensionsAll(vb_digit_x, 0, vb_digit_w, height);
	vb_digit->setTextColor(COL_MENUCONTENT);
	vb_digit->setCornerRadius(cornerRad());
	vb_digit->setCornerType(CORNER_RIGHT);
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

	// backup original x&y pos
	int _dx = vb_digit->getXPos();
	int _dy = vb_digit->getYPos();

	// get real x&y pos
	int dx = vb_digit->getRealXPos();
	int dy = vb_digit->getRealYPos();

	// set real x&y pos
	vb_digit->setDimensionsAll(dx, dy, vb_digit_w, height);

	// paint digit
	vb_digit->paint(CC_SAVE_SCREEN_NO);

	// restore original x&y pos
	vb_digit->setDimensionsAll(_dx, _dy, vb_digit_w, height);
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

	frameBuffer = CFrameBuffer::getInstance();

	Init();
}

void CVolumeHelper::Init()
{

	x  = frameBuffer->getScreenX() + h_spacer;
	y  = frameBuffer->getScreenY() + v_spacer;
	sw = g_settings.screen_EndX - h_spacer;
	sh = frameBuffer->getScreenHeight();

	initVolBarSize();
	initMuteIcon();
	initInfoClock();
}

void CVolumeHelper::initInfoClock()
{
	digit_offset = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitOffset();
	digit_h      = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getDigitHeight();
	int t1       = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(widest_number);
	int t2       = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(":");
	clock_dy     = digit_h + (int)((float)digit_offset * 1.5);
	clock_dx     = t1*6 + t2*2;
	clock_ax     = sw - clock_dx;
	clock_ay     = y;
	vol_ay       = y;
	mute_corrY   = 0;

	if (g_settings.mode_clock) {
		if (mute_dy > clock_dy)
			clock_ay += (mute_dy - clock_dy) / 2;
		else
			mute_corrY = (clock_dy - mute_dy) / 2;
	}
	else {
		if (mute_dy > vol_height)
			vol_ay += (mute_dy - vol_height) / 2;
		else
			mute_corrY = (vol_height - mute_dy) / 2;
	}
}

void CVolumeHelper::initMuteIcon()
{
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MUTE, &mute_dx, &mute_dy);
	mute_ax 	= sw - mute_dx;
	mute_ay 	= y;
}

void CVolumeHelper::initVolBarSize()
{
	vol_height	= 18;
	icon_width	= 0;
	digit_width	= 0;
	int tmp_h	= 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_VOLUME, &icon_width, &tmp_h);
	tmp_h		+= 2;
	icon_width	+= 8;
	vol_height 	= max(vol_height, tmp_h);

	if (g_settings.volume_digits) {
		tmp_h = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getDigitHeight() + (g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getDigitOffset() * 18) / 10;
		vol_height = max(vol_height, tmp_h);
		digit_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth("100");
	}
}

void CVolumeHelper::refresh()
{
	Init();
}

CVolumeHelper* CVolumeHelper::getInstance()
{
	static CVolumeHelper* Helper = NULL;
	if(!Helper)
		Helper = new CVolumeHelper();
	return Helper;
}
