/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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


#ifndef __epgview__
#define __epgview__

#include <driver/rcinput.h>
#include <system/settings.h>

#include <driver/movieinfo.h>
#include "widget/menue.h"

#include <vector>
#include <string>

class CFrameBuffer;
class CEpgData
{
	private:
		CFrameBuffer		*frameBuffer;
		CChannelEventList	evtlist;
		CChannelEventList	followlist;
		CEPGData		epgData;
		CComponentsHeader	*header;
		CComponentsFrmChain 	*Bottombox;
		CComponentsPictureScalable *lpic, *rpic;
		CComponentsText 	*lText, *rText;
		CProgressBar 		*pb;
		Font			*font_title;
		std::string 		epg_date;
		std::string 		epg_start;
		std::string 		epg_end;
		int			epg_done;
		bool			bigFonts;
		bool 			has_follow_screenings;
		bool 			call_fromfollowlist;
		bool			tmdb_active;
		int			stars;
		time_t			tmp_curent_zeit;

		uint64_t		prev_id;
		time_t			prev_zeit;
		uint64_t		next_id;
		time_t 			next_zeit;

		int			ox, oy, sx, sy, toph, sb;
		int			emptyLineCount, info1_lines;
		int         		textCount;
		typedef std::pair<std::string,int> epg_pair;
		std::vector<epg_pair> epgText;
		std::vector<epg_pair> epgText_saved;
		std::string epgTextSwitch;
		std::string extMovieInfo;
		int			topheight,topboxheight;
		int			buttonheight,botboxheight;
		int			medlineheight,medlinecount;

		MI_MOVIE_INFO *mp_movie_info;

		void GetEPGData(const t_channel_id channel_id, uint64_t id, time_t* startzeit, bool clear = true );
		void GetPrevNextEPGData( uint64_t id, time_t* startzeit );
		void addTextToArray( const std::string & text, int screening );
		void processTextToArray(std::string text, int screening = 0, bool has_cover = false);
		void showText(int startPos, int ypos, bool has_cover = false, bool fullClear = true);
		bool hasFollowScreenings(const t_channel_id channel_id, const std::string & title);
		int FollowScreenings(const t_channel_id channel_id, const std::string & title);
		void showTimerEventBar(bool show, bool adzap = false, bool mp_info = false);
		void showProgressBar();
		bool isCurrentEPG(const t_channel_id channel_id);

	public:

		CEpgData();
		~CEpgData();
		void start( );
		int show(const t_channel_id channel_id, uint64_t id = 0, time_t* startzeit = NULL, bool doLoop = true, bool callFromfollowlist = false, bool mp_info = false );
		int show_mp(MI_MOVIE_INFO *mi, int mp_position = 0, int mp_duration = 0, bool doLoop = true);
		void hide();
		void ResetModules();
};



class CEPGDataHandler : public CMenuTarget
{
	public:
		int  exec( CMenuTarget* parent,  const std::string &actionkey);

};



#endif

