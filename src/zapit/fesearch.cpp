/*
 * fesearch.cpp - the frontend a stream may take, chosen without sharing one
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
 */

#include "fesearch.h"

#include <algorithm>
#include <cstddef>

namespace fesearch
{

namespace
{

bool valid(const std::vector<FeView> &fes, int i)
{
	return i >= 0 && (size_t) i < fes.size();
}

/* The members of one master: the first free one that is no twin, else the
   first free twin, else none. */
int pickOnMaster(const std::vector<FeView> &fes, const FeView &master)
{
	int free_frontend = -1;
	int free_twin = -1;
	bool loop_busy = false;

	for (size_t k = 0; k < master.links.size(); ++k)
	{
		const int i = master.links[k];
		if (!valid(fes, i))
			continue;
		const FeView &fe = fes[i];

		if (fe.locked)
		{
			/* Turning the dish would take the channel away from the member
			   that holds it, so no member of this master is free. */
			if (master.have_rotor && !fe.same_pos)
				return -1;
			/* A loop-through member shares the cable with the master: one
			   that holds a band or polarisation the channel would change
			   leaves only the twins free. */
			if (!loop_busy && !fe.is_twin && master.have_loop && !fe.loop_can_tune)
			{
				free_frontend = -1;
				loop_busy = true;
			}
			continue;
		}

		if (fe.is_twin)
		{
			if (free_twin < 0)
				free_twin = i;
		}
		else if (!loop_busy && free_frontend < 0)
			free_frontend = i;
	}

	return (free_frontend >= 0) ? free_frontend : free_twin;
}

} // namespace

int pickIdle(const std::vector<FeView> &fes)
{
	for (size_t i = 0; i < fes.size(); ++i)
	{
		const FeView &m = fes[i];
		if (!m.usable)
			continue;

		if (m.mode == FeView::Master)
		{
			const int got = pickOnMaster(fes, m);
			if (got >= 0)
				return got;
		}
		else if (m.mode == FeView::Independent)
		{
			if (!m.locked)
				return (int) i;
		}
	}
	return -1;
}

Binding binding(int fenumber)
{
	Binding b;
	b.demux = fenumber + 1;
	b.source = fenumber;
	return b;
}

bool keptBy(int usecount, int streams, bool holds_lock)
{
	return usecount > (holds_lock ? streams : 0);
}

namespace
{

/* How a frontend stands for a recording: free, used by streams alone, or
   used by something a recording does not take away. With live_locked the
   picture's frontend is locked, as the recording's first try locks it. */
enum Hold
{
	Free,
	StreamsOnly,
	Fixed
};

const Use &useOf(const std::vector<Use> &use, int i)
{
	static const Use none;
	return (size_t) i < use.size() ? use[i] : none;
}

Hold holdOf(const std::vector<FeView> &fes, const std::vector<Use> &use, int i, bool live_locked)
{
	const Use &u = useOf(use, i);
	const bool live = live_locked && u.live;
	if (!fes[i].locked && !live)
		return Free;
	if (!live && u.streams > 0 && !u.kept)
		return StreamsOnly;
	return Fixed;
}

bool shared(const Use &u, bool no_same_fe)
{
	return u.on_tp && (!no_same_fe || u.same_service);
}

/* Whether getFrontend takes a member of this master as the frontends are:
   a busy one it shares, or a free one. */
bool masterOffers(const std::vector<FeView> &fes, const std::vector<Use> &use, const FeView &master,
		  bool live_locked, bool no_same_fe)
{
	bool free_frontend = false;
	bool free_twin = false;
	bool loop_busy = false;

	for (size_t k = 0; k < master.links.size(); ++k)
	{
		const int i = master.links[k];
		if (!valid(fes, i))
			continue;
		const FeView &fe = fes[i];

		if (holdOf(fes, use, i, live_locked) != Free)
		{
			if (master.have_rotor && !fe.same_pos)
				return false;
			if (shared(useOf(use, i), no_same_fe))
				return true;
			if (!loop_busy && !fe.is_twin && master.have_loop && !fe.loop_can_tune)
			{
				free_frontend = false;
				loop_busy = true;
			}
			continue;
		}

		if (fe.is_twin)
			free_twin = true;
		else if (!loop_busy)
			free_frontend = true;
	}
	return free_frontend || free_twin;
}

bool foundAsItIs(const std::vector<FeView> &fes, const std::vector<Use> &use, bool live_kept, bool no_same_fe)
{
	// getFrontend shares the live frontend before it walks.
	for (size_t i = 0; i < fes.size(); ++i)
	{
		const Use &u = useOf(use, (int) i);
		if (live_kept && u.live && shared(u, no_same_fe))
			return true;
	}

	for (size_t i = 0; i < fes.size(); ++i)
	{
		const FeView &m = fes[i];
		if (!m.usable)
			continue;
		if (m.mode == FeView::Master)
		{
			if (masterOffers(fes, use, m, live_kept, no_same_fe))
				return true;
		}
		else if (m.mode == FeView::Independent)
		{
			if (holdOf(fes, use, (int) i, live_kept) == Free || shared(useOf(use, (int) i), no_same_fe))
				return true;
		}
	}
	return false;
}

/* The frontends whose streams have to end for the walk to take target on
   master (an independent frontend is its own master, without links), or
   false when the target or a member in its way is used by more than
   streams. */
bool endFor(const std::vector<FeView> &fes, const std::vector<Use> &use, const FeView &master, int target,
	    bool live_locked, std::vector<int> &end)
{
	end.clear();
	const Hold own = holdOf(fes, use, target, live_locked);
	if (own == Fixed)
		return false;
	if (own == StreamsOnly)
		end.push_back(fes[target].number);

	if (master.mode != FeView::Master)
		return true;

	for (size_t k = 0; k < master.links.size(); ++k)
	{
		const int i = master.links[k];
		if (!valid(fes, i) || i == target)
			continue;
		const Hold h = holdOf(fes, use, i, live_locked);
		if (h == Free)
			continue;
		const FeView &fe = fes[i];
		const bool rotor = master.have_rotor && !fe.same_pos;
		const bool loop = master.have_loop && !fes[target].is_twin && !fe.is_twin && !fe.loop_can_tune;
		if (!rotor && !loop)
			continue;
		if (h == Fixed)
			return false;
		end.push_back(fe.number);
	}
	return true;
}

// A later candidate wins only with fewer frontends to end.
void keepFewer(const std::vector<int> &end, bool &found, std::vector<int> &best)
{
	if (!found || end.size() < best.size())
	{
		best = end;
		found = true;
	}
}

/* The candidate that ends streams on the fewest frontends, the first the
   walk meets among equals; false when there is none. */
bool fewestToEnd(const std::vector<FeView> &fes, const std::vector<Use> &use, bool live_locked,
		 std::vector<int> &best)
{
	bool found = false;
	std::vector<int> end;

	for (size_t i = 0; i < fes.size(); ++i)
	{
		const FeView &m = fes[i];
		if (!m.usable)
			continue;
		if (m.mode == FeView::Master)
		{
			for (size_t k = 0; k < m.links.size(); ++k)
			{
				const int t = m.links[k];
				if (valid(fes, t) && endFor(fes, use, m, t, live_locked, end))
					keepFewer(end, found, best);
			}
		}
		else if (m.mode == FeView::Independent)
		{
			if (endFor(fes, use, m, (int) i, live_locked, end))
				keepFewer(end, found, best);
		}
	}
	return found;
}

} // namespace

StreamsToEnd streamsToEnd(const std::vector<FeView> &fes, const std::vector<Use> &use,
			  bool live_kept, bool no_same_fe)
{
	StreamsToEnd r;
	if (foundAsItIs(fes, use, live_kept, no_same_fe))
		r.stage = 1;
	else if (fewestToEnd(fes, use, live_kept, r.end))
		r.stage = 2;
	else if (live_kept && fewestToEnd(fes, use, false, r.end))
		r.stage = 3;
	else
		r.end.clear();
	std::sort(r.end.begin(), r.end.end());
	return r;
}

} // namespace fesearch
