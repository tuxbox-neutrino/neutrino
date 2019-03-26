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
#include <driver/pictureviewer/pictureviewer.h>

extern CPictureViewer * g_PicViewer;

using namespace std;


//-------------------------------------------------------------------------------------------------------
//sub class CComponentsPicture from CComponentsItem
CComponentsPicture::CComponentsPicture(	const int &x_pos, const int &y_pos, const int &w, const int &h,
					const std::string& image_name,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow, int transparent)
{
	init(x_pos, y_pos, w, h, image_name, parent, shadow_mode, color_frame, color_background, color_shadow, transparent, SCALE);
}

CComponentsPicture::CComponentsPicture(	const int &x_pos, const int &y_pos,
					const std::string& image_name,
					CComponentsForm *parent,
					int shadow_mode,
					fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow, int transparent)
{
	init(x_pos, y_pos, 0, 0, image_name, parent, shadow_mode, color_frame, color_background, color_shadow, transparent, NO_SCALE);
}


void CComponentsPicture::init(	const int &x_pos, const int &y_pos, const int &w, const int &h,
				const string& image_name,
				CComponentsForm *parent,
				int shadow_mode,
				fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow, int transparent,
				bool allow_scale)
{
	//CComponents, CComponentsItem
	cc_item_type.id 	= CC_ITEMTYPE_PICTURE;
	cc_item_type.name 	= "cc_image_box";

	//CComponents
	x =	x_old	= x_pos;
	y =	y_old	= y_pos;
	width	= width_old = dx = dxc = w;
	height	= height_old = dy = dyc = h;
	dx_orig = dy_orig = 0;
	pic_name = pic_name_old = image_name;
	shadow		= shadow_mode;
	shadow_w	= OFFSET_SHADOW;
	col_frame 	= color_frame;
	col_body	= color_background;
	col_shadow	= color_shadow;
	col_shadow_clean= col_body;
	do_scale	= allow_scale;
	image_cache	= NULL; //image
	enable_cache	= false;
	is_image_painted= false;
	do_paint	= true;
	image_transparent = transparent;
	cc_paint_cache	= false; //bg
	keep_dx_aspect 	= false;
	keep_dy_aspect	= false;
	need_init	= true;
	initCCItem();
	initParent(parent);
}

void CComponentsPicture::clearCache()
{
	if (image_cache){
		dprintf(DEBUG_DEBUG, "\033[32m[CComponentsPicture] %s - %d: clean up image cache %s\033[0m\n", __func__, __LINE__, pic_name.c_str());
		delete[] image_cache;
		image_cache = NULL;
	}
}

void CComponentsPicture::setPicture(const std::string& picture_name)
{
	if (pic_name == picture_name)
		return;
	width	= dx = dxc = 0;
	height	= dy = dyc = 0;
	need_init = true;
	clearCache();
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

void CComponentsPicture::setWidth(const int& w, bool keep_aspect)
{
	if (w == width && keep_aspect == keep_dy_aspect)
		return;
	CComponentsItem::setWidth(w);
	need_init = true;
	do_scale = true;
	keep_dy_aspect = keep_aspect;
	initCCItem();
}

void CComponentsPicture::setHeight(const int& h, bool keep_aspect)
{
	if (h == height && keep_aspect == keep_dx_aspect)
		return;
	CComponentsItem::setHeight(h);
	need_init = true;
	do_scale = true;
	keep_dx_aspect = keep_aspect;
	initCCItem();
}

void CComponentsPicture::setXPos(const int& xpos)
{
	CComponentsItem::setXPos(xpos);
	if (xpos == x)
		return;
	need_init = true;
	initCCItem();
}

void CComponentsPicture::setYPos(const int& ypos)
{
	CComponentsItem::setYPos(ypos);
	if (ypos == y)
		return;
	need_init = true;
	initCCItem();
}

void CComponentsPicture::initCCItem()
{
	if (pic_name.empty() || !need_init){
		dprintf(DEBUG_DEBUG, "[CComponentsPicture] %s - %d : no init required [file: %s] [need init: %d]...\n",  __func__, __LINE__, pic_name.c_str(), need_init);
		return;
	}
	//reset condition for new init
	need_init = false;

	//check for path or name, set icon or image with full path, has no path, then use as icon and disble scale mode
	string::size_type pos = pic_name.find("/", 0);
	if (pos == string::npos)
		do_scale = false;

	//initial internal size
	if (!do_scale){
		//use image/icon size as object dimension values
		frameBuffer->getIconSize(pic_name.c_str(), &width, &height);

		/* frameBuffer->getIconSize() normally evaluates only icon names, no paths.
		 * So it is possible that we have wrong dimension values.
		 * So we fall back to picture viewer methode.
		 * That's always a cramp, why we don't have an unified solution in render classes?
		 * Anyway...this is only a workaround, otherwies it is possible, that dimension values are wrong or = 0 and
		 * this could lead to problems if external items are reliant on these values,
		 * and in worst case, no image would be painted!
		*/
		if (width == 0 || height == 0){
			int dx_tmp, dy_tmp;
			g_PicViewer->getSize(pic_name.c_str(), &dx_tmp, &dy_tmp);
			if (width == 0)
				width = dx_tmp;
			if (height == 0)
				height = dy_tmp;
		}
		dx_orig = width;
		dy_orig = height;

		/* leave init methode here if we in no scale mode
		 * otherwise goto next step!
		*/
		return;
	}
	else{	/* Here we are in scale mode
		 * first check current item dimensions (width/height) and for different values and
		 * check internal dimension values (dx/dy) and ensure that values are >0
		 * real image size
		*/
		g_PicViewer->getSize(pic_name.c_str(), &dx_orig, &dy_orig);
		if  ((dx != width || dy != height) || (dx == 0 || dy == 0)){
			dx = dx_orig;
			dy = dy_orig;
			//g_PicViewer->getSize(pic_name.c_str(), &dx, &dy);
		}
	}

	/* on next step check item dimensions (width/height) for 0 values
	 * and fill with current internal (dx/dy) dimension values.
	 * values <= 0 are not allowed
	*/
	if (width == 0)
		width = dx;
	if (height == 0)
		height = dy;

	/* on next step, check dimensions and
	 * leave if dimensions are equal
	 */
	if (width == dx && height == dy)
		return;

	/* finally handle scale behavior
	 * This evaluates the parameters given
	 * by setters setWidth/setHeight
	 * these steps are required to assign the current image dimensions to item dimensions
	*/
	if (keep_dx_aspect && dy)
	{
		float h_ratio = float(height)*100/(float)dy;
		width = int(h_ratio*(float)dx/100);
		if (frameBuffer->needAlign4Blit() &&
		    do_scale && (width > 10 || height > 10))
			width = frameBuffer->getWidth4FB_HW_ACC(x+fr_thickness, width-2*fr_thickness)+2*fr_thickness;
		keep_dx_aspect = false;
	}
	if (keep_dy_aspect && dx)
	{
		float w_ratio = float(width)*100/(float)dx;
		height = int(w_ratio*(float)dy/100);
		keep_dy_aspect = false;
	}

	//resize image and apply current assigned scale values
	int w_2scale = width;
	int h_2scale = height;
	g_PicViewer->rescaleImageDimensions(&width, &height, w_2scale, h_2scale);
}

void CComponentsPicture::initPosition(int *x_position, int *y_position)
{
	*x_position = x;
	*y_position = y;

	if (cc_parent){ //using of real x/y values to paint images if this picture object is bound in a parent form
		*x_position = cc_xr;
		*y_position = cc_yr;
	}
}


void CComponentsPicture::getRealSize(int* dx_original, int *dy_original)
{
	*dx_original = dx_orig; 
	*dy_original = dy_orig;
}

int CComponentsPicture::getWidth()
{
	int w, h;
	getSize(&w, &h);
	return w;
}

int CComponentsPicture::getHeight()
{
	int w, h;
	getSize(&w, &h);
	return h;
}

void CComponentsPicture::paintPicture()
{
	is_image_painted = false;
	//initialize image position
	int x_pic = x;
	int y_pic = y;
	initPosition(&x_pic, &y_pic);
	x_pic += fr_thickness;
	y_pic += fr_thickness;

	initCCItem();

	if (pic_name.empty()){
		clearCache();
		return;
	}

	if (cc_allow_paint){
		if (image_cache == NULL){
			frameBuffer->SetTransparent(image_transparent);
			if (do_scale)
				is_image_painted = g_PicViewer->DisplayImage(pic_name, x_pic, y_pic, width-2*fr_thickness, height-2*fr_thickness);
			else
				is_image_painted = frameBuffer->paintIcon(pic_name, x_pic, y_pic, height, 1, do_paint, paint_bg, col_body);

			if (is_image_painted){
				frameBuffer->SetTransparentDefault();
				if (enable_cache && do_scale){
					dprintf(DEBUG_DEBUG, "\033[32m[CComponentsPicture] %s - %d: create cached image from pic_name=%s\033[0m\n", __func__, __LINE__, pic_name.c_str());
					dxc = width-2*fr_thickness;
					dyc = height-2*fr_thickness;
					image_cache = getScreen(x_pic, y_pic, dxc, dyc);
				}
			}
			else
				dprintf(DEBUG_NORMAL, "\033[31m[CComponentsPicture] %s - %d: error: paint of image failed: %s\033[0m\n", __func__, __LINE__, pic_name.c_str());
		}else{
			dprintf(DEBUG_DEBUG, "\033[32m[CComponentsPicture] %s - %d: paint cached image from pic_name=%s\033[0m\n", __func__, __LINE__, pic_name.c_str());
			frameBuffer->RestoreScreen(x_pic, y_pic, dxc, dyc, image_cache);
		}
	}
}

void CComponentsPicture::paint(bool do_save_bg)
{
	if (pic_name.empty())
		return;
	paintInit(do_save_bg);
	paintPicture();
}

void CComponentsPicture::hide()
{
	CComponents::hide();
	is_image_painted = false;
}

bool CComponentsPicture::hasChanges()
{
	bool ret = false;
	if (pic_name != pic_name_old){
		pic_name_old = pic_name;
		ret = true;
	}
	if (CCDraw::hasChanges())
		ret = true;

	return ret;
}

void CComponentsPicture::paintTrigger()
{
	if (!is_painted  && !isPicPainted())
		paint1();
	else
		hide();
}


CComponentsChannelLogo::CComponentsChannelLogo( const int &x_pos, const int &y_pos, const int &w, const int &h,
						const std::string& channelName,
						const uint64_t& channelId,
						CComponentsForm *parent,
						int shadow_mode,
						fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow, int transparent)
						:CComponentsPicture(x_pos, y_pos, w, h,
						"", parent, shadow_mode,
						color_frame, color_background, color_shadow, transparent)
{
	init(channelId, channelName, SCALE);
}

CComponentsChannelLogo::CComponentsChannelLogo( const int &x_pos, const int &y_pos,
						const std::string& channelName,
						const uint64_t& channelId,
						CComponentsForm *parent,
						int shadow_mode,
						fb_pixel_t color_frame, fb_pixel_t color_background, fb_pixel_t color_shadow, int transparent)
						:CComponentsPicture(x_pos, y_pos, 0, 0,
						"", parent, shadow_mode,
						color_frame, color_background, color_shadow, transparent)
{
	init(channelId, channelName, NO_SCALE);
}

void CComponentsChannelLogo::init(const uint64_t& channelId, const std::string& channelName, bool allow_scale)
{
	cc_item_type.id 	= CC_ITEMTYPE_CHANNEL_LOGO;
	cc_item_type.name 	= "cc_channel_logo_box";
	channel_name = "";
	channel_id = 0;
	alt_pic_name = "";
	setChannel(channelId, channelName);
	do_scale = allow_scale;
	enable_event_logo = false;
}
void CComponentsChannelLogo::setAltLogo(const std::string& picture_name)
{
	if (alt_pic_name == picture_name)
		return;
	need_init = true;
	alt_pic_name = picture_name;
	channel_id = 0;
	channel_name = "";
	has_logo = !alt_pic_name.empty();
	if (has_logo)
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
	need_init = true;
	string image = pic_name;

	if (channelId || !channelName.empty()){
		if ((channel_id == channelId) && (channel_name == channelName))
			need_init = false;
	}

	channel_id = channelId;
	channel_name = channelName;

	int dummy;

	has_logo = g_PicViewer->GetLogoName(channel_id, channel_name, image, &dummy, &dummy, false, enable_event_logo);

	if (!has_logo)//no logo was found, use altrenate icon or logo
		image = alt_pic_name;

	//if logo or alternate image still not available, set has logo to false
	has_logo = !image.empty();

	//refresh object
	setPicture(image);

	//set has_logo to false if no dimensions were detected
	if (width && height)
		has_logo = true;

	doPaintBg(false);
}
