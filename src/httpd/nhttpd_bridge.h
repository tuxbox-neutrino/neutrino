/*
 * nhttpd_bridge.h - nhttpd's hook into the REST layer
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * One hook, attached behind nhttpd's authentication and before every hook
 * that claims a URL by its file extension. It claims the API namespace in
 * the prepare pass, so no legacy hook sees the request, and answers in the
 * send pass by handing the request to httpd::dispatch() and the Response
 * back into the hook handler in api mode.
 *
 * This header is read from src/nhttpd, which is built without exceptions;
 * it names nothing of the layer beyond a forward declaration. The catch
 * for anything the layer throws is in the implementation, which is built
 * with them.
 */

#ifndef __httpd_nhttpd_bridge_h__
#define __httpd_nhttpd_bridge_h__

#include <string>

#include "yhook.h"

namespace httpd
{
struct Response;
}

namespace nhttpd_bridge
{
/* What the transport does with a whole-file answer of a known length once
   the request's Range header (raw value, empty for none) is read against
   it: the code to state, the stretch to send, and the Content-Range line
   where one belongs. Only a 200 takes ranges - everything else sends the
   whole file under its own code, which is what the source's server does. A 416
   sends no file at all; the caller answers with a problem document that
   carries content_range, so a caller that asked past the end can work out
   what to ask for without fetching anything. */
struct FdPlan
{
	int code;
	uint64_t offset;
	uint64_t length;
	std::string content_range;
};
FdPlan planFdAnswer(int code, uint64_t file_length, const std::string &range_header);

/* Whether the file a route answers with (fd) is held back: one of the files
   that hold who may reach the box, by inode (yprotect); one without a name,
   but one the route made for this answer itself (own_unnamed); and one that
   lies in the configuration directory, /proc, /sys or /dev, but one of the
   box's own documents opened by its fixed name (box_document) in the first. */
bool heldBack(int fd, bool own_unnamed, bool box_document);
}

#include "httpd/server_control.h"

class CApiBridge : public Cyhook
{
public:
	CApiBridge();
	virtual ~CApiBridge();

	virtual std::string getHookName(void) { return "api bridge"; }
	virtual std::string getHookVersion(void) { return "1.0.0"; }
	virtual THandleStatus Hook_PrepareResponse(CyhookHandler *hh);
	virtual THandleStatus Hook_SendResponse(CyhookHandler *hh);
	// The stream pass: registers the stream (or refuses it as an ordinary
	// answer), writes the header, and pumps the connection until it ends.
	// A carried answer goes to its own thread instead (carry.h), and the
	// pass returns as soon as it runs there.
	virtual THandleStatus Hook_SendStream(CyhookHandler *hh, CySocket *sock);

	// Whether the tables are ones this server can answer from. Asked once
	// before the hook is attached; a false answer keeps it off, with why.
	static bool routesOk(std::string *why);
	// The prefixes this hook answers, told to the core so the request reader
	// treats them as API targets.
	static void registerPrefixes();
	/* Everything this hook keeps running beside the requests - the event
	   streams and the carried answers - started with the server and ended
	   with it, each behind the exception barrier every entry from nhttpd
	   has. startStreams opens the door to carried answers again; a job an
	   earlier stop ended stays ended. stopStreams closes it, cancels every
	   carried answer, stops the event streams and waits for them as long as
	   it takes, and waits for the carried answers up to carry::stopBoundMs():
	   they run on threads of their own and touch no hook. */
	static void startStreams();
	static void stopStreams();
	// Whether this request is one of ours, on the decoded url.
	static bool claims(const CyhookHandler *hh);
	// Response into the hook handler: status, headers, body; a file as its
	// descriptor; an event stream or a carried answer (a program to run, an
	// address to relay) handed to the stream pass, or refused with 501 where
	// nothing carries its kind.
	static void answerWith(CyhookHandler *hh, const httpd::Response &r);
};

#endif /* __httpd_nhttpd_bridge_h__ */
