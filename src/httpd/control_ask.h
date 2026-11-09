/*
 * control_ask.h - the box asks before a browser runs a command of the old interface
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The old interface changes the box with a GET: /control/, /cgi-bin/, /fb/,
 * /api/v4/, /y/, and any request carrying a template command
 * (login_bridge::needsOwnPage). While the page login is off, and for the
 * addresses it lets in without a login, nothing stands in front of it, so a
 * link, an image or a form on any site the browser shows can switch the box to
 * standby. A request from a browser that shows nothing saying it came from a
 * page of this box is therefore answered with a page that asks, in place of the
 * command. The command runs once somebody says so on that page: a POST of its
 * form carrying a token the box handed out with the page, for this command and
 * for five minutes. A page of another site cannot read that page, so it cannot
 * send the token.
 *
 * The owner can let such commands run without asking (control_without_asking
 * in the access file, beside trusted_proxies and allowed_hosts): with it on, the
 * old interface answers as it did before.
 *
 * Read from src/nhttpd, which is built without exceptions: every call nhttpd
 * makes here catches whatever the layer under it throws and answers the
 * refusing value (test/unit/scan/check-exception-boundary.sh).
 */

#ifndef __httpd_control_ask_h__
#define __httpd_control_ask_h__

#include "login_bridge.h"

#include <map>
#include <string>

#include <time.h>

namespace httpd
{

/* The fields the page's form sends: the token, and the box ticked to run such
   commands without asking from now on, as "true". The token's field holds the word
   token, which a log line leaves out the value of. No command takes either name, and
   the box hands out no page for a request that names one. */
extern const char kAskTokenField[];
extern const char kAskRememberField[];

/* How long a token stands after the page that carries it was handed out. */
const time_t kAskSeconds = 300;

/* Whether the box answers this request with the page that asks instead of running
   it: a request from a browser (login_bridge::fromBrowser) for a target that needs a
   page of this box (login_bridge::needsOwnPage), showing nothing saying it came from
   one (login_bridge::fromThisBox), while control_without_asking is off. True where
   the answer cannot be had, so a failure here runs nothing. */
bool asksBeforeRunning(const login_bridge::Ask &a);

/* The page that asks, for a browser that sent method ("GET") for target, the request
   target as written ("/control/standby?on"), whose decoded path is path and whose
   query as written is query, carrying params as nhttpd's reader parsed them.

   It names the command in plain words where it knows it, and shows the whole
   address, path and query, the value of a parameter whose name says it is a secret
   left out, of a value that is an address its user, password and query values, and
   every character that shows nothing or turns the text around it. In the language
   the pages are shown in (webLanguage): German for de, English for every other. A
   new token is handed out with it, held with the target, the method and params: the
   request that runs once somebody says so, never a body. The caller hands out no
   page for a request naming a field of the form or changing the account.

   Empty where the page could not be built, a token that could not be drawn among it;
   nothing is held then. */
std::string askingPage(const std::string &target, const std::string &method, const std::string &path,
                       const std::string &query, const std::map<std::string, std::string> &params);

/* The Content-Security-Policy the page goes out with: its own style and script, its
   form and its fetch to this box, a picture the box answers with, and no frame of any
   page around it. */
std::string askingPagePolicy();

/* Whether a POST of the page's form may run the command it asked about: it comes from
   a page of this box (login_bridge::fromThisBox), and token is one handed out with the
   page for target, unused and not older than kAskSeconds. The token is gone afterwards
   whatever the answer. Where it may, params and method are those of the request the
   page was handed out for, so what runs is that request and the command sees neither
   field of the form, and with remember the switch is turned on: from now on such a
   command runs without asking. remember counts only where that request did not name
   the field itself: the form posts back to the address it was asked for, so a field
   in that address reaches the confirmation as if the box had been ticked. A switch
   that could not be written leaves the command to run and asks again next time.

   False, with why, where it may not, and nothing is written then. */
bool confirmedRun(const login_bridge::Ask &a, const std::string &target, const std::string &token,
                  bool remember, std::map<std::string, std::string> *params, std::string *method,
                  std::string *why);

/* What a refused confirmation is answered with: plain text in the language of the
   pages, saying the command did not run and how to be asked anew. */
std::string askRefusedText();

/* Whether control_without_asking is on: the line of the access file in use
   (configPath), the file looked at whenever this is asked and read again when it
   changed. False where the file is gone or cannot be read, and where the answer
   cannot be had: the box asks. What config() holds where no file is in use. */
bool controlWithoutAsking();

/* Turns the switch on or off in the access file in use, under AccountLock. False,
   with why, where it could not be written. */
bool setControlWithoutAsking(bool on, std::string *why);

/* What the token clock reads, in seconds of a monotonic clock; nought gives the
   real one back. */
void setAskClockForTest(time_t now);

/* Drops every token handed out. */
void forgetAskTokensForTest();

} // namespace httpd

#endif /* __httpd_control_ask_h__ */
