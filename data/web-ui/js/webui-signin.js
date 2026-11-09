// The parts the sign-in page (login.html) and the page that sets a
// password of one's own (password.html) share. Neither loads
// webui-header.js or the client of the API: both stand before a session.

// The eye that shows or hides a password, crossed out while it is hidden
var LOGIN_EYE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
var LOGIN_EYEOFF = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/><line x1="1" y1="1" x2="23" y2="23"/></svg>';
document.querySelectorAll('.login-pw-row .webui-btn').forEach(function(b) { b.innerHTML = LOGIN_EYEOFF; });

function loginTogglePw(id, btn) {
	var el = document.getElementById(id);
	var show = el.type === 'password';
	el.type = show ? 'text' : 'password';
	btn.setAttribute('aria-pressed', show ? 'true' : 'false');
	btn.innerHTML = show ? LOGIN_EYE : LOGIN_EYEOFF;
}

// Where to go once signed in or once the password is set. The page opened
// then is opened from this page, a page of the box, so it gets what only
// the box's own pages get: a command of the old server (/control/standby?on)
// would run. So only a page of this interface is taken, and anything else
// goes to the start page:
// - a path on this box: one leading slash, no second one, no backslash, no
//   control byte or space anywhere (a browser drops tabs and line ends
//   before it reads an address, so "/<tab>/host" would become "//host"),
//   and the address as the browser reads it still on this box;
// - a page: a folder ("/", "/extensions/files/") or a file whose name ends
//   in .html or .htm, in parts of letters, digits and . _ ~ - with none
//   starting with a dot, so nothing in it is escaped or a step up;
// - nothing under /control/, /cgi-bin/, /fb/, /api/ or /y/, in any case;
// - no query naming execute, the old server's command parameter, also
//   escaped;
// with its query and the place on the page kept. The box sends a browser
// here with a page only (mod_auth); this holds for every other way in.
var SIGNIN_PAGE = /^\/(?:[A-Za-z0-9_~-][A-Za-z0-9._~-]*\/)*(?:[A-Za-z0-9_~-][A-Za-z0-9._~-]*\.html?)?$/i;
var SIGNIN_COMMANDS = /^\/(?:control|cgi-bin|fb|api|y)(?:\/|$)/i;
function signinRunsCommand(search) {
	var text = search.replace(/\+/g, ' ');
	try {
		text = decodeURIComponent(text);
	} catch (e) {
		return true;
	}
	return /execute/i.test(text);
}
function signinSafeNext(next) {
	if (!next || /[\u0000- \u007f\\]/.test(next) || next.charAt(0) !== '/' || next.charAt(1) === '/')
		return '/';
	try {
		var u = new URL(next, location.origin);
		if (u.origin !== location.origin || !SIGNIN_PAGE.test(u.pathname) || SIGNIN_COMMANDS.test(u.pathname)
				|| signinRunsCommand(u.search))
			return '/';
		return u.pathname + u.search + u.hash;
	} catch (e) {
		return '/';
	}
}
