/*
	remotebox_client - the client another box files its recordings here with,
	driven from the shell one call at a time.

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

/* test/http/check-remotebox.sh runs the client code a peer box speaks to this
   box's /api/v1 with against a running box, and reads the box back over curl
   after every write. This program is the thin end of that: one process is one
   call, so the shell sees every step - the login the client makes on its own
   when the box refuses a write included - exactly as the peer would make it,
   and nothing here re-implements the wire. The client itself is the peer's
   own file under peer/system, kept word for word.

   usage: remotebox_client <[user:pass@]host[:port]> <command> [args]
     safety                                    the two margins, in seconds
     timers                                    every recording timer the box holds
     channel <hex id>                          the name the box gives a channel
     add <hex id> <start> <stop> <announce> [local_pre local_post]
     modify <id> <start> <stop> <announce>
     remove <id>

   The first line printed is always "result=<word> http=<code>"; the words are
   the client's own outcomes. Data lines follow, one per item. The exit code
   repeats the outcome: 0 OK, 1 NO_ANSWER, 2 NOT_PERMITTED, 3 NOT_THERE,
   4 REFUSED; 64 for a call this program did not understand, 74 when the
   report channel could not be set up. */

#include <system/remotetimer.h>

#include <global.h>
#include <gui/widget/progresswindow.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <unistd.h>

/* The client's http tool draws on a progress window and reads the proxy
   settings off the program's settings; neither exists here. The same three
   lines and the one definition answer for them in the suite the client was
   written with. */
SNeutrinoSettings g_settings;
unsigned int CProgressWindow::getGlobalStatus(void) { return 0; }
void CProgressWindow::showLocalStatus(const unsigned int, const unsigned int, const std::string &) {}
void CProgressWindow::showGlobalStatus(const unsigned int, const unsigned int, const std::string &) {}

namespace
{

const char *word(CRemoteTimerClient::result_t r)
{
	switch (r)
	{
		case CRemoteTimerClient::RESULT_OK:            return "OK";
		case CRemoteTimerClient::RESULT_NO_ANSWER:     return "NO_ANSWER";
		case CRemoteTimerClient::RESULT_NOT_PERMITTED: return "NOT_PERMITTED";
		case CRemoteTimerClient::RESULT_NOT_THERE:     return "NOT_THERE";
		case CRemoteTimerClient::RESULT_REFUSED:       return "REFUSED";
	}
	return "REFUSED";
}

int usage()
{
	fputs("usage: remotebox_client <[user:pass@]host[:port]> safety | timers | channel <hex> |\n"
	      "       add <hex> <start> <stop> <announce> [local_pre local_post] |\n"
	      "       modify <id> <start> <stop> <announce> | remove <id>\n", stderr);
	return 64;
}

/* Whole and nothing else: a number with a tail, none at all, or one too wide
   for the type is a call this program did not understand, not an id of
   nought and not the id its low bits happen to spell. */
bool number(const char *text, long long &out)
{
	char *end = NULL;
	errno = 0;
	out = strtoll(text, &end, 10);
	return *text != 0 && end != NULL && *end == 0 && errno != ERANGE;
}

bool hexId(const char *text, t_channel_id &out)
{
	char *end = NULL;
	errno = 0;
	out = (t_channel_id) strtoull(text, &end, 16);
	return *text != 0 && end != NULL && *end == 0 && errno != ERANGE;
}

/* The client takes ids and margins as int and times as time_t, which is
   narrower than what was parsed - on a 32-bit box for the times as well. */
bool fitsInt(long long v)
{
	return v >= INT_MIN && v <= INT_MAX;
}

bool fitsTime(long long v)
{
	return (long long) (time_t) v == v;
}

/* A timer as the client takes one: value initialised, because the client
   reads every member it sends and a member left as the stack had it would be
   sent too. */
CTimerd::responseGetTimer oneOff(long long start, long long stop, long long announce)
{
	CTimerd::responseGetTimer timer = CTimerd::responseGetTimer();
	timer.eventType = CTimerd::TIMER_RECORD;
	timer.alarmTime = (time_t) start;
	timer.stopTime = (time_t) stop;
	timer.announceTime = (time_t) announce;
	timer.eventRepeat = CTimerd::TIMERREPEAT_ONCE;
	timer.repeatCount = 0;
	timer.epg_starttime = 0;
	return timer;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 3)
		return usage();

	/* The report goes to what stdout was when this started, and stdout itself
	   is pointed at stderr from here on: the peer's http tool says what it does
	   on stdout when DEBUG is defined, which this tree's debug builds do, and a
	   caller reading the report by line must not find those steps in it. The
	   file is the peer's and stays as it is; the seam is on this side. */
	FILE *report = fdopen(dup(STDOUT_FILENO), "w");
	if (report == NULL)
	{
		perror("remotebox_client: report channel");
		return 74;
	}
	fflush(stdout);
	if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
	{
		perror("remotebox_client: stdout");
		return 74;
	}

	remotebox_address_t box;
	if (!parseRemoteBoxAddress(argv[1], box))
		return usage();

	const std::string command = argv[2];
	CRemoteTimerClient client(box);
	CRemoteTimerClient::result_t r;
	std::string data;
	char line[512];

	if (command == "safety" && argc == 3)
	{
		int pre = 0, post = 0;
		r = client.getRecordingSafety(pre, post);
		snprintf(line, sizeof(line), "safety pre=%d post=%d\n", pre, post);
		data = line;
	}
	else if (command == "timers" && argc == 3)
	{
		CTimerd::TimerList list;
		r = client.getTimers("peer", list);
		for (CTimerd::TimerList::const_iterator t = list.begin(); t != list.end(); ++t)
		{
			snprintf(line, sizeof(line),
				"timer id=%d channel_id=%llx start=%lld stop=%lld announce=%lld repeat=%d"
				" repeat_count=%u state=%d rem_pre=%d rem_post=%d title=%s\n",
				t->eventID, (unsigned long long) t->channel_id, (long long) t->alarmTime,
				(long long) t->stopTime, (long long) t->announceTime, (int) t->eventRepeat,
				(unsigned) t->repeatCount, (int) t->eventState, t->rem_pre, t->rem_post,
				t->epgTitle);
			data += line;
		}
	}
	else if (command == "channel" && argc == 4)
	{
		t_channel_id id = 0;
		if (!hexId(argv[3], id))
			return usage();
		std::string name;
		r = client.getChannelName(id, name);
		data = "name=" + name + "\n";
	}
	else if (command == "add" && (argc == 7 || argc == 9))
	{
		t_channel_id id = 0;
		long long start = 0, stop = 0, announce = 0, local_pre = 0, local_post = 0;
		if (!hexId(argv[3], id) || !number(argv[4], start) || !number(argv[5], stop)
			|| !number(argv[6], announce))
			return usage();
		if (argc == 9 && (!number(argv[7], local_pre) || !number(argv[8], local_post)))
			return usage();
		if (!fitsTime(start) || !fitsTime(stop) || !fitsTime(announce)
			|| !fitsInt(local_pre) || !fitsInt(local_post))
			return usage();
		CTimerd::responseGetTimer timer = oneOff(start, stop, announce);
		timer.channel_id = id;
		r = client.addRecordTimer(timer, (int) local_pre, (int) local_post);
	}
	else if (command == "modify" && argc == 7)
	{
		long long id = 0, start = 0, stop = 0, announce = 0;
		if (!number(argv[3], id) || !number(argv[4], start) || !number(argv[5], stop)
			|| !number(argv[6], announce))
			return usage();
		if (!fitsInt(id) || !fitsTime(start) || !fitsTime(stop) || !fitsTime(announce))
			return usage();
		CTimerd::responseGetTimer timer = oneOff(start, stop, announce);
		timer.eventID = (int) id;
		r = client.modifyTimer(timer);
	}
	else if (command == "remove" && argc == 4)
	{
		long long id = 0;
		if (!number(argv[3], id) || !fitsInt(id))
			return usage();
		r = client.removeTimer((int) id);
	}
	else
		return usage();

	fprintf(report, "result=%s http=%ld\n", word(r), client.httpCode());
	fputs(data.c_str(), report);
	fclose(report);
	return (int) r;
}
