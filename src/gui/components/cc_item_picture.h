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
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CC_ITEM_PICTURE_H__
#define __CC_ITEM_PICTURE_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc_base.h"
#include <string>
#include <driver/pictureviewer/pictureviewer.h>

//! Sub class of CComponentsItem. Shows box with image with assigned attributes.
/*!
Picture is usable as an object like each other CCItems.
*/

class CComponentsPicture : public CComponentsItem
{
	protected:
		///possible image formats
		std::vector<std::string> v_ext;

		///property: name of image (without extensionn) full path to image (with extension), icon names to find in /widget/icons.h, icons will paint never scaled
		std::string pic_name;
 
		///indicate that image was sucessful painted
		bool is_image_painted;
		///image is defined only by name without full path, handling as icon, not as scalable image, default = false
		bool is_icon;
		///sets that image may be painted, default = false
		bool do_paint;

		void init(	const int &x_pos, const int &y_pos, const int &w, const int &h,
				const std::string& image_name,
				CComponentsForm *parent,
				bool has_shadow,
				fb_pixel_t color_frame,
				fb_pixel_t color_background,
				fb_pixel_t color_shadow);

		///initialize all required attributes
		void initCCItem();
		///initialize position of picture object dependendly from settings
		void initPosition(int *x_position, int *y_position);
		///paint image
		void paintPicture();

	public:
		CComponentsPicture( 	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::string& image_name,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		CComponentsPicture( 	const int &x_pos, const int &y_pos,
					const std::string& image_name,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///sets an image name (unscaled icons only), full image path or url to an iamge file (scalable)
		virtual void setPicture(const std::string& picture_name);
		///sets an image name (unscaled icons only), full image path or url to an iamge file (scalable)
		virtual void setPicture(const char* picture_name);

		///return paint mode of internal image, true=image was painted, please do not to confuse with isPainted()! isPainted() is related to item itself.
		virtual inline bool isPicPainted(){return is_image_painted;};

		///handle image size
		void getImageSize(int* width_image, int *height_image);

		///paint item
		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		///hide item
		virtual void hide(bool no_restore = false);
};

class CComponentsChannelLogo : public CComponentsPicture
{
	private:
		///channel id
		uint64_t channel_id;
		///channel name
		std::string channel_name;
		
		///alternate image file, if no channel logo is available
		std::string alt_pic_name;
		
		///indicates that logo is available, after paint or new instance, value = false
		bool has_logo;

	public:
		CComponentsChannelLogo( const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::string& channelName = "",
					const uint64_t& channelId =0,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);
		
		CComponentsChannelLogo( const int &x_pos, const int &y_pos,
					const std::string& channelName = "",
					const uint64_t& channelId =0,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		///set channel id and/or channel name, NOTE: channel name is prefered
		void setChannel(const uint64_t& channelId, const std::string& channelName);
		
		///set an alternate logo if no logo is available NOTE: value of has_logo will set to true
		void setAltLogo(const std::string& picture_name);
		///set an alternate logo if no logo is available, NOTE: value of has_logo will set to true
		void setAltLogo(const char* picture_name);
		
		///returns true, if any logo is available, also if an alternate logo was setted
		bool hasLogo(){return has_logo;};
		
};

#endif
