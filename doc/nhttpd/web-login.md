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
  off.

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

## Remote control apps and scripts

The old `/control/` interface asks for a login only while
**Authentication** is on under **System settings → Network → Web server
(nhttpd)**. The box ships with it off. Then `/control/` answers anybody who
reaches the box, without a password, whatever an app sends, except for a
change to the account. The user name and the password change over
`/control/` only for a request from a page of this box
(`Sec-Fetch-Site: same-origin`, or an `Origin` or `Referer` naming the box)
that signs in, with HTTP Basic authentication or in the web interface, or
that comes from the box itself while no proxy is configured in front of it,
whether Authentication is off or on. Any other request to change them is
refused as a whole, also from the address range under **No-auth clients**,
and so is every one while the box still waits for a password of your own:
set that one in the web interface. A change that crosses a reset at the box
or another change of the login, checked against the login before and arriving
after, is refused as a whole as well; send it again with the login now in
effect.

With Authentication on, `/control/` answers with HTTP Basic authentication,
as before, also while the first password is in effect. Calls from the box
itself, and from the address range under **No-auth clients**, are not asked.
Apps that send the user name and password with every request work, and so
do apps that wait for the server to ask for them first. An app that signs in
with a password these boxes once shipped with stops working when the box
replaces it. Enter the first password in the app. After you set your own
password, enter that one.

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
- `PUT /api/v1/account/password` takes `{"current": "...", "password": "..."}`
  and answers `200` with the session state once the new password is in
  effect.
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
- `PUT /api/v1/system/webserver` holds a new `password` to the same rules,
  except that it may be the current one. Only the reset at the box sets a
  first password again.
- While the TV shows the message with the first password,
  `GET /api/v1/osd/screenshot` answers `409 password-on-screen`, and so do
  `GET /api/v1/osd/display/screenshot` and
  `GET /api/v1/osd/displays/graphlcd/screenshot`, because GraphLCD can
  mirror the TV's menus. The older `/control/screenshot` and
  `/control/glcdscreenshot` answer their error form with
  `the screen shows a password`. Ask again once the message is gone.
