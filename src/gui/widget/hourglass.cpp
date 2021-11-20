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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hourglass.h"

#include <global.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <neutrinoMessages.h>

#define MAX_IMAGES 30

CHourGlass::CHourGlass(	const int x_pos,
			const int y_pos,
			const int w,
			const int h,
			const std::string &image_basename,
			const int64_t &interval,
			CComponentsForm *parent,
			int shadow_mode,
			fb_pixel_t color_frame,
			fb_pixel_t color_body,
			fb_pixel_t color_shadow) : CComponentsShapeSquare(x_pos, y_pos, w, h, parent, shadow_mode, color_frame, color_body, color_shadow)
{
	cc_item_type.name = "wg_hourglass";

	hg_image_basename = image_basename;
	cc_bg_image = cc_bg_image_old = cc_bg_sel_image = cc_bg_sec_image = "";

	initImageFiles();

	hg_file_num = 0;
	hg_interval = 0;
	hg_timer = NULL;

	if (!hg_img_files.empty())
	{
		hg_interval = interval == HG_AUTO_PAINT_INTERVAL ? (int64_t)(1000/hg_img_files.size()) : interval;
		hg_timer = new CComponentsTimer(hg_interval);
		hg_timer->OnTimer.connect(sigc::bind(sigc::mem_fun(*this, &CHourGlass::paint), true));
	}
	else
		dprintf(DEBUG_NORMAL, "[CHourGlass] [%s - %d] NOTE: No %s-images found..\n", __func__, __LINE__,  hg_image_basename.c_str());
}

CHourGlass::~CHourGlass()
{
	if(hg_timer)
	{
		delete hg_timer;
		hg_timer = NULL;
	}
}

void CHourGlass::initImageFiles()
{
	std::string path = "";
	std::string filename = "";
	hg_img_files.clear();

	path = frameBuffer->getIconPath(hg_image_basename, "png");
	if (file_exists(path))
		hg_img_files.push_back(hg_image_basename);

	for (int i = 0; i <= MAX_IMAGES; i++)
	{
		filename = hg_image_basename;
		filename += to_string(i);
		path = frameBuffer->getIconPath(filename, "png");
		if (file_exists(path))
			hg_img_files.push_back(filename);
		else
			continue;
	}
}

void CHourGlass::paint(const bool &do_save_bg)
{
	if (hg_img_files.empty())
		return;

	std::lock_guard<std::mutex> g(hg_mutex);

	if (hg_file_num > hg_img_files.size()-1)
		hg_file_num = 0;

	cc_bg_image = frameBuffer->getIconPath(hg_img_files.at(hg_file_num), "png");

	if (do_save_bg)
		hide();
	else
		kill();

	if (!hg_timer->isRun())
		hg_timer->startTimer();
	else 
		paintInit(do_save_bg);

	hg_file_num ++;
}


CHourGlassProc::CHourGlassProc(	const int x_pos,
		const int y_pos,
		const sigc::slot<void> &Slot,
		const int w,
		const int h,
		const std::string &image_basename,
		const int64_t &interval,
		CComponentsForm *parent,
		int shadow_mode,
		fb_pixel_t color_frame,
		fb_pixel_t color_body,
		fb_pixel_t color_shadow) : CHourGlass(x_pos, y_pos, w, h, image_basename, interval, parent, shadow_mode, color_frame, color_body, color_shadow)
{
	cc_item_type.name = "wg_hourglass_proc";

	OnRun.connect(Slot);
}

void CHourGlassProc::paint(const bool &do_save_bg)
{
	CHourGlass::paint(do_save_bg);
}

int CHourGlassProc::exec()
{
	paint(true);
	OnRun();
	hide();

	return messages_return::handled;
}
