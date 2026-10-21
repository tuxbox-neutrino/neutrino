// The script of login.html.

// The texts in the language the box shows its pages in (webui-i18n.js),
// the title with them.
webuiTexts.then(function() {
	document.title = 'Neutrino-Web-UI - ' + webuiL('login.title', 'Sign in');
});

// Local eye icons: this page deliberately does not load webui-header.js
var LOGIN_EYE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
var LOGIN_EYEOFF = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/><line x1="1" y1="1" x2="23" y2="23"/></svg>';
// The icon mirrors the state: crossed-out eye while masked
document.querySelectorAll('.login-pw-row .webui-btn').forEach(function(b) { b.innerHTML = LOGIN_EYEOFF; });

function loginTogglePw(id, btn) {
	var el = document.getElementById(id);
	var show = el.type === 'password';
	el.type = show ? 'text' : 'password';
	btn.setAttribute('aria-pressed', show ? 'true' : 'false');
	btn.innerHTML = show ? LOGIN_EYE : LOGIN_EYEOFF;
}

// Where to go once signed in: a path on this box and nothing else -
// one leading slash, no second one, no backslash, no control byte or
// space anywhere (a browser drops tabs and line ends before it reads an
// address, so "/<tab>/host" would become "//host"), and the address as
// the browser reads it still on this box.
function loginSafeNext(next) {
	if (!next || /[\u0000-\u0020\u007f\\]/.test(next) || next.charAt(0) !== '/' || next.charAt(1) === '/')
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

var loginParams = new URLSearchParams(location.search);
var loginNext = loginSafeNext(loginParams.get('next'));

function loginNote(id, text) {
	['loginErr', 'loginWait', 'loginOut'].forEach(function(n) {
		document.getElementById(n).hidden = (n !== id);
	});
	if (!id)
		return;
	var note = document.getElementById(id);
	// the error note says a wrong login unless it is told what else
	if (id === 'loginErr')
		note.textContent = text || webuiL('login.failed', 'Sign-in failed. Please check username and password.');
	else if (text)
		note.textContent = text;
	if (note.hasAttribute('tabindex'))
		note.focus();
}

async function loginSubmit(e) {
	e.preventDefault();
	var btn = document.getElementById('loginSubmit');
	var pass = document.getElementById('loginPass');
	if (btn.disabled)
		return;
	btn.disabled = true;
	loginNote(null);
	try {
		var resp = await fetch('/api/v1/login', {
			method: 'POST', credentials: 'same-origin', cache: 'no-store',
			headers: {'Content-Type': 'application/json'},
			body: JSON.stringify({user: document.getElementById('loginUser').value, password: pass.value})
		});
		if (resp.ok) {
			// signed in with the password the box shipped with: one of
			// the owner's own first, then on to where the login was going
			var opened = null;
			try { opened = await resp.json(); } catch (ex) { /* no document */ }
			if (opened && opened.password_change_required) {
				location.replace('/password.html?next=' + encodeURIComponent(loginNext));
				return;
			}
			location.replace(loginNext);
			return;
		}
		// A session is open already: the login route then asks for its
		// second token, and there is nothing left to sign in to.
		if (resp.status === 403) {
			var s = await fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
			var state = s.ok ? await s.json() : null;
			if (state && state.authenticated) {
				location.replace(loginNext);
				return;
			}
		}
		// a wrong login is the box saying no to these two; anything else
		// is the box, and is not to be read as a wrong password
		if (resp.status === 429)
			loginNote('loginWait', webuiL('login.wait', 'Too many attempts. Please wait %s seconds and try again.').replace('%s', resp.headers.get('Retry-After') || '60'));
		else if (resp.status === 400 || resp.status === 401 || resp.status === 403)
			loginNote('loginErr');
		else
			loginNote('loginErr', webuiKindText(resp.status));
	} catch (ex) {
		loginNote('loginErr', webuiL('err.no_answer', 'The box did not answer'));
	}
	btn.disabled = false;
	pass.select();
}
document.getElementById('loginForm').addEventListener('submit', loginSubmit);

if (loginParams.get('out') === '1')
	loginNote('loginOut');
