/*
 * settle.h - a write of several settings as the cases about pairs and conditions name it
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __support_settle_h__
#define __support_settle_h__

#include "coreapi/base/result.h"
#include "coreapi/settings/couple.h"
#include "coreapi/settings/settings.h"

#include <string>
#include <utility>
#include <vector>

/* One member of a write: one the caller named, or one a coupling added because of the
   member named in because, empty for the caller's own. */
struct BatchMember
{
	std::string key;
	std::string value;
	std::string because;
};

// A member the write does not store, with its answer and the settings that took it down.
struct BatchRefusal
{
	std::string key;
	coreapi::Error error;
	std::vector<std::string> depends_on;
};

/* The first two of the three passes a write of several settings makes, as the route makes
   them, and nothing written: check() on each member, then settleBatch() over what passed.
   batch comes back as what would be written. A member the caller named that the settling
   leaves out as the value the box holds (holdsStored() says so) is in it as kept, since the
   route answers it as landed; what a coupling added names in because the member that brought it. refused keeps
   what it held, which counts as not written, and gains what fell. */
inline void keepTogether(std::vector<BatchMember> &batch, std::vector<BatchRefusal> &refused)
{
	using namespace coreapi::settings;

	BatchOverlay sent;
	Refusals out;
	for (size_t i = 0; i < batch.size(); ++i)
	{
		coreapi::Result<void> checked = check(batch[i].key, batch[i].value);
		if (!checked.ok())
			out.push_back(std::make_pair(batch[i].key, checked.error()));
		else
			sent.values.push_back(std::make_pair(batch[i].key, batch[i].value));
	}

	BatchOverlay settled = sent;
	settleBatch(settled, out);

	// Who brought what: the couplings over what the caller named and what was not refused.
	BatchOverlay left;
	for (size_t i = 0; i < sent.values.size(); ++i)
	{
		bool fell = false;
		for (size_t r = 0; r < out.size() && !fell; ++r)
			fell = out[r].first == sent.values[i].first;
		if (!fell)
			left.values.push_back(sent.values[i]);
	}
	Refusals ignored;
	std::vector<Addition> added;
	applyCouplings(left, ignored, added);

	batch.clear();
	for (size_t i = 0; i < settled.values.size(); ++i)
	{
		BatchMember m;
		m.key = settled.values[i].first;
		m.value = settled.values[i].second;
		bool named = false;
		for (size_t s = 0; s < sent.values.size() && !named; ++s)
			named = sent.values[s].first == m.key;
		for (size_t a = 0; !named && a < added.size() && m.because.empty(); ++a)
		{
			if (added[a].key == m.key)
				m.because = added[a].trigger;
		}
		batch.push_back(m);
	}
	for (size_t i = 0; i < left.values.size(); ++i)
	{
		bool there = false;
		for (size_t b = 0; b < batch.size() && !there; ++b)
			there = batch[b].key == left.values[i].first;
		bool brought = false;
		for (size_t a = 0; a < added.size() && !brought; ++a)
			brought = added[a].key == left.values[i].first;
		bool fell = false;
		for (size_t r = 0; r < out.size() && !fell; ++r)
			fell = out[r].first == left.values[i].first;
		/* Only what the settling may leave out, the value the box holds already: anything
		   else missing here would be a member dropped without an answer, which must show. */
		if (!there && !fell && !brought && holdsStored(left.values[i].first, left.values[i].second))
		{
			BatchMember m;
			m.key = left.values[i].first;
			m.value = left.values[i].second;
			batch.push_back(m);
		}
	}

	for (size_t i = 0; i < out.size(); ++i)
	{
		BatchRefusal r;
		r.key = out[i].first;
		r.error = out[i].second;
		r.depends_on = out[i].second.depends_on;
		refused.push_back(r);
	}
}

/* The answer the settling gives one value written alone beside the values in also, which
   are taken as written: ok, or the error it is refused with. */
inline coreapi::Result<void> settleOne(const std::string &key, const std::string &value,
				       const std::vector<std::pair<std::string, std::string> > &also =
					       std::vector<std::pair<std::string, std::string> >())
{
	using namespace coreapi::settings;
	BatchOverlay batch;
	batch.values = also;
	batch.values.push_back(std::make_pair(key, value));
	Refusals out;
	settleBatch(batch, out);
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i].first == key)
			return coreapi::Result<void>::failure(out[i].second);
	}
	return coreapi::Result<void>::success();
}

#endif
