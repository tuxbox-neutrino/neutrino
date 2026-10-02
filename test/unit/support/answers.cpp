/*
 * answers.cpp - what the shipped routes answered while the suite ran
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

#include "answers.h"
#include "counts.h"

#include "httpd/doc/openapi.h"
#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/router.h"
#include "httpd/status.h"

#include "coreapi/base/errors.h"

#include "jsoncpp/json/json.h"

#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace httpd;

namespace
{

typedef std::pair<int, std::string> Outcome;
typedef std::map<Outcome, std::string> Outcomes;

struct Watched
{
	std::set<const Endpoint *>                  shipped;
	std::map<const Endpoint *, Outcomes>        seen;
	// Any table counts: a rule is one check.
	std::set<std::string>                       rules_reached;
};

Watched &watched()
{
	static Watched w;
	return w;
}

std::set<std::string> &examplesAccepted()
{
	static std::set<std::string> s;
	return s;
}

std::string asText(const ::Json::Value &v)
{
	if (v.isString())
		return v.asString();
	::Json::FastWriter w;
	std::string out = w.write(v);
	while (!out.empty() && out[out.size() - 1] == '\n')
		out.erase(out.size() - 1);
	return out;
}

const ::Json::Value &documentParsed()
{
	static ::Json::Value doc;
	if (doc.isNull())
	{
		::Json::Reader reader;
		reader.parse(openapi::document(), doc);
	}
	return doc;
}

std::set<std::string> routesWithBodyExamples()
{
	std::set<std::string> out;
	const ::Json::Value &paths = documentParsed()["paths"];
	const ::Json::Value::Members names = paths.getMemberNames();
	for (size_t i = 0; i < names.size(); ++i)
	{
		const ::Json::Value::Members verbs = paths[names[i]].getMemberNames();
		for (size_t j = 0; j < verbs.size(); ++j)
		{
			const ::Json::Value &body = paths[names[i]][verbs[j]]["requestBody"]["content"]
				["application/json"];
			if (!body.isMember("example"))
				continue;
			std::string verb = verbs[j];
			for (size_t k = 0; k < verb.size(); ++k)
				verb[k] = (char)(verb[k] - 'a' + 'A');
			out.insert(verb + " " + names[i]);
		}
	}
	return out;
}

std::string ruleKey(const char *rule, int http, const std::string &code)
{
	char head[16];
	std::snprintf(head, sizeof(head), " %d ", http);
	return std::string(rule) + head + code;
}

std::string routeName(const Endpoint &ep)
{
	return std::string(methodName(ep.method)) + " " + ep.path;
}

void watch(const Endpoint &ep, const Response &r)
{
	std::string code;
	std::string detail;
	if (r.content_type == problemContentType())
	{
		::Json::Value doc;
		::Json::Reader reader;
		const std::string prefix = "/errors/";
		if (reader.parse(r.body, doc) && doc.isObject() &&
		    doc["type"].asString().compare(0, prefix.size(), prefix) == 0)
		{
			code = doc["type"].asString().substr(prefix.size());
			detail = doc["detail"].asString();
		}
	}

	if (!code.empty())
	{
		std::vector<openapi::StatedRefusal> stated;
		openapi::statedRefusals(ep, stated);
		/* Two rules can share a status and a code, the level and the second
		   token both answering 403 not-permitted. The detail tells them apart
		   then, and a refusal reaches only the rule whose words it carries;
		   where one rule has the pair to itself, the pair is enough. */
		size_t sharing = 0;
		for (size_t i = 0; i < stated.size(); ++i)
			if (stated[i].rule != NULL && stated[i].http == r.code &&
			    code == coreapi::codeString(stated[i].code))
				++sharing;
		for (size_t i = 0; i < stated.size(); ++i)
		{
			if (stated[i].rule != NULL && stated[i].http == r.code &&
			    code == coreapi::codeString(stated[i].code) &&
			    (sharing < 2 || detail == stated[i].detail))
				watched().rules_reached.insert(ruleKey(stated[i].rule, r.code, code));
		}
	}

	if (watched().shipped.count(&ep) == 0)
		return;
	Outcomes &o = watched().seen[&ep];
	const Outcome key(r.code, code);
	if (o.find(key) == o.end())
		o[key] = detail;
}

const struct
{
	unsigned bit;
	int      code;
} kAnswerBits[] = {
	{ Answers200, StatusOk },
	{ Answers201, StatusCreated },
	{ Answers202, StatusAccepted },
	{ Answers204, StatusNoContent },
	{ Answers206, StatusPartialContent },
	{ Answers207, StatusMultiStatus },
};

bool declaresSuccess(const Endpoint &ep, int code)
{
	for (size_t i = 0; i < sizeof(kAnswerBits) / sizeof(kAnswerBits[0]); ++i)
	{
		if (kAnswerBits[i].code == code)
			return (ep.answers & kAnswerBits[i].bit) != 0;
	}
	return false;
}

bool states(const std::vector<openapi::StatedRefusal> &stated, int http, const std::string &code)
{
	for (size_t i = 0; i < stated.size(); ++i)
	{
		if (stated[i].http == http && code == coreapi::codeString(stated[i].code))
			return true;
	}
	return false;
}

} // namespace

void watchAnswers()
{
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	for (size_t t = 0; t < count; ++t)
	{
		for (size_t i = 0; i < tables[t]->count; ++i)
			watched().shipped.insert(&tables[t]->endpoints[i]);
	}
	setAnswerWatchForTest(&watch);
}

bool answersAgree(const char *actual_path)
{
	setAnswerWatchForTest(NULL);

	bool agree = true;
	size_t own = 0;
	size_t refusals_seen = 0;
	std::set<std::string> rules_stated;

	FILE *out = std::fopen(actual_path, "w");
	size_t count = 0;
	const RouteTable *const *tables = allRoutes(&count);
	for (size_t t = 0; t < count; ++t)
	{
		for (size_t i = 0; i < tables[t]->count; ++i)
		{
			const Endpoint &ep = tables[t]->endpoints[i];
			const Outcomes &seen = watched().seen[&ep];
			std::vector<openapi::StatedRefusal> stated;
			openapi::statedRefusals(ep, stated);

			for (Outcomes::const_iterator o = seen.begin(); o != seen.end(); ++o)
			{
				const int http = o->first.first;
				const std::string &code = o->first.second;
				if (out != NULL)
					std::fprintf(out, "%s %d %s\t%s\n", routeName(ep).c_str(), http,
					             code.c_str(), o->second.c_str());
				if (code.empty() && http < 300)
				{
					if (declaresSuccess(ep, http))
						continue;
					std::fprintf(stderr, "%s answered %d, which its table does not declare\n",
					             routeName(ep).c_str(), http);
					agree = false;
					continue;
				}
				++refusals_seen;
				if (code.empty() || !states(stated, http, code))
				{
					std::fprintf(stderr, "%s answered %d %s, which neither its table nor a rule states: %s\n",
					             routeName(ep).c_str(), http, code.c_str(), o->second.c_str());
					agree = false;
				}
			}

			for (size_t k = 0; k < stated.size(); ++k)
			{
				const std::string code = coreapi::codeString(stated[k].code);
				if (stated[k].rule != NULL)
				{
					rules_stated.insert(ruleKey(stated[k].rule, stated[k].http, code));
					continue;
				}
				++own;
				if (seen.find(Outcome(stated[k].http, code)) != seen.end())
					continue;
				std::fprintf(stderr, "%s declares %d %s, and no case was answered with it\n",
				             routeName(ep).c_str(), stated[k].http, code.c_str());
				agree = false;
			}
		}
	}
	if (out != NULL)
		std::fclose(out);

	for (std::set<std::string>::const_iterator r = rules_stated.begin(); r != rules_stated.end(); ++r)
	{
		if (watched().rules_reached.count(*r) != 0)
			continue;
		std::fprintf(stderr, "the rule refusal %s is stated and no case was answered with it\n",
		             r->c_str());
		agree = false;
	}

	const std::set<std::string> examples = routesWithBodyExamples();
	for (std::set<std::string>::const_iterator e = examples.begin(); e != examples.end(); ++e)
	{
		if (examplesAccepted().count(*e) != 0)
			continue;
		std::fprintf(stderr, "%s gives a body example no case sent through it and saw accepted\n",
		             e->c_str());
		agree = false;
	}
	recordCount("body examples their routes accepted", examplesAccepted().size());

	recordCount("refusals the routes declare of their own", own);
	recordCount("refusals the shipped routes were seen to give", refusals_seen);
	recordCount("rule refusals the document states", rules_stated.size());
	return agree;
}

int sendBodyExample(const char *method, const char *path,
                    const std::map<std::string, std::string> &fills,
                    const std::string &from, const std::string &to)
{
	std::string verb = method;
	for (size_t k = 0; k < verb.size(); ++k)
		verb[k] = (char)(verb[k] - 'A' + 'a');
	const ::Json::Value &op = documentParsed()["paths"][path][verb];
	const ::Json::Value &example = op["requestBody"]["content"]["application/json"]["example"];
	if (example.isNull())
		return 0;

	std::string target = path;
	std::string query;
	const ::Json::Value &params = op["parameters"];
	for (::Json::ArrayIndex i = 0; i < params.size(); ++i)
	{
		const std::string name = params[i]["name"].asString();
		const std::string in = params[i]["in"].asString();
		std::map<std::string, std::string>::const_iterator fill = fills.find(name);
		const bool given = params[i].isMember("example");
		if (in == "path")
		{
			const std::string value = (fill != fills.end()) ? fill->second
			                                               : asText(params[i]["example"]);
			const std::string hole = "{" + name + "}";
			target.replace(target.find(hole), hole.size(), value);
		}
		else if (in == "query" && (params[i]["required"].asBool() || fill != fills.end()))
		{
			query += (query.empty() ? "" : "&") + name + "=" +
			         ((fill != fills.end()) ? fill->second : given ? asText(params[i]["example"]) : "");
		}
	}

	std::string body = asText(example);
	if (!from.empty())
	{
		for (size_t at = body.find(from); at != std::string::npos; at = body.find(from, at + to.size()))
			body.replace(at, from.size(), to);
	}

	const Response r = dispatch(methodFromString(method), target, query, body, "127.0.0.1",
	                            AuthLevel::System, std::string(), std::string(),
	                            "box.example:8081");
	if (r.code >= 200 && r.code < 300)
		examplesAccepted().insert(std::string(method) + " " + path);
	else
		std::fprintf(stderr, "%s %s refused its own body example %s with %d: %s\n", method, target.c_str(),
		             body.c_str(), r.code, r.body.c_str());
	return r.code;
}
