// The script of wizard.html, the welcome page.

// index.js sends a browser here while the web interface's settings answer
// startup other than true. The page asks for the language and the start page
// and writes startup=true only when it is left by Get started, Skip or its
// link to the web server's settings, so it is shown until somebody went
// through it, and never after.

// The language in effect, by its code, so it is only written when it
// changes (a write has the web server read its file again).
var wizStartLang = '';
var WIZ_AWAY = 'away';
// a write under way: the buttons and the link wait for it
var wizBusy = false;

/* The session this page writes with. A browser without one signs in
   first, and one that still owes the owner's own password sets it first;
   both come back here, so the choices are made once, by a session that
   may write them. */
async function wizSession() {
	var sr = await fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
	var s = sr.ok ? await sr.json() : null;
	var back = encodeURIComponent(location.pathname);
	if (s && s.authenticated && s.password_change_required) {
		location.replace('/password.html?next=' + back);
		throw new Error(WIZ_AWAY);
	}
	if (!s || !s.authenticated) {
		location.replace('/login.html?next=' + back);
		throw new Error(WIZ_AWAY);
	}
	return s;
}

// /api/v1 with the session this page has, and its second token for a change.
async function wizV1(method, path, body) {
	var s = await wizSession();
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

function wizError(msg) {
	var el = document.getElementById('wizErr');
	el.hidden = !msg;
	if (!msg)
		return;
	el.textContent = msg;
	el.focus();
}

// The languages the pages can be shown in, the one in effect chosen.
async function wizLoadLanguages() {
	var sel = document.getElementById('wizLang');
	try {
		var l = await wizV1('GET', '/webui/language');
		(l.items || []).forEach(function(it) {
			var o = document.createElement('option');
			o.value = it.code;
			o.textContent = it.name;
			o.selected = it.code === l.selected;
			sel.appendChild(o);
		});
		wizStartLang = l.selected || '';
	} catch (e) {
		if (e.message === WIZ_AWAY)
			return;
		// no list to choose from: said in the field, which then stays as it is
		var o = document.createElement('option');
		o.value = '';
		o.textContent = webuiL('state.failed', 'Could not be read');
		sel.replaceChildren(o);
		sel.disabled = true;
	}
}

// The start page set so far chosen, where it is one of the list.
async function wizLoadStartPage() {
	try {
		var w = await wizV1('GET', '/webui/settings');
		var page = '';
		((w && w.items) || []).forEach(function(it) { if (it.key === 'start_page') page = String(it.value).replace(/^\//, ''); });
		var sel = document.getElementById('wizPage');
		if (Array.prototype.some.call(sel.options, function(o) { return o.value === page; }))
			sel.value = page;
	} catch (e) { /* the first of the list stays chosen */ }
}

// The page shown in the language chosen, before anything is written.
async function wizPreviewLanguage() {
	var code = document.getElementById('wizLang').value;
	if (!code)
		return;
	await webuiI18nLoad(code);
	webuiI18nApply();
}

function wizSetBusy(on) {
	wizBusy = on;
	document.getElementById('wizForm').setAttribute('aria-busy', on ? 'true' : 'false');
	['wizGo', 'wizSkip'].forEach(function(id) {
		document.getElementById(id).setAttribute('aria-disabled', on ? 'true' : 'false');
	});
}

/* Leaves the page for where: with the choices (Get started, the link) or
   without them (Skip). startup is written last, with the start page, so
   it says true only once the rest went through. */
async function wizLeave(withChoices, where) {
	if (wizBusy)
		return;
	wizSetBusy(true);
	wizError('');
	var body = {startup: 'true'};
	try {
		if (withChoices) {
			var lang = document.getElementById('wizLang').value;
			if (lang && lang !== wizStartLang) {
				await wizV1('PUT', '/webui/language', {language: lang});
				wizStartLang = lang;
			}
			body.start_page = document.getElementById('wizPage').value;
		}
		await wizV1('PATCH', '/webui/settings', body);
		location.replace(where === undefined ? '/' + body.start_page : where);
	} catch (e) {
		wizSetBusy(false);
		if (e.message !== WIZ_AWAY)
			wizError(wizErrText(e));
	}
}

document.getElementById('wizForm').addEventListener('submit', function(e) {
	e.preventDefault();
	wizLeave(true);
});
// the start page as it is set: index.js picks it
document.getElementById('wizSkip').addEventListener('click', function() { wizLeave(false, '/'); });
document.getElementById('wizProtectLink').addEventListener('click', function(e) {
	// a new tab or window opens the settings and leaves this page as it is
	if (e.button !== 0 || e.ctrlKey || e.metaKey || e.shiftKey || e.altKey)
		return;
	e.preventDefault();
	wizLeave(true, this.getAttribute('href'));
});
document.getElementById('wizLang').addEventListener('change', function() { wizPreviewLanguage(); });

webuiAfterTexts(async function() {
	try {
		await wizSession();
	} catch (e) {
		// on the way to the sign-in or the password page; a box that did not
		// answer gets the page all the same, and a write then says so
		if (e.message === WIZ_AWAY)
			return;
	}
	document.getElementById('wizCard').hidden = false;
	await Promise.all([wizLoadLanguages(), wizLoadStartPage()]);
});
