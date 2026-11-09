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

namespace httpd
{
class AccountLock;
}

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

/* Whether a request over the old server may change the account, and why not where it
   may not. The old configuration route asks this first, before anything of a request
   naming the account is written, so a request refused here changes nothing at all.
   It may when all of these hold:
   - it comes from a page of this box, the rule change() holds it to as well;
   - it carries the Basic pair of the account (basicAdmits) or a session
     (sessionAdmits), or it comes from the box itself: a peer on the loopback while
     no proxy is configured in front of this server;
   - the box does not wait for a password of the owner's own (passwordOwed), which
     PUT /api/v1/account/password sets against the current one;
   - user and password, where given, pass httpd::usernameAcceptable and
     httpd::passwordAcceptable, the checks the file and change() hold them to.

   With the page login switched off nothing in front of the old routes asks who the
   caller is, and the account is what opens the REST layer, so the headers saying that
   a request came from a page of this box are not enough on their own: any program can
   send them. The loopback is, because root on the box can write nhttpd-access.conf
   directly. Behind a proxy on the box every request arrives from the loopback, and a
   configured proxy is the only sign of one, so with one configured the loopback counts
   for nothing. An address the old server lets in without a login
   (mod_auth.no_auth_client) is none of these.

   admitted, when it is not null, is left carrying what the request was found to be,
   for the transaction below to look at again under the account's lock. */
struct Admitted
{
	/* The account the request was checked against (httpd::accountRevision), read
	   before anything was checked against it, so a change landing during the check
	   reads as one landing after it. */
	unsigned long long revision;

	/* The session that let the request in, and empty where the Basic pair or the box
	   itself did: the session a change keeps signed in. */
	std::string session;

	// Whether a password still owed refuses the change, as it does over the old server.
	bool owed_refuses;

	Admitted() : revision(0), owed_refuses(false) {}
};

bool mayChange(const Ask &a, const std::string &user, const std::string &password, std::string *why,
               Admitted *admitted = 0);

/* One change to the account as one transaction: the account's lock (httpd::AccountLock)
   held from a second look at what a check before it found to the last write, so nothing
   changes the account, or ends the session that asked, in between, and nothing is
   written for a request that no longer stands. The check itself (mayChange, or a
   route's own look at the password in effect) runs before this and outside the lock,
   being a key derivation.

   admits takes that second look: the account in effect is the one the request was
   checked against, the session that let it in is still live, and, where the check
   refuses a password still owed, none is owed. False, with why, and change refuses
   from then on. change is the change below, under the lock this holds, and keeps the
   session admits found signed in, under new tokens (httpd::reissueSession) where the
   change made a new account: session() is then the cookie token the caller is to be
   handed, the old one naming nothing any more, and it is the one admits found where
   nothing changed. Asked without admits it keeps nobody, which is the screen at the
   box, and session() is empty, as it is where no new token could be drawn.

   A writer of more than the account holds this across that write as well and makes it
   only once the account went through: the old configuration route writes the other
   keys of nhttpd.conf after it, taking httpd::ServerFileLock inside this one.

   Lives on the stack or in a pointer the caller deletes, and gives the lock up when it
   goes. Nothing here throws; a lock that could not be set up refuses everything. */
class Transaction
{
public:
	Transaction();
	~Transaction();

	bool admits(const Admitted &a, std::string *why);
	bool change(const std::string &user, const std::string &password, std::string *why,
	            const Ask *from = 0);
	const std::string &session() const;

private:
	Transaction(const Transaction &);
	Transaction &operator=(const Transaction &);

	httpd::AccountLock *held_;
	std::string         keep_;
	bool                refused_;
};

/* The Set-Cookie value that hands a session's token out, for the old configuration
   route to send the one Transaction::session names. */
std::string sessionCookie(const std::string &token);

/* Closes the session the Cookie header names, and whether there was one. */
bool closeSessionOf(const std::string &cookie_header);

/* The Set-Cookie value that makes a browser forget the session cookie, with the
   attributes the REST logout sends. */
std::string forgetSessionCookie();

/* The account's name and whether a password is stored. */
std::string user();
bool passwordSet();

/* Whether the box still waits for a password of the owner's own: the stored one
   is one these boxes have shipped with, or the first password the box drew for
   itself (httpd::drawFirstPassword). Every place that asks the first login to
   change the password asks this. The shipped half is answered from a remembered
   result until the stored form changes, because it costs a derivation per
   candidate and pages ask it on every load. */
bool passwordOwed();

/* Puts a first password in place of a stored one these boxes shipped with, so
   that no box goes on opening to a login anybody can look up. Run once at the
   start, between reading the configuration and starting the server, so no
   request is answered in between. The name stays as it is.

   True when there was nothing to replace or it was replaced. False, with why,
   when a shipped password stands and could not be replaced (the file refused,
   the random source failed): it then stays in effect, and the screen at the box
   says so. */
bool replaceShippedPassword(std::string *why);

/* The way back in from the menu at the box, for an owner who no longer knows
   the password: user root and a new first password, which the screen shows and
   the first login asks to replace. first is handed the password to show and is
   empty on a refusal, which leaves the login as it was. Every session ends with it,
   and every token drawn from one: whoever got in with the password that was lost is
   out again. */
bool resetToFirstPassword(std::string *first, std::string *why);

/* A change to the account from the screen at the box.

   An empty name or an empty password leaves that half as it is: a screen cannot
   show a stored form, so it opens the field empty, and an untouched field must not
   take the password off the box. Written to nhttpd-access.conf and in effect for every
   login when this returns.

   A new name or a new password ends every session opened before it, and every token
   drawn from one, wherever it was opened: whoever else held the account is signed
   out. This keeps nobody signed in, which is right for the screen at the box; a caller
   over the old server or the REST layer changes the account through Transaction,
   whose admits names the session that asked, and that one stays signed in under the
   new account, its cookie and its second token as they were. What it drew before ends
   with the rest. A change that changes nothing ends nothing.

   A change arriving over the network goes through Transaction, whose change takes the
   request that asked (from) and refuses one that did not come from a page of this
   box, by the rule sessionAdmits applies to the GET targets: the old routes that
   carry it take a GET, and with the page login switched off nothing else stands
   between another site's link and the account. This one takes no request, so nothing
   asking over a network reaches it without the second look.

   False, with why, when the file would not take a value or could not be written;
   nothing is changed then.
   A password set here has to pass httpd::passwordAcceptable, whoever sets it.
   A first password standing goes with any password set here and stays with a
   name changed alone. */
bool change(const std::string &user, const std::string &password, std::string *why);

} // namespace login_bridge

#endif /* __httpd_login_bridge_h__ */
