/*
 * channels.cpp - channels, bouquets and zapping, above the zapit client
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

#include "channels.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/eventbus.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

#include <limits.h>
#include <stdlib.h>

#include <neutrinoMessages.h>
#include <timerdclient/timerdtypes.h>
#include <zapit/types.h>

namespace coreapi
{
namespace channels
{

// The status always comes from the source, so one cause cannot become two
// answers on the way out. Only the code and the wording are this layer's.

/* THE ORDER THE WHOLE LIST IS ANSWERED IN, and why it is not the one the box
   keeps it in.

   The box holds its channels in a map keyed on the identifier, so reading them
   out gives them in identifier order, which is a sixty-four bit number nobody
   sees. A reader gets the numbers it does show scattered through the list:
   540, 529, 528, 549, 653. It looks like no order at all, and it made both the
   list of every channel and the picker a timer is written with unusable for
   finding anything.

   So they come out by the number the box draws beside them. Every present
   channel has one: the bouquets are numbered in their own order first, and
   what no bouquet holds is swept into one last bouquet, sorted by name, and
   numbered after them (CBouquetManager::makeRemainingChannelsBouquet).

   Sorted here and not where they are read, because the box adapter is not
   linked into the tests and an order is a promise a reader holds this layer to.

   Only this listing. A bouquet's own listing keeps the bouquet's order, which
   is what the box draws and what a person dragging rows about has just set.

   A nought would sort ahead of everything, so it goes last instead: it means
   the box holds no place for the channel, not that it holds the first. */
void byNumberThenName(ChannelList &all)
{
	std::stable_sort(all.begin(), all.end(),
		[](const ChannelInfo &a, const ChannelInfo &b)
		{
			if ((a.number == 0) != (b.number == 0))
				return b.number == 0;
			if (a.number != b.number)
				return a.number < b.number;
			// Two channels of one number is the box having kept numbers from a
			// file that named one twice. The name keeps the order steady across
			// two readings so that a cursor walks the list once.
			if (a.name != b.name)
				return a.name < b.name;
			return a.id < b.id;
		});
}

Result<ChannelList> list(bool tv)
{
	ChannelList out;
	Status s = channelSource().allChannels(tv, out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");
	byNumberThenName(out);
	return ok(std::move(out));
}

Result<ChannelInfo> get(ChannelId id)
{
	ChannelInfo out;
	Status s = channelSource().findChannel(id, out);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchChannel,
			    "no channel with that id");
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");
	return ok(std::move(out));
}

Result<ChannelInfo> current()
{
	ChannelInfo out;
	Status s = channelSource().currentChannel(out);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoRunningChannel,
			    "nothing is playing");
	if (s != Status::Ok)
		return fail(s, ErrorCode::CurrentChannelUnresolved,
			    "the running channel is not in the channel list");
	return ok(std::move(out));
}

Result<ChannelStreams> streams(ChannelId id)
{
	ChannelStreams out;
	Status s = channelSource().channelStreams(id, out);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchChannel,
			    "no channel with that id");
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");
	return ok(std::move(out));
}

Result<BouquetList> bouquets()
{
	BouquetList out;
	Status s = channelSource().allBouquets(out);
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");
	return ok(std::move(out));
}

Result<ChannelList> bouquetChannels(uint32_t bouquet_id)
{
	ChannelList out;
	Status s = channelSource().bouquetChannels(bouquet_id, out);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchBouquet,
			    "no bouquet with that id");
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");
	return ok(std::move(out));
}

Result<int> mode()
{
	int out = 0;
	Status s = channelSource().currentMode(out);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoMode,
			    "the box has not picked a mode yet");
	if (s != Status::Ok)
		return fail(s, ErrorCode::ModeUnavailable,
			    "the box mode could not be read");
	return ok(out);
}

namespace
{

/* Where a path leads when it is a link, and nothing when it leads to itself.
   Cleared on equality rather than answered twice: a caller handed two equal
   paths would read the second as somewhere else the first points at. */
std::string leadsTo(const std::string &path)
{
	char real[PATH_MAX + 1];
	std::memset(real, 0, sizeof(real));
	if (realpath(path.c_str(), real) == NULL)
		return std::string();
	if (path == real)
		return std::string();
	return std::string(real);
}

/* A number and a name in one row, so the two cannot be put out of step: the
   table this is taken from kept them in two arrays walked by one index, a shape
   where reordering one renames every system. The number is a group and not one
   identifier: a system is issued a range of identifiers sharing an upper byte. */
struct CaSystem
{
	uint16_t    group;
	const char *name;
};

const CaSystem kCaSystems[] = {
	{ 0x0600, "Irdeto" },
	{ 0x1700, "Betacrypt" },
	{ 0x0100, "Seca" },
	{ 0x0500, "Viaccess" },
	{ 0x1800, "Nagra" },
	{ 0x0B00, "Conax" },
	{ 0x0D00, "Cryptoworks" },
	{ 0x0900, "Videoguard" },
	{ 0x2600, "Biss" },
	{ 0x4A00, "DreCrypt" },
	{ 0x0E00, "PowerVU" },
	{ 0x1000, "Tandberg" },
	{ 0x5600, "Verimatrix" },
};

// Nothing for an identifier no row names, which leaves the number standing on
// its own. A name taken off the nearest row would be this program saying
// something about a system it does not know.
const char *caSystemName(uint16_t caid)
{
	const uint16_t group = (uint16_t) (caid & 0xFF00);
	for (size_t i = 0; i < sizeof(kCaSystems) / sizeof(kCaSystems[0]); i++)
	{
		if (kCaSystems[i].group == group)
			return kCaSystems[i].name;
	}
	return "";
}

/* The two words the box files a reading of its channel lists under. Written
   once, so that the pair cannot come apart at the one place they are chosen
   between. */
const char kSoftReload[] = "webif_reload";
const char kHardReload[] = "webif_hardreload";

} // namespace

Result<std::string> logo(ChannelId id)
{
	/* The name the box has for the channel, and nothing where it has none. The
	   search goes by the name and by the identifier both, so a channel the list
	   does not hold narrows it rather than ending it. */
	std::string name;
	ChannelInfo c;
	if (channelSource().findChannel(id, c) == Status::Ok)
		name = c.name;

	std::string path;
	Status s = logoSource().logoFor(id, name, path);
	if (s != Status::Ok)
		return fail(s, ErrorCode::NoSuchLogo,
			    "no picture for that channel");
	return ok(std::move(path));
}

Result<LogoList> logos(bool tv, bool files)
{
	ChannelList all;
	Status s = channelSource().allChannels(tv, all);
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");

	LogoList out;
	out.reserve(all.size());
	for (size_t i = 0; i < all.size(); i++)
	{
		LogoInfo one;
		one.id = all[i].id;
		one.short_id = all[i].id & GUIDE_KEY_MASK;
		one.name = all[i].name;
		// Only where the caller asked for it: one walk of the picture
		// directories per channel.
		if (files)
		{
			std::string path;
			if (logoSource().logoFor(one.id, one.name, path) == Status::Ok)
			{
				one.path = path;
				one.resolved = leadsTo(path);
			}
		}
		out.push_back(std::move(one));
	}
	return ok(std::move(out));
}

Result<CaidList> currentCaids()
{
	std::vector<uint16_t> raw;
	Status s = channelSource().currentCaids(raw);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoRunningChannel,
			    "nothing is playing");
	if (s != Status::Ok)
		return fail(s, ErrorCode::CurrentChannelUnresolved,
			    "the running channel is not in the channel list");

	CaidList out;
	out.reserve(raw.size());
	for (size_t i = 0; i < raw.size(); i++)
	{
		CaidInfo one;
		one.caid = raw[i];
		one.system = caSystemName(raw[i]);
		out.push_back(one);
	}
	return ok(std::move(out));
}

Result<void> reloadChannels(bool hard)
{
	/* The services are written out for the soft reading and not for the hard
	   one, which is the whole of what the two are: writing them out first is
	   what makes what the box holds win over what is on the disc. */
	Status s = channelSource().reloadChannels(!hard, hard ? kHardReload : kSoftReload);
	/* The channel stack was not there to be asked, which is the one way this
	   fails and is the same thing as a channel list that cannot be read. */
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the box did not read its channel lists again");
	return ok();
}

// The commands below name only which message they mean. How one is carried,
// and how a refusal reads, belongs to the transport rather than to the domain.

Result<void> zap(ChannelId id)
{
	ChannelInfo probe;
	Status s = channelSource().findChannel(id, probe);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchChannel,
			    "no channel with that id");
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");

	// Zeroed and not merely filled in, because the loop reads a whole event
	// out of this message and only one of its fields is this command's.
	CTimerd::EventInfo event = CTimerd::EventInfo();
	event.channel_id = (t_channel_id) id;

	return postPayload(NeutrinoMessages::ZAPTO, &event, sizeof(event));
}

Result<void> setMode(Mode m)
{
	// The mode alone. The flag that can ride beside it would leave the running
	// channel where it is, which is not what asking for a mode means.
	neutrino_msg_data_t value = (m == Mode::Radio) ? NeutrinoModes::mode_radio
						       : NeutrinoModes::mode_tv;

	return postCommand(NeutrinoMessages::CHANGEMODE, value);
}

namespace
{

/* The position the source addresses that name by. One place, so that no two of
   the calls below can disagree about which bouquet a name means, and so that
   the answer for a name nothing carries is written once. */
Result<uint32_t> position(const std::string &name)
{
	uint32_t id = 0;
	Status s = channelSource().bouquetByName(name, id);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchBouquet,
			    "no bouquet of that name");
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");
	return ok(id);
}

/* What every write below ends with, and what makes looking for the change
   afterwards worth anything.

   A change is in memory until the list is written out, and the box goes on
   drawing the list it loaded until that is read back, so leaving either out
   loses the change. What the two do not say is that the change happened: the
   box answers none of the writes above, so the call that sent one comes back
   before the box has looked at it, and these two would then be writing out a
   list nothing had been done to yet. The third is the one that waits. */
Result<void> writeOut()
{
	Status s = channelSource().saveBouquets();
	if (s == Status::Ok)
		s = channelSource().reloadBouquets();
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetNotChanged,
			    "the change did not reach the box");

	/* Said here and not at any of the seven, for the reason the writing out is
	   here. The wait that makes it true is the announcing call's own. */
	return announceBouquetsChanged();
}

// What a write of the bouquet itself answers when the box turns it down. The
// three outcomes are one code, which is stated where the code is declared.
Result<void> changed(Status s)
{
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetNotChanged,
			    "the box did not change the bouquet");
	return writeOut();
}

/* Every call below ends by reading its own change back, because everything up to
   here answers for the sending and not for the doing: the box takes each command
   off a socket and acts on it afterwards, so the one failure none of the answers
   above can see is the command that arrived and did nothing.

   What the reading back cannot see is the file the list was written to. It reads
   the list the box is holding, and a box that could not keep the file looks from
   here exactly like one that did.

   Each call gets the read that fits it rather than one read for all seven,
   because the same list satisfies a rename and fails a move. */

// The list read back is not the one that was asked for. The code is the one a
// refusal already answers with: what a caller is told is that the bouquet is not
// what it asked for.
Result<void> notChanged()
{
	return fail(Status::Internal, ErrorCode::BouquetNotChanged,
		    "the box took the change and the bouquet list does not carry it");
}

// And the list could not be read at all, which says nothing about what the box
// did with the change and must not be answered as though it had.
Result<void> unreadable(Status s)
{
	return fail(s, ErrorCode::BouquetListUnavailable,
		    "the bouquet list could not be read");
}

// Whether the list carries that name now. The same lookup position() makes and
// not a call of it, because a name nothing carries is an answer here and a
// refusal there.
Result<void> probeName(const std::string &name, bool wanted)
{
	uint32_t id = 0;
	const Status s = channelSource().bouquetByName(name, id);
	if (s == Status::Ok)
		return wanted ? ok() : notChanged();
	if (s == Status::NotFound)
		return wanted ? notChanged() : ok();
	return unreadable(s);
}

// Where the list carries it now, which is the whole of what a move is: the
// name is in the list either way and only the place it sits at moved.
Result<void> probePosition(const std::string &name, uint32_t id)
{
	uint32_t now = 0;
	const Status s = channelSource().bouquetByName(name, now);
	if (s == Status::NotFound)
		return notChanged();
	if (s != Status::Ok)
		return unreadable(s);
	return (now == id) ? ok() : notChanged();
}

// Which of the two states a bouquet carries. One read for both of them,
// because they are a bit each on one record and differ only in which bit.
enum class Flag { Hidden, Locked };

Result<void> probeFlag(const std::string &name, Flag which, bool on)
{
	uint32_t id = 0;
	Status s = channelSource().bouquetByName(name, id);
	if (s == Status::NotFound)
		return notChanged();
	if (s != Status::Ok)
		return unreadable(s);

	BouquetList all;
	s = channelSource().allBouquets(all);
	if (s != Status::Ok)
		return unreadable(s);

	// Taken at the position the lookup answered with rather than found by
	// walking the list for the name: a bouquet keeps its identity and the
	// caption it prints in two fields, and only that lookup knows both.
	if (id == 0 || (size_t) id > all.size())
		return notChanged();

	const BouquetInfo &one = all[id - 1];
	const bool now = (which == Flag::Hidden) ? one.hidden : one.locked;
	return (now == on) ? ok() : notChanged();
}

// Mirrors the channel stack's own split, where everything that is not radio
// counts as television.
bool isTelevision(ServiceKind k)
{
	return k != ServiceKind::Radio && k != ServiceKind::WebRadio;
}

/* The half the list was written to, whole. Compared as a sequence and not as a
   set, because the box counts the numbers it draws beside a channel off that
   order, so a read that only asked which channels are in the half would pass a
   write that put them in and lost the order. */
Result<void> probeChannels(const std::string &name, const ChannelIdList &ids, Mode kind)
{
	uint32_t id = 0;
	Status s = channelSource().bouquetByName(name, id);
	if (s == Status::NotFound)
		return notChanged();
	if (s != Status::Ok)
		return unreadable(s);

	ChannelList now;
	s = channelSource().bouquetChannels(id, now);
	if (s == Status::NotFound)
		return notChanged();
	if (s != Status::Ok)
		return unreadable(s);

	ChannelIdList half;
	half.reserve(now.size());
	for (size_t i = 0; i < now.size(); i++)
		if (isTelevision(now[i].kind) == (kind == Mode::Tv))
			half.push_back(now[i].id);
	return (half == ids) ? ok() : notChanged();
}

} // namespace

Result<void> addBouquet(const std::string &name)
{
	uint32_t taken = 0;
	Status s = channelSource().bouquetByName(name, taken);
	if (s == Status::Ok)
		return fail(Status::Conflict, ErrorCode::NameTaken,
			    "a bouquet of that name is already there");
	if (s != Status::NotFound)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	const Result<void> made = changed(channelSource().addBouquet(name));
	if (!made.ok())
		return made;
	return probeName(name, true);
}

Result<void> renameBouquet(const std::string &from, const std::string &to)
{
	Result<uint32_t> at = position(from);
	if (!at.ok())
		return fail(at.error());

	// Asked before the rename and not after, so a name something else carries
	// leaves both bouquets as they were. Renaming to the name it already has is
	// refused by this as well, which is the honest answer: the name is taken.
	uint32_t taken = 0;
	Status s = channelSource().bouquetByName(to, taken);
	if (s == Status::Ok)
		return fail(Status::Conflict, ErrorCode::NameTaken,
			    "a bouquet of that name is already there");
	if (s != Status::NotFound)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	const Result<void> done = changed(channelSource().renameBouquet(at.value(), to));
	if (!done.ok())
		return done;

	/* Both ends of it, because each answers a failure the other passes: a rename
	   that landed on the bouquet beside the one it was meant for leaves the new
	   name in the list with the old one still beside it, and one whose name
	   arrived cut short leaves the old name gone and the new one nowhere. */
	const Result<void> there = probeName(to, true);
	if (!there.ok())
		return there;
	return probeName(from, false);
}

Result<void> moveBouquet(const std::string &name, Direction d)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	// How long the list is, because the last bouquet has nowhere below it and
	// the source is told a position rather than a direction.
	BouquetList all;
	Status s = channelSource().allBouquets(all);
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	const uint32_t id = at.value();
	const bool up = (d == Direction::Up);
	if (up ? (id <= 1) : (id >= (uint32_t) all.size()))
		return fail(Status::InvalidArgument, ErrorCode::AlreadyAtTheEnd,
			    "the bouquet is already at that end of the list");

	const uint32_t to = up ? id - 1 : id + 1;
	const Result<void> done = changed(channelSource().moveBouquet(id, to));
	if (!done.ok())
		return done;
	return probePosition(name, to);
}

Result<void> deleteBouquet(const std::string &name)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	const Result<void> done = changed(channelSource().deleteBouquet(at.value()));
	if (!done.ok())
		return done;
	return probeName(name, false);
}

Result<void> setBouquetHidden(const std::string &name, bool hidden)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	const Result<void> done = changed(channelSource().setBouquetHidden(at.value(), hidden));
	if (!done.ok())
		return done;
	return probeFlag(name, Flag::Hidden, hidden);
}

Result<void> setBouquetLock(const std::string &name, bool locked)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	const Result<void> done = changed(channelSource().setBouquetLock(at.value(), locked));
	if (!done.ok())
		return done;
	return probeFlag(name, Flag::Locked, locked);
}

Result<void> setBouquetChannels(const std::string &name, const ChannelIdList &ids, Mode kind)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	/* Every one of them before any of them, because what follows replaces what
	   the bouquet held: a list checked as it was applied would leave the bouquet
	   holding the part of it that was good while the caller is told it failed. */
	for (size_t i = 0; i < ids.size(); i++)
	{
		ChannelInfo probe;
		Status s = channelSource().findChannel(ids[i], probe);
		if (s == Status::NotFound)
			return fail(s, ErrorCode::NoSuchChannel,
				    "no channel with that id");
		if (s != Status::Ok)
			return fail(s, ErrorCode::ChannelListUnavailable,
				    "the channel list could not be read");
	}

	const Result<void> done =
		changed(channelSource().setBouquetChannels(at.value(), ids, kind == Mode::Tv));
	if (!done.ok())
		return done;
	return probeChannels(name, ids, kind);
}

Result<void> announceBouquetsChanged()
{
	/* The wait first. None of the writes the box is given is answered by the
	   box, so the call that sent one comes back while the box still has it in
	   front of it, and a reader woken any earlier would read the bouquet in the
	   middle of being renumbered: the numbers handed back in and the bouquet
	   holding everything no other one holds emptied and refilling. */
	Status s = channelSource().awaitCommands();
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetNotChanged,
			    "the change did not reach the box");

	/* It names no bouquet. What a change does is not confined to the bouquet it
	   was made about, because a move renumbers its neighbour and a delete moves
	   everything below it. */
	Event e;
	e.type = EventType::BouquetsChanged;
	EventBus::instance().publish(e);
	return ok();
}

} // namespace channels
} // namespace coreapi
