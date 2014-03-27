/*
        Copyright (C) 2013 CoolStream International Ltd

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __SCAN_EPG__
#define __SCAN_EPG__

#include <zapit/zapit.h>

typedef std::map<transponder_id_t, t_channel_id> eit_scanmap_t;
typedef std::pair<transponder_id_t, t_channel_id> eit_scanmap_pair_t;
typedef eit_scanmap_t::iterator eit_scanmap_iterator_t;

class CEpgScan
{
	public:
		enum {
			SCAN_OFF,
			SCAN_CURRENT,
			SCAN_FAV,
			SCAN_SEL
		};
		enum {
			MODE_LIVE = 0x1,
			MODE_STANDBY = 0x2,
			MODE_ALWAYS = 0x3
		};
	private:
		int current_bnum;
		int current_mode;
		int current_bmode;
		bool allfav_done;
		bool selected_done;
		bool standby;
		eit_scanmap_t scanmap;
		t_channel_id next_chid;
		t_channel_id live_channel_id;
		std::set<transponder_id_t> scanned;
		uint32_t rescan_timer;
		bool scan_in_progress;

		void AddBouquet(CChannelList * clist);
		bool AddFavorites();
		bool AddSelected();
		void AddTransponders();
		void EnterStandby();
		bool CheckMode();
		void AddTimer();

		CEpgScan();
	public:
		~CEpgScan();
		static CEpgScan * getInstance();

		int handleMsg(const neutrino_msg_t _msg, neutrino_msg_data_t data);
		void Next();
		void Clear();
		void Start(bool instandby = false);
		void Stop();
		bool Running();
};

#endif
