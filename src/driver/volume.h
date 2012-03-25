/*
	volume bar - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2011-2012 M. Liebmann (micha-bbg)

	License: GPL

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU Library General Public
	License along with this library; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CVOLUME__
#define __CVOLUME__

#define ROUNDED g_settings.rounded_corners ? vbar_h/2 : 0

class CVolume
{
	private:
		void refreshVolumebar(int current_volume);

		int x, y, sy, sw, sh;
		int mute_ax, mute_ay, mute_dy, mute_ay_old;
		int mute_icon_dx, mute_icon_dy;
		int icon_w, icon_h, icon_x, icon_y;
		int vbar_w, vbar_h;
		int progress_w, progress_h, progress_x, progress_y, pB;
		int digit_w, digit_h, digit_offset, digit_x, digit_y, digit_b_x, digit_b_w;
		int VolumeFont, colShadow, colBar, colFrame, colContent;
		int ShadowOffset;
		int rounded;
		int m_mode;
		bool paintShadow, paintDigits, MuteIconFrame;

	public:
		CVolume();
		~CVolume();
		static CVolume* getInstance();

		int spacer, mute_dx;
		void Init();
		void AudioMute(int newValue, bool isEvent= false);
		void setvol(int vol);
		void setVolume(const neutrino_msg_t key, const bool bDoPaint = true, bool nowait = false);
		int getStartPosTop(){ return sy; }
		int getEndPosRight(){ return sw; }
};


#endif // __CVOLUME__
