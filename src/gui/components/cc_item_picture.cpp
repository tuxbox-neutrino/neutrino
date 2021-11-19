/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2021, Thilo Graf 'dbt'
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

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include "cc_item_picture.h"
#include <unistd.h>
#include <system/debug.h>
#include <driver/pictureviewer/pictureviewer.h>

extern CPictureViewer *g_PicViewer;

CCPictureBase::CCPictureBase(const int &x_pos,
				const int &y_pos,
				const int &w,
				const int &h,
				const std::string &image,
				CComponentsForm *parent,
				int shadow_mode,
				fb_pixel_t color_frame,
				fb_pixel_t color_background,
				fb_pixel_t color_shadow,
				const int &transparency_mode)
				: CComponentsShapeSquare(x_pos, y_pos, w, h, NULL, shadow_mode, color_frame, color_background, color_shadow)
{
	cc_item_type.id 	= CC_ITEMTYPE_PICTURE;
	cc_item_type.name 	= image.empty() ? "cc_image_box" : image;
	cc_bg_image_tr_mode	= transparency_mode;

	setPicture(image, w, h);
	initParent(parent);
}

void CCPictureBase::setPicture(const std::string &name, const int &w, const int &h)
{
	setBodyBGImageName(name, "", "");

	dx_orig = 0;
	dy_orig = 0;
	int dx_tmp = dx_orig;
	int dy_tmp = dy_orig;

	if (getBodyBGImage().empty())
	{
		dprintf(DEBUG_INFO, "[CCPictureBase] [%s - %d] no image defined or doesn't exists %s\n", __func__, __LINE__, name.c_str());
		return;
	}

	g_PicViewer->getSize(getBodyBGImage().c_str(), &dx_orig, &dy_orig);

	float dx_ratio = static_cast<float>(dx_orig) / static_cast<float>(std::max(1, w));
	float dy_ratio = static_cast<float>(dy_orig) / static_cast<float>(std::max(1, h));

	dx_tmp = w;
	dy_tmp = h;

	if (w == 0 && h == 0)
	{
		dx_tmp = dx_orig;
		dy_tmp = dy_orig;
	}
	else
	{
		if (w == 0)
			dx_tmp = static_cast<int>(static_cast<float>(dx_orig) / dy_ratio);
		if (h == 0)
			dy_tmp = static_cast<int>(static_cast<float>(dy_orig) / dx_ratio);
	}

	CCDraw::setWidth(dx_tmp);
	CCDraw::setHeight(dy_tmp);
}

void CCPictureBase::setPicture(const char *name, const int &w, const int &h)
{
	std::string s_tmp = "";
	if (name)
		s_tmp = std::string(name);
	setPicture(s_tmp,  w, h);
}

void CCPictureBase::setWidth(const int &w, bool keep_aspect)
{
	setPicture(cc_bg_std_image, w, keep_aspect ? 0 : height);
}

void CCPictureBase::setHeight(const int &h, bool keep_aspect)
{
	setPicture(cc_bg_std_image, keep_aspect ? 0 : width, h);
}

void CCPictureBase::getRealSize(int *dx, int *dy)
{
	*dx = dx_orig;
	*dy = dy_orig;
}

CComponentsChannelLogo::CComponentsChannelLogo(const int &x_pos,
						const int &y_pos,
						const int &w,
						const int &h,
						const std::string &channelName,
						const uint64_t &channelId,
						CComponentsForm *parent,
						int shadow_mode,
						fb_pixel_t color_frame,
						fb_pixel_t color_background,
						fb_pixel_t color_shadow,
						const int &transparency_mode)
						: CCPictureBase(x_pos, y_pos, w, h, "", parent, shadow_mode, color_frame, color_background, color_shadow, transparency_mode)
{
	cc_item_type.id 	= CC_ITEMTYPE_CHANNEL_LOGO;
	cc_item_type.name 	= "cc_channel_logo_box";

	setChannel(channelId, channelName, w, h);
}


void CComponentsChannelLogo::setChannel(const uint64_t &channelId, const std::string &channelName, const int &w, const int &h)
{
	std::string image = "";
	channel_id = channelId;
	channel_name = channelName;
	has_logo = false;

	//dimensions not required here, will be handled in member setPicture(), therefore here only dummy variables
	int dmy_x, dmy_y;
	has_logo = g_PicViewer->GetLogoName(channel_id, channel_name, image, &dmy_x, &dmy_y, false, enable_event_logo);

	//no logo was found, use alternate icon or logo
	if (!has_logo)
		image = alt_pic_name;

	//if logo or alternate image not available, set has_logo to false
	has_logo = !image.empty();
	if (!has_logo)
		image = "blank";

	//refresh object
	setPicture(image, w, h);

	//for sure, if no dimensions were detected set value of has_logo = false
	if (width && height && has_logo)
		has_logo = true;
}

void CComponentsChannelLogo::setAltLogo(const std::string &name, const int &w, const int &h)
{
	if (alt_pic_name == name)
		return;

	alt_pic_name = name;
	channel_id = 0;
	channel_name = "";
	has_logo = !alt_pic_name.empty();

	if (has_logo)
		setPicture(alt_pic_name, w, h);
}

void CComponentsChannelLogo::setAltLogo(const char *name, const int &w, const int &h)
{
	std::string s_tmp = "";

	if (name)
		s_tmp = std::string(name);

	setAltLogo(s_tmp, w, h);
}
