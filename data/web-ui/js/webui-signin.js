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

// Where to go once signed in or once the password is set: a path on this
// box and nothing else - one leading slash, no second one, no backslash,
// no control byte or space anywhere (a browser drops tabs and line ends
// before it reads an address, so "/<tab>/host" would become "//host"), and
// the address as the browser reads it still on this box.
function signinSafeNext(next) {
	if (!next || /[\u0000- \u007f\\]/.test(next) || next.charAt(0) !== '/' || next.charAt(1) === '/')
		return '/';
	try {
		var u = new URL(next, location.origin);
		var path = u.pathname + u.search + u.hash;
		// "/x/..//host" reads as the path "//host": an address of its own
		return u.origin === location.origin && path.charAt(1) !== '/' ? path : '/';
	} catch (e) {
		return '/';
	}
}
