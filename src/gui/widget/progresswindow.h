/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Implementation of CComponent Window class.
	Copyright (C) 2014 Thilo Graf 'dbt'

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


#ifndef __progresswindow__
#define __progresswindow__

#include <driver/framebuffer.h>
#include <system/localize.h>

#include <gui/components/cc.h>
#include "menue.h"

#include <string>

class CProgressWindow : public CComponentsWindow, public CMenuTarget
{
	private:
		CProgressBar *local_bar, *global_bar;
		CComponentsLabel *status_txt;

		unsigned int global_progress;
		unsigned int local_progress;
		int w_bar_frame;

		void Init();

	public:

		CProgressWindow(CComponentsForm *parent = NULL);
		void setTitle(const neutrino_locale_t title);
		virtual void hide(bool no_restore = false);

		virtual int exec( CMenuTarget* parent, const std::string & actionKey );

		virtual void showGlobalStatus(const unsigned int prog);
		virtual unsigned int getGlobalStatus(void);
		virtual void showLocalStatus(const unsigned int prog);
		virtual void showStatusMessageUTF(const std::string & text); // UTF-8
};


#endif
