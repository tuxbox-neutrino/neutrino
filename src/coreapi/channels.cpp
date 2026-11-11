/*
 * channels.cpp - channels, bouquets and zapping, above the zapit client
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, fixes)
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

#include <config.h>

#include "channels.h"
#include "coreapi/archive.h"
#include "coreapi/playback.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/deps.h"
#include "coreapi/base/eventbus.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <neutrinoMessages.h>
#include <timerdclient/timerdtypes.h>
#include <zapit/types.h>

#include <xmlinterface.h>

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

namespace
{

// A mode that cannot be read is not standby.
Result<void> somethingCanPlay()
{
	int m = 0;
	if (channelSource().currentMode(m) == Status::Ok &&
	    m == NeutrinoModes::mode_standby)
		return fail(Status::NotFound, ErrorCode::NoRunningChannel,
			    "nothing is playing, the box is in standby");
	return ok();
}

} // anonymous namespace

Result<ChannelInfo> playing()
{
	Result<void> awake = somethingCanPlay();
	if (!awake.ok())
		return fail(awake.error());
	// The channel stack still names the last channel while the player shows a file.
	const playback::Snapshot shown = playback::snapshot();
	if (shown.active && !shown.timeshift)
		return fail(Status::NotFound, ErrorCode::NoRunningChannel,
			    "no channel is playing, the box plays a recording or a file; GET /api/v1/playback says which");
	return current();
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

/* Held around every save of its channel files this layer asks the box for, and
   around putting a file in place up to the reading that takes it in: a save of
   the lists the box holds that fell between the two would write them over the
   file just put there, and the reading would read them back. A function's own,
   so this file carries no object whose construction order matters. Never held
   by the box's own threads, so waiting for the box while holding it waits for
   nobody who waits for it. */
OpenThreads::Mutex &filesLock()
{
	static OpenThreads::Mutex lock;
	return lock;
}

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
	Result<void> awake = somethingCanPlay();
	if (!awake.ok())
		return fail(awake.error());

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
	OpenThreads::ScopedLock<OpenThreads::Mutex> files(filesLock());
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

/* Here and not in the main loop, which cannot tell a web zap from a zap timer's.
   An unreadable mode refuses, or it could wake a box nobody said may be woken. */
Result<void> standbyAllows(bool wake)
{
	int m = 0;
	Status s = channelSource().currentMode(m);
	// No mode yet is a box starting up.
	if (s == Status::NotFound)
		return ok();
	if (s != Status::Ok)
		return fail(s, ErrorCode::ModeUnavailable,
			    "the box mode could not be read");
	if (m == NeutrinoModes::mode_standby && !wake)
		return fail(Status::Conflict, ErrorCode::BoxInStandby,
			    "the box is in standby");
	return ok();
}

Result<void> zap(ChannelId id, bool wake, bool stop_playback)
{
	ChannelInfo probe;
	Status s = channelSource().findChannel(id, probe);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchChannel,
			    "no channel with that id");
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");

	// Before standby: waking the box would not free the tuner.
	bool possible = false;
	s = channelSource().canZap(id, possible);
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the channel list could not be read");
	if (!possible)
		return fail(Status::Conflict, ErrorCode::RecordingHoldsTuner,
			    "a recording holds the tuner this channel needs");

	// The loop ends a file playback for a zap on its own, so leave is all this adds.
	Result<void> free = archive::playbackAllows(stop_playback);
	if (!free.ok())
		return free;

	// The loop zaps through channel lists the box builds only once it has a mode.
	int box_mode = 0;
	if (channelSource().currentMode(box_mode) == Status::NotFound)
		return fail(Status::Conflict, ErrorCode::ModeUnavailable, "the box has not finished starting");

	Result<void> allowed = standbyAllows(wake);
	if (!allowed.ok())
		return allowed;

	// Zeroed and not merely filled in, because the loop reads a whole event
	// out of this message and only one of its fields is this command's.
	CTimerd::EventInfo event = CTimerd::EventInfo();
	event.channel_id = (t_channel_id) id;

	return postPayload(NeutrinoMessages::ZAPTO, &event, sizeof(event));
}

Result<void> setMode(Mode m, bool wake)
{
	// The loop zaps for a mode change as well, through the same lists as zap().
	int box_mode = 0;
	if (channelSource().currentMode(box_mode) == Status::NotFound)
		return fail(Status::Conflict, ErrorCode::ModeUnavailable, "the box has not finished starting");

	Result<void> allowed = standbyAllows(wake);
	if (!allowed.ok())
		return allowed;

	// The mode alone. The flag that can ride beside it would leave the running
	// channel where it is, which is not what asking for a mode means.
	// wakeup always, so a box that went into standby since the check wakes whole.
	neutrino_msg_data_t value = (m == Mode::Radio) ? NeutrinoModes::mode_radio
						       : NeutrinoModes::mode_tv;

	return postCommand(NeutrinoMessages::CHANGEMODE, value | NeutrinoModes::wakeup);
}

namespace
{

/* The position the source addresses that name by, for a call that changes the
   bouquet. One place, so that no two of the calls below can disagree about
   which bouquet a name means, and so that the answers for a name nothing
   carries and for a bouquet the box builds itself are written once. Such a
   bouquet is refused before anything is sent: the box writes it to none of its
   files and makes it again, so a change to it would not last. */
Result<uint32_t> position(const std::string &name)
{
	uint32_t id = 0;
	Status s = channelSource().bouquetByName(name, id);
	if (s == Status::NotFound)
		return fail(s, ErrorCode::NoSuchBouquet,
			    "no bouquet of that name");
	if (s == Status::Conflict)
		return fail(s, ErrorCode::NameShared,
			    "more than one bouquet carries that name; rename one of them on the box first");
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	/* Asked of the position the name resolved to and not of the name: a
	   bouquet answers to its name inside the box as well as to the one it
	   shows, and the list carries only the second. */
	BouquetList all;
	s = channelSource().allBouquets(all);
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");
	if (id >= 1 && id <= all.size() && !all[id - 1].editable)
		return fail(Status::Conflict, ErrorCode::BouquetNotEditable,
			    "the box builds this bouquet itself, so a change to it would not last");
	return ok(id);
}

/* What every write below ends with, and what makes looking for the change
   afterwards worth anything.

   A change is in memory until the list is written out. The box takes the
   commands in the order they were sent, so the save finds the change made, and
   after answering it numbers the lists again and tells the screen; the second
   call is there for a source whose save does not, and sends nothing on the box.
   Neither answer covers the numbering, which is why the third call is the one
   that waits before anything is read back. */
Result<void> writeOut()
{
	/* Held to the end, the wait included: the box answers the save before it is
	   through with it, and writes the services after that answer where a
	   channel left its last bouquet. Only the wait at the end says it is done. */
	OpenThreads::ScopedLock<OpenThreads::Mutex> files(filesLock());
	Status s = channelSource().saveBouquets();
	if (s != Status::Ok)
	{
		/* Said all the same, and answered with a code of its own. The box
		   carries a change out before it is asked to save it, and numbers its
		   lists again and tells the screen after a save that failed as well,
		   so the list it runs has the change and a page over the network
		   holds that list. Not the code for a change that did not happen: a
		   caller told that makes it again, and a second add finds the name
		   taken, a second rename finds the old name gone, and a second move
		   moves the bouquet on by another place. Whether the list really
		   carries the change is read back by the caller, as after a save that
		   went through. A box that cannot be waited on afterwards is answered
		   as one that cannot be waited on after a good save: the change is not
		   known to have been carried out, and nothing is said. */
		const Result<void> said = announceBouquetsChanged();
		if (!said.ok())
			return said;
		return fail(s, ErrorCode::BouquetNotSaved,
			    "saving the bouquet files failed: the list the box runs carries "
			    "the change, the files may not");
	}
	s = channelSource().reloadBouquets();
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetNotChanged,
			    "the change is saved, but the box did not number its lists again");

	/* Said here and not at any of the seven, for the reason the writing out is
	   here. The wait that makes it true is the announcing call's own. */
	return announceBouquetsChanged();
}

// What a write of the bouquet itself answers when the box turns it down: the
// code for a change not made, as is a change the list read back does not carry.
// A save that failed has a code of its own; both are stated where they are
// declared.
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

// Whether the change is to be looked for in the list: after a write that went
// through, and after one whose save alone failed, the box carrying the change
// either way.
bool lookFor(const Result<void> &done)
{
	return done.ok() || done.error().code == ErrorCode::BouquetNotSaved;
}

// What a call answers once the change was looked for: what the writing out
// answered when the list carries the change, and what the reading found when
// it does not, a save that failed on a change the box never made being a change
// not made.
Result<void> settled(const Result<void> &done, const Result<void> &there)
{
	return there.ok() ? done : there;
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

/* Each channel once, at the first place the list names it. A bouquet file can
   name a channel twice and the box then holds both, while the listing answers
   each channel once, so this is the half as a caller reads it. */
ChannelIdList onceEach(const ChannelIdList &list)
{
	ChannelIdList out;
	out.reserve(list.size());
	std::set<ChannelId> seen;
	for (size_t i = 0; i < list.size(); i++)
		if (seen.insert(list[i]).second)
			out.push_back(list[i]);
	return out;
}

/* The half the list was written to, whole. Compared as a sequence and not as a
   set, because the box counts the numbers it draws beside a channel off that
   order, so a read that only asked which channels are in the half would pass a
   write that put them in and lost the order.

   Compared as the listing answers it, each channel once. A half that held a
   channel twice keeps the second one: the list names the channel, so it is not
   taken out, and taking it out would take out both and could take the channel
   off the box. The moves put it behind the list, and a reading back held to
   every entry found the half longer than the list and answered that nothing
   had changed, for every list sent to such a bouquet. */
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
	return (onceEach(half) == ids) ? ok() : notChanged();
}

/* What the list costs the box: the channels the half holds now that the list
   leaves out and that no other bouquet holds. The channel stack forgets a channel
   that leaves the last bouquet holding it, so these would be gone from the box.
   Every bouquet counts as holding one, the provider lists and a hidden bouquet
   too, as they do there. */
Status leftAlone(uint32_t at, const ChannelIdList &ids, Mode kind, ChannelIdList &out)
{
	// A bouquet the source no longer knows holds nothing, here and below.
	ChannelList held;
	Status s = channelSource().bouquetChannels(at, held);
	if (s != Status::Ok && s != Status::NotFound)
		return s;

	// Once each, however often the bouquet file names it.
	ChannelIdList leaving;
	for (size_t i = 0; i < held.size(); i++)
		if (isTelevision(held[i].kind) == (kind == Mode::Tv) &&
		    std::find(ids.begin(), ids.end(), held[i].id) == ids.end() &&
		    std::find(leaving.begin(), leaving.end(), held[i].id) == leaving.end())
			leaving.push_back(held[i].id);
	if (leaving.empty())
		return Status::Ok;

	BouquetList all;
	s = channelSource().allBouquets(all);
	if (s != Status::Ok)
		return s;
	for (size_t b = 0; b < all.size() && !leaving.empty(); b++)
	{
		if (all[b].id == at)
			continue;
		ChannelList members;
		s = channelSource().bouquetChannels(all[b].id, members);
		if (s == Status::NotFound)
			continue;
		if (s != Status::Ok)
			return s;
		for (size_t m = 0; m < members.size(); m++)
			leaving.erase(std::remove(leaving.begin(), leaving.end(), members[m].id),
				      leaving.end());
	}
	out = leaving;
	return Status::Ok;
}

// Says which ones, the first few of them, in the form the routes write an id in.
std::string removalDetail(const ChannelIdList &alone)
{
	std::string d = "no other bouquet holds ";
	const size_t shown = (alone.size() < 8) ? alone.size() : 8;
	for (size_t i = 0; i < shown; i++)
	{
		char hex[24];
		std::snprintf(hex, sizeof(hex), "%llx", (unsigned long long) alone[i]);
		d += (i == 0) ? "" : ", ";
		d += hex;
	}
	if (alone.size() > shown)
		d += " and more";
	d += (alone.size() == 1)
		? ", so the box would forget it; send the list again with remove_from_box to take it off the box"
		: ", so the box would forget them; send the list again with remove_from_box to take them off the box";
	return d;
}

} // namespace

Result<void> addBouquet(const std::string &name)
{
	uint32_t taken = 0;
	Status s = channelSource().bouquetByName(name, taken);
	if (s == Status::Ok || s == Status::Conflict)
		return fail(Status::Conflict, ErrorCode::NameTaken,
			    "a bouquet of that name is already there");
	if (s != Status::NotFound)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	const Result<void> made = changed(channelSource().addBouquet(name));
	if (!lookFor(made))
		return made;
	return settled(made, probeName(name, true));
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
	if (s == Status::Ok || s == Status::Conflict)
		return fail(Status::Conflict, ErrorCode::NameTaken,
			    "a bouquet of that name is already there");
	if (s != Status::NotFound)
		return fail(s, ErrorCode::BouquetListUnavailable,
			    "the bouquet list could not be read");

	const Result<void> done = changed(channelSource().renameBouquet(at.value(), to));
	if (!lookFor(done))
		return done;

	/* Both ends of it, because each answers a failure the other passes: a rename
	   that landed on the bouquet beside the one it was meant for leaves the new
	   name in the list with the old one still beside it, and one whose name
	   arrived cut short leaves the old name gone and the new one nowhere. */
	const Result<void> there = probeName(to, true);
	if (!there.ok())
		return there;
	return settled(done, probeName(from, false));
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
	if (!lookFor(done))
		return done;
	return settled(done, probePosition(name, to));
}

Result<void> deleteBouquet(const std::string &name)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	const Result<void> done = changed(channelSource().deleteBouquet(at.value()));
	if (!lookFor(done))
		return done;
	return settled(done, probeName(name, false));
}

Result<void> setBouquetHidden(const std::string &name, bool hidden)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	const Result<void> done = changed(channelSource().setBouquetHidden(at.value(), hidden));
	if (!lookFor(done))
		return done;
	return settled(done, probeFlag(name, Flag::Hidden, hidden));
}

Result<void> setBouquetLock(const std::string &name, bool locked)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	const Result<void> done = changed(channelSource().setBouquetLock(at.value(), locked));
	if (!lookFor(done))
		return done;
	return settled(done, probeFlag(name, Flag::Locked, locked));
}

Result<void> setBouquetChannels(const std::string &name, const ChannelIdList &asked, Mode kind,
                                bool remove_from_box)
{
	Result<uint32_t> at = position(name);
	if (!at.ok())
		return fail(at.error());

	/* A channel named twice once, where it is named first, as the route already
	   reads a body. The box would put it in twice, the half being read before
	   anything is sent. */
	const ChannelIdList ids = onceEach(asked);

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
		/* The box files a channel by its own kind and not by the half it is
		   named for, so one of the other kind would go into the other half
		   and the half named here would come out without it. */
		if (isTelevision(probe.kind) != (kind == Mode::Tv))
			return fail(Status::InvalidArgument, ErrorCode::WrongKindOfChannel,
				    kind == Mode::Tv ? "a radio channel cannot go into the television list"
						     : "a television channel cannot go into the radio list");
	}

	/* Asked before anything changes as well, so that a refusal leaves the
	   bouquet as it was. Without this a page that only meant to tidy a bouquet
	   could take a channel off the box for good, which nothing but a scan brings
	   back. */
	if (!remove_from_box)
	{
		ChannelIdList alone;
		Status s = leftAlone(at.value(), ids, kind, alone);
		if (s != Status::Ok)
			return unreadable(s);
		if (!alone.empty())
			return fail(Status::Conflict, ErrorCode::ChannelWouldBeRemoved,
				    removalDetail(alone));
	}

	const Result<void> done =
		changed(channelSource().setBouquetChannels(at.value(), ids, kind == Mode::Tv));
	if (!lookFor(done))
		return done;
	return settled(done, probeChannels(name, ids, kind));
}

Result<void> announceBouquetsChanged()
{
	/* The wait first. Of what it is given the box answers only the save, as
	   soon as the files are out, and it numbers the lists again after that
	   answer, so the call that sent the last command comes back while the box
	   still has work of it in front of it. A reader woken any earlier would
	   find the change with the numbering and the bouquet of everything no
	   other bouquet holds from before it, and go away with that. */
	Status s = channelSource().awaitCommands();
	if (s != Status::Ok)
		return fail(s, ErrorCode::BouquetNotChanged,
			    "the change did not reach the box");

	publishBouquetsChanged();
	return ok();
}

void publishBouquetsChanged()
{
	/* It names no bouquet. What a change does is not confined to the bouquet it
	   was made about, because a move renumbers its neighbour and a delete moves
	   everything below it. */
	Event e;
	e.type = EventType::BouquetsChanged;
	EventBus::instance().publish(e);
}


namespace internal
{

const char *upload_dir = "/tmp";
const char *zapit_dir = ZAPITDIR;

bool filesLockTaken()
{
	if (filesLock().trylock() != 0)
		return true;
	filesLock().unlock();
	return false;
}

} // namespace internal

namespace
{

// The four files by name, each with whether it is one of the two that list services.
struct ChannelFileName
{
	ChannelFile which;
	const char *name;
	bool services;
};

const ChannelFileName kChannelFiles[] = {
	{ ServicesFile,     "services.xml",   true },
	{ MyServicesFile,   "myservices.xml", true },
	{ BouquetsFile,     "bouquets.xml",   false },
	{ UserBouquetsFile, "ubouquets.xml",  false },
};

bool letterOrDigit(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// A name in the upload directory and nothing more: no way out of it, no hidden name.
bool plainUpload(const std::string &file)
{
	if (file.size() < 5 || file.size() > 128 || !letterOrDigit(file[0]))
		return false;
	for (size_t i = 0; i < file.size(); ++i)
	{
		const char c = file[i];
		if (!letterOrDigit(c) && c != '.' && c != '_' && c != '-')
			return false;
	}
	return file.compare(file.size() - 4, 4, ".xml") == 0;
}

enum class Copy { Done, TooLarge, Broken };

// All of in written to out, as long as that is no more than most bytes.
Copy copyAtMost(int in, int out, size_t most)
{
	std::vector<char> buf(65536);
	size_t total = 0;
	for (;;)
	{
		const ssize_t n = read(in, &buf[0], buf.size());
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
			return Copy::Broken;
		if (n == 0)
			return Copy::Done;
		total += (size_t) n;
		if (total > most)
			return Copy::TooLarge;
		ssize_t done = 0;
		while (done < n)
		{
			const ssize_t w = write(out, &buf[done], n - done);
			if (w < 0 && errno == EINTR)
				continue;
			if (w <= 0)
				return Copy::Broken;
			done += w;
		}
	}
}

/* The upload, copied to to, a name of this program's own. Opened without
   following a link and without waiting, so neither a link nor a pipe put in the
   upload's place after it was looked at is read, and refused unless it is a
   regular file. */
Copy takeOver(const std::string &from, const std::string &to)
{
	const int in = open(from.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (in < 0)
		return Copy::Broken;
	struct stat st;
	if (fstat(in, &st) != 0 || !S_ISREG(st.st_mode))
	{
		close(in);
		return Copy::Broken;
	}
	const int out = open(to.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (out < 0)
	{
		close(in);
		return Copy::Broken;
	}
	Copy c = copyAtMost(in, out, kMaxChannelFileBytes);
	close(in);
	if (close(out) != 0 && c == Copy::Done)
		c = Copy::Broken;
	return c;
}

// The directory the upload was copied into, with the copy.
void removeStaging(const std::string &dir, const std::string &copy)
{
	unlink(copy.c_str());
	rmdir(dir.c_str());
}

/* Whether the box's own reader takes the file and it is a list of the kind its
   name says: the root element zapit, holding only the elements the box reads
   out of such a file. Anything else would read as no channels or no bouquets
   at all, once the box's own file is gone. A child without a name is text
   between the elements, which the box passes over as well. Each sat, cable and
   terrestrial element has to carry its name, as the box's own files do: the
   reader makes a string of a cable's or a terrestrial's unasked, and one
   without would bring the box down on every start. Written for both trees the
   reader can be built on, the tree of its own and pugixml, whose node is a
   value and not a pointer. */
bool channelFileOk(const std::string &path, bool services, std::string &why)
{
	xmlDocPtr doc = parseXmlFile(path.c_str(), false);
	if (doc == NULL)
	{
		why = "the file is no XML the box can read";
		return false;
	}
	bool ok = true;
	const xmlNodePtr root = xmlDocGetRootElement(doc);
	const char *top = root ? xmlGetName(root) : NULL;
	if (top == NULL || std::strcmp(top, "zapit") != 0)
	{
		why = "the file's root element is not zapit";
		ok = false;
	}
	for (xmlNodePtr n = ok ? xmlChildrenNode(root) : xmlNodePtr(); ok && n; n = xmlNextNode(n))
	{
		const char *name = xmlGetName(n);
		if (name == NULL || *name == '\0')
			continue;
		if (services)
		{
			if (std::strcmp(name, "sat") != 0 && std::strcmp(name, "cable") != 0 &&
			    std::strcmp(name, "terrestrial") != 0)
			{
				why = std::string("the file is no list of services: it holds a ") + name +
				      " element where only sat, cable and terrestrial belong";
				ok = false;
			}
			else if (xmlGetAttribute(n, "name") == NULL)
			{
				why = std::string("the file has a ") + name + " element without the name the box looks it up by";
				ok = false;
			}
		}
		else if (std::strcmp(name, "Bouquet") != 0)
		{
			why = std::string("the file is no list of bouquets: it holds a ") + name +
			      " element where only Bouquet belongs";
			ok = false;
		}
	}
	xmlFreeDoc(doc);
	return ok;
}

enum class Placed { InPlace, NotDurable, Untouched };

/* The checked copy put in place of the box's file: written beside it under a
   hidden name, on the disc before the rename, with the mode the box's own
   files have, and renamed over it, so a reader finds the old file or the new
   one. The rename replaces a link in the file's place and does not follow it.
   NotDurable for a file in place whose rename the disc did not confirm, which
   is still to be read in; Untouched where the box's file is as it was. */
Placed putInPlace(const std::string &copy, const std::string &dir, const char *name)
{
	const int d = open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (d < 0)
		return Placed::Untouched;
	const int in = open(copy.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (in < 0)
	{
		close(d);
		return Placed::Untouched;
	}
	std::string pattern = dir + "/." + name + ".XXXXXX";
	std::vector<char> buf(pattern.begin(), pattern.end());
	buf.push_back('\0');
	const int out = mkstemp(&buf[0]);
	if (out < 0)
	{
		close(in);
		close(d);
		return Placed::Untouched;
	}
	fcntl(out, F_SETFD, FD_CLOEXEC);
	const std::string beside(&buf[0]);
	bool ok = copyAtMost(in, out, kMaxChannelFileBytes) == Copy::Done;
	close(in);
	ok = ok && fchmod(out, 0644) == 0 && fsync(out) == 0;
	ok = close(out) == 0 && ok;
	ok = ok && rename(beside.c_str(), (dir + "/" + name).c_str()) == 0;
	if (!ok)
	{
		unlink(beside.c_str());
		close(d);
		return Placed::Untouched;
	}
	// the rename on the disc as well
	const bool durable = fsync(d) == 0;
	close(d);
	return durable ? Placed::InPlace : Placed::NotDurable;
}

} // namespace

Result<void> putChannelFile(ChannelFile which, const std::string &upload)
{
	const ChannelFileName *file = NULL;
	for (size_t i = 0; i < sizeof(kChannelFiles) / sizeof(kChannelFiles[0]); i++)
		if (kChannelFiles[i].which == which)
			file = &kChannelFiles[i];
	if (file == NULL)
		return fail(Status::InvalidArgument, ErrorCode::BadName, "there is no such channel file");
	if (!plainUpload(upload))
		return fail(Status::InvalidArgument, ErrorCode::BadName,
			    "the upload is a plain name in the temporary directory, of letters, digits, dots, "
			    "dashes and underscores, ending in .xml");
	const std::string from = std::string(internal::upload_dir) + "/" + upload;
	struct stat st;
	if (lstat(from.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		return fail(Status::NotFound, ErrorCode::NoSuchName, "there is no uploaded file of that name");

	std::string pattern = std::string(internal::upload_dir) + "/channelfile.XXXXXX";
	std::vector<char> buf(pattern.begin(), pattern.end());
	buf.push_back('\0');
	if (mkdtemp(&buf[0]) == NULL)
		return fail(Status::Internal, ErrorCode::ChannelFileRefused, "no directory to check the file in");
	const std::string dir(&buf[0]);
	const std::string copy = dir + "/" + file->name;
	const Copy taken = takeOver(from, copy);
	unlink(from.c_str());
	if (taken == Copy::TooLarge)
	{
		removeStaging(dir, copy);
		return fail(Status::InvalidArgument, ErrorCode::ChannelFileRefused, "the file is larger than 16 MiB");
	}
	if (taken != Copy::Done)
	{
		removeStaging(dir, copy);
		return fail(Status::Internal, ErrorCode::ChannelFileRefused, "the upload could not be taken over");
	}

	std::string why;
	if (!channelFileOk(copy, file->services, why))
	{
		removeStaging(dir, copy);
		return fail(Status::InvalidArgument, ErrorCode::ChannelFileRefused, why);
	}

	OpenThreads::ScopedLock<OpenThreads::Mutex> files(filesLock());
	/* The box through with every command before this one first: it answers a
	   save before it is done writing, and a save whose own wait went unanswered
	   gave the lock back while that writing may still be under way. */
	if (channelSource().awaitCommands() != Status::Ok)
	{
		removeStaging(dir, copy);
		return fail(Status::Internal, ErrorCode::ChannelFileRefused,
			    "the box's channel manager did not answer, so the file was not put in place");
	}
	const Placed placed = putInPlace(copy, internal::zapit_dir, file->name);
	removeStaging(dir, copy);
	if (placed == Placed::Untouched)
		return fail(Status::Internal, ErrorCode::ChannelFileRefused,
			    std::string("the file could not be put in place of the box's ") + file->name +
			    ", which is as it was");
	const Status s = channelSource().reloadChannels(false, kHardReload);
	if (s != Status::Ok)
		return fail(s, ErrorCode::ChannelListUnavailable,
			    "the file is in place, and the box did not read its channel lists again");
	if (placed == Placed::NotDurable)
		return fail(Status::Internal, ErrorCode::ChannelFileRefused,
			    "the file is in place and the box has read it, but the disc did not confirm the "
			    "change, which a power cut may undo");
	return ok();
}

} // namespace channels
} // namespace coreapi
