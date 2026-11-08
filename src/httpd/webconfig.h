/*
 * webconfig.h - the webserver own configuration file
 *
 * Copyright (C) 2026 NI-Team
 * Copyright (C) 2026 Tuxbox-Project (additions, rework, fixes)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __httpd_webconfig_h__
#define __httpd_webconfig_h__

#include "netmatch.h"
#include "server.h"

#include <map>
#include <string>
#include <vector>

namespace httpd
{

/* Everything the server is told before it answers anything: where it listens,
   who may reach it, and on what showing.

   Named for what it holds and not config, which is what configure calls the
   header it generates. Two headers of one name in one include path is settled
   by the order of the flags, and a file here asking for the generated one and
   being handed this one compiles perfectly and carries none of the feature
   macros. That went unnoticed once already.

   The listening half is the ServerConfig beside this and not a port and an
   address of its own: two structures describing one socket is how a file saying
   one port ends up in front of a daemon started from a default saying another.
   Starting the server is start(config().server).

   That half is read out of nhttpd.conf and not out of this server's own file:
   the port and the address are nhttpd's, which binds them, and the menu at the
   box sets them there. Everything else here is the access file's
   (HTTPD_ACCESS_FILE).

   An aggregate with no constructor, filled by defaultWebConfig below, so a value
   nobody filled in has no password anything verifies against and no network
   anything is admitted from. */
struct WebConfig
{
	ServerConfig server;

	/* Who may present a password. Empty is not a name anything answers to, and
	   this file makes that true rather than assuming it: whenever the name ends
	   up empty the password below is emptied with it. Without that an empty name
	   matching an empty header field is a way in. */
	std::string  username;

	/* The stored form credentials.h writes, and empty when the box has no
	   password. Empty verifies against nothing, so a box that has not been given
	   one answers no request that needs one. */
	std::string  password_hash;

	/* Where a caller may read without presenting anything. Empty admits nobody. A
	   box whose file says nothing about it is seeded with defaultLanPrefixes,
	   because the alternative is a fresh box on which the local network cannot
	   read anything; a box whose file says something this cannot use gets the
	   empty list instead, for the reason under load below. */
	std::vector<NetPrefix> lan_read;

	/* Whose forwarded-for header is believed. Empty by default and empty for a
	   box nobody configured, because believing that header from an arbitrary peer
	   lets any caller name the address it is judged on. */
	std::vector<NetPrefix> trusted_proxies;

	/* Whether the file named a proxy list at all, which is a different question
	   from whether the list has anything in it.

	   An empty list believes nobody. Read alone it also describes a box with
	   nothing in front of it, and that is the box on which reading without a
	   password from the private ranges is allowed. So a line naming a proxy that
	   this could not read would, if only the list were looked at, turn that
	   exemption back ON. proxiesConfigured below is the reading everything else
	   uses. */
	bool trusted_proxies_named;

	/* Where the pages are served from, and empty means none are. Named by default
	   to this server's own directory (defaultWebConfig, PRIVATE_HTTPDDIR in
	   webconfig.cpp); a file this cannot read at that key falls back to empty and
	   serves nothing, for the reason under load below. */
	std::string  docroot;

	/* How long a session lasts without a request. Every request it carries starts
	   this again, up to the ceiling auth.h states for openSession. */
	unsigned     session_lifetime_s;
	unsigned     sse_max_streams;

	/* Whether the answers shaped like the old server's are offered. On, because
	   this box has nothing else to hand the plugins, the second box syncing timers
	   over /control/ and every third party the counter at
	   /api/v1/system/legacy-usage has not shown to have moved on. Answering does
	   not widen what a caller may do here, which is levels.h's own table; it only
	   says the table is asked at all.

	   Leaving the surface out of the program entirely is --disable-legacy-api
	   (configure.ac): a build decision this key cannot make. This key is the one a
	   running box already carrying the surface can turn off later without a
	   rebuild. */
	bool         legacy_enabled;

	/* Whether the pages this server draws show a picture beside a channel. On.

	   HERE AND NOT IN neutrino.conf. What it decides is what a browser asks this
	   server for; the television is drawn by the program itself out of its own
	   settings, and a switch that lived there would turn the logos off on the
	   screen in the room as well. A viewer on a telephone over a slow link is the
	   reason this one exists.

	   THE ROUTE THAT HANDS A PICTURE OVER READS THIS AND REFUSES. It was a matter
	   of drawing at first, read by the page and acted on there, and that held only
	   for whoever could read it: the route that answers how this server is set up
	   is System, so the reader on the home network who presented nothing never
	   learnt it and went on drawing the pictures. So the refusal is at the route
	   (GET /api/v1/channels/{id}/logo), where it holds for every caller. */
	bool         channel_logos;
};

// The values a box that has never been configured runs on.
WebConfig defaultWebConfig();

/* Whether something in front of this server is configured, which is the switch
   that turns the read exemption off.

   One reading in one place, because the two halves live in two members and
   asking either alone is how the wide answer was reached once already: a list
   that is empty because nobody wrote one and a list that is empty because the
   line naming one could not be read are the same list and not the same box. */
bool proxiesConfigured(const WebConfig &c);

/* Whether peer is loopback: an address in 127.0.0.0/8, also written as an IPv4
   address mapped into IPv6, or ::1. Judged against those two networks and never
   against lan_read, which is a line an operator can edit and which grants Read and
   no more. trusted_proxies is not read here either: behind a proxy on the box every
   request arrives from the loopback, so a caller that lets the loopback stand for
   the box asks proxiesConfigured as well, as login_bridge::mayChange does. */
bool isLoopbackPeer(const std::string &peer);

/* What is in effect. Installed by load or by setConfigForTest and read everywhere
   else.

   Read while the server answers requests and written before it starts, which is
   the whole of the arrangement: nothing reloads this under a running daemon, so
   the reference stays valid and the read needs no lock. Whatever adds a reload
   has to stop the daemon around it or replace this with something a request
   thread can hold. */
const WebConfig &config();


/* The password the images carry for the old server (configure:
   WEB_DEFAULT_PASSWORD), in the nhttpd.conf they install and as that server's
   value for a file naming none. No access file is written with it: where a box
   would have started on it, it starts on a first password (below). */
extern const char kShippedPassword[];

/* The passwords boxes of this line have shipped with, the one above among them,
   and whether a password is one of them. A login using one is asked for a
   password of its own. */
const char *const *shippedPasswords(size_t *count);
bool isShippedPassword(const std::string &secret);

/* A first password: what a box starts on instead of one of the above, drawn by
   the box for itself, so no two boxes open to the same login and none opens to
   one anybody can look up. The screen at the box shows it and the first login
   asks to replace it, as it asks to replace a shipped one.

   Ten characters out of an alphabet without the ones a person misreads off a
   television (no i, l, o, 0 or 1), each drawn out of the system's random source
   and none of them bent towards any letter. Empty when that source could not be
   read, and then nothing is written with it. */
const size_t kFirstPasswordChars = 10;
std::string drawFirstPassword();

// The fewest characters a password set for the account has.
const size_t kMinPasswordChars = 8;

/* Whether a password may be set for the account: at least kMinPasswordChars
   characters (counted as UTF-8 code points, as a person counts them), no
   control characters, and none of the shipped ones. why says what is wrong.
   Every writer of the account's password asks this, and a first password
   passes it. */
bool passwordAcceptable(const std::string &password, std::string *why);

/* Whether a name may be the account's: not empty, and nothing in it that the file
   would not read back as written, which is a control byte, a number sign or a space
   at one of its ends. why says what is wrong. saveWebSettings writes no other name. */
bool usernameAcceptable(const std::string &name, std::string *why);

/* Reads path and installs what it holds.

   False when the file cannot be read at all, and then nothing is installed: a
   reload of a file somebody deleted does not empty the policy.

   True when the file was read, whatever it did not hold and whatever in it could
   not be used. Every unusable value is named in configProblems below.

   THE RULE FOR A VALUE THAT CANNOT BE USED, and the one to keep when a key is
   added here: a key the file does not name falls back to the default, and a key
   the file names but whose value cannot be recovered falls back to the LEAST the
   key can mean. What was written is unknown, so anything wider than the least is
   granting more than the file asked for. The line that made this rule was

       lan_read=10.9.0.0/16 # only our own net

   which the parser cuts at the number sign. Falling back to the default there
   would seed six private networks including both halves of loopback, so an
   operator narrowing their own exemption by hand would have widened it.

   THE LEAST A KEY CAN MEAN IS NOT THE SMALLEST VALUE IT CAN HOLD. It is the
   narrowest state of the box, and the two part company wherever a value is a
   switch as well as a quantity. That is trusted_proxies below: the empty list is
   the smallest list there is and it is also the state in which reading without a
   password is allowed, so what stands in for an unreadable proxy line is a list
   that admits nobody AND the memory that the key was named.

   What each key does when it is named and cannot be used:

     lan_read            the empty list, which admits nobody
     trusted_proxies     the empty list, which believes nobody, and the read
                         exemption stays off because the key was named
     username            empty, which takes password_hash with it
     password_hash       empty, which verifies against nothing
     docroot             empty, which serves nothing
     session_lifetime_s  the shortest a session may be
     sse_max_streams     none
     legacy_enabled      off
     channel_logos       off, which is the least a switch that shows something
                         can mean
     api_tokens          none, which is also what the key absent means

   port and bind_address are not keys of this file any more. Where the server
   answers is read out of server_path, nhttpd.conf on a box, the way nhttpd
   reads it when it starts: its own defaults, and an empty address meaning every
   interface. nhttpd binds what that file says whatever this makes of it, so a
   rule of this file's own there would only tell a caller something nhttpd is
   not doing. A line naming either key here is said and has no effect. An empty
   server_path leaves nhttpd's defaults in effect; a file at it that cannot be
   read does the same and says so.

   api_tokens is the one key whose value does not end up in the structure above. A
   token is a credential, so load hands it to the table a bearer header is
   answered out of (auth.h), and the table is emptied first so reading the file
   twice leaves the tokens it names rather than two of each. It is also the one
   key where the least it can mean and the default are the same thing, so its two
   failing readings are told apart only by what is said.

   THAT KEY CARRIES THE TOKENS THEMSELVES AND NOT A STORED FORM OF THEM, unlike
   password_hash beside it: nothing in this product writes a stored form out, so
   a key that demanded one would be a key nobody could write.

   It is also the one list here that forgives a space at either end of an entry,
   where the two network lists drop one. An entry there is a word out of a closed
   set and a run of hexadecimal, so a space beside either cannot make it mean
   something else.

   legacy_enabled is the third: its default is on. A line naming the key that this
   cannot read is still a line naming it, the same as trusted_proxies above, so
   what stands in for it is off and not the default.

   WHAT COUNTS AS NAMED. The parser takes the name to be every byte in front of
   the first separator, untrimmed and case sensitive, so both

       lan_read = 10.9.0.0/16
       LAN_READ=10.9.0.0/16

   hand their value over under a name nothing here asks for. Read literally those
   keys are absent, which is the default, which is the wide answer the rule above
   exists to refuse, reached by the most ordinary things a person does when
   writing a file. So a line whose name can be taken for one of these keys,
   ignoring space around it and ignoring case, counts as that key being named and
   unreadable.

   Taken for and not read as. The value is not used under the corrected name
   either: what the name settles is which key was meant, and therefore which
   least value stands in its place.

   A line naming something that can be taken for no key here is said and no more:
   refusing the file for one would make a working configuration unusable after an
   upgrade. */
bool load(const std::string &path, const std::string &server_path = std::string());

/* The file the last load was asked for, and empty before anything asked for one.

   Here because whatever writes this configuration has to write the file the
   running server was told to read, and the program names that file once, on the
   way up. A writer that reached for that name itself would be right on a box and
   wrong in every case, each of which runs against a file of its own.

   Remembered whatever the read came to, because what this answers is where the
   box keeps the file and not what was in it.

   Read while the server answers requests, like the configuration itself, and
   written before it starts. */
const std::string &configPath();

/* The file the last load read where the server answers out of, nhttpd.conf on a
   box, and empty where it was handed none. A save writes the port and the address
   there, and only there. */
const std::string &serverPath();

/* Installs a whole configuration, so a case can put a policy in front of the code
   that reads one without a file to read. The files the last load read are
   forgotten with it, there being none behind this one: a save after it writes no
   file a case before it left named. Nothing in the product calls it. */
void setConfigForTest(const WebConfig &c);

/* The account every login checks against: the name and the stored form of its
   password.

   Apart from config() because it changes under a running server. The screen at the
   box, the old pages and the settings page change it without restarting anything
   (login_bridge.h), and config() may only be replaced while the daemon is stopped.
   So this is a copy under a lock: installed together with everything else by load
   and setConfigForTest, and on its own by publishLogin once a save wrote a new one.
   Every reader of the account reads it here and not out of config(). */
struct Login
{
	std::string username;
	std::string password_hash;
};

Login loginInEffect();

/* The first password in effect, in plain: what the screen at the box shows when
   asked, and empty unless the stored form in effect is that password. The file
   holds it the same way, on a line of its own beside the stored form that goes
   with the first change.

   Not a member of Login, which every login check copies: this is the one
   password the program holds as it stands, and it is held once. Published
   together with the account by publishLogin, which drops it unless it is handed
   one, under the same lock; a replaced one is wiped and not merely freed. */
std::string firstPasswordInEffect();
bool firstPasswordStands();
void publishLogin(const Login &l, const std::string &first_password = std::string());

/* Whether this run of the program wrote a first password into the file the
   server reads: on a start that found no file, or one holding a password these
   boxes shipped with, or at the menu of the box. Which is when the screen at the
   box shows it without being asked. */
bool firstPasswordWrittenThisRun();

/* Held by load for the whole of a read, by a change to the account at the box
   (login_bridge::change) from reading what is in effect to publishing what it
   wrote, and by the REST route that writes the same file (PUT
   /api/v1/system/webserver) from its reading to its answer. A change runs on a
   thread that is not stopped while load replaces what is in effect, so without
   this they could interleave: a change reading a configuration being replaced,
   publishing an account older than the file a reload just installed, or two
   saves each rewriting the file the other one read. */
class AccountLock
{
public:
	AccountLock();
	~AccountLock();

private:
	AccountLock(const AccountLock &);
	AccountLock &operator=(const AccountLock &);
};

/* What the last load or migration dropped, one line each, and also written to the
   standard error as it happens.

   A value that cannot be used is worth more than the absence of it: the
   difference between a file that says nothing about who may read and one whose
   only line about it was mistyped is the difference between a box behaving as
   shipped and a box nobody can reach.

   No line here carries a value read out of either file except where the key's own
   meaning makes the value harmless to show, and no line carries a raw line of a
   file at all. A stored form is what an offline attack is mounted against, this
   list is public, and both halves of it reach a terminal. What is shown is
   escaped and cut short.

   No line names a password. The migration says that the old file held one of
   the passwords these boxes shipped with without saying which (migrateFrom), and
   a first password is in the access file and on the screen of the box and
   nowhere else.

   Bounded. A file within the size this reads can hold a quarter of a million
   unusable lines. When the ceiling is reached one last line says the rest were
   not listed.

   A copy, taken under the list's own lock: lines reach it from threads that hold
   the account's lock, from writers of nhttpd.conf that hold only theirs, and from
   reads that hold none. */
std::vector<std::string> configProblems();

/* What nhttpd.conf holds for the web server besides where it answers: the lines
   the settings page offers, read the way nhttpd reads them and put in effect when
   nhttpd starts. This layer acts on none of them. It reads and writes them so that
   the page, the menu at the box and nhttpd agree on one file.

   Constructed as nhttpd's own defaults, the values it runs on where its file names
   none (yhttpd.cpp ReadConfig, mod_auth, mod_sendfile). */
struct ServerOptions
{
	ServerOptions();

	bool        page_login;          // mod_auth.authenticate: the pages ask for the login
	std::string no_login_prefix;     // mod_auth.no_auth_client: callers whose address
	                                 // begins with this are never asked, and empty for none
	bool        threading;           // webserver.threading
	std::string override_directory;  // WebsiteMain.override_directory: a file here is
	                                 // served in place of the pages' own of that name
	std::string hosted_directory;    // WebsiteMain.hosted_directory
	bool        send_all_types;      // mod_sendfile.sendAll: a file of a type nhttpd has
	                                 // no name for is sent all the same
};

bool sameServerOptions(const ServerOptions &a, const ServerOptions &b);

/* Held by every writer of nhttpd.conf in this program from its read to its write:
   the save below, the restart that puts a port back, the menu at the box
   (gui/nhttpd_setup.cpp), the old config route (controlapi.cpp) and nhttpd
   reading its file when it starts or is told to read it again (yhttpd.cpp,
   ReadConfig), which with its hooks writes defaults back.
   Each of them rewrites the whole file, so two at once would have the second
   write back what the first just changed.

   Taken after AccountLock where both are held, never before it: a writer that
   changes the account as well releases this one first. */
class ServerFileLock
{
public:
	ServerFileLock();
	~ServerFileLock();

private:
	ServerFileLock(const ServerFileLock &);
	ServerFileLock &operator=(const ServerFileLock &);
};

/* A whole nhttpd.conf written out of its lines, the way nhttpd's own writer lays
   them out, one key and its value to a line in the order of the map. Unlike that
   writer, every write is checked and the content committed before the new file
   takes the place of the old one, so a full disk leaves the old file whole rather
   than a short one in its place. The file keeps its mode; one that is not there
   yet gets the mode nhttpd's writer gives it.

   For every writer of that file, each holding ServerFileLock. False, with the old
   file as it was, where anything before the replacement failed. */
bool saveServerFile(const std::string &path, const std::map<std::string, std::string> &lines);

/* What nhttpd.conf (serverPath) says about the above now, and nhttpd's defaults
   where it says nothing, where there is no such file, or where none was named. */
ServerOptions currentServerOptions();

/* THE LANGUAGE THE PAGES ARE SHOWN IN.

   nhttpd.conf keeps it under Language.selected as the name of a file in the pages'
   language directory (English, Deutsch), which is what the old pages and the
   server's own texts read. A page is told and tells it as the code of that
   language (en, de), the name a browser and a translation go by; the table behind
   these calls maps one onto the other. */
struct WebLanguage
{
	std::string code; // en, de, ...
	std::string name; // what the language calls itself, for a menu
};

/* The languages the pages can be shown in, in the order of the table: those whose
   texts the server hands out as /languages/<code>.json, from the override
   directory or the pages' own. A language with only the old pages' file is left
   out, as the interface would show English throughout. */
std::vector<WebLanguage> webLanguages();

/* The code of the language nhttpd.conf names, and "en" where it names one this
   server knows no code for, which the pages show in English. */
std::string webLanguage();

/* Writes the language of a code webLanguages() offers into nhttpd.conf, holding
   ServerFileLock. False where the code is none it offers or the file could not be
   written, the file then left as it was. The caller has nhttpd read the file
   again (reloadNhttpdConfig in server_control.h) after it released the lock. */
bool setWebLanguage(const std::string &code);

/* The settings somebody changes at the box itself, which are fewer than the file
   holds and fewer than the structure at the top of this header.

   What is here is what a screen driven with four arrow keys and a number pad can
   ask for: where the server answers, on which interface, and under what name and
   password. A list of networks, a bearer token and the lifetime of a session are
   settings too, and not one of them is a thing anybody types on a remote control.

   Not a WebConfig, and that is the whole of the difference. A screen handed one
   of those would write back eleven members having been asked about four, so every
   key the file names and the screen does not offer would come back out of
   whatever this program's default happened to be on the day. The save below
   rewrites the lines it owns and leaves the rest alone. */
struct WebSettings
{
	/* Where the server answers, which the save below writes into nhttpd.conf
	   (serverPath) and not into the access file: nhttpd binds them, and the menu at
	   the box sets them in that file too. */
	int         port;
	std::string bind_address;
	std::string username;

	/* The password as somebody types it. Hashed by the save below and never
	   written as it stands, and read only when change_password says a new one was
	   typed.

	   That switch is here because a stored form cannot be turned back into the
	   password it was made from, so a screen has nothing to put in the field and
	   opens it empty. An empty field left alone therefore has to mean the stored
	   form stays, or every save by somebody who came to change the port would take
	   the password off the box.

	   Which leaves no way to take a password off deliberately, and that is the
	   direction to be wrong in: a box without one answers nothing that changes
	   anything, from anywhere. */
	std::string password;
	bool        change_password;

	/* Whether the password above is a first password (drawFirstPassword), which
	   the file then keeps in plain beside its stored form until the next one is
	   set. Read only together with change_password: any other password typed
	   takes that line out of the file, and a save typing none leaves it as it
	   stands, so a name changed alone does not let the box off the change it
	   still asks for. */
	bool        first_password = false;

	/* Whether the pages show a picture beside a channel. The one value here that
	   does not decide where the server answers or who may tell it anything, which
	   is why it is the one a caller can move without being locked out by getting
	   it wrong. */
	bool        channel_logos;

	/* The rest of what nhttpd.conf holds for the server, written there beside the
	   port and the address and only where it differs from what that file says. A
	   writer starts from currentWebSettings, which reads them out of the file; one
	   that starts from nothing writes nhttpd's defaults. */
	ServerOptions options;

	/* Whether the save rewrites the name and the password and nothing else, which
	   is what a change to the account from the box or the old pages is. The port,
	   the address, the options and the logo switch are then left as they stand
	   and not even judged, so such a change made while a REST save waits for its
	   restart cannot write the values in effect back over the ones that save just
	   wrote. */
	bool        account_only = false;
};

// The above as they stand in what is in effect, which is what a screen opens on.
// The screen at the box offers the account; the logo switch is a line the route
// writes and that screen leaves where it found it. Between a save into the file the
// server reads and the load that puts it in effect, what that save wrote: a writer
// in that window starts from it and does not write back what it replaced. The port
// and the address are what nhttpd.conf says at the moment of asking, whatever is in
// effect, because the menu at the box writes them there without passing through
// this layer, and a writer starting from anything older would move them back.
WebSettings currentWebSettings();

/* What is in effect, whatever a save since wrote: what the consequences of a
   change are measured against, a caller asking what a restart will do to it. */
WebSettings settingsInEffect();

/* What saves into the file the server reads still wait for the load that puts
   them in effect: whether one of them carried a new password, and whether a
   restart that reads the file was armed for them (armRestart, called by the route
   that arms one). Both clear when a load reads the file back; the second also
   when an armed restart ran and its read failed, the saves still waiting. */
bool newPasswordAwaitsLoad();
bool restartAwaitsLoad();
void armRestart();

/* Writes the values above: the name, a new password's stored form and the logo
   switch into path, leaving every other line of it where it stands, comments
   included; the port, the address and the options into nhttpd.conf (serverPath),
   each only where it differs from what that file says. A rewrite of the lines this
   owns and not a file written out of a structure, for the reason WebSettings gives.

   WHAT COUNTS AS A LINE NAMING ONE OF THOSE KEYS IS THE QUESTION LOAD ASKS AND
   NOT AN EXACT MATCH. A line spelled Username, a line with a space in front of
   the separator and a line carrying a number sign anywhere are all lines load
   reads as that key being named and unusable, and one of them left standing
   beside the line written here would leave the file naming one account and the
   server checking against none. So they are taken out, and so is a second line
   naming a key already written, a file naming one key twice being read as
   whichever line came last. A line naming port or bind_address, which this file
   no longer holds, is taken out too.

   False and nothing written when a value cannot be used: a port that changes to
   one outside 1 to 65535, an address that changes to one nhttpd cannot listen on
   as written (anything but a literal of the first family), a prefix that changes
   to something no address begins with, a directory that changes to anything but
   an absolute path without a step up, any of these changing on a server never
   told which file holds them, an empty user name,
   a name carrying a byte that does not read back as it was written, a password
   that could not be hashed, and a path that names a link rather than a file. The
   link is refused because what replaces the file is put in place by name, which
   would replace the link and not what it points at.

   Nothing is half written. The new content goes to a name of its own, is
   committed there, and takes the place of path in one step. What this cannot
   promise, where writing a file that was not there can, is that nothing happened
   when it answers false: once the new file is in place there is no old one to go
   back to, so a failure after that point is said and answered true.

   nhttpd.conf is written first, under ServerFileLock, as nhttpd writes it and with
   every write checked before the new file takes the old one's place, and put back
   when path then cannot be written, so a save answered false has not moved where
   the next start answers.

   from is what the caller started from, currentWebSettings as it read it. Given,
   only what s changes against it is written into nhttpd.conf, so a value the menu
   at the box wrote there since the caller read it stays, unless the caller changed
   that same value. Without it, whatever s says that differs from the file is
   written.

   The file that replaces path is readable and writable by its owner and by nobody
   else, whatever the mode of the file it replaces.

   A box with no file at that name gets one, with the same head the migration
   writes. */
bool saveWebSettings(const std::string &path, const WebSettings &s,
                     std::string *stored_hash = NULL, const WebSettings *from = NULL);

/* Reads path and nhttpd.conf (serverPath) again and puts the running server on
   what they now say.

   The daemon is stopped across the read and not around it, which is the
   arrangement config() above describes: a request thread reads what is in effect
   without a lock. Stopping first is also the only way a new port becomes the port
   that is bound.

   False when the file could not be read, and then nothing was replaced and the
   server comes back up on what it was already running. False when nothing came up
   on a port or an address that changed: the server is then put back once on the
   port and the address it answered on before, and nhttpd.conf with it, so the
   next start does not repeat the failure. False when the server did not come
   back up at all, and then the box answers nowhere. Each is said rather than
   hidden. */
bool reloadAndRestart(const std::string &path);

/* Writes out_path from what nhttpd_conf holds. Carried across: the user name, and
   the password as the stored form credentials.h writes. Where the server answers
   is not carried: it stays in nhttpd_conf, which nhttpd goes on reading and which
   load reads it out of as well.

   THE PASSWORD IS CARRIED WHATEVER THE SWITCH SAID, AND REPLACED WHERE IT IS ONE
   THESE BOXES SHIPPED WITH.

   That file ships with the switch its own server checks passwords under turned
   off, and with a password under it that is also compiled into that server as the
   value it uses when the file names none, so on a box nobody edited the value is
   a constant anybody can read in the source. Carried, it would open the web
   interface of every upgraded box to the same login. So a shipped value is not
   carried: the file is written with a first password instead
   (drawFirstPassword), which the screen of the box shows, and the problem list
   says that the old file held one of the shipped passwords, without saying
   which. Any other value is carried as it stands, being one somebody chose.

   A file carrying no password at all is the one case that writes none, and the
   written file says so in its first lines.

   WHAT THE WRITTEN FILE LEADS WITH IS THE LOGIN, and under it where to change it
   and how to start over. The reader it is written for is whoever
   has just been refused a login and opened the file over a shell. Under that
   stands every key this reads, each with what it decides and with the default in
   force the moment the file was written, and only the account is written as
   lines: a default written out as a line freezes the defaults of the day into a
   box that will never see tomorrow's, and this format has no inert line to write
   one on, the parser reading anything that carries the separator as a setting.

   Everything else in that file belongs to the other server or to its page
   templates and is left there, and two things are left behind on purpose: the
   switch that turned authentication off, which is the one property this work
   exists to remove; and the page directory, which holds the other server's pages
   and not these.

   What is carried is what the old server was running on and not what its file was
   written with. Both read that file through the same parser, so a value whose
   line carries a number sign was already the shorter value everywhere the old
   server ever compared it, and carrying the longer one would change who can log
   in. Every such line is named.

   The old file is not touched, so a box rolled back to an older image finds it as
   it left it.

   False and nothing written when the source cannot be read, when a value in it
   cannot be carried without changing what it says, when the password cannot be
   hashed or a first password not drawn, or when out_path already exists. An
   existing file is a refusal and not an overwrite, because the file it would
   overwrite is the one holding the credential the box is running on.

   out_path is created with the mode it keeps and never a wider one. True means
   the file is there and will still be there after the power goes: the content is
   committed under a working name and then linked into place, and the directory
   entry naming it is committed before this answers, so there is no moment at
   which a partial credential is readable and no power cut leaves one behind. */
bool migrateFrom(const std::string &nhttpd_conf, const std::string &out_path);

/* Leaves the box with a configuration file at out_path, run on every start.

   THE ONE OUTCOME THIS RULES OUT IS A BOX WITH NO FILE AND NO LOGIN. The call
   above refuses in four places and the write under it in a fifth, and every one
   of them used to end at a box whose web interface had no credential at all: it
   answers reads from the private networks, refuses everything that changes
   anything, and says why only on a standard error nobody is reading. Two owners
   reached that state with a file naming a user and a password, and the first they
   knew of it was that they could not log in.

   So a name that is free gets a file, whatever happened to the migration. Where
   the old file was carried across, with its values. Where it was refused or there
   was nothing to carry, with user root and a first password the box draws for
   itself (drawFirstPassword), which its screen shows. The written file says which of the two it is, and says why in the words of the list
   above, because the owner looks in the file and not in a boot log.

   True only when this call wrote out_path.

   Four states, and a box is in exactly one of them:

     anything at out_path      nothing is written and nothing is said, which is
                               every start after the one that wrote it. A file
                               at former_path beside it is removed.

     a file at former_path     taken over whole under out_path and then removed:
     or a link to one          the name this file had before (HTTPD_ACCESS_FORMER_FILE).
                               A link is read through and then removed, what it
                               points at being left alone. Where it cannot be
                               taken over, or not read to its end, nothing
                               changes and the next start tries again.

     only the old file         the migration is attempted, and its refusal is
                               not the end: what it could not carry is written
                               into a file with a first password instead.

     nothing at either name    a file with a first password, which is what a box
                               nobody ever configured now gets on its first
                               start.

   Anything, and not a file: a directory, a symbolic link and a link pointing
   nowhere all count as something being at that name. This never writes over what
   is there and never follows a name somebody else can create, so the question is
   whether the name is taken and not what is behind it. A name taken that way is
   the one state this cannot rescue a box out of.

   False when the name was free and nothing could be written there either, which
   is a directory that is not there, not a directory, or not writable. Nothing is
   left behind and the next start tries again.

   Racing another writer for out_path is settled the same way the call above
   settles it: creation is exclusive. */
bool ensureConfigFile(const std::string &nhttpd_conf, const std::string &out_path,
		      const std::string &former_path = std::string());

} // namespace httpd

#endif
