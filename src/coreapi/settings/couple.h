/*
 * couple.h - the settings a write of several keeps together
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_couple_h__
#define __coreapi_couple_h__

#include "coreapi/base/result.h"
#include "settings.h"

#include <string>
#include <utility>
#include <vector>

namespace coreapi
{
namespace settings
{

/* One setting of a write of several: one the caller named, or one the write added
   because of the member named in because, which is empty for the caller's own. */
struct BatchMember
{
	std::string key;
	std::string value;
	std::string because;
};

/* A member the write does not store, with the answer for it and the settings that took
   it down, none where its own value was refused. */
struct BatchRefusal
{
	std::string key;
	Error error;
	std::vector<std::string> depends_on;
};

/* Holds a write of several settings to the ones that are one fact in two parts, before
   anything is stored.

   A start channel is what the box zaps to, its identifier, and the name beside it is what
   a person reads and follows from the channel list. The identifier written alone is taken
   and brings the name of its channel with it; the name written alone names no channel and
   is refused; both together are taken as sent, the way the box's own chooser writes them.

   The weather reads only the location, so a city written without it renames the forecast
   and leaves it where it was: the two are written together or not at all. A place written
   that is not the one the box holds also empties the postal code a search had entered,
   which no longer describes it, and a postal code the same write gives with text is
   refused with the city.

   Then every member is held to check(), and what passed to its conditions, the row's and
   those of the entry its value names, judged on what the members left leave behind
   (conditionsAllow()). One refused takes down what belongs with it: its partner, what it
   brought, and what brought it, so a pair never lands in part for a value that could have
   been foreseen. refused may already hold members the caller
   refused by rules of its own; those count as written for the pairs and take their
   partners down the same way. What is left in batch is what the caller stores. */
void keepTogether(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused);

/* setting-condition-not-met naming the settings that refused it, in the detail after
   message and in depends_on. */
Error conditionRefusal(const char *message, const std::vector<std::string> &because_of);

/* The other half of a setting that is one fact in two parts, NULL for any other. writes,
   where given, says how the two are written: "id" where the identifier alone is written
   and the box fills in the other half, "both" where the two are written together. */
const char *pairPartner(const std::string &key, const char **writes = NULL);

// "tv" or "radio" for a start channel row, which list its channel comes from; NULL for any other.
const char *startChannelKind(const std::string &key);

/* What the panel of the LCD4Linux display takes, by its type as the driver numbers it:
   the highest brightness. */
long lcd4lBrightnessCeiling(long display_type);

/* A setting a coupling added to the batch and the member that made it do so, so that
   an addition which cannot land takes its trigger down with it and a trigger which
   cannot land leaves no addition behind. */
struct Addition
{
	std::string key;
	std::string trigger;
};

/* What one coupling sees and may change of a write of several settings. It reads the
   members the caller named and, for a setting the batch does not name, what the store
   holds, and it changes the batch only through put(), imply() and refuse(), so every
   change is held to the rules a member of the batch is held to. */
class CoupledBatch
{
public:
	CoupledBatch(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added);

	// The value the batch writes under key, NULL when it names none.
	const std::string *written(const char *key) const;

	// The number the batch writes under key. False when it names none.
	bool writtenNumber(const char *key, long &out) const;

	// The setting as it stands once the batch has landed. False when it cannot be read.
	bool current(const char *key, std::string &out) const;

	/* Puts a value that follows from the member named because, in place of one the caller
	   named for the same setting: the caller's was only ever a copy of what the coupling
	   derives. The value is held to what check() holds a member to, and where it fails
	   the member named because is refused with that answer instead, since a write that
	   cannot be kept whole must not land in part. */
	bool put(const char *key, const std::string &value, const char *because);

	/* Puts a value the member named because requires. A caller who named the same
	   setting with another value asked for two things that cannot both hold, so both
	   members are refused; the same value is no contradiction and is taken as sent. */
	bool imply(const char *key, const std::string &value, const char *because);

	// Notes that the addition under key is also due to the member named because.
	void link(const char *key, const char *because);

	// Takes the member out of the batch and answers it as a condition that does not hold.
	void refuse(const char *key, const char *message);

	/* Two settings that are one fact in two parts. Written alone, either would leave the
	   other describing something else, so a member whose partner the batch does not name
	   is refused. True when both are named and stay in. */
	bool requirePair(const char *first, const char *second);

private:
	BatchOverlay &batch_;
	Refusals &refused_;
	std::vector<Addition> &added_;
};

typedef void (*Coupling)(CoupledBatch &);

/* Runs every coupling once, in the order the registry names them. No coupling puts a
   setting that another one is triggered by, so the order decides nothing and none can
   start another. Called first by settleBatch(), before any condition is judged: a
   condition has to see the batch the couplings made. */
void applyCouplings(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added);

// How many couplings the registry runs, which bounds how long settling can take.
size_t couplingCount();

// Two settings written together or not at all, as the couplings of the pair name them.
struct KeyPair
{
	const char *first;
	const char *second;
};

extern const KeyPair kChannelPairs[];
extern const size_t kChannelPairCount;
extern const KeyPair kWeatherPairs[];
extern const size_t kWeatherPairCount;

/* Adds to keys the partner of each key that is one half of a pair, so a reset of one
   half is a reset of both. */
void addPartners(std::vector<std::string> &keys);

// One per area, each in its own file.
void coupleEpg(CoupledBatch &b);
void coupleOsd(CoupledBatch &b);
void coupleChannel(CoupledBatch &b);
void coupleWeather(CoupledBatch &b);
void couplePlugins(CoupledBatch &b);

} // namespace settings
} // namespace coreapi

#endif
