# Signing in to the web interface

## First sign-in

Every box ships with the same login for its web interface:

| User | Password   |
|------|------------|
| root | `neutrino` |

The sign-in page shows this as a hint. The value is set once, at build time
(`WEB_DEFAULT_PASSWORD` in `configure.ac`).

Since anybody can look this password up, the first sign-in asks for a
password of your own. Until one is set, the web interface does nothing else:

- The pages send you to **Set your own password**. Enter the current
  password and the new one twice.
- The new password needs at least 8 characters. It may not be one of the
  passwords these boxes have shipped with, and it may not be the current
  one.
- The new password is in effect at once. Your session stays open.

A box updated from an older image may still use a password those images
shipped with: `tuxbox`, `coolstream` or `ni`. It is treated the same way and
asks for a password of your own at the next sign-in.

## Remote control apps and scripts

The old `/control/` interface keeps answering with HTTP Basic
authentication, as before, also while the default password is in effect.
Apps that send the user name and password with every request work, and so
do apps that wait for the server to ask for them first.

After you set your own password, enter the new one in those apps as well.

## Forgot the password

Reset it at the box itself:
**Settings → Network → Web server (nhttpd) → Reset web password**.

After a confirmation, the login is back to user `root` and the default
password. The next sign-in over the web asks for a password of your own
again.

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
- `PUT /api/v1/webserver` holds a new `password` to the same rules. Only the
  reset at the box sets the default password again.
