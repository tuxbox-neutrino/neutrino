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
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CC_ITEM_PICTURE_V2_H__
#define __CC_ITEM_PICTURE_V2_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "cc_base.h"
#include "cc_frm.h"
#include "cc_item_shapes.h"
#include <string>

//! Shows box with image with assigned attributes.
/*!
Usable as an object like each other CCItems.
*/

class CCPicture : public CComponentsShapeSquare
{
	private:
		int dx_orig,  dy_orig;
	public:

		CCPicture(const int &x_pos,
			const int &y_pos,
			const int &w,
			const int &h,
			const std::string &image,
			CComponentsForm *parent = NULL,
			int shadow_mode = CC_SHADOW_OFF,
			fb_pixel_t color_frame = COL_FRAME_PLUS_0,
			fb_pixel_t color_background = 0,
			fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
			const int &transparency_mode = CFrameBuffer::TM_EMPTY);

		virtual~CCPicture() {};

		///sets an image name, full image path or url to an image file
		void setPicture(const std::string &name, const int &w = 0, const int &h = 0);
		///sets an image name, full image path or url to an image file
		void setPicture(const char *name, const int &w = 0, const int &h = 0);

		///returns current assigned image name
		std::string getPictureName() {return cc_bg_std_image;}

		void SetTransparent(const int &mode) {setBodyBGImageTranparencyMode(mode);}

		///import base class methods for width and height to avoid -Woverloaded-virtual
		using CCDraw::setWidth;
		using CCDraw::setHeight;

		///set width of object, keep_aspect = true keeps ratio related to height
		void setWidth(const int &w, bool keep_aspect = true);
		///set height of object, keep_aspect = true keeps ratio related to width
		void setHeight(const int &h, bool keep_aspect = true);

		///get original image size
		void getRealSize(int *dx, int *dy);
};

//! Shows box with channel logo with assigned attributes.
/*!
Usable as an object like each other CCItems.
*/

class CComponentsChannelLogo : public CCPicture
{
	private:
		///channel id
		uint64_t channel_id;
		///channel name
		std::string channel_name;

		///mode of event logo
		bool enable_event_logo;

		///alternate image file, if no channel logo is available
		std::string alt_pic_name;

		///indicates that logo is available, after paint or new instance, value = false
		bool has_logo;

	public:
		/*!
		Constructor for channel image objects: use this for scaled channel logos.
		Does the same like class CComponentsPicture() with parameters w (width) and h (height), see above!
		Requires parameter channel_name or channelId instead image filename
		NOTE: channel name string is prefered!
		 */
		CComponentsChannelLogo(const int &x_pos,
			const int &y_pos,
			const int &w,
			const int &h,
			const std::string &channelName = "",
			const uint64_t &channelId = 0,
			CComponentsForm *parent = NULL,
			int shadow_mode = CC_SHADOW_OFF,
			fb_pixel_t color_frame = COL_FRAME_PLUS_0,
			fb_pixel_t color_background = 0,
			fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
			const int &transparency_mode = CFrameBuffer::TM_EMPTY);

		/*!
		Constructor for channel image objects: use this for non scaled channel logos.
		Does the same like class CComponentsPicture() without parameters w (width) and h (height), see above!
		Requires parameter channel_name or channelId instead image filename
		NOTE: channel name string is prefered!
		 */
		CComponentsChannelLogo(const int &x_pos,
			const int &y_pos,
			const std::string &channelName = "",
			const uint64_t &channelId = 0,
			CComponentsForm *parent = NULL,
			int shadow_mode = CC_SHADOW_OFF,
			fb_pixel_t color_frame = COL_FRAME_PLUS_0,
			fb_pixel_t color_background = 0,
			fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
			const int &transparency_mode = CFrameBuffer::TM_EMPTY)
			: CComponentsChannelLogo(x_pos, y_pos, 0, 0, channelName, channelId, parent, shadow_mode, color_frame, color_background, color_shadow, transparency_mode) {};

		virtual~CComponentsChannelLogo() {};

		///set channel id and/or channel name, NOTE: channel name is prefered
		void setChannel(const uint64_t &channelId, const std::string &channelName, const int &w = 0, const int &h = 0);

		uint64_t getChannelID() {return channel_id;}

		///set an alternate logo if no logo is available NOTE: value of has_logo will set to true
		void setAltLogo(const std::string &name, const int &w = 0, const int &h = 0);
		///set an alternate logo if no logo is available, NOTE: value of has_logo will set to true
		void setAltLogo(const char *name, const int &w = 0, const int &h = 0);

		///enable/disable eventlogo
		void enableEventLogo(bool enable = true) {enable_event_logo = enable;}
		void disableEventLogo() {enableEventLogo(false);}

		///returns true, if any logo is available, also if an alternate logo was defined
		bool hasLogo() {return has_logo;}

};

class CComponentsPicture : public CComponentsForm
{
	private:
		CCPicture *ccp;

		void init(const int &x_pos, const int &y_pos, const std::string &image, const int &transparency_mode)
		{
			setPos(x_pos, y_pos);

			if (ccp == NULL)
			{
				ccp = new CCPicture(0, 0, width, height, image, this, CC_SHADOW_OFF, COL_FRAME_PLUS_0, 0, COL_SHADOW_PLUS_0, transparency_mode);
			}
			else
			{
				ccp->setPicture(image, width, height);
				ccp->SetTransparent(transparency_mode);
			}

			CComponentsForm::setWidth(ccp->getWidth());
			CComponentsForm::setHeight(ccp->getHeight());
		};

	public:
		/*!
		Use this for scaled images.
		Dimensions are defined with parameters w (width) and h (height).
		Only dimension values > 0 have a scale effect!
		If one of the dimension parameters = 0, the ratio of image will be kept.
		If scaling is not required, you should use overloaded class CComponentsPicture without dimension parameters or set values of w and h = 0.
		width and height are retrievable e.g. with getWidth() or getHeight().

		* @param[in]   int x_pos,  item position
		* @param[in]   int y_pos,  item position
		* @param[in]   int w,  item width
		* @param[in]   int h,  item height
		* @param[in]   std::string image,  image name,  path or url
		* @param[in]   CComponentsForm *parent,  parent form in which this item is embedded,  optional,  default NULL
		* @param[in]   int shadow_mode,  mode of shadow,  optional,  default OFF
		* @param[in]   fb_pixel_t color_frame,  color of frame,  optional,  default COL_FRAME_PLUS_0
		* @param[in]   fb_pixel_t color_background,  color of background,  optional,  default 0
		* @param[in]   fb_pixel_t color_shadow,  color of shadow,  optional,  default COL_SHADOW_PLUS_0
		* @param[in]   int transparency_mode,  mode of image,  optional,  default CFrameBuffer::TM_EMPTY
		*/
		CComponentsPicture(const int &x_pos,
			const int &y_pos,
			const int &w,
			const int &h,
			const std::string &image,
			CComponentsForm *parent = NULL,
			int shadow_mode = CC_SHADOW_OFF,
			fb_pixel_t color_frame = COL_FRAME_PLUS_0,
			fb_pixel_t color_background = 0,
			fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
			int transparency_mode = CFrameBuffer::TM_EMPTY)
			: CComponentsForm(x_pos, y_pos, w, h, parent, shadow_mode, color_frame, color_background, color_shadow)
		{
			cc_item_type.name = image.empty() ? "cc_image_frm" : image;
			ccp = NULL;
			paint_bg = false;
			init(x_pos, y_pos, image, transparency_mode);
		};

		/*!
		Use this for non scaled images.
		Dimensions are defined by image itself.
		If scaling is required, you should use class CComponentsPicture with dimension parameters see above.
		width and height are defined by given image dimensions itself and are retrievable e.g. with getWidth() or getHeight().
		* @param[in]   int x_pos,  item position
		* @param[in]   int y_pos,  item position
		* @param[in]   std::string image,  image name,  path or url
		* @param[in]   CComponentsForm *parent,  parent form in which this item is embedded,  optional,  default NULL
		* @param[in]   int shadow_mode,  mode of shadow,  optional,  default OFF
		* @param[in]   fb_pixel_t color_frame,  color of frame,  optional,  default COL_FRAME_PLUS_0
		* @param[in]   fb_pixel_t color_background,  color of background,  optional,  default 0
		* @param[in]   fb_pixel_t color_shadow,  color of shadow,  optional,  default COL_SHADOW_PLUS_0
		* @param[in]   int transparency_mode,  mode of image,  optional,  default CFrameBuffer::TM_EMPTY
		*/
		CComponentsPicture(const int &x_pos,
			const int &y_pos,
			const std::string &image,
			CComponentsForm *parent = NULL,
			int shadow_mode = CC_SHADOW_OFF,
			fb_pixel_t color_frame = COL_FRAME_PLUS_0,
			fb_pixel_t color_background = 0,
			fb_pixel_t color_shadow = COL_SHADOW_PLUS_0,
			int transparency_mode = CFrameBuffer::TM_EMPTY)
			: CComponentsPicture(x_pos, y_pos, 0, 0, image, parent, shadow_mode, color_frame, color_background, color_shadow, transparency_mode)
		{};

		virtual~CComponentsPicture() {};

		/*!
		Passing original image size, use this to get origin dimensions of embedded image.
		* @param[in]   *int pointer to image width
		* @param[in]   *int pointer to image height
		*/
		void getRealSize(int *dx, int *dy);

		/*!
		Sets an image name, full image path or url to an image file.
		Different dimension values > 0 will scale images, if is only one dimension value != 0, ratio will be kept.
		* @param[in]   std::string or const *char for image
		* @param[in]   int w image width, optional,  default 0
		* @param[in]   int h image height, optional,  default 0
		*/
		void setPicture(const std::string &name, const int &w = 0, const int &h = 0);
		void setPicture(const char *name, const int &w = 0, const int &h = 0);

		/*!
		 * This will return the current image with full path
		 *
		 * @return std::string	returns image path
		*/
		std::string getPictureName();

		/*!
		Sets image transparency mode
		* @param[in]   int mode
		*/
		void SetTransparent(const int &mode);

		//import base class methods for width and height to avoid -Woverloaded-virtual
		using CCDraw::setWidth;
		using CCDraw::setHeight;

		/*!
		Sets width of object, keep_aspect = true keeps ratio related to height
		* @param[in]   int w image width
		* @param[in]   bool keep_aspect,  optional default = true
		*/
		void setWidth(const int &w, bool keep_aspect = true);

		/*!
		Sets height of object, keep_aspect = true keeps ratio related to width
		* @param[in]   int h image width
		* @param[in]   bool keep_aspect,  optional default = true
		*/
		void setHeight(const int &h, bool keep_aspect = true);

		/*!
		Paints item
		* @param[in]   bool do_save_bg,  optional default = true. True allows using hide() to restore background
		*
		* @see paintInit()
		*/
		void paint(const bool &do_save_bg = true);

		/**Enable/Disable paint caching for body and shadow
		 * @see	       CCDraw::enablePaintCache(),  CCDraw::disablePaintCache()
		*/
 		void enablePaintCache(const bool &enable = true) {ccp->enablePaintCache(enable); enablePaintCache(enable);}
		void disablePaintCache(){enablePaintCache(false);}
};

#endif  //__CC_ITEM_PICTURE_V2_H__
