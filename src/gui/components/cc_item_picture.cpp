/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

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
#include "cc_item_picture.h"
#include <unistd.h>

extern CPictureViewer * g_PicViewer;

using namespace std;


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsPicture from CComponentsItem
CComponentsPicture::CComponentsPicture(	const int x_pos, const int y_pos, const int w, const int h,
					const std::string& image_name, const int alignment, bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	init(x_pos, y_pos, image_name, alignment, has_shadow, color_frame, color_background, color_shadow);

	width	= w;
	height	= h;
	pic_paint_mode 	= CC_PIC_IMAGE_MODE_AUTO,

	initVarPicture();
}

void CComponentsPicture::init(	int x_pos, int y_pos, const string& image_name, const int alignment, bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsItem
	initVarItem();
	cc_item_type 	= CC_ITEMTYPE_PICTURE;

	//CComponentsPicture
	pic_name 	= image_name;
	pic_align	= alignment;
	pic_offset	= 1;
	pic_paint	= true;
	pic_paintBg	= false;
	pic_painted	= false;
	do_paint	= false;
	pic_max_w	= 0;
	pic_max_h	= 0;
	if (pic_name.empty())
		pic_width = pic_height = 0;

	//CComponents
	x = pic_x	= x_pos;
	y = pic_y	= y_pos;
	height		= 0;
	width 		= 0;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_background;
	col_shadow	= color_shadow;
}

void CComponentsPicture::setPicture(const std::string& picture_name)
{
	pic_name = picture_name;
	initVarPicture();
}


void CComponentsPicture::setPictureAlign(const int alignment)
{
	pic_align	= alignment;
	initVarPicture();
}


void CComponentsPicture::initVarPicture()
{
	pic_width = pic_height = 0;
	pic_painted = false;
	do_paint = false;

	if (pic_name == "")
		return;

	if (pic_max_w == 0)
		pic_max_w = width-2*fr_thickness;

	if (pic_max_h == 0)
		pic_max_h = height-2*fr_thickness;

	//set the image mode depends of name syntax, icon names contains no path,
	//so we can detect the required image mode
	if (pic_paint_mode == CC_PIC_IMAGE_MODE_AUTO){
		if (!access(pic_name.c_str(), F_OK ))
			pic_paint_mode = CC_PIC_IMAGE_MODE_ON;
		else
			pic_paint_mode = CC_PIC_IMAGE_MODE_OFF;
	}

	if (pic_paint_mode == CC_PIC_IMAGE_MODE_OFF){
		frameBuffer->getIconSize(pic_name.c_str(), &pic_width, &pic_height);
#if 0
		pic_width = max(pic_width, pic_max_w);
		pic_height = max(pic_height, pic_max_h);
#endif
	}
	
	if (pic_paint_mode == CC_PIC_IMAGE_MODE_ON) {
		g_PicViewer->getSize(pic_name.c_str(), &pic_width, &pic_height);
		if((pic_width > pic_max_w) || (pic_height > pic_max_h))
			g_PicViewer->rescaleImageDimensions(&pic_width, &pic_height, pic_max_w, pic_max_h);
	}

#ifdef DEBUG_CC
	if (pic_width == 0 || pic_height == 0)
		printf("[CComponentsPicture] %s file: %s, no icon dimensions found! width = %d, height = %d\n", __FUNCTION__, pic_name.c_str(),  pic_width, pic_height);
#endif

	initPosition();

	int sw = (shadow ? shadow_w :0);
	width = max(max(pic_width, pic_max_w), width)  + sw ;
	height = max(max(pic_height, pic_max_h), height)  + sw ;
}

void CComponentsPicture::initPosition()
{
	//using of real x/y values to paint images if this picture object is bound in a parent form
	int px = x, py = y;
	if (cc_parent){
		px = cc_xr;
		py = cc_yr;
	}

	if (pic_height>0 && pic_width>0){
		if (pic_align & CC_ALIGN_LEFT)
			pic_x = px+fr_thickness;
		if (pic_align & CC_ALIGN_RIGHT)
			pic_x = px+width-pic_width-fr_thickness;
		if (pic_align & CC_ALIGN_TOP)
			pic_y = py+fr_thickness;
		if (pic_align & CC_ALIGN_BOTTOM)
			pic_y = py+height-pic_height-fr_thickness;
		if (pic_align & CC_ALIGN_HOR_CENTER)
			pic_x = px+width/2-pic_width/2;
		if (pic_align & CC_ALIGN_VER_CENTER)
			pic_y = py+height/2-pic_height/2;

		do_paint = true;
	}
}

void CComponentsPicture::paintPicture()
{
	pic_painted = false;

	if (do_paint && cc_allow_paint){
#ifdef DEBUG_CC
	printf("	[CComponentsPicture] %s: paint image: %s (do_paint=%d)\n", __FUNCTION__, pic_name.c_str(), do_paint);
#endif
		if (pic_paint_mode == CC_PIC_IMAGE_MODE_OFF)
			pic_painted = frameBuffer->paintIcon(pic_name, pic_x, pic_y, 0 /*pic_max_h*/, pic_offset, pic_paint, pic_paintBg, col_body);
		else if (pic_paint_mode == CC_PIC_IMAGE_MODE_ON)
			pic_painted = g_PicViewer->DisplayImage(pic_name, pic_x, pic_y, pic_width, pic_height);
	}

	if  (pic_painted)
		do_paint = false;
}

void CComponentsPicture::paint(bool do_save_bg)
{
	initVarPicture();
	paintInit(do_save_bg);
	paintPicture();
}

void CComponentsPicture::hide(bool no_restore)
{
	hideCCItem(no_restore);
	pic_painted = false;
}


CComponentsChannelLogo::CComponentsChannelLogo( const int x_pos, const int y_pos, const int w, const int h,
						const uint64_t& channelId, const std::string& channelName,
						const int alignment, bool has_shadow,
						fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
						:CComponentsPicture(x_pos, y_pos, w, h,
						"", alignment, has_shadow,
						color_frame, color_background, color_shadow)
{
	channel_id = channelId;
	channel_name = channelName;
	initVarPictureChannellLogo();
}

void CComponentsChannelLogo::setPicture(const std::string& picture_name)
{
	pic_name = picture_name;
	channel_id = 0;
	channel_name = "";
	initVarPictureChannellLogo();
}

void CComponentsChannelLogo::setChannel(const uint64_t& channelId, const std::string& channelName)
{
	channel_id = channelId; 
	channel_name = channelName;
	initVarPictureChannellLogo();
}

void CComponentsChannelLogo::initVarPictureChannellLogo()
{
	string tmp_logo = pic_name;
	has_logo = false;

	if (!(channel_id == 0 && channel_name.empty() && pic_name.empty()))
		has_logo = GetLogoName(channel_id, channel_name, pic_name, &pic_width, &pic_height);

	if (!has_logo)
		pic_name = tmp_logo;
	
// #ifdef DEBUG_CC
	printf("\t[CComponentsChannelLogo] %s: init image: %s (has_logo=%d, channel_id=%" PRIu64 ")\n", __func__, pic_name.c_str(), has_logo, channel_id);
// #endif
	
	initVarPicture();
}

void CComponentsChannelLogo::paint(bool do_save_bg)
{
	initVarPictureChannellLogo();
	paintInit(do_save_bg);
	paintPicture();
	has_logo = false; //reset
}
