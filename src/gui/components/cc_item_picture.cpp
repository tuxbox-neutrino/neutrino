/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'
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
#include <system/debug.h>


using namespace std;


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsPicture from CComponentsItem
CComponentsPicture::CComponentsPicture(	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::string& image_path,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	init(x_pos, y_pos, w, h, image_path, parent, has_shadow, color_frame, color_background, color_shadow);
}

CComponentsPicture::CComponentsPicture(	const int &x_pos, const int &y_pos,
					const std::string& image_name,
					CComponentsForm *parent,
					bool has_shadow,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	init(x_pos, y_pos, 0, 0, image_name, parent, has_shadow, color_frame, color_background, color_shadow);
}

void CComponentsPicture::init(	const int &x_pos, const int &y_pos, const int &w, const int &h,
				const string& image_name,
				CComponentsForm *parent,
				bool has_shadow,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
{
	//CComponents, CComponentsItem
	cc_item_type 	= CC_ITEMTYPE_PICTURE;

	//CComponents
	x 		= x_pos;
	y 		= y_pos;
	height		= h;
	width 		= w;
	shadow		= has_shadow;
	shadow_w	= SHADOW_OFFSET;
	col_frame 	= color_frame;
	col_body	= color_background;
	col_shadow	= color_shadow;

	//CComponentsPicture
	pic_name 	= image_name;
	is_icon		= false;

	is_image_painted= false;
	do_paint	= true;

	getSupportedImageFormats(v_ext);
	v_ext.resize(unique(v_ext.begin(), v_ext.end()) - v_ext.begin());
	initCCItem();
	initParent(parent);
}

void CComponentsPicture::setPicture(const std::string& picture_name)
{
	pic_name = picture_name;
	initCCItem();
}

void CComponentsPicture::setPicture(const char* picture_name)
{
	string s_tmp = "";
	if (picture_name)
		s_tmp = string(picture_name);
	setPicture(s_tmp);
}


void CComponentsPicture::initCCItem()
{
	//handle size
	int w_pic = width;
	int h_pic = height;
	
	if (pic_name.empty())
		return;

	//check for path or name, set icon or image with full path
	string::size_type pos = pic_name.find("/", 0);
	is_icon = (pos == string::npos);

	dprintf(DEBUG_INFO, "[CComponentsPicture] %s: detected image file: is_icon: %d (pos= %d), pic_name=%s\n", __func__, is_icon, pos, pic_name.c_str());

	//get current image size
	getImageSize(&w_pic, &h_pic);

	//for icons (names without explicit path) set dimensions of "this" to current image...//TODO: centering image/icon
	if (is_icon){
		width = w_pic;
		height = max(h_pic, height);
	}
	else{ //defined values ​​in constructor or defined via setters defined, have priority, value 0 is not allowed
		if (width == 0)
			width = w_pic;
		if (height == 0)
			height = h_pic;
	}

	//resize/scale image if required, if no icon mode detected, use real image size
	if (!is_icon){
		if (width != w_pic || height != h_pic)
			rescaleImageDimensions(&w_pic, &h_pic, width, height);
	}
}

void CComponentsPicture::initPosition(int *x_position, int *y_position)
{
	//using of real x/y values to paint images if this picture object is bound in a parent form
	*x_position = x;
	*y_position = y;

	if (cc_parent){
		*x_position = cc_xr;
		*y_position = cc_yr;
	}
}


void CComponentsPicture::getImageSize(int* width_image, int *height_image)
{
	if (!is_icon)
		CPictureViewer::getSize(pic_name.c_str(), width_image, height_image);
	else
		frameBuffer->getIconSize(pic_name.c_str(), width_image, height_image);
}

void CComponentsPicture::paintPicture()
{
	is_image_painted = false;
	//initialize image position
	int x_pic = x;
	int y_pic = y;
	initPosition(&x_pic, &y_pic);

	if (pic_name.empty())
		return;

	dprintf(DEBUG_INFO, "[CComponentsPicture] %s: paint image file: pic_name=%s\n", __func__, pic_name.c_str());
	if (cc_allow_paint){
		if (!is_icon)
			is_image_painted = DisplayImage(pic_name, x_pic, y_pic, width, height);
		else
			is_image_painted = frameBuffer->paintIcon(pic_name, x_pic, y_pic, height, 1, do_paint, paint_bg, col_body);
	}
}

void CComponentsPicture::paint(bool do_save_bg)
{
	paintInit(do_save_bg);
	paintPicture();
}

void CComponentsPicture::hide(bool no_restore)
{
	hideCCItem(no_restore);
	is_image_painted = false;
}


CComponentsChannelLogo::CComponentsChannelLogo( const int &x_pos, const int &y_pos, const int &w, const int &h,
						const std::string& channelName,
						const uint64_t& channelId,
						CComponentsForm *parent,
						bool has_shadow,
						fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
						:CComponentsPicture(x_pos, y_pos, w, h,
						"", parent, has_shadow,
						color_frame, color_background, color_shadow)
{
	setChannel(channelId, channelName);
	alt_pic_name = "";
}

CComponentsChannelLogo::CComponentsChannelLogo( const int &x_pos, const int &y_pos,
						const std::string& channelName,
						const uint64_t& channelId,
						CComponentsForm *parent,
						bool has_shadow,
						fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow)
						:CComponentsPicture(x_pos, y_pos, 0, 0,
						"", parent, has_shadow,
						color_frame, color_background, color_shadow)
{
	setChannel(channelId, channelName);
	alt_pic_name = "";
}


void CComponentsChannelLogo::setAltLogo(const std::string& picture_name)
{
	alt_pic_name = picture_name;
	channel_id = 0;
	channel_name = "";
	has_logo = true;
	initCCItem();
}

void CComponentsChannelLogo::setAltLogo(const char* picture_name)
{
	string s_tmp = "";
	if (picture_name)
		s_tmp = string(picture_name);
	this->setAltLogo(s_tmp);
}

void CComponentsChannelLogo::setChannel(const uint64_t& channelId, const std::string& channelName)
{
	channel_id = channelId; 
	channel_name = channelName;

	has_logo = GetLogoName(channel_id, channel_name, pic_name, &width, &height);

	if (!has_logo)
		pic_name = alt_pic_name;
	
	initCCItem();
}
