/*
 * streamhold.h - the frontends zapit holds for streams, by owner
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
 *
 * zapit binds a frontend for a stream on its own thread and answers the
 * stream server with a number. The hold has to end exactly once whatever
 * happens to that answer: it may never arrive, the stream may fail to start,
 * the stream may end, the program may be stopped by a signal. So the hold is
 * entered here, under a number, before the answer goes out, and only zapit
 * ever gives a frontend back - after the owner dropped the number, after the
 * owner never came for it, or when zapit stops.
 *
 * Eight places, each a word with a generation and a state:
 *
 *   FREE --offer--> OFFERED --adopt--> ADOPTED
 *                      |                  |
 *                      +------drop--------+--> DROPPED --collect--> FREE
 *   OFFERED past its time --collect--> FREE
 *   anything but FREE --shutdown--> FREE
 *
 * A number is the generation and the place together. Every operation on a
 * number checks the generation, so a number that is done with matches
 * nothing, and a place whose generation has reached the top is retired
 * rather than wrapped. A frontend is given back only by the compare and swap
 * that takes a place from another state to FREE, and only offer leaves FREE,
 * with a new generation: so each generation reaches FREE once, and its
 * frontend is given back once.
 *
 * offer, collect, shutdown and held run on zapit's thread only. adopt and
 * drop run on any thread; drop also in a signal handler, so neither takes a
 * lock nor reads a clock. Each of their loops ends after at most three
 * rounds, because a failed compare and swap means the place moved on within
 * the same generation, and a generation moves at most through OFFERED,
 * ADOPTED, DROPPED and FREE.
 */

#ifndef __zapit_streamhold_h__
#define __zapit_streamhold_h__

#include <stddef.h>
#include <stdint.h>

namespace streamhold
{

typedef uint32_t HoldId;

// How long an offered hold waits for its owner to come for it.
const uint32_t kAdoptWithinMs = 5000;

// The monotonic clock in milliseconds. It wraps; times are compared by their
// signed difference.
uint32_t nowMs();

/* zapit's thread only. */

/* Enters a hold on fe and answers its number, or 0 when no place is free or
   the register is closed. The owner has kAdoptWithinMs to adopt it. */
HoldId offer(void *fe, uint32_t now_ms);

// Whether offer would find a place now.
bool canOffer();

/* Gives back the frontend of every dropped hold and of every offered hold
   whose owner did not come in time, each through release, once. */
void collect(uint32_t now_ms, void (*release)(void *fe));

/* Closes the register and gives back every frontend still held. Afterwards
   offer answers 0, adopt answers NULL and drop does nothing. */
void shutdown(void (*release)(void *fe));

/* Writes the frontend of every place in use to out, at most max of them, and
   answers how many. A dropped hold counts until it is collected. */
size_t held(void **out, size_t max);

/* Any thread. */

/* Takes over an offered hold and answers its frontend, or NULL when the
   number is unknown, done with, already adopted or past its time. The
   frontend is the one the hold was offered with; it stays valid only for as
   long as zapit runs - a caller checks that the program is still serving
   before it uses it. */
void *adopt(HoldId id, uint32_t now_ms);

/* Ends a hold, offered or adopted; zapit gives its frontend back after the
   command it is running. Does nothing for a number that is done with. Safe
   in a signal handler. */
void drop(HoldId id);

#ifdef STREAMHOLD_TEST_POINTS
/* Seams for the cases, compiled into the test binary only. */
enum TestPoint
{
	AfterAdoptLoad,
	AfterAdoptCas,
	AfterDropLoad,
	AfterCollectLoad
};
void setTestHook(void (*hook)(TestPoint, unsigned place));
void setGenerationForTest(unsigned place, uint32_t gen);
void resetForTest();
const unsigned kPlacesForTest = 8;
const uint32_t kTopGenerationForTest = (1u << 29) - 1;
#endif

} // namespace streamhold

#endif
