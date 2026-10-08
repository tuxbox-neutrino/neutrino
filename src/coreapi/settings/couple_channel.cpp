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

#include "coreapi/channels.h"

#include <cerrno>
#include <cstdlib>

namespace coreapi
{
namespace settings
{

/* A start channel is what the box zaps to, its identifier; the name beside it is what a
   person reads and follows from the channel list. So the identifier written alone is
   taken and brings the name of that channel with it, while a name written alone names no
   channel and is refused. Both written together are taken as sent, the way the box's own
   chooser writes them. */
const KeyPair kChannelPairs[] =
{
	{ "startchanneltv", "startchanneltv_id" },
	{ "startchannelradio", "startchannelradio_id" }
};

const size_t kChannelPairCount = sizeof(kChannelPairs) / sizeof(kChannelPairs[0]);

namespace
{

bool idOf(const std::string &text, ChannelId &out)
{
	if (text.empty())
		return false;
	errno = 0;
	char *end = NULL;
	const unsigned long long n = strtoull(text.c_str(), &end, 16);
	if (errno != 0 || *end != '\0')
		return false;
	out = (ChannelId) n;
	return true;
}

/* The name the id stands for, nothing for no channel at all; false with why where there is
   none, or where the channel is of the other kind than the list the pair starts on. */
bool nameOf(ChannelId id, bool radio, std::string &name, Error &why)
{
	if (id == 0)
	{
		name.clear();
		return true;
	}
	Result<ChannelInfo> found = channels::get(id);
	if (!found.ok())
	{
		why = found.error();
		return false;
	}
	const ServiceKind k = found.value().kind;
	const bool is_radio = k == ServiceKind::Radio || k == ServiceKind::WebRadio;
	if (is_radio != radio)
	{
		why = Error(Status::InvalidArgument, ErrorCode::NotAListedValue,
			    std::string("the channel is a ") + (is_radio ? "radio" : "television") +
			    " channel, and this start channel takes a " + (radio ? "radio" : "television") + " one");
		return false;
	}
	name = found.value().name;
	return true;
}

} // anonymous namespace

const char *startChannelKind(const std::string &key)
{
	for (size_t i = 0; i < kChannelPairCount; ++i)
	{
		if (key == kChannelPairs[i].first || key == kChannelPairs[i].second)
			return i == 0 ? "tv" : "radio";
	}
	return NULL;
}

void coupleChannel(CoupledBatch &b)
{
	for (size_t i = 0; i < kChannelPairCount; ++i)
	{
		const char *const name_key = kChannelPairs[i].first;
		const char *const id_key = kChannelPairs[i].second;
		const std::string *id_text = b.written(id_key);
		if (id_text == NULL || b.written(name_key) != NULL)
		{
			b.requirePair(name_key, id_key);
			continue;
		}

		// Malformed text is the row's own refusal, which check() gives.
		ChannelId id = 0;
		if (!idOf(*id_text, id))
			continue;
		// The same channel again changes nothing, and a channel since lost must not refuse it.
		Result<std::string> stored = get(id_key);
		ChannelId was = 0;
		if (stored.ok() && idOf(stored.value(), was) && was == id)
			continue;

		std::string name;
		Error why;
		if (!nameOf(id, i == 1, name, why))
		{
			b.refuseWith(id_key, why);
			continue;
		}
		b.put(name_key, name, id_key);
	}
}

} // namespace settings
} // namespace coreapi
