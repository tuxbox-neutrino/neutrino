/*
	Neutrino graphlcd menue

	Copyright (C) 2012 martii

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef ENABLE_GRAPHLCD

#ifndef __glcdsetup_h__
#define __glcdsetup_h__

#include <sys/types.h>
#include <string.h>
#include <system/setting_helpers.h>
#include <system/settings.h>
#include <driver/glcd/glcd.h>
#include <gui/widget/menue.h>

class GLCD_Menu : public CMenuTarget, public CChangeObserver
{
	private:
		int width;
		SNeutrinoGlcdTheme oldTheme;
		int GLCD_Menu_Select_Driver();
		CMenuForwarder *select_driver;
	public:
		GLCD_Menu();
		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		bool changeNotify(const neutrino_locale_t, void *);
		int GLCD_Menu_Settings();
		int GLCD_Standby_Settings();
		int GLCD_Brightness_Settings();
		int GLCD_Theme_Settings();
		int GLCD_Theme_Position_Settings();
};
#endif // __glcdsetup_h__
#endif // ENABLE_GRAPHLCD
