/*
 * bouquetorder.cpp - bouquet positions, and the moves that reorder them
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "bouquetorder.h"

#include <stddef.h>

namespace coreapi
{

namespace
{

/* Where that channel sits at or after the position given, and the size of the
   list for one it does not hold. Searched forward from the place being filled
   rather than from the front, because everything before it is already where it
   was asked to be: a channel named twice would otherwise be found at its settled
   place and moved away from it. */
size_t at(const ChannelIdList &list, size_t from, ChannelId id)
{
	for (size_t i = from; i < list.size(); i++)
		if (list[i] == id)
			return i;
	return list.size();
}

} // namespace

void channelOrderMoves(const ChannelIdList &have, const ChannelIdList &want,
                       ChannelMoveList &out)
{
	out.clear();

	// The list as each move leaves it, so that the next one is chosen against
	// what the channel stack will be holding by the time it is made and not
	// against what it holds now.
	ChannelIdList as_it_goes = have;

	size_t next = 0;
	for (size_t i = 0; i < want.size() && next < as_it_goes.size(); i++)
	{
		const size_t found = at(as_it_goes, next, want[i]);
		if (found >= as_it_goes.size())
			continue;

		if (found != next)
		{
			ChannelMove m;
			m.from = found;
			m.to = next;
			out.push_back(m);

			const ChannelId moved = as_it_goes[found];
			as_it_goes.erase(as_it_goes.begin() + (ptrdiff_t) found);
			as_it_goes.insert(as_it_goes.begin() + (ptrdiff_t) next, moved);
		}
		next++;
	}
}

} // namespace coreapi
