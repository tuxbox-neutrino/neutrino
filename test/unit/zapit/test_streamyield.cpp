/*
 * test_streamyield.cpp - the command that ends stream holds for a recording
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * streamyield::run is the order zapit follows when a recording asks it to
 * end the stream holds it needs. These cases drive the shipped object over a
 * model that writes down every step it is asked for, and whether the
 * frontend manager's lock was held at the time.
 */

#include "support/catch.hpp"

#include <zapit/streamyield.h>

#include <string>
#include <vector>

using namespace streamyield;

namespace
{

const uint64_t kChannel = 0x1001;

struct Box : public Ops
{
	std::vector<std::string> steps;
	bool serving_now;
	int64_t clock;
	int stage;
	std::vector<int> end;
	int lock_depth;
	bool revoked_unlocked;
	Reply sent;
	int answers;

	Box()
		: serving_now(true), clock(1000), stage(2), lock_depth(0), revoked_unlocked(false), answers(0)
	{
		sent.stage = 99;
		sent.fe_mask = 99;
	}

	void collect() { steps.push_back("collect"); }
	bool serving() { return serving_now; }
	int64_t nowMs() { return clock; }
	void lockFrontends()
	{
		lock_depth++;
		steps.push_back("lock");
	}
	void unlockFrontends()
	{
		lock_depth--;
		steps.push_back("unlock");
	}
	int search(uint64_t, std::vector<int> &out)
	{
		steps.push_back(lock_depth > 0 ? "search" : "search-unlocked");
		out = end;
		return stage;
	}
	void revoke(int fenumber)
	{
		revoked_unlocked = revoked_unlocked || lock_depth == 0;
		steps.push_back("revoke " + std::to_string(fenumber));
	}
	bool answer(const Reply &r)
	{
		answers++;
		sent = r;
		steps.push_back("answer");
		return true;
	}

	size_t at(const std::string &step) const
	{
		for (size_t i = 0; i < steps.size(); ++i)
			if (steps[i] == step)
				return i;
		return steps.size();
	}
	int count(const std::string &prefix) const
	{
		int n = 0;
		for (size_t i = 0; i < steps.size(); ++i)
			if (steps[i].compare(0, prefix.size(), prefix) == 0)
				n++;
		return n;
	}
};

const int64_t kPlenty = 1000 + 12000;

} // namespace

TEST_CASE("Y1: the holds dropped since the last command are collected before the search", "[zapit][streamyield]")
{
	Box box;
	box.end.push_back(1);
	run(box, kChannel, kPlenty, true);
	REQUIRE(box.at("collect") == 0);
	REQUIRE(box.at("collect") < box.at("search"));
}

TEST_CASE("Y2: every hold is taken back under the lock, and the answer follows them", "[zapit][streamyield]")
{
	Box box;
	box.end.push_back(1);
	box.end.push_back(3);
	const Reply r = run(box, kChannel, kPlenty, true);

	REQUIRE(box.count("revoke") == 2);
	REQUIRE_FALSE(box.revoked_unlocked);
	REQUIRE(box.at("revoke 1") > box.at("lock"));
	REQUIRE(box.at("revoke 3") < box.at("unlock"));
	REQUIRE(box.at("answer") > box.at("unlock"));
	REQUIRE(box.answers == 1);
	REQUIRE(r.stage == 2);
	REQUIRE(r.fe_mask == ((1u << 1) | (1u << 3)));
	REQUIRE(box.sent.stage == 2);
	REQUIRE(box.sent.fe_mask == r.fe_mask);
}

TEST_CASE("Y3: a frontend found as it is takes nothing back", "[zapit][streamyield]")
{
	Box box;
	box.stage = 1;
	const Reply r = run(box, kChannel, kPlenty, true);
	REQUIRE(box.count("revoke") == 0);
	REQUIRE(r.stage == 1);
	REQUIRE(r.fe_mask == 0);

	box.steps.clear();
	box.stage = 0;
	REQUIRE(run(box, kChannel, kPlenty, true).stage == 0);
	REQUIRE(box.count("revoke") == 0);
}

TEST_CASE("Y4: too little time for the answer to arrive takes nothing back", "[zapit][streamyield]")
{
	Box box;
	box.end.push_back(1);

	const Reply r = run(box, kChannel, box.clock + kMarginMs - 1, true);
	REQUIRE(r.stage == kRefused);
	REQUIRE(r.fe_mask == 0);
	REQUIRE(box.count("search") == 0);
	REQUIRE(box.count("revoke") == 0);
	REQUIRE(box.answers == 1);

	// A second ahead is enough.
	box.steps.clear();
	REQUIRE(run(box, kChannel, box.clock + kMarginMs, true).stage == 2);
	REQUIRE(box.count("revoke") == 1);
}

TEST_CASE("Y5: channel 0 only collects", "[zapit][streamyield]")
{
	Box box;
	box.end.push_back(1);
	const Reply r = run(box, 0, kPlenty, true);
	REQUIRE(r.stage == 0);
	REQUIRE(r.fe_mask == 0);
	REQUIRE(box.steps.size() == 2);
	REQUIRE(box.steps[0] == "collect");
	REQUIRE(box.steps[1] == "answer");
}

TEST_CASE("Y6: where a hold locks nothing, a recording takes the frontend as it is", "[zapit][streamyield]")
{
	Box box;
	box.end.push_back(1);
	const Reply r = run(box, kChannel, kPlenty, false);
	REQUIRE(r.stage == 0);
	REQUIRE(r.fe_mask == 0);
	REQUIRE(box.at("collect") == 0);
	REQUIRE(box.count("search") == 0);
	REQUIRE(box.count("revoke") == 0);
}

TEST_CASE("a zapit that stops refuses after collecting", "[zapit][streamyield]")
{
	Box box;
	box.end.push_back(1);
	box.serving_now = false;
	const Reply r = run(box, kChannel, kPlenty, true);
	REQUIRE(r.stage == kRefused);
	REQUIRE(box.at("collect") == 0);
	REQUIRE(box.count("search") == 0);
	REQUIRE(box.count("revoke") == 0);
}
