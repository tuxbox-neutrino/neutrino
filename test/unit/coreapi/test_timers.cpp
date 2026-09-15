/*
 * test_timers.cpp - tests for timers
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
#include "coreapi/base/eventbus.h"
#include "coreapi/timers.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <utility>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <connection/basicmessage.h>
#include <timerdclient/timerdclient.h>
#include <timerdclient/timerdmsg.h>
#include <timerdclient/timerdtypes.h>

using namespace coreapi;

namespace
{

// A recording that begins after the fake's clock and ends after it begins, so
// that a case which means to fail one rule is not already failing another.
TimerInfo goodRecording()
{
	TimerInfo t;
	t.type = (int) TimerType::Record;
	t.channel_id = 0x2b66;
	t.start = 2000;
	t.stop = 3000;
	t.title = "Tatort";
	return t;
}

// Keeps what the bus delivered, so a case can ask what a change announced.
struct Recorder : public Subscriber
{
	std::vector<Event> seen;

	Recorder() { EventBus::instance().subscribe(this); }

	void onEvent(const Event &e) { seen.push_back(e); }

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
};

} // anonymous namespace

/* Nothing the message loop sends says a timer changed, so a reader that keeps
   timers has no way to learn that another reader changed one. The announcement
   is made where the change is known to have been taken, and it has to carry
   which timer it was. */
TEST_CASE("a timer that was taken is announced", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);
	Recorder listener;

	Result<uint32_t> created = timers::create(goodRecording());
	REQUIRE(created.ok());
	REQUIRE(listener.countOf(EventType::TimerChanged) == 1);
	REQUIRE(listener.seen.back().value == (int) created.value());

	TimerInfo changed = goodRecording();
	changed.id = created.value();
	changed.stop = 4000;
	REQUIRE(timers::modify(changed).ok());
	REQUIRE(listener.countOf(EventType::TimerChanged) == 2);
	REQUIRE(listener.seen.back().value == (int) created.value());

	REQUIRE(timers::remove(created.value()).ok());
	REQUIRE(listener.countOf(EventType::TimerChanged) == 3);
	REQUIRE(listener.seen.back().value == (int) created.value());
}

TEST_CASE("a change the box did not take is not announced", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);
	Recorder listener;

	// No id, so it is refused before the box is asked at all.
	TimerInfo nameless = goodRecording();
	REQUIRE_FALSE(timers::modify(nameless).ok());

	// An id the box does not have.
	REQUIRE_FALSE(timers::remove(4711).ok());

	// A recording that ends before it begins.
	TimerInfo backwards = goodRecording();
	backwards.stop = backwards.start - 1;
	REQUIRE_FALSE(timers::create(backwards).ok());

	REQUIRE(listener.countOf(EventType::TimerChanged) == 0);
}

TEST_CASE("a removed timer is gone from the list", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	Result<uint32_t> created = timers::create(goodRecording());
	REQUIRE(created.ok());

	Result<TimerList> before = timers::list();
	REQUIRE(before.ok());
	REQUIRE(before.value().size() == 1);

	REQUIRE(timers::remove(created.value()).ok());

	Result<TimerList> after = timers::list();
	REQUIRE(after.ok());
	REQUIRE(after.value().empty());
}

TEST_CASE("removing an unknown timer is NotFound", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	Result<void> r = timers::remove(4711);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);

	// Nothing was sent to the daemon, so the answer comes from the list read
	// and not from a removal that quietly did nothing.
	REQUIRE(fake.removals == 0);
}

TEST_CASE("a recording timer that ends before it starts is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.start = 3000;
	t.stop = 2000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(fake.timers.empty());
}

TEST_CASE("a recording timer that ends when it starts is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.stop = t.start;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

TEST_CASE("a recording timer without a channel is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.channel_id = 0;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(fake.timers.empty());
}

TEST_CASE("a zap timer without a channel is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	t.type = (int) TimerType::Zapto;
	t.channel_id = 0;
	t.start = 2000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

// The kinds that do not name a channel have to stay creatable, or the rule
// above would be a rule against every timer that is not a recording.
TEST_CASE("a shutdown timer needs no channel", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	t.type = (int) TimerType::Shutdown;
	t.start = 2000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE(r.ok());
	REQUIRE(r.value() == 1);
}

TEST_CASE("a timer of a kind the box does not have is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	// The daemon keeps two for a kind it no longer has, and builds nothing for
	// it, so a timer that names it would be answered with an id of zero.
	t.type = 2;
	t.start = 2000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::NoSuchTimerType);
	REQUIRE(fake.timers.empty());
}

TEST_CASE("a one-off timer that begins before now is rejected", "[timers]")
{
	FakeTimerSource fake;
	fake.clock = 100000;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.start = 99000;
	t.stop = 101000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::TimerInThePast);
}

// A caller that builds a moment out of separate fields has no seconds to give
// and zeroes them, so a start inside the current minute is the ordinary way of
// asking for now and must not be refused.
TEST_CASE("a one-off timer inside the current minute is taken", "[timers]")
{
	FakeTimerSource fake;
	fake.clock = 100000;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.start = 100000 - (100000 % 60);
	t.stop = 101000;

	REQUIRE(timers::create(t).ok());
}

TEST_CASE("a second before the current minute is not inside it", "[timers]")
{
	FakeTimerSource fake;
	fake.clock = 100000;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.start = 100000 - (100000 % 60) - 1;
	t.stop = 101000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
}

// A repeating timer whose first occurrence has gone by is how one is normally
// entered: the daemon moves it to its next occurrence instead of refusing it.
TEST_CASE("a repeating timer may begin before now", "[timers]")
{
	FakeTimerSource fake;
	fake.clock = 100000;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.start = 99000;
	t.stop = 99600;
	t.repeat = (int) CTimerd::TIMERREPEAT_DAILY;

	REQUIRE(timers::create(t).ok());
}

// Only the clock fails here, so the answer cannot be the one a daemon that has
// stopped answering gives, and the code is asked as well as the status: a
// create that failed for any other reason carries a different one.
TEST_CASE("a clock that cannot be read fails the create", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);
	fake.now_status = Status::Internal;

	Result<uint32_t> r = timers::create(goodRecording());
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(r.error().code == ErrorCode::ClockUnavailable);
	REQUIRE(fake.timers.empty());
}

// A repeating timer never asks the clock, so a clock that cannot be read costs
// only the rule that needs it.
TEST_CASE("a clock that cannot be read leaves a repeating create alone", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);
	fake.now_status = Status::Internal;

	TimerInfo t = goodRecording();
	t.repeat = (int) CTimerd::TIMERREPEAT_DAILY;

	REQUIRE(timers::create(t).ok());
}

TEST_CASE("an immediate recording without a channel is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	t.type = (int) TimerType::ImmediateRecord;
	t.channel_id = 0;
	t.start = 2000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::TimerWithoutChannel);
	REQUIRE(fake.timers.empty());
}

/* The kind the daemon cannot be handed, and what is handed over instead. Filed as
   already running, an immediate recording is never fired, and what fires a recording
   timer is what starts the recording: the row would report itself as running and write
   nothing at all. Asking only whether the call succeeded would pass for the layer that
   made the dead row. */
TEST_CASE("an immediate recording reaches the daemon as one whose start has come", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	t.type = (int) TimerType::ImmediateRecord;
	t.channel_id = 0x2b66;
	t.start = 2000;
	t.stop = 6000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE(r.ok());
	REQUIRE(fake.timers.size() == 1);
	REQUIRE(fake.timers[0].type == (int) TimerType::Record);
	REQUIRE(fake.timers[0].type != (int) TimerType::ImmediateRecord);
	// Everything else is the caller's and travels untouched.
	REQUIRE(fake.timers[0].channel_id == 0x2b66);
	REQUIRE(fake.timers[0].start == 2000);
	REQUIRE(fake.timers[0].stop == 6000);
}

// The other kinds are handed over as they were asked for, so the translation
// above is about the one kind and not about every timer that carries a channel.
TEST_CASE("an ordinary recording is handed over as the kind it was asked for", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	t.type = (int) TimerType::Record;
	t.channel_id = 0x2b66;
	t.start = 2000;
	t.stop = 6000;

	REQUIRE(timers::create(t).ok());
	REQUIRE(fake.timers.size() == 1);
	REQUIRE(fake.timers[0].type == (int) TimerType::Record);

	TimerInfo z;
	z.type = (int) TimerType::Zapto;
	z.channel_id = 0x2b66;
	z.start = 2000;

	REQUIRE(timers::create(z).ok());
	REQUIRE(fake.timers.size() == 2);
	REQUIRE(fake.timers[1].type == (int) TimerType::Zapto);
}

/* The rule about a duration is asked of both recording kinds, because one of
   them turns into the other. Without it an immediate recording with no end
   reaches the daemon as a recording timer with no stop time, and the daemon
   drops such a timer as finished the moment it has fired it: the recording
   starts, the row that carries its end is gone, and nothing can be asked to
   stop it afterwards. */
TEST_CASE("an immediate recording that ends no later than it begins is refused", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	t.type = (int) TimerType::ImmediateRecord;
	t.channel_id = 0x2b66;
	t.start = 2000;
	t.stop = 0;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::RecordingWithoutDuration);
	REQUIRE(fake.timers.empty());
}

// The daemon builds nothing for this kind: it is the label another box's timer
// wears here, and it reaches the same arm as a kind the daemon never had.
TEST_CASE("a remotebox timer cannot be made here", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t;
	t.type = (int) TimerType::Remotebox;
	t.start = 2000;

	Result<uint32_t> r = timers::create(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(r.error().code == ErrorCode::NoSuchTimerType);
	REQUIRE(fake.timers.empty());
}

TEST_CASE("a timer the daemon refuses as a duplicate is a conflict", "[timers]")
{
	FakeTimerSource fake;
	fake.add_status = Status::Conflict;
	InstalledTimerSource installed(&fake);

	Result<uint32_t> r = timers::create(goodRecording());
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Conflict);
}

TEST_CASE("the id of a new timer comes from the daemon", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	Result<uint32_t> first = timers::create(goodRecording());
	REQUIRE(first.ok());
	Result<uint32_t> second = timers::create(goodRecording());
	REQUIRE(second.ok());
	REQUIRE(first.value() != second.value());

	Result<TimerList> all = timers::list();
	REQUIRE(all.ok());
	REQUIRE(all.value().size() == 2);
	REQUIRE(all.value()[0].id == first.value());
	REQUIRE(all.value()[1].id == second.value());
}

// The daemon acknowledges a removal with nothing at all, so an Ok that rested
// on the message having been sent would also be an Ok for a daemon that has
// stopped acting on what it is sent.
TEST_CASE("a removal the daemon ignores is not reported as done", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	Result<uint32_t> created = timers::create(goodRecording());
	REQUIRE(created.ok());

	fake.ignore_removals = true;
	Result<void> r = timers::remove(created.value());
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	// Its own code. The daemon refusing the removal and the daemon taking it
	// and keeping the timer are two outcomes, and a caller holding only the
	// code has nothing else to tell them apart by.
	REQUIRE(r.error().code == ErrorCode::TimerStillThere);

	// It was sent, and the answer comes from reading the list back afterwards.
	REQUIRE(fake.removals == 1);
}

TEST_CASE("a list that cannot be read does not read as a missing timer", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);
	fake.status = Status::Internal;

	Result<void> r = timers::remove(1);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::Internal);
	REQUIRE(fake.removals == 0);

	Result<TimerList> all = timers::list();
	REQUIRE_FALSE(all.ok());
	REQUIRE(all.error().status == Status::Internal);
}

TEST_CASE("an empty timer list is an answer and not an error", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	Result<TimerList> all = timers::list();
	REQUIRE(all.ok());
	REQUIRE(all.value().empty());
	REQUIRE(fake.list_reads == 1);
}

TEST_CASE("changing a timer the box does not have is NotFound", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.id = 4711;

	Result<void> r = timers::modify(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(fake.modifications == 0);
}

TEST_CASE("changing a timer without an id is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	Result<uint32_t> created = timers::create(goodRecording());
	REQUIRE(created.ok());

	TimerInfo t = goodRecording();
	t.id = 0;

	Result<void> r = timers::modify(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(fake.modifications == 0);
}

TEST_CASE("a change that leaves a recording no duration is rejected", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	Result<uint32_t> created = timers::create(goodRecording());
	REQUIRE(created.ok());

	TimerInfo t = goodRecording();
	t.id = created.value();
	t.stop = t.start;

	Result<void> r = timers::modify(t);
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::InvalidArgument);
	REQUIRE(fake.modifications == 0);
}

// A change moves a timer that already exists, and one whose moment has gone by
// is exactly the timer someone reaches for, so the rule that guards a new
// timer must not be asked here.
TEST_CASE("a change may move a timer that already began", "[timers]")
{
	FakeTimerSource fake;
	fake.clock = 100000;
	InstalledTimerSource installed(&fake);

	TimerInfo t = goodRecording();
	t.start = 200000;
	t.stop = 201000;
	Result<uint32_t> created = timers::create(t);
	REQUIRE(created.ok());

	t.id = created.value();
	t.start = 99000;
	t.stop = 99600;
	REQUIRE(timers::modify(t).ok());

	Result<TimerList> all = timers::list();
	REQUIRE(all.ok());
	REQUIRE(all.value().size() == 1);
	REQUIRE(all.value()[0].start == 99000);
}

TEST_CASE("the timer list is handed over rather than copied", "[timers]")
{
	FakeTimerSource fake;
	InstalledTimerSource installed(&fake);

	REQUIRE(timers::create(goodRecording()).ok());

	Result<TimerList> all = timers::list();
	REQUIRE(all.ok());
	const void *inside = (const void *) &all.value()[0];
	TimerList taken = std::move(all).value();
	REQUIRE((const void *) &taken[0] == inside);
}

/* The layer above hands the daemon's client a title for every kind of timer that carries
   one, and for a recording that title used to reach nothing: the client copies a
   RecordingInfo into the smaller block the wire carries, field by field, and the title
   was the one field that copy left out. The block on the wire has had room for it all
   along.

   Driven over a real socket against the shipped client and not against a copy of what it
   is meant to send, because what this is about is a field one copy forgot, and a second
   copy written here would forget it in the same place. */

namespace
{

/* An address of this process's own. TIMERD_UDS_NAME is a fixed name under /tmp
   that the daemon of a running box already listens on, and binding it here
   would take that name away from whoever holds it. Which address the client
   asks for is not what this case is about. */
const char *privateTimerdSocket()
{
	static std::string name;
	if (name.empty())
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "/tmp/coreapi-timerd.%d.sock", (int) getpid());
		name = buf;
	}
	return name.c_str();
}

/* The shipped client with its address replaced and nothing else, the way the
   zapit client is driven a few files over. The address is a private virtual of
   the class, which a derived class may still answer for. */
class TimerdClientHere : public CTimerdClient
{
	private:
		const char *getSocketName() const { return privateTimerdSocket(); }
};

/* A daemon that answers the two exchanges one add is made of: the list the
   client reads to see whether such a timer is already there, and the add
   itself. What arrives in the second is kept, which is the whole point. */
class TimerdPeer
{
	public:
		TimerdPeer() : got_record(false), listen_fd(-1), joined(false)
		{
			std::memset(&taken, 0, sizeof(taken));
			unlink(privateTimerdSocket());

			listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
			REQUIRE(listen_fd != -1);
			sockaddr_un addr;
			std::memset(&addr, 0, sizeof(addr));
			addr.sun_family = AF_UNIX;
			std::strncpy(addr.sun_path, privateTimerdSocket(), sizeof(addr.sun_path) - 1);
			/* Checked: a bind that failed leaves nothing listening, and the
			   case would then read a client that reached no daemon at all as
			   one whose block arrived without a title. */
			REQUIRE(bind(listen_fd, (sockaddr *) &addr, sizeof(addr)) == 0);
			REQUIRE(listen(listen_fd, 4) == 0);

			/* An accept nobody satisfies would be joined below and would hang
			   the whole run with nothing to read about why. */
			struct timeval tv;
			tv.tv_sec = 30;
			tv.tv_usec = 0;
			REQUIRE(setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0);

			REQUIRE(pthread_create(&thread, 0, serve, this) == 0);
		}

		~TimerdPeer()
		{
			finish();
			if (listen_fd != -1)
				close(listen_fd);
			unlink(privateTimerdSocket());
		}

		/* What the peer kept may only be read once its thread has stopped
		   writing it, so the join is a step of its own rather than only the
		   destructor's. */
		void finish()
		{
			if (joined)
				return;
			joined = true;
			pthread_join(thread, 0);
		}

		CTimerd::TransferRecordingInfo taken;
		bool                           got_record;

	private:
		static void *serve(void *arg)
		{
			static_cast<TimerdPeer *>(arg)->loop();
			return 0;
		}

		/* Both exchanges, in the order the client makes them, each on a
		   connection of its own because the client closes after every one. */
		void loop()
		{
			for (int exchange = 0; exchange < 2; exchange++)
			{
				int fd = accept(listen_fd, 0, 0);
				if (fd < 0)
					return;
				answer(fd);
				close(fd);
			}
		}

		void answer(int fd)
		{
			CBasicMessage::Header head;
			if (!readAll(fd, &head, sizeof(head)))
				return;

			if (head.cmd == CTimerdMsg::CMD_GETTIMERLIST)
			{
				// No timers at all, so nothing this add could be a repeat of.
				CTimerdMsg::generalInteger none;
				none.number = 0;
				send(fd, &none, sizeof(none), MSG_NOSIGNAL);
				return;
			}

			if (head.cmd != CTimerdMsg::CMD_ADDTIMER)
				return;

			CTimerdMsg::commandAddTimer add;
			if (!readAll(fd, &add, sizeof(add)))
				return;
			if (add.eventType == CTimerd::TIMER_RECORD &&
			    readAll(fd, &taken, sizeof(taken)))
				got_record = true;

			CTimerdMsg::responseAddTimer reply;
			reply.eventID = 4711;
			send(fd, &reply, sizeof(reply), MSG_NOSIGNAL);
		}

		static bool readAll(int fd, void *into, size_t bytes)
		{
			char *at = static_cast<char *>(into);
			size_t left = bytes;
			while (left > 0)
			{
				ssize_t got = recv(fd, at, left, 0);
				if (got <= 0)
					return false;
				at += got;
				left -= (size_t) got;
			}
			return true;
		}

		int       listen_fd;
		bool      joined;
		pthread_t thread;
};

} // namespace

TEST_CASE("a recording timer carries the name it was given to the daemon", "[timers][ipc]")
{
	TimerdPeer peer;
	TimerdClientHere client;

	CTimerd::RecordingInfo asked;
	asked.channel_id = 0x2b66;
	asked.epg_id = 0;
	asked.epg_starttime = 0;
	asked.apids = TIMERD_APIDS_CONF;
	asked.recordingSafety = false;
	asked.autoAdjustToEPG = false;
	asked.channel_ci = false;
	asked.eventID = 0;
	std::strcpy(asked.recordingDir, "/media/sda1/movies");
	std::strcpy(asked.epgTitle, "Tatort");

	const int id = client.addTimerEvent(CTimerd::TIMER_RECORD, &asked, 0, 2000, 3000,
					    CTimerd::TIMERREPEAT_ONCE, 0);

	peer.finish();

	// The exchange itself, so that an empty title cannot be read as a title
	// the daemon never got a block for at all.
	REQUIRE(id == 4711);
	REQUIRE(peer.got_record);
	REQUIRE(std::string(peer.taken.recordingDir) == "/media/sda1/movies");
	REQUIRE(std::string(peer.taken.epgTitle) == "Tatort");
}

/* The one place where a title with nothing in it must still arrive as a title with
   nothing in it. The block the caller fills is not cleared before it is filled and the
   daemon now reads this field, so a sender that writes only the fields it cares about
   must not leave a name behind for it. The block is built on bytes that have been
   written over first, so a constructor leaving the two arrays alone is caught here. */
TEST_CASE("a recording timer with no name of its own sends none", "[timers][ipc]")
{
	TimerdPeer peer;
	TimerdClientHere client;

	unsigned char space[sizeof(CTimerd::RecordingInfo)];
	std::memset(space, 0xAB, sizeof(space));
	CTimerd::RecordingInfo *asked = new (space) CTimerd::RecordingInfo();

	asked->channel_id = 0x2b66;
	asked->epg_id = 0;
	asked->epg_starttime = 0;
	asked->apids = TIMERD_APIDS_CONF;
	asked->recordingSafety = false;
	asked->autoAdjustToEPG = false;
	asked->channel_ci = false;
	asked->eventID = 0;
	std::strcpy(asked->recordingDir, "/media/sda1/movies");

	client.addTimerEvent(CTimerd::TIMER_RECORD, asked, 0, 2000, 3000,
			     CTimerd::TIMERREPEAT_ONCE, 0);

	peer.finish();

	REQUIRE(peer.got_record);
	REQUIRE(std::string(peer.taken.epgTitle).empty());
}
