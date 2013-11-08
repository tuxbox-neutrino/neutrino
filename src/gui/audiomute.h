/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	audioMute - Neutrino-GUI
	Copyright (C) 2013 M. Liebmann (micha-bbg)

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifndef __CAUDIOMUTE__
#define __CAUDIOMUTE__

#include <gui/components/cc.h>

class CAudioMute : public CComponentsPicture
{
	private:
		int y_old;
		bool do_paint_mute_icon;

	public:

		CAudioMute();
// 		~CAudioMute();
		static CAudioMute* getInstance();

		void AudioMute(int newValue, bool isEvent= false);
		void doPaintMuteIcon(bool mode) { do_paint_mute_icon = mode; }
		void enableMuteIcon(bool enable);
};

#endif // __CAUDIOMUTE__
