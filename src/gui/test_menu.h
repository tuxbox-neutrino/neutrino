/*
	test menu - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011 T. Graf 'dbt'
	Homepage: http://www.dbox2-tuning.net/

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __TEST_MENU__
#define __TEST_MENU__

#include <gui/widget/menue.h>
#include <gui/widget/icons.h>
#include <gui/components/cc.h>
#include <gui/components/cc_frm.h>
#include <gui/components/cc_frm_button.h>
#include <gui/components/cc_frm_clock.h>
#include <gui/components/cc_item_shapes.h>
// #define TEST_MENU

#include <string>

class CTestMenu : public CMenuTarget
{
	private:
		CComponentsShapeCircle * circle;
		CComponentsShapeSquare* sq;
		CComponentsPicture* pic;
		CComponentsForm *form;
		CComponentsText *txt;
		CComponentsHeader *header;
		CComponentsHeader *footer;
		CComponentsIconForm *iconform;
		CComponentsWindow *window;
		CComponentsButton *button;
		CComponentsFrmClock *clock ,*clock_r;
		int width, selected;

		int showTestMenu();
		void showHWTests(CMenuWidget *widget);
		void showCCTests(CMenuWidget *widget);

	public:	
		CTestMenu();
		~CTestMenu();
		int exec(CMenuTarget* parent, const std::string & actionKey);
};


#endif
