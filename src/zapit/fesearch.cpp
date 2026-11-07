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

/* Whether a locked link on an FBC root holds a band or polarisation the
   channel would change: the root tunes the input the link tunes from. */
bool linksBlock(const std::vector<FeView> &fes, const FeView &root)
{
	for (size_t k = 0; k < root.fbc_links.size(); ++k)
	{
		const int j = root.fbc_links[k];
		if (valid(fes, j) && fes[j].locked && !fes[j].loop_can_tune)
			return true;
	}
	return false;
}

/* The members of the master at index m: the first free one that is no twin,
   else the first free twin, else none. When the first free one is the master
   itself and a link on it blocks, only a twin is left. */
int pickOnMaster(const std::vector<FeView> &fes, int m)
{
	const FeView &master = fes[m];
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

	if (free_frontend == m && linksBlock(fes, master))
		free_frontend = -1;
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
			const int got = pickOnMaster(fes, (int) i);
			if (got >= 0)
				return got;
		}
		else if (m.mode == FeView::Independent)
		{
			if (!m.locked && m.root_carries && !linksBlock(fes, m))
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

/* linksBlock as a recording sees it: a link is held when it is not Free. */
bool linksBlockFor(const std::vector<FeView> &fes, const std::vector<Use> &use, const FeView &root,
		   bool live_locked)
{
	for (size_t k = 0; k < root.fbc_links.size(); ++k)
	{
		const int j = root.fbc_links[k];
		if (valid(fes, j) && !fes[j].loop_can_tune && holdOf(fes, use, j, live_locked) != Free)
			return true;
	}
	return false;
}

/* Whether getFrontend takes a member of the master at index m as the
   frontends are: a busy one it shares, or a free one. */
bool masterOffers(const std::vector<FeView> &fes, const std::vector<Use> &use, int m,
		  bool live_locked, bool no_same_fe)
{
	const FeView &master = fes[m];
	int free_frontend = -1;
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
				free_frontend = -1;
				loop_busy = true;
			}
			continue;
		}

		if (fe.is_twin)
			free_twin = true;
		else if (!loop_busy && free_frontend < 0)
			free_frontend = i;
	}
	if (free_frontend == m && linksBlockFor(fes, use, master, live_locked))
		free_frontend = -1;
	return free_frontend >= 0 || free_twin;
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
			if (masterOffers(fes, use, (int) i, live_kept, no_same_fe))
				return true;
		}
		else if (m.mode == FeView::Independent)
		{
			if ((holdOf(fes, use, (int) i, live_kept) == Free && m.root_carries &&
			     !linksBlockFor(fes, use, m, live_kept)) ||
			    shared(useOf(use, (int) i), no_same_fe))
				return true;
		}
	}
	return false;
}

/* Each frontend once, by its index, ascending. A frontend may be named
   twice: a loop member tied to its master is one of the master's links and
   one of its FBC links, and a candidate is weighed by how many frontends it
   ends. A number is no such identity, as it is unique only within its
   adapter. */
void normalize(std::vector<int> &ends)
{
	std::sort(ends.begin(), ends.end());
	ends.erase(std::unique(ends.begin(), ends.end()), ends.end());
}

/* For endFor: each FBC link tied to root, target aside, that holds a band or
   polarisation the channel would change goes into ends, as the root tunes the
   input it tunes from; false when one of them is used by more than streams. */
bool fbcLinksEnd(const std::vector<FeView> &fes, const std::vector<Use> &use, const FeView &root, int target,
		 bool live_locked, std::vector<int> &ends)
{
	for (size_t k = 0; k < root.fbc_links.size(); ++k)
	{
		const int j = root.fbc_links[k];
		if (!valid(fes, j) || j == target || fes[j].loop_can_tune)
			continue;
		const Hold h = holdOf(fes, use, j, live_locked);
		if (h == Free)
			continue;
		if (h == Fixed)
			return false;
		ends.push_back(j);
	}
	return true;
}

/* The numbers of the frontends whose streams have to end for the walk to
   take target on the master at index m (an independent frontend is its own
   master, without links), or false when the target or a frontend in its way
   is used by more than streams. In its way are a member the master's rotor
   would turn away from, and on a loop-through master a member that is no twin
   and holds another band or polarisation. The FBC links tied to the root
   count where the walk weighs them: for an independent target, which is its
   own root, and for a target on a master that is free once the streams end,
   as the walk then takes the master first; a member of a master that stays
   held is taken without them, and a twin keeps its own control. An
   independent target no root carries the channel's input for is out. */
bool endFor(const std::vector<FeView> &fes, const std::vector<Use> &use, int m, int target,
	    bool live_locked, std::vector<int> &end)
{
	const FeView &master = fes[m];
	std::vector<int> ends;  // indices
	end.clear();
	const Hold own = holdOf(fes, use, target, live_locked);
	if (own == Fixed)
		return false;
	if (own == StreamsOnly)
		ends.push_back(target);

	if (master.mode != FeView::Master)
	{
		if (!fes[target].root_carries || !fbcLinksEnd(fes, use, master, target, live_locked, ends))
			return false;
	}
	else
	{
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
			ends.push_back(i);
		}

		const bool master_freed = holdOf(fes, use, m, live_locked) == Free ||
					  std::find(ends.begin(), ends.end(), m) != ends.end();
		if (!fes[target].is_twin && master_freed && !fbcLinksEnd(fes, use, master, target, live_locked, ends))
			return false;
	}

	normalize(ends);
	for (size_t k = 0; k < ends.size(); ++k)
		end.push_back(fes[ends[k]].number);
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
				if (valid(fes, t) && endFor(fes, use, (int) i, t, live_locked, end))
					keepFewer(end, found, best);
			}
		}
		else if (m.mode == FeView::Independent)
		{
			if (endFor(fes, use, (int) i, (int) i, live_locked, end))
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

bool raiseKept(const std::vector<int *> &kept)
{
	for (size_t i = 0; i < kept.size(); ++i)
		++*kept[i];
	return !kept.empty();
}

bool releaseKept(const std::vector<int *> &kept, const std::vector<const int *> &all)
{
	for (size_t i = 0; i < kept.size(); ++i)
		if (*kept[i] > 0)
			--*kept[i];
	for (size_t i = 0; i < all.size(); ++i)
		if (*all[i] != 0)
			return true;
	return false;
}

bool turnsDish(bool have_rotor, bool fe_usals, bool sat_listed, int motor_position, bool sat_usals)
{
	if (have_rotor || fe_usals)
		return true;
	return sat_listed && (motor_position != 0 || sat_usals);
}

} // namespace fesearch
