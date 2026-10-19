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

/* The other half of a setting that is one fact in two parts, NULL for any other. writes,
   where given, says how the two are written: "id" where the identifier alone is written
   and the box fills in the other half, "both" where the two are written together. */
const char *pairPartner(const std::string &key, const char **writes = NULL);

// "tv" or "radio" for a start channel row, which list its channel comes from; NULL for any other.
const char *startChannelKind(const std::string &key);

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

	// The number as it stands once the batch has landed. False when it cannot be read as one.
	bool currentNumber(const char *key, long &out) const;

	// Whether the batch writes key with another value than the stored one, or one that cannot be read.
	bool changes(const char *key) const;

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

	/* Takes the member out of the batch and answers it as a condition that does not hold,
	   naming the settings because_of that refused it. */
	void refuse(const char *key, const char *message, const std::vector<std::string> &because_of);
	void refuse(const char *key, const char *message, const char *because_of);

	// Takes the member out of the batch and answers it with e, for a refusal that is no condition.
	void refuseWith(const char *key, const Error &e);

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

/* setting-condition-not-met naming the settings that refused it, in the detail after
   message and in depends_on. */
Error conditionRefusal(const char *message, const std::vector<std::string> &because_of);

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
/* Two settings whose rows' conditions only read each other and whose coupling judges the pair a
   batch leaves. Within a batch the coupling's answer stands in for those conditions. */
extern const KeyPair kJudgedPairs[];
extern const size_t kJudgedPairCount;

// True when every condition of the row reads the partner of a judged pair it is one half of.
bool judgedByCoupling(const Descriptor &row);

/* Adds to keys the partner of each key that is one half of a pair, so a reset of one
   half is a reset of both. */
void addPartners(std::vector<std::string> &keys);

// One per area, each in its own file.
void coupleEpg(CoupledBatch &b);
void coupleOsd(CoupledBatch &b);
void coupleChannel(CoupledBatch &b);
void coupleWeather(CoupledBatch &b);
void couplePlugins(CoupledBatch &b);
void coupleCi(CoupledBatch &b);
void coupleLcd4l(CoupledBatch &b);

/* What the panel of the LCD4Linux display takes, by its type as the driver numbers it:
   the highest brightness and whether a skin is one the panel has. */
long lcd4lBrightnessCeiling(long display_type);
bool lcd4lSkinOffered(long display_type, long skin);

} // namespace settings
} // namespace coreapi

#endif
