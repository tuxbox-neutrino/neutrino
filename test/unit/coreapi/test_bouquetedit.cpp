/*
 * test_bouquetedit.cpp - tests for editing one bouquet
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

#include "support/catch.hpp"
#include "support/fakes.h"
#include "coreapi/channels.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/eventbus.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace coreapi;

namespace
{
// Ids as the box hands them out. The satellite position and the transport id
// live above bit 31, so a 32 bit lookup would name a different channel.
const ChannelId ERSTE  = 0xb9b0040200016dcbULL;
const ChannelId ZDF    = 0xbdf8040300017a01ULL;
const ChannelId DLF    = 0xb9b004020001780fULL;
// Shares its low half with ERSTE and differs only above bit 31.
const ChannelId ABSENT = 0x1e24040200016dcbULL;

ChannelInfo mk(ChannelId id, const char *name, ServiceKind kind)
{
	ChannelInfo c;
	c.id = id;
	c.name = name;
	c.kind = kind;
	return c;
}

BouquetInfo bq(uint32_t id, const char *name)
{
	BouquetInfo b;
	b.id = id;
	b.name = name;
	b.user_bouquet = true;
	return b;
}

/* Three bouquets and three channels, with the middle bouquet holding one of
   each kind, so that a call naming one half can be seen to leave the other
   alone. Put back from a destructor for the reason every fixture here has one:
   a check that fails unwinds past a last line, and the case after it would be
   reading a source that is gone. */
struct Edit
{
	FakeChannelSource      fake;
	InstalledChannelSource installed;

	Edit() : installed(&fake)
	{
		fake.channels.push_back(mk(ERSTE, "Das Erste HD", ServiceKind::Tv));
		fake.channels.push_back(mk(ZDF, "ZDF HD", ServiceKind::Tv));
		fake.channels.push_back(mk(DLF, "Deutschlandfunk", ServiceKind::Radio));

		fake.bouquets.push_back(bq(1, "Erstes Bouquet"));
		fake.bouquets.push_back(bq(2, "Favoriten"));
		fake.bouquets.push_back(bq(3, "Drittes Bouquet"));

		fake.bouquet_members[2].push_back(fake.channels[0]);
		fake.bouquet_members[2].push_back(fake.channels[2]);
	}

	private:
		Edit(const Edit &);
		Edit &operator=(const Edit &);
};

// Whether the list holds that channel, so a case says which channel it means
// rather than which position it happens to sit at.
bool holds(const ChannelList &all, ChannelId id)
{
	for (size_t i = 0; i < all.size(); i++)
		if (all[i].id == id)
			return true;
	return false;
}

ChannelIdList one(ChannelId id)
{
	ChannelIdList out;
	out.push_back(id);
	return out;
}

// The television members of a half, in the order the list carries them, so
// that a case about order compares one sequence against another rather than
// reading positions out of a list that also holds the radio members.
ChannelIdList televisionOf(const ChannelList &all)
{
	ChannelIdList out;
	for (size_t i = 0; i < all.size(); i++)
		if (all[i].kind == ServiceKind::Tv)
			out.push_back(all[i].id);
	return out;
}

// The one answer a call that changed nothing may not give, written once so
// that a case saying it about all seven says the same thing about each of them
// rather than about whichever one it got round to.
void notDone(const char *which, const Result<void> &r)
{
	INFO(which);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::BouquetNotChanged);
}
}

TEST_CASE("adding a bouquet that exists is a conflict", "[bouquetedit]")
{
	Edit e;

	REQUIRE(channels::addBouquet("Viertes Bouquet").ok());
	const Result<void> again = channels::addBouquet("Viertes Bouquet");
	REQUIRE_FALSE(again.ok());
	REQUIRE(again.error().status == Status::Conflict);
	REQUIRE(again.error().code == ErrorCode::NameTaken);

	// The refusal is worth nothing if it is written after the fact: a second
	// bouquet under a name that is taken is what the name being a key means.
	REQUIRE(e.fake.bouquets.size() == 4);
}

TEST_CASE("every editing call writes and reloads", "[bouquetedit]")
{
	/* The fake counts them, because a caller must not be able to forget. This
	   is the one place that sees a call which changed the list and stopped
	   there, and that is the failure the interface this replaces left to every
	   client of it: the change is in memory, the file still says what it said,
	   and the next start throws the work away with nothing to say so. */
	Edit e;
	FakeChannelSource &f = e.fake;
	const unsigned before = f.saves;

	REQUIRE(channels::addBouquet("Neu").ok());
	REQUIRE(channels::renameBouquet("Neu", "Neuer").ok());
	REQUIRE(channels::moveBouquet("Neuer", channels::Direction::Up).ok());
	REQUIRE(channels::setBouquetHidden("Neuer", true).ok());
	REQUIRE(channels::setBouquetLock("Neuer", true).ok());
	REQUIRE(channels::setBouquetChannels("Neuer", one(ZDF), channels::Mode::Tv).ok());
	REQUIRE(channels::deleteBouquet("Neuer").ok());

	// All seven and not a sample of them, because the number is what says
	// which of the calls is the one that forgot.
	REQUIRE(f.saves == before + 7);
	REQUIRE(f.reloads == before + 7);
	// And each of them waited for the box afterwards, which is what makes the
	// reading back each of them does worth anything.
	REQUIRE(f.awaits == before + 7);
}

TEST_CASE("a write waits for the box after it writes the list out", "[bouquetedit]")
{
	/* Where the wait sits is the whole of what it is for. The box answers none
	   of the changes, so a call that sent one comes back before the box has
	   looked at it: a wait taken before the list was written out and read back
	   would be a wait for work nobody had asked for yet, and everything after
	   it would still be reading the list as it was. */
	Edit e;
	FakeChannelSource &f = e.fake;

	REQUIRE(channels::addBouquet("Neu").ok());

	REQUIRE(f.awaits == 1);
	REQUIRE(f.saves_at_await == 1);
	REQUIRE(f.reloads_at_await == 1);
}

TEST_CASE("a box that does not come back from the wait is not a change made",
          "[bouquetedit]")
{
	// The third of the three and answered like the other two: what a caller is
	// told is that the bouquet is not what it asked for, because a box that
	// cannot be waited on is one nothing can be read back from either.
	Edit e;
	e.fake.await_status = Status::Busy;

	const Result<void> r = channels::addBouquet("Viertes Bouquet");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
	REQUIRE(r.error().code == ErrorCode::BouquetNotChanged);

	// It was reached, so the answer is the wait failing and not the list never
	// having been written out.
	REQUIRE(e.fake.saves == 1);
	REQUIRE(e.fake.reloads == 1);
	REQUIRE(e.fake.awaits == 1);
}

TEST_CASE("a name nobody has is not found", "[bouquetedit]")
{
	Edit e;
	const size_t before = e.fake.bouquets.size();

	const Result<void> gone = channels::deleteBouquet("gibtesnicht");
	REQUIRE_FALSE(gone.ok());
	REQUIRE(gone.error().status == Status::NotFound);
	REQUIRE(gone.error().code == ErrorCode::NoSuchBouquet);

	/* And nothing was taken away. A lookup that let an unknown name through
	   would land on whichever bouquet the list happened to begin with, so a
	   case reading only the answer would pass while a bouquet went missing. */
	REQUIRE(e.fake.bouquets.size() == before);
	REQUIRE(e.fake.saves == 0);
}

TEST_CASE("renaming moves the key", "[bouquetedit]")
{
	// Stated because a client holding the old name holds nothing after this.
	Edit e;

	REQUIRE(channels::addBouquet("Alt").ok());
	REQUIRE(channels::renameBouquet("Alt", "Neu").ok());

	// Asked apart rather than in one line, because reading the error of an
	// answer that succeeded ends the process, and a case that takes the run
	// down with it says less than one that fails.
	const Result<void> stale = channels::deleteBouquet("Alt");
	REQUIRE_FALSE(stale.ok());
	REQUIRE(stale.error().status == Status::NotFound);

	REQUIRE(channels::deleteBouquet("Neu").ok());
}

TEST_CASE("renaming to a name something else carries moves nothing", "[bouquetedit]")
{
	Edit e;

	const Result<void> taken = channels::renameBouquet("Favoriten", "Erstes Bouquet");
	REQUIRE_FALSE(taken.ok());
	REQUIRE(taken.error().status == Status::Conflict);
	REQUIRE(taken.error().code == ErrorCode::NameTaken);

	// Both of them as they were, which is the whole of what asking before the
	// rename rather than after it buys.
	REQUIRE(e.fake.bouquets[0].name == "Erstes Bouquet");
	REQUIRE(e.fake.bouquets[1].name == "Favoriten");
	REQUIRE(e.fake.saves == 0);
}

TEST_CASE("a bouquet that was made is in the list and one that was taken away is not",
          "[bouquetedit]")
{
	Edit e;

	REQUIRE(channels::addBouquet("Viertes Bouquet").ok());
	const Result<BouquetList> made = channels::bouquets();
	REQUIRE(made.ok());
	REQUIRE(made.value().size() == 4);
	REQUIRE(made.value()[3].name == "Viertes Bouquet");
	// At the end of the list and numbered from one, which is how every read
	// here names a bouquet.
	REQUIRE(made.value()[3].id == 4);

	REQUIRE(channels::deleteBouquet("Erstes Bouquet").ok());
	const Result<BouquetList> left = channels::bouquets();
	REQUIRE(left.ok());
	REQUIRE(left.value().size() == 3);
	REQUIRE(left.value()[0].name == "Favoriten");
	// Everything after the one that went is numbered again, which is why a
	// caller holding a number holds a different bouquet and a caller holding a
	// name holds the one it was given.
	REQUIRE(left.value()[0].id == 1);
}

TEST_CASE("a bouquet moves one place and is refused at either end", "[bouquetedit]")
{
	Edit e;

	REQUIRE(channels::moveBouquet("Favoriten", channels::Direction::Up).ok());
	REQUIRE(e.fake.bouquets[0].name == "Favoriten");
	REQUIRE(e.fake.bouquets[1].name == "Erstes Bouquet");

	const Result<void> top = channels::moveBouquet("Favoriten", channels::Direction::Up);
	REQUIRE_FALSE(top.ok());
	REQUIRE(top.error().status == Status::InvalidArgument);
	REQUIRE(top.error().code == ErrorCode::AlreadyAtTheEnd);
	// Refused rather than passed over: an answer of ok that shifted nothing is
	// the one a caller cannot tell from the one it asked for.
	REQUIRE(e.fake.bouquets[0].name == "Favoriten");

	const Result<void> bottom = channels::moveBouquet("Drittes Bouquet",
	                                                  channels::Direction::Down);
	REQUIRE_FALSE(bottom.ok());
	REQUIRE(bottom.error().code == ErrorCode::AlreadyAtTheEnd);
	REQUIRE(e.fake.bouquets[2].name == "Drittes Bouquet");
}

TEST_CASE("hiding and locking reach the bouquet they name", "[bouquetedit]")
{
	Edit e;

	REQUIRE(channels::setBouquetHidden("Favoriten", true).ok());
	REQUIRE(channels::setBouquetLock("Favoriten", true).ok());
	REQUIRE(e.fake.bouquets[1].hidden);
	REQUIRE(e.fake.bouquets[1].locked);
	// And the bouquets on either side of it were not what was meant.
	REQUIRE_FALSE(e.fake.bouquets[0].hidden);
	REQUIRE_FALSE(e.fake.bouquets[2].locked);

	REQUIRE(channels::setBouquetHidden("Favoriten", false).ok());
	REQUIRE_FALSE(e.fake.bouquets[1].hidden);
	REQUIRE(e.fake.bouquets[1].locked);
}

TEST_CASE("setting one half of a bouquet leaves the other half alone", "[bouquetedit]")
{
	Edit e;

	REQUIRE(channels::setBouquetChannels("Favoriten", one(ZDF), channels::Mode::Tv).ok());

	/* Read out of the list and not out of the answer. An endpoint of this
	   branch once answered ok while emptying three lists, and a case that
	   compared the answer would have passed. */
	const ChannelList now = e.fake.bouquet_members[2];
	REQUIRE(now.size() == 2);
	REQUIRE(holds(now, ZDF));
	REQUIRE(holds(now, DLF));
	REQUIRE_FALSE(holds(now, ERSTE));
}

TEST_CASE("the order a list is written in is the order the bouquet holds", "[bouquetedit]")
{
	/* Membership was all this promised once, and a caller could not read back
	   what it had written: the box draws a bouquet in the order it holds it
	   and counts the channel numbers off that order, so a call that kept only
	   which channels are in it answered a request nobody made. */
	Edit e;

	ChannelIdList asked;
	asked.push_back(ZDF);
	asked.push_back(ERSTE);
	REQUIRE(channels::setBouquetChannels("Favoriten", asked, channels::Mode::Tv).ok());
	REQUIRE(televisionOf(e.fake.bouquet_members[2]) == asked);

	// And the other way round, because a list that happened to arrive in the
	// order the bouquet already held would say nothing.
	ChannelIdList back;
	back.push_back(ERSTE);
	back.push_back(ZDF);
	REQUIRE(channels::setBouquetChannels("Favoriten", back, channels::Mode::Tv).ok());
	REQUIRE(televisionOf(e.fake.bouquet_members[2]) == back);

	// The radio half is where it was, which is what naming a half means.
	REQUIRE(holds(e.fake.bouquet_members[2], DLF));
}

TEST_CASE("a list naming a channel the box does not have changes nothing", "[bouquetedit]")
{
	Edit e;

	ChannelIdList asked;
	asked.push_back(ZDF);
	asked.push_back(ABSENT);

	const Result<void> r = channels::setBouquetChannels("Favoriten", asked,
	                                                    channels::Mode::Tv);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoSuchChannel);

	// The bouquet as it was, because every id is looked up before any of them
	// is applied: a list checked as it was applied would leave the bouquet
	// holding the good part of it while the caller is told it failed.
	const ChannelList now = e.fake.bouquet_members[2];
	REQUIRE(now.size() == 2);
	REQUIRE(holds(now, ERSTE));
	REQUIRE(holds(now, DLF));
	REQUIRE(e.fake.saves == 0);
}

TEST_CASE("a change the box turned down is not written out", "[bouquetedit]")
{
	Edit e;
	e.fake.write_status = Status::Denied;

	const Result<void> r = channels::deleteBouquet("Favoriten");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Denied);
	REQUIRE(r.error().code == ErrorCode::BouquetNotChanged);

	REQUIRE(e.fake.bouquets.size() == 3);
	REQUIRE(e.fake.saves == 0);
	REQUIRE(e.fake.reloads == 0);
}

TEST_CASE("a change that was not written out is reported as one that did not happen",
          "[bouquetedit]")
{
	// One code for the three of them, because the change reaches the box only
	// if all three go through and a caller told which one failed would do the
	// same thing about any of them.
	Edit e;
	e.fake.save_status = Status::Busy;

	const Result<void> unwritten = channels::addBouquet("Viertes Bouquet");
	REQUIRE_FALSE(unwritten.ok());
	REQUIRE(unwritten.error().status == Status::Busy);
	REQUIRE(unwritten.error().code == ErrorCode::BouquetNotChanged);
	// The read back is not attempted once the write is gone, so the two
	// answers cannot be read as one having happened twice.
	REQUIRE(e.fake.saves == 1);
	REQUIRE(e.fake.reloads == 0);

	e.fake.save_status = Status::Ok;
	e.fake.reload_status = Status::Internal;
	const Result<void> unread = channels::addBouquet("Fuenftes Bouquet");
	REQUIRE_FALSE(unread.ok());
	REQUIRE(unread.error().status == Status::Internal);
	REQUIRE(unread.error().code == ErrorCode::BouquetNotChanged);
	REQUIRE(e.fake.reloads == 1);
}

TEST_CASE("a bouquet list that cannot be read is not a bouquet that is missing",
          "[bouquetedit]")
{
	/* The two answers have to stay apart: a box whose bouquet list cannot be
	   read at all would otherwise report every name as one nothing carries,
	   and a caller acting on that would make a second bouquet under a name
	   that is already there. */
	Edit e;
	e.fake.bouquets_status = Status::Busy;

	const Result<void> r = channels::deleteBouquet("Favoriten");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Busy);
	REQUIRE(r.error().code == ErrorCode::BouquetListUnavailable);

	const Result<void> made = channels::addBouquet("Viertes Bouquet");
	REQUIRE_FALSE(made.ok());
	REQUIRE(made.error().status == Status::Busy);
	REQUIRE(made.error().code == ErrorCode::BouquetListUnavailable);
	REQUIRE(e.fake.bouquets.size() == 3);
}

TEST_CASE("a change the box took and did not make is not answered as done", "[bouquetedit]")
{
	/* The failure none of the answers underneath can carry, and the one every
	   one of these seven used to report as done: the box takes a command off a
	   socket and acts on it afterwards, so a command that arrived and did
	   nothing looks from below exactly like one that worked. Nothing but
	   reading the list back afterwards tells them apart. */
	Edit e;
	e.fake.writes_dropped = true;

	// Each with the read that fits it, so this is seven cases and not one: the
	// same list satisfies a rename and fails a move.
	notDone("adding", channels::addBouquet("Viertes Bouquet"));
	notDone("renaming", channels::renameBouquet("Favoriten", "Anders"));
	notDone("moving", channels::moveBouquet("Favoriten", channels::Direction::Up));
	notDone("hiding", channels::setBouquetHidden("Favoriten", true));
	notDone("locking", channels::setBouquetLock("Favoriten", true));
	notDone("filling", channels::setBouquetChannels("Favoriten", one(ZDF),
	                                                channels::Mode::Tv));
	notDone("deleting", channels::deleteBouquet("Favoriten"));

	// And the list is where it was, which is what the answers alone cannot
	// say: a refusal that had changed something would read the same.
	REQUIRE(e.fake.bouquets.size() == 3);
	REQUIRE(e.fake.bouquets[1].name == "Favoriten");
}

TEST_CASE("a rename that landed on another bouquet is not a rename", "[bouquetedit]")
{
	/* Half of what a rename promises, and the half a read looking only for the
	   new name passes: the name asked for is in the list, on a bouquet nobody
	   named, and the bouquet that was to be renamed still carries the old one.
	   A caller told this had worked would hold a name that answers to two
	   bouquets. */
	Edit e;
	e.fake.rename_lands_beside = true;

	notDone("renaming", channels::renameBouquet("Favoriten", "Anders"));

	// Stated, because it is what makes this case about the old name and not
	// about the new one: the new name did arrive.
	REQUIRE(e.fake.bouquets[2].name == "Anders");
	REQUIRE(e.fake.bouquets[1].name == "Favoriten");
}

TEST_CASE("a rename whose name arrived cut short is not a rename", "[bouquetedit]")
{
	/* And the other half, which a read looking only for the old name being
	   gone passes: the bouquet was renamed and to something nobody asked for.
	   A field of fixed width in the command does this on its own, and the
	   caller is then holding a name the box does not have. */
	Edit e;
	e.fake.rename_cuts_name = true;

	notDone("renaming", channels::renameBouquet("Favoriten", "Anders"));

	// The old name is gone, so nothing but looking for the new one catches it.
	REQUIRE(e.fake.bouquets[1].name == "Ander");
}

TEST_CASE("a half filled in the wrong order is not the list that was asked for",
          "[bouquetedit]")
{
	/* The order is half of what this call carries: the box counts the numbers
	   it draws beside a channel off it, so a caller that wrote a list cannot
	   read its own request back from a half that holds those channels in some
	   other sequence. A read comparing which channels are in the half would
	   answer ok to this. */
	Edit e;
	e.fake.order_ignored = true;

	ChannelIdList asked;
	asked.push_back(ZDF);
	asked.push_back(ERSTE);
	notDone("filling", channels::setBouquetChannels("Favoriten", asked,
	                                                channels::Mode::Tv));

	// Every channel that was asked for is in the half, which is what makes
	// this a case about the order and nothing else.
	ChannelIdList turned;
	turned.push_back(ERSTE);
	turned.push_back(ZDF);
	REQUIRE(televisionOf(e.fake.bouquet_members[2]) == turned);
}

TEST_CASE("a list that cannot be read back is not a change that did not happen",
          "[bouquetedit]")
{
	/* The two answers have to stay apart, and the read after a write is where they come
	   closest. A list that stopped being readable between the two says nothing about
	   what the box did with the change, and a caller told the bouquet was not changed
	   would make the change again or undo one that had been made. The box rebuilds this
	   list from a thread of its own, so the window is not a contrivance. All four of
	   the reads, because the rule is the same for every one of them. */
	Edit e;
	e.fake.reads_fail_after_write = true;

	const Result<void> made = channels::addBouquet("Viertes Bouquet");
	REQUIRE_FALSE(made.ok());
	REQUIRE(made.error().status == Status::Busy);
	REQUIRE(made.error().code == ErrorCode::BouquetListUnavailable);
	// And it had been made, which is exactly what the answer must not deny.
	REQUIRE(e.fake.bouquets.size() == 4);

	// Put back before each of the rest, or the lookup every one of these makes
	// before it writes would fail first and the case would be about that.
	e.fake.bouquets_status = Status::Ok;
	const Result<void> hid = channels::setBouquetHidden("Favoriten", true);
	REQUIRE_FALSE(hid.ok());
	REQUIRE(hid.error().code == ErrorCode::BouquetListUnavailable);
	REQUIRE(e.fake.bouquets[1].hidden);

	e.fake.bouquets_status = Status::Ok;
	const Result<void> moved = channels::moveBouquet("Favoriten",
	                                                 channels::Direction::Up);
	REQUIRE_FALSE(moved.ok());
	REQUIRE(moved.error().code == ErrorCode::BouquetListUnavailable);
	REQUIRE(e.fake.bouquets[0].name == "Favoriten");

	e.fake.bouquets_status = Status::Ok;
	const Result<void> filled = channels::setBouquetChannels("Favoriten", one(ZDF),
	                                                         channels::Mode::Tv);
	REQUIRE_FALSE(filled.ok());
	REQUIRE(filled.error().code == ErrorCode::BouquetListUnavailable);
	REQUIRE(holds(e.fake.bouquet_members[1], ZDF));
}

/* Nothing the message loop sends says a bouquet changed, so a page holding a
   list has no way to learn that the screen or a second page changed one, and
   shows a list that is no longer there until somebody reloads it. */

namespace
{
/* What the bus delivered, and what the box had done by the time each of them
   arrived. The second half is the point: the announcement is only worth
   anything on the far side of the wait, so what a case reads is the state the
   subscriber was handed rather than the state at the end of the call. */
struct Watcher : public Subscriber
{
	const FakeChannelSource *fake;
	std::vector<Event>       seen;
	std::vector<unsigned>    awaits_at_event;

	explicit Watcher(const FakeChannelSource *f) : fake(f)
	{
		EventBus::instance().subscribe(this);
	}

	void onEvent(const Event &e)
	{
		seen.push_back(e);
		awaits_at_event.push_back(fake->awaits);
	}

	size_t countOf(EventType t) const
	{
		size_t n = 0;
		for (size_t i = 0; i < seen.size(); i++)
		{
			if (seen[i].type == t)
				n++;
		}
		return n;
	}

	private:
		Watcher(const Watcher &);
		Watcher &operator=(const Watcher &);
};

/* A call that did not change anything, whatever stopped it. The status is not
   pinned here as it is above, because these cases put the refusal at a
   different step each time and the status is the one the step answered with;
   what they are about is that nothing was said on the bus. */
void turnedDown(const char *which, const Result<void> &r)
{
	INFO(which);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().code == ErrorCode::BouquetNotChanged);
}
}

TEST_CASE("every editing call announces that the bouquets changed",
          "[bouquetedit]")
{
	// All seven and not a sample of them, for the reason the count of writes
	// is all seven: the number is what says which of the calls is the one that
	// went out without saying so.
	Edit e;
	Watcher watch(&e.fake);

	REQUIRE(channels::addBouquet("Neu").ok());
	REQUIRE(channels::renameBouquet("Neu", "Neuer").ok());
	REQUIRE(channels::moveBouquet("Neuer", channels::Direction::Up).ok());
	REQUIRE(channels::setBouquetHidden("Neuer", true).ok());
	REQUIRE(channels::setBouquetLock("Neuer", true).ok());
	REQUIRE(channels::setBouquetChannels("Neuer", one(ZDF), channels::Mode::Tv).ok());
	REQUIRE(channels::deleteBouquet("Neuer").ok());

	REQUIRE(watch.countOf(EventType::BouquetsChanged) == 7);
	REQUIRE(watch.seen.size() == 7);
}

TEST_CASE("the announcement is made after the box has been waited for",
          "[bouquetedit]")
{
	/* The box answers none of these changes, so a subscriber told before the
	   wait would go and read the list and be handed the one it already had,
	   which is the whole of what such a reader is listening for. */
	Edit e;
	Watcher watch(&e.fake);

	REQUIRE(channels::addBouquet("Neu").ok());

	REQUIRE(watch.awaits_at_event.size() == 1);
	REQUIRE(watch.awaits_at_event[0] == 1);
}

TEST_CASE("a change the box turned down is not announced", "[bouquetedit]")
{
	Edit e;
	e.fake.write_status = Status::Busy;
	Watcher watch(&e.fake);

	turnedDown("add", channels::addBouquet("Neu"));
	turnedDown("rename", channels::renameBouquet("Favoriten", "Neuer"));
	turnedDown("move", channels::moveBouquet("Favoriten", channels::Direction::Up));
	turnedDown("hide", channels::setBouquetHidden("Favoriten", true));
	turnedDown("lock", channels::setBouquetLock("Favoriten", true));
	turnedDown("fill", channels::setBouquetChannels("Favoriten", one(ZDF),
	                                                channels::Mode::Tv));
	turnedDown("delete", channels::deleteBouquet("Favoriten"));

	REQUIRE(watch.seen.empty());
}

TEST_CASE("a change that never reached the box is not announced",
          "[bouquetedit]")
{
	/* Each of the three the writing out is made of, because the announcement
	   stands behind all three and a subscriber told after any one of them
	   failed would be told about a change the next start throws away. */
	Edit e;
	Watcher watch(&e.fake);

	e.fake.save_status = Status::Internal;
	turnedDown("save", channels::addBouquet("Neu"));

	e.fake.save_status = Status::Ok;
	e.fake.reload_status = Status::Internal;
	turnedDown("reload", channels::addBouquet("Neuer"));

	e.fake.reload_status = Status::Ok;
	e.fake.await_status = Status::Busy;
	turnedDown("wait", channels::addBouquet("Noch einer"));

	REQUIRE(watch.seen.empty());
}

/* The screen's own bouquet editors write the lists out through the channel
   stack and never reach the calls above, so a page holding a list learns
   nothing about what a viewer did at the television. They say it themselves,
   with the one call below, and what it is worth is the wait it makes first. */

TEST_CASE("the announcement made on its own reaches the bus", "[bouquetedit]")
{
	Edit e;
	Watcher watch(&e.fake);

	REQUIRE(channels::announceBouquetsChanged().ok());

	REQUIRE(watch.countOf(EventType::BouquetsChanged) == 1);
	REQUIRE(watch.seen.size() == 1);
}

TEST_CASE("the announcement made on its own is made after the wait",
          "[bouquetedit]")
{
	/* The box answers a write of the lists as soon as the files are out and
	   numbers the lists again afterwards, so the editor that called this is
	   back on its feet while the box is still working. A reader woken before
	   the wait reads the list in the middle of that numbering. */
	Edit e;
	Watcher watch(&e.fake);

	REQUIRE(channels::announceBouquetsChanged().ok());

	REQUIRE(watch.awaits_at_event.size() == 1);
	REQUIRE(watch.awaits_at_event[0] == 1);
}

TEST_CASE("the announcement writes nothing of its own", "[bouquetedit]")
{
	/* The caller is a screen that has already written the lists out its own
	   way, and this saying so again would be a second write of two files after
	   every change made at the television. */
	Edit e;
	Watcher watch(&e.fake);

	REQUIRE(channels::announceBouquetsChanged().ok());

	REQUIRE(e.fake.saves == 0);
	REQUIRE(e.fake.reloads == 0);
	REQUIRE(watch.seen.size() == 1);
}

TEST_CASE("a box that cannot be waited for is not announced", "[bouquetedit]")
{
	/* What is left when the wait fails is a box there is nothing to be said
	   about: the change is the caller's and was made, and whether the box is
	   through with it is the one thing this could have found out. */
	Edit e;
	e.fake.await_status = Status::Busy;
	Watcher watch(&e.fake);

	const Result<void> said = channels::announceBouquetsChanged();

	REQUIRE_FALSE(said.ok());
	REQUIRE(said.error().code == ErrorCode::BouquetNotChanged);
	REQUIRE(watch.seen.empty());
}
