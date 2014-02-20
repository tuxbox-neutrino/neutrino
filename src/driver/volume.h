/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	volume bar - Neutrino-GUI
	Copyright (C) 2011-2013 M. Liebmann (micha-bbg)

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

#ifndef __CVOLUME__
#define __CVOLUME__

#include <driver/framebuffer.h>
#include <gui/volumebar.h>

class CVolume : public CChangeObserver
{
	private:
		CFrameBuffer * frameBuffer;
		CVolumeBar *volscale;

		int mute_ax, mute_ay, mute_dx, mute_dy;
		int m_mode;
		/* volume adjustment variables */
		t_channel_id channel_id;
		int apid;

	public:
		CVolume();
		~CVolume();
		static CVolume* getInstance();

		void setvol(int vol);
		void setVolume(const neutrino_msg_t key);
		void setVolumeExt(int vol);

		void SetCurrentPid(int pid) { apid = pid; }
		void SetCurrentChannel(t_channel_id id) { channel_id = id; }
		bool hideVolscale();
		void showVolscale();
		bool changeNotify(const neutrino_locale_t OptionName, void *);
};

#endif // __CVOLUME__
