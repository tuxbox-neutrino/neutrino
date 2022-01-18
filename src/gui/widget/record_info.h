/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Show record infos.
	Copyright (C) 2022, Thilo Graf 'dbt'

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


#ifndef __REC_INFO_H__
#define __REC_INFO_H__

#include <gui/components/cc.h>

class CRecInfo : public CComponentsForm
{
	private:
		CComponentsPicture *rv_rec_img, *rv_ts_img;
		CComponentsTextTransp *rv_text;
		int rv_dx_cached;
		void init();

	public:
		CRecInfo(	const int &x_pos,
				const int &y_pos,
				const int &w = 0,
				const int &h = 0,
				CComponentsForm *parent = NULL,
				const int &shadow_mode = CC_SHADOW_OFF,
				const fb_pixel_t &color_frame = COL_FRAME_PLUS_0,
				const fb_pixel_t &color_body = COL_MENUCONTENT_PLUS_0,
				const fb_pixel_t &color_shadow = COL_SHADOW_PLUS_0);

		virtual ~CRecInfo();
};
#endif
