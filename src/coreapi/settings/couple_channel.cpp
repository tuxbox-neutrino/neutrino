/*
 * couple_channel.cpp - the start channel name and identifier
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

#include "couple.h"

namespace coreapi
{
namespace settings
{

/* A start channel is picked once and gives a name and an identifier together. The name
   is what a person reads and the identifier is what the box zaps to, so one written
   without the other describes a channel that is not the one tuned. */
const KeyPair kChannelPairs[] =
{
	{ "startchanneltv", "startchanneltv_id" },
	{ "startchannelradio", "startchannelradio_id" }
};

const size_t kChannelPairCount = sizeof(kChannelPairs) / sizeof(kChannelPairs[0]);

void coupleChannel(CoupledBatch &b)
{
	for (size_t i = 0; i < kChannelPairCount; ++i)
		b.requirePair(kChannelPairs[i].first, kChannelPairs[i].second);
}

} // namespace settings
} // namespace coreapi
