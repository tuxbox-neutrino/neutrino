/*
	Neutrino-GUI  -   DBoxII-Project

	TimeOSD by Zwen

	Homepage: http://dbox.cyberphoria.org/

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

#ifndef __timeosd__
#define __timeosd__

#include <gui/components/cc.h>
#include <time.h>

class CTimeOSD : public CComponentsFrmClock
{
	public:
		enum mode
		{
			MODE_ASC,
			MODE_DESC,
			MODE_BAR,
			MODE_HIDE
		};
	
	private:
		CProgressBar		timescale;
		mode m_mode, tmp_mode;
		bool m_restore;
		bool mp_time_forced;
		time_t m_time_show;

		void initTimeString();
		void updatePos(int position, int duration);
		void KillAndResetTimescale();

	public:
		CTimeOSD();
// 		~CTimeOSD(); is inherited
		void Init();
		static CTimeOSD* getInstance();
		void show(time_t time_show, bool force = true);
		void kill();
		bool IsVisible() {return m_mode != MODE_HIDE;}
		void update(int position, int duration);
		void switchMode(int position, int duration);
		mode getMode() { return m_mode; };
		void setMode (mode mode_) { m_mode = mode_; };
		mode getTmpMode() { return tmp_mode; };
		void setTmpMode () { tmp_mode = m_mode; };
		bool getRestore() { return m_restore; };
		void setRestore(bool m) { m_restore = m; };
		void setHeight(const int){}//NOTE: dummy member, height is strictly bound to settings
		bool getMpTimeForced() { return mp_time_forced; };
		void setMpTimeForced(bool m) { mp_time_forced = m; };
};
#endif
