/*
 * test_streamadmission.cpp - whether a stream may have a tuner, and which way
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The rule a stream is admitted by, on the facts zapit reads under the
 * frontend manager's lock. The box this is written for has the picture on
 * one frontend, PiP slot i reading demux i + 1, and a stream on frontend f
 * reading demux f + 1.
 */

#include "support/catch.hpp"

#include <zapit/streamadmission.h>
#include <zapit/streamholdcmd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <vector>

#include <pthread.h>
#include <time.h>

using namespace streamadmission;

namespace
{

const uint64_t kLive = 0x1001;
const uint64_t kOther = 0x2002;
const uint64_t kPip = 0x3003;

// Transponder ids as zapit builds them: frequency, position, tsid, onid.
const uint64_t kLiveTp = 0x2cec00c0044d0001ULL;
const uint64_t kOtherTp = 0x2ed400c0043e0001ULL;

// The picture on frontend fe, tuned, one PiP slot, nothing in it.
Facts liveOn(int fe)
{
	Facts f;
	f.live_dvb = true;
	f.live_tuned = true;
	f.live_channel = kLive;
	f.live_fe = fe;
	f.slots = 1;
	return f;
}

/* As liveOn, the live frontend tuned to kLiveTp, and a request for a
   channel on transponder tp. */
Facts sharingOn(int fe, uint64_t tp)
{
	Facts f = liveOn(fe);
	f.live_tp = kLiveTp;
	f.channel_tp = tp;
	return f;
}

void pipOn(Facts &f, int slot, int fe, uint64_t channel)
{
	f.slot[slot].active = true;
	f.slot[slot].fe = fe;
	f.slot[slot].channel = channel;
}

bool has(const std::vector<int> &v, int fe)
{
	return std::find(v.begin(), v.end(), fe) != v.end();
}

} // namespace

TEST_CASE("the live channel streams from the picture's frontend", "[zapit][admission]")
{
	Facts f = liveOn(0);
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("another channel needs an idle frontend", "[zapit][admission]")
{
	Facts f = liveOn(0);
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("the channel a PiP shows is refused", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	REQUIRE(decide(f, kPip) == Refuse);
	// Even when it is the live channel as well
	pipOn(f, 0, 1, kLive);
	REQUIRE(decide(f, kLive) == Refuse);
}

TEST_CASE("a slot that is not active is not looked at", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.slot[0].channel = kOther;  // left behind, its frontend gone
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("the live channel is refused when a PiP reads the live demux from elsewhere", "[zapit][admission]")
{
	/* Live on frontend 0 reads demux 0; a stream on it reads demux 1 and sets
	   that demux's source to frontend 0. PiP slot 0 reads demux 1 from
	   frontend 1. */
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	REQUIRE(decide(f, kLive) == Refuse);
}

TEST_CASE("the live channel is admitted when the PiP on its demux has the same source", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("the live channel is admitted when the PiP reads another demux", "[zapit][admission]")
{
	// Live on frontend 1 reads demux 2 for a stream; slot 0 reads demux 1.
	Facts f = liveOn(1);
	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("the live channel on frontend 0 is refused while the AV-input PiP reads demux 1", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.avinput_pip = true;
	REQUIRE(decide(f, kLive) == Refuse);
	// On another frontend its demux is not demux 1.
	f = liveOn(1);
	f.avinput_pip = true;
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("the live channel is not live in standby, untuned or from the web", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.standby = true;
	REQUIRE(decide(f, kLive) == Idle);

	f = liveOn(0);
	f.live_tuned = false;
	REQUIRE(decide(f, kLive) == Idle);

	f = liveOn(0);
	f.live_dvb = false;
	REQUIRE(decide(f, kLive) == Idle);

	f = liveOn(-1);
	REQUIRE(decide(f, kLive) == Idle);
}

TEST_CASE("B1: in the box's standby with zapit awake the live channel takes an idle frontend", "[zapit][admission]")
{
	/* zapit stays awake for the EPG scan: not in its own standby, the scan's
	   channel tuned on frontend 0. Nothing is shown. */
	Facts f = liveOn(0);
	f.box_standby = true;
	REQUIRE_FALSE(liveRuns(f));
	REQUIRE(decide(f, kLive) == Idle);
	std::vector<int> h;
	held(f, h);
	REQUIRE_FALSE(has(h, 0));
}

TEST_CASE("B2: without the box's standby the live channel is the picture's, as before", "[zapit][admission]")
{
	Facts f = liveOn(0);
	REQUIRE(liveRuns(f));
	REQUIRE(decide(f, kLive) == Live);
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 0));
}

TEST_CASE("B3: zapit's own standby alone still frees the live frontend, as before", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.standby = true;
	REQUIRE_FALSE(liveRuns(f));
	REQUIRE(decide(f, kLive) == Idle);
	std::vector<int> h;
	held(f, h);
	REQUIRE_FALSE(has(h, 0));
}

TEST_CASE("a refused live channel is not offered an idle frontend instead", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("A1: while a recording starts no stream is admitted, live or idle", "[zapit][admission]")
{
	Facts f = liveOn(0);
	REQUIRE(decide(f, kLive) == Live);
	REQUIRE(decide(f, kOther) == Idle);

	f.recording_starting = true;
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Refuse);

	// In standby, where every stream would be an idle one.
	f.standby = true;
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("a build whose GUI starts PiPs itself refuses every stream", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.variant = QuadPip;
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("with dynamic demux only the live channel streams, and only without PiP", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.variant = DynamicDemux;
	REQUIRE(decide(f, kLive) == Live);
	REQUIRE(decide(f, kOther) == Refuse);

	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Refuse);

	f = liveOn(0);
	f.variant = DynamicDemux;
	f.avinput_pip = true;
	f.live_fe = 1;
	REQUIRE(decide(f, kLive) == Refuse);
}

TEST_CASE("on the PiP demux variant both ways need the PiP off", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.variant = Cst;
	REQUIRE(decide(f, kLive) == Live);
	REQUIRE(decide(f, kOther) == Idle);

	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kLive) == Refuse);
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("slots beyond the box's own are not read", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.slots = 1;
	pipOn(f, 2, 1, kOther);   // garbage in a slot this box does not have
	REQUIRE(decide(f, kOther) == Idle);

	f.slots = 3;
	REQUIRE(decide(f, kOther) == Refuse);

	f.slots = 9;   // more than there can be: read as the most there can be
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("held: the picture's frontend while live runs, also untuned", "[zapit][admission]")
{
	Facts f = liveOn(1);
	f.live_tuned = false;
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 1));

	f.standby = true;
	h.clear();
	held(f, h);
	REQUIRE_FALSE(has(h, 1));

	f = liveOn(1);
	f.live_dvb = false;   // a web channel holds no frontend
	h.clear();
	held(f, h);
	REQUIRE(h.empty());
}

TEST_CASE("held: a PiP slot's frontend and the frontend with the slot's number", "[zapit][admission]")
{
	Facts f = liveOn(0);
	f.slots = 2;
	pipOn(f, 1, 2, kPip);   // slot 1 reads demux 2 from frontend 2
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 0));     // the picture
	REQUIRE(has(h, 2));     // the slot's frontend
	REQUIRE(has(h, 1));     // a stream on frontend 1 would read demux 2
	REQUIRE_FALSE(has(h, 3));
}

TEST_CASE("held: frontend 0 while the AV-input PiP reads demux 1", "[zapit][admission]")
{
	Facts f = liveOn(1);
	f.avinput_pip = true;
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 0));
	REQUIRE(has(h, 1));
}

TEST_CASE("held: live on 0 and PiP slot 0 on 1 hold both, leaving frontend 2", "[zapit][admission]")
{
	Facts f = liveOn(0);
	pipOn(f, 0, 1, kPip);
	std::vector<int> h;
	held(f, h);
	REQUIRE(has(h, 0));
	REQUIRE(has(h, 1));
	REQUIRE_FALSE(has(h, 2));
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("another channel on the live transponder shares the picture's frontend", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	REQUIRE(decide(f, kOther) == Shared);
	// The live channel itself stays the live way.
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("a channel on another transponder still needs an idle frontend", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kOtherTp);
	REQUIRE(decide(f, kOther) == Idle);

	// Only the whole id counts: the same tsid and onid at another frequency
	// is another transponder.
	f = sharingOn(0, (kLiveTp & 0x0000ffffffffffffULL) | (0x2ee8ULL << 48));
	REQUIRE(decide(f, kOther) == Idle);
	// as is the same frequency at another position.
	f = sharingOn(0, (kLiveTp & 0xffff0000ffffffffULL) | (0x0082ULL << 32));
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("a live frontend that never had a transponder is shared with nobody", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, 0);
	f.live_tp = 0;
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("the live transponder is not shared in standby, untuned or from the web", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	f.standby = true;
	REQUIRE(decide(f, kOther) == Idle);

	// The EPG scan in the box's standby tunes frontend 0 with nothing shown.
	f = sharingOn(0, kLiveTp);
	f.box_standby = true;
	REQUIRE(decide(f, kOther) == Idle);

	f = sharingOn(0, kLiveTp);
	f.live_tuned = false;
	REQUIRE(decide(f, kOther) == Idle);

	f = sharingOn(0, kLiveTp);
	f.live_dvb = false;
	REQUIRE(decide(f, kOther) == Idle);

	f = sharingOn(-1, kLiveTp);
	REQUIRE(decide(f, kOther) == Idle);
}

TEST_CASE("the live transponder is not shared when a PiP reads the live demux from elsewhere", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	pipOn(f, 0, 1, kPip);
	REQUIRE(decide(f, kOther) == Idle);   // as any other channel

	// The PiP on the live demux with the same source does not stand in the way.
	f = sharingOn(0, kLiveTp);
	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kOther) == Shared);

	// Nor one that reads another demux.
	f = sharingOn(1, kLiveTp);
	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kOther) == Shared);
}

TEST_CASE("the live transponder on frontend 0 is not shared while the AV-input PiP reads demux 1", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	f.avinput_pip = true;
	REQUIRE(decide(f, kOther) == Idle);

	f = sharingOn(1, kLiveTp);
	f.avinput_pip = true;
	REQUIRE(decide(f, kOther) == Shared);
}

TEST_CASE("a channel the PiP shows is refused on the live transponder too", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	pipOn(f, 0, 0, kOther);
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("while a channel scan may tune the live frontend its transponder is not shared", "[zapit][admission][shared]")
{
	/* The scan tunes the live frontend without moving its transponder id,
	   so the id still names the transponder of the live channel. */
	Facts f = sharingOn(0, kLiveTp);
	f.scanning = true;
	REQUIRE(decide(f, kOther) == Idle);   // as any other channel
	REQUIRE(decide(f, kLive) == Live);    // the live channel is not looked at here

	f = sharingOn(0, kLiveTp);
	f.scanning = true;
	f.variant = DynamicDemux;
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("where the CI module serves one channel only, a second CI channel does not share", "[zapit][admission][shared]")
{
	// Both marked for the module and no other CI channel allowed: as any other channel.
	Facts f = sharingOn(0, kLiveTp);
	f.live_ci = true;
	f.channel_ci = true;
	f.ci_one_service = true;
	REQUIRE(decide(f, kOther) == Idle);
	REQUIRE(decide(f, kLive) == Live);   // the live channel is its own service

	f.variant = DynamicDemux;
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("CI channels share the live transponder where the module may serve more", "[zapit][admission][shared]")
{
	// All channels (0) and the same transponder (1) are the one setting: false.
	Facts f = sharingOn(0, kLiveTp);
	f.live_ci = true;
	f.channel_ci = true;
	f.ci_one_service = false;
	REQUIRE(decide(f, kOther) == Shared);
}

TEST_CASE("a channel shares with one CI channel when only one of the two is marked", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	f.ci_one_service = true;
	f.live_ci = true;
	REQUIRE(decide(f, kOther) == Shared);

	f = sharingOn(0, kLiveTp);
	f.ci_one_service = true;
	f.channel_ci = true;
	REQUIRE(decide(f, kOther) == Shared);

	f = sharingOn(0, kLiveTp);
	f.ci_one_service = true;
	REQUIRE(decide(f, kOther) == Shared);
}

TEST_CASE("with no_same_fe the live frontend carries only its own service", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	f.no_same_fe = true;
	REQUIRE(decide(f, kOther) == Idle);
	REQUIRE(decide(f, kLive) == Live);
}

TEST_CASE("the variants: no sharing with dynamic demux or quad PiP, on the PiP demux variant only without PiP", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	f.variant = DynamicDemux;
	REQUIRE(decide(f, kOther) == Refuse);
	REQUIRE(decide(f, kLive) == Live);

	f = sharingOn(0, kLiveTp);
	f.variant = QuadPip;
	REQUIRE(decide(f, kOther) == Refuse);

	f = sharingOn(0, kLiveTp);
	f.variant = Cst;
	REQUIRE(decide(f, kOther) == Shared);
	pipOn(f, 0, 0, kPip);
	REQUIRE(decide(f, kOther) == Refuse);
}

TEST_CASE("while a recording starts the live transponder is not shared either", "[zapit][admission][shared]")
{
	Facts f = sharingOn(0, kLiveTp);
	f.recording_starting = true;
	REQUIRE(decide(f, kOther) == Refuse);
}

/* The writers of PiP state that run on other threads than zapit's, against
   the admission zapit runs (streamholdcmd::run, the shipped object), on a
   model: one lock standing for the frontend manager's, the facts read from
   the model's state, a demux source table both sides write, and the demux
   1 reader the PiP is. The admission must never set the source of demux 1
   while a PiP reads it. check-pip-source-order.sh holds the program's code
   to the order these writers follow. */

namespace
{

// A point one thread waits at until the case lets it go.
struct Hold
{
	pthread_mutex_t m;
	pthread_cond_t c;
	bool armed, arrived, go;

	Hold() : armed(false), arrived(false), go(false)
	{
		pthread_mutex_init(&m, 0);
		pthread_cond_init(&c, 0);
	}
	~Hold()
	{
		pthread_cond_destroy(&c);
		pthread_mutex_destroy(&m);
	}
	void pass()
	{
		pthread_mutex_lock(&m);
		if (armed)
		{
			armed = false;
			arrived = true;
			pthread_cond_broadcast(&c);
			while (!go)
				pthread_cond_wait(&c, &m);
		}
		pthread_mutex_unlock(&m);
	}
	void waitArrived()
	{
		struct timespec until;
		clock_gettime(CLOCK_REALTIME, &until);
		until.tv_sec += 10;
		pthread_mutex_lock(&m);
		while (!arrived)
			if (pthread_cond_timedwait(&c, &m, &until) == ETIMEDOUT)
				break;
		const bool got = arrived;
		pthread_mutex_unlock(&m);
		REQUIRE(got);
	}
	void letGo()
	{
		pthread_mutex_lock(&m);
		go = true;
		pthread_cond_broadcast(&c);
		pthread_mutex_unlock(&m);
	}
};

/* The box: live on frontend 0, so a stream of the live channel binds demux 1
   with source 0; frontends 1 and 2 idle. */
struct PipModel : public streamholdcmd::Ops
{
	pthread_mutex_t fe_lock;     // the frontend manager's lock

	// What the facts are read from, written by the PiP writers under fe_lock.
	bool avinput_flag;
	bool slot_active;
	int  slot_fe;

	// What the hardware does, written without the lock.
	std::atomic<int>  source1;          // the source demux 1 reads
	std::atomic<bool> pip_reads_demux1;  // a PiP decodes demux 1 right now

	std::atomic<int> bound_demux1_under_pip;
	std::atomic<int> bound_demux1;
	int fe[3];

	Hold in_facts;   // the admission, holding the lock, reading the facts

	PipModel()
		: avinput_flag(false), slot_active(false), slot_fe(-1), source1(-1),
		  pip_reads_demux1(false), bound_demux1_under_pip(0), bound_demux1(0)
	{
		pthread_mutex_init(&fe_lock, 0);
		fe[0] = 0;
		fe[1] = 1;
		fe[2] = 2;
	}
	~PipModel() { pthread_mutex_destroy(&fe_lock); }

	void setSource(int demux, int src)
	{
		if (demux != 1)
			return;
		bound_demux1++;
		if (pip_reads_demux1.load())
			bound_demux1_under_pip++;
		source1 = src;
	}

	void collect() {}
	bool serving() { return true; }
	int64_t nowMs() { return 1; }
	streamholdcmd::Kind channel(uint64_t) { return streamholdcmd::Broadcast; }
	bool roomForHold() { return true; }
	void lockFrontends() { pthread_mutex_lock(&fe_lock); }
	void unlockFrontends() { pthread_mutex_unlock(&fe_lock); }
	void facts(streamadmission::Facts &f)
	{
		in_facts.pass();
		f.live_dvb = true;
		f.live_tuned = true;
		f.live_channel = kLive;
		f.live_fe = 0;
		f.slots = 1;
		f.slot[0].active = slot_active;
		f.slot[0].channel = kPip;
		f.slot[0].fe = slot_fe;
		f.avinput_pip = avinput_flag;
	}
	void heldByStreams(std::vector<int> &) {}
	void *holdLive(bool)
	{
		setSource(1, 0);
		return &fe[0];
	}
	void openFrontends() {}
	void *pickIdle(const std::vector<int> &held, bool bind, bool)
	{
		for (int i = 0; i < 3; ++i)
		{
			if (std::find(held.begin(), held.end(), i) != held.end())
				continue;
			if (bind)
				setSource(i + 1, i);
			return &fe[i];
		}
		return 0;
	}
	int number(void *p) { return *static_cast<int *>(p); }
	bool tune(void *) { return true; }
	bool readPids(void *) { return true; }
	void unlockFrontend(void *) {}
	uint32_t offer(void *) { return 5; }
	void drop(uint32_t) {}
	bool answer(const streamholdcmd::Reply &) { return true; }
};

struct Admission
{
	PipModel &box;
	uint64_t channel;
	streamholdcmd::Reply got;
	pthread_t thread;

	Admission(PipModel &b, uint64_t ch) : box(b), channel(ch)
	{
		REQUIRE(pthread_create(&thread, 0, &go, this) == 0);
	}
	void join() { pthread_join(thread, 0); }
	static void *go(void *arg)
	{
		Admission *a = static_cast<Admission *>(arg);
		a->got = streamholdcmd::run(a->box, a->channel, 0, false, false);
		return 0;
	}
};

/* The AV-input PiP as StartAVInputPiP starts it: the flag under the lock,
   then demux 1 switched to the input (source 2) and read. Stops are the
   points a case holds it at. */
struct AvPip
{
	PipModel &box;
	Hold before_lock, in_lock, before_switch;
	pthread_t thread;
	bool started;

	explicit AvPip(PipModel &b) : box(b), started(false) {}
	void start()
	{
		started = true;
		REQUIRE(pthread_create(&thread, 0, &go, this) == 0);
	}
	void join()
	{
		if (started)
			pthread_join(thread, 0);
	}
	static void *go(void *arg)
	{
		AvPip *p = static_cast<AvPip *>(arg);
		p->before_lock.pass();
		pthread_mutex_lock(&p->box.fe_lock);
		p->in_lock.pass();
		p->box.avinput_flag = true;
		pthread_mutex_unlock(&p->box.fe_lock);
		p->before_switch.pass();
		p->box.source1 = 2;
		p->box.pip_reads_demux1 = true;
		return 0;
	}
};

} // namespace

TEST_CASE("AV-input PiP: an admission holding the lock first is overwritten by the switch", "[zapit][admission][avpip]")
{
	PipModel box;
	AvPip pip(box);
	box.in_facts.armed = true;
	Admission a(box, kLive);
	box.in_facts.waitArrived();      // the admission holds the lock

	pip.before_lock.armed = true;
	pip.start();
	pip.before_lock.waitArrived();   // everything the PiP does before the lock is done
	pip.before_lock.letGo();         // and it waits for the lock now, or soon
	box.in_facts.letGo();
	a.join();
	pip.join();

	REQUIRE(a.got.verdict == streamholdcmd::Granted);
	REQUIRE(box.bound_demux1 == 1);
	REQUIRE(box.bound_demux1_under_pip == 0);
	REQUIRE(box.source1 == 2);       // the PiP's picture is intact
}

TEST_CASE("AV-input PiP: an admission after the flag leaves demux 1 alone", "[zapit][admission][avpip]")
{
	PipModel box;
	AvPip pip(box);
	pip.before_switch.armed = true;
	pip.start();
	pip.before_switch.waitArrived(); // the flag is out, demux 1 not yet switched

	Admission live(box, kLive);
	live.join();
	Admission other(box, kOther);
	other.join();
	pip.before_switch.letGo();
	pip.join();

	REQUIRE(live.got.verdict == streamholdcmd::Busy);
	REQUIRE(other.got.verdict == streamholdcmd::Granted);
	REQUIRE(other.got.fenumber == 1);
	REQUIRE(box.bound_demux1 == 0);
	REQUIRE(box.source1 == 2);
}

TEST_CASE("AV-input PiP: an admission waiting for the lock sees the flag", "[zapit][admission][avpip]")
{
	PipModel box;
	AvPip pip(box);
	pip.in_lock.armed = true;
	pip.start();
	pip.in_lock.waitArrived();       // the PiP holds the lock, the flag not yet set

	Admission a(box, kLive);         // waits for the lock
	pip.in_lock.letGo();
	a.join();
	pip.join();

	REQUIRE(a.got.verdict == streamholdcmd::Busy);
	REQUIRE(box.bound_demux1 == 0);
	REQUIRE(box.source1 == 2);
}

TEST_CASE("AV-input PiP: no order of the two lets an admission switch demux 1 under it", "[zapit][admission][avpip]")
{
	for (int i = 0; i < 300; ++i)
	{
		PipModel box;
		AvPip pip(box);
		pip.start();
		Admission a(box, (i & 1) ? kLive : kOther);
		a.join();
		pip.join();
		REQUIRE(box.bound_demux1_under_pip == 0);
		REQUIRE(box.source1 == 2);
	}
}

namespace
{

/* StopPip as it stops slot 0, which reads demux 1 from frontend 1: the CAM
   told, the demux and decoders stopped, and only then, under the lock, the
   slot cleared. */
struct PipStop
{
	PipModel &box;
	Hold between, in_lock;
	pthread_t thread;

	explicit PipStop(PipModel &b) : box(b) {}
	void start() { REQUIRE(pthread_create(&thread, 0, &go, this) == 0); }
	void join() { pthread_join(thread, 0); }
	static void *go(void *arg)
	{
		PipStop *p = static_cast<PipStop *>(arg);
		p->box.pip_reads_demux1 = false;   // demux and decoders stopped
		p->between.pass();
		pthread_mutex_lock(&p->box.fe_lock);
		p->in_lock.pass();
		p->box.slot_active = false;
		p->box.slot_fe = -1;
		pthread_mutex_unlock(&p->box.fe_lock);
		return 0;
	}
};

// A PiP in slot 0 on frontend 1, reading demux 1.
void pipRunning(PipModel &box)
{
	box.slot_active = true;
	box.slot_fe = 1;
	box.source1 = 1;
	box.pip_reads_demux1 = true;
}

} // namespace

TEST_CASE("StopPip: an admission while the slot is still said to be busy keeps off it", "[zapit][admission][stoppip]")
{
	PipModel box;
	pipRunning(box);
	PipStop stop(box);
	stop.between.armed = true;
	stop.start();
	stop.between.waitArrived();        // stopped, the slot not yet cleared

	Admission live(box, kLive);
	live.join();
	stop.between.letGo();
	stop.join();

	// The live channel would set demux 1, which the slot is still said to read.
	REQUIRE(live.got.verdict == streamholdcmd::Busy);
	REQUIRE(box.bound_demux1 == 0);
}

TEST_CASE("StopPip: an admission holding the lock first sees the slot busy", "[zapit][admission][stoppip]")
{
	PipModel box;
	pipRunning(box);
	box.in_facts.armed = true;
	Admission live(box, kLive);
	box.in_facts.waitArrived();

	PipStop stop(box);
	stop.start();                      // stops the devices, then waits for the lock
	box.in_facts.letGo();
	live.join();
	stop.join();

	REQUIRE(live.got.verdict == streamholdcmd::Busy);
	REQUIRE(box.bound_demux1_under_pip == 0);
}

TEST_CASE("StopPip: an admission after the slot was cleared may use demux 1", "[zapit][admission][stoppip]")
{
	PipModel box;
	pipRunning(box);
	PipStop stop(box);
	stop.start();
	stop.join();

	Admission live(box, kLive);
	live.join();
	REQUIRE(live.got.verdict == streamholdcmd::Granted);
	REQUIRE(box.bound_demux1 == 1);
	REQUIRE(box.bound_demux1_under_pip == 0);
}

TEST_CASE("StopPip: no order of the two lets an admission bind under the PiP", "[zapit][admission][stoppip]")
{
	for (int i = 0; i < 300; ++i)
	{
		PipModel box;
		pipRunning(box);
		PipStop stop(box);
		stop.start();
		Admission live(box, kLive);
		live.join();
		stop.join();
		REQUIRE(box.bound_demux1_under_pip == 0);
	}
}
