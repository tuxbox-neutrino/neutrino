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
	std::string said = message;
	for (size_t i = 0; i < because_of.size(); ++i)
		said += (i == 0 ? ": " : ", ") + because_of[i];
	refuseWith(batch, refused, key, Error(Status::Conflict, ErrorCode::SettingConditionNotMet, said),
		   because_of);
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
	BatchMember brought;
	brought.key = kPostalCode;
	brought.because = kCity;
	batch.push_back(brought);
}

} // anonymous namespace

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

} // namespace settings
} // namespace coreapi
