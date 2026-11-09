// The script of password.html.

// The texts in the language the box shows its pages in (webui-i18n.js),
// the title with them.
webuiTexts.then(function() {
	document.title = 'Neutrino-Web-UI - ' + webuiL('pw.title', 'Set your own password');
});

var pwNext = signinSafeNext(new URLSearchParams(location.search).get('next'));
// asked when said, so they are in the language the texts came in
function pwText(which) {
	return {
		mismatch: function() { return webuiL('pw.mismatch', 'The two new passwords do not match.'); },
		short: function() { return webuiL('pw.short', 'The new password needs at least 8 characters.'); },
		wrong: function() { return webuiL('pw.wrong', 'The current password is not correct.'); },
		bad: function() { return webuiL('pw.bad', 'Please choose another password: at least 8 characters, no control characters, not the one in use and not one the box was delivered with.'); },
		unwritten: function() { return webuiL('pw.unwritten', 'The box could not store the new password; the old one stays.'); },
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
		// set: this browser goes on under the new cookie the answer carries
		if (resp.ok) {
			location.replace(pwNext);
			return;
		}
		// each refusal the route names in the page's words, never the box's
		// own English; another by the kind its status says
		if (resp.status === 403)
			pwNote(pwText('wrong'));
		else if (resp.status === 400)
			pwNote(pwText('bad'));
		else if (resp.status === 429)
			pwNote(pwText('wait').replace('%s', resp.headers.get('Retry-After') || '60'));
		else if (resp.status === 500)
			pwNote(pwText('unwritten'));
		else
			pwNote(pwText('failed') + ': ' + webuiKindText(resp.status));
	} catch (ex) {
		pwNote(pwText('failed') + ': ' + webuiL('err.no_answer', 'The box did not answer'));
	}
	btn.disabled = false;
}
document.getElementById('pwForm').addEventListener('submit', pwSubmit);
// a page that has no session to change a password with sends to the login
pwSession();
