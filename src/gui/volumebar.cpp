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
#include "gui/movieplayer.h"

#include <neutrino.h>
#include <gui/infoclock.h>
#include <gui/timeosd.h>
#include <driver/neutrinofonts.h>
#include <driver/fontrenderer.h>
#include <system/debug.h>

extern CTimeOSD *FileTimeOSD;

using namespace std;


CVolumeBar::CVolumeBar()
{
	initVarVolumeBar();
}

void CVolumeBar::initVarVolumeBar()
{
	col_body 	= COL_MENUCONTENT_PLUS_0;
	corner_rad 	= CORNER_RADIUS_MID;
	vb_item_offset 	= OFFSET_INNER_SMALL;
	height          = CFrameBuffer::getInstance()->scale2Res(g_settings.volume_size);

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

	initVolumeBarSize();
	initVolumeBarPosition();
	initVolumeBarItems();
}

//calculates size referred for possible activated clock or/and mute icon
void CVolumeBar::initVolumeBarSize()
{
	CVolumeHelper *cvh = CVolumeHelper::getInstance();
	cvh->refresh();
	cvh->getSpacer(&h_spacer, &v_spacer);
	cvh->getDimensions(&x, &y, &sw, &sh, &vb_icon_w, &vb_digit_w);
	cvh->getVolBarDimensions(&y, &height);

	//vb_digit_w += corner_rad/2;

	//scale
	vb_pbw 		= CFrameBuffer::getInstance()->scale2Res(200);
	vb_pbh 		= height-2*vb_item_offset;

	//result for width
	width = (vb_icon_w + vb_pbw + vb_digit_w) + 4*vb_item_offset + corner_rad/2;

	//adapt x-pos
	vb_pbx 		= vb_item_offset + vb_icon_w + vb_item_offset;
	vb_icon_x 	= vb_pbx/2 - vb_icon_w/2 + vb_item_offset;
	vb_digit_x	= vb_pbx + vb_pbw + vb_item_offset;

	// mute icon
	cvh->getMuteIconDimensions(&mute_ax, &mute_ay, &mute_dx, &mute_dy);
	// info clock
	int dummy;
	cvh->getInfoClockDimensions(&dummy, &clock_y, &clock_width, &clock_height);
	int mute_corrY = 0;
	if (mute_dy < height)
		mute_corrY = (height - mute_dy) / 2;
	cvh->setMuteIconCorrY(mute_corrY);

	vb_pby 		= height/2-vb_pbh/2;
}

//init current position of form
void CVolumeBar::initVolumeBarPosition()
{
	CNeutrinoApp* neutrino = CNeutrinoApp::getInstance();

	switch (g_settings.volume_pos)
	{
		case VOLUMEBAR_POS_TOP_RIGHT:{
			int x_corr 	= 0;
			if (( neutrino->getMode() != NeutrinoModes::mode_scart ) && ( neutrino->getMode() != NeutrinoModes::mode_audio) && ( neutrino->getMode() != NeutrinoModes::mode_pic)) {
				if ((neutrino->isMuted()) && (!g_settings.mode_clock))
					x_corr = mute_dx + h_spacer;
				if (CNeutrinoApp::getInstance()->getChannellistIsVisible() == true)
					y += std::max(39, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + v_spacer;
				else if (g_settings.mode_clock)
					y = clock_y + clock_height + v_spacer + OFFSET_SHADOW;
			}
			x = sw - width - x_corr;
			break;
		}
		case VOLUMEBAR_POS_TOP_LEFT:
			if (FileTimeOSD->IsVisible())
				y = clock_y + clock_height + v_spacer + OFFSET_SHADOW;
			break;
		case VOLUMEBAR_POS_BOTTOM_LEFT:
			y = (sh + frameBuffer->getScreenY()) - height - v_spacer;
			break;
		case VOLUMEBAR_POS_BOTTOM_RIGHT:
			x = sw - width;
			y = (sh + frameBuffer->getScreenY()) - height - v_spacer;
			break;
		case VOLUMEBAR_POS_TOP_CENTER:
			x = ((sw - width) / 2) + x - h_spacer/2;
			break;
		case VOLUMEBAR_POS_BOTTOM_CENTER:
			x = ((sw - width) / 2) + x - h_spacer/2;
			y = (sh + frameBuffer->getScreenY()) - height - v_spacer;
			break;
		case VOLUMEBAR_POS_HIGHER_CENTER:
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
	if (!vb_icon){
		vb_icon = new CComponentsPicture(vb_icon_x, CC_CENTERED, vb_icon_w, height, NEUTRINO_ICON_VOLUME);
		//add icon to container
		addCCItem(vb_icon);
	}

	vb_icon->setDimensionsAll(vb_icon_x, CC_CENTERED, vb_icon_w, height);
	vb_icon->setColorBody(col_body);
	vb_icon->setCorner(corner_rad, CORNER_LEFT);
}

//create new scale
void CVolumeBar::initVolumeBarScale()
{
	if (!vb_pb){
		vb_pb = new CProgressBar();
		//add progressbar to container
		addCCItem(vb_pb);
	}

	vb_pb->setType(CProgressBar::PB_REDRIGHT);
	vb_pb->setRgb(85, 75, 100);
	vb_pb->setFrameThickness(FRAME_WIDTH_NONE);
	vb_pb->setProgress(vb_pbx, vb_pby, vb_pbw, vb_pbh, *vb_vol, 100);
}

//set digit text with current volume value
void CVolumeBar::initVolumeBarDigitValue()
{
	vb_digit->kill(col_body);
	vb_digit->setText(*vb_vol ,vb_digit_mode, *(CVolumeHelper::getInstance()->vb_font));
}

//create digit
void CVolumeBar::initVolumeBarDigit()
{
	if (!vb_digit)
		vb_digit = new CComponentsLabel(this);

	vb_digit->setDimensionsAll(vb_digit_x, 0, vb_digit_w, height);
	vb_digit->setTextColor(COL_MENUCONTENT_TEXT);
	vb_digit->setCorner(corner_rad, CORNER_RIGHT);
	initVolumeBarDigitValue();
}

//refresh and paint digit
void CVolumeBar::paintVolumeBarDigit()
{
	// paint digit
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

void CVolumeBar::paint(bool do_save_bg)
{
	//prepare items
	initVolumeBarItems();

	//paint form contents
	if (!is_painted)
		CComponentsForm::paint(do_save_bg);
	else
		repaintVolScale();
}



// CVolumeHelper ####################################################################################################

CVolumeHelper::CVolumeHelper()
{
	h_spacer	= OFFSET_INNER_MID;
	v_spacer	= OFFSET_INNER_SMALL;
	vb_font		= NULL;
	clock_font	= NULL;

	frameBuffer = CFrameBuffer::getInstance();

	CNeutrinoApp::getInstance()->OnAfterSetupFonts.connect(sigc::mem_fun(this, &CVolumeHelper::resetFont));

	Init();
}

void CVolumeHelper::resetFont()
{
	if (vb_font){
		vb_font		= NULL;
		dprintf(DEBUG_INFO, "\033[33m[CVolumeHelper][%s - %d] reset vb font \033[0m\n", __func__, __LINE__);
	}
	if (clock_font){
		clock_font	= NULL;
		dprintf(DEBUG_INFO, "\033[33m[CVolumeHelper][%s - %d] reset clock font \033[0m\n", __func__, __LINE__);
	}
}

void CVolumeHelper::Init(Font* font)
{

	x  = frameBuffer->getScreenX() + h_spacer;
	y  = frameBuffer->getScreenY() + v_spacer;
	sw = g_settings.screen_EndX - h_spacer;
	sh = frameBuffer->getScreenHeight();

	initVolBarSize();
	initMuteIcon();
	initInfoClock(font);
}

void CVolumeHelper::initInfoClock(Font* font)
{
	if (font == NULL) {
		int dx = 0;
		int dy = g_settings.infoClockFontSize;
		clock_font = *CNeutrinoFonts::getInstance()->getDynFont(dx, dy, g_settings.infoClockSeconds ? "%H:%M:%S" : "%H:%M");
	}
	else
		clock_font = font;

	digit_offset = (clock_font)->getDigitOffset();
	digit_h      = (clock_font)->getDigitHeight();
	int t1       = (clock_font)->getMaxDigitWidth();
	int t2       = (clock_font)->getRenderWidth(":");
	clock_dy     = digit_h + (int)((float)digit_offset * 1.3);
	if (g_settings.infoClockSeconds)
		clock_dx     = t1*7 + t2*2;
	else
		clock_dx     = t1*5 + t2*1;
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

	time_dx = t1*7 + t2*2;
	time_ax = frameBuffer->getScreenX() + h_spacer;
}

void CVolumeHelper::initMuteIcon()
{
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MUTE, &mute_dx, &mute_dy);
	mute_ax 	= sw - mute_dx;
	mute_ay 	= y;
}

void CVolumeHelper::initVolBarSize()
{
	icon_width		= 0;
	icon_height		= 0;
	digit_width		= 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_VOLUME, &icon_width, &icon_height);
	icon_height		= max(icon_height, 16); // if no icon available
	icon_height		+= OFFSET_INNER_MIN;
	icon_width		+= OFFSET_INNER_MID;
	g_settings.volume_size	= max(g_settings.volume_size, icon_height);
	vol_height		= CFrameBuffer::getInstance()->scale2Res(g_settings.volume_size);

	if (g_settings.volume_digits) {
		CNeutrinoFonts *cnf = CNeutrinoFonts::getInstance();
		cnf->setFontUseDigitHeight(true);
		int tmp_h	= vol_height;
		digit_width	= 0;
		vb_font		= cnf->getDynFont(digit_width, tmp_h, "100", CNeutrinoFonts::FONT_STYLE_REGULAR, CNeutrinoFonts::FONT_ID_VOLBAR);
		digit_width	+= OFFSET_INNER_SMALL;
		vol_height	= max(vol_height, tmp_h);
	}
}

int CVolumeHelper::getInfoClockX()
{
	if (CNeutrinoApp::getInstance()->isMuted())
		return clock_ax - mute_dx - h_spacer;
	else
		return clock_ax;
}

void CVolumeHelper::refresh(Font* font)
{
	Init(font);
}

CVolumeHelper* CVolumeHelper::getInstance()
{
	static CVolumeHelper* Helper = NULL;
	if(!Helper)
		Helper = new CVolumeHelper();
	return Helper;
}
