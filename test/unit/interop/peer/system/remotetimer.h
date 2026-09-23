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

#ifndef __remotetimer__
#define __remotetimer__

#include <string>

#include <timerdclient/timerdtypes.h>
#include <system/httptool.h>

class CTimerdClient;

/* Where another box answers and what this box presents itself with there. The
   credential is held apart from the address because it no longer travels in the
   address: the API wants it in the body of a login route. */
struct remotebox_address_t
{
	std::string address;
	unsigned int port;
	std::string user;
	std::string pass;

	remotebox_address_t() : port(80) {}
};

/* "[user:pass@]host[:port]", which is how both the box's own list and the one
   parameter of the legacy send endpoint spell a remote box. The tail is read as
   a port only where it is all digits, so an address carrying colons of its own
   keeps them. */
bool parseRemoteBoxAddress(const std::string &text, remotebox_address_t &out);

/* What one timer is on the wire, which is narrower than the daemon's own answer
   on purpose: these are the fields the two write routes take, and a caller
   handing over the whole answer would be handing over fields no route reads. */
struct remotetimer_t
{
	int          id;
	t_channel_id channel_id;
	time_t       alarm;
	time_t       stop;
	time_t       announce;
	time_t       epg_start;
	int          repeat;
	uint32_t     repeat_count;

	remotetimer_t()
		: id(0), channel_id(0), alarm(0), stop(0), announce(0), epg_start(0),
		  repeat(0), repeat_count(0) {}
};

/* ONE CONVERSATION WITH ONE OTHER BOX.

   One object is one session. The login route answers with a cookie and with a
   second token; the client holds both for as long as it lives and sends the
   second token back in the header that answer named, on every call that changes
   something.

   Nothing logs in up front. A box may grant what is asked without a session at
   all, and a login it never asked for is a credential handed over for nothing;
   so a call is made as it stands and the credential is presented only once the
   box has refused. The retry happens once, and a second refusal is the caller's
   to report. */
class CRemoteTimerClient
{
	public:
		enum result_t
		{
			RESULT_OK,
			// Nothing came back at all: the box is off, or the address is wrong.
			RESULT_NO_ANSWER,
			// The box answered, and what it said is that the name and the
			// password open nothing.
			RESULT_NOT_PERMITTED,
			// The box holds nothing of that name, which for a channel and for a
			// timer is an answer and not a fault.
			RESULT_NOT_THERE,
			// Anything else it refused with, and an answer this could not read.
			RESULT_REFUSED
		};

		CRemoteTimerClient(const remotebox_address_t &box, int connecttimeout = 10000);

		/* Every recording timer the other box holds, appended as this box's own
		   rows with the other box's name and its two margins stamped into each.

		   The margins are answered in SECONDS, which is what the daemon here
		   keeps them in. The API states them in minutes, and the turn is made
		   here rather than at each caller. */
		result_t getTimers(const std::string &rbname, CTimerd::TimerList &out);
		result_t getRecordingSafety(int &pre, int &post);
		result_t getChannelName(t_channel_id id, std::string &name);

		/* One recording timer of this box, as the other box should hold it: the
		   margins this box put on it taken off, and the flag that has the other
		   box put its own on instead. The daemon here widens a recording when it
		   files it, so the times read back off it are the programme's plus this
		   box's margins, and those two arguments are what has to come off again.
		   What the other box's margins are is its own business and is never
		   asked for. */
		result_t addRecordTimer(const CTimerd::responseGetTimer &timer, int local_pre,
			int local_post);
		/* A timer the other box already holds, as it now reads here. Its times
		   are the other box's own, so nothing is taken off or put on. */
		result_t modifyTimer(const CTimerd::responseGetTimer &timer);
		result_t removeTimer(int id);

		long httpCode() const { return http.getLastHttpCode(); }

		/* The readers and the writer, named here so the suite can drive them
		   against a body it wrote rather than against a box it has not got. */
		static bool readTimers(const std::string &body, const std::string &rbname,
			CTimerd::TimerList &out);
		static bool readRecordingSafety(const std::string &body, int &pre, int &post);
		static bool readChannelName(const std::string &body, std::string &name);
		static std::string timerBody(const remotetimer_t &timer, bool creating);

	private:
		CRemoteTimerClient(const CRemoteTimerClient &);
		CRemoteTimerClient &operator=(const CRemoteTimerClient &);

		std::string url(const std::string &path) const;
		result_t login();
		/* One call, retried once behind a fresh login where the box refused the
		   first for want of one. */
		result_t call(const std::string &method, const std::string &path,
			const std::string &body, std::string &answer);

		remotebox_address_t box;
		int connecttimeout;
		CHTTPTool http;
		std::string csrf;
		std::string csrf_header;
		bool safety_known;
		int safety_pre;
		int safety_post;
};

/* Every recording timer this box holds, sent to another box, and each one that
   arrived dropped here. force drops the local timer whatever the other box made
   of it, which is what the endpoint offering this has always called force.

   Client code, although the endpoint reaching it is the old server's: what it
   does is talk to another box, and it sat among the old server's helpers only
   because that is where the endpoint was written. */
void sendAllTimersTo(CTimerdClient *timerd, const std::string &address, bool force);

#endif
