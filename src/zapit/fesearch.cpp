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

} // namespace fesearch
