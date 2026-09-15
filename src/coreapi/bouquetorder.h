/*
 * bouquetorder.h - bouquet positions, and the moves that reorder them
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

#ifndef __coreapi_bouquetorder_h__
#define __coreapi_bouquetorder_h__

#include "coreapi/base/types.h"

#include <stddef.h>

#include <vector>

namespace coreapi
{

/* One channel taken out of a bouquet at one place and put back in at another.
   Both are positions in the list as it stands at the moment that move is made,
   counted from nought, and every move after it reads the list the one before
   left behind. That is the shape the channel stack's own move takes, and it is
   why these are a sequence and not a set. */
struct ChannelMove
{
	size_t from;
	size_t to;
};

typedef std::vector<ChannelMove> ChannelMoveList;

/* The moves that turn the order one half of a bouquet holds now into the order
   that was asked for. Here rather than beside the call that makes them, because
   this is the whole of that work that can be exercised without a box.

   Neither list has to be the other's contents. A channel want names that have
   does not hold is passed over, because a move against a position nothing is at
   would put some other channel where this one was meant to go. A channel have
   holds that want does not name is left where the moves push it, behind
   everything want does name and in the order it was in: taking it out is the
   call before this one, and ordering is not the place to decide it stays or
   goes.

   Nothing is moved that is already where it belongs, so a list handed back the
   way it came produces no moves at all. */
void channelOrderMoves(const ChannelIdList &have, const ChannelIdList &want,
                       ChannelMoveList &out);

} // namespace coreapi

#endif
