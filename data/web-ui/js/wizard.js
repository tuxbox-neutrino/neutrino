// The script of wizard.html.

// Auge-Icons lokal: die Wizard-Seite laedt webui-header.js bewusst nicht
var WIZ_EYE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
var WIZ_EYEOFF = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/><line x1="1" y1="1" x2="23" y2="23"/></svg>';
// The icon mirrors the state: crossed-out eye while masked
document.querySelectorAll('.wiz-pw-row .webui-btn').forEach(function(b) { b.innerHTML = WIZ_EYEOFF; });

function wizTogglePw(id, btn) {
	var el = document.getElementById(id);
	var show = el.type === 'password';
	el.type = show ? 'text' : 'password';
	btn.setAttribute('aria-pressed', show ? 'true' : 'false');
	btn.innerHTML = show ? WIZ_EYE : WIZ_EYEOFF;
}

var wizParams = new URLSearchParams(location.search);
var wizLoginOnly = wizParams.get('step') === 'login';
var wizTarget = 'dashboard.html';
var wizPwShipped = false;

// The active language, by its code, so it is only written when it
// changes (a write has the web server read its file again).
var wizStartLang = '';

/* /api/v1 with the session this page has, and its second token for a
   change. Without one, the sign-in; with one that still owes the
   owner's own password, the page that sets it. Both come back here. */
var WIZ_AWAY = 'away';
async function wizV1(method, path, body) {
	var sr = await fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
	var s = sr.ok ? await sr.json() : null;
	var back = encodeURIComponent(location.pathname + location.search);
	if (s && s.authenticated && s.password_change_required) {
		location.replace('/password.html?next=' + back);
		throw new Error(WIZ_AWAY);
	}
	if (!s || !s.authenticated) {
		location.replace('/login.html?next=' + back);
		throw new Error(WIZ_AWAY);
	}
	var headers = {};
	if (method !== 'GET') headers[s.csrf_header] = s.csrf;
	if (body !== undefined) headers['Content-Type'] = 'application/json';
	var r = await fetch('/api/v1' + path, {method: method, credentials: 'same-origin', cache: 'no-store',
		headers: headers, body: body === undefined ? undefined : JSON.stringify(body)});
	var data = null;
	try { data = await r.json(); } catch (ignore) {}
	if (!r.ok) {
		var e = new Error((data && (data.detail || data.title)) || ('HTTP ' + r.status));
		e.status = r.status;
		throw e;
	}
	return data;
}

// a refusal in the page's language: by the kind its status says, and a
// box that did not answer as such (the box's own text is English)
function wizErrText(e) {
	return e && e.status ? webuiKindText(e.status) : webuiL('err.no_answer', 'The box did not answer');
}

// The languages the pages can be shown in, the active one chosen.
async function wizLoadLanguages() {
	try {
		var l = await wizV1('GET', '/webui/language');
		var sel = document.getElementById('wizLang');
		(l.items || []).forEach(function(it) {
			var o = document.createElement('option');
			o.value = it.code;
			o.textContent = it.name;
			o.selected = it.code === l.selected;
			sel.appendChild(o);
		});
		wizStartLang = l.selected || '';
	} catch (e) {
		// no list to choose from: said in the field, which then stays as it is
		var none = document.getElementById('wizLang');
		var o = document.createElement('option');
		o.value = '';
		o.textContent = webuiL('state.failed', 'Could not be read');
		none.replaceChildren(o);
		none.disabled = true;
	}
}

async function wizLoadLogin() {
	try {
		var w = await wizV1('GET', '/system/webserver');
		document.getElementById('wizAuth').checked = !!w.page_login;
		document.getElementById('wizUser').value = w.user || 'root';
		wizPwShipped = !!w.password_shipped;
	} catch (e) {}
}

function wizShowLogin() {
	document.getElementById('wizCardBasics').hidden = true;
	document.getElementById('wizCardLogin').hidden = false;
}

async function wizNext(btn) {
	btn.disabled = true;
	var lang = document.getElementById('wizLang').value;
	wizTarget = document.getElementById('wizPage').value;
	try {
		await wizV1('PATCH', '/webui/settings', {startup: 'true', start_page: wizTarget});
		if (lang && lang !== wizStartLang)
			await wizV1('PUT', '/webui/language', {language: lang});
		wizShowLogin();
	} catch (e) {
		btn.disabled = false;
		wizError(e.message === WIZ_AWAY ? WIZ_AWAY : wizErrText(e), 'wizErr0');
	}
}

function wizError(msg, id) {
	if (msg === WIZ_AWAY) return;
	var el = document.getElementById(id || 'wizErr');
	el.hidden = false;
	el.textContent = msg;
	el.focus();
}

/* A change of the login restarts the web server once the answer is out;
   the next page is asked for only once it answers again, which its
   start count says. Port and address are not the wizard's to change.
   Not back within half a minute, each question given two seconds:
   said, the note as it was, and the page stays, the change being
   saved all the same. */
async function wizAfterRestart(answer) {
	var caller = (answer && answer.caller) || {};
	var was = (answer && answer.webserver) || {};
	if (!caller.restarting) return;
	var note = document.querySelector('#wizCardLogin .wiz-note');
	var noteWas = note ? note.textContent : '';
	if (note) note.textContent = webuiL('set.ws_restarting', 'The web server restarts with the new settings');
	var end = Date.now() + 30000;
	while (Date.now() < end) {
		await new Promise(function(r) { setTimeout(r, 300); });
		// a question left hanging is given up after two seconds, and
		// cut off where the browser can: the deadline holds either way
		var opts = {credentials: 'same-origin', cache: 'no-store'};
		var ask = typeof AbortController === 'function' ? new AbortController() : null;
		if (ask) opts.signal = ask.signal;
		var timer = 0;
		var poll = fetch('/api/v1/system/webserver', opts)
			.then(function(r) { return r.ok ? r.json() : null; });
		poll.catch(function() {});
		var cut = new Promise(function(done) {
			timer = setTimeout(function() { if (ask) ask.abort(); done(null); }, 2000);
		});
		try {
			var now = await Promise.race([poll, cut]);
			if (now && typeof now.restarts === 'number' && now.restarts > was.restarts)
				return;
		} catch (e) { /* not back yet */ }
		finally { clearTimeout(timer); }
	}
	if (note) note.textContent = noteWas;
	var late = new Error(webuiL('wiz.login_not_back', 'Saved, but the web server does not answer again yet. Please press Save again in a moment.'));
	late.wizSaid = true;
	throw late;
}

function wizLoginDone() {
	location.href = '/' + wizTarget;
}

async function wizLoginSave(btn) {
	var auth = document.getElementById('wizAuth').checked;
	var user = document.getElementById('wizUser').value;
	var pw = document.getElementById('wizPass').value;
	var pw2 = document.getElementById('wizPass2').value;
	if (pw !== pw2) { wizError(webuiL('wiz.login_mismatch', 'Passwords do not match.')); return; }
	// the page login on and still the password every box ships with: not saved
	if (auth && !pw && wizPwShipped) { wizError(webuiL('wiz.login_needpw', 'Please enter your own password.')); return; }
	// skipping while the server restarts would ask for the next page
	// in the gap: both wait for the save
	var skip = document.getElementById('wizSkip');
	btn.disabled = skip.disabled = true;
	var body = {page_login: auth, user: user};
	if (pw)
		body.password = pw;
	try {
		var answer = await wizV1('PUT', '/system/webserver', body);
		// saved: a second save after a server slow to come back asks
		// for what is in effect, and sending the password again would
		// count as a change and restart the server once more
		if (pw) {
			document.getElementById('wizPass').value = '';
			document.getElementById('wizPass2').value = '';
			wizPwShipped = false;
		}
		await wizAfterRestart(answer);
		wizLoginDone();
	} catch (e) {
		btn.disabled = skip.disabled = false;
		// the one this page writes itself (not back in time) is its own text
		wizError(e.message === WIZ_AWAY ? WIZ_AWAY : e.wizSaid ? e.message : wizErrText(e));
	}
}

webuiAfterTexts(function() {
	if (wizLoginOnly)
		wizShowLogin();
	wizLoadLanguages();
	wizLoadLogin();
});
