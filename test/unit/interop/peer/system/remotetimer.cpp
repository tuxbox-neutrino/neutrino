/*
	Neutrino-GUI  -   DBoxII-Project

	Remote timers: what this box asks another box's HTTP API for.

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <system/remotetimer.h>

#include <system/helpers.h>
#include <system/helpers-json.h>

#include <timerdclient/timerdclient.h>

#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

/* Where the routes live. Written once, because a path spelt twice is two that
   come apart. */
const char kLogin[]    = "/api/v1/login";
const char kTimers[]   = "/api/v1/timers";
const char kChannels[] = "/api/v1/channels/";
const char kRecording[] = "/api/v1/settings/recording";

const char kJson[] = "application/json";

/* The seconds a call may take. Well under a screen's patience and far under the
   client's own default, which is half an hour and meant for a download. */
const int kTimeout = 20;

const int kSecondsPerMinute = 60;

/* A member as a number whether it arrived as one or as the text of one. The
   API answers times and counts as numbers, but a body is another box's and a
   reader that insisted on the type would drop a whole list over one member. */
bool jsonNumber(const Json::Value &from, const char *key, long long &out)
{
	if (!from.isObject() || !from.isMember(key))
		return false;

	const Json::Value &m = from[key];
	if (m.isIntegral())
	{
		out = m.asInt64();
		return true;
	}
	if (m.isString())
	{
		out = ::strtoll(m.asCString(), NULL, 10);
		return true;
	}
	return false;
}

/* Asked for by type rather than converted, because jsoncpp is built with
   exceptions and this tree is not: a conversion it refuses would leave the
   program through a throw nothing here can catch. */
std::string jsonText(const Json::Value &from, const char *key)
{
	if (!from.isObject() || !from.isMember(key))
		return std::string();

	const Json::Value &m = from[key];
	return m.isString() ? m.asString() : std::string();
}

/* An identifier, which the API states as hexadecimal text and not as a number:
   it is sixty four bits wide and the numbers JSON carries are not. */
uint64_t jsonHexId(const Json::Value &from, const char *key)
{
	const std::string text = jsonText(from, key);
	if (text.empty())
		return 0;
	return (uint64_t) ::strtoull(text.c_str(), NULL, 16);
}

void copyInto(char *into, size_t room, const std::string &text)
{
	strncpy(into, text.c_str(), room - 1);
	into[room - 1] = 0;
}

/* An identifier as the routes spell one: lowercase hexadecimal with nothing in
   front of it. */
std::string hexId(t_channel_id id)
{
	char text[24];
	snprintf(text, sizeof(text), "%llx", (unsigned long long) id);
	return std::string(text);
}

/* One member of an object being written, with the comma the member before it
   needs. */
void appendNumber(std::string &into, bool &first, const char *key, long long value)
{
	if (!first)
		into += ',';
	first = false;
	into += '"';
	into += key;
	into += "\":";
	into += to_string((long long) value);
}

void appendFlag(std::string &into, bool &first, const char *key, bool value)
{
	if (!first)
		into += ',';
	first = false;
	into += '"';
	into += key;
	into += "\":";
	into += value ? "true" : "false";
}

void appendText(std::string &into, bool &first, const char *key, const std::string &value)
{
	if (!first)
		into += ',';
	first = false;
	into += '"';
	into += key;
	into += "\":\"";
	/* What JSON insists on and nothing more. Every text this writes is a name, a
	   password or an identifier, so the escape is here for the password alone,
	   which is whatever the person typed. */
	for (size_t i = 0; i < value.length(); ++i)
	{
		const unsigned char c = (unsigned char) value[i];
		if (c == '"' || c == '\\')
		{
			into += '\\';
			into += (char) c;
		}
		else if (c < 0x20)
		{
			char escape[8];
			snprintf(escape, sizeof(escape), "\\u%04x", c);
			into += escape;
		}
		else
			into += (char) c;
	}
	into += '"';
}

} // namespace

bool parseRemoteBoxAddress(const std::string &text, remotebox_address_t &out)
{
	std::string rest = text;

	/* The credential first, and at the last at sign rather than the first: a
	   password may carry one and a host name may not. */
	const size_t at = rest.rfind('@');
	if (at != std::string::npos)
	{
		const std::string credential = rest.substr(0, at);
		rest = rest.substr(at + 1);

		const size_t colon = credential.find(':');
		if (colon == std::string::npos)
			out.user = credential;
		else
		{
			out.user = credential.substr(0, colon);
			out.pass = credential.substr(colon + 1);
		}
	}

	/* A bracketed address keeps whatever is inside the brackets, so the port is
	   only ever looked for behind them. */
	size_t from = 0;
	if (!rest.empty() && rest[0] == '[')
	{
		const size_t close = rest.find(']');
		if (close == std::string::npos)
			return false;
		from = close + 1;
	}

	const size_t colon = rest.find(':', from);
	if (colon == std::string::npos)
		out.address = rest;
	else
	{
		const std::string port = rest.substr(colon + 1);
		/* Only digits, so an address that is all colons and nothing else does
		   not lose its tail to a port that is not there. */
		bool numeric = !port.empty();
		for (size_t i = 0; i < port.length() && numeric; ++i)
			numeric = port[i] >= '0' && port[i] <= '9';

		if (numeric)
		{
			out.address = rest.substr(0, colon);
			out.port = (unsigned int) ::atoi(port.c_str());
		}
		else
			out.address = rest;
	}

	return !out.address.empty();
}

CRemoteTimerClient::CRemoteTimerClient(const remotebox_address_t &b, int ct)
	: box(b)
	, connecttimeout(ct)
	, safety_known(false)
	, safety_pre(0)
	, safety_post(0)
{
}

std::string CRemoteTimerClient::url(const std::string &path) const
{
	std::string out = "http://";
	out += box.address;
	out += ':';
	out += to_string(box.port);
	out += path;
	return out;
}

CRemoteTimerClient::result_t CRemoteTimerClient::login()
{
	/* Whatever the box handed out before is thrown away first. A login carrying
	   a session that is still live is a request that changes something under a
	   live cookie, which the gate at the other end answers by asking for the
	   second token; carrying none, it is asked for nothing. */
	http.clearCookies();
	csrf.clear();
	csrf_header.clear();

	std::string body = "{";
	bool first = true;
	appendText(body, first, "user", box.user);
	appendText(body, first, "password", box.pass);
	body += '}';

	std::string answer;
	if (!http.sendRequest("POST", url(kLogin), answer, body, kJson,
			std::vector<std::string>(), connecttimeout, kTimeout))
		return RESULT_NO_ANSWER;

	const long code = http.getLastHttpCode();
	if (code < 200 || code >= 300)
		return code == 401 || code == 403 ? RESULT_NOT_PERMITTED : RESULT_REFUSED;

	std::string err;
	Json::Value root;
	if (!parseJsonFromString(answer, &root, &err))
		return RESULT_REFUSED;

	csrf = jsonText(root, "csrf");
	/* The name of the header comes out of the answer and is not written down
	   here: the server states it so that a caller does not hold a second copy of
	   its own spelling. */
	csrf_header = jsonText(root, "csrf_header");
	if (csrf.empty() || csrf_header.empty())
		return RESULT_REFUSED;

	return RESULT_OK;
}

CRemoteTimerClient::result_t CRemoteTimerClient::call(const std::string &method,
	const std::string &path, const std::string &body, std::string &answer)
{
	for (int attempt = 0; attempt < 2; attempt++)
	{
		std::vector<std::string> headers;
		if (!csrf.empty() && !csrf_header.empty())
			headers.push_back(csrf_header + ": " + csrf);

		answer.clear();
		if (!http.sendRequest(method, url(path), answer, body,
				body.empty() ? std::string() : std::string(kJson), headers,
				connecttimeout, kTimeout))
			return RESULT_NO_ANSWER;

		const long code = http.getLastHttpCode();
		if (code >= 200 && code < 300)
			return RESULT_OK;
		if (code == 404)
			return RESULT_NOT_THERE;
		if (code != 401 && code != 403)
			return RESULT_REFUSED;

		/* Refused for want of a session. One login and one more try: a second
		   refusal is the box saying no to the credential itself, and asking
		   again would only spend the other end's count of attempts. */
		if (attempt > 0)
			return RESULT_NOT_PERMITTED;

		const result_t opened = login();
		if (opened != RESULT_OK)
			return opened;
	}

	return RESULT_NOT_PERMITTED;
}

bool CRemoteTimerClient::readTimers(const std::string &body, const std::string &rbname,
	CTimerd::TimerList &out)
{
	std::string text = body;
	std::string err;
	Json::Value root;
	if (!parseJsonFromString(text, &root, &err))
		return false;

	const Json::Value &items = root["items"];
	if (!items.isArray())
		return false;

	for (unsigned int i = 0; i < items.size(); i++)
	{
		const Json::Value &item = items[i];
		/* Recordings alone, as this screen has always shown: the other kinds a
		   box holds are its own business and none of them can be fetched here. */
		if (jsonText(item, "kind") != "record")
			continue;

		/* Value initialised, so a member the answer leaves out reads as nought
		   and not as whatever was on the stack. The audio selection is the one
		   this used to get wrong: the old answer said only whether it was the
		   box's own, and where it did not the field was never written at all. */
		CTimerd::responseGetTimer timer = CTimerd::responseGetTimer();
		long long number = 0;

		timer.eventType = CTimerd::TIMER_REMOTEBOX;
		copyInto(timer.remotebox_name, sizeof(timer.remotebox_name), rbname);

		if (jsonNumber(item, "id", number))
			timer.eventID = (int) number;
		if (jsonNumber(item, "state", number))
			timer.eventState = (CTimerd::CTimerEventStates) number;
		/* Whole, weekday bits and all. The old wire carried the repeat masked
		   and the weekdays beside it as seven letters, which had to be folded
		   back together; this one carries the daemon's own number. */
		if (jsonNumber(item, "repeat", number))
			timer.eventRepeat = (CTimerd::CTimerEventRepeat) number;
		if (jsonNumber(item, "repeat_count", number))
			timer.repeatCount = (uint32_t) number;
		if (jsonNumber(item, "start", number))
			timer.alarmTime = (time_t) number;
		if (jsonNumber(item, "stop", number))
			timer.stopTime = (time_t) number;
		if (jsonNumber(item, "announce", number))
			timer.announceTime = (time_t) number;
		if (jsonNumber(item, "epg_start", number))
			timer.epg_starttime = (time_t) number;

		timer.channel_id = jsonHexId(item, "channel_id");
		timer.epg_id = jsonHexId(item, "epg_id");
		copyInto(timer.epgTitle, sizeof(timer.epgTitle), jsonText(item, "title"));

		/* The API neither reports a per timer audio selection nor takes one, so
		   every timer it makes records whatever the box it runs on is set to.
		   Saying anything else here would draw a selection this box can neither
		   read nor set. */
		timer.apids = TIMERD_APIDS_CONF;

		out.push_back(timer);
	}

	return true;
}

bool CRemoteTimerClient::readRecordingSafety(const std::string &body, int &pre, int &post)
{
	std::string text = body;
	std::string err;
	Json::Value root;
	if (!parseJsonFromString(text, &root, &err))
		return false;

	const Json::Value &items = root["items"];
	if (!items.isArray())
		return false;

	bool seen_pre = false;
	bool seen_post = false;
	for (unsigned int i = 0; i < items.size(); i++)
	{
		const std::string id = jsonText(items[i], "id");
		long long minutes = 0;
		if (!jsonNumber(items[i], "value", minutes))
			continue;

		/* Minutes here and seconds everywhere the daemon is asked, which is the
		   one thing this read is for. The screen that offers the two settings
		   divides by sixty on its way in and multiplies on its way out, and the
		   settings route answers what that screen shows. */
		if (id == "record_safety_time_before")
		{
			pre = (int) minutes * kSecondsPerMinute;
			seen_pre = true;
		}
		else if (id == "record_safety_time_after")
		{
			post = (int) minutes * kSecondsPerMinute;
			seen_post = true;
		}
	}

	return seen_pre && seen_post;
}

bool CRemoteTimerClient::readChannelName(const std::string &body, std::string &name)
{
	std::string text = body;
	std::string err;
	Json::Value root;
	if (!parseJsonFromString(text, &root, &err))
		return false;

	name = jsonText(root, "name");
	return !name.empty();
}

std::string CRemoteTimerClient::timerBody(const remotetimer_t &timer, bool creating)
{
	std::string body = "{";
	bool first = true;

	if (creating)
	{
		appendText(body, first, "kind", "record");
		appendText(body, first, "channel_id", hexId(timer.channel_id));
	}

	appendNumber(body, first, "start", (long long) timer.alarm);
	appendNumber(body, first, "stop", (long long) timer.stop);
	appendNumber(body, first, "announce", (long long) timer.announce);
	appendNumber(body, first, "repeat", timer.repeat);
	appendNumber(body, first, "repeat_count", (long long) timer.repeat_count);

	/* Only on the way in. The route that changes a timer takes neither the guide
	   entry nor the two flags below, the daemon having no way to move any of
	   them once the timer is built.

	   Both flags are asked for whatever this box did with its own copy, which is
	   what the old wire's rs and aj said and for the same reason: the times sent
	   above are the programme's, so the other box has to put its own margins on
	   them, and a recording that has moved in the guide is one the other box
	   follows rather than one this box would have to notice and send again. */
	if (creating)
	{
		appendNumber(body, first, "epg_start", (long long) timer.epg_start);
		appendFlag(body, first, "recording_safety", true);
		appendFlag(body, first, "auto_adjust", true);
	}

	body += '}';
	return body;
}

CRemoteTimerClient::result_t CRemoteTimerClient::getRecordingSafety(int &pre, int &post)
{
	if (safety_known)
	{
		pre = safety_pre;
		post = safety_post;
		return RESULT_OK;
	}

	std::string answer;
	const result_t got = call("GET", kRecording, std::string(), answer);
	if (got != RESULT_OK)
		return got;

	if (!readRecordingSafety(answer, safety_pre, safety_post))
		return RESULT_REFUSED;

	safety_known = true;
	pre = safety_pre;
	post = safety_post;
	return RESULT_OK;
}

CRemoteTimerClient::result_t CRemoteTimerClient::getTimers(const std::string &rbname,
	CTimerd::TimerList &out)
{
	std::string answer;
	const result_t got = call("GET", kTimers, std::string(), answer);
	if (got != RESULT_OK)
		return got;

	const size_t first = out.size();
	if (!readTimers(answer, rbname, out))
		return RESULT_REFUSED;

	/* The margins the other box puts on a recording, which the rows just read
	   need so that one fetched over here comes back the length it was meant to
	   be. A box that will not say leaves them at nought, which is what a box
	   with no margins reports anyway. */
	int pre = 0, post = 0;
	getRecordingSafety(pre, post);
	for (size_t i = first; i < out.size(); i++)
	{
		out[i].rem_pre = pre;
		out[i].rem_post = post;
	}

	return RESULT_OK;
}

CRemoteTimerClient::result_t CRemoteTimerClient::getChannelName(t_channel_id id,
	std::string &name)
{
	std::string answer;
	const result_t got = call("GET", kChannels + hexId(id), std::string(), answer);
	if (got != RESULT_OK)
		return got;

	return readChannelName(answer, name) ? RESULT_OK : RESULT_REFUSED;
}

CRemoteTimerClient::result_t CRemoteTimerClient::addRecordTimer(
	const CTimerd::responseGetTimer &timer, int local_pre, int local_post)
{
	remotetimer_t out;
	out.channel_id = timer.channel_id;
	out.repeat = (int) timer.eventRepeat;
	out.repeat_count = timer.repeatCount;
	out.epg_start = timer.epg_starttime;

	/* The programme's own times, which is this box's margins taken back off the
	   ones the daemon filed. What the other box's margins are is never asked:
	   the body says to apply them, so a box set to wider ones holds the
	   recording for its own stretch and not for a copy of this box's. */
	out.alarm = timer.alarmTime + local_pre;
	out.announce = timer.announceTime + local_pre;
	out.stop = timer.stopTime - local_post;

	std::string answer;
	return call("POST", kTimers, timerBody(out, true), answer);
}

CRemoteTimerClient::result_t CRemoteTimerClient::modifyTimer(
	const CTimerd::responseGetTimer &timer)
{
	remotetimer_t out;
	out.alarm = timer.alarmTime;
	out.stop = timer.stopTime;
	out.announce = timer.announceTime;
	out.repeat = (int) timer.eventRepeat;
	out.repeat_count = timer.repeatCount;

	std::string answer;
	return call("PATCH", std::string(kTimers) + "/" + to_string(timer.eventID),
		timerBody(out, false), answer);
}

CRemoteTimerClient::result_t CRemoteTimerClient::removeTimer(int id)
{
	std::string answer;
	return call("DELETE", std::string(kTimers) + "/" + to_string(id), std::string(), answer);
}

void sendAllTimersTo(CTimerdClient *timerd, const std::string &address, bool force)
{
	remotebox_address_t box;
	if (timerd == NULL || !parseRemoteBoxAddress(address, box))
		return;

	CTimerd::TimerList timerlist;
	timerd->getTimerList(timerlist);
	std::sort(timerlist.begin(), timerlist.end());

	int pre = 0, post = 0;
	timerd->getRecordingSafety(pre, post);

	/* The short connect timeout this endpoint has always used: it answers its
	   own caller before the other box has been reached, and a box that is off
	   must not hold that answer up. */
	CRemoteTimerClient client(box, 300);

	for (CTimerd::TimerList::iterator timer = timerlist.begin(); timer != timerlist.end(); ++timer)
	{
		if (timer->eventType != CTimerd::TIMER_RECORD)
			continue;

		const bool arrived = client.addRecordTimer(*timer, pre, post) == CRemoteTimerClient::RESULT_OK;
		if (arrived || force)
			timerd->removeTimerEvent(timer->eventID);
	}
}
