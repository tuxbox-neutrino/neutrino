/*
 * test_tuner.cpp - tests for signal and frontends
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

#include "support/catch.hpp"
#include "support/fakes.h"

#include "httpd/endpoint.h"
#include "httpd/endpoints.h"
#include "httpd/router.h"
#include "httpd/schema.h"

#include "coreapi/base/errors.h"
#include "coreapi/tuner.h"
#include <neutrinoMessages.h>
#include "coreapi/base/types.h"

#include "jsoncpp/json/json.h"

#include <memory>
#include <string>
#include <vector>

namespace
{

/* The tuner these cases stand for, filled in one place so that a case saying
   something about one field is not also quietly deciding the rest. Every value
   is distinct and none of them is nought, so a field read out of the one beside
   it or left at its default shows as the wrong number rather than as the right
   one. */
coreapi::FrontendInfo satelliteTuner()
{
	coreapi::FrontendInfo fe;
	fe.adapter = 1;
	fe.number = 2;
	fe.name = "a satellite tuner";
	fe.satellite = true;
	fe.cable = false;
	fe.terrestrial = false;
	fe.in_use = true;
	fe.live = true;
	return fe;
}

// The second one differs from the first in every field, so an answer that
// carried one of them twice cannot read as an answer carrying both.
coreapi::FrontendInfo hybridTuner()
{
	coreapi::FrontendInfo fe;
	fe.adapter = 3;
	fe.number = 4;
	fe.name = "a hybrid tuner";
	fe.satellite = false;
	fe.cable = true;
	fe.terrestrial = true;
	fe.in_use = false;
	fe.live = false;
	return fe;
}

/* A reading whose scale is not the one the box happens to use, so that a layer
   which knows the scale rather than carrying it answers the wrong number here.
   Every other value is distinct for the reason the tuners above are. */
coreapi::SignalInfo aReading()
{
	coreapi::SignalInfo sig;
	sig.adapter = 5;
	sig.number = 6;
	sig.strength = 12345;
	sig.snr = 4321;
	sig.full_scale = 20000;
	sig.bit_error_rate = 77;
	sig.locked = true;
	return sig;
}

} // namespace

using namespace coreapi;
using namespace httpd;

/* A box with no tuner is the ordinary case here and on any build that links this layer
   without the frontends, so it has to be an answer and not a fault. NotSupported says
   the box does not do this; Internal would say something here broke, and a caller told
   that would retry a box that is answering correctly. Both directions are pinned in one
   case, because an implementation answering one code for both would satisfy either half
   read alone. */
TEST_CASE("a box with no tuner is told apart from tuners that could not be read", "[tuner]")
{
	FakeTunerSource src;
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	Result<SignalInfo> none = tuner::live();
	REQUIRE_FALSE(none.ok());
	REQUIRE(none.error().status == Status::NotSupported);
	REQUIRE(std::string(codeString(none.error().code)) == "no-tuner");
	// The source was asked. Without this the case would pass just as well
	// against a layer that answers this out of nothing at all.
	REQUIRE(src.live_reads == 1);

	src.live_status = Status::Internal;
	Result<SignalInfo> broken = tuner::live();
	REQUIRE_FALSE(broken.ok());
	REQUIRE(broken.error().status == Status::Internal);
	REQUIRE(std::string(codeString(broken.error().code)) == "signal-unavailable");
	REQUIRE(src.live_reads == 2);
}

/* The readings are what the tuner measured and the scale is what it measured
   them against, and this layer carries both rather than dividing one by the
   other. The copied control API divides, and what it sends cannot be turned
   back into a reading. */
TEST_CASE("a reading is handed on as it was taken, with what it was taken against", "[tuner]")
{
	FakeTunerSource src;
	src.signal = aReading();
	src.live_status = Status::Ok;
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	Result<SignalInfo> got = tuner::live();
	REQUIRE(got.ok());
	const SignalInfo sig = got.value();

	REQUIRE(sig.adapter == 5);
	REQUIRE(sig.number == 6);
	REQUIRE(sig.strength == 12345u);
	REQUIRE(sig.snr == 4321u);
	// Not the sixteen bits the box's own frontends report, which is the whole
	// point: a layer that knew the scale instead of carrying it answers 65535
	// here and every reading above becomes a different fraction.
	REQUIRE(sig.full_scale == 20000u);
	REQUIRE(sig.bit_error_rate == 77u);
	REQUIRE(sig.locked);
}

// Having no tuner is something this can say, so it is said rather than refused:
// an empty list is an answer a caller reads, and a refusal is one it retries.
TEST_CASE("a box with no tuner has an empty list of them and not a refusal", "[tuner]")
{
	FakeTunerSource src;
	InstalledTunerSource installed(&src);
	/* The list is answered out of what the box is doing as well as out of what
	   it has, so both stand here the way both stand on a running box. */
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);

	Result<FrontendList> got = tuner::frontends();
	REQUIRE(got.ok());
	REQUIRE(got.value().empty());
	REQUIRE(src.list_reads == 1);
}

TEST_CASE("every tuner the box has comes back, in order and whole", "[tuner]")
{
	FakeTunerSource src;
	src.tuners.push_back(satelliteTuner());
	src.tuners.push_back(hybridTuner());
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	Result<FrontendList> got = tuner::frontends();
	REQUIRE(got.ok());
	const FrontendList all = got.value();
	REQUIRE(all.size() == 2);

	// The order is the box's, so a layer that sorted or reversed them would be
	// naming a different tuner by the same position.
	REQUIRE(all[0].adapter == 1);
	REQUIRE(all[0].number == 2);
	REQUIRE(all[0].name == "a satellite tuner");
	REQUIRE(all[0].satellite);
	REQUIRE_FALSE(all[0].cable);
	REQUIRE_FALSE(all[0].terrestrial);
	REQUIRE(all[0].in_use);
	REQUIRE(all[0].live);

	REQUIRE(all[1].adapter == 3);
	REQUIRE(all[1].number == 4);
	REQUIRE(all[1].name == "a hybrid tuner");
	REQUIRE_FALSE(all[1].satellite);
	REQUIRE(all[1].cable);
	REQUIRE(all[1].terrestrial);
	REQUIRE_FALSE(all[1].in_use);
	REQUIRE_FALSE(all[1].live);
}

// The other direction of the empty list above: a box whose tuners could not be
// read at all has to say so, or a caller reads it as a box with none.
TEST_CASE("tuners that could not be read at all are a refusal and not an empty list", "[tuner]")
{
	FakeTunerSource src;
	src.tuners.push_back(satelliteTuner());
	src.list_status = Status::Internal;
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	Result<FrontendList> got = tuner::frontends();
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().status == Status::Internal);
	REQUIRE(std::string(codeString(got.error().code)) == "frontend-list-unavailable");
	REQUIRE(src.list_reads == 1);
}

namespace
{

/* Cleared going in as well as coming out, so a case elsewhere that failed and
   unwound past its own restore cannot decide what these read. */
struct ShippedRoutes
{
	ShippedRoutes() { setRoutesForTest(NULL); }
	~ShippedRoutes() { setRoutesForTest(NULL); }

	private:
		ShippedRoutes(const ShippedRoutes &);
		ShippedRoutes &operator=(const ShippedRoutes &);
};

struct Reply
{
	int         code;
	std::string content_type;
	std::string body;

	Reply() : code(0) {}
};

// Granted a read, which is what a caller on the box's own network has without
// presenting anything, and what both routes here declare.
Reply get(const std::string &path)
{
	const Response r = dispatch(Get, path, "", "", "127.0.0.1", AuthLevel::Read);

	Reply out;
	out.code = r.code;
	out.content_type = r.content_type;
	out.body = r.body;
	return out;
}

::Json::Value parsed(const std::string &body)
{
	::Json::CharReaderBuilder builder;
	std::unique_ptr< ::Json::CharReader> reader(builder.newCharReader());
	::Json::Value root;
	std::string errs;
	REQUIRE(reader->parse(body.data(), body.data() + body.size(), &root, &errs));
	return root;
}

// The route of that path out of the shipped tables, so a case holding an answer
// to a shape holds it to the shape the server declares.
const Endpoint *routeFor(const char *path)
{
	size_t tables = 0;
	const RouteTable *const *t = allRoutes(&tables);
	for (size_t i = 0; i < tables; ++i)
	{
		for (size_t j = 0; j < t[i]->count; ++j)
		{
			if (t[i]->endpoints[j].method == Get &&
			    std::string(path) == t[i]->endpoints[j].path)
				return &t[i]->endpoints[j];
		}
	}
	return NULL;
}

bool kindMatches(const ::Json::Value &v, FieldType t)
{
	const bool whole = v.isIntegral() && !v.isBool();
	switch (t)
	{
		case FieldType::Bool:   return v.isBool();
		case FieldType::Int:    return whole;
		case FieldType::UInt:   return whole && v.asInt64() >= 0;
		case FieldType::Number: return whole || v.isDouble();
		case FieldType::String: return v.isString();
		case FieldType::Time:   return whole;
		case FieldType::ChannelId: return v.isString();
		case FieldType::Object: return v.isObject();
		case FieldType::Array:  return v.isArray();
	}
	return false;
}

/* One answer against the shape its route declares, in both directions: every
   member the shape calls for has to be there and be of the kind it declares,
   and every member the answer carries has to be one the shape names. Without
   the second direction a member added to a handler and left out of the shape
   beside it would go unmentioned, which is a document describing an answer
   nobody sends. */
void checkShape(const ::Json::Value &v, const Schema &s, const std::string &where)
{
	INFO(where << " against " << s.name);
	REQUIRE(v.isObject());

	for (size_t i = 0; i < s.count; ++i)
	{
		const FieldDesc &f = s.fields[i];
		INFO("member " << f.name);
		REQUIRE(v.isMember(f.name));
		REQUIRE(kindMatches(v[f.name], f.type));

		if (f.type == FieldType::Array && f.nested != NULL)
		{
			for (::Json::ArrayIndex e = 0; e < v[f.name].size(); ++e)
				checkShape(v[f.name][e], *f.nested, where + "." + f.name);
		}
	}

	const ::Json::Value::Members names = v.getMemberNames();
	for (size_t i = 0; i < names.size(); ++i)
	{
		bool declared = false;
		for (size_t j = 0; j < s.count && !declared; ++j)
			declared = names[i] == s.fields[j].name;
		INFO("member " << names[i]);
		REQUIRE(declared);
	}
}

} // namespace

/* A route nothing names in the list of tables is a route the server does not
   have, and the link says nothing about it: the table is a constant somebody
   else has to ask for. So the two are looked up in the shipped list rather than
   driven through a table written here. */
TEST_CASE("both tuner routes are in the tables the server ships, and both are reads", "[tuner]")
{
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;
	ShippedRoutes shipped;

	const Endpoint *signal = routeFor("/api/v1/tuner/signal");
	REQUIRE(signal != NULL);
	REQUIRE(signal->auth == AuthLevel::Read);
	REQUIRE(signal->schema != NULL);

	const Endpoint *list = routeFor("/api/v1/tuner/frontends");
	REQUIRE(list != NULL);
	REQUIRE(list->auth == AuthLevel::Read);
	REQUIRE(list->schema != NULL);

	// The tables the server answers from have to be ones it can answer from at
	// all, which is what refuses a pair of routes that would both match.
	std::string why;
	INFO(why);
	REQUIRE(routesAreSane(&why));
}

TEST_CASE("the signal route answers the readings and what they are out of", "[tuner]")
{
	ShippedRoutes shipped;
	FakeTunerSource src;
	src.signal = aReading();
	src.live_status = Status::Ok;
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	const Reply r = get("/api/v1/tuner/signal");
	REQUIRE(r.code == 200);
	REQUIRE(r.content_type == "application/json");

	const ::Json::Value root = parsed(r.body);
	REQUIRE(root["adapter"].asInt() == 5);
	REQUIRE(root["number"].asInt() == 6);
	REQUIRE(root["strength"].asUInt() == 12345u);
	REQUIRE(root["snr"].asUInt() == 4321u);
	REQUIRE(root["full_scale"].asUInt() == 20000u);
	REQUIRE(root["bit_error_rate"].asUInt() == 77u);
	REQUIRE(root["locked"].asBool());

	const Endpoint *ep = routeFor("/api/v1/tuner/signal");
	REQUIRE(ep != NULL);
	checkShape(root, *ep->schema, "/api/v1/tuner/signal");
}

/* Not implemented rather than a fault of the server, and the code beside it
   says which of the two it is. A five hundred here would have every caller on
   a box without a tuner reporting a broken box. */
TEST_CASE("a box with no tuner is answered as one that cannot, not as one that broke", "[tuner]")
{
	ShippedRoutes shipped;
	FakeTunerSource src;
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	const Reply r = get("/api/v1/tuner/signal");
	REQUIRE(r.code == 501);
	REQUIRE(r.content_type == "application/problem+json");
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/no-tuner");
	// It went through the handler rather than being turned away before one was
	// reached, which is the only way this answer says anything about the tuner.
	REQUIRE(src.live_reads == 1);
}

TEST_CASE("the frontend route lists every tuner and answers an empty list for none", "[tuner]")
{
	ShippedRoutes shipped;
	FakeTunerSource src;
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	const Reply empty = get("/api/v1/tuner/frontends");
	REQUIRE(empty.code == 200);
	REQUIRE(parsed(empty.body)["items"].size() == 0);

	src.tuners.push_back(satelliteTuner());
	src.tuners.push_back(hybridTuner());

	const Reply r = get("/api/v1/tuner/frontends");
	REQUIRE(r.code == 200);
	const ::Json::Value items = parsed(r.body)["items"];
	REQUIRE(items.size() == 2);
	REQUIRE(items[0]["adapter"].asInt() == 1);
	REQUIRE(items[0]["number"].asInt() == 2);
	REQUIRE(items[0]["name"].asString() == "a satellite tuner");
	REQUIRE(items[0]["satellite"].asBool());
	REQUIRE(items[0]["in_use"].asBool());
	REQUIRE(items[0]["live"].asBool());
	REQUIRE(items[1]["name"].asString() == "a hybrid tuner");
	REQUIRE(items[1]["cable"].asBool());
	REQUIRE(items[1]["terrestrial"].asBool());
	REQUIRE_FALSE(items[1]["live"].asBool());

	const Endpoint *ep = routeFor("/api/v1/tuner/frontends");
	REQUIRE(ep != NULL);
	checkShape(parsed(r.body), *ep->schema, "/api/v1/tuner/frontends");
}

// The other direction at the wire, as it is one floor down: a box whose tuners
// could not be read says so rather than answering the list a box with none has.
TEST_CASE("tuners that could not be read are a refusal at the wire too", "[tuner]")
{
	ShippedRoutes shipped;
	FakeTunerSource src;
	src.list_status = Status::Internal;
	InstalledTunerSource installed(&src);
	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);
	chans.mode = NeutrinoModes::mode_tv;

	const Reply r = get("/api/v1/tuner/frontends");
	REQUIRE(r.code == 500);
	REQUIRE(r.content_type == "application/problem+json");
	REQUIRE(parsed(r.body)["type"].asString() == "/errors/frontend-list-unavailable");
}

TEST_CASE("no tuner is named as the source of a picture that does not come from one", "[tuner]")
{
	/* WHAT A TUNER SAYS WHILE A STREAM PLAYS. The box keeps the assignment on
	   the frontend it last tuned to, so the layer below goes on reporting it
	   while the picture comes from the network. Carried through, the card says
	   a tuner delivers a picture beside a reading of nothing, which is the one
	   answer a person cannot make sense of. */
	FakeTunerSource src;
	src.tuners.push_back(satelliteTuner());
	InstalledTunerSource installed(&src);

	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);

	chans.mode = NeutrinoModes::mode_tv;
	coreapi::Result<coreapi::FrontendList> watching = coreapi::tuner::frontends();
	REQUIRE(watching.ok());
	REQUIRE(watching.value().size() == 1);
	REQUIRE(watching.value()[0].live);

	chans.mode = NeutrinoModes::mode_webtv;
	coreapi::Result<coreapi::FrontendList> streaming = coreapi::tuner::frontends();
	REQUIRE(streaming.ok());
	REQUIRE_FALSE(streaming.value()[0].live);
	// What holds it is a different question and is answered as before.
	REQUIRE(streaming.value()[0].in_use);

	/* A mode nobody could read is not a reason to call a tuner idle. */
	chans.mode_status = coreapi::Status::NotFound;
	coreapi::Result<coreapi::FrontendList> unknown = coreapi::tuner::frontends();
	REQUIRE(unknown.ok());
	REQUIRE(unknown.value()[0].live);
}

TEST_CASE("no reading is offered for a tuner that is carrying nothing", "[tuner]")
{
	/* The reading names the frontend the box would take a picture from, and
	   that assignment stands whatever plays. Taken while a stream from the
	   network is on, it prints a measurement of a tuner nothing uses, over a
	   list that calls the same tuner free. */
	FakeTunerSource src;
	src.tuners.push_back(satelliteTuner());
	InstalledTunerSource installed(&src);

	FakeChannelSource chans;
	InstalledChannelSource holds(&chans);

	src.live_status = coreapi::Status::Ok;

	chans.mode = NeutrinoModes::mode_tv;
	REQUIRE(coreapi::tuner::live().ok());
	const unsigned read_while_watching = src.live_reads;

	chans.mode = NeutrinoModes::mode_webtv;
	coreapi::Result<coreapi::SignalInfo> streaming = coreapi::tuner::live();
	REQUIRE_FALSE(streaming.ok());
	REQUIRE(streaming.error().code == coreapi::ErrorCode::NoTuner);
	// And the box was not asked, because there is nothing there to ask about.
	REQUIRE(src.live_reads == read_while_watching);
}
