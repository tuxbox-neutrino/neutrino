/*
 * fesearch.h - the frontend a stream may take, chosen without sharing one
 *
 * Copyright (C) 2011 CoolStream International Ltd
 * Copyright (C) 2012-2016 Stefan Seyfried
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * The walk CFEManager::getFrontend makes over the frontends, as plain data
 * and without a frontend object, so that it can be run where no tuner is.
 * It differs from that walk in what it may hand back: only a frontend nobody
 * uses. getFrontend also returns a busy frontend that already sits on the
 * channel's transponder, and the live frontend when it does; a stream given
 * such a frontend shares it, and the next zap of whoever holds it retunes the
 * stream away. A change to the walk in getFrontend has to be checked against
 * this one, and against streamsToEnd, which walks the same way for a
 * recording that found no frontend and names the streams whose frontends it
 * may have instead.
 */

#ifndef __zapit_fesearch_h__
#define __zapit_fesearch_h__

#include <vector>

namespace fesearch
{

/* One frontend as the walk sees it. The adapter in CFEManager fills these in
   the order of its frontend map. */
struct FeView
{
	enum Mode
	{
		Other,        // unused, or linked: reached through its master only
		Independent,
		Master
	};

	int  number;
	Mode mode;
	/* For a master or an independent frontend: it offers the channel's
	   delivery system, the user did not force it to another one, and a
	   satellite frontend has the channel's position configured. */
	bool usable;
	/* In use (a use count above zero), or held for something that must not
	   lose it: the picture, a PiP slot, a running stream. */
	bool locked;
	// The position its dish points at is the channel's.
	bool same_pos;
	/* Its band and polarisation let a loop-through partner tune the channel,
	   or it is not tuned at all. */
	bool loop_can_tune;
	bool is_twin;
	/* For a master: the indices of itself and of every frontend linked to it,
	   master first, in the order the walk visits them. */
	std::vector<int> links;
	bool have_loop;
	bool have_rotor;

	FeView()
		: number(-1), mode(Other), usable(false), locked(false), same_pos(false),
		  loop_can_tune(true), is_twin(false), have_loop(false), have_rotor(false)
	{
	}
};

/* The index of the frontend a stream of the channel may take, or -1 when no
   frontend is free for it. A frontend that is locked is never chosen. A
   master with a rotor that a locked member keeps on another position offers
   none of its members, and on a loop-through master a locked member whose
   band or polarisation the channel would change keeps the others free only
   as twins. A free frontend comes before a free twin, and the first master or
   independent frontend in the order given that offers one wins. */
int pickIdle(const std::vector<FeView> &fes);

/* Where a stream on frontend number fe reads from without dynamic demux:
   demux fe + 1, with fe as its source. Demux 0 is the picture's. */
struct Binding
{
	int demux;
	int source;
};

Binding binding(int fenumber);

/* What uses one frontend, beside its view, as a recording of a channel
   sees it. The adapter in CFEManager fills one per view, in the same
   order. */
struct Use
{
	int  streams;       // stream holds the register has on it
	/* Used for more than its streams: by a recording, a zap that locked
	   it, or a PiP that shows from it. */
	bool kept;
	bool live;          // the picture runs from it
	bool on_tp;         // tuned to the channel's transponder
	bool same_service;  // carrying the channel itself

	Use() : streams(0), kept(false), live(false), on_tp(false), same_service(false)
	{
	}
};

/* Whether a frontend with this use count is used for more than its stream
   holds. Where a hold locks its frontend each hold is one of the count;
   where it does not, every use is something else. */
bool keptBy(int usecount, int streams, bool holds_lock);

struct StreamsToEnd
{
	/* 1: getFrontend finds a frontend as it is: a free one, or a busy one
	      on the channel's transponder it may share;
	   2: it finds one once the streams on the frontends in end are ended,
	      the picture's frontend kept;
	   3: it finds one once the picture's frontend is given up as well,
	      with the streams on it;
	   0: none, whatever streams end. */
	int stage;
	std::vector<int> end;   // frontend numbers, ascending

	StreamsToEnd() : stage(0)
	{
	}
};

/* The streams a recording of the channel has to end to get a frontend.
   With live_kept the picture runs: its frontend counts as locked, as the
   recording locks it for its first try, its streams are not ended at stage
   2, and stage 3 gives it up. Without, no frontend is the picture's.

   Only a frontend used by nothing but streams counts as one whose streams
   may end. A candidate is a frontend the walk could take once some streams
   end: besides itself, every busy member its master's rotor would have to
   turn away from, and on a loop-through master every busy member, no twin,
   whose band or polarisation the channel would change - unless the
   candidate is a twin itself.
   A candidate that such a member blocks and whose streams may not end is
   out. Of the candidates, the one that ends streams on the fewest
   frontends wins, and of those the first the walk meets. With no_same_fe a
   busy frontend is shared only with its own service. */
StreamsToEnd streamsToEnd(const std::vector<FeView> &fes, const std::vector<Use> &use,
			  bool live_kept, bool no_same_fe);

} // namespace fesearch

#endif
