/*
 * ep_tuner.cpp - routes for signal and frontends
 *
 * Copyright (C) 2026 NI-Team
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/tuner.h"
#include "coreapi/base/types.h"

#include <cstddef>
#include <utility>

namespace httpd
{

namespace
{

const FieldDesc kSignalFields[] = {
	HTTPD_MEMBER("adapter", FieldType::Int, "which adapter the tuner that answered is on"),
	HTTPD_MEMBER("number", FieldType::Int,
		"which tuner of that adapter it is, the pair being what names one"),
	HTTPD_MEMBER("strength", FieldType::UInt, "how strong the signal is, out of full_scale below"),
	HTTPD_MEMBER("snr", FieldType::UInt,
		"how far the signal stands above the noise, out of full_scale below"),
	HTTPD_MEMBER("full_scale", FieldType::UInt,
		"what the two readings above are out of, so a reader wanting a percentage has both halves of the division"),
	HTTPD_MEMBER("bit_error_rate", FieldType::UInt,
		"how many bits arrived wrong, as a count the tuner keeps and not a reading out of a range, so nothing divides it"),
	HTTPD_MEMBER("locked", FieldType::Bool,
		"whether the tuner has a signal it can decode, which is what tells a weak signal from none at all"),
};

const Schema kSignalSchema = { "signal", HTTPD_FIELDS(kSignalFields) };

const FieldDesc kFrontendFields[] = {
	HTTPD_MEMBER("adapter", FieldType::Int, "which adapter this tuner is on"),
	HTTPD_MEMBER("number", FieldType::Int,
		"which tuner of that adapter it is, the pair being what names one"),
	HTTPD_MEMBER("name", FieldType::String,
		"what the driver calls it, which is the only name a tuner has"),
	HTTPD_MEMBER("satellite", FieldType::Bool, "whether it can receive from a satellite"),
	HTTPD_MEMBER("cable", FieldType::Bool, "whether it can receive from a cable"),
	HTTPD_MEMBER("terrestrial", FieldType::Bool, "whether it can receive from an aerial"),
	HTTPD_MEMBER("in_use", FieldType::Bool,
		"whether something is holding it, which on a box with two tuners is what a recording looks like"),
	HTTPD_MEMBER("live", FieldType::Bool,
		"whether the live picture is coming from this one, which nothing does while a stream or a recording is what plays"),
};

const Schema kFrontendSchema = { "frontend", HTTPD_FIELDS(kFrontendFields) };

const FieldDesc kFrontendListFields[] = {
	HTTPD_LIST_OF("items", &kFrontendSchema,
		"every tuner this box has, in the order the box found them, and empty for a box with none"),
};

const Schema kFrontendListSchema = { "frontend-list", HTTPD_FIELDS(kFrontendListFields) };

Response liveSignal(const Request &)
{
	coreapi::Result<coreapi::SignalInfo> got = coreapi::tuner::live();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::SignalInfo sig = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 160);
	j.beginObject();
	j.key("adapter");
	j.value((long) sig.adapter);
	j.key("number");
	j.value((long) sig.number);
	/* The readings as the tuner reported them, and the scale beside them. The copied
	   control API turns the same two into a percentage before it sends them, which
	   loses the reading; a caller that wants that percentage has what it needs to work
	   it out. */
	j.key("strength");
	j.value((unsigned long) sig.strength);
	j.key("snr");
	j.value((unsigned long) sig.snr);
	j.key("full_scale");
	j.value((unsigned long) sig.full_scale);
	j.key("bit_error_rate");
	j.value((unsigned long) sig.bit_error_rate);
	j.key("locked");
	j.value(sig.locked);
	j.endObject();
	return out;
}

Response listFrontends(const Request &)
{
	coreapi::Result<coreapi::FrontendList> got = coreapi::tuner::frontends();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::FrontendList all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 32 + 160 * all.size());
	j.beginObject();
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.size(); ++i)
	{
		j.beginObject();
		// The pair the box names a tuner by and never where it sits in this
		// answer, for the reason every list here carries an identifier.
		j.key("adapter");
		j.value((long) all[i].adapter);
		j.key("number");
		j.value((long) all[i].number);
		j.key("name");
		j.value(all[i].name);
		j.key("satellite");
		j.value(all[i].satellite);
		j.key("cable");
		j.value(all[i].cable);
		j.key("terrestrial");
		j.value(all[i].terrestrial);
		j.key("in_use");
		j.value(all[i].in_use);
		j.key("live");
		j.value(all[i].live);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

/* The tuner stack down and up again, and the running channel tuned again after
   it. Nothing comes back but the code: the answer to what the reset came to is
   the reading beside it, and a document written here saying the box had been
   asked would be a second and staler statement of what that read answers.

   No content and not accepted, because this is not a message handed to a queue:
   the channel daemon is told and answers, and by the time this line is reached
   it has taken all three parts of it. */
Response resetTuner(const Request &)
{
	coreapi::Result<void> done = coreapi::tuner::reset();
	if (!done.ok())
		return problemFor(done.error());
	return noContent();
}

const Endpoint kTunerEndpoints[] = {
	{ Method::Get, "/api/v1/tuner/signal", AuthLevel::Read,
	  "what the tuner carrying the picture is measuring",
	  NULL, 0, &kSignalSchema, &liveSignal, false },
	{ Method::Get, "/api/v1/tuner/frontends", AuthLevel::Read,
	  "every tuner this box has and what each of them is doing",
	  NULL, 0, &kFrontendListSchema, &listFrontends, false },
	/* System, where the reload of the channel lists beside it is Write, and the
	   difference is what each of them touches. That one rewrites what the box
	   holds in memory and leaves the picture where it is; this one drives the
	   channel daemon's own standby, which is the mechanism
	   POST /api/v1/system/standby is System for, and every live viewer and
	   every stream this box is serving loses its picture while it runs. The
	   copied surface asked for the same thing through its command runner, which
	   is System there too.

	   What that is worth is a token: the one account this box has reaches
	   System by signing in either way, so the level is what keeps a token given
	   out for reading and writing from being able to take the picture off. */
	{ Method::Post, "/api/v1/tuner/reset", AuthLevel::System,
	  "puts the tuners down and up again and tunes the running channel afresh, for a picture that has gone while the box carried on",
	  NULL, 0, NULL, &resetTuner, false },
};

} // namespace

extern const RouteTable tunerTable = {
	HTTPD_TABLE("tuner", kTunerEndpoints)
};

} // namespace httpd
