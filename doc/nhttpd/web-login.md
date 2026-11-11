# Signing in to the web interface

## First sign-in

Boxes do not share a login. A new box makes up a first password of its own
the first time it starts: 10 characters, lower-case letters and digits,
leaving out `i`, `l`, `o`, `0` and `1`, which are easy to misread.

The TV shows it once, at that start, in a message with the address of the
web interface, the user name and the password. The message stays until you
press OK, for 5 minutes at most. It is not shown when the box starts into
standby or for a timer. To see it later, or if it was not shown, choose
**System settings → Network → Web server (nhttpd) → Show the first password**.
The sign-in page points there as well.

While the message with the password is on the TV, the box software takes
no screenshot: none with the remote control's key, none for the screenshot
page of the web interface, which says so, and none for remote tools (see
below). A screenshot that was being taken when the message came up is not
kept either. A GraphLCD front display that mirrors the TV's menus stays
dark meanwhile, and no picture of it is taken. A program that reads the
screen by itself, outside the box software, is not held back.

The user is `root`, unless the box already had another user name.

When the pages ask you to sign in depends on **Authentication** under
**System settings → Network → Web server (nhttpd)**:

- Off, as the box ships: the pages open without a sign-in, and from the
  home network they show what is on the box. Away from the home network,
  and at your first change, they ask you to sign in.
- On: the pages ask you to sign in first. Calls from the box itself, and
  from the address range under **No-auth clients**, open the pages without
  a sign-in, and are asked at their first change, as with Authentication
  off. Once a proxy is entered in `nhttpd-access.conf` (`trusted_proxies`,
  even an empty or unreadable line), or the file cannot be read, they are
  asked like everybody else: behind a proxy every request comes from the
  proxy's address.

Sign in with the user and the first password. Because that password is shown
on the TV, the first sign-in asks for a password of your own. Until you set
one, a signed-in browser can do nothing else:

- The pages send you to **Set your own password**. Enter the current
  password and the new one twice.
- The new password needs at least 8 characters and no control characters.
  It may not be the current one, and it may not be one of the passwords
  these boxes have shipped with.
- The new password is in effect at once. This browser stays signed in,
  with a new sign-in cookie: a copy of the old one no longer works.
  Every other browser and app signed in to the web interface is signed out
  and has to sign in again with the new password.

Then a new box shows its welcome page once: the language and the start
page of the web interface, **Get started** or **Skip**. It asks for the
sign-in and the password of your own first, whichever is still due, with
Authentication off as well, so nothing is asked twice. Get started, Skip and
its link to the web server's settings each end it for good; closing the
page without one of them leaves it to come again. A box whose web interface
settings came over from the former interface (`yWeb.conf`) does not show it.

To change the password or the user name later, in the settings of the web
server, enter the current password as well: the one you sign in with. A
wrong one counts like a wrong sign-in.

Until you set your own password, the box also keeps the first password in
`nhttpd-access.conf` in its configuration folder (usually
`/var/tuxbox/config`), readable by root only. Setting your own password
removes it from there.

### A box updated from an older version

- If its login still uses a password these boxes once shipped with, the
  box makes up a first password at the next start, before the web interface
  answers anybody, and shows it on the TV as above. The user name stays.
- A password of your own that the box already had stays as it is.
- If a box that keeps a shipped password in `nhttpd-access.conf` cannot
  store a first password, the old password stays in effect, and the TV says
  so after the start. The next sign-in over the web still asks for a
  password of your own.
- A box that has no `nhttpd-access.conf` yet and cannot write one, new or
  updated from a version without it, has no password that signs in until a
  later start can write the file.

## Names the box answers to

The web interface, `/control/` and `/api/v1` answer only a request that names
the box. These names work:

- an IP address, such as `http://192.168.1.5/`;
- a name without a dot, such as `localhost`, `h7` or a name your router
  gives the box (`wohnzimmer`);
- such a name with `.local`, or with a domain the box's name lookup searches
  (`search` or `domain` in `/etc/resolv.conf`), such as `h7.fritz.box` or
  `wohnzimmer.fritz.box`;
- the host name of the box, also where it carries a domain of its own;
- a name listed in `nhttpd-access.conf` in the line `allowed_hosts`,
  separated by commas, such as a DynDNS name or the name of a proxy in front
  of the box. Names only: a port after a name is left out, and the line
  takes no comment (a `#` makes the whole line unreadable).

Only your home network answers for the first kinds of name, so nobody
outside can make one of them lead to the box.

Any other name gets a short page in English and German that says how to
reach the box by its IP address, naming the address to a device on the home
network. To use the name, add it to `allowed_hosts`;
the box reads the line again within 10 seconds, without a restart. The web
interface shows the list under **Settings → Webserver**, but cannot change
it. A request without a `Host`
header, as some old scripts send it, is answered as before. A request that
names `Host` twice, the same name or not, gets the short page as well.

This keeps out a page of another site that has its own name lead to the
box's address: a browser would treat the box's answers as that page's own.

## Remote control apps and scripts

The old `/control/` interface asks for a login only while
**Authentication** is on under **System settings → Network → Web server
(nhttpd)**. The box ships with it off. Then `/control/` answers anybody who
reaches the box, without a password, whatever an app sends, except for a
change to the account, and a browser is asked first (see below). The user
name and the password change over
`/control/` only for a request from a page of this box
(`Sec-Fetch-Site: same-origin`, or an `Origin` or `Referer` naming the box)
that signs in with HTTP Basic authentication, or that comes from the box
itself while no proxy is configured in front of it, whether Authentication
is off or on. A browser signed in to the web interface changes them in the
settings of the web server there, which ask for the current password; over
`/control/` its sign-in alone is not enough. Any other request to change
them is refused as a whole, also from the address range under **No-auth
clients**, and so is every one while the box still waits for a password of
your own: set that one in the web interface. A change that crosses a reset
at the box or another change of the login, checked against the login before
and arriving after, is refused as a whole as well; send it again with the
login now in effect.

With Authentication on, `/control/` answers with HTTP Basic authentication,
as before, also while the first password is in effect. Calls from the box
itself, and from the address range under **No-auth clients**, are not asked,
unless a proxy is entered in `nhttpd-access.conf` (`trusted_proxies`).
Apps that send the user name and password with every request work, and so
do apps that wait for the server to ask for them first. A browser that signs
in this way is answered on `/control/` only when the request comes from a
page of the box (`Sec-Fetch-Site: same-origin`, or an `Origin` or `Referer`
naming the box), as for a sign-in in the web interface. Opened from a
bookmark, the address bar or another site, the box asks before it runs the
command (see below). Apps and scripts whose `User-Agent` does not start with
`Mozilla/` are not affected. A script that sends a browser's `User-Agent`,
such as PowerShell's `Invoke-WebRequest` or `Invoke-RestMethod`, is held to
the browser rule and gets a short text saying so: give it a `User-Agent` of
its own (`-UserAgent`), or an `Origin` header naming the box. An app that
signs in
with a password these boxes once shipped with stops working when the box
replaces it. Enter the first password in the app. After you set your own
password, enter that one.

With Authentication on, a browser that is not signed in and opens a
`/control/` address, or another command of the old interface, from a
bookmark, the address bar or another site is sent to the sign-in page.
After the sign-in it lands on the start page, not on the command: a link on
another site could otherwise have the box run it the moment you sign in.
Open the address again, and the box asks as below. A page of the web
interface opens after the sign-in as before.

### The box asks before a browser runs a command

A page on any site can have your browser open an address of the box, with a
link, a picture or a form, and that address can be a command of the old
interface: `/control/standby?on` puts the box in standby. So when a browser
opens such a command (an address under `/control/`, `/cgi-bin/`, `/fb/`,
`/api/v4/` or `/y/`, or one with an `execute` parameter) and nothing shows
that the request came from a page of the box (`Sec-Fetch-Site: same-origin`,
or an `Origin` or `Referer` naming the box), the box does not run it. It
shows a short page that asks first, in the language of the web interface:

- It says what the command does in plain words where it knows the command
  (standby, wake, switch to a channel, a key of the remote control, restart,
  shut down, the volume, a message on the TV, …), and shows its whole
  address. A command it does not know stands as its address alone. The value
  of a parameter whose name says it holds a password, a PIN, a token, a
  ticket, a secret or a key is shown as `[hidden]`, and so are a user name
  and password inside an address given as a value.
- **Run** runs it. A command that only answers `ok` goes on to the start
  page of the web interface; any other answer is shown, under a heading
  that says whether the box ran the command. **Cancel**, or the Escape key,
  goes to the start page without running anything.
- **Run without asking from now on**: from then on the box runs such
  commands without asking. A page on another site could then run them as
  well. You can change this again under **Settings → Webserver → Access →
  Run commands without asking**. Off as the box ships.

The question holds for one answer and for 5 minutes. It is asked of a
browser that would reach the command otherwise:

- with Authentication off, every browser;
- with Authentication on, a browser from the address range under
  **No-auth clients** or on the box itself, and a browser that is signed in
  or sends the user name and password with HTTP Basic authentication; Run
  then signs in the same way.

It is asked also for a command that only reads, such as `/control/info` or
`/control/volume` without a value. Requests from the pages of the web
interface, and apps and scripts whose `User-Agent` does not start with
`Mozilla/`, are not asked. Every other request from a browser to such a
command gets the short text above and runs nothing: a picture, a frame or a
call from a page on another host, such as a dashboard that shows the box's
screenshot, among them, until the switch is on. So does a change to the
account (`/control/config` naming the user name or the password), which the
box takes only from the web interface, and an address that names a field
of the page that asks itself (`ask_token`, `control_without_asking`).

The switch is the line `control_without_asking` in `nhttpd-access.conf`,
`true` or `false`, read again while the box runs. A line the box cannot
read counts as `false`, and so does a file that is gone or cannot be
read.

## When many sign-ins fail

A wrong password slows the next sign-in down, so that nobody can try
passwords one after the other:

- From one address, the first 2 wrong passwords cost nothing. After that,
  each wrong one doubles the wait before the next try from there, up to one
  minute. The box forgets the address after 10 minutes without a try.
- All addresses together may get the password wrong 20 times in a short
  while. After that the box takes one more try every 15 seconds. While none
  is left, a sign-in waits, also one with the right password, for at most
  15 seconds: the sign-in page asks you to try again in a moment.
- While wrong passwords keep arriving, the sign-ins share the tries that
  come back, so one may have to be tried again more than once, for as long
  as the guessing goes on.
- An address the box saw a right password from in the last 30 days is not
  held up by this, only by its own wait: your own devices sign in as
  usual. A new device, or one away for longer, waits with the rest.
- The box remembers these addresses only while it runs. After it started
  again, also after a deep standby or a power cut, it knows none of them,
  and until a device has signed in once it waits with the rest.
- Behind a proxy every caller comes from the address of the proxy. Once
  somebody has signed in through it, every try through it is held back only
  by the wait of that one address, not by the 20 tries shared by all.
- Sessions already open and the menu at the box are not affected. Nobody is
  locked out: a right password clears the wait of its address.

Someone who mistypes a few times does not notice any of this. Over
`/api/v1` such a wait is answered `429 too-many-attempts` with a
`Retry-After` header in seconds.

## Forgot the password

Reset it at the box itself:
**System settings → Network → Web server (nhttpd) → Reset web password**.

After a confirmation, the user is `root` again and the box makes up a new
first password. The TV shows it in the same message as at the first start.
Every browser and app signed in to the web interface is signed out. The next
sign-in over the web asks for a password of your own again.

## What a sign-in can do

Signed in, a browser has full control over the box: it can change every
setting, put settings back from a backup, install extensions and run
scripts on the box. Sign out on a device that others use as well.

Putting settings back from a backup in the web interface never changes the
login. The user name, the password and the API tokens stay as the box has
them, whatever the backup holds, also a backup of this box or of the box it
replaces. The restore in the settings menu at the box puts back everything
the backup holds, the login included.

## Signed out after a change of the login

A new password or a new user name signs out everybody who signed in before
it, wherever it was set: in the web interface, over `/control/`, in the menu
at the box or with the reset at the box. A browser signed in to the web
interface that made the change stays signed in, also over `/control/`, with
a new sign-in cookie, so a copy of the one it had before is signed out with
everybody else. An app
that signs in with HTTP Basic holds no sign-in to keep and sends the new
password from then on. A change in the menu at the box keeps nobody signed
in. Saving the settings without changing the user name or the password signs
nobody out.

The live updates an open page of the web interface receives (the event
stream, `GET /api/v1/events`) end with the sign-in they were opened with:
after a sign-out, once the sign-in has run out, and after a change of the
login, also in the browser that made the change. The page opens them again
with the sign-in it holds then. Updates an app opened with an API token, or
that the box sends without a sign-in, are not affected.

## For programs using `/api/v1`

- While the box still uses a password it shipped with, or a first password
  it made up for itself, it holds back every caller that may write: a
  session, and an API token of level `write` or `system`.
  `GET /api/v1/session` answers `password_change_required: true` for such
  a caller.
- Until a password of your own is set, such a caller reaches only
  `POST /api/v1/login`, `GET /api/v1/session`, `POST /api/v1/logout` and
  `PUT /api/v1/account/password`. The password change needs level
  `system`, so a `write` token reaches only the first three. Every other
  route it could use refuses it with `403 password-change-required`, also
  the routes that only read.
- An API token of level `read`, and a caller that reads without a password
  from the home network (`lan_read`), are not affected.
- `GET /api/v1/stream/browser/{id}` starts a conversion and may switch the
  TV picture. With a session it is answered only when the request comes from
  a page of the box (`Sec-Fetch-Site: same-origin`, or an `Origin` or
  `Referer` naming the box) or carries the session's `X-CSRF-Token`, as the
  web interface's player does; otherwise `403 not-permitted`. A bearer token
  needs none of this.
- `PUT /api/v1/account/password` takes `{"current": "...", "password": "..."}`
  and answers `200` with the session state once the new password is in
  effect. A wrong `current` is answered `403 wrong-current-password`.
- A new password or user name ends every session opened before it, and every
  token drawn through a session (`POST /api/v1/token/media`), also one the
  asking session drew. The session the change was made with stays signed in
  under a new cookie and a new `csrf` token, which the answer carries:
  `Set-Cookie`, and `csrf` in the session state `PUT /api/v1/account/password`
  answers with (`200`) or in `caller` of `PUT /api/v1/system/webserver`. The
  cookie it came with names nothing from then on. With
  `PUT /api/v1/system/webserver` the other sessions end when the restart
  puts the change in effect. API tokens named in `nhttpd-access.conf`
  (`api_tokens`) are not affected.
- A change to the login that crosses a reset at the box or another change of
  the login writes nothing: `PUT /api/v1/account/password` answers
  `403 not-permitted`, and so does `PUT /api/v1/system/webserver` for a
  session that ended meanwhile.
- A sign-out of the session a change is made with, from another tab say,
  that comes before the change is written, also while the current password
  is checked or the new one prepared, keeps it from being written: both routes
  answer `403 not-permitted` and nothing changes. One that comes once the
  change is being written waits for it: the change is in effect, and the
  browser is signed out, with no new cookie, the answer saying the caller
  holds no session.
- `PUT /api/v1/system/webserver` holds a new `password` to the same rules,
  except that it may be the current one. It sets a new `password` or a new
  `user` only together with `current`, the password in effect, checked and
  counted as at `PUT /api/v1/account/password`: without it
  `400 missing-parameter`, with a wrong one `403 wrong-current-password`. The
  user name sent back as it is needs none, and a box with no password at
  all needs none. Only the reset at the box sets a first password again.
- `PUT /api/v1/system/webserver` also takes `control_without_asking`, `true`
  or `false`, which `GET` answers beside the rest: whether a browser runs a
  command of the old interface without being asked. It is in effect at once
  and restarts nothing. Like every write of that route it needs level
  `system`, and with a session the `X-CSRF-Token` header.
- While the TV shows the message with the first password,
  `GET /api/v1/osd/screenshot` answers `409 password-on-screen`, and so do
  `GET /api/v1/osd/display/screenshot` and
  `GET /api/v1/osd/displays/graphlcd/screenshot`, because GraphLCD can
  mirror the TV's menus. The older `/control/screenshot` and
  `/control/glcdscreenshot` answer their error form with
  `the screen shows a password`. Ask again once the message is gone.
