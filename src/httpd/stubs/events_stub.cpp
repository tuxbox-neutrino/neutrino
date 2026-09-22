/*
 * events_stub.cpp - what the document generator asks the event stream
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The OpenAPI generator asks two things of the event module: whether a
 * route is the stream, and what content type the stream carries. This
 * tree has no stream yet (it needs a streaming path in nhttpd first), so
 * no route is one. The content type is the one the stream will carry, so
 * the document does not change when the module arrives.
 */

#include "httpd/events.h"

namespace httpd
{
namespace events
{

const char *streamContentType()
{
	return "text/event-stream";
}

bool isStreamRoute(const Endpoint &)
{
	return false;
}

} // namespace events
} // namespace httpd
