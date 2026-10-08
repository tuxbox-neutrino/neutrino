/*
 * couple.cpp - the settings a write of several keeps together
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "coreapi/settings/couple.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/schema.h"
#include "coreapi/channels.h"
#include "coreapi/settings/settings.h"

#include <cerrno>
#include <cstdlib>

namespace coreapi
{
namespace settings
{

namespace
{

struct ChannelPair
{
	const char *name;
	const char *id;
	bool        radio;
};

const ChannelPair kChannelPairs[] =
{
	{ "startchanneltv", "startchanneltv_id", false },
	{ "startchannelradio", "startchannelradio_id", true }
};

const size_t kChannelPairCount = sizeof(kChannelPairs) / sizeof(kChannelPairs[0]);

const char *const kCity = "weather_city";
const char *const kLocation = "weather_location";
const char *const kPostalCode = "weather_postalcode";

const char *const kOneFact = "the setting is one fact with another and is written together with it";
const char *const kContradicts = "the setting contradicts another one written with it";

/* Which pair a key is half of, so that a refused half can take the other down: the
   index of its channel pair, the number of channel pairs for the weather place, and -1
   for a key in no pair. */
int pairOf(const std::string &key)
{
	for (size_t i = 0; i < kChannelPairCount; ++i)
	{
		if (key == kChannelPairs[i].name || key == kChannelPairs[i].id)
			return (int) i;
	}
	if (key == kCity || key == kLocation)
		return (int) kChannelPairCount;
	return -1;
}

int pairOf(const BatchMember &m)
{
	// What a write brought belongs where what brought it does.
	return pairOf(m.because.empty() ? m.key : m.because);
}

BatchMember *find(std::vector<BatchMember> &batch, const char *key)
{
	for (size_t i = 0; i < batch.size(); ++i)
	{
		if (batch[i].key == key)
			return &batch[i];
	}
	return NULL;
}

bool refusedBefore(const std::vector<BatchRefusal> &refused, const char *key)
{
	for (size_t i = 0; i < refused.size(); ++i)
	{
		if (refused[i].key == key)
			return true;
	}
	return false;
}

// Written by the caller, whether or not it is still to be stored.
bool named(std::vector<BatchMember> &batch, const std::vector<BatchRefusal> &refused, const char *key)
{
	BatchMember *m = find(batch, key);
	return (m != NULL && m->because.empty()) || refusedBefore(refused, key);
}

// Takes the member out of the batch and answers it with e.
void refuseWith(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused,
		const std::string &key, const Error &e, const std::vector<std::string> &because_of)
{
	for (size_t i = 0; i < batch.size(); ++i)
	{
		if (batch[i].key != key)
			continue;
		BatchRefusal r;
		r.key = key;
		r.error = e;
		r.depends_on = because_of;
		refused.push_back(r);
		batch.erase(batch.begin() + i);
		return;
	}
}

/* Takes the member out of the batch as one that cannot be kept with the settings
   named, which the answer lists after the message. */
void refuseFor(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused,
	       const std::string &key, const char *message, const std::vector<std::string> &because_of)
{
	refuseWith(batch, refused, key, conditionRefusal(message, because_of), because_of);
}

void refuseFor(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused,
	       const std::string &key, const char *message, const char *because_of)
{
	refuseFor(batch, refused, key, message, std::vector<std::string>(1, because_of));
}

/* The name the identifier stands for, nothing for no channel at all. False with why
   where the box has no such channel, or where it is of the other kind than the list
   the start channel comes from. */
bool nameOf(unsigned long long id, bool radio, std::string &name, Error &why)
{
	if (id == 0)
	{
		name.clear();
		return true;
	}
	Result<ChannelInfo> found = channels::get((ChannelId) id);
	if (!found.ok())
	{
		why = found.error();
		return false;
	}
	const ServiceKind k = found.value().kind;
	const bool is_radio = k == ServiceKind::Radio || k == ServiceKind::WebRadio;
	if (is_radio != radio)
	{
		why = Error(Status::InvalidArgument, ErrorCode::NotAListedValue,
			    std::string("the channel is a ") + (is_radio ? "radio" : "television") +
			    " channel, and this start channel takes a " + (radio ? "radio" : "television") + " one");
		return false;
	}
	name = found.value().name;
	return true;
}

void coupleChannels(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused)
{
	for (size_t i = 0; i < kChannelPairCount; ++i)
	{
		const ChannelPair &p = kChannelPairs[i];
		const bool has_name = named(batch, refused, p.name);
		const bool has_id = named(batch, refused, p.id);
		if (has_name && !has_id)
		{
			refuseFor(batch, refused, p.name, kOneFact, p.id);
			continue;
		}
		BatchMember *id_member = find(batch, p.id);
		if (has_name || id_member == NULL)
			continue;

		// Text that is no identifier is the row's own refusal, which check() gives.
		unsigned long long id = 0;
		if (!readChannelIdText(id_member->value, id))
			continue;
		/* The channel that is already the start channel changes nothing, and one the
		   list has lost since must not refuse writing it again. */
		Result<std::string> stored = get(p.id);
		unsigned long long was = 0;
		if (stored.ok() && readChannelIdText(stored.value(), was) && was == id)
			continue;

		std::string name;
		Error why;
		if (!nameOf(id, p.radio, name, why))
		{
			refuseWith(batch, refused, p.id, why, std::vector<std::string>());
			continue;
		}
		BatchMember brought;
		brought.key = p.name;
		brought.value = name;
		brought.because = p.id;
		batch.push_back(brought);
	}
}

void coupleWeather(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused)
{
	const bool has_city = named(batch, refused, kCity);
	const bool has_location = named(batch, refused, kLocation);
	if (has_city != has_location)
	{
		if (has_city)
			refuseFor(batch, refused, kCity, kOneFact, kLocation);
		else
			refuseFor(batch, refused, kLocation, kOneFact, kCity);
		return;
	}
	if (!has_city || find(batch, kCity) == NULL)
		return;

	BatchMember *postal = find(batch, kPostalCode);
	if (postal != NULL)
	{
		// The same empty code the place brings is no contradiction and is taken as sent.
		if (!postal->value.empty())
		{
			refuseFor(batch, refused, kPostalCode, kContradicts, kCity);
			refuseFor(batch, refused, kCity, kContradicts, kPostalCode);
		}
		return;
	}
	if (refusedBefore(refused, kPostalCode))
		return;

	/* The place the box already holds, written back as it was, moves nothing, so it brings
	   nothing either, as the start channel the box already starts on does. Bringing the
	   empty postal code all the same would hold a write that changes nothing to the
	   conditions of a setting it never named, which refuse it while the weather is off. */
	const BatchMember *city = find(batch, kCity);
	const BatchMember *location = find(batch, kLocation);
	Result<std::string> city_was = get(kCity);
	Result<std::string> location_was = get(kLocation);
	if (city != NULL && location != NULL && city_was.ok() && location_was.ok() &&
	    city->value == city_was.value() && location->value == location_was.value())
		return;

	BatchMember brought;
	brought.key = kPostalCode;
	brought.because = kCity;
	batch.push_back(brought);
}

} // anonymous namespace

Error conditionRefusal(const char *message, const std::vector<std::string> &because_of)
{
	std::string said = message;
	for (size_t i = 0; i < because_of.size(); ++i)
		said += (i == 0 ? ": " : ", ") + because_of[i];
	Error e(Status::Conflict, ErrorCode::SettingConditionNotMet, said);
	e.depends_on = because_of;
	return e;
}

void keepTogether(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused)
{
	coupleChannels(batch, refused);
	coupleWeather(batch, refused);

	/* Every value to its row first, so that nothing is stored of a pair whose other half
	   would be refused. What the write brought answers for what brought it: the caller
	   sent that one and nothing else. */
	for (size_t i = 0; i < batch.size();)
	{
		Result<void> fits = check(batch[i].key, batch[i].value);
		if (fits.ok())
		{
			++i;
			continue;
		}
		const std::string key = batch[i].key;
		const std::string because = batch[i].because;
		if (because.empty())
		{
			refuseWith(batch, refused, key, fits.error(), std::vector<std::string>());
			continue;
		}
		batch.erase(batch.begin() + i);
		refuseWith(batch, refused, because, fits.error(), std::vector<std::string>());
		i = 0;
	}

	/* Then the conditions, the row's and those of the entry a value names, judged on what
	   the members left would leave behind. Every member they refuse in a round is taken out
	   together, as the source's settling does, so the order of the members does not decide
	   which of two that refuse each other lands: both are refused. A member taken out no
	   longer lands, so the rest are asked again until no answer changes; the members only
	   ever leave, so this ends. What the write brought answers for what brought it, as
	   above. */
	for (bool again = true; again;)
	{
		BatchOverlay after;
		for (size_t i = 0; i < batch.size(); ++i)
			after.values.push_back(std::make_pair(batch[i].key, batch[i].value));
		std::vector<BatchMember> failing;
		std::vector<Error> why;
		for (size_t i = 0; i < batch.size(); ++i)
		{
			Result<void> allowed = conditionsAllow(batch[i].key, batch[i].value, after);
			if (allowed.ok())
				continue;
			failing.push_back(batch[i]);
			why.push_back(allowed.error());
		}
		for (size_t f = 0; f < failing.size(); ++f)
		{
			if (failing[f].because.empty())
			{
				refuseWith(batch, refused, failing[f].key, why[f], why[f].depends_on);
				continue;
			}
			BatchMember *brought = find(batch, failing[f].key.c_str());
			if (brought != NULL)
				batch.erase(batch.begin() + (brought - &batch[0]));
			refuseWith(batch, refused, failing[f].because, why[f], why[f].depends_on);
		}
		again = !failing.empty();
	}

	/* A refused half takes the rest of its pair down, and what the write brought for it.
	   Once over the batch is enough: nothing taken down here is the half of another pair. */
	for (size_t i = 0; i < batch.size();)
	{
		const int pair = pairOf(batch[i]);
		std::vector<std::string> down;
		for (size_t r = 0; pair >= 0 && r < refused.size(); ++r)
		{
			if (pairOf(refused[r].key) == pair)
				down.push_back(refused[r].key);
		}
		if (down.empty())
		{
			++i;
			continue;
		}
		if (batch[i].because.empty())
			refuseFor(batch, refused, batch[i].key, kOneFact, down);
		else
			batch.erase(batch.begin() + i);
	}
}

const char *pairPartner(const std::string &key, const char **writes)
{
	for (size_t i = 0; i < kChannelPairCount; ++i)
	{
		const char *other = NULL;
		if (key == kChannelPairs[i].name)
			other = kChannelPairs[i].id;
		else if (key == kChannelPairs[i].id)
			other = kChannelPairs[i].name;
		if (other == NULL)
			continue;
		if (writes != NULL)
			*writes = "id";
		return other;
	}
	const char *other = key == kCity ? kLocation : key == kLocation ? kCity : NULL;
	if (other != NULL && writes != NULL)
		*writes = "both";
	return other;
}

const char *startChannelKind(const std::string &key)
{
	for (size_t i = 0; i < kChannelPairCount; ++i)
	{
		if (key == kChannelPairs[i].name || key == kChannelPairs[i].id)
			return kChannelPairs[i].radio ? "radio" : "tv";
	}
	return NULL;
}

CoupledBatch::CoupledBatch(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added)
	: batch_(batch), refused_(refused), added_(added)
{
}

const std::string *CoupledBatch::written(const char *key) const
{
	for (size_t i = 0; i < batch_.values.size(); ++i)
	{
		if (batch_.values[i].first == key)
			return &batch_.values[i].second;
	}
	return NULL;
}

namespace
{

bool numberOf(const std::string &text, long &out)
{
	if (text.empty())
		return false;
	errno = 0;
	char *end = NULL;
	const long n = strtol(text.c_str(), &end, 10);
	if (errno != 0 || *end != '\0')
		return false;
	out = n;
	return true;
}

} // anonymous namespace

bool CoupledBatch::writtenNumber(const char *key, long &out) const
{
	const std::string *text = written(key);
	return text != NULL && numberOf(*text, out);
}

bool CoupledBatch::current(const char *key, std::string &out) const
{
	const std::string *text = written(key);
	if (text != NULL)
	{
		out = *text;
		return true;
	}
	Result<std::string> stored = get(key);
	if (!stored.ok())
		return false;
	out = stored.value();
	return true;
}

bool CoupledBatch::currentNumber(const char *key, long &out) const
{
	std::string text;
	return current(key, text) && numberOf(text, out);
}

bool CoupledBatch::changes(const char *key) const
{
	const std::string *text = written(key);
	if (text == NULL)
		return false;
	Result<std::string> stored = get(key);
	if (!stored.ok())
		return true;
	long a = 0;
	long b = 0;
	if (numberOf(*text, a) && numberOf(stored.value(), b))
		return a != b;
	return *text != stored.value();
}

Error conditionRefusal(const char *message, const std::vector<std::string> &because_of)
{
	std::string said = message;
	for (size_t i = 0; i < because_of.size(); ++i)
		said += (i == 0 ? ": " : ", ") + because_of[i];
	Error e(Status::Conflict, ErrorCode::SettingConditionNotMet, said);
	e.depends_on = because_of;
	return e;
}

void CoupledBatch::refuse(const char *key, const char *message, const std::vector<std::string> &because_of)
{
	for (size_t i = 0; i < batch_.values.size(); ++i)
	{
		if (batch_.values[i].first != key)
			continue;
		refused_.push_back(std::make_pair(std::string(key), conditionRefusal(message, because_of)));
		batch_.values.erase(batch_.values.begin() + i);
		return;
	}
}

void CoupledBatch::refuseWith(const char *key, const Error &e)
{
	for (size_t i = 0; i < batch_.values.size(); ++i)
	{
		if (batch_.values[i].first != key)
			continue;
		refused_.push_back(std::make_pair(std::string(key), e));
		batch_.values.erase(batch_.values.begin() + i);
		return;
	}
}

void CoupledBatch::refuse(const char *key, const char *message, const char *because_of)
{
	refuse(key, message, std::vector<std::string>(1, because_of));
}

void CoupledBatch::link(const char *key, const char *because)
{
	Addition a;
	a.key = key;
	a.trigger = because;
	added_.push_back(a);
}

bool CoupledBatch::put(const char *key, const std::string &value, const char *because)
{
	Result<void> checked = check(key, value);
	if (!checked.ok())
	{
		for (size_t i = 0; i < batch_.values.size(); ++i)
		{
			if (batch_.values[i].first != because)
				continue;
			refused_.push_back(std::make_pair(std::string(because), checked.error()));
			batch_.values.erase(batch_.values.begin() + i);
			break;
		}
		return false;
	}

	for (size_t i = 0; i < batch_.values.size(); ++i)
	{
		if (batch_.values[i].first == key)
		{
			batch_.values[i].second = value;
			return true;
		}
	}
	batch_.values.push_back(std::make_pair(std::string(key), value));
	link(key, because);
	return true;
}

bool CoupledBatch::imply(const char *key, const std::string &value, const char *because)
{
	const std::string *named = written(key);
	if (named != NULL)
	{
		long have = 0;
		long want = 0;
		if (numberOf(*named, have) && numberOf(value, want) ? have == want : *named == value)
			return true;
		static const char *const kSaid = "the setting contradicts another one written with it";
		refuse(key, kSaid, because);
		refuse(because, kSaid, key);
		return false;
	}
	return put(key, value, because);
}

bool CoupledBatch::clear(const char *key, const char *because)
{
	if (written(key) != NULL)
	{
		static const char *const kSaid = "the setting contradicts another one written with it";
		refuse(key, kSaid, because);
		refuse(because, kSaid, key);
		return false;
	}

	Result<void> checked = checkCleared(key);
	if (!checked.ok())
	{
		for (size_t i = 0; i < batch_.values.size(); ++i)
		{
			if (batch_.values[i].first != because)
				continue;
			refused_.push_back(std::make_pair(std::string(because), checked.error()));
			batch_.values.erase(batch_.values.begin() + i);
			break;
		}
		return false;
	}

	batch_.values.push_back(std::make_pair(std::string(key), std::string()));
	batch_.cleared.push_back(key);
	link(key, because);
	return true;
}

bool judgedByCoupling(const Descriptor &row)
{
	if (row.condition_count == 0)
		return false;
	for (size_t p = 0; p < kJudgedPairCount; ++p)
	{
		const char *partner = NULL;
		if (std::string(row.key) == kJudgedPairs[p].first)
			partner = kJudgedPairs[p].second;
		else if (std::string(row.key) == kJudgedPairs[p].second)
			partner = kJudgedPairs[p].first;
		if (partner == NULL)
			continue;
		for (size_t c = 0; c < row.condition_count; ++c)
		{
			const Condition &cond = row.conditions[c];
			if (cond.any_of != NULL || cond.key == NULL || std::string(cond.key) != partner)
				return false;
		}
		return true;
	}
	return false;
}

bool CoupledBatch::requirePair(const char *first, const char *second)
{
	const bool has_first = written(first) != NULL;
	const bool has_second = written(second) != NULL;
	if (has_first && has_second)
		return true;
	static const char *const kSaid = "the setting is one fact with another and is written together with it";
	if (has_first)
		refuse(first, kSaid, second);
	if (has_second)
		refuse(second, kSaid, first);
	return false;
}

namespace
{

const Coupling kCouplings[] =
{
	coupleEpg,
	coupleOsd,
	coupleChannel,
	coupleWeather,
	couplePlugins,
	coupleCi,
	coupleLcd4l
};

void partnersFrom(const KeyPair *pairs, size_t n, std::vector<std::string> &keys)
{
	const size_t had = keys.size();
	for (size_t i = 0; i < n; ++i)
	{
		for (size_t k = 0; k < had; ++k)
		{
			const char *other = NULL;
			if (keys[k] == pairs[i].first)
				other = pairs[i].second;
			else if (keys[k] == pairs[i].second)
				other = pairs[i].first;
			if (other == NULL)
				continue;
			bool present = false;
			for (size_t m = 0; m < keys.size() && !present; ++m)
				present = keys[m] == other;
			if (!present)
				keys.push_back(other);
		}
	}
}

} // anonymous namespace

const char *pairPartner(const std::string &key, const char **writes)
{
	const struct { const KeyPair *pairs; size_t n; const char *writes; } kinds[] =
	{
		{ kChannelPairs, kChannelPairCount, "id" },
		{ kWeatherPairs, kWeatherPairCount, "both" }
	};
	for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); ++k)
	{
		for (size_t i = 0; i < kinds[k].n; ++i)
		{
			const char *other = NULL;
			if (key == kinds[k].pairs[i].first)
				other = kinds[k].pairs[i].second;
			else if (key == kinds[k].pairs[i].second)
				other = kinds[k].pairs[i].first;
			if (other == NULL)
				continue;
			if (writes != NULL)
				*writes = kinds[k].writes;
			return other;
		}
	}
	return NULL;
}

void addPartners(std::vector<std::string> &keys)
{
	partnersFrom(kChannelPairs, kChannelPairCount, keys);
	partnersFrom(kWeatherPairs, kWeatherPairCount, keys);
}

size_t couplingCount()
{
	return sizeof(kCouplings) / sizeof(kCouplings[0]);
}

void applyCouplings(BatchOverlay &batch, Refusals &refused, std::vector<Addition> &added)
{
	CoupledBatch coupled(batch, refused, added);
	for (size_t i = 0; i < sizeof(kCouplings) / sizeof(kCouplings[0]); ++i)
		kCouplings[i](coupled);
}

} // namespace settings
} // namespace coreapi
