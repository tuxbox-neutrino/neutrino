// The script of password.html.

// The texts in the language the box shows its pages in (webui-i18n.js),
// the title with them.
webuiTexts.then(function() {
	document.title = 'Neutrino-Web-UI - ' + webuiL('pw.title', 'Set your own password');
});

// Local eye icons: this page deliberately does not load webui-header.js
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

// Where to go once the password is set: a path on this box and nothing
// else, by the rule the login page applies.
function pwSafeNext(next) {
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
var pwNext = pwSafeNext(new URLSearchParams(location.search).get('next'));
// asked when said, so they are in the language the texts came in
function pwText(which) {
	return {
		mismatch: function() { return webuiL('pw.mismatch', 'The two new passwords do not match.'); },
		short: function() { return webuiL('pw.short', 'The new password needs at least 8 characters.'); },
		wrong: function() { return webuiL('pw.wrong', 'The current password is not correct.'); },
		failed: function() { return webuiL('pw.failed', 'The password could not be set'); },
		wait: function() { return webuiL('login.wait', 'Too many attempts. Please wait %s seconds and try again.'); }
	}[which]();
}

function pwNote(text) {
	var note = document.getElementById('pwErr');
	note.hidden = !text;
	if (!text)
		return;
	note.textContent = text;
	note.focus();
}

// The session this page works on, with the second token a change sends
// back; none means signing in first, and coming back here after.
async function pwSession() {
	var r = await fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
	var s = r.ok ? await r.json() : null;
	if (!s || !s.authenticated) {
		location.replace('/login.html?next=' + encodeURIComponent('/password.html?next=' + pwNext));
		return null;
	}
	return s;
}

async function pwSubmit(e) {
	e.preventDefault();
	var btn = document.getElementById('pwSubmit');
	if (btn.disabled)
		return;
	var current = document.getElementById('pwCurrent').value;
	var next = document.getElementById('pwNew').value;
	// characters as a person counts them, as the box counts them
	if (Array.from(next).length < 8)
		return pwNote(pwText('short'));
	if (next !== document.getElementById('pwRepeat').value)
		return pwNote(pwText('mismatch'));
	btn.disabled = true;
	pwNote('');
	try {
		var s = await pwSession();
		if (!s)
			return;
		var headers = {'Content-Type': 'application/json'};
		headers[s.csrf_header] = s.csrf;
		var resp = await fetch('/api/v1/account/password', {
			method: 'PUT', credentials: 'same-origin', cache: 'no-store', headers: headers,
			body: JSON.stringify({current: current, password: next})
		});
		if (resp.status === 204) {
			location.replace(pwNext);
			return;
		}
		var detail = '';
		try { detail = (await resp.json()).detail || ''; } catch (ex) {}
		if (resp.status === 403)
			pwNote(pwText('wrong'));
		else if (resp.status === 429)
			pwNote(pwText('wait').replace('%s', resp.headers.get('Retry-After') || '60'));
		else
			pwNote(pwText('failed') + (detail ? ': ' + detail : ''));
	} catch (ex) {
		pwNote(pwText('failed'));
	}
	btn.disabled = false;
}
document.getElementById('pwForm').addEventListener('submit', pwSubmit);
// a page that has no session to change a password with sends to the login
pwSession();
