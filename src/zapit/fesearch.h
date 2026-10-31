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
 * this one.
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

} // namespace fesearch

#endif
