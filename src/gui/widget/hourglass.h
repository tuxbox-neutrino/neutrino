/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Hourglass, provides an hourglass/snakeloader function to visualize running processes.
	Copyright (C) 2021, Thilo Graf 'dbt'

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


#ifndef __HOURGLASS_H__
#define __HOURGLASS_H__

#include <gui/components/cc.h>
#include <gui/components/cc_timer.h>

//! CHourglass shows a small image chain with timed order to visualize running processes or breaks.
/*!
 CHourglass shows only a timer controlled image chain, CHourglassProc will run an additional process.
 */

class CHourGlass : public CComponentsShapeSquare
{
	private:
		std::string 	  	hg_image_basename;
		std::vector<std::string> hg_img_files;
		int64_t		 	hg_interval;
		CComponentsTimer* 	hg_timer;
		size_t			hg_file_num;

		void initImageFiles();
	public:
		/**CHourGlass Constructor
		 * @param[in]	x_pos
		 * 	@li 	expects type int, x position on screen
		 * @param[in]	y_pos
		 * 	@li 	expects type int, y position on screen
		 * @param[in]	w
		 * 	@li 	optional: expects type int, defines box width, default value = 48
		 * @param[in]	h
		 * 	@li 	optional: expects type int, defines box height, default value = 48
		 * @param[in]	image_basename
		 * 	@li 	optional: expects type std::string, basename as name template for the image set, default = "hourglass"
		 * 		The images should have the png, format recommended dimensions are 48x48px, respectively the same values for width and height (see: arguments w and h),
		 * 		Different dimensions than arguments cause scaling.
		 * 		Filenames must have a digit suffix e.g:  <image_basename>0.png ... <image_basename>3.png
		 * 		The max count of images are defined in MAX_IMAGES.
		 * 		Images must be stored as usual inside the defined icon directories.
		 * @param[in]	interval
		 * 	@li 	optional: expects type int64_t, defines paint interval, default = 84ms
		 * @param[in]	parent
		 * 	@li 	optional: expects type CComponentsForm, as a parent container in which this object is embedded, default = NULL (stand alone)
		 * @param[in]	shadow_mode
		 * 	@li 	optional: expects type int, defines the shadow mode of object, default = CC_SHADOW_OFF (no shadow)
		 * @param[in]	color_frame
		 * 	@li 	optional: expects type int, defines the frame color of object, default = COL_FRAME_PLUS_0
		 * @param[in]	color_body
		 * 	@li 	optional: expects type int, defines the body color of object, default = COL_MENUCONTENT_PLUS_0
		 * 		NOTE: body background is already defined with the image_basename, pure body will only paint if no image is available.
		 * @param[in]	color_shadow
		 * 	@li 	optional: expects type int, defines the shadow color of object, default = COL_SHADOW_PLUS_0

		 * 	@see	class CComponentsShapeSquare()
		 */
		CHourGlass(	const int x_pos,
				const int y_pos,
				const int w = 48,
				const int h = 48,
				const std::string& image_basename = "hourglass",
				const int64_t& interval = 84,
				CComponentsForm *parent = NULL,
				int shadow_mode = CC_SHADOW_OFF,
				fb_pixel_t color_frame = COL_FRAME_PLUS_0,
				fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
				fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		virtual ~CHourGlass();

		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);
};

class CHourGlassProc : public CHourGlass
{
	private:
		sigc::signal<void> OnRun;

	public:
		/**CHourGlassProc Constructor
		 * 	@see    CHourGlass() for other parameters
		 * 	@li 	CHourGlassProc() have only one additional parameter, see as follows:
		 * @param[in]	Slot
		 * 	@li 	expects sigc::slot<void>
		 * 	@li 	example:
		 * 	@li 	sigc::slot<void> sl = sigc::mem_fun(g_Plugins, &CPlugins::loadPlugins);\n
		 * 		CHourGlassProc proc(20, 20, sl);
		 * 		int ret = proc.exec();
		 * 	@li 	or use a function with parameter(s):
		 * 		sigc::slot<void> sl = sigc::bind(sigc::mem_fun(*this, &CMyClass::foo), arg1, arg2, arg3, arg4);\n
		 * 		CHourGlassProc proc(20, 20, sl);
		 * 		int ret = proc.exec();
		 * 	@see	CTestMenu for more examples.
		 *
		 * 	@see	class CComponentsShapeSquare(), CHourGlass()
		 */
		CHourGlassProc(	const int x_pos,
				const int y_pos,
				const sigc::slot<void> &Slot,
				const int w = 48,
				const int h = 48,
				const std::string& image_basename = "hourglass",
				const int64_t& interval = 84,
				CComponentsForm *parent = NULL,
				int shadow_mode = CC_SHADOW_OFF,
				fb_pixel_t color_frame = COL_FRAME_PLUS_0,
				fb_pixel_t color_body = COL_MENUCONTENT_PLUS_0,
				fb_pixel_t color_shadow = COL_SHADOW_PLUS_0);

		virtual ~CHourGlassProc(){};

		void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES);

		/// paint and execute process at once, return value = messages_return::handled
		int exec();
};

#endif //__HOURGLASS_H__
