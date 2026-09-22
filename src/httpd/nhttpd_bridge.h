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

class CApiBridge : public Cyhook
{
public:
	CApiBridge();
	virtual ~CApiBridge();

	virtual std::string getHookName(void) { return "api bridge"; }
	virtual std::string getHookVersion(void) { return "1.0.0"; }
	virtual THandleStatus Hook_PrepareResponse(CyhookHandler *hh);
	virtual THandleStatus Hook_SendResponse(CyhookHandler *hh);

	// Whether the tables are ones this server can answer from. Asked once
	// before the hook is attached; a false answer keeps it off, with why.
	static bool routesOk(std::string *why);
	// The prefixes this hook answers, told to the core so the request reader
	// treats them as API targets.
	static void registerPrefixes();
	// Whether this request is one of ours, on the decoded url.
	static bool claims(const CyhookHandler *hh);
	// Response into the hook handler: status, headers, body; a file read
	// whole; the shapes nhttpd cannot carry yet refused with 501.
	static void answerWith(CyhookHandler *hh, const httpd::Response &r);
};

#endif /* __httpd_nhttpd_bridge_h__ */
