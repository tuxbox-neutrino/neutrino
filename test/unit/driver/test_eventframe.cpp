/*
 * test_eventframe.cpp - what the GUI loop makes of an event that did not
 * arrive the way its header says
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"
#include "support/rcfixture.h"

#include <eventserver.h>

#include <cstring>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

// What a sender writes before it goes: the header, then as much of the body
// as it got out.
void sendFrame(unsigned eventID, unsigned initiatorID, unsigned dataSize,
	       const char *body, size_t bodyBytes)
{
	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	REQUIRE(s >= 0);
	struct sockaddr_un a;
	memset(&a, 0, sizeof(a));
	a.sun_family = AF_UNIX;
	strncpy(a.sun_path, currentEventSocketPath(), sizeof(a.sun_path) - 1);
	REQUIRE(connect(s, (struct sockaddr *) &a, sizeof(a)) == 0);

	CEventServer::eventHead head;
	head.eventID = eventID;
	head.initiatorID = initiatorID;
	head.dataSize = dataSize;
	REQUIRE(write(s, &head, sizeof(head)) == (ssize_t) sizeof(head));
	if (bodyBytes != 0)
		REQUIRE(write(s, body, bodyBytes) == (ssize_t) bodyBytes);
	close(s);
}

} // namespace

TEST_CASE("an event that arrives whole is handed on", "[rcinput]")
{
	requireFreeEventSocket();
	CRCInput rc;

	sendFrame(NeutrinoMessages::EVT_POPUP, CEventServer::INITID_HTTPD, 6, "hello", 6);

	neutrino_msg_t got = 0;
	neutrino_msg_data_t gotdata = 0;
	rc.getMsg_ms(&got, &gotdata, 500);
	REQUIRE(got == NeutrinoMessages::EVT_POPUP);
	CHECK(std::string((const char *) gotdata) == "hello");
	delete[] (unsigned char *) gotdata;
}

TEST_CASE("an event without a body is handed on", "[rcinput]")
{
	requireFreeEventSocket();
	CRCInput rc;

	sendFrame(NeutrinoMessages::STANDBY_TOGGLE, CEventServer::INITID_HTTPD, 0, NULL, 0);

	neutrino_msg_t got = 0;
	neutrino_msg_data_t gotdata = 0;
	rc.getMsg_ms(&got, &gotdata, 500);
	CHECK(got == NeutrinoMessages::STANDBY_TOGGLE);
}

TEST_CASE("an event whose body stops short of what its header says is dropped", "[rcinput]")
{
	/* A sender that runs out of time part way through a body closes the
	   connection and keeps the event to send again. What did arrive is a
	   header and the start of a body, here a text whose end is not among
	   it. Handed on, it was read past what came in, and the event then
	   arrived a second time with the retry. */
	requireFreeEventSocket();
	CRCInput rc;

	sendFrame(NeutrinoMessages::EVT_POPUP, CEventServer::INITID_HTTPD, 64, "cut here", 8);

	neutrino_msg_t got = 0;
	neutrino_msg_data_t gotdata = 0;
	rc.getMsg_ms(&got, &gotdata, 300);
	CHECK(got != NeutrinoMessages::EVT_POPUP);
	if (got == NeutrinoMessages::EVT_POPUP)
		delete[] (unsigned char *) gotdata;
}

TEST_CASE("an event whose header claims more than any event carries is not read", "[rcinput]")
{
	/* The size is the sender's and was taken as it came: all ones made the
	   buffer for it one byte longer than the largest number, which is no
	   buffer at all, and the body was then read into that. */
	requireFreeEventSocket();
	CRCInput rc;

	sendFrame(NeutrinoMessages::EVT_POPUP, CEventServer::INITID_HTTPD, 0xffffffffu, "cut here", 8);

	neutrino_msg_t got = 0;
	neutrino_msg_data_t gotdata = 0;
	rc.getMsg_ms(&got, &gotdata, 300);
	CHECK(got != NeutrinoMessages::EVT_POPUP);
	if (got == NeutrinoMessages::EVT_POPUP)
		delete[] (unsigned char *) gotdata;
}
