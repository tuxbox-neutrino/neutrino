// The script of login.html.

// The texts in the language the box shows its pages in (webui-i18n.js),
// the title with them.
webuiTexts.then(function() {
	document.title = 'Neutrino-Web-UI - ' + webuiL('login.title', 'Sign in');
});

var loginParams = new URLSearchParams(location.search);
var loginNext = signinSafeNext(loginParams.get('next'));

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
