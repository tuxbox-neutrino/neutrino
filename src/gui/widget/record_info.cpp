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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>

#include "record_info.h"
#include <gui/widget/menue_target.h>

#include <driver/record.h>
#include <daemonc/remotecontrol.h>
#include <driver/fontrenderer.h>
#include <system/helpers.h>


extern CRemoteControl *g_RemoteControl;	/* neutrino.cpp */

CRecInfo::CRecInfo( const int &x_pos,
			const int &y_pos,
			const int &w,
			const int &h,
			CComponentsForm *parent,
			const int &shadow_mode,
			const fb_pixel_t &color_frame,
			const fb_pixel_t &color_body,
			const fb_pixel_t &color_shadow) : CComponentsForm(x_pos, y_pos, w, h, parent, shadow_mode, color_frame, color_body, color_shadow)
{
	cc_item_type.name= "record_view";
	setShadowWidth(OFFSET_SHADOW/2);
	setCorner(RADIUS_MIN, CORNER_ALL);
	rv_dx_cached = w;
	OnBeforePaint.connect(sigc::mem_fun(*this, &CRecInfo::init));
	init();
}

CRecInfo::~CRecInfo()
{

}

void CRecInfo::init()
{
	clear();

	// restore cached preset of width, because if preset of width was changed
	width = rv_dx_cached;

	// init basic objects
	rv_rec_img = NULL;
	rv_ts_img = NULL;

	// init text vars
	std::string rec_icon = "";
	std::string ts_icon = "";
	std::string s_records = "0x";

	CRecordManager *crm = CRecordManager::getInstance();
	bool recordModeActive = crm->RecordingStatus();
	if (recordModeActive)
	{
		rec_icon = NEUTRINO_ICON_REC_GRAY;
		ts_icon = NEUTRINO_ICON_AUTO_SHIFT_GRAY;

		// get current record count
		int records = crm->GetRecordCount();

		// get global record mode
		int rec_mode = crm->GetRecordMode();

		// get current channel id
		t_channel_id cur_chid = g_RemoteControl->current_channel_id;

		// get current channel record mode
		int cur_rec_mode = crm->GetRecordMode(cur_chid);

		// set 'active' icon for record mode
		if (cur_rec_mode & CRecordManager::RECMODE_REC)
			rec_icon = NEUTRINO_ICON_REC;

		// set 'active' icon for timeshift mode
		if (cur_rec_mode & CRecordManager::RECMODE_TSHIFT)
			ts_icon = NEUTRINO_ICON_AUTO_SHIFT;

		if (rec_mode == CRecordManager::RECMODE_REC_TSHIFT)
			records--; // subtract ts

		s_records = to_string(records) + "x";
	}

	// init images: create image objects, set position dimensions and space for icons inside record box (form)
	int w_rec_img = 0;
	int w_ts_img = 0;
	int h_rec_img = 0;
	int h_ts_img = 0;
	int w_icon_space = OFFSET_INNER_MIN;

	if (!rec_icon.empty())
	{
		rv_rec_img = new CComponentsPicture(OFFSET_INNER_MIN, 0, rec_icon, this);
		w_rec_img = rv_rec_img->getWidth();
		h_rec_img = rv_rec_img->getHeight();
		w_icon_space += w_rec_img + OFFSET_INNER_MIN;
	}

	if (!ts_icon.empty())
	{
		rv_ts_img = new CComponentsPicture(w_icon_space, 0, ts_icon, this);
		w_ts_img = rv_ts_img->getWidth();
		h_ts_img = rv_ts_img->getHeight();
		w_icon_space += w_ts_img + OFFSET_INNER_MIN;
	}

	// set font type for record count text
	Font *font_rv = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL];

	// init height of record info box
	int h_min = height;
	if (height == 0)
	{
		int h_img_max = std::max(h_rec_img, h_ts_img);
		h_min = std::max(h_img_max + 2*OFFSET_INNER_MIN, font_rv->getHeight() + 2*OFFSET_INNER_MIN);
		height = std::max(h_min, height);
	}

	// init image positions
	if (rv_rec_img)
		rv_rec_img->setYPos(height/2 - h_rec_img/2);
	if (rv_ts_img)
		rv_ts_img->setYPos(height/2 - h_ts_img/2);

	// init text
	int x_txt = w_icon_space;
	rv_text = new CComponentsTextTransp(this, x_txt, 0, font_rv->getRenderWidth(s_records), height, s_records, CTextBox::RIGHT, font_rv);
	int y_txt = height/2 - rv_text->getHeight()/2;
	rv_text->setPos(x_txt, y_txt);
	rv_text->doPaintBg(false);

	// finally set width of record info box (if different to passed parameters or width == 0
	int w_min = w_icon_space + rv_text->getWidth() + OFFSET_INNER_MIN;
	width = std::max(w_min, width);
}
