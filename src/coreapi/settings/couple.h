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

#include <string>
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
   also empties the postal code a search had entered, which no longer describes it, and a
   postal code the same write gives with text is refused with the city.

   Then every member is held to check(). One the row refuses takes down what belongs with
   it: its partner, what it brought, and what brought it, so a pair never lands in part
   for a value that could have been foreseen. refused may already hold members the caller
   refused by rules of its own; those count as written for the pairs and take their
   partners down the same way. What is left in batch is what the caller stores. */
void keepTogether(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused);

/* The other half of a setting that is one fact in two parts, NULL for any other. writes,
   where given, says how the two are written: "id" where the identifier alone is written
   and the box fills in the other half, "both" where the two are written together. */
const char *pairPartner(const std::string &key, const char **writes = NULL);

// "tv" or "radio" for a start channel row, which list its channel comes from; NULL for any other.
const char *startChannelKind(const std::string &key);

} // namespace settings
} // namespace coreapi

#endif
