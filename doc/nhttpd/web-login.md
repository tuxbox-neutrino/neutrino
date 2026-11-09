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
- The new password is in effect at once. Your session stays open.

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
set that one in the web interface.

With Authentication on, `/control/` answers with HTTP Basic authentication,
as before, also while the first password is in effect. Calls from the box
itself, and from the address range under **No-auth clients**, are not asked.
Apps that send the user name and password with every request work, and so
do apps that wait for the server to ask for them first. An app that signs in
with a password these boxes once shipped with stops working when the box
replaces it. Enter the first password in the app. After you set your own
password, enter that one.

## Forgot the password

Reset it at the box itself:
**System settings → Network → Web server (nhttpd) → Reset web password**.

After a confirmation, the user is `root` again and the box makes up a new
first password. The TV shows it in the same message as at the first start.
The next sign-in over the web asks for a password of your own again.

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
  and answers `204` once the new password is in effect.
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
