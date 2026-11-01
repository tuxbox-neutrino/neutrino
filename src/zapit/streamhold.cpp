/*
 * streamhold.cpp - the frontends zapit holds for streams, by owner
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
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
 */

#include "streamhold.h"

#include <atomic>

#include <time.h>

/* drop runs in a signal handler, which may only use atomics that need no
   lock behind them. */
static_assert(ATOMIC_INT_LOCK_FREE == 2 && ATOMIC_POINTER_LOCK_FREE == 2,
	      "the hold register needs lock-free atomics");

namespace streamhold
{

namespace
{

enum State
{
	Free = 0,
	Offered = 1,
	Adopted = 2,
	Dropped = 3
};

const unsigned kPlaces = 8;
const unsigned kPlaceBits = 3;
/* A number is the generation shifted over the place, in 32 bits: 29 bits of
   generation. The state word has room for 30, so the number is the
   narrower of the two and sets the top. */
const uint32_t kTopGeneration = (1u << 29) - 1;

/* Never destroyed and never moved, so a late drop from a signal handler, or
   an adopt after zapit has stopped, still reads a place that exists. */
struct Place
{
	std::atomic<uint32_t> word;     // generation << 2 | state
	std::atomic<void *>   fe;
	std::atomic<uint32_t> adopt_by; // nowMs() by which the owner has to come
};

Place places[kPlaces];
std::atomic<bool> closed(false);

#ifdef STREAMHOLD_TEST_POINTS
void (*test_hook)(TestPoint, unsigned) = 0;
#define TEST_POINT(p, place) do { if (test_hook) test_hook((p), (place)); } while (0)
#else
#define TEST_POINT(p, place) do { } while (0)
#endif

inline uint32_t generationOf(uint32_t word)
{
	return word >> 2;
}

inline State stateOf(uint32_t word)
{
	return (State) (word & 3u);
}

inline uint32_t wordOf(uint32_t gen, State s)
{
	return (gen << 2) | (uint32_t) s;
}

inline bool late(uint32_t now_ms, uint32_t by_ms)
{
	return (int32_t) (now_ms - by_ms) > 0;
}

/* The place and generation a number names, or false for a number no offer
   could have answered. */
bool split(HoldId id, unsigned &place, uint32_t &gen)
{
	place = id & ((1u << kPlaceBits) - 1);
	gen = id >> kPlaceBits;
	return gen != 0 && gen <= kTopGeneration;
}

} // namespace

uint32_t nowMs()
{
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (uint32_t) ts.tv_sec * 1000u + (uint32_t) (ts.tv_nsec / 1000000L);
}

HoldId offer(void *fe, uint32_t now_ms)
{
	if (closed.load(std::memory_order_acquire))
		return 0;

	for (unsigned i = 0; i < kPlaces; ++i)
	{
		Place &p = places[i];
		/* Only this thread takes a place out of FREE, and nobody else
		   changes a FREE place, so what is read here holds until the store
		   below. */
		const uint32_t w = p.word.load(std::memory_order_acquire);
		if (stateOf(w) != Free || generationOf(w) >= kTopGeneration)
			continue;

		const uint32_t gen = generationOf(w) + 1;
		p.fe.store(fe, std::memory_order_relaxed);
		p.adopt_by.store(now_ms + kAdoptWithinMs, std::memory_order_relaxed);
		// Published together with the two fields above.
		p.word.store(wordOf(gen, Offered), std::memory_order_release);
		return (gen << kPlaceBits) | i;
	}
	return 0;
}

bool canOffer()
{
	if (closed.load(std::memory_order_acquire))
		return false;
	for (unsigned i = 0; i < kPlaces; ++i)
	{
		const uint32_t w = places[i].word.load(std::memory_order_acquire);
		if (stateOf(w) == Free && generationOf(w) < kTopGeneration)
			return true;
	}
	return false;
}

void *adopt(HoldId id, uint32_t now_ms)
{
	unsigned i;
	uint32_t gen;
	if (!split(id, i, gen))
		return 0;
	Place &p = places[i];

	for (;;)
	{
		uint32_t w = p.word.load(std::memory_order_acquire);
		TEST_POINT(AfterAdoptLoad, i);
		if (generationOf(w) != gen || stateOf(w) != Offered)
			return 0;

		/* Read before the compare and swap that makes it this caller's. Once
		   the hold is adopted it may be dropped, collected and the place
		   offered again with another frontend at any moment; a frontend
		   read after the swap could be that one. */
		void *fe = p.fe.load(std::memory_order_relaxed);
		const uint32_t by = p.adopt_by.load(std::memory_order_relaxed);
		// Past its time: never handed out, collect gives it back.
		if (late(now_ms, by))
			return 0;

		if (p.word.compare_exchange_strong(w, wordOf(gen, Adopted),
						   std::memory_order_acq_rel,
						   std::memory_order_acquire))
		{
			TEST_POINT(AfterAdoptCas, i);
			return fe;
		}
	}
}

void drop(HoldId id)
{
	unsigned i;
	uint32_t gen;
	if (!split(id, i, gen))
		return;
	Place &p = places[i];

	for (;;)
	{
		uint32_t w = p.word.load(std::memory_order_acquire);
		TEST_POINT(AfterDropLoad, i);
		if (generationOf(w) != gen)
			return;
		const State s = stateOf(w);
		if (s != Offered && s != Adopted)
			return;
		if (p.word.compare_exchange_strong(w, wordOf(gen, Dropped),
						   std::memory_order_acq_rel,
						   std::memory_order_acquire))
			return;
	}
}

void collect(uint32_t now_ms, void (*release)(void *fe))
{
	for (unsigned i = 0; i < kPlaces; ++i)
	{
		Place &p = places[i];
		for (;;)
		{
			uint32_t w = p.word.load(std::memory_order_acquire);
			TEST_POINT(AfterCollectLoad, i);
			const State s = stateOf(w);
			const bool due = s == Dropped ||
				(s == Offered && late(now_ms, p.adopt_by.load(std::memory_order_relaxed)));
			if (!due)
				break;
			if (p.word.compare_exchange_strong(w, wordOf(generationOf(w), Free),
							   std::memory_order_acq_rel,
							   std::memory_order_acquire))
			{
				if (release)
					release(p.fe.load(std::memory_order_relaxed));
				break;
			}
			// An owner adopted or dropped it in between: look again.
		}
	}
}

void shutdown(void (*release)(void *fe))
{
	closed.store(true, std::memory_order_release);
	for (unsigned i = 0; i < kPlaces; ++i)
	{
		Place &p = places[i];
		for (;;)
		{
			uint32_t w = p.word.load(std::memory_order_acquire);
			if (stateOf(w) == Free)
				break;
			if (p.word.compare_exchange_strong(w, wordOf(generationOf(w), Free),
							   std::memory_order_acq_rel,
							   std::memory_order_acquire))
			{
				if (release)
					release(p.fe.load(std::memory_order_relaxed));
				break;
			}
		}
	}
}

size_t revoke(void *fe, void (*release)(void *fe))
{
	size_t n = 0;
	for (unsigned i = 0; i < kPlaces; ++i)
	{
		Place &p = places[i];
		for (;;)
		{
			uint32_t w = p.word.load(std::memory_order_acquire);
			/* Only this thread sets a place's frontend, and only while the
			   place is FREE, so the frontend read here is the one of the
			   generation in w. */
			if (stateOf(w) == Free || p.fe.load(std::memory_order_relaxed) != fe)
				break;
			if (p.word.compare_exchange_strong(w, wordOf(generationOf(w), Free),
							   std::memory_order_acq_rel,
							   std::memory_order_acquire))
			{
				if (release)
					release(fe);
				n++;
				break;
			}
			// An owner adopted or dropped it in between: look again.
		}
	}
	return n;
}

size_t held(void **out, size_t max)
{
	size_t n = 0;
	for (unsigned i = 0; i < kPlaces && n < max; ++i)
	{
		const uint32_t w = places[i].word.load(std::memory_order_acquire);
		if (stateOf(w) != Free)
			out[n++] = places[i].fe.load(std::memory_order_relaxed);
	}
	return n;
}

#ifdef STREAMHOLD_TEST_POINTS
void setTestHook(void (*hook)(TestPoint, unsigned))
{
	test_hook = hook;
}

void setGenerationForTest(unsigned place, uint32_t gen)
{
	places[place].word.store(wordOf(gen, Free));
}

void resetForTest()
{
	test_hook = 0;
	closed.store(false);
	for (unsigned i = 0; i < kPlaces; ++i)
	{
		places[i].word.store(0);
		places[i].fe.store(0);
		places[i].adopt_by.store(0);
	}
}
#endif

} // namespace streamhold
