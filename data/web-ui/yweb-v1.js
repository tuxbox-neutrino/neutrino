// yweb-v1.js - what the modern pages share to talk to the REST interface
// /api/v1 (load after Y_Lang.yjs and yweb-header.js).
//
//   ywebV1('GET', '/bouquets')                            -> parsed JSON
//   ywebV1('GET', '/channels', {query: {mode: 'tv'}})     -> parsed JSON
//   ywebV1('PUT', '/bouquets/' + encodeURIComponent(n) + '/hidden',
//          {body: {on: true}})                            -> null (204)
//
// A refusal throws an Error with the box's reason as its message and the
// HTTP status in .status (answers come as application/problem+json).
//
// Reading needs no sign-in from the home network; changing anything does,
// and so does reading from anywhere else. The box answers either with 403,
// so a 403 opens the sign-in dialog (POST /api/v1/login) unless a session
// is open, and the request is sent once more afterwards. A session that is
// open but whose token went stale gets the token read again instead.
// Changes carry the session's token in the header the session names (CSRF
// protection). opts.refused() is called when the box refused the request and
// it waits for the session to be read or the sign-in, opts.retried() when it
// is sent once more afterwards: a page that keeps count of its changes on the
// way leaves the wait out.
//
// ywebV1Events({name: function(data) {...}}) opens one EventSource on
// /api/v1/events and calls the handler of each event that arrives with its
// data ({channel_id, value, text}). The browser reconnects by itself, except
// after a refusal: a stream the box refused for want of a sign-in is opened
// again once the sign-in went through.

var _ywebV1Csrf = null;	// {header, token} of the open session, if any
var _ywebV1SignInPending = null;	// the one sign-in dialog of the page, while open
var _ywebV1Stream = null;	// {handlers, es, closed} of ywebV1Events

function _ywebV1Url(path, query) {
	var url = '/api/v1' + path;
	if (query) {
		var q = [];
		for (var k in query) {
			if (!Object.prototype.hasOwnProperty.call(query, k)) continue;
			if (query[k] === null || query[k] === undefined) continue;
			q.push(encodeURIComponent(k) + '=' + encodeURIComponent(query[k]));
		}
		if (q.length) url += '?' + q.join('&');
	}
	return url;
}

// Reads the session state and keeps its token; true when one is open.
async function ywebV1Session() {
	_ywebV1Csrf = null;
	try {
		var resp = await fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
		if (!resp.ok) return false;
		var s = await resp.json();
		if (s && s.authenticated && s.csrf && s.csrf_header) {
			_ywebV1Csrf = {header: s.csrf_header, token: s.csrf};
			return true;
		}
	} catch (e) { /* no session then */ }
	return false;
}

function _ywebV1Send(method, path, opts) {
	var headers = {};
	var init = {method: method, credentials: 'same-origin', cache: 'no-store', headers: headers};
	if (opts.body !== undefined) {
		headers['Content-Type'] = 'application/json';
		init.body = JSON.stringify(opts.body);
	}
	if (method !== 'GET' && _ywebV1Csrf)
		headers[_ywebV1Csrf.header] = _ywebV1Csrf.token;
	return fetch(_ywebV1Url(path, opts.query), init);
}

async function _ywebV1Error(resp, method, path) {
	var text = '';
	try {
		var p = await resp.json();
		if (p) text = p.detail || p.title || '';
	} catch (e) { /* not a problem document */ }
	var err = new Error(text || (method + ' ' + path + ': HTTP ' + resp.status));
	err.status = resp.status;
	return err;
}

async function ywebV1(method, path, opts) {
	opts = opts || {};
	var resp = await _ywebV1Send(method, path, opts);
	if (resp.status === 403) {
		// no open session, or a stale token
		if (opts.refused) opts.refused();
		var open = await ywebV1Session();
		if (open || await ywebV1SignIn()) {
			if (opts.retried) opts.retried();
			resp = await _ywebV1Send(method, path, opts);
		}
	}
	if (!resp.ok) throw await _ywebV1Error(resp, method, path);
	if (resp.status === 202 || resp.status === 204) return null;
	var type = resp.headers.get('Content-Type') || '';
	return type.indexOf('json') >= 0 ? resp.json() : resp.text();
}

// Every page of a paged list (items + next_cursor), as one array.
async function ywebV1All(path, query) {
	var items = [];
	var q = Object.assign({}, query || {}, {limit: 500});
	for (;;) {
		var page = await ywebV1('GET', path, {query: q});
		items = items.concat((page && page.items) || []);
		if (!page || !page.next_cursor) return items;
		q.cursor = page.next_cursor;
	}
}

// The sign-in dialog; resolves true once signed in, false when cancelled.
// Requests refused at the same time share the one dialog.
function ywebV1SignIn() {
	if (_ywebV1SignInPending) return _ywebV1SignInPending;
	_ywebV1SignInPending = _ywebV1SignInDialog().then(function(ok) {
		_ywebV1SignInPending = null;
		if (ok && _ywebV1Stream && _ywebV1Stream.closed) _ywebV1Open();
		return ok;
	});
	return _ywebV1SignInPending;
}

function _ywebV1SignInDialog() {
	return new Promise(function(resolve) {
		var before = document.activeElement;
		var backdrop = document.createElement('div');
		backdrop.className = 'yweb-confirm-backdrop';
		backdrop.innerHTML =
			'<div class="yweb-confirm-dialog" role="dialog" aria-modal="true" aria-labelledby="yweb-v1-signin-title">' +
				'<h4 id="yweb-v1-signin-title"></h4>' +
				'<p class="yweb-v1-why"></p>' +
				'<label class="yweb-prompt-label" for="yweb-v1-user"></label>' +
				'<input type="text" class="yweb-prompt-input" id="yweb-v1-user" autocomplete="username" maxlength="255">' +
				'<label class="yweb-prompt-label" for="yweb-v1-pass"></label>' +
				'<input type="password" class="yweb-prompt-input" id="yweb-v1-pass" autocomplete="current-password" maxlength="255">' +
				'<p class="yweb-v1-err" role="alert" hidden></p>' +
				'<div class="yweb-confirm-actions">' +
					'<button type="button" class="confirm-cancel"></button>' +
					'<button type="button" class="confirm-ok"></button>' +
				'</div>' +
			'</div>';
		backdrop.querySelector('h4').textContent = ywebL('v1.signin_title', 'Sign in');
		backdrop.querySelector('.yweb-v1-why').textContent =
			ywebL('v1.signin_why', 'The box asks for a sign-in for this.');
		var labels = backdrop.querySelectorAll('.yweb-prompt-label');
		labels[0].textContent = ywebL('v1.user', 'User');
		labels[1].textContent = ywebL('v1.password', 'Password');
		var user = backdrop.querySelector('#yweb-v1-user');
		var pass = backdrop.querySelector('#yweb-v1-pass');
		var err = backdrop.querySelector('.yweb-v1-err');
		var okBtn = backdrop.querySelector('.confirm-ok');
		var cancelBtn = backdrop.querySelector('.confirm-cancel');
		okBtn.textContent = ywebL('v1.signin', 'Sign in');
		cancelBtn.textContent = ywebL('msg.cancel', 'Cancel');

		var done = false;
		function finish(result) {
			if (done) return;
			done = true;
			backdrop.remove();
			if (before && before.focus) before.focus();
			resolve(result);
		}
		async function submit() {
			if (okBtn.disabled) return;
			if (!user.value) { user.focus(); return; }
			okBtn.disabled = true;
			err.hidden = true;
			// a wrong password says so; any other refusal says what the box said
			var why = ywebL('v1.signin_failed', 'Sign-in failed');
			try {
				var resp = await fetch('/api/v1/login', {
					method: 'POST', credentials: 'same-origin', cache: 'no-store',
					headers: {'Content-Type': 'application/json'},
					body: JSON.stringify({user: user.value, password: pass.value})
				});
				if (resp.ok) {
					var s = await resp.json();
					if (s && s.authenticated && s.csrf && s.csrf_header) {
						_ywebV1Csrf = {header: s.csrf_header, token: s.csrf};
						finish(true);
						return;
					}
				} else if (resp.status !== 401) {
					why = (await _ywebV1Error(resp, 'POST', '/login')).message;
				}
			} catch (e) { /* refused the same way below */ }
			err.textContent = why;
			err.hidden = false;
			okBtn.disabled = false;
			pass.select();
		}
		okBtn.onclick = submit;
		cancelBtn.onclick = function() { finish(false); };
		backdrop.onclick = function(e) { if (e.target === backdrop) finish(false); };
		backdrop.onkeydown = function(e) {
			if (e.key === 'Enter' && e.target !== cancelBtn) { e.preventDefault(); submit(); }
			if (e.key === 'Escape') finish(false);
			if (e.key === 'Tab') {
				// the focus stays within the dialog
				var stops = [user, pass, cancelBtn, okBtn].filter(function(el) { return !el.disabled; });
				var at = stops.indexOf(document.activeElement);
				var next = e.shiftKey ? (at <= 0 ? stops.length - 1 : at - 1) : (at + 1) % stops.length;
				e.preventDefault();
				stops[next].focus();
			}
		};
		document.body.appendChild(backdrop);
		user.focus();
	});
}

// One EventSource for the page; handlers keyed by event name.
function ywebV1Events(handlers) {
	if (!window.EventSource) return null;
	_ywebV1Stream = {handlers: handlers, es: null, closed: false};
	return _ywebV1Open();
}

function _ywebV1Open() {
	var stream = _ywebV1Stream;
	var es = new EventSource('/api/v1/events');
	stream.es = es;
	stream.closed = false;
	Object.keys(stream.handlers).forEach(function(name) {
		es.addEventListener(name, function(e) {
			var data = null;
			try { data = JSON.parse(e.data); } catch (ignore) { /* keep null */ }
			stream.handlers[name](data);
		});
	});
	// a refused stream is not retried by the browser; a sign-in reopens it
	es.addEventListener('error', function() {
		if (es.readyState === EventSource.CLOSED) stream.closed = true;
	});
	return es;
}
