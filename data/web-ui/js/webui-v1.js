// webui-v1.js - what the modern pages share to talk to the REST interface
// /api/v1 (load after webui-i18n.js and webui-header.js).
//
//   webuiV1('GET', '/bouquets')                            -> parsed JSON
//   webuiV1('GET', '/channels', {query: {mode: 'tv'}})     -> parsed JSON
//   webuiV1('PUT', '/bouquets/' + encodeURIComponent(n) + '/hidden',
//          {body: {on: true}})                            -> null (204)
//   webuiV1Blob('/osd/screenshot', {format: 'png'})        -> Blob
//   webuiV1('PUT', '/storage/file', {query: {path: p}, raw: blob})
//                                                         -> bytes as the body
//   webuiV1UploadPieces(file, '/tmp/x.tar.gz', onProgress) -> a file of any size
//   webuiV1Download('/system/backup', 'settings.tar.gz')   -> saved, its name
//
// A refusal throws an Error with the box's reason as its message, the HTTP
// status in .status and the code of the refusal in .code (answers come as
// application/problem+json, whose type is /errors/ and the code), and in
// .dependsOn the keys of the settings the refusal names as its cause (its
// depends_on, [] where it names none); a 403 the user did not sign in for
// carries .cancelled. webuiToastError (in webui-header.js) tells the user
// why, in the page's language.
//
// An answer without a body (202, 204) resolves null; with opts.accepted it
// resolves true for 202, which says the box took the request on, and false
// for 204, which says there was nothing to do.
//
// Every channel answer (GET /channels, /channels/current, /channels/{id})
// names the coding the box expects of the channel's picture; it is noted as
// it passes (webuiNoteChannels), so the pages offer playing in the browser
// as that coding allows without asking the box for it.
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
// way leaves the wait out. opts.once sends a request that checks a password
// once and no more: its 403 is thrown as it came, without reading the session
// again, opening the sign-in or sending it a second time, which would count a
// wrong password twice; a page reads the session (webuiV1Session, and
// webuiV1SignIn where none is open) before such a request instead.
//
// A change of the login gives the page's session new tokens: the browser keeps
// the new cookie by itself, and webuiV1Renewed(csrf) takes the second token the
// answer carries (caller.csrf of PUT /system/webserver); without one it leaves
// the token the page has.
//
// A change the box takes long to answer shows that it is under way, a bar
// at the top and a spinner on the button whose click sent it
// (webuiWaitBegin in webui-header.js); nothing shows while the sign-in is
// open, and reads show nothing. opts.mark = false leaves the button out
// (the bar alone), for a button pressed again and again, such as a key of
// the remote control.
//
// webuiV1Events({name: function(data) {...}}) opens the page's one
// EventSource on /api/v1/events and calls the handler of each event that
// arrives with its data ({channel_id, value, text}); a second call adds its
// handlers to the same stream. The stream is let go at once when the page is
// hidden or left, and taken up again when it is shown. The browser reconnects
// by itself after a broken line. A refusal, which an EventSource does not
// explain, is asked about once with a plain GET of the stream: 501 says the
// box carries no event streams, and none is asked for again until the page
// loads anew; 503 is tried again after its Retry-After, 401 and 403 once a
// sign-in went through, anything else after a wait that grows from 15 s to
// 2 min; every wait is spread by 30 to 50 %, so the tabs a box turned away do
// not all come back at once. A handler named `open` is called with null at
// each opening, one named `drop` when a stream that was open is lost (the box
// went away), one named `refused` when the box refuses it for want of a
// sign-in (401, 403). webuiV1EventsLive() says whether the stream is open
// right now, webuiV1SignInOpen() whether the page's sign-in dialog is open.
// What a page shows of the box follows the stream with webuiFollow: read
// again exactly once after a reconnect (an opening after a refusal, a
// failure or the page being hidden) and, without a live stream, when the
// page comes into view and on a timer, on which a read that failed is made
// again with a live stream too; it opens no stream itself. The head, which
// is not a page, listens to the same stream through webuiV1Watch(name, fn).
//
// webuiStopRecordingConfirmed(what, {button, title, channel}) ends a
// recording and webuiPlayRecordingConfirmed(id, {button}) plays one on the
// TV; each says so once the box confirms it (an event or a read), not on the
// 202, and keeps its button busy meanwhile. opts.channel is a key for what a
// stop was pressed for, which the helpers give no other meaning: the guide
// dialog passes the channel it shows, a page any string of its own (a
// timer's, say). A second stop of the same recording waits for the first:
// by its id where the page passes one, else (a stop that finds its
// recording at the press) by the key. A button that serves several keys
// shows the stop only while it serves that one (webuiStopShownFor).
//
// webuiV1Offers('POST', '/playback/stop') says whether the box has a route,
// from its description of the interface, so a page offers a newer route
// only where the box has it.

var _webuiV1Csrf = null;	// {header, token} of the open session, if any
var _webuiV1SignInPending = null;	// the one sign-in dialog of the page, while open
var _webuiV1Stream = null;	// the page's stream (webuiV1Events), see _webuiV1NewStream
var _webuiV1Watchers = [];	// [name, fn] pairs of webuiV1Watch

function _webuiV1Url(path, query) {
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

// Keeps the token of a session document (GET /api/v1/session); true when it names one.
function webuiV1KeepSession(s) {
	if (s && s.authenticated && s.csrf && s.csrf_header) {
		_webuiV1Csrf = {header: s.csrf_header, token: s.csrf};
		return true;
	}
	return false;
}

/* The session's token in the header the session names, {name: token}, or {}
   without one. For a GET that starts something, which the box takes on a
   session only when it shows it came from a page of the box: the header
   says so wherever the browser sends no Referer, and a page of another
   site cannot send it. */
function webuiV1SessionHeaders() {
	var h = {};
	if (_webuiV1Csrf)
		h[_webuiV1Csrf.header] = _webuiV1Csrf.token;
	return h;
}

// Reads the session state and keeps its token; true when one is open.
async function webuiV1Session() {
	_webuiV1Csrf = null;
	try {
		var resp = await webuiFetchBox('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
		if (!resp.ok) return false;
		return webuiV1KeepSession(await resp.json());
	} catch (e) { /* no session then */ }
	return false;
}

/* A change of the login gives the page's session new tokens: the cookie the
   browser keeps by itself from the answer, the second token the page carries
   from the answer's csrf. Where the page held none yet, its next change reads
   the session first, as it always does. */
function webuiV1Renewed(csrf) {
	if (!csrf) return;
	if (_webuiV1Csrf)
		_webuiV1Csrf = {header: _webuiV1Csrf.header, token: csrf};
}

/* The token a change has to carry, read before the page's first change goes
   out: without it every first change of a page was refused (403) and sent
   again, a failed request in the browser's console each time. A later 403
   still reads it again (a session that timed out, or one opened since). */
var _webuiV1FirstRead = null;

async function _webuiV1Send(method, path, opts) {
	if (method !== 'GET' && !_webuiV1Csrf) {
		if (!_webuiV1FirstRead)
			_webuiV1FirstRead = webuiV1Session();
		await _webuiV1FirstRead;
	}
	var headers = {};
	var init = {method: method, credentials: 'same-origin', cache: 'no-store', headers: headers};
	if (opts.raw !== undefined) {
		// bytes as they are (a file or a piece of one), not a document
		headers['Content-Type'] = 'application/octet-stream';
		init.body = opts.raw;
	} else if (opts.body !== undefined) {
		headers['Content-Type'] = 'application/json';
		init.body = JSON.stringify(opts.body);
	}
	if (method !== 'GET' && _webuiV1Csrf)
		headers[_webuiV1Csrf.header] = _webuiV1Csrf.token;
	return webuiFetchBox(_webuiV1Url(path, opts.query), init);
}

async function _webuiV1Error(resp, method, path) {
	var text = '';
	var code = '';
	var depends = [];
	try {
		var p = await resp.json();
		if (p) text = p.detail || p.title || '';
		if (p && typeof p.type === 'string' && p.type.indexOf('/errors/') === 0)
			code = p.type.substring(8);
		if (p && Array.isArray(p.depends_on))
			depends = p.depends_on.map(String);
	} catch (e) { /* not a problem document */ }
	var err = new Error(text || (method + ' ' + path + ': HTTP ' + resp.status));
	err.status = resp.status;
	err.code = code;
	err.dependsOn = depends;
	return err;
}

/* A 403 left after the user cancelled the sign-in it opened is no news to
   them: marked cancelled, webuiToastError says nothing about it. */
function _webuiV1Declined(err, declined) {
	if (declined && err.status === 403)
		err.cancelled = true;
	return err;
}

/* A refusal saying the box still opens to the password it shipped with: the
   page the owner sets one of their own on, and back here afterwards. Read from a
   copy, so the caller can still read the answer. */
async function _webuiV1ChangeOwed(resp) {
	if (resp.status !== 403)
		return false;
	try {
		var p = await resp.clone().json();
		if (p && p.type === '/errors/password-change-required') {
			location.replace('/password.html?next=' + encodeURIComponent(location.pathname + location.search));
			return true;
		}
	} catch (e) { /* not a problem document */ }
	return false;
}

async function webuiV1(method, path, opts) {
	opts = opts || {};
	// the button is the one of the click being handled, so taken before the
	// first await
	var waits = method !== 'GET' && typeof webuiWaitBegin === 'function';
	var btn = waits && opts.mark !== false ? webuiWaitButton() : null;
	var done = waits ? webuiWaitBegin(btn) : null;
	try {
		var resp = await _webuiV1Send(method, path, opts);
		if (await _webuiV1ChangeOwed(resp))
			throw await _webuiV1Error(resp, method, path);
		var declined = false;
		// opts.once: a request that checks a password is not sent a second time
		if (resp.status === 403 && !opts.once) {
			// no open session, or a stale token
			if (done) done();
			if (opts.refused) opts.refused();
			var open = await webuiV1Session();
			if (open || await webuiV1SignIn()) {
				if (opts.retried) opts.retried();
				if (waits) done = webuiWaitBegin(btn);
				resp = await _webuiV1Send(method, path, opts);
			} else {
				declined = true;
			}
		}
		if (!resp.ok) throw _webuiV1Declined(await _webuiV1Error(resp, method, path), declined);
		if (resp.status === 202 || resp.status === 204) return opts.accepted ? resp.status === 202 : null;
		var type = resp.headers.get('Content-Type') || '';
		var doc = await (type.indexOf('json') >= 0 ? resp.json() : resp.text());
		if (method === 'GET' && /^\/channels(\/[^/?]+)?(\?|$)/.test(path))
			webuiNoteChannels(doc);
		return doc;
	} finally {
		if (done) done();
	}
}

// A picture (or any other file) as a Blob, for a screenshot: the same
// sign-in on 403 as webuiV1, and a refusal throws the same Error.
// opts.quiet leaves the sign-in out and throws on a 403 (a probe at load).
async function webuiV1Blob(path, query, opts) {
	opts = opts || {};
	var url = _webuiV1Url(path, query);
	var get = function() { return webuiFetchBox(url, {credentials: 'same-origin', cache: 'no-store'}); };
	var resp = await get();
	if (await _webuiV1ChangeOwed(resp))
		throw await _webuiV1Error(resp, 'GET', path);
	var declined = false;
	if (resp.status === 403 && !opts.quiet) {
		if (await webuiV1Session() || await webuiV1SignIn())
			resp = await get();
		else
			declined = true;
	}
	if (!resp.ok) throw _webuiV1Declined(await _webuiV1Error(resp, 'GET', path), declined);
	return resp.blob();
}

/* The name a Content-Disposition gives a file, '' where it gives none: the
   encoded form (filename*=UTF-8''...), which the box writes for a playlist
   and a file of its folders, before the plain one, which it writes for a
   backup. A folder in the name is taken out. */
function _webuiV1FileName(disposition) {
	var name = '';
	var m = /filename\*=UTF-8''([^;\s]+)/i.exec(disposition);
	if (m) {
		try {
			name = decodeURIComponent(m[1]);
		} catch (e) { /* not encoded as it says */ }
	}
	if (!name) {
		m = /filename="([^"]+)"/.exec(disposition);
		name = m ? m[1] : '';
	}
	return name.replace(/[\/\\]/g, '_');
}

// A file the box answers, read with the same sign-in on 403 as webuiV1: the
// answer, or the refusal thrown as webuiV1 throws it.
async function _webuiV1GetFile(path) {
	var get = function() { return webuiFetchBox(_webuiV1Url(path), {credentials: 'same-origin', cache: 'no-store'}); };
	var resp = await get();
	if (await _webuiV1ChangeOwed(resp))
		throw await _webuiV1Error(resp, 'GET', path);
	var declined = false;
	if (resp.status === 403) {
		if (await webuiV1Session() || await webuiV1SignIn())
			resp = await get();
		else
			declined = true;
	}
	if (!resp.ok) throw _webuiV1Declined(await _webuiV1Error(resp, 'GET', path), declined);
	return resp;
}

// A file the box answers as a download, saved under the name the box gives it
// (Content-Disposition), else under fallback; the same sign-in on 403 as
// webuiV1. Resolves to the name it was saved under.
async function webuiV1Download(path, fallback) {
	var resp = await _webuiV1GetFile(path);
	var name = _webuiV1FileName(resp.headers.get('Content-Disposition') || '') || fallback;
	var blob = await resp.blob();
	var a = document.createElement('a');
	a.href = URL.createObjectURL(blob);
	a.download = name;
	document.body.appendChild(a);
	a.click();
	a.remove();
	setTimeout(function() { URL.revokeObjectURL(a.href); }, 1000);
	return name;
}

// Pieces an upload goes up in: well below the server's body ceiling of 1 MiB,
// and small enough that one arrives within the 10 s the server gives a body
// over a slow link (some 26 KB/s).
var WEBUI_V1_PIECE = 256 * 1024;

// How long the box holds a file to be, -1 when it cannot say
async function _webuiV1Length(path) {
	var at = path.lastIndexOf('/');
	var listed = await webuiV1('GET', '/storage/files', {query: {path: path.substring(0, at) || '/'}});
	var name = path.substring(at + 1);
	var e = ((listed && listed.items) || []).find(function(x) { return x.id === name; });
	return e && e.attributes_read ? e.size : -1;
}

/* A file of any size up to the box, at path inside the box's roots: the first
   piece with PUT, which refuses a name in use, each further one with PATCH at
   its offset. A piece is sent once more after a broken line; one that reached
   the box although its answer did not is found by the file's length, so it is
   neither lost nor written twice. onProgress(sent, total) after each piece. An
   upload that fails takes away what it wrote; a first piece refused because the
   name is taken wrote nothing, and what is there is somebody else's. */
async function webuiV1UploadPieces(file, path, onProgress) {
	var send = function(offset, piece, again) {
		return offset === 0
			? webuiV1('PUT', '/storage/file', {query: {path: path, overwrite: again ? true : null}, raw: piece})
			: webuiV1('PATCH', '/storage/file', {query: {path: path, offset: offset}, raw: piece});
	};
	var wrote = false;
	try {
		var offset = 0;
		do {
			var piece = file.slice(offset, offset + WEBUI_V1_PIECE);
			try {
				await send(offset, piece, false);
			} catch (e) {
				if (e.status) throw e;	// the box answered: a refusal, not a broken line
				await new Promise(function(r) { setTimeout(r, 1000); });
				try {
					await send(offset, piece, true);
				} catch (e2) {
					if (e2.code !== 'offset-mismatch' || await _webuiV1Length(path) !== offset + piece.size) throw e2;
				}
			}
			wrote = true;
			offset += piece.size;
			if (onProgress) onProgress(offset, file.size);
		} while (offset < file.size);
	} catch (e) {
		if (wrote)
			webuiV1('DELETE', '/storage/path', {query: {path: path}}).catch(function() {});
		throw e;
	}
}

// Every page of a paged list (items + next_cursor), as one array; opts
// as webuiV1's ({once: true}: a read of its own that asks for no sign-in).
async function webuiV1All(path, query, opts) {
	var items = [];
	var q = Object.assign({}, query || {}, {limit: 500});
	for (;;) {
		var page = await webuiV1('GET', path, Object.assign({}, opts, {query: q}));
		items = items.concat((page && page.items) || []);
		if (!page || !page.next_cursor) return items;
		q.cursor = page.next_cursor;
	}
}

/* Whether the box has a route, as its description of the interface (GET
   /api/openapi.json, read once per page on the first ask, without a sign-in)
   lists it, so a page offers a route only on a box that has it. path is the
   route below /api/v1 as the description writes it ('/playback/stop',
   '/recordings/archive/{id}/play'). A box without a description, or with
   one that does not list the route, has it not, and neither has one whose
   route answered 404 or 405 (webuiV1Gone). webuiV1Offers resolves true or
   false; webuiV1OffersNow says it at once, null while the description is
   still on its way. A read that got no answer, another refusal than 404 or
   a body that does not parse is made again at the next ask. */
var _webuiV1Routes = null;	// 'METHOD /api/v1/...' -> true, false for a box without a description
var _webuiV1RoutesRead = null;
var _webuiV1Gone = Object.create(null);

function _webuiV1RouteKey(method, path) {
	return method.toUpperCase() + ' /api/v1' + path;
}

function webuiV1Offers(method, path) {
	if (!_webuiV1RoutesRead) {
		_webuiV1RoutesRead = webuiFetchBox('/api/openapi.json', {credentials: 'same-origin'}).then(function(resp) {
			if (resp.status === 404)
				return null;
			if (!resp.ok)
				throw new Error('HTTP ' + resp.status);
			return resp.json();
		}).then(function(doc) {
			var paths = doc && doc.paths;
			if (!paths || typeof paths !== 'object') {
				_webuiV1Routes = false;
				return;
			}
			var routes = Object.create(null);
			Object.keys(paths).forEach(function(p) {
				Object.keys(paths[p] || {}).forEach(function(m) { routes[m.toUpperCase() + ' ' + p] = true; });
			});
			_webuiV1Routes = routes;
		}, function() {
			_webuiV1RoutesRead = null;
		});
	}
	return _webuiV1RoutesRead.then(function() { return webuiV1OffersNow(method, path) === true; });
}

function webuiV1OffersNow(method, path) {
	var key = _webuiV1RouteKey(method, path);
	if (_webuiV1Gone[key])
		return false;
	if (_webuiV1Routes === null)
		return null;
	return !!(_webuiV1Routes && _webuiV1Routes[key]);
}

// A route that answered 404 or 405: the box has it not, whatever its description says
function webuiV1Gone(method, path) {
	_webuiV1Gone[_webuiV1RouteKey(method, path)] = true;
}

// The sign-in dialog; resolves true once signed in, false when cancelled.
// Requests refused at the same time share the one dialog.
// Whether the page's sign-in dialog is open
function webuiV1SignInOpen() {
	return !!_webuiV1SignInPending;
}

function webuiV1SignIn() {
	if (_webuiV1SignInPending) return _webuiV1SignInPending;
	_webuiV1SignInPending = _webuiV1SignInDialog().then(function(ok) {
		_webuiV1SignInPending = null;
		if (ok) _webuiV1SignedIn();
		return ok;
	});
	return _webuiV1SignInPending;
}

function _webuiV1SignInDialog() {
	return new Promise(function(resolve) {
		var before = document.activeElement;
		var backdrop = document.createElement('div');
		backdrop.className = 'webui-confirm-backdrop';
		backdrop.innerHTML =
			'<div class="webui-confirm-dialog" role="dialog" aria-modal="true" aria-labelledby="webui-v1-signin-title">' +
				'<h4 id="webui-v1-signin-title"></h4>' +
				'<p class="webui-v1-why"></p>' +
				'<label class="webui-prompt-label" for="webui-v1-user"></label>' +
				'<input type="text" class="webui-prompt-input" id="webui-v1-user" autocomplete="username" maxlength="255">' +
				'<label class="webui-prompt-label" for="webui-v1-pass"></label>' +
				'<input type="password" class="webui-prompt-input" id="webui-v1-pass" autocomplete="current-password" maxlength="255">' +
				'<p class="webui-v1-err" role="alert" hidden></p>' +
				'<p class="webui-v1-first"></p>' +
				'<div class="webui-confirm-actions">' +
					'<button type="button" class="confirm-cancel"></button>' +
					'<button type="button" class="confirm-ok"></button>' +
				'</div>' +
			'</div>';
		backdrop.querySelector('h4').textContent = webuiL('v1.signin_title', 'Sign in');
		backdrop.querySelector('.webui-v1-why').textContent =
			webuiL('v1.signin_why', 'The box asks for a sign-in for this.');
		// the sign-in page's sentence: where the first password is shown, never which
		backdrop.querySelector('.webui-v1-first').textContent =
			webuiL('login.first', 'First sign-in: with the user and password the TV shows (System settings → Network → Web server). You will then be asked for a password of your own.');
		var labels = backdrop.querySelectorAll('.webui-prompt-label');
		labels[0].textContent = webuiL('v1.user', 'User');
		labels[1].textContent = webuiL('v1.password', 'Password');
		var user = backdrop.querySelector('#webui-v1-user');
		var pass = backdrop.querySelector('#webui-v1-pass');
		var err = backdrop.querySelector('.webui-v1-err');
		var okBtn = backdrop.querySelector('.confirm-ok');
		var cancelBtn = backdrop.querySelector('.confirm-cancel');
		okBtn.textContent = webuiL('v1.signin', 'Sign in');
		cancelBtn.textContent = webuiL('msg.cancel', 'Cancel');

		var done = false;
		var letGo = null;
		function finish(result) {
			if (done) return;
			done = true;
			if (letGo) letGo();
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
			var why = webuiL('v1.signin_failed', 'Sign-in failed');
			try {
				var resp = await webuiFetchBox('/api/v1/login', {
					method: 'POST', credentials: 'same-origin', cache: 'no-store',
					headers: {'Content-Type': 'application/json'},
					body: JSON.stringify({user: user.value, password: pass.value})
				});
				if (resp.ok) {
					var s = await resp.json();
					if (s && s.authenticated && s.csrf && s.csrf_header) {
						_webuiV1Csrf = {header: s.csrf_header, token: s.csrf};
						// what was signed into can be ended from the header
						if (typeof webuiUpdateLogoutBtn === 'function')
							webuiUpdateLogoutBtn(true);
						finish(true);
						return;
					}
				} else if (resp.status !== 401) {
					// in the page's language where the header is there to say it
					var refusal = await _webuiV1Error(resp, 'POST', '/login');
					why = typeof webuiErrText === 'function' ? webuiErrText(refusal) : refusal.message;
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
		// in the order of the open dialogs (webui-header.js): after a press on
		// its text, which leaves the focus on the page, Escape still closes it
		// and Tab stays in it
		if (typeof _webuiDialogHold === 'function')
			letGo = _webuiDialogHold(backdrop.querySelector('.webui-confirm-dialog'), function() { finish(false); },
				function() { return [user, pass, cancelBtn, okBtn].filter(function(el) { return !el.disabled; }); });
		user.focus();
	});
}

// The legacy helpers (/api/v4, the EPG dialog, recording, streaming) take a
// channel id with all 16 hex digits; v1 writes one without leading zeros.
function webuiLegacyChannelId(id) {
	return ('0000000000000000' + String(id)).slice(-16);
}

// Sends a request that puts something on the television (a zap, a
// recording) and asks about the two refusals the user can lift: the movie
// player playing a file (stop_playback) and the box in standby (wake), then
// sends it again. texts names the sheet's title, its button and the two
// questions. Resolves true once the box took it, false when the user said
// no; any other refusal throws as webuiV1 does.
async function webuiV1Lifting(path, body, texts) {
	body = Object.assign({}, body);
	for (;;) {
		try {
			await webuiV1('POST', path, {body: body});
			return true;
		} catch (e) {
			if (e.status === 409 && e.code === 'playback-running' && !body.stop_playback) {
				if (!await _webuiV1Ask(texts.stop, texts.title, texts.ok))
					return false;
				body.stop_playback = true;
				continue;
			}
			if (e.status === 409 && e.code === 'box-in-standby' && !body.wake) {
				if (!await _webuiV1Ask(texts.wake, texts.title, texts.ok))
					return false;
				body.wake = true;
				continue;
			}
			throw e;
		}
	}
}

// Plays a channel through POST /api/v1/zap, asking as above.
function webuiV1Zap(channelId) {
	return webuiV1Lifting('/zap', {channel_id: channelId}, {
		title: webuiL('v1.zap_title', 'Switch channel'),
		ok: webuiL('v1.zap_ok', 'Switch'),
		stop: webuiL('v1.zap_stop_playback', 'The player is playing a file. Stop it and switch to the channel?'),
		wake: webuiL('v1.zap_wake', 'The box is in standby. Switch it on and play the channel?')
	});
}

// The questions of playing a recording on the TV (webuiV1Lifting)
function _webuiPlayTexts() {
	return {
		title: webuiL('media.ask_title', 'Play recording'),
		ok: webuiL('media.play', 'Play on TV'),
		stop: webuiL('media.ask_stop', 'The player is playing a file. Stop it and play this recording?'),
		wake: webuiL('media.ask_wake', 'The box is in standby. Switch it on and play this recording?')
	};
}

// Plays a finished recording on the TV through POST
// /api/v1/recordings/archive/{id}/play, asking as above, and says so.
async function webuiPlayRecording(id) {
	try {
		const ok = await webuiV1Lifting('/recordings/archive/' + encodeURIComponent(id) + '/play', {}, _webuiPlayTexts());
		if (ok) webuiShowToast(webuiL('media.played', 'Playback started'));
	} catch (e) {
		webuiToastError('', e);
	}
}

/* The same, said once the TV plays it: the 202 says no more than that the
   box took the request on. The button (opts.button, else the one pressed)
   stays busy from the 202 on; a playback event naming the recording
   confirms it, else a read of what plays after 2 s and, where that does
   not show it, after 6 s. Then "Playback started", or the warning that the
   box has not started it yet, or that this could not be checked when that
   last read fails or gives no answer within WEBUI_PLAY_CHECK; a disk that
   spins up may take longer, so a playback event naming it within
   WEBUI_PLAY_LATE after that still says "Playback started". A second call
   for the recording meanwhile waits for the first; one after the warning
   starts again. Resolves true once the TV plays it, false otherwise
   (declined, refused, not started in time). */
var WEBUI_PLAY_CHECKS = [2000, 6000];
var WEBUI_PLAY_CHECK = 10000;
var WEBUI_PLAY_LATE = 30000;
var _webuiPlays = Object.create(null);	// the starts under way or warned of, by recording id
var _webuiPlaysHeard = false;		// whether the starts follow the playback events yet

function webuiPlayRecordingConfirmed(id, opts) {
	opts = opts || {};
	const key = String(id);
	const was = _webuiPlays[key];
	if (was && !was.over)
		return was.done;
	if (was)
		_webuiPlayForget(was);
	const play = {id: key, btn: opts.button || webuiWaitButton(), over: false, marked: false, timers: []};
	play.done = new Promise(function(resolve) { play.resolve = resolve; });
	_webuiPlays[key] = play;
	if (!_webuiPlaysHeard) {
		_webuiPlaysHeard = true;
		webuiV1Watch('playback', function(d) {
			const ev = webuiPlaybackEvent(d);
			const p = ev && !ev.stopped && ev.source === 'recording' ? _webuiPlays[ev.id] : null;
			if (!p)
				return;
			if (!p.over) {
				_webuiPlayEnd(p, true);
			} else {
				// warned of already: it started late after all
				_webuiPlayForget(p);
				webuiShowToast(webuiL('media.played', 'Playback started'));
			}
		});
	}
	_webuiPlayRun(play);
	return play.done;
}

async function _webuiPlayRun(play) {
	try {
		if (!await webuiV1Lifting('/recordings/archive/' + encodeURIComponent(play.id) + '/play', {}, _webuiPlayTexts())) {
			_webuiPlayEnd(play, false, true);
			return;
		}
	} catch (e) {
		webuiToastError('', e);
		_webuiPlayEnd(play, false, true);
		return;
	}
	// the event may have come before the answer
	if (play.over)
		return;
	if (play.btn) {
		_webuiWaitMark(play.btn, true);
		play.marked = true;
	}
	WEBUI_PLAY_CHECKS.forEach(function(at, i) {
		play.timers.push(setTimeout(function() { _webuiPlayCheck(play, i === WEBUI_PLAY_CHECKS.length - 1); }, at));
	});
}

// The last read decides; not answering within WEBUI_PLAY_CHECK counts as
// a failed read
async function _webuiPlayCheck(play, last) {
	if (play.over)
		return;
	const read = _webuiV1Read('/playback').catch(function() { return null; });
	let limit = null;
	const doc = !last ? await read : await Promise.race([read,
		new Promise(function(done) { limit = setTimeout(function() { done(null); }, WEBUI_PLAY_CHECK); })]);
	clearTimeout(limit);
	if (play.over)
		return;
	const now = webuiPlaybackOf(doc);
	if (now && now.source === 'recording' && now.id === play.id) {
		_webuiPlayEnd(play, true);
	} else if (last) {
		webuiShowToast(doc ? webuiL('media.not_played', 'The box has not started the playback yet.')
			: webuiL('media.play_unchecked', 'Could not check whether the playback started.'), 'warn');
		_webuiPlayEnd(play, false, true, true);
	}
}

// late: warned of, and still listened for until WEBUI_PLAY_LATE has passed
function _webuiPlayEnd(play, ok, quiet, late) {
	if (play.over)
		return;
	play.over = true;
	play.timers.forEach(clearTimeout);
	play.timers = [];
	if (late)
		play.timers.push(setTimeout(function() { _webuiPlayForget(play); }, WEBUI_PLAY_LATE));
	else if (_webuiPlays[play.id] === play)
		delete _webuiPlays[play.id];
	if (play.marked)
		_webuiWaitMark(play.btn, false);
	play.marked = false;
	if (ok && !quiet)
		webuiShowToast(webuiL('media.played', 'Playback started'));
	play.resolve(ok);
}

function _webuiPlayForget(play) {
	play.timers.forEach(clearTimeout);
	play.timers = [];
	if (_webuiPlays[play.id] === play)
		delete _webuiPlays[play.id];
}

/* What the TV's player plays, as GET /api/v1/playback says it, while it
   plays a recording or a file: {source, id, title}, with the id for a
   recording; null for live television, its timeshift, standby and a player
   that has just ended. */
function webuiPlaybackOf(doc) {
	if (!doc || (doc.source !== 'recording' && doc.source !== 'file') || doc.state === 'stopped')
		return null;
	if (doc.source === 'recording') {
		const r = doc.recording || {};
		return {source: 'recording', id: r.id || '', title: r.title || r.channel || ''};
	}
	const f = doc.file || {};
	return {source: 'file', id: '', title: f.title || f.name || ''};
}

/* The same from a playback event, whose text names the player's state,
   what it plays and, for a recording, its id ("playing recording <id>",
   "paused file", "stopped recording <id>"): {stopped, source, id}; null for
   a text of another form. */
function webuiPlaybackEvent(data) {
	const m = /^(\S+) (\S+)(?: (\S+))?/.exec((data && data.text) || '');
	return m ? {stopped: m[1] === 'stopped', source: m[2], id: m[3] || ''} : null;
}

// Why Play on TV waits: what the TV plays, a recording by its title
function webuiPlayLockText(playing) {
	if (playing.source === 'file')
		return webuiL('media.lock_file', 'A file is playing on the TV. Stop it first.');
	if (playing.title)
		return webuiL('media.lock', '“%s” is playing on the TV. Stop it first.').replace('%s', playing.title);
	return webuiL('media.lock_recording', 'A recording is playing on the TV. Stop it first.');
}

/* Play on TV held back while the TV plays something else: marked
   aria-disabled, the reason its description and its tooltip, and still
   focusable, so a press can say why (the caller says it and starts
   nothing). An empty reason frees it again; tip is its tooltip when free,
   none where it has its word. One element of the page holds the reason. */
function webuiPlayLock(btn, reason, tip) {
	if (reason) {
		let note = document.getElementById('webuiPlayLock');
		if (!note) {
			note = document.createElement('span');
			note.id = 'webuiPlayLock';
			note.className = 'webui-visually-hidden';
			document.body.appendChild(note);
		}
		if (note.textContent !== reason)
			note.textContent = reason;
		btn.setAttribute('aria-disabled', 'true');
		btn.setAttribute('aria-describedby', note.id);
		btn.title = reason;
	} else {
		btn.removeAttribute('aria-disabled');
		btn.removeAttribute('aria-describedby');
		if (tip)
			btn.title = tip;
		else
			btn.removeAttribute('title');
	}
}

// A yes or no in a sheet (webui-dom.js); without the helper layer the
// browser's own confirm.
function _webuiV1Ask(text, title, ok) {
	if (typeof webuiSheet !== 'function')
		return Promise.resolve(window.confirm(text));
	return new Promise(function(resolve) {
		webuiSheet({
			title: title,
			body: webuiEl('p', {}, text),
			actions: [
				{label: webuiL('msg.cancel', 'Cancel'), value: false},
				{label: ok, value: true, primary: true}
			],
			onClose: function(result) { resolve(result === true); }
		});
	});
}

// ---- the event stream ---------------------------------------------------

// The waits before a refused stream is asked for again: they grow with each
// refusal in a row and start small again once a stream stayed open a minute.
var WEBUI_V1_WAITS = [15000, 30000, 60000, 120000];
var WEBUI_V1_STAYED = 60000;
// A reopen of the page's own that has not opened by then leaves the reads
// it holds back to the page again ('late').
var WEBUI_V1_BACK = 3000;
// How long a first read of webuiFollow waits for the page's stream to open
// or be refused: a read made once the box has taken the stream on cannot
// miss a change made before that.
var WEBUI_V1_SETUP = 1000;
// Events of the stream itself, told by this file, not sent by the box.
var WEBUI_V1_OWN = {drop: true, late: true, refused: true};

/* The stream's state: state says what it does now ('connecting', 'open',
   'asking' why it was refused, 'waiting' to ask again, 'paused' while the
   page is hidden, 'refused' until a sign-in, 'off' for good); off is set
   once the box said it carries no streams. gap: the next opening comes back
   after a refusal, a failure or a pause, which reconnect says to the
   handlers of that opening. closed: the last EventSource failed for good. */
function _webuiV1NewStream() {
	return {on: [], es: null, state: 'connecting', off: false, closed: false, gap: false,
		reconnect: false, opens: 0, wasOpen: false, step: 0, asked: 0, timer: null,
		back: null, stayed: null, reopening: false};
}

// A wait spread by 30 to 50 %
function _webuiV1Spread(ms) {
	return Math.round(ms * (1.3 + Math.random() * 0.2));
}

// The page's one EventSource; handlers keyed by event name. A second call
// adds its handlers to the same stream. A page loaded out of view opens it
// once it is shown.
function webuiV1Events(handlers) {
	if (!window.EventSource) {
		_webuiV1SetUpDone();
		return null;
	}
	var stream = _webuiV1Stream;
	var fresh = !stream;
	if (fresh)
		stream = _webuiV1Stream = _webuiV1NewStream();
	Object.keys(handlers || {}).forEach(function(name) {
		stream.on.push([name, handlers[name]]);
		if (!fresh && stream.es)
			_webuiV1Listen(stream.es, name, handlers[name]);
	});
	if (fresh) {
		if (document.hidden) {
			stream.state = 'paused';
			stream.gap = true;
			_webuiV1SetUpDone();
		} else {
			_webuiV1Open();
		}
	}
	return stream.es;
}

function webuiV1EventsLive() {
	return !!(window.EventSource && _webuiV1Stream && _webuiV1Stream.es &&
		_webuiV1Stream.es.readyState === EventSource.OPEN);
}

function _webuiV1Listen(es, name, fn) {
	if (WEBUI_V1_OWN[name])
		return;
	es.addEventListener(name, function(e) {
		var data = null;
		try { data = JSON.parse(e.data); } catch (ignore) { /* keep null */ }
		fn(data);
	});
}

// webuiV1Watch(name, fn): fn also gets the event `name` on whatever stream
// a page opens, beside the page's own handler of it (the head follows the
// timers this way), from the stream there now on.
function webuiV1Watch(name, fn) {
	_webuiV1Watchers.push([name, fn]);
	if (_webuiV1Stream && _webuiV1Stream.es)
		_webuiV1Listen(_webuiV1Stream.es, name, fn);
}

// An event of the stream itself to whoever listens for it by that name
function _webuiV1Tell(name) {
	var stream = _webuiV1Stream;
	(stream ? stream.on : []).concat(_webuiV1Watchers).forEach(function(h) {
		if (h[0] !== name)
			return;
		try {
			h[1](null);
		} catch (e) {
			console.error(e);
		}
	});
}

/* Opens the stream unless one is open or opening: a wait running out, a
   sign-in and the page coming into view all come here, so there is never
   more than one. A new EventSource after an earlier one reconnects. */
function _webuiV1Open() {
	var stream = _webuiV1Stream;
	if (!stream)
		return null;
	clearTimeout(stream.timer);
	stream.timer = null;
	var old = stream.es;
	if (old && old.readyState !== EventSource.CLOSED)
		return old;
	if (old)
		stream.gap = true;
	var es = new EventSource('/api/v1/events');
	stream.es = es;
	stream.closed = false;
	stream.wasOpen = false;
	stream.state = 'connecting';
	// first, so the page's handlers of an opening find whether it reconnects
	es.addEventListener('open', function() { if (stream.es === es) _webuiV1Opened(stream, es); });
	stream.on.forEach(function(h) { _webuiV1Listen(es, h[0], h[1]); });
	_webuiV1Watchers.forEach(function(w) { _webuiV1Listen(es, w[0], w[1]); });
	es.addEventListener('error', function() { if (stream.es === es) _webuiV1Failed(stream, es); });
	return es;
}

// An EventSource opens once more only after it lost its connection, so a
// later opening of the same one reconnects even where no error was seen.
function _webuiV1Opened(stream, es) {
	stream.reconnect = stream.gap || !!es._webuiOpened;
	es._webuiOpened = true;
	stream.gap = false;
	stream.opens++;
	stream.wasOpen = true;
	stream.state = 'open';
	_webuiV1BackDone(stream);
	clearTimeout(stream.stayed);
	stream.stayed = setTimeout(function() { stream.step = 0; }, WEBUI_V1_STAYED);
	_webuiV1Settled = true;
	_webuiV1SetUpDone();
}

/* A stream that was open and is lost is a drop ('drop': the box went away,
   or restarts). The browser comes back by itself from a broken line; a
   refusal it gives up on, and the box is asked why. */
function _webuiV1Failed(stream, es) {
	stream.gap = true;
	clearTimeout(stream.stayed);
	if (stream.wasOpen) {
		stream.wasOpen = false;
		_webuiV1Tell('drop');
	}
	if (es.readyState !== EventSource.CLOSED) {
		stream.state = 'connecting';
		return;
	}
	stream.closed = true;
	_webuiV1Settled = true;
	_webuiV1SetUpDone();
	if (stream.off)
		stream.state = 'off';
	else
		_webuiV1Probe(stream);
}

/* Why the box refused: the stream's own GET, let go as soon as its head is
   in (a stream it hands out would hold one of the box's places). Asked once
   per refusal, and never once the box said it carries no streams. */
function _webuiV1Probe(stream) {
	if (stream.off || stream.state === 'asking')
		return;
	stream.state = 'asking';
	stream.asked++;
	var ctl = typeof AbortController === 'function' ? new AbortController() : null;
	var limit = ctl ? setTimeout(function() { ctl.abort(); }, 10000) : null;
	webuiFetchBox('/api/v1/events', {credentials: 'same-origin', cache: 'no-store', signal: ctl ? ctl.signal : undefined})
		.then(function(resp) {
			var got = {status: resp.status, after: resp.headers.get('Retry-After')};
			if (ctl)
				ctl.abort();
			return got;
		}, function() {
			return {status: 0, after: null};
		})
		.then(function(got) {
			clearTimeout(limit);
			_webuiV1Judge(stream, got);
		});
}

/* 501: the box carries no streams, none until the page loads anew, and the
   page lives on its reads; 401 and 403 wait for a sign-in; 503 for its
   Retry-After; anything else, a stream the box handed out that failed in
   the browser among it, for a wait that grows. */
function _webuiV1Judge(stream, got) {
	if (stream.es && stream.es.readyState !== EventSource.CLOSED)
		return;
	var s = got.status;
	if (s === 501) {
		stream.off = true;
		stream.state = 'off';
		return;
	}
	if (s === 401 || s === 403) {
		stream.state = 'refused';
		_webuiV1Tell('refused');
		return;
	}
	var after = s === 503 ? parseInt(got.after, 10) : NaN;
	if (after > 0) {
		_webuiV1Later(stream, _webuiV1Spread(after * 1000));
		return;
	}
	var wait = WEBUI_V1_WAITS[Math.min(stream.step, WEBUI_V1_WAITS.length - 1)];
	stream.step++;
	_webuiV1Later(stream, _webuiV1Spread(wait));
}

function _webuiV1Later(stream, ms) {
	clearTimeout(stream.timer);
	stream.timer = null;
	if (document.hidden) {
		stream.state = 'paused';
		return;
	}
	stream.state = 'waiting';
	stream.timer = setTimeout(function() {
		stream.timer = null;
		_webuiV1Reopen(stream);
	}, ms);
}

/* A reopen of the page's own (a wait run out, a sign-in, the page shown
   again): its opening reads what the page shows (webuiFollow), and the
   reads the page would make without a stream wait for it, up to
   WEBUI_V1_BACK. */
function _webuiV1Reopen(stream) {
	if (document.hidden) {
		clearTimeout(stream.timer);
		stream.timer = null;
		stream.state = 'paused';
		return;
	}
	stream.reopening = true;
	clearTimeout(stream.back);
	stream.back = setTimeout(function() {
		_webuiV1BackDone(stream);
		_webuiV1Tell('late');
	}, WEBUI_V1_BACK);
	_webuiV1Open();
}

function _webuiV1BackDone(stream) {
	stream.reopening = false;
	clearTimeout(stream.back);
	stream.back = null;
}

// Whether a reopen of the page's own is under way
function _webuiV1Reopening() {
	return !!(_webuiV1Stream && _webuiV1Stream.reopening);
}

// A sign-in went through: a stream the box refused is asked for again
function _webuiV1SignedIn() {
	var stream = _webuiV1Stream;
	if (stream && stream.closed && !stream.off)
		_webuiV1Reopen(stream);
}

/* A page out of view lets its stream go at once, as a timer started then
   may never run on a phone, and a hidden tab would keep one of the few
   streams the box and the browser allow; shown again, it takes it up. */
function _webuiV1Release(stream) {
	if (stream.off || stream.state === 'refused')
		return;
	clearTimeout(stream.timer);
	stream.timer = null;
	_webuiV1BackDone(stream);
	clearTimeout(stream.stayed);
	if (stream.es && stream.es.readyState !== EventSource.CLOSED) {
		stream.es.close();
		stream.wasOpen = false;
	}
	stream.gap = true;
	stream.state = 'paused';
}

function _webuiV1Shown() {
	var stream = _webuiV1Stream;
	if (!stream)
		return;
	if (document.hidden)
		_webuiV1Release(stream);
	else if (stream.state === 'paused')
		_webuiV1Reopen(stream);
}

// before any part of a page follows the box, so a reopen is under way when
// the parts hear that the page is shown again
document.addEventListener('visibilitychange', _webuiV1Shown);
window.addEventListener('pagehide', function() { if (_webuiV1Stream) _webuiV1Release(_webuiV1Stream); });
window.addEventListener('pageshow', function(e) { if (e.persisted) _webuiV1Shown(); });

/* The first reads of webuiFollow, which wait until the page's stream has
   opened or been refused, or WEBUI_V1_SETUP has passed: a page without a
   stream, or one whose stream hangs, reads a moment later. _webuiV1Settled:
   the stream has opened or been refused once. */
var _webuiV1SetUpWaits = null;
var _webuiV1SetUpOver = false;
var _webuiV1Settled = false;

function _webuiV1WhenSetUp(fn) {
	if (_webuiV1SetUpOver || !window.EventSource) {
		fn();
		return;
	}
	if (!_webuiV1SetUpWaits) {
		_webuiV1SetUpWaits = [];
		setTimeout(_webuiV1SetUpDone, WEBUI_V1_SETUP);
	}
	_webuiV1SetUpWaits.push(fn);
}

function _webuiV1SetUpDone() {
	_webuiV1SetUpOver = true;
	var waits = _webuiV1SetUpWaits || [];
	_webuiV1SetUpWaits = null;
	waits.forEach(function(fn) {
		try {
			fn();
		} catch (e) {
			console.error(e);
		}
	});
}

/* webuiFollow({reread, events, every, fallback, first}): what a part of
   the page shows of the box, kept current.
   - reread() once each time the page's stream comes back after a gap (a
     refusal, a failure, the page hidden) and after each of the events
     named;
   - without a live stream (none on the page, or the box refused it):
     fallback(), else reread(), when the page comes into view and every
     `every` ms while it is shown, though not while a reopen of the stream
     is under way, whose opening reads; nothing while the page is hidden;
   - the first read, first() or else reread(), once the page's stream has
     opened or been refused (or after WEBUI_V1_SETUP: a page without a
     stream, or one whose stream hangs). A first read made before that is
     made once more when the stream first opens. first: false leaves the
     first read to the page;
   - while the part's last read failed (it resolved false or threw),
     fallback() every `every` ms while the page is shown, with a live
     stream as well, until one answers. A read the box refuses for good,
     or whose sign-in the user declined, is better not reported as failed:
     it would be made again every `every` ms;
   - no fallback() while a read of the part is under way, with or without
     a stream; one that has not answered within WEBUI_V1_FOLLOW_WAIT is
     taken as failed and no longer as under way, so a box that holds a
     request open gets at most one more per `every` ms.
   It opens no stream: it follows the one the page opens with
   webuiV1Events, through webuiV1Watch. */
var WEBUI_V1_FOLLOW_WAIT = 10000;
function webuiFollow(opts) {
	var f = {before: false, failed: false, made: 0, busy: 0};
	f.reread = function() { return _webuiV1FollowRead(f, opts.reread); };
	f.fallback = function() { return _webuiV1FollowRead(f, opts.fallback || opts.reread); };
	(opts.events || []).forEach(function(name) {
		webuiV1Watch(name, function() { f.reread(); });
	});
	webuiV1Watch('open', function() { _webuiV1FollowOpen(f); });
	webuiV1Watch('late', function() { _webuiV1FollowBack(f); });
	document.addEventListener('visibilitychange', function() { _webuiV1FollowBack(f); });
	if (opts.every)
		setInterval(function() { _webuiV1FollowBack(f, true); }, opts.every);
	var first = opts.first === false ? null : opts.first || opts.reread;
	if (first)
		_webuiV1WhenSetUp(function() {
			f.before = !_webuiV1Settled && !!window.EventSource;
			_webuiV1FollowRead(f, first);
		});
	return f;
}

// One read of the part; unless a later one was made meanwhile, its outcome
// is whether the part's last read failed, and not answering within
// WEBUI_V1_FOLLOW_WAIT counts as failing (an answer after that still counts)
function _webuiV1FollowRead(f, read) {
	var n = ++f.made;
	var under = true;
	f.busy++;
	var limit = setTimeout(function() {
		under = false;
		f.busy--;
		if (n === f.made)
			f.failed = true;
	}, WEBUI_V1_FOLLOW_WAIT);
	var made;
	try {
		made = read();
	} catch (e) {
		console.error(e);
		made = false;
	}
	Promise.resolve(made).then(function(v) { return v !== false; }, function() { return false; }).then(function(ok) {
		clearTimeout(limit);
		if (under) {
			under = false;
			f.busy--;
		}
		if (n === f.made)
			f.failed = !ok;
	});
	return made;
}

function _webuiV1FollowOpen(f) {
	if ((_webuiV1Stream && _webuiV1Stream.reconnect) || f.before) {
		f.before = false;
		f.reread();
	}
}

// Without a live stream when the page comes into view and on the part's
// timer; on the timer with a live stream too while the last read failed;
// never while a read of the part is under way
function _webuiV1FollowBack(f, timer) {
	if (document.hidden || _webuiV1Reopening() || f.busy)
		return;
	if (!webuiV1EventsLive() || (timer && f.failed))
		f.fallback();
}

// ---- EPG dialog, recording and streaming over /api/v1 -------------------
// Shared by the dashboard, the channel list, the guide and the timer list.
// Channel ids are v1's: hexadecimal without leading zeros.

/* Whether the box hands channel logos out at all, asked once with the
   first channel a page draws: a list of hundreds would otherwise ask for
   each in vain. Unknown (null) until asked; a failed ask is asked again. */
let _webuiLogos = null;
async function webuiLogosOffered(channelId) {
	if (_webuiLogos !== null || !channelId)
		return _webuiLogos !== false;
	try {
		const r = await webuiFetchBox('/api/v1/channels/' + encodeURIComponent(channelId) + '/logo', {credentials: 'same-origin'});
		// a channel without a logo is a 404 as well, but not this one
		const p = r.status === 404 ? await r.json().catch(() => null) : null;
		_webuiLogos = !(p && p.type === '/errors/logos-not-offered');
	} catch (e) { /* asked again next time */ }
	return _webuiLogos !== false;
}

/* A channel's logo as the pages draw it: a tile holding the picture,
   loaded as it comes into view, or the text given where there is none
   (the box hands none out, or has none for the channel). className adds
   the size a place wants. */
function webuiChannelLogo(channelId, text, className) {
	const tile = document.createElement('span');
	tile.className = 'webui-channel-logo' + (className ? ' ' + className : '');
	tile.setAttribute('aria-hidden', 'true');
	const none = function() {
		tile.replaceChildren(document.createTextNode(text || ''));
		tile.classList.add('no-logo');
	};
	if (_webuiLogos === false || !channelId) {
		none();
		return tile;
	}
	const img = document.createElement('img');
	img.alt = '';
	img.loading = 'lazy';
	img.onerror = none;
	img.src = '/api/v1/channels/' + encodeURIComponent(channelId) + '/logo';
	tile.appendChild(img);
	return tile;
}

// The dialog, built on its first opening, once the texts are in: a head
// tinted with the accent with the channel, the programme's time, its title
// and short text and how far it has run; then its long text and what
// follows on the channel; the actions at its foot. The channel and the
// title name the dialog.
function _webuiEpgModalHTML() {
	return `
<div class="webui-epg-modal-backdrop" id="webuiEpgModal" onclick="webuiCloseEpgModal(event)">
	<div class="webui-epg-modal webui-epg-show" onclick="event.stopPropagation()">
		<header class="webui-epg-hero">
			<div class="webui-epg-modal-channel">
				<span id="webuiEpgModalLogo"></span>
				<div class="webui-epg-modal-channel-text">
					<b id="webuiEpgModalChannel">--</b>
					<span class="webui-epg-modal-number" id="webuiEpgModalNumber"></span>
				</div>
			</div>
			${_webuiCloseBtn('webuiCloseEpgModal()')}
			<p class="webui-epg-time" id="webuiEpgCurrentTime">--:-- – --:--</p>
			<h2 class="webui-epg-title" id="webuiEpgCurrentTitle">${webuiLH('msg.loading', 'Loading...')}</h2>
			<p class="webui-epg-sub" id="webuiEpgSub" hidden></p>
			<div class="webui-epg-run" id="webuiEpgRun" hidden>
				<div class="webui-epg-progress" id="webuiEpgProgress" aria-hidden="true">
					<div class="webui-epg-progress-bar" id="webuiEpgProgressBar" style="width: 0%"></div>
				</div>
				<span id="webuiEpgRunText"></span>
			</div>
		</header>
		<div class="webui-epg-modal-body">
			<div id="webuiEpgTimerMarks"></div>
			<div class="webui-epg-description" id="webuiEpgCurrentDesc"></div>
			<section class="webui-epg-next" id="webuiEpgNextSection" hidden>
				<h3 class="webui-epg-next-label" id="webuiEpgNextLabel">${webuiLH('epg.after', 'Next')}</h3>
				<ul class="webui-epg-next-list" id="webuiEpgNextList"></ul>
			</section>
		</div>
		<footer class="webui-epg-modal-footer">
			<p class="webui-epg-play-note" id="webuiEpgPlayNote" hidden></p>
			<button type="button" class="webui-btn primary" onclick="webuiEpgModalZap()" title="${webuiLH('epg.zap_title', 'Switch to this channel now')}">
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="2" y="7" width="20" height="15" rx="2"/><polyline points="17 2 12 7 7 2"/></svg>
				<span>${webuiLH('epg.zap', 'Zap')}</span>
			</button>
			<button type="button" class="webui-btn webui-epg-rec-btn" id="webuiEpgRecBtn" onclick="webuiEpgModalToggleRec()" title="${webuiLH('epg.timer_rec_title', 'Set a recording timer')}">
				<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><circle cx="12" cy="12" r="7"/></svg>
				<span>${webuiLH('rec.start', 'Record')}</span>
			</button>
			<button type="button" class="webui-btn" id="webuiEpgZapTimerBtn" onclick="webuiEpgModalZapTimer()" title="${webuiLH('epg.timer_zap_title', 'Set a zap timer')}" hidden>
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M18 8a6 6 0 0 0-12 0c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.7 21a2 2 0 0 1-3.4 0"/></svg>
				<span>${webuiLH('epg.timer_zap', 'Zap timer')}</span>
			</button>
			<button type="button" class="webui-btn" id="webuiEpgPlayBtn" onclick="webuiPlayInBrowser()">
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="2" y="4" width="20" height="14" rx="2"/><path d="M10 8.5v5l4.5-2.5z"/><path d="M8 21h8"/></svg>
				<span>${webuiLH('play.in_browser', 'Play in browser')}</span>
			</button>
			<span class="webui-epg-ways">
			<button type="button" class="webui-epg-round labelled" id="webuiEpgM3uBtn" onclick="webuiOpenInPlayer()" title="${webuiLH('play.external', 'Open in a player (M3U)')}" aria-label="${webuiLH('play.external', 'Open in a player (M3U)')}">
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 17H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/><polygon points="12 15 17 21 7 21"/></svg>
				<span>${webuiLH('play.external_short', 'M3U')}</span>
			</button>
			<button type="button" class="webui-epg-round labelled" id="webuiEpgCopyBtn" onclick="webuiCopyChannelAddress()" title="${webuiLH('play.copy', 'Copy address')}" aria-label="${webuiLH('play.copy', 'Copy address')}">
				${WEBUI_COPY_SVG}
				<span>${webuiLH('play.copy_short', 'Copy')}</span>
			</button>
			</span>
		</footer>
	</div>
</div>
`;
}

// "Today", "Tomorrow" or the weekday and date of a time in seconds, and the
// programme's day, time and length in one line
function _webuiEpgDay(sec) {
	const d = new Date(sec * 1000), now = new Date(webuiBoxNow());
	const day = function(x) { return new Date(x.getFullYear(), x.getMonth(), x.getDate()).getTime(); };
	const diff = Math.round((day(d) - day(now)) / 86400000);
	if (diff === 0) return webuiL('epg.today', 'Today');
	if (diff === 1) return webuiL('epg.tomorrow', 'Tomorrow');
	return d.toLocaleDateString(document.documentElement.lang || undefined, {weekday: 'short', day: '2-digit', month: '2-digit'});
}

function _webuiEpgMinutes(sec) {
	return webuiL('epg.minutes', '%s min').replace('%s', Math.max(1, Math.round(sec / 60)));
}

function _webuiEpgWhen(ev) {
	return [_webuiEpgDay(ev.start), webuiClock(ev.start) + ' – ' + webuiClock(ev.start + ev.duration),
		ev.duration > 0 ? _webuiEpgMinutes(ev.duration) : ''].filter(Boolean).join(' · ');
}

// The channel and the guide event the dialog shows, for its buttons
let webuiEpgModalChannelId = null;
let webuiEpgModalChannelName = null;
let _webuiEpgModalKind = '';
let webuiEpgModalEvent = null;
let webuiEpgModalIsRecording = false;
let _webuiEpgModalOnTimer = null;
let _webuiEpgModalToken = 0;


/* The names of channels by their v1 ids, for the lists that hold ids alone
   (timers, recordings): each asked of the box once and kept for the page,
   also while two reads ask at once; '' for a channel the box does not
   know. The overview and the timer list share them. A timer without a
   channel (a reminder, a sleep timer) carries the id 0, which names none. */
const webuiNames = Object.create(null);
const _webuiNaming = Object.create(null);
async function webuiEnsureNames(ids) {
	await Promise.all(ids.map(function(id) {
		if (!id || id === '0' || id in webuiNames) return null;
		if (!_webuiNaming[id]) {
			_webuiNaming[id] = webuiV1('GET', '/channels/' + encodeURIComponent(id)).catch(function() { return null; }).then(function(ch) {
				webuiNames[id] = (ch && ch.name) || '';
				delete _webuiNaming[id];
			});
		}
		return _webuiNaming[id];
	}));
}

// A guide read that finds nothing is an answer, not a failure
function _webuiV1Maybe(path, query) {
	return webuiV1('GET', path, {query: query}).catch(function(e) {
		if (e.status === 404) return null;
		throw e;
	});
}

// An EPG identity excludes safety margins from neighbouring programmes.
// Manual timers and events without an identity use their overlapping times.
function webuiTimersForEvent(items, channelId, ev) {
	if (!ev) return [];
	return items.filter(function(t) {
		if (t.channel_id !== channelId || t.state >= 3 ||
			!['record', 'immediate-record', 'zapto'].includes(t.kind)) return false;
		if (t.epg_id && t.epg_id !== '0' && t.epg_start && ev.id && ev.id !== '0')
			return String(t.epg_id) === String(ev.id) && Number(t.epg_start) === Number(ev.start);
		return t.start < ev.start + ev.duration && (t.stop > t.start ? t.stop > ev.start : t.start >= ev.start);
	});
}

/* The mark of a programme's timers, all taken from the timer list: a zap
   timer ('zap'), a recording scheduled ('record') or running ('record
   running', state 2). A zap timer is over once it switched (the daemon
   finishes it at once), so it marks nothing after that. compact leaves the
   symbol and puts the words where only a screen reader reads them. */
function webuiEventTimerMarkKind(items) {
	const record = items.filter(function(t) { return t.kind !== 'zapto'; });
	if (!record.length) return items.length ? 'zap' : '';
	return record.some(function(t) { return t.state === 2; }) ? 'record running' : 'record';
}

function webuiEventTimerMark(items, compact) {
	if (!items.length) return null;
	const label = items.map(function(t) {
		return t.kind === 'zapto' ? webuiL('epg.timer_zap_planned', 'Channel switch scheduled') :
			t.state === 2 ? webuiL('epg.timer_record_running', 'Recording timer running') :
			webuiL('epg.timer_record_planned', 'Recording scheduled');
	}).filter(function(s, i, all) { return all.indexOf(s) === i; });
	return webuiEl('span', {className: 'webui-pill webui-timer-mark ' + webuiEventTimerMarkKind(items)},
		webuiMarkup('<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/></svg>'),
		webuiEl('span', {className: compact ? 'webui-visually-hidden' : ''}, label.join(' · ')));
}

/* The dialog's Record says Stop recording while the box records its
   channel, for the programme running now and where the box has no guide
   for the channel. What the box records comes from the page's shared read
   (webuiOnRecordings in webui-header.js), so opening the dialog reads
   nothing; the dialog shown follows each read that comes back. */
let _webuiEpgRecShown = 0;	// the dialog (its token) whose Record follows the recordings
let _webuiEpgRecFollowed = false;

function _webuiEpgRecSeen(view) {
	const modal = document.getElementById('webuiEpgModal');
	if (!modal || !modal.classList.contains('open') || _webuiEpgRecShown !== _webuiEpgModalToken) return;
	if (webuiEpgModalEvent && !_webuiEpgRuns(webuiEpgModalEvent)) return;	// over since it opened
	webuiEpgModalUpdateRecBtn(view.items.some(function(it) {
		return it.channel_id === webuiEpgModalChannelId;
	}));
}

function _webuiEpgRecFollow() {
	_webuiEpgRecShown = _webuiEpgModalToken;
	if (_webuiEpgRecFollowed) {
		_webuiEpgRecSeen(webuiRecordingsNow());
	} else {
		_webuiEpgRecFollowed = true;
		webuiOnRecordings(_webuiEpgRecSeen);
	}
}

function webuiEpgTimerMarks() {
	const host = document.getElementById('webuiEpgTimerMarks');
	if (!host) return;
	const mark = webuiEventTimerMark(webuiTimersForEvent(webuiTimerItems, webuiEpgModalChannelId, webuiEpgModalEvent));
	host.replaceChildren.apply(host, mark ? [mark] : []);
	document.querySelectorAll('#webuiEpgNextList li').forEach(function(li) {
		const old = li.querySelector('.webui-timer-mark');
		if (old) old.remove();
		const next = webuiEventTimerMark(webuiTimersForEvent(webuiTimerItems, webuiEpgModalChannelId, li._webuiEvent));
		if (next) li.querySelector('span').appendChild(next);
	});
}
// the marks follow every timer list the page takes (webuiOnTimers)
webuiOnTimers(webuiEpgTimerMarks);

// whether an event runs now
function _webuiEpgRuns(ev) {
	const now = webuiBoxNow() / 1000;
	return !!ev && ev.start <= now && now < ev.start + ev.duration;
}

/* A programme of a channel: the event given (opts.event, a programme of the
   guide), else what runs now on the channel; its long text, what follows
   it, and the channel's logo and number (opts.number). opts.onTimer is
   called once a timer was set from the dialog. opts.kind, the channel's kind,
   offers playing a web channel in the page; where a caller does not say, the
   box is asked. */
async function webuiShowEpgModal(channelId, channelName, opts) {
	opts = opts || {};
	if (!document.getElementById('webuiEpgModal'))
		document.body.insertAdjacentHTML('beforeend', _webuiEpgModalHTML());

	const modal = document.getElementById('webuiEpgModal');
	const titleEl = document.getElementById('webuiEpgCurrentTitle');
	const timeEl = document.getElementById('webuiEpgCurrentTime');
	const subEl = document.getElementById('webuiEpgSub');
	const descEl = document.getElementById('webuiEpgCurrentDesc');
	const run = document.getElementById('webuiEpgRun');
	const progressBar = document.getElementById('webuiEpgProgressBar');
	const channelEl = document.getElementById('webuiEpgModalChannel');
	const numberEl = document.getElementById('webuiEpgModalNumber');
	const nextSection = document.getElementById('webuiEpgNextSection');
	const nextList = document.getElementById('webuiEpgNextList');
	const recBtn = document.getElementById('webuiEpgRecBtn');

	const token = ++_webuiEpgModalToken;
	webuiEpgModalChannelId = channelId;
	webuiEpgModalChannelName = channelName || webuiL('epg.programme', 'Programme');
	webuiEpgModalEvent = null;
	webuiEpgTimerMarks();
	_webuiEpgModalOnTimer = opts.onTimer || null;

	channelEl.textContent = webuiEpgModalChannelName;
	numberEl.textContent = opts.number > 0 ? webuiL('epg.number', 'Programme %s').replace('%s', opts.number) : '';
	// the logo once the box has said whether it hands logos out (asked
	// once a page); the dialog opens without waiting for that, an empty
	// tile of the logo's size keeping its place
	const logo = document.getElementById('webuiEpgModalLogo');
	logo.replaceChildren(webuiEl('span', {className: 'webui-channel-logo webui-epg-modal-logo', attrs: {'aria-hidden': 'true'}}));
	webuiLogosOffered(channelId).then(function() {
		if (token === _webuiEpgModalToken)
			logo.replaceChildren(webuiChannelLogo(channelId, webuiEpgModalChannelName.slice(0, 12), 'webui-epg-modal-logo'));
	});
	titleEl.textContent = webuiL('msg.loading', 'Loading...');
	timeEl.textContent = '--:-- – --:--';
	subEl.textContent = '';
	subEl.hidden = true;
	descEl.textContent = '';
	progressBar.style.width = '0%';
	run.hidden = true;
	nextSection.hidden = true;
	nextList.replaceChildren();
	document.getElementById('webuiEpgZapTimerBtn').hidden = true;
	recBtn.hidden = false;
	// handed to the player, which reads the channel where it is not known
	_webuiEpgModalKind = opts.kind || '';
	webuiPlayOffer(document.getElementById('webuiEpgPlayBtn'), channelId, webuiEpgModalChannelName,
		document.getElementById('webuiEpgPlayNote'), document.getElementById('webuiEpgM3uBtn'));

	modal.classList.add('open');
	document.body.style.overflow = 'hidden';
	if (modal._webuiRelease)
		modal._webuiRelease();
	modal._webuiRelease = webuiDialogA11y(modal.querySelector('.webui-epg-modal'), {close: function() { webuiCloseEpgModal(); }});
	// named by the channel and the programme, so it says whose it is
	// when the box has no guide for the channel
	modal.querySelector('.webui-epg-modal').setAttribute('aria-labelledby', 'webuiEpgModalChannel webuiEpgCurrentTitle');

	webuiEpgModalUpdateRecBtn(false);
	// a stop pressed in this dialog belongs to the channel it showed then
	webuiStopShownFor(recBtn, channelId);

	try {
		const ev = opts.event || await _webuiV1Maybe('/epg/current', {channel: channelId});
		if (token !== _webuiEpgModalToken) return;	// opened for another programme meanwhile
		if (!ev) {
			titleEl.textContent = webuiL('epg.nodata', 'No EPG data available');
			timeEl.textContent = '';
			_webuiEpgRecFollow();
			return;
		}
		webuiEpgModalEvent = ev;
		webuiEpgTimerMarks();
		// a stop ends what runs now: a later or past programme of the
		// channel does not show it
		if (!_webuiEpgRuns(ev))
			webuiStopShownFor(recBtn, '');
		const end = ev.start + ev.duration;
		const now = webuiBoxNow() / 1000;
		titleEl.textContent = ev.title || '--';
		timeEl.textContent = _webuiEpgWhen(ev);
		webuiEpgModalUpdateRecBtn(false);
		// how far it has run, for the one running now; a later one can be
		// zapped to by a timer; one that is over can be neither recorded nor
		// zapped to
		if (_webuiEpgRuns(ev)) {
			run.hidden = false;
			progressBar.style.width = Math.min(100, Math.max(0, (now - ev.start) / ev.duration * 100)) + '%';
			document.getElementById('webuiEpgRunText').textContent = [
				webuiL('epg.running_since', 'on since %s').replace('%s', webuiClock(ev.start)),
				webuiL('dash.left', '%s left').replace('%s', _webuiEpgMinutes(end - now))].join(' · ');
			_webuiEpgRecFollow();
		} else if (ev.start > now) {
			document.getElementById('webuiEpgZapTimerBtn').hidden = false;
		} else {
			recBtn.hidden = true;
		}

		// its short text under the title, its long text below; a short text
		// that is the only and a long one goes below as well. "No
		// description" only where the programme has no text at all: a short
		// text under the title is its description
		const full = ev.id ? await _webuiV1Maybe('/epg/event', {id: ev.id, start: ev.start}).catch(function() { return null; }) : null;
		if (token !== _webuiEpgModalToken) return;
		const short = (full && full.description) || ev.description || '';
		const long = (full && full.long_description) || '';
		const shortUp = short && (long || short.length <= 120);
		subEl.textContent = shortUp ? short : '';
		subEl.hidden = !shortUp;
		descEl.textContent = long || (shortUp ? '' : short) || (short ? '' : webuiL('epg.no_desc', 'No description available'));

		// the next two on the channel, each with a zap timer of its own
		const win = await _webuiV1Maybe('/epg', {channel: channelId, from: end, to: end + 6 * 3600}).catch(function() { return null; });
		if (token !== _webuiEpgModalToken) return;
		const next = ((win && win.items) || []).filter(function(e) { return e.start >= end; }).slice(0, 2);
		if (next.length) {
			document.getElementById('webuiEpgNextLabel').textContent =
				webuiL('epg.after_on', 'Next on %s').replace('%s', webuiEpgModalChannelName);
			nextList.replaceChildren.apply(nextList, next.map(function(e) {
				const t = e.title || '--';
				const li = document.createElement('li');
				li._webuiEvent = e;
				const b = document.createElement('b');
				b.textContent = webuiClock(e.start);
				const what = document.createElement('span');
				what.textContent = t;
				if (e.duration > 0) {
					const len = document.createElement('small');
					len.textContent = _webuiEpgMinutes(e.duration);
					what.appendChild(len);
				}
				const bell = document.createElement('button');
				bell.type = 'button';
				bell.className = 'webui-epg-round';
				bell.title = webuiL('epg.timer_zap_title', 'Set a zap timer');
				bell.setAttribute('aria-label', webuiL('epg.timer_zap', 'Zap timer') + ': ' + t);
				bell.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M18 8a6 6 0 0 0-12 0c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.7 21a2 2 0 0 1-3.4 0"/></svg>';
				bell.addEventListener('click', async function() {
					const done = _webuiEpgModalOnTimer;
					if (await webuiZapTimer(channelId, e) && done) done();
				});
				li.append(b, what, bell);
				return li;
			}));
			nextSection.hidden = false;
			webuiEpgTimerMarks();
		}
	} catch (e) {
		if (token !== _webuiEpgModalToken) return;
		titleEl.textContent = webuiL('msg.load_fail', 'Loading failed');
		console.error('Failed to load EPG:', e);
	}
}

function webuiCloseEpgModal(event) {
	if (event && event.target !== event.currentTarget) return;
	const modal = document.getElementById('webuiEpgModal');
	if (modal) {
		modal.classList.remove('open');
		if (modal._webuiRelease) {
			modal._webuiRelease();
			modal._webuiRelease = null;
		}
	}
	document.body.style.overflow = '';
	// what it showed is no event a later Rec elsewhere may take
	webuiEpgModalEvent = null;
	_webuiEpgModalOnTimer = null;
}

// The dialog's channel, now: closed once the box switched
async function webuiEpgModalZap() {
	if (!webuiEpgModalChannelId) return;
	try {
		if (await webuiV1Zap(webuiEpgModalChannelId))
			webuiCloseEpgModal();
	} catch (e) {
		webuiToastError('', e);
	}
}

/* A zap timer at an event's start, or this minute for one that runs.
   Resolves true once it is set. */
async function webuiZapTimer(channelId, ev) {
	try {
		await webuiV1('POST', '/timers', {body: {kind: 'zapto', channel_id: channelId,
			start: Math.max(ev.start, Math.floor(webuiBoxNow() / 60000) * 60), epg_id: ev.id, epg_start: ev.start}});
		webuiShowToast(webuiL('epg.timer_zap_set', 'Zap timer set!'));
		return true;
	} catch (e) {
		webuiToastError(webuiL('epg.timer_fail', 'Setting the timer failed'), e);
		return false;
	}
}

async function webuiEpgModalZapTimer() {
	const ev = webuiEpgModalEvent, done = _webuiEpgModalOnTimer;
	if (!ev || !webuiEpgModalChannelId) return;
	if (await webuiZapTimer(webuiEpgModalChannelId, ev)) {
		webuiCloseEpgModal();
		if (done) done();
	}
}

/* A playlist with the channel for a player on this computer, saved as a
   file: the box writes it (GET /api/v1/stream/playlist/{id}), with the
   address it was asked under, the port its stream server listens on and the
   channel's name. Without an argument, the channel of the guide dialog. */
function webuiOpenInPlayer(channelId) {
	channelId = channelId || webuiEpgModalChannelId;
	if (!channelId) return;
	webuiV1Download('/stream/playlist/' + encodeURIComponent(channelId), 'channel.m3u').catch(function(e) {
		webuiToastError(webuiL('play.external', 'Open in a player (M3U)'), e);
	});
}

const WEBUI_COPY_SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="9" y="9" width="12" height="12" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></svg>';

/* Puts a text on the clipboard; resolves whether it got there. Where the
   page is served without TLS the clipboard API is not there
   (navigator.clipboard is undefined outside a secure context): a field out
   of sight is selected and copied with the old command instead, inside the
   open modal dialog where there is one (the one holding the focus, else the
   last opened), as the page around it takes no focus. */
async function webuiCopyText(text) {
	if (navigator.clipboard && window.isSecureContext) {
		try {
			await navigator.clipboard.writeText(text);
			return true;
		} catch (e) {
			// refused (a permission, a frame): the old way may still work
		}
	}
	const before = document.activeElement;
	const field = document.createElement('textarea');
	field.className = 'webui-clip';
	field.value = text;
	field.setAttribute('readonly', '');
	field.setAttribute('aria-hidden', 'true');
	field.tabIndex = -1;
	const open = document.querySelectorAll('dialog[open]');
	((before && before.closest && before.closest('dialog[open]')) || open[open.length - 1] || document.body).appendChild(field);
	let done = false;
	try {
		field.focus({preventScroll: true});
		field.select();
		field.setSelectionRange(0, text.length);
		done = document.execCommand('copy');
	} catch (e) {
		done = false;
	}
	field.remove();
	if (before && before.focus)
		before.focus({preventScroll: true});
	return !!done;
}

/* The address a player opens, as a playlist of the box names it (path below
   /api/v1): the playlist read with the page's session and its line that is
   an http or https address, so the address is the one the playlist file
   gives a player, with the token where the box writes one; '' where it
   names none. */
async function webuiPlaylistAddress(path) {
	const resp = await _webuiV1GetFile(path);
	const lines = (await resp.text()).split(/\r?\n/).map(function(l) { return l.trim(); });
	return lines.find(function(l) { return /^https?:\/\//i.test(l); }) || '';
}

/* Copy address, beside each way to a player (M3U): the stream address of
   the playlist at path to the clipboard, and the way to open it in VLC.
   Where the browser refuses the copy, a sheet shows the address selected,
   to copy by hand. */
async function webuiCopyAddress(path) {
	let address;
	try {
		address = await webuiPlaylistAddress(path);
	} catch (e) {
		webuiToastError(webuiL('play.copy', 'Copy address'), e);
		return;
	}
	if (!address) {
		webuiShowToast(webuiL('play.copy_none', 'The playlist names no address'), true);
		return;
	}
	if (await webuiCopyText(address))
		webuiShowToast(webuiL('play.copied', 'Address copied. In VLC: Media → Open Network Stream'));
	else
		_webuiShowAddress(address);
}

// A channel's address; without an argument, the guide dialog's channel
function webuiCopyChannelAddress(channelId) {
	channelId = channelId || webuiEpgModalChannelId;
	if (channelId)
		webuiCopyAddress('/stream/playlist/' + encodeURIComponent(channelId));
}

// The address shown selected in a sheet, for a copy by hand
function _webuiShowAddress(address) {
	const why = webuiL('play.copy_refused', 'The browser did not allow copying. Copy the address from here.');
	if (typeof webuiSheet !== 'function') {
		window.prompt(why, address);
		return;
	}
	const field = webuiEl('input', {type: 'text', id: 'webuiCopyField', value: address, className: 'webui-copy-field',
		attrs: {readonly: '', autofocus: '', spellcheck: 'false'}, on: {focus: function() { this.select(); }}});
	webuiSheet({
		title: webuiL('play.copy', 'Copy address'),
		body: [webuiEl('p', {}, why),
			webuiEl('label', {htmlFor: 'webuiCopyField', className: 'webui-visually-hidden'}, webuiL('play.address', 'Stream address')),
			field],
		actions: [{label: webuiL('msg.close', 'Close'), value: 'close'}]
	});
	field.select();
}

/* The coding of a channel's picture as this tab has learnt it: the player
   notes what the box reports for a channel it plays, and a channel answer
   whose hint the box read off the channel itself (source stream) counts the
   same. Only an H.264 or H.265 picture plays in a browser (the box does not
   re-encode one), so a channel known to send another is offered for a player
   on this computer instead. Kept for the tab, so the other pages know it
   too. */
const _webuiVideoKey = 'webui-video-codecs';
let _webuiVideoSeen = null;
function _webuiVideoCodecs() {
	if (!_webuiVideoSeen) {
		_webuiVideoSeen = Object.create(null);
		try {
			Object.assign(_webuiVideoSeen, JSON.parse(sessionStorage.getItem(_webuiVideoKey) || '{}'));
		} catch (e) { /* known to this page only */ }
	}
	return _webuiVideoSeen;
}

/* The coding the channel list's service type names for a channel, where it
   names MPEG-2: a guess, since that type is the generic one for television
   and allowed for other codings too. Kept apart from what was learnt, which
   it never replaces. */
const _webuiGuessKey = 'webui-video-guesses';
let _webuiGuessSeen = null;
function _webuiVideoGuesses() {
	if (!_webuiGuessSeen) {
		_webuiGuessSeen = Object.create(null);
		try {
			Object.assign(_webuiGuessSeen, JSON.parse(sessionStorage.getItem(_webuiGuessKey) || '{}'));
		} catch (e) { /* known to this page only */ }
	}
	return _webuiGuessSeen;
}

function _webuiKeep(key, map) {
	try {
		sessionStorage.setItem(key, JSON.stringify(map));
	} catch (e) { /* known to this page only */ }
}

// Notes a coding learnt for a channel; true where that changed what was known
function _webuiNoteVideoOnly(channelId, codec) {
	if (!channelId || !codec || codec === 'unknown') return false;
	const seen = _webuiVideoCodecs();
	if (seen[channelId] === codec) return false;
	seen[channelId] = codec;
	return true;
}

function webuiNoteVideo(channelId, codec) {
	if (_webuiNoteVideoOnly(channelId, codec))
		_webuiKeep(_webuiVideoKey, _webuiVideoCodecs());
}

/* The hints of a channel answer, a list ({items}) or one channel: a coding
   the box read off the channel (stream) is learnt; one the service type
   names (service_type) is kept as a guess where it is MPEG-2, and an
   earlier guess goes where the type now names another. A web channel hints
   at nothing. Written to the tab once per answer. */
function webuiNoteChannels(doc) {
	const list = doc && Array.isArray(doc.items) ? doc.items : doc && doc.id ? [doc] : [];
	const guesses = _webuiVideoGuesses();
	let learnt = false, guessed = false;
	list.forEach(function(ch) {
		if (!ch || !ch.id || typeof ch.video_codec_hint !== 'string') return;
		const id = String(ch.id), hint = ch.video_codec_hint;
		if (ch.video_codec_hint_source === 'stream') {
			if (_webuiNoteVideoOnly(id, hint)) learnt = true;
		} else if (ch.video_codec_hint_source === 'service_type') {
			const guess = hint === 'mpeg2' ? hint : '';
			if ((guesses[id] || '') === guess) return;
			if (guess) guesses[id] = guess;
			else delete guesses[id];
			guessed = true;
		}
	});
	if (learnt) _webuiKeep(_webuiVideoKey, _webuiVideoCodecs());
	if (guessed) _webuiKeep(_webuiGuessKey, guesses);
}

// The coding of a channel's picture where no browser plays it, else ''
function webuiUnplayableVideo(channelId) {
	const codec = channelId ? _webuiVideoCodecs()[channelId] : '';
	return codec && ['h264', 'hevc', 'none'].indexOf(codec) < 0 ? codec : '';
}

// The coding the channel list makes likely where nothing was learnt, else ''
function webuiVideoGuess(channelId) {
	if (!channelId || _webuiVideoCodecs()[channelId]) return '';
	return _webuiVideoGuesses()[channelId] || '';
}

const WEBUI_VIDEO_NAMES = {mpeg2: 'MPEG-2', h264: 'H.264', hevc: 'H.265', cavs: 'AVS'};

/* Why a channel does not play in the browser: the channel and the coding of
   its picture where that is known to be the reason; else the box cannot
   convert it (it carries no converter, or cannot decode the sound). With
   hint, the way out follows it. */
function webuiUnplayableText(name, codec, hint) {
	const args = codec ? [name || webuiL('play.this_channel', 'This channel'), WEBUI_VIDEO_NAMES[codec] || String(codec).toUpperCase()]
		: [name || webuiL('play.the_channel', 'the channel')];
	const text = (codec ? webuiL('play.unplayable_video', '%s sends %s video. Browsers cannot play it.')
		: webuiL('play.unplayable', 'This box cannot convert %s for the browser.')).replace(/%s/g, function() { return args.shift(); });
	return hint ? text + ' ' + webuiL('play.unplayable_hint', 'Open the channel in a player instead (M3U).') : text;
}

/* What stands in the way of playing a channel in the browser, as this tab
   knows it: {sure: true, text} where its picture is known to play in no
   browser, {sure: false, text} where the channel list says MPEG-2, else
   null. Either way the playlist for a player is the way to point to. */
function webuiPlayDoubt(channelId, channelName) {
	const codec = webuiUnplayableVideo(channelId);
	if (codec)
		return {sure: true, text: webuiUnplayableText(channelName, codec, true)};
	if (webuiVideoGuess(channelId) === 'mpeg2')
		return {sure: false, text: webuiL('play.guess_mpeg2', 'The channel list says %s sends MPEG-2; browsers usually cannot play it. Open it in a player (M3U).')
			.replace('%s', channelName || webuiL('play.this_channel', 'This channel'))};
	return null;
}

/* Play in browser, offered for a channel: greyed out where its picture is
   known to play in no browser, but still focusable, so the reason can be
   read and a press tells it; offered where the channel list only makes MPEG-2
   likely. The reason or the doubt is the tooltip and, where the caller has
   one, the text of note, which then describes the button; m3u, the way to a
   player, is marked as the one to take. */
function webuiPlayOffer(btn, channelId, channelName, note, m3u) {
	const doubt = webuiPlayDoubt(channelId, channelName);
	if (doubt && doubt.sure)
		btn.setAttribute('aria-disabled', 'true');
	else
		btn.removeAttribute('aria-disabled');
	if (doubt)
		btn.title = doubt.text;
	else
		btn.removeAttribute('title');
	if (note) {
		note.textContent = doubt ? doubt.text : '';
		note.hidden = !doubt;
		if (doubt && note.id)
			btn.setAttribute('aria-describedby', note.id);
		else
			btn.removeAttribute('aria-describedby');
	}
	if (m3u)
		m3u.classList.toggle('suggested', !!doubt);
}

/* A channel played in this page (js/webui-player.js, loaded on the first
   use): the box fetches a web channel for the browser, so it plays here
   although the server it comes from does not let a page read it, and
   converts one from its tuners where the browser cannot decode its sound.
   Without arguments, the channel of the guide dialog, which makes way for
   the player. A channel known to play in no browser is not tried: the
   reason is said instead. */
var _webuiPlayerLoad = null;
function webuiPlayInBrowser(channelId, channelName, kind) {
	if (!channelId) {
		channelId = webuiEpgModalChannelId;
		channelName = webuiEpgModalChannelName;
		kind = _webuiEpgModalKind;
	}
	if (!channelId) return;
	const codec = webuiUnplayableVideo(channelId);
	if (codec) {
		webuiShowToast(webuiUnplayableText(channelName, codec, true), 'note');
		return;
	}
	if (!_webuiPlayerLoad)
		_webuiPlayerLoad = webuiLoadScript('/js/webui-player.js?v=1');
	_webuiPlayerLoad.then(function() {
		const epg = document.getElementById('webuiEpgModal');
		if (epg && epg.classList.contains('open'))
			webuiCloseEpgModal();
		webuiPlayerOpen({id: channelId, name: channelName, kind: kind});
	}, function(e) {
		_webuiPlayerLoad = null;
		webuiToastError(webuiL('play.in_browser', 'Play in browser'), e);
	});
}

// Records a guide event through POST /api/v1/timers: the one given, the one
// the dialog shows for its channel, else what runs now on the channel. An
// event already running starts recording this minute, a timer being refused
// a start before it. The recording margin is asked for as the timer form
// asks for it; the box takes it for a running event too (the start moves
// before now and the timer fires at its next tick, the end moves later).
// Resolves true once the timer is set.
async function webuiRecordEvent(channelId, channelName, ev) {
	channelId = channelId || webuiEpgModalChannelId;
	channelName = channelName || webuiEpgModalChannelName;
	if (!ev && channelId === webuiEpgModalChannelId) ev = webuiEpgModalEvent;
	if (!channelId) return false;
	try {
		if (!ev) ev = await _webuiV1Maybe('/epg/current', {channel: channelId});
		if (!ev) {
			webuiShowToast(webuiL('epg.nodata', 'No EPG data available'), true);
			return false;
		}
		const minute = Math.floor(webuiBoxNow() / 60000) * 60;
		const body = {kind: 'record', channel_id: channelId, start: Math.max(ev.start, minute),
			stop: ev.start + ev.duration, title: ev.title || '', recording_safety: true};
		if (ev.id) {
			body.epg_id = ev.id;
			body.epg_start = ev.start;
		}
		await webuiV1('POST', '/timers', {body: body});
		webuiShowToast(webuiL('rec.planned', 'Recording scheduled') + ': ' + (channelName || ''));
		return true;
	} catch (e) {
		console.error('Record failed:', e);
		webuiToastError(webuiL('msg.error', 'Error'), e);
		return false;
	}
}

// Ends a recording through DELETE /api/v1/recordings/{id}. The id is the
// timer the recording carries, so a timer list can pass its own; a page
// that has to find the recording first does so with a read at the press
// (webuiRecordingsRead), never from the list it shows.
async function webuiStopRecording(recordingId) {
	try {
		await webuiV1('DELETE', '/recordings/' + encodeURIComponent(recordingId));
		webuiShowToast(webuiL('rec.stopped', 'Recording stopped'));
		return true;
	} catch (e) {
		webuiToastError(webuiL('rec.stop_fail', 'Stop failed'), e);
		return false;
	}
}

/* The same, said once the box has ended the recording: its 202 says no
   more than that it took the request on.
   - what: the recording's id, or a function that finds it at the press as
     a read then shows it and resolves {id, title}, null where none runs
     (nothing is said), false where the user declined the sign-in the read
     asked for (nothing is said or sent) or undefined where the read failed
     (Stop failed);
   - the button (opts.button, else the one pressed) says "Stopping…" with a
     spinner from the press on and keeps its width; a second press for the
     same recording waits for the first and sends nothing, told by its id
     where one is given, else by opts.channel;
   - opts.channel is a key for what the press was for, a channel (the guide
     dialog's) or any string the page chooses: a button that serves several
     keys shows the stop only while it serves that one (webuiStopShownFor);
   - the box confirms with a record-stop event naming the recording, or with
     a shared read of the recordings, made after the 202, that no longer
     lists it (webuiOnRecordings); without a live stream nothing brings such
     a read, so one is made after each of WEBUI_STOP_READS. Then "Recording
     stopped: <title>" (opts.title, else the title found), after the read
     that the event brings, so the page shows the end before the word comes;
   - without either within WEBUI_STOP_WAIT one more read decides, given at
     most WEBUI_STOP_CHECK: ended, or a warning that the box has not ended
     it yet, or that this could not be checked;
   - a recording the box no longer has (no-such-recording) is said as a
     note and read again; any other refusal is said at once.
   Resolves 'stopped', 'gone', 'running', null (none found) or false
   (refused, the read at the press failed or its sign-in was declined). */
var WEBUI_STOP_READS = [1500, 4000];
var WEBUI_STOP_WAIT = 8000;
var WEBUI_STOP_CHECK = 10000;
var _webuiStops = Object.create(null);	// the stops under way whose recording is known, by its id
var _webuiStopsRunning = [];		// every stop under way, its recording found or not yet
var _webuiStopsHeard = false;		// whether the stops follow the record events and the shared read yet

function webuiStopRecordingConfirmed(what, opts) {
	opts = opts || {};
	const btn = opts.button || webuiWaitButton();
	const id = typeof what === 'function' ? null : String(what);
	const channel = opts.channel || '';
	const same = _webuiStopsRunning.find(function(s) {
		return id ? s.id === id : !!channel && s.channel === channel;
	});
	if (same)
		return same.done;
	const stop = {id: id, channel: channel, title: opts.title || '', btn: null, seq: null, heard: false, over: false, timers: []};
	stop.done = new Promise(function(resolve) { stop.resolve = resolve; });
	_webuiStopsRunning.push(stop);
	if (btn)
		_webuiStopMark(stop, btn);
	_webuiStopsFollow();
	_webuiStopRun(stop, what);
	return stop.done;
}

async function _webuiStopRun(stop, what) {
	if (typeof what === 'function') {
		let found;
		try {
			found = await what();
		} catch (e) {
			found = undefined;
		}
		if (found === null) {
			_webuiStopEnd(stop, null);
			return;
		}
		if (found === false) {
			_webuiStopEnd(stop, false);
			return;
		}
		if (!found || found.id === undefined || found.id === null) {
			webuiShowToast(webuiL('rec.stop_fail', 'Stop failed'), true);
			_webuiStopEnd(stop, false);
			return;
		}
		stop.title = stop.title || found.title || '';
		stop.id = String(found.id);
	}
	const other = _webuiStops[stop.id];
	if (other) {
		// stopped from elsewhere on the page already: its outcome, said once
		_webuiStopEnd(stop, await other.done, true);
		return;
	}
	_webuiStops[stop.id] = stop;
	try {
		await webuiV1('DELETE', '/recordings/' + encodeURIComponent(stop.id), {mark: false});
	} catch (e) {
		if (e.code === 'no-such-recording') {
			webuiShowToast(webuiL('rec.stop_gone', 'The recording had already ended.'), 'note');
			webuiRecordingsRead();
			_webuiStopEnd(stop, 'gone');
		} else {
			webuiToastError(webuiL('rec.stop_fail', 'Stop failed'), e);
			_webuiStopEnd(stop, false);
		}
		return;
	}
	if (stop.over)
		return;
	stop.seq = webuiRecordingsNow().seq;
	if (stop.heard)
		_webuiStopHeard(stop);
	if (!webuiV1EventsLive())
		WEBUI_STOP_READS.forEach(function(at) {
			stop.timers.push(setTimeout(function() { if (!stop.over) webuiRecordingsRead(); }, at));
		});
	stop.timers.push(setTimeout(function() { _webuiStopLate(stop); }, WEBUI_STOP_WAIT));
}

function _webuiStopsFollow() {
	if (_webuiStopsHeard)
		return;
	_webuiStopsHeard = true;
	webuiV1Watch('record-stop', function(d) {
		const stop = d ? _webuiStops[String(d.value)] : null;
		if (!stop || stop.heard)
			return;
		stop.heard = true;
		if (stop.seq !== null)
			_webuiStopHeard(stop);
	});
	webuiOnRecordings(function(view) {
		Object.keys(_webuiStops).forEach(function(id) {
			const stop = _webuiStops[id];
			if (stop.seq === null || !(view.seq > stop.seq))
				return;
			if (stop.heard || (view.known && !view.stale && !_webuiListsRecording(view, id)))
				_webuiStopEnd(stop, 'stopped');
		});
	});
}

// The box said it ended: done with the shared read that event brings, or
// in a moment without one
function _webuiStopHeard(stop) {
	stop.timers.push(setTimeout(function() { _webuiStopEnd(stop, 'stopped'); }, 1500));
}

function _webuiListsRecording(view, id) {
	return view.items.some(function(it) { return String(it.id) === id; });
}

// No word of the box in time: one more read decides, or not answering
// within WEBUI_STOP_CHECK, says that it could not
async function _webuiStopLate(stop) {
	if (stop.over)
		return;
	let limit = null;
	const view = await Promise.race([webuiRecordingsRead(),
		new Promise(function(done) { limit = setTimeout(function() { done(null); }, WEBUI_STOP_CHECK); })]);
	clearTimeout(limit);
	if (stop.over)
		return;
	if (view && view.known && !view.stale && !_webuiListsRecording(view, stop.id)) {
		_webuiStopEnd(stop, 'stopped');
		return;
	}
	webuiShowToast(!view || view.stale ? webuiL('rec.stop_unchecked', 'Could not check whether the recording stopped.')
		: webuiL('rec.stop_pending', 'The box has not stopped the recording yet.'), 'warn');
	_webuiStopEnd(stop, 'running');
}

function _webuiStopEnd(stop, result, quiet) {
	if (stop.over)
		return;
	stop.over = true;
	stop.timers.forEach(clearTimeout);
	if (stop.id !== null && _webuiStops[stop.id] === stop)
		delete _webuiStops[stop.id];
	const at = _webuiStopsRunning.indexOf(stop);
	if (at >= 0)
		_webuiStopsRunning.splice(at, 1);
	_webuiStopUnmark(stop);
	if (result === 'stopped' && !quiet)
		webuiShowToast(stop.title ? webuiL('rec.stopped_title', 'Recording stopped: %s').replace('%s', stop.title)
			: webuiL('rec.stopped', 'Recording stopped'));
	stop.resolve(result);
}

// The button shows the stop: one that showed another stop gives that up,
// and the button that showed this one before is free again
function _webuiStopMark(stop, btn) {
	const was = btn._webuiStopRun;
	if (was && was !== stop)
		was.btn = null;
	if (stop.btn && stop.btn !== btn)
		_webuiStopUnmark(stop);
	stop.btn = btn;
	btn._webuiStopRun = stop;
	_webuiBusyWords(btn, webuiL('rec.stopping', 'Stopping…'));
}

function _webuiStopUnmark(stop) {
	const btn = stop.btn;
	stop.btn = null;
	if (btn && btn._webuiStopRun === stop) {
		btn._webuiStopRun = null;
		_webuiBusyWords(btn, '');
	}
}

/* A button that serves several keys (opts.channel of
   webuiStopRecordingConfirmed: the channels of the guide dialog's Record,
   or a page's own) shows a stop only while it serves the key the stop was
   pressed for: it gives the stop up when it serves another, or '' (none),
   and takes it up again when it serves that one once more. */
function webuiStopShownFor(btn, channel) {
	const run = _webuiStopsRunning.find(function(s) { return !!s.channel && s.channel === channel; });
	if (run)
		_webuiStopMark(run, btn);
	else if (btn._webuiStopRun)
		_webuiStopUnmark(btn._webuiStopRun);
}

/* A press the box takes a while to carry out, shown on its button: the
   words say what is under way, beside a spinner in their colour, in place
   of what the button holds (which the page may keep drawing meanwhile);
   the button keeps its width, so a quick second click lands on it and not
   on its neighbour, is busy and takes no press. '' ends it. */
function _webuiBusyWords(btn, words) {
	if (words) {
		if (!btn.classList.contains('webui-busy-words')) {
			const width = btn.getBoundingClientRect().width;
			btn._webuiBusyWidth = btn.style.minWidth;
			if (width)
				btn.style.minWidth = width + 'px';
		}
		btn.setAttribute('data-busy-words', words);
		btn.classList.add('webui-busy-words');
		btn.setAttribute('aria-busy', 'true');
	} else if (btn.classList.contains('webui-busy-words')) {
		btn.classList.remove('webui-busy-words');
		btn.removeAttribute('data-busy-words');
		btn.removeAttribute('aria-busy');
		btn.style.minWidth = btn._webuiBusyWidth || '';
	}
}

// The dialog's Record: a recording of the programme it shows, or, for the
// one running now while the box records it, the end of that recording
async function webuiEpgModalToggleRec() {
	const done = _webuiEpgModalOnTimer;
	// a stop under way takes no press, not from the keyboard either (its
	// label may still be the one of the programme before)
	const btn = document.getElementById('webuiEpgRecBtn');
	if (btn && btn._webuiStopRun)
		return;
	if (webuiEpgModalIsRecording) {
		// this channel's recording and no other, as a read at the press finds
		// it (with a sign-in where the box asks for one): one that ended since
		// the dialog opened leaves nothing to stop here, and the button says
		// Record again without a word; a read that fails stops nothing, and
		// one whose sign-in the user declined says nothing either. The
		// button says that the stop is under way until the box has ended it,
		// then Record, unless the channel is being recorded once more.
		const channelId = webuiEpgModalChannelId;
		const channelName = webuiEpgModalChannelName;
		let stopped = '';
		const ended = await webuiStopRecordingConfirmed(async function() {
			const view = await webuiRecordingsRead({signIn: true});
			if (view.stale)
				return view.declined ? false : undefined;
			const rec = view.items.find(function(it) { return it.channel_id === channelId; });
			stopped = rec ? String(rec.id) : '';
			return rec ? {id: rec.id, title: rec.title || channelName} : null;
		}, {channel: channelId});
		if ((ended === null || ended === 'stopped' || ended === 'gone') && channelId === webuiEpgModalChannelId
			&& (!webuiEpgModalEvent || _webuiEpgRuns(webuiEpgModalEvent)))
			webuiEpgModalUpdateRecBtn(webuiRecordingsNow().items.some(function(it) {
				return it.channel_id === channelId && String(it.id) !== stopped;
			}));
	} else if (await webuiRecordEvent()) {
		if (done) done();
		// the recording begins when the box fires the timer; its record-start
		// event marks the button, or without the event stream a read later
		if (!_webuiEpgRuns(webuiEpgModalEvent))
			webuiCloseEpgModal();
		else
			webuiRecordingsAfter();
	}
}

// The dialog's Record: Stop recording while the box records the one
// running now, Plan recording for a later one, else Record
function webuiEpgModalUpdateRecBtn(isRecording) {
	webuiEpgModalIsRecording = isRecording;
	const btn = document.getElementById('webuiEpgRecBtn');
	if (!btn) return;
	const ev = webuiEpgModalEvent;
	const later = !!ev && ev.start > webuiBoxNow() / 1000;
	const label = btn.querySelector('span');
	if (label) label.textContent = isRecording ? webuiL('rec.stop', 'Stop recording') :
		later ? webuiL('rec.plan', 'Plan recording') : webuiL('rec.start', 'Record');
	btn.classList.toggle('recording', isRecording);
}
