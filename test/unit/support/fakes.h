/*
 * fakes.h - fake seams the tests run the core API against
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

#ifndef __test_fakes_h__
#define __test_fakes_h__

#include "coreapi/base/deps.h"
#include "coreapi/settings/settingstable.h"

#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

/* The array operators are replaced for the whole binary in test_postmsg.cpp,
   which counts every release. Declared here because a message payload is the
   only thing this layer allocates that way, so a test can tell a block that
   was freed from one that was left behind. */
extern long test_array_deletes;

// Records what it was handed. A refused command has its payload freed by the
// transport, so only what an accepting sink recorded may be read back.
struct FakeCommandSink : public coreapi::CommandSink
{
	std::vector<std::pair<neutrino_msg_t, neutrino_msg_data_t> > posted;
	coreapi::Status answer;

	FakeCommandSink() : answer(coreapi::Status::Ok) {}

	coreapi::Status post(neutrino_msg_t msg, neutrino_msg_data_t data)
	{
		posted.push_back(std::make_pair(msg, data));
		return answer;
	}
};

/* The two globals a case installs into are put back by a destructor, because a
   failed assertion unwinds past a trailing statement and would leave one
   pointing at a fake that is already gone. Declared after the fake it names, so
   the global is cleared before the object it names is. */
struct InstalledSink
{
	explicit InstalledSink(coreapi::CommandSink *s) { coreapi::setCommandSink(s); }
	~InstalledSink() { coreapi::setCommandSink(0); }
};

struct InstalledChannelSource
{
	explicit InstalledChannelSource(coreapi::ChannelSource *s) { coreapi::setChannelSource(s); }
	~InstalledChannelSource() { coreapi::setChannelSource(0); }
};

struct FakeChannelSource : public coreapi::ChannelSource
{
	coreapi::ChannelList channels;
	coreapi::BouquetList bouquets;
	coreapi::ChannelInfo current;
	// More than one bouquet, so resolving the wrong one can be told apart
	// from resolving none.
	std::map<uint32_t, coreapi::ChannelList> bouquet_members;

	// Anything but Ok is handed back untouched, which is the only way into a
	// caller's error branches and the only way to check that it forwards the
	// status instead of inventing one. Both reads of a group share a status
	// because on the box they share a cause.
	coreapi::Status channels_status;
	coreapi::Status bouquets_status;
	coreapi::Status current_status;
	coreapi::Status mode_status;

	// The box mode, in the numbering the mode messages carry. Left at nothing
	// the box ever reports, so a test that means one has to say which.
	int mode;

	// Where the last read left its payload. A caller that hands the same
	// address back has moved it out rather than copied it.
	mutable const void *last_buffer;

	FakeChannelSource()
		: channels_status(coreapi::Status::Ok),
		  bouquets_status(coreapi::Status::Ok),
		  current_status(coreapi::Status::NotFound),
		  mode_status(coreapi::Status::Ok),
		  mode(0),
		  last_buffer(0),
		  services_saved(0),
		  reinits(0),
		  reload_channels_status(coreapi::Status::Ok),
		  saves(0),
		  reloads(0),
		  write_status(coreapi::Status::Ok),
		  save_status(coreapi::Status::Ok),
		  reload_status(coreapi::Status::Ok),
		  awaits(0),
		  saves_at_await(0),
		  reloads_at_await(0),
		  await_status(coreapi::Status::Ok),
		  writes_dropped(false),
		  rename_cuts_name(false),
		  rename_lands_beside(false),
		  order_ignored(false),
		  reads_fail_after_write(false) {}

	coreapi::Status allChannels(bool tv, coreapi::ChannelList &out) const
	{
		if (channels_status != coreapi::Status::Ok)
			return channels_status;
		out.clear();
		for (size_t i = 0; i < channels.size(); i++)
			if (isTv(channels[i].kind) == tv)
				out.push_back(channels[i]);
		remember(out);
		return coreapi::Status::Ok;
	}

	coreapi::Status findChannel(coreapi::ChannelId id, coreapi::ChannelInfo &out) const
	{
		if (channels_status != coreapi::Status::Ok)
			return channels_status;
		for (size_t i = 0; i < channels.size(); i++)
			if (channels[i].id == id) { out = channels[i]; remember(out); return coreapi::Status::Ok; }
		return coreapi::Status::NotFound;
	}

	/* The box's own rule, which is the whole identifier first and the lower
	   forty eight bits after it, and among several of the latter the lowest
	   whole identifier. Written out here rather than left as "the first one
	   held", so that a case can put two channels sharing those bits in either
	   order and still pin which of them is answered. */
	coreapi::Status guideChannel(coreapi::ChannelId asked, coreapi::ChannelId &whole,
				     coreapi::ChannelId &key) const
	{
		if (channels_status != coreapi::Status::Ok)
			return channels_status;

		const coreapi::ChannelInfo *found = 0;
		for (size_t i = 0; i < channels.size(); i++)
			if (channels[i].id == asked)
				found = &channels[i];
		if (found == 0)
		{
			const coreapi::ChannelId wanted = asked & coreapi::GUIDE_KEY_MASK;
			for (size_t i = 0; i < channels.size(); i++)
			{
				if ((channels[i].id & coreapi::GUIDE_KEY_MASK) != wanted)
					continue;
				if (found == 0 || channels[i].id < found->id)
					found = &channels[i];
			}
		}
		if (found == 0)
			return coreapi::Status::NotFound;

		whole = found->id;
		// Nought is a channel nothing filled in rather than one mapped onto
		// the channel numbered nought, and every channel on the box carries
		// its own identifier here where nothing mapped it.
		key = found->epg_id != 0 ? found->epg_id : found->id;
		return coreapi::Status::Ok;
	}

	/* What each channel's streams look like, filed under the channel. A
	   channel with no entry here is a channel the box has and has never
	   switched to, which is Ok with nothing known and is what every channel
	   this fake holds is until a case says otherwise. */
	std::map<coreapi::ChannelId, coreapi::ChannelStreams> streams;

	coreapi::Status channelStreams(coreapi::ChannelId id, coreapi::ChannelStreams &out) const
	{
		if (channels_status != coreapi::Status::Ok)
			return channels_status;
		bool held = false;
		for (size_t i = 0; i < channels.size() && !held; i++)
			held = channels[i].id == id;
		if (!held)
			return coreapi::Status::NotFound;
		std::map<coreapi::ChannelId, coreapi::ChannelStreams>::const_iterator it =
			streams.find(id);
		out = (it != streams.end()) ? it->second : coreapi::ChannelStreams();
		return coreapi::Status::Ok;
	}

	coreapi::Status allBouquets(coreapi::BouquetList &out) const
	{
		if (bouquets_status != coreapi::Status::Ok)
			return bouquets_status;
		out = bouquets;
		remember(out);
		return coreapi::Status::Ok;
	}

	coreapi::Status bouquetChannels(uint32_t id, coreapi::ChannelList &out) const
	{
		if (bouquets_status != coreapi::Status::Ok)
			return bouquets_status;
		std::map<uint32_t, coreapi::ChannelList>::const_iterator it = bouquet_members.find(id);
		if (it == bouquet_members.end())
			return coreapi::Status::NotFound;
		out = it->second;
		remember(out);
		return coreapi::Status::Ok;
	}

	coreapi::Status currentChannel(coreapi::ChannelInfo &out) const
	{
		if (current_status != coreapi::Status::Ok)
			return current_status;
		out = current;
		remember(out);
		return coreapi::Status::Ok;
	}

	coreapi::Status currentMode(int &out) const
	{
		if (mode_status != coreapi::Status::Ok)
			return mode_status;
		out = mode;
		return coreapi::Status::Ok;
	}

	// What the running channel says it is scrambled under, read only while the
	// status of the running channel beside it is Ok.
	std::vector<uint16_t> current_caids;

	/* Sets that status as well, because a case naming what the running channel
	   is scrambled under is a case about a box with something playing, and a
	   box with nothing playing is what this fake is by default. A case meaning
	   an idle box writes the status instead. */
	void setCurrentCaids(const std::vector<uint16_t> &caids)
	{
		current_caids = caids;
		current_status = coreapi::Status::Ok;
	}

	coreapi::Status currentCaids(std::vector<uint16_t> &out) const
	{
		if (current_status != coreapi::Status::Ok)
			return current_status;
		out = current_caids;
		return coreapi::Status::Ok;
	}

	/* What a reading of the channel lists left behind. Counted on the way in
	   and before the status is read, for the reason the two counts below the
	   bouquet writes are: a reading the layer above never asked for and one the
	   box turned down leave the same box behind. */
	unsigned services_saved;
	unsigned reinits;
	std::string reload_reason;
	coreapi::Status reload_channels_status;

	coreapi::Status reloadChannels(bool save_services, const std::string &reason)
	{
		if (save_services)
			services_saved++;
		reinits++;
		reload_reason = reason;
		return reload_channels_status;
	}

	/* Counted, and counted before the status is read, because what the cases
	   are about is whether the layer above made the call at all: a write that
	   was never followed by either of these and one that was followed by two
	   that the box turned down leave the same list behind. */
	unsigned saves;
	unsigned reloads;

	/* Anything but Ok is handed back untouched, which is the only way into the
	   caller's error branches. Three rather than one, because on the box they
	   do not share a cause: a change refused, a list that could not be written
	   out and a list that could not be read back are three things going wrong
	   in three places, and only the first of them leaves nothing behind. */
	coreapi::Status write_status;
	coreapi::Status save_status;
	coreapi::Status reload_status;

	/* Counted like the two above, and the two counts as they stood when it
	   was reached, because where this sits is the whole of what it is for: a
	   wait taken before the list was written out and read back would be a
	   wait for work that had not been asked for yet. */
	unsigned awaits;
	unsigned saves_at_await;
	unsigned reloads_at_await;
	coreapi::Status await_status;

	/* The box takes the change and makes none of it. Every write answers ok
	   and the list stands still, which is what a command written to a socket
	   nobody reads looks like from here: the one failure an answer cannot
	   carry, and the reason there is anything to read back for. */
	bool writes_dropped;

	// A rename whose name arrives cut short, which a field of fixed width in
	// the command does on its own: the old name is gone and the new one is
	// nowhere.
	bool rename_cuts_name;

	// A rename that lands on the bouquet after the one it was meant for: the
	// new name is in the list and the old one is still beside it.
	bool rename_lands_beside;

	// The members set and the order not, which is the half of that call a read
	// back asking only which channels are in the half would pass.
	bool order_ignored;

	/* The list stops being readable once a change has been made. That window
	   is between a write and the read after it and neither of them can see
	   into it, because the box rebuilds this list from a thread of its own.
	   What must not come of it is an answer saying the change did not happen,
	   which is the one thing nobody knows at that point. */
	bool reads_fail_after_write;

	/* What every write below ends with once it has made its change, so that
	   the read after it meets the list in the state the knob above describes.
	   A write that was dropped never reaches this, which is what keeps the two
	   knobs from standing for one thing. */
	coreapi::Status made()
	{
		if (reads_fail_after_write)
			bouquets_status = coreapi::Status::Busy;
		return coreapi::Status::Ok;
	}

	coreapi::Status bouquetByName(const std::string &name, uint32_t &id) const
	{
		if (bouquets_status != coreapi::Status::Ok)
			return bouquets_status;
		for (size_t i = 0; i < bouquets.size(); i++)
		{
			if (bouquets[i].name != name)
				continue;
			id = (uint32_t) i + 1;
			return coreapi::Status::Ok;
		}
		return coreapi::Status::NotFound;
	}

	coreapi::Status addBouquet(const std::string &name)
	{
		if (write_status != coreapi::Status::Ok)
			return write_status;
		if (writes_dropped)
			return coreapi::Status::Ok;
		coreapi::BouquetInfo one;
		one.name = name;
		one.user_bouquet = true;
		bouquets.push_back(one);
		renumber();
		return made();
	}

	coreapi::Status renameBouquet(uint32_t id, const std::string &name)
	{
		if (write_status != coreapi::Status::Ok)
			return write_status;
		if (!holds(id))
			return coreapi::Status::NotFound;
		if (writes_dropped)
			return coreapi::Status::Ok;
		const uint32_t at = (rename_lands_beside && holds(id + 1)) ? id + 1 : id;
		bouquets[at - 1].name = (rename_cuts_name && !name.empty())
			? name.substr(0, name.size() - 1)
			: name;
		return made();
	}

	coreapi::Status moveBouquet(uint32_t id, uint32_t to_id)
	{
		if (write_status != coreapi::Status::Ok)
			return write_status;
		if (!holds(id) || !holds(to_id))
			return coreapi::Status::NotFound;
		if (writes_dropped)
			return coreapi::Status::Ok;
		const coreapi::BouquetInfo one = bouquets[id - 1];
		bouquets.erase(bouquets.begin() + (long)(id - 1));
		bouquets.insert(bouquets.begin() + (long)(to_id - 1), one);
		renumber();
		return made();
	}

	coreapi::Status deleteBouquet(uint32_t id)
	{
		if (write_status != coreapi::Status::Ok)
			return write_status;
		if (!holds(id))
			return coreapi::Status::NotFound;
		if (writes_dropped)
			return coreapi::Status::Ok;
		bouquets.erase(bouquets.begin() + (long)(id - 1));
		renumber();
		return made();
	}

	coreapi::Status setBouquetHidden(uint32_t id, bool hidden)
	{
		if (write_status != coreapi::Status::Ok)
			return write_status;
		if (!holds(id))
			return coreapi::Status::NotFound;
		if (writes_dropped)
			return coreapi::Status::Ok;
		bouquets[id - 1].hidden = hidden;
		return made();
	}

	coreapi::Status setBouquetLock(uint32_t id, bool locked)
	{
		if (write_status != coreapi::Status::Ok)
			return write_status;
		if (!holds(id))
			return coreapi::Status::NotFound;
		if (writes_dropped)
			return coreapi::Status::Ok;
		bouquets[id - 1].locked = locked;
		return made();
	}

	coreapi::Status setBouquetChannels(uint32_t id, const coreapi::ChannelIdList &ids, bool tv)
	{
		if (write_status != coreapi::Status::Ok)
			return write_status;
		if (!holds(id))
			return coreapi::Status::NotFound;
		if (writes_dropped)
			return coreapi::Status::Ok;

		coreapi::ChannelList kept;
		const coreapi::ChannelList &was = bouquet_members[id];
		// The other half untouched, which is the whole of what naming a half
		// means: a fake that replaced both would let a call that lost the
		// radio members of a bouquet pass.
		for (size_t i = 0; i < was.size(); i++)
			if (isTv(was[i].kind) != tv)
				kept.push_back(was[i]);
		/* In the order the list names them, which is what the seam promises
		   and not an accident of walking it forwards: a fake that kept only
		   which channels were named would pass a layer that dropped the order
		   on the way down, and the order is half of what this call carries. */
		for (size_t i = 0; i < ids.size(); i++)
		{
			const size_t at = order_ignored ? ids.size() - 1 - i : i;
			for (size_t j = 0; j < channels.size(); j++)
				if (channels[j].id == ids[at])
					kept.push_back(channels[j]);
		}
		bouquet_members[id] = kept;
		recount(id);
		return made();
	}

	coreapi::Status saveBouquets()
	{
		saves++;
		return save_status;
	}

	coreapi::Status reloadBouquets()
	{
		reloads++;
		return reload_status;
	}

	coreapi::Status awaitCommands()
	{
		awaits++;
		saves_at_await = saves;
		reloads_at_await = reloads;
		return await_status;
	}

	bool holds(uint32_t id) const
	{
		return id >= 1 && (size_t) id <= bouquets.size();
	}

	/* The box numbers a bouquet by where it sits, so anything that changes the
	   order or the length of the list moves every number after it, and the
	   members move with them. Done here for the same reason: a fake handing out
	   numbers that stood still would let a caller pass that addressed a bouquet
	   by a number the box had already given to another one. */
	void renumber()
	{
		std::map<uint32_t, coreapi::ChannelList> moved;
		for (size_t i = 0; i < bouquets.size(); i++)
		{
			const uint32_t was = bouquets[i].id;
			bouquets[i].id = (uint32_t) i + 1;
			std::map<uint32_t, coreapi::ChannelList>::const_iterator it =
				bouquet_members.find(was);
			if (it != bouquet_members.end())
				moved[bouquets[i].id] = it->second;
		}
		bouquet_members.swap(moved);
	}

	// The two counts a bouquet carries, from what it now holds rather than
	// from what it was told, so a case reading them back is reading the list.
	void recount(uint32_t id)
	{
		const coreapi::ChannelList &now = bouquet_members[id];
		uint32_t tv = 0;
		uint32_t radio = 0;
		for (size_t i = 0; i < now.size(); i++)
		{
			if (isTv(now[i].kind))
				tv++;
			else
				radio++;
		}
		bouquets[id - 1].tv_count = tv;
		bouquets[id - 1].radio_count = radio;
	}

	// A vector hands out its buffer, which a copy has to reallocate. A channel
	// holds none, so its name stands in, and that only tells a copy apart
	// where the string is not copy on write.
	template <typename T> void remember(const std::vector<T> &v) const
	{
		last_buffer = v.empty() ? 0 : (const void *) &v[0];
	}

	void remember(const coreapi::ChannelInfo &c) const
	{
		last_buffer = (const void *) c.name.data();
	}

	// Mirrors zapit, where anything that is not radio counts as television.
	static bool isTv(coreapi::ServiceKind k)
	{
		return k != coreapi::ServiceKind::Radio && k != coreapi::ServiceKind::WebRadio;
	}
};

struct InstalledEpgSource
{
	explicit InstalledEpgSource(coreapi::EpgSource *s) { coreapi::setEpgSource(s); }
	~InstalledEpgSource() { coreapi::setEpgSource(0); }
};

/* The guide and a channel list at once, which is what a guide read takes: the guide is
   asked under the key a channel's schedule is filed by, and the channel list is the
   only thing that turns that key into the identifier the answer may name a channel
   with.

   The list starts empty on purpose, and empty is a box holding none of the channels the
   guide has heard of. Every read then answers exactly what it was asked, which is what
   a case saying nothing about channels means. */
struct InstalledGuideSources
{
	FakeChannelSource channels;

	InstalledChannelSource in_channels;
	InstalledEpgSource     in_epg;

	explicit InstalledGuideSources(coreapi::EpgSource *s)
		: in_channels(&channels), in_epg(s) {}

	private:
		InstalledGuideSources(const InstalledGuideSources &);
		InstalledGuideSources &operator=(const InstalledGuideSources &);
};

/* Holds one schedule for every channel at once and picks out of it with the same two
   rules the real source uses, so a case that pins a window or a current event pins the
   rule and not a second copy of it.

   Its channels are matched on the lower forty eight bits, because that is all the event
   manager on the box keeps. A fake matching whole identifiers would be one where
   widening a half looked like it worked and finding the schedule behind it did not. */
struct FakeEpgSource : public coreapi::EpgSource
{
	static bool sameGuideChannel(coreapi::ChannelId a, coreapi::ChannelId b)
	{
		return (a & coreapi::GUIDE_KEY_MASK) == (b & coreapi::GUIDE_KEY_MASK);
	}

	coreapi::EventList events;

	// What the source calls now. A clock is what the real source has instead,
	// and this is how a case says which moment it means.
	time_t now;

	// Anything but Ok is handed back untouched, which is the only way into a
	// caller's error branches. All three reads share one, because on the box
	// they share a cause: the event manager either answers or it does not.
	coreapi::Status status;

	// What the last search was asked for, so a case can tell a query that
	// reached the source from one the domain answered on its own.
	mutable std::string last_query;

	// Counted for the same reason: a read that was refused before it got here
	// and one that got here and matched nothing both leave an empty list.
	mutable unsigned event_reads;

	// Where the last read left its payload, as in the channel fake.
	mutable const void *last_buffer;

	FakeEpgSource()
		: now(0), status(coreapi::Status::Ok), event_reads(0), last_buffer(0),
		  honours_bounds(false), last_limit(0), last_returned(0), detail_reads(0) {}

	coreapi::Status eventsForChannel(coreapi::ChannelId id, time_t from, time_t to,
					 coreapi::EventList &out) const
	{
		event_reads++;
		if (status != coreapi::Status::Ok)
			return status;
		out.clear();
		for (size_t i = 0; i < events.size(); i++)
			if (sameGuideChannel(events[i].channel_id, id) &&
			    coreapi::eventInWindow(events[i].start, events[i].duration, from, to))
				out.push_back(events[i]);
		remember(out);
		return coreapi::Status::Ok;
	}

	coreapi::Status currentEvent(coreapi::ChannelId id, coreapi::EventInfo &out) const
	{
		if (status != coreapi::Status::Ok)
			return status;
		for (size_t i = 0; i < events.size(); i++)
			if (sameGuideChannel(events[i].channel_id, id) &&
			    coreapi::eventCoversTime(events[i].start, events[i].duration, now))
			{
				out = events[i];
				remember(out);
				return coreapi::Status::Ok;
			}
		return coreapi::Status::NotFound;
	}

	// Substring over the title, which is enough to tell a query that was passed on from
	// one that was dropped or altered on the way.
	/* Off, this ignores the window and the ceiling it is handed, so a case sees the
	   layer above narrow what a source gave it. On, it does what the source on the box
	   does. Both shapes have to answer the same, and a fake that could only be one of
	   them is how a guarantee the shipped source could not keep went unnoticed. */
	bool honours_bounds;

	// What the ceiling was when this was last asked, so a case can see that the
	// layer above asks for more than it means to hand back, and how many came
	// back, so a case can see which of the two shapes it drove.
	mutable size_t last_limit;
	mutable size_t last_returned;

	coreapi::Status search(const std::string &query, time_t from, time_t to,
			       size_t limit, coreapi::EventList &out) const
	{
		last_query = query;
		last_limit = limit;
		if (status != coreapi::Status::Ok)
			return status;
		out.clear();
		for (size_t i = 0; i < events.size(); i++)
		{
			if (events[i].title.find(query) == std::string::npos)
				continue;
			if (honours_bounds)
			{
				if (!coreapi::eventInWindow(events[i].start, events[i].duration, from, to))
					continue;
				if (out.size() == limit)
					break;
			}
			out.push_back(events[i]);
		}
		last_returned = out.size();
		remember(out);
		return coreapi::Status::Ok;
	}

	/* What the guide answers for one event, under the pair that names it. Held
	   apart from the list above and not derived from it, because on the box
	   they are two reads of two different records: an entry here and no event
	   above it is what a case drives the one against the other with. */
	std::vector<coreapi::EventDetail> details;

	// Counted for the same reason the list reads are, so a case can tell a
	// detail read that was refused before it got here from one that got here
	// and found nothing.
	mutable unsigned detail_reads;

	coreapi::Status eventDetail(uint64_t event_id, time_t start,
				    coreapi::EventDetail &out) const
	{
		detail_reads++;
		if (status != coreapi::Status::Ok)
			return status;
		for (size_t i = 0; i < details.size(); i++)
		{
			// Both halves, as the event manager matches them: one identifier
			// carries a showing per broadcast and the moment picks one of them.
			if (details[i].event_id != event_id || details[i].start != start)
				continue;
			out = details[i];
			return coreapi::Status::Ok;
		}
		return coreapi::Status::NotFound;
	}

	void remember(const coreapi::EventList &v) const
	{
		last_buffer = v.empty() ? 0 : (const void *) &v[0];
	}

	void remember(const coreapi::EventInfo &e) const
	{
		last_buffer = (const void *) e.title.data();
	}
};


struct InstalledTimerSource
{
	explicit InstalledTimerSource(coreapi::TimerSource *s) { coreapi::setTimerSource(s); }
	~InstalledTimerSource() { coreapi::setTimerSource(0); }
};

/* Keeps the timers in a vector and hands out ids the way the daemon does, from
   one upwards and never reused, so that a case can tell the id of a timer that
   was made from the position it happens to sit at. */
struct FakeTimerSource : public coreapi::TimerSource
{
	coreapi::TimerList timers;

	// What the source calls now, as in the event fake. Left at the epoch, so a
	// case that means a particular moment has to say which.
	time_t clock;

	// Anything but Ok is handed back untouched, which is the only way into a
	// caller's error branches. One status for all four, because on the box
	// they share a cause: the daemon either answers or it does not.
	coreapi::Status status;

	// The clock has one of its own, because it is the only read here that does
	// not go to the daemon: a case that means a failing clock has to be able to
	// fail it without failing everything the daemon answers, or the answer it
	// gets cannot be told from the one a dead daemon gives.
	coreapi::Status now_status;

	// Answered instead of adding, so that a case can reach the branch that
	// reports a timer the daemon refused without building a duplicate.
	coreapi::Status add_status;

	/* Takes the removal and does nothing, which is what a daemon that has
	   stopped reading its socket looks like from here: the removal is
	   acknowledged with nothing either way. */
	bool ignore_removals;

	// Counted, because a removal that was refused before it got here and one
	// that got here and did nothing both leave the list unchanged.
	mutable unsigned list_reads;
	unsigned removals;
	unsigned modifications;

	uint32_t next_id;

	FakeTimerSource()
		: clock(0), status(coreapi::Status::Ok), now_status(coreapi::Status::Ok),
		  add_status(coreapi::Status::Ok), ignore_removals(false),
		  list_reads(0), removals(0), modifications(0), next_id(1) {}

	coreapi::Status list(coreapi::TimerList &out) const
	{
		list_reads++;
		if (status != coreapi::Status::Ok)
			return status;
		out = timers;
		return coreapi::Status::Ok;
	}

	coreapi::Status add(const coreapi::TimerInfo &t, uint32_t &new_id)
	{
		if (status != coreapi::Status::Ok)
			return status;
		if (add_status != coreapi::Status::Ok)
			return add_status;
		coreapi::TimerInfo stored = t;
		stored.id = next_id++;
		timers.push_back(stored);
		new_id = stored.id;
		return coreapi::Status::Ok;
	}

	coreapi::Status modify(const coreapi::TimerInfo &t)
	{
		modifications++;
		if (status != coreapi::Status::Ok)
			return status;
		for (size_t i = 0; i < timers.size(); i++)
		{
			if (timers[i].id == t.id)
			{
				uint32_t keep = timers[i].id;
				timers[i] = t;
				timers[i].id = keep;
				return coreapi::Status::Ok;
			}
		}
		return coreapi::Status::NotFound;
	}

	coreapi::Status remove(uint32_t id)
	{
		removals++;
		if (status != coreapi::Status::Ok)
			return status;
		if (ignore_removals)
			return coreapi::Status::Ok;
		for (size_t i = 0; i < timers.size(); i++)
		{
			if (timers[i].id == id)
			{
				timers.erase(timers.begin() + (long) i);
				return coreapi::Status::Ok;
			}
		}
		return coreapi::Status::Ok;
	}

	coreapi::Status now(time_t &out) const
	{
		if (now_status != coreapi::Status::Ok)
			return now_status;
		out = clock;
		return coreapi::Status::Ok;
	}
};

struct InstalledEventSink
{
	explicit InstalledEventSink(coreapi::EventSink *s) { coreapi::setEventSink(s); }
	~InstalledEventSink() { coreapi::setEventSink(0); }
};

/* Records what it was asked to send, body and all. The body is copied because
   the real sink writes it to a socket before it returns and a caller is free to
   hand it a local, so a fake that kept the pointer would read a dead one. */
struct FakeEventSink : public coreapi::EventSink
{
	struct Sent
	{
		unsigned    id;
		std::string body;
	};

	std::vector<Sent> sent;
	coreapi::Status answer;

	// The one id that is refused, so a case can reach the branch that reports an
	// event the box does not carry without failing every other send.
	unsigned unsupported;

	FakeEventSink() : answer(coreapi::Status::Ok), unsupported(0) {}

	coreapi::Status send(unsigned event_id, const void *body, size_t size)
	{
		if (unsupported != 0 && event_id == unsupported)
			return coreapi::Status::NotSupported;
		if (answer != coreapi::Status::Ok)
			return answer;
		Sent s;
		s.id = event_id;
		if (body != 0 && size != 0)
			s.body.assign((const char *) body, size);
		sent.push_back(s);
		return coreapi::Status::Ok;
	}
};

struct InstalledSystemSource
{
	explicit InstalledSystemSource(coreapi::SystemSource *s) { coreapi::setSystemSource(s); }
	~InstalledSystemSource() { coreapi::setSystemSource(0); }
};

struct FakeSystemSource : public coreapi::SystemSource
{
	coreapi::BoxInfo box;
	int volume_level;
	bool is_muted;

	// Anything but Ok is handed back untouched, which is the only way into a
	// caller's error branches. One status per read rather than one for all
	// three, because the box name comes out of a library and the two others come
	// off a socket, so on the box they do not share a cause.
	coreapi::Status box_status;
	coreapi::Status volume_status;
	coreapi::Status mute_status;

	/* How many slots the box is to be taken as having, and which of them hold a
	   module. A slot beyond the end of the list holds none, so a case saying
	   two slots and naming one is a box with an empty second slot rather than a
	   case that has to fill both. */
	unsigned          ci_slots;
	std::vector<bool> ci_seated;
	bool              ci_used;

	/* One per read. The two slot readings come out of the conditional access
	   layer and the last out of the channel stack, and the count and the slot
	   are apart from one another so that a failure at the second of them can be
	   driven at all: one status for both would always stop at the first. */
	coreapi::Status ci_slot_status;
	coreapi::Status ci_present_status;
	coreapi::Status ci_use_status;

	// Every slot this was asked about, in the order it was asked, so a case can
	// show that the walk stops at the module it found.
	mutable std::vector<unsigned> ci_asked;

	FakeSystemSource()
		: volume_level(0), is_muted(false),
		  box_status(coreapi::Status::Ok),
		  volume_status(coreapi::Status::Ok),
		  mute_status(coreapi::Status::Ok),
		  ci_slots(0), ci_used(false),
		  ci_slot_status(coreapi::Status::Ok),
		  ci_present_status(coreapi::Status::Ok),
		  ci_use_status(coreapi::Status::Ok) {}

	coreapi::Status boxInfo(coreapi::BoxInfo &out) const
	{
		if (box_status != coreapi::Status::Ok)
			return box_status;
		out = box;
		return coreapi::Status::Ok;
	}

	coreapi::Status volume(int &out) const
	{
		if (volume_status != coreapi::Status::Ok)
			return volume_status;
		out = volume_level;
		return coreapi::Status::Ok;
	}

	coreapi::Status muted(bool &out) const
	{
		if (mute_status != coreapi::Status::Ok)
			return mute_status;
		out = is_muted;
		return coreapi::Status::Ok;
	}

	coreapi::Status ciSlotCount(unsigned &out) const
	{
		if (ci_slot_status != coreapi::Status::Ok)
			return ci_slot_status;
		out = ci_slots;
		return coreapi::Status::Ok;
	}

	coreapi::Status ciModulePresent(unsigned slot, bool &out) const
	{
		ci_asked.push_back(slot);
		if (ci_present_status != coreapi::Status::Ok)
			return ci_present_status;
		out = slot < ci_seated.size() && ci_seated[slot];
		return coreapi::Status::Ok;
	}

	coreapi::Status ciInUse(bool &out) const
	{
		if (ci_use_status != coreapi::Status::Ok)
			return ci_use_status;
		out = ci_used;
		return coreapi::Status::Ok;
	}
};

struct InstalledSettingsSource
{
	explicit InstalledSettingsSource(coreapi::SettingsSource *s) { coreapi::setSettingsSource(s); }
	~InstalledSettingsSource() { coreapi::setSettingsSource(0); }
};

// For a case that installs the real source, which keeps the only pointer to
// itself and leaves the case none to put back.
struct ClearedSettingsSource
{
	~ClearedSettingsSource() { coreapi::setSettingsSource(0); }
};

/* Puts the shipped table back from a destructor, because a check that fails
   unwinds past a last line: every other case here walks the table, and one left
   holding a fixture would be checking the fixture rather than what the program
   declares. */
struct InstalledSettingsTable
{
	InstalledSettingsTable(const coreapi::Descriptor *t, size_t n)
	{
		coreapi::setSettingsTable(t, n);
	}
	~InstalledSettingsTable() { coreapi::setSettingsTable(0, 0); }
};

/* Counts what it was handed and answers what a case told it to. The key is
   copied because the caller hands over a pointer into whichever table was
   installed, and a case may read it back after that table is gone. */
struct FakeApplier : public coreapi::SettingsApplier
{
	unsigned calls;
	std::string last_key;
	bool answer;

	FakeApplier() : calls(0), answer(true) {}

	bool apply(const char *key)
	{
		calls++;
		last_key = (key != 0) ? key : "";
		return answer;
	}
};

/* Takes the applier away from a destructor, for the reason every other guard
   here has one: a check that fails unwinds past a last line, and the registry
   would go on naming an object that is gone. The section name is kept rather
   than copied, so a case has to hand over one that outlives the guard. */
struct InstalledApplier
{
	const char *section;

	InstalledApplier(const char *s, coreapi::SettingsApplier *a) : section(s)
	{
		coreapi::registerSettingsApplier(s, a);
	}
	~InstalledApplier() { coreapi::registerSettingsApplier(section, 0); }
};

/* Keeps the two kinds apart in two maps, as the store on the box does not: a
   read that went to the wrong one answers a key that is there rather than
   nothing, and only a fake that can hold both under one name shows it. */
struct FakeSettingsSource : public coreapi::SettingsSource
{
	std::map<std::string, long> ints;
	std::map<std::string, std::string> strings;

	// Counted, because a write that only reached memory and one that was saved
	// leave the same value behind.
	unsigned persisted;

	/* Fails the next call whichever it is and clears itself, which is the only
	   way into a caller's error branches. One call rather than all of them, so
	   a case can fail one read and see the calls around it still land. */
	mutable bool fail_next;

	FakeSettingsSource() : persisted(0), fail_next(false) {}

	bool failing() const
	{
		if (!fail_next)
			return false;
		fail_next = false;
		return true;
	}

	coreapi::Status readInt(const char *key, long &out) const
	{
		if (failing())
			return coreapi::Status::Internal;
		std::map<std::string, long>::const_iterator it = ints.find(key);
		if (it == ints.end())
			return coreapi::Status::NotFound;
		out = it->second;
		return coreapi::Status::Ok;
	}

	coreapi::Status readString(const char *key, std::string &out) const
	{
		if (failing())
			return coreapi::Status::Internal;
		std::map<std::string, std::string>::const_iterator it = strings.find(key);
		if (it == strings.end())
			return coreapi::Status::NotFound;
		out = it->second;
		return coreapi::Status::Ok;
	}

	coreapi::Status writeInt(const char *key, long value)
	{
		if (failing())
			return coreapi::Status::Internal;
		ints[key] = value;
		return coreapi::Status::Ok;
	}

	coreapi::Status writeString(const char *key, const std::string &value)
	{
		if (failing())
			return coreapi::Status::Internal;
		strings[key] = value;
		return coreapi::Status::Ok;
	}

	coreapi::Status persist()
	{
		if (failing())
			return coreapi::Status::Internal;
		persisted++;
		return coreapi::Status::Ok;
	}
};

struct InstalledLocaleSource
{
	explicit InstalledLocaleSource(coreapi::LocaleSource *s) { coreapi::setLocaleSource(s); }
	~InstalledLocaleSource() { coreapi::setLocaleSource(0); }
};

/* What a case has for a catalog: a map a case fills by hand rather than anything read
   off a disc, so a case names exactly the keys it means to have resolve. A key the map
   does not carry is NotFound, the same answer the real one gives, so a case checking
   that a route leaves a field out need not install anything at all. */
struct FakeLocaleSource : public coreapi::LocaleSource
{
	std::map<std::string, std::string> texts;

	coreapi::Status text(const char *key, std::string &out) const
	{
		if (key == NULL)
			return coreapi::Status::NotFound;
		std::map<std::string, std::string>::const_iterator it = texts.find(key);
		if (it == texts.end())
			return coreapi::Status::NotFound;
		out = it->second;
		return coreapi::Status::Ok;
	}
};

/* Below this, one place per subject for the fakes the cases still have to be given.

   Kept apart for the reason the seams they stand in for are kept apart: written where
   the last one happened to end they would all arrive in the same part of this file.

   Each is empty. A fake whose seam already exists above goes beside that seam.

   What is not marked out here is the object that fills every seam at once at the end of
   this file. A seam only belongs in it when its accessor ends the process with nothing
   installed. */

// Writing bouquets needs no fake of its own: it is the same table the channel
// source above reads, so it is on that fake beside the reads of it.

/* The remote control. Nothing here is wired to a node, so this is where a case sees
   what would have gone out: the code of the last key and how many keys reached the
   device at all. The second is what a case meaning nothing to have been sent reads,
   because a name turned away above this and a key the node refused leave the same empty
   answer behind. The flag reads false, which is what a box nobody has locked is. */
struct FakeInputDevice : public coreapi::InputDevice
{
	unsigned long last_code;

	// Counted on the way in, so that a key refused before it got here and one
	// the node turned away do not read alike.
	unsigned writes;

	// Read only while the status beside it is Ok.
	bool is_locked;

	/* One status per call and not one for both, because on the box the two
	   have nothing to fail over: a key is stopped by the node it is written to
	   and the flag by a build holding none. */
	coreapi::Status send_status;
	coreapi::Status lock_status;

	FakeInputDevice()
		: last_code(0), writes(0), is_locked(false),
		  send_status(coreapi::Status::Ok),
		  lock_status(coreapi::Status::Ok) {}

	coreapi::Status sendKey(unsigned long code)
	{
		writes++;
		if (send_status != coreapi::Status::Ok)
			return send_status;
		last_code = code;
		return coreapi::Status::Ok;
	}

	coreapi::Status locked(bool &out) const
	{
		if (lock_status != coreapi::Status::Ok)
			return lock_status;
		out = is_locked;
		return coreapi::Status::Ok;
	}
};

struct InstalledInputDevice
{
	explicit InstalledInputDevice(coreapi::InputDevice *d) { coreapi::setInputDevice(d); }
	~InstalledInputDevice() { coreapi::setInputDevice(0); }
};

/* The screen, and a build that cannot read one is what these cases run in, so nothing
   here reaches a decoder or a framebuffer: a capture is a file this writes, which is
   what makes a case about clearing them up able to count anything at all. A fake that
   only remembered being asked would leave such a case counting nothing.

   The display on the front of the box answers NotSupported until a case says otherwise,
   because most boxes have none. */
struct FakeScreenshotSource : public coreapi::ScreenshotSource
{
	// What a capture leaves in the file.
	std::string content;

	/* One status per capture rather than one for both, because on the box the
	   two have nothing to fail over: one is the video decoder and the
	   framebuffer, the other is a display most boxes do not have. */
	coreapi::Status screen_status;
	coreapi::Status display_status;

	// Counted on the way in, so a capture refused above this and one that was
	// taken and came to nothing do not read alike.
	unsigned screen_shots;
	unsigned display_shots;

	/* Where the layer above said to put it, which halves it asked for, and
	   which form it asked the file to be written in. The form is kept because
	   nothing else can see it: what the capture does with it is behind the
	   encoder, and a layer that read a form and sent another one down would
	   look from the answer exactly like one that read it right. */
	std::string last_path;
	bool last_osd;
	bool last_video;
	coreapi::PictureFormat last_format;

	FakeScreenshotSource()
		: content("a picture"),
		  screen_status(coreapi::Status::Ok),
		  display_status(coreapi::Status::NotSupported),
		  screen_shots(0), display_shots(0),
		  last_osd(false), last_video(false),
		  last_format(coreapi::PictureFormat::Png) {}

	coreapi::Status captureScreen(bool osd, bool video, coreapi::PictureFormat format,
				      const std::string &path)
	{
		screen_shots++;
		last_osd = osd;
		last_video = video;
		last_format = format;
		last_path = path;
		if (screen_status != coreapi::Status::Ok)
			return screen_status;
		return writeFile(path) ? coreapi::Status::Ok : coreapi::Status::Internal;
	}

	coreapi::Status captureDisplay(const std::string &path)
	{
		display_shots++;
		last_path = path;
		if (display_status != coreapi::Status::Ok)
			return display_status;
		return writeFile(path) ? coreapi::Status::Ok : coreapi::Status::Internal;
	}

	private:
		bool writeFile(const std::string &path) const
		{
			std::FILE *f = std::fopen(path.c_str(), "wb");
			if (f == NULL)
				return false;
			const bool whole = content.empty() ||
				std::fwrite(content.data(), 1, content.size(), f) == content.size();
			std::fclose(f);
			return whole;
		}
};

struct InstalledScreenshotSource
{
	explicit InstalledScreenshotSource(coreapi::ScreenshotSource *s) { coreapi::setScreenshotSource(s); }
	~InstalledScreenshotSource() { coreapi::setScreenshotSource(0); }
};

/* The tuner, and a box with none is what it answers by default: no frontend is
   carrying a picture and the list of them is empty. That is what the build
   these cases run in is, and a fake handing out a tuner unasked would let a
   case pass that the box it stands for could not answer at all. A case that
   means a box with tuners says which. */
struct FakeTunerSource : public coreapi::TunerSource
{
	coreapi::FrontendList tuners;

	// What the live reading answers with, and read only while its status is Ok.
	coreapi::SignalInfo signal;

	/* Anything but Ok is handed back untouched, which is the only way into a caller's
	   error branches. One status per read rather than one for both, because on the box
	   they do not share a cause: a box with tuners and nothing tuned answers the list
	   and not the reading. */
	coreapi::Status list_status;
	coreapi::Status live_status;

	/* What the reset answers, and how many times it was asked for. Counted
	   because the reset has nothing to hand back: a request refused before this
	   was reached and one that reached it and was turned down leave the same
	   nothing behind. */
	coreapi::Status reset_status;
	unsigned resets;

	// Counted, because a read refused before it got here and one that got here
	// and found no tuner both leave an empty answer behind.
	mutable unsigned list_reads;
	mutable unsigned live_reads;

	FakeTunerSource()
		: list_status(coreapi::Status::Ok),
		  live_status(coreapi::Status::NotSupported),
		  reset_status(coreapi::Status::Ok), resets(0),
		  list_reads(0), live_reads(0) {}

	coreapi::Status frontends(coreapi::FrontendList &out) const
	{
		list_reads++;
		if (list_status != coreapi::Status::Ok)
			return list_status;
		out = tuners;
		return coreapi::Status::Ok;
	}

	coreapi::Status liveSignal(coreapi::SignalInfo &out) const
	{
		live_reads++;
		if (live_status != coreapi::Status::Ok)
			return live_status;
		out = signal;
		return coreapi::Status::Ok;
	}

	coreapi::Status reset()
	{
		resets++;
		return reset_status;
	}
};

struct InstalledTunerSource
{
	explicit InstalledTunerSource(coreapi::TunerSource *s) { coreapi::setTunerSource(s); }
	~InstalledTunerSource() { coreapi::setTunerSource(0); }
};

/* The plugins of the box. Nothing here scans a disc, so what a case sees is a
   list it wrote and a record of what was asked to run, the second counted on
   the way in so that a name turned away above this and a run the box refused do
   not read alike. */
struct FakePluginSource : public coreapi::PluginSource
{
	coreapi::PluginList plugins;

	// Anything but Ok is handed back untouched, which is the only way into a
	// caller's error branches.
	coreapi::Status list_status;

	// Counted, because a read refused before it got here and one that got here
	// and found no plugins both leave an empty answer behind.
	mutable unsigned reads;

	FakePluginSource() : list_status(coreapi::Status::Ok), reads(0) {}

	// One entry made of what a case actually says about it, so a case adding
	// one does not have to write out the fields it has nothing to say about.
	void add(const std::string &name, const std::string &title)
	{
		coreapi::PluginInfo one;
		one.name = name;
		one.title = title;
		one.kind = coreapi::PluginKind::Script;
		plugins.push_back(one);
	}

	coreapi::Status list(coreapi::PluginList &out) const
	{
		reads++;
		if (list_status != coreapi::Status::Ok)
			return list_status;
		out = plugins;
		return coreapi::Status::Ok;
	}
};

struct InstalledPluginSource
{
	explicit InstalledPluginSource(coreapi::PluginSource *s) { coreapi::setPluginSource(s); }
	~InstalledPluginSource() { coreapi::setPluginSource(0); }
};


/* The picture a channel is shown with. Nothing here reads the box's own
   directories either, so what a case says is which channels have one and where,
   and a case meaning a channel with no picture says that rather than leaving it
   to whatever the machine running this happens to carry. */
struct FakeLogoSource : public coreapi::LogoSource
{
	// Which channels have a picture and where it is. A channel that is not
	// named here has none.
	std::map<coreapi::ChannelId, std::string> paths;

	// Anything but Ok is handed back untouched, which is the only way into a
	// caller's error branches.
	coreapi::Status status;

	/* Counted on the way in, so a search turned away above this and one that
	   got here and found nothing do not read alike. A case about a channel
	   with no picture rests on it: without the count that case is green
	   whether or not anything ever looked. */
	mutable unsigned searches;

	// What the layer above said the channel is called, which is half of what
	// the search on the box goes by and is not derivable from the identifier.
	mutable std::string last_name;

	FakeLogoSource() : status(coreapi::Status::Ok), searches(0) {}

	void put(coreapi::ChannelId id, const std::string &path) { paths[id] = path; }

	coreapi::Status logoFor(coreapi::ChannelId id, const std::string &name,
				std::string &path) const
	{
		searches++;
		last_name = name;
		if (status != coreapi::Status::Ok)
			return status;
		std::map<coreapi::ChannelId, std::string>::const_iterator it = paths.find(id);
		if (it == paths.end())
			return coreapi::Status::NotFound;
		path = it->second;
		return coreapi::Status::Ok;
	}
};

struct InstalledLogoSource
{
	explicit InstalledLogoSource(coreapi::LogoSource *s) { coreapi::setLogoSource(s); }
	~InstalledLogoSource() { coreapi::setLogoSource(0); }
};


/* What the box is recording. Nothing here writes to a disc, so what a case sees is a
   list it wrote, and beside it a record of every number this was asked to stop, in the
   order it was asked. The numbers are kept on the way in and not only on the way out,
   so a request turned away before it got here and one the box refused do not read
   alike: a case about ending a recording that is not there rests on that. */
struct FakeRecordingSource : public coreapi::RecordingSource
{
	coreapi::RecordingList recordings;

	// Anything but Ok is handed back untouched, which is the only way into a
	// caller's error branches.
	coreapi::Status list_status;
	coreapi::Status stop_status;

	// Counted, because a read refused before it got here and one that got here
	// and found nothing both leave an empty answer behind.
	mutable unsigned reads;

	std::vector<uint32_t> stopped;

	FakeRecordingSource()
		: list_status(coreapi::Status::Ok),
		  stop_status(coreapi::Status::Ok),
		  reads(0) {}

	// One entry made of what a case actually says about it, so a case adding
	// one does not have to write out the fields it has nothing to say about.
	void add(uint32_t id, coreapi::ChannelId channel, const std::string &path,
		 bool timeshift = false, bool from_timer = false)
	{
		coreapi::RecordingInfo one;
		one.id = id;
		one.channel_id = channel;
		one.path = path;
		one.timeshift = timeshift;
		one.from_timer = from_timer;
		recordings.push_back(one);
	}

	coreapi::Status list(coreapi::RecordingList &out) const
	{
		reads++;
		if (list_status != coreapi::Status::Ok)
			return list_status;
		out = recordings;
		return coreapi::Status::Ok;
	}

	coreapi::Status stop(uint32_t id)
	{
		stopped.push_back(id);
		return stop_status;
	}
};

struct InstalledRecordingSource
{
	explicit InstalledRecordingSource(coreapi::RecordingSource *s) { coreapi::setRecordingSource(s); }
	~InstalledRecordingSource() { coreapi::setRecordingSource(0); }
};


/* All twelve seams whose accessors end the process, filled at once, for a case that has
   to get past the check the server makes before it starts without caring what any of
   them answer.

   The fakes are declared ahead of the objects that install them, because members are
   built in the order they are written and torn down backwards: a pointer installed
   before the object it names existed would be a pointer to nothing.

   A case that has something to say about one of these installs its own over the top,
   and taking that one away clears the seam rather than putting this one back. So the
   inner one has to outlive every request the case makes. */
struct InstalledDependencies
{
	FakeChannelSource channels;
	FakeEpgSource     epg;
	FakeTimerSource   timers;
	FakeSystemSource  system;
	FakeTunerSource   tuner;
	FakeInputDevice   input;
	FakeScreenshotSource screen;
	FakeLogoSource    logos;
	FakePluginSource  plugins;
	FakeRecordingSource recordings;
	FakeCommandSink   commands;
	FakeEventSink     events;

	InstalledChannelSource in_channels;
	InstalledEpgSource     in_epg;
	InstalledTimerSource   in_timers;
	InstalledSystemSource  in_system;
	InstalledTunerSource   in_tuner;
	InstalledInputDevice   in_input;
	InstalledScreenshotSource in_screen;
	InstalledLogoSource    in_logos;
	InstalledPluginSource  in_plugins;
	InstalledRecordingSource in_recordings;
	InstalledSink          in_commands;
	InstalledEventSink     in_events;

	InstalledDependencies()
		: in_channels(&channels),
		  in_epg(&epg),
		  in_timers(&timers),
		  in_system(&system),
		  in_tuner(&tuner),
		  in_input(&input),
		  in_screen(&screen),
		  in_logos(&logos),
		  in_plugins(&plugins),
		  in_recordings(&recordings),
		  in_commands(&commands),
		  in_events(&events)
	{
	}

	private:
		InstalledDependencies(const InstalledDependencies &);
		InstalledDependencies &operator=(const InstalledDependencies &);
};

/* The pair the timer daemon holds, in the seconds it holds them in. Both are kept,
   because the daemon takes them together and the defect a single number would hide is a
   write of one that carries the other away with it. Counted, so a case can tell a write
   that reached the daemon from one that only looked as though it had. */
struct FakeRecordingSafety : public coreapi::RecordingSafetySource
{
	int before;
	int after;

	// Counted through a const read, because the seam's read is const.
	mutable unsigned reads;
	unsigned writes;

	/* Fails the next call whichever it is and clears itself, the way the
	   settings fake beside it does: that is the only way into the branches a
	   caller takes when the daemon cannot be reached. */
	mutable bool fail_next;

	FakeRecordingSafety() : before(0), after(0), reads(0), writes(0), fail_next(false) {}

	bool failing() const
	{
		if (!fail_next)
			return false;
		fail_next = false;
		return true;
	}

	coreapi::Status read(int &b, int &a) const
	{
		reads++;
		if (failing())
			return coreapi::Status::NotSupported;
		b = before;
		a = after;
		return coreapi::Status::Ok;
	}

	coreapi::Status write(int b, int a)
	{
		writes++;
		if (failing())
			return coreapi::Status::NotSupported;
		before = b;
		after = a;
		return coreapi::Status::Ok;
	}
};

struct InstalledRecordingSafety
{
	explicit InstalledRecordingSafety(coreapi::RecordingSafetySource *s)
	{
		coreapi::setRecordingSafetySource(s);
	}
	~InstalledRecordingSafety() { coreapi::setRecordingSafetySource(0); }
};

/* The sets a screen would answer for, as a map a case fills by hand: a case
   names exactly what it means to have an answer for, and every other name is
   NotFound, which is what a name nobody registered really answers. */
struct FakeSettingChoices : public coreapi::SettingChoices
{
	std::map<std::string, std::vector<coreapi::SettingChoice> > sets;

	// Counted, so a case can tell an answer that came from here from one a
	// table carried all along.
	mutable unsigned asked;

	FakeSettingChoices() : asked(0) {}

	void offer(const std::string &name, long value, const std::string &label)
	{
		coreapi::SettingChoice one;
		one.value = value;
		one.label = label;
		sets[name].push_back(one);
	}

	coreapi::Status values(const char *name, std::vector<coreapi::SettingChoice> &out) const
	{
		asked++;
		if (name == 0)
			return coreapi::Status::NotFound;
		std::map<std::string, std::vector<coreapi::SettingChoice> >::const_iterator it =
			sets.find(name);
		if (it == sets.end())
			return coreapi::Status::NotFound;
		out = it->second;
		return coreapi::Status::Ok;
	}
};

struct InstalledSettingChoices
{
	explicit InstalledSettingChoices(coreapi::SettingChoices *s)
	{
		coreapi::setSettingChoices(s);
	}
	~InstalledSettingChoices() { coreapi::setSettingChoices(0); }
};

/* The size the box draws at, as the object beside the settings keeps it.
   Counted for the reason the pair above is: a write that reached this and one
   that only looked as though it had leave the same number behind otherwise. */
struct FakeOsdResolution : public coreapi::OsdResolutionSource
{
	int mode;

	mutable unsigned reads;
	unsigned writes;

	mutable bool fail_next;

	FakeOsdResolution() : mode(0), reads(0), writes(0), fail_next(false) {}

	bool failing() const
	{
		if (!fail_next)
			return false;
		fail_next = false;
		return true;
	}

	coreapi::Status read(int &out) const
	{
		reads++;
		if (failing())
			return coreapi::Status::NotSupported;
		out = mode;
		return coreapi::Status::Ok;
	}

	coreapi::Status write(int in)
	{
		writes++;
		if (failing())
			return coreapi::Status::NotSupported;
		mode = in;
		return coreapi::Status::Ok;
	}
};

struct InstalledOsdResolution
{
	explicit InstalledOsdResolution(coreapi::OsdResolutionSource *s)
	{
		coreapi::setOsdResolutionSource(s);
	}
	~InstalledOsdResolution() { coreapi::setOsdResolutionSource(0); }
};

#endif
