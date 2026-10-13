/*
 * login_bridge.h - the one login, asked from outside the REST layer
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The box has one account and keeps it in one place: the name and the
 * stored form of the password in nhttpd-access.conf. The REST layer's login, the
 * old server's gate (mod_auth), the screen at the box (CNhttpdSetup) and
 * the old configuration routes all ask here, so none of them holds a copy
 * of its own, and a password changed in one of them is the password every
 * other one takes, at once and without a restart.
 *
 * Read from src/nhttpd and src/gui, which are built without exceptions:
 * nothing here throws. Every call catches whatever the layer under it does
 * and answers the refusing value.
 */

#ifndef __httpd_login_bridge_h__
#define __httpd_login_bridge_h__

#include <string>

namespace login_bridge
{

/* What the old server's gate knows about a request, as the raw values it read. */
struct Ask
{
	std::string path;           // nhttpd's UrlData["path"], e.g. "/control/"
	std::string peer;           // the far end of the socket
	std::string cookie;         // the Cookie header
	std::string authorization;  // the Authorization header
	std::string sec_fetch_site; // the Sec-Fetch-Site header
	std::string origin;         // the Origin header
	std::string referer;        // the Referer header
	std::string host;           // the Host header

	/* Whether the request carries a template command for the old server's
	   parser (an `execute` parameter), which any .yhtm page runs before it
	   renders: such a request changes the box with a GET wherever it goes. */
	bool runs_commands;

	Ask() : runs_commands(false) {}
};

/* Whether a session of the one login stands for this request.

   On the targets that change the box with a GET (/control/, /cgi-bin/, /fb/,
   /api/v4/, /y/, and any request carrying a template command) only when the
   request shows it came from a page of this box:
   Sec-Fetch-Site same-origin or, from a browser that does not send that header,
   an Origin or else a Referer naming the host the request was sent to. A link on
   another site carries the cookie too, SameSite=Lax letting it through on a
   top-level navigation, and on those targets that link would be the whole of
   the credential. Everywhere else, the pages, a session is enough.

   A session that answers is moved on, as a request through the REST gate moves
   it. */
bool sessionAdmits(const Ask &a);

/* Whether the Authorization header carries the Basic pair of the one account,
   checked against the stored form.

   A pair that was right is remembered, as a keyed digest and never as it stands,
   for as long as the name and the stored form stay the same, so a script sending it with
   every request pays for one derivation. A pair not remembered goes through the
   gate the REST login goes through, wrong answers counted against the address. */
bool basicAdmits(const Ask &a);

/* Closes the session the Cookie header names, and whether there was one. */
bool closeSessionOf(const std::string &cookie_header);

/* The Set-Cookie value that makes a browser forget the session cookie, with the
   attributes the REST logout sends. */
std::string forgetSessionCookie();

/* The account's name and whether a password is stored. */
std::string user();
bool passwordSet();

/* Whether the stored password is one these boxes have shipped with. Answered from
   a remembered result until the stored form changes, because the question costs a
   derivation per candidate and pages ask it on every load. */
bool passwordIsShipped();

/* A change to the account from the screen at the box or from a page.

   An empty name or an empty password leaves that half as it is: a screen cannot
   show a stored form, so it opens the field empty, and an untouched field must not
   take the password off the box. Written to nhttpd-access.conf and in effect for every
   login when this returns; sessions already open stay open, as they do when the
   REST route changes the password.

   from is the request that asked, for a change arriving over the old server, and
   NULL for the screen at the box. Such a change is taken only from a page of this
   box, by the rule sessionAdmits applies to the GET targets: the old routes that
   carry it take a GET, and with the page login switched off nothing else stands
   between another site's link and the account. A program changes the account
   through the REST route, with a sign-in.

   False, with why, when the request did not come from a page of this box, or the
   file would not take a value or could not be written; nothing is changed then.
   A password set here has to pass httpd::passwordAcceptable, whoever sets it;
   reset_to_shipped lets the shipped password through, for the menu at the
   box that puts the login back for an owner who lost theirs. */
bool change(const std::string &user, const std::string &password, std::string *why,
            const Ask *from = 0, bool reset_to_shipped = false);

} // namespace login_bridge

#endif /* __httpd_login_bridge_h__ */
