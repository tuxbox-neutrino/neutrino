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
// application/problem+json, whose type is /errors/ and the code); a 403 the
// user did not sign in for carries .cancelled. webuiToastError (in
// webui-header.js) tells the user why, in the page's language.
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
// webuiV1Events({name: function(data) {...}}) opens one EventSource on
// /api/v1/events and calls the handler of each event that arrives with its
// data ({channel_id, value, text}). The browser reconnects by itself, except
// after a refusal: a stream the box refused for want of a sign-in is opened
// again once the sign-in went through. A handler named `open` is called with
// null each time the stream (re)connects, which is when a page rereads what
// it may have missed. webuiV1EventsLive() says whether the stream is open
// right now; a page polls what it follows while it is not.

var _webuiV1Csrf = null;	// {header, token} of the open session, if any
var _webuiV1SignInPending = null;	// the one sign-in dialog of the page, while open
var _webuiV1Stream = null;	// {handlers, es, closed} of webuiV1Events

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

// Reads the session state and keeps its token; true when one is open.
async function webuiV1Session() {
	_webuiV1Csrf = null;
	try {
		var resp = await fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
		if (!resp.ok) return false;
		var s = await resp.json();
		if (s && s.authenticated && s.csrf && s.csrf_header) {
			_webuiV1Csrf = {header: s.csrf_header, token: s.csrf};
			return true;
		}
	} catch (e) { /* no session then */ }
	return false;
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
	return fetch(_webuiV1Url(path, opts.query), init);
}

async function _webuiV1Error(resp, method, path) {
	var text = '';
	var code = '';
	try {
		var p = await resp.json();
		if (p) text = p.detail || p.title || '';
		if (p && typeof p.type === 'string' && p.type.indexOf('/errors/') === 0)
			code = p.type.substring(8);
	} catch (e) { /* not a problem document */ }
	var err = new Error(text || (method + ' ' + path + ': HTTP ' + resp.status));
	err.status = resp.status;
	err.code = code;
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
	var resp = await _webuiV1Send(method, path, opts);
	if (await _webuiV1ChangeOwed(resp))
		throw await _webuiV1Error(resp, method, path);
	var declined = false;
	if (resp.status === 403) {
		// no open session, or a stale token
		if (opts.refused) opts.refused();
		var open = await webuiV1Session();
		if (open || await webuiV1SignIn()) {
			if (opts.retried) opts.retried();
			resp = await _webuiV1Send(method, path, opts);
		} else {
			declined = true;
		}
	}
	if (!resp.ok) throw _webuiV1Declined(await _webuiV1Error(resp, method, path), declined);
	if (resp.status === 202 || resp.status === 204) return null;
	var type = resp.headers.get('Content-Type') || '';
	return type.indexOf('json') >= 0 ? resp.json() : resp.text();
}

// A picture (or any other file) as a Blob, for a screenshot: the same
// sign-in on 403 as webuiV1, and a refusal throws the same Error.
// opts.quiet leaves the sign-in out and throws on a 403 (a probe at load).
async function webuiV1Blob(path, query, opts) {
	opts = opts || {};
	var url = _webuiV1Url(path, query);
	var get = function() { return fetch(url, {credentials: 'same-origin', cache: 'no-store'}); };
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

// A file the box answers as a download, saved under the name the box gives it
// (Content-Disposition), else under fallback; the same sign-in on 403 as
// webuiV1. Resolves to the name it was saved under.
async function webuiV1Download(path, fallback) {
	var get = function() { return fetch(_webuiV1Url(path), {credentials: 'same-origin', cache: 'no-store'}); };
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
	var m = /filename="([^"\/\\]+)"/.exec(resp.headers.get('Content-Disposition') || '');
	var name = m ? m[1] : fallback;
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

// Every page of a paged list (items + next_cursor), as one array.
async function webuiV1All(path, query) {
	var items = [];
	var q = Object.assign({}, query || {}, {limit: 500});
	for (;;) {
		var page = await webuiV1('GET', path, {query: q});
		items = items.concat((page && page.items) || []);
		if (!page || !page.next_cursor) return items;
		q.cursor = page.next_cursor;
	}
}

// The sign-in dialog; resolves true once signed in, false when cancelled.
// Requests refused at the same time share the one dialog.
function webuiV1SignIn() {
	if (_webuiV1SignInPending) return _webuiV1SignInPending;
	_webuiV1SignInPending = _webuiV1SignInDialog().then(function(ok) {
		_webuiV1SignInPending = null;
		if (ok && _webuiV1Stream && _webuiV1Stream.closed) _webuiV1Open();
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
			var why = webuiL('v1.signin_failed', 'Sign-in failed');
			try {
				var resp = await fetch('/api/v1/login', {
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

// One EventSource for the page; handlers keyed by event name.
function webuiV1Events(handlers) {
	if (!window.EventSource) return null;
	_webuiV1Stream = {handlers: handlers, es: null, closed: false};
	return _webuiV1Open();
}

function webuiV1EventsLive() {
	return !!(window.EventSource && _webuiV1Stream && _webuiV1Stream.es &&
		_webuiV1Stream.es.readyState === EventSource.OPEN);
}

function _webuiV1Open() {
	var stream = _webuiV1Stream;
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
		const r = await fetch('/api/v1/channels/' + encodeURIComponent(channelId) + '/logo', {credentials: 'same-origin'});
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

// The dialog, built on its first opening, once the texts are in
function _webuiEpgModalHTML() {
	return `
<div class="webui-epg-modal-backdrop" id="webuiEpgModal" onclick="webuiCloseEpgModal(event)">
	<div class="webui-epg-modal" onclick="event.stopPropagation()">
		<div class="webui-epg-modal-header">
			<div class="webui-epg-modal-channel">
				<span id="webuiEpgModalLogo"></span>
				<div class="webui-epg-modal-channel-text">
					<h3 id="webuiEpgModalChannel">--</h3>
					<div class="webui-epg-modal-number" id="webuiEpgModalNumber"></div>
				</div>
			</div>
			${_webuiCloseBtn('webuiCloseEpgModal()')}
		</div>
		<div class="webui-epg-modal-body">
			<div class="webui-epg-current">
				<div class="webui-epg-title" id="webuiEpgCurrentTitle">${webuiLH('msg.loading', 'Loading...')}</div>
				<div class="webui-epg-time" id="webuiEpgCurrentTime">--:-- - --:--</div>
				<div class="webui-epg-progress" id="webuiEpgProgress" aria-hidden="true">
					<div class="webui-epg-progress-bar" id="webuiEpgProgressBar" style="width: 0%"></div>
				</div>
				<div class="webui-epg-description" id="webuiEpgCurrentDesc"></div>
			</div>
			<div class="webui-epg-next" id="webuiEpgNextSection" style="display: none;">
				<div class="webui-epg-next-label">${webuiLH('epg.after', 'Next')}</div>
				<div class="webui-epg-next-title" id="webuiEpgNextTitle">--</div>
				<div class="webui-epg-next-time" id="webuiEpgNextTime">--:-- - --:--</div>
			</div>
		</div>
		<div class="webui-epg-modal-footer">
			<button type="button" class="webui-btn" onclick="webuiEpgModalZap()" title="${webuiLH('epg.zap_title', 'Switch to this channel now')}">
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polygon points="5,3 19,12 5,21"/></svg>
				<span>${webuiLH('epg.zap', 'Zap')}</span>
			</button>
			<button type="button" class="webui-btn" id="webuiEpgZapTimerBtn" onclick="webuiEpgModalZapTimer()" title="${webuiLH('epg.timer_zap_title', 'Set a zap timer')}" hidden>
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="12" cy="12" r="10"/><polyline points="12,6 12,12 16,14"/></svg>
				<span>${webuiLH('epg.timer_zap', 'Zap timer')}</span>
			</button>
			<button type="button" class="webui-btn webui-epg-rec-btn" id="webuiEpgRecBtn" onclick="webuiEpgModalToggleRec()">
				<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><circle cx="12" cy="12" r="8"/></svg>
				<span>${webuiLH('rec.start', 'Record')}</span>
			</button>
			<button type="button" class="webui-btn primary" onclick="webuiStreamChannel()" title="${webuiLH('bq.stream_title', 'Start stream')}">
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">
					<path d="M5 17H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/>
					<polygon points="12,15 17,21 7,21"/>
				</svg>
				<span>${webuiLH('msg.stream', 'Stream')}</span>
			</button>
		</div>
	</div>
</div>
`;
}

// The channel and the guide event the dialog shows, for its buttons
let webuiEpgModalChannelId = null;
let webuiEpgModalChannelName = null;
let webuiEpgModalEvent = null;
let webuiEpgModalIsRecording = false;
let _webuiEpgModalOnTimer = null;
let _webuiEpgModalToken = 0;

function _webuiClock(sec) {
	return webuiFormatTime(new Date(sec * 1000));
}

// A guide read that finds nothing is an answer, not a failure
function _webuiV1Maybe(path, query) {
	return webuiV1('GET', path, {query: query}).catch(function(e) {
		if (e.status === 404) return null;
		throw e;
	});
}

// whether an event runs now
function _webuiEpgRuns(ev) {
	const now = Date.now() / 1000;
	return !!ev && ev.start <= now && now < ev.start + ev.duration;
}

/* A programme of a channel: the event given (opts.event, a programme of the
   guide), else what runs now on the channel; its long text, what follows
   it, and the channel's logo and number (opts.number). opts.onTimer is
   called once a timer was set from the dialog. */
async function webuiShowEpgModal(channelId, channelName, opts) {
	opts = opts || {};
	if (!document.getElementById('webuiEpgModal'))
		document.body.insertAdjacentHTML('beforeend', _webuiEpgModalHTML());

	const modal = document.getElementById('webuiEpgModal');
	const titleEl = document.getElementById('webuiEpgCurrentTitle');
	const timeEl = document.getElementById('webuiEpgCurrentTime');
	const descEl = document.getElementById('webuiEpgCurrentDesc');
	const progress = document.getElementById('webuiEpgProgress');
	const progressBar = document.getElementById('webuiEpgProgressBar');
	const channelEl = document.getElementById('webuiEpgModalChannel');
	const numberEl = document.getElementById('webuiEpgModalNumber');
	const nextSection = document.getElementById('webuiEpgNextSection');
	const nextTitleEl = document.getElementById('webuiEpgNextTitle');
	const nextTimeEl = document.getElementById('webuiEpgNextTime');

	const token = ++_webuiEpgModalToken;
	webuiEpgModalChannelId = channelId;
	webuiEpgModalChannelName = channelName || webuiL('epg.programme', 'Programme');
	webuiEpgModalEvent = null;
	_webuiEpgModalOnTimer = opts.onTimer || null;

	channelEl.textContent = webuiEpgModalChannelName;
	numberEl.textContent = opts.number > 0 ? webuiL('epg.number', 'Programme %s').replace('%s', opts.number) : '';
	await webuiLogosOffered(channelId);
	if (token !== _webuiEpgModalToken) return;
	document.getElementById('webuiEpgModalLogo').replaceChildren(
		webuiChannelLogo(channelId, webuiEpgModalChannelName.slice(0, 12), 'webui-epg-modal-logo'));
	titleEl.textContent = webuiL('msg.loading', 'Loading...');
	timeEl.textContent = '--:-- - --:--';
	descEl.textContent = '';
	progressBar.style.width = '0%';
	progress.hidden = true;
	nextSection.style.display = 'none';
	document.getElementById('webuiEpgZapTimerBtn').hidden = true;

	modal.classList.add('open');
	document.body.style.overflow = 'hidden';
	if (modal._webuiRelease)
		modal._webuiRelease();
	modal._webuiRelease = webuiDialogA11y(modal.querySelector('.webui-epg-modal'), {close: function() { webuiCloseEpgModal(); }});

	webuiEpgModalUpdateRecBtn(false);

	try {
		const ev = opts.event || await _webuiV1Maybe('/epg/current', {channel: channelId});
		if (token !== _webuiEpgModalToken) return;	// opened for another programme meanwhile
		if (!ev) {
			titleEl.textContent = webuiL('epg.nodata', 'No EPG data available');
			return;
		}
		webuiEpgModalEvent = ev;
		const end = ev.start + ev.duration;
		titleEl.textContent = ev.title || '--';
		timeEl.textContent = _webuiClock(ev.start) + ' - ' + _webuiClock(end) +
			(ev.duration > 0 ? ' (' + webuiL('epg.minutes', '%s min').replace('%s', Math.round(ev.duration / 60)) + ')' : '');
		descEl.textContent = ev.description || '';
		// how far it has run, for the one running now; a later one can be zapped to by a timer
		if (_webuiEpgRuns(ev)) {
			progress.hidden = false;
			progressBar.style.width = Math.min(100, Math.max(0, (Date.now() / 1000 - ev.start) / ev.duration * 100)) + '%';
			webuiCheckRecordingForChannel(channelId).then(function(rec) {
				if (token === _webuiEpgModalToken) webuiEpgModalUpdateRecBtn(!!rec);
			});
		} else if (ev.start > Date.now() / 1000) {
			document.getElementById('webuiEpgZapTimerBtn').hidden = false;
		}

		const full = ev.id ? await _webuiV1Maybe('/epg/event', {id: ev.id, start: ev.start}).catch(function() { return null; }) : null;
		if (token !== _webuiEpgModalToken) return;
		const text = full ? [full.description, full.long_description].filter(Boolean).join('\n\n') : '';
		descEl.textContent = text || ev.description || webuiL('epg.no_desc', 'No description available');

		const win = await _webuiV1Maybe('/epg', {channel: channelId, from: end, to: end + 6 * 3600}).catch(function() { return null; });
		if (token !== _webuiEpgModalToken) return;
		const next = win && (win.items || []).find(function(e) { return e.start >= end; });
		if (next) {
			nextTitleEl.textContent = next.title || '--';
			nextTimeEl.textContent = _webuiClock(next.start) + ' - ' + _webuiClock(next.start + next.duration);
			nextSection.style.display = 'block';
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
			start: Math.max(ev.start, Math.floor(Date.now() / 60000) * 60), epg_id: ev.id, epg_start: ev.start}});
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

// A playlist for a player on this computer: the box's stream server with the
// channel, which wants the id with all 16 digits
function webuiStreamChannel(channelId, channelName) {
	channelId = channelId || webuiEpgModalChannelId;
	channelName = channelName || webuiEpgModalChannelName;
	if (!channelId) return;
	const plist = "#EXTM3U\n#EXTINF:-1," + channelName + "\nhttp://" + window.location.hostname + ":31339/id=" + webuiLegacyChannelId(channelId) + "\n";
	const blob = new Blob([plist], { type: "application/octet-stream" });
	const a = document.createElement('a');
	a.href = URL.createObjectURL(blob);
	a.download = "livestream.m3u";
	a.click();
	URL.revokeObjectURL(a.href);
}

// Records a guide event through POST /api/v1/timers: the one given, the one
// the dialog shows for its channel, else what runs now on the channel. An
// event already running starts recording this minute, a timer being refused
// a start before it. Resolves true once the timer is set.
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
		const minute = Math.floor(Date.now() / 60000) * 60;
		const body = {kind: 'record', channel_id: channelId, start: Math.max(ev.start, minute),
			stop: ev.start + ev.duration, title: ev.title || ''};
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

// Ends a recording through DELETE /api/v1/recordings/{id}; without an id the
// one the box is recording, its timeshift left out. The id is the timer the
// recording carries, so a timer list can pass its own.
async function webuiStopRecording(recordingId) {
	try {
		if (!recordingId) {
			const r = await webuiV1('GET', '/recordings');
			const rec = ((r && r.items) || []).find(function(it) { return !it.timeshift; });
			if (!rec) {
				webuiShowToast(webuiL('rec.stop_fail', 'Stop failed'), true);
				return false;
			}
			recordingId = rec.id;
		}
		await webuiV1('DELETE', '/recordings/' + encodeURIComponent(recordingId));
		webuiShowToast(webuiL('rec.stopped', 'Recording stopped'));
		return true;
	} catch (e) {
		webuiToastError(webuiL('rec.stop_fail', 'Stop failed'), e);
		return false;
	}
}

// The dialog's Record: a recording of the programme it shows, or, for the
// one running now while the box records it, the end of that recording
async function webuiEpgModalToggleRec() {
	const done = _webuiEpgModalOnTimer;
	if (webuiEpgModalIsRecording) {
		// this channel's recording and no other: one that ended since the
		// dialog opened leaves nothing to stop here
		const rec = await webuiCheckRecordingForChannel(webuiEpgModalChannelId);
		if (!rec || await webuiStopRecording(rec.id))
			webuiEpgModalUpdateRecBtn(false);
	} else if (await webuiRecordEvent()) {
		if (done) done();
		if (_webuiEpgRuns(webuiEpgModalEvent))
			webuiCheckRecordingForChannel(webuiEpgModalChannelId).then(function(rec) { webuiEpgModalUpdateRecBtn(!!rec); });
		else
			webuiCloseEpgModal();
	}
}

function webuiEpgModalUpdateRecBtn(isRecording) {
	webuiEpgModalIsRecording = isRecording;
	const btn = document.getElementById('webuiEpgRecBtn');
	if (!btn) return;
	const label = btn.querySelector('span');
	if (label) label.textContent = isRecording ? webuiL('rec.stop', 'Stop recording') : webuiL('rec.start', 'Record');
	btn.style.animation = isRecording ? 'webui-pulse 1.5s infinite' : '';
}

// The recording the box is making of a channel, its timeshift left out, or null
async function webuiCheckRecordingForChannel(channelId) {
	try {
		const r = await webuiV1('GET', '/recordings');
		return ((r && r.items) || []).find(function(it) {
			return !it.timeshift && it.channel_id === channelId;
		}) || null;
	} catch (e) {
		return null;
	}
}
