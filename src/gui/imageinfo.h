/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implementation of component classes
	Copyright (C) 2013, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifndef __imageinfo__
#define __imageinfo__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/menue.h>
#include <gui/components/cc_frm.h>
#include <gui/components/cc_item_tvpic.h>
#include <configfile.h>

typedef struct image_info_t
{
	neutrino_locale_t caption;
	std::string info_text;
	
} image_info_struct_t;

class CImageInfo : public CMenuTarget
{
	private:
		int item_offset; //distance between items and to boarder
		int item_top; //start line in y below header
		std::string license_txt;

		std::vector<image_info_t> v_info;
		
		void Init();
		void InitMinitv();
		void InitInfos();
		void InitLicenseText();
		void ShowWindow();
		void ScrollLic(bool scrollDown);
		
		CComponentsWindow  	*cc_win;
		CComponentsPIP		*cc_tv;
		CComponentsInfoBox 	*cc_lic;
		CConfigFile     	config;

	public:

		CImageInfo();
		~CImageInfo();

		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};

#endif

