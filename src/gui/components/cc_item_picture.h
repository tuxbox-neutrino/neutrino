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
Picture is usable like each other CCItems.
*/

class CComponentsPicture : public CComponentsItem
{
	protected:
		///some internal modes for icon and image handling
		enum
		{
			CC_PIC_IMAGE_MODE_OFF 	= 0, //paint pictures in icon mode, mainly not scaled
			CC_PIC_IMAGE_MODE_ON	= 1, //paint pictures in image mode, paint scaled if required
			CC_PIC_IMAGE_MODE_AUTO	= 2
		};

		///property: path or name of image, icon names to find in /widget/icons.h, icons will paint never scaled
		std::string pic_name;
		///property: interface to CFrameBuffer::paintIcon() arg 5
		unsigned char pic_offset;

		bool pic_paint, pic_paintBg, pic_painted, do_paint;
		int pic_align, pic_x, pic_y, pic_width, pic_height;
		int pic_max_w, pic_max_h, pic_paint_mode;

		void init(	const int &x_pos, const int &y_pos, const int &w, const int &h,
				const std::string& image_name,
				const int &alignment,
				CComponentsForm *parent,
				bool has_shadow,
				fb_pixel_t color_frame,
				fb_pixel_t color_background,
				fb_pixel_t color_shadow);

		///initialize all required attributes
		void initCCItem();
		///initialize position of picture object dependendly from settings
		void initPosition();
		void paintPicture();

	public:
		CComponentsPicture( 	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::string& image_name,
					const int &alignment = CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		virtual inline void setPictureOffset(const unsigned char offset){pic_offset = offset;};
		virtual inline void setPicturePaint(bool paint_p){pic_paint = paint_p;};
		virtual inline void setPicturePaintBackground(bool paintBg){pic_paintBg = paintBg;};
		virtual void setPicture(const std::string& picture_name);
		virtual void setPicture(const char* picture_name);
		virtual void setPictureAlign(const int alignment);

		///return paint mode of internal image, true=image was painted, please do not to confuse with isPainted()! isPainted() is related to item itself.
		virtual inline bool isPicPainted(){return pic_painted;};

		virtual void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
		virtual void hide(bool no_restore = false);
		virtual inline void getPictureSize(int *pwidth, int *pheight){*pwidth=pic_width; *pheight=pic_height;};
		virtual void setMaxWidth(const int w_max){pic_max_w = w_max;};
		virtual void setMaxHeight(const int h_max){pic_max_h = h_max;};
};

class CComponentsChannelLogo : public CComponentsPicture, CPictureViewer
{
	protected:
		///initialize all required attributes
		void initVarPictureChannellLogo();

	private:
		uint64_t channel_id;
		std::string channel_name;
		bool has_logo;

	public:
		CComponentsChannelLogo( const int &x_pos, const int &y_pos, const int &w, const int &h,
					const uint64_t& channelId =0,
					const std::string& channelName = "",
					const int &alignment = CC_ALIGN_HOR_CENTER | CC_ALIGN_VER_CENTER,
					CComponentsForm *parent = NULL,
					bool has_shadow = CC_SHADOW_OFF,
					fb_pixel_t color_frame = COL_MENUCONTENT_PLUS_6,
					fb_pixel_t color_background = 0,
					fb_pixel_t color_shadow = COL_MENUCONTENTDARK_PLUS_0);

		void setChannel(const uint64_t& channelId, const std::string& channelName);
		void setPicture(const std::string& picture_name);
		void setPicture(const char* picture_name);
		bool hasLogo(){return has_logo;};
		void paint(bool do_save_bg = CC_SAVE_SCREEN_YES);
};

#endif
