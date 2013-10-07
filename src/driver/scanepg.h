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
	private:
		int current_bnum;
		int current_mode;
		bool allfav_done;
		eit_scanmap_t scanmap;
		t_channel_id next_chid;
		std::set<transponder_id_t> scanned;
		void Next();
		void AddBouquet(CChannelList * clist);
		bool AddFavorites();

		CEpgScan();
	public:
		~CEpgScan();
		static CEpgScan * getInstance();

		void handleMsg(const neutrino_msg_t _msg, neutrino_msg_data_t data);
		void Clear();
};

#endif
