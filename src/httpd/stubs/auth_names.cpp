/*
 * auth_names.cpp - the names the router and the document ask of auth
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The source's auth.cpp carries sessions, tokens, credentials and CSRF. On
 * nhttpd, until the session layer arrives, access is what nhttpd's own
 * Basic authentication grants, and the router is told the level from
 * the bridge. What the router and the OpenAPI generator still ask of
 * auth are five names; they are answered here, so the document names
 * the same headers and parameters a session layer will use later.
 */

#include "httpd/auth.h"

namespace httpd
{

const char *notPermittedDetail()
{
	return "this endpoint is not open to this caller";
}

const char *authLevelName(AuthLevel a)
{
	switch (a)
	{
		case AuthLevel::Public: return "public";
		case AuthLevel::Read:   return "read";
		case AuthLevel::Write:  return "write";
		case AuthLevel::System: return "system";
	}
	return "system";
}

const char *sessionCookieName()
{
	return "neutrino-session";
}

const char *csrfHeaderName()
{
	return "X-CSRF-Token";
}

const char *queryTokenName()
{
	return "token";
}

} // namespace httpd
