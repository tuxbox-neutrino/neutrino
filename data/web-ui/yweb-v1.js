// yweb-v1.js - what the modern pages share to talk to the REST interface
// /api/v1 (load after Y_Lang.yjs and yweb-header.js).
//
//   ywebV1('GET', '/bouquets')                            -> parsed JSON
//   ywebV1('GET', '/channels', {query: {mode: 'tv'}})     -> parsed JSON
//   ywebV1('PUT', '/bouquets/' + encodeURIComponent(n) + '/hidden',
//          {body: {on: true}})                            -> null (204)
//   ywebV1Blob('/osd/screenshot', {format: 'png'})        -> Blob
//   ywebV1('PUT', '/storage/file', {query: {path: p}, raw: blob})
//                                                         -> bytes as the body
//
// A refusal throws an Error with the box's reason as its message, the HTTP
// status in .status and the code of the refusal in .code (answers come as
// application/problem+json, whose type is /errors/ and the code).
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
// again once the sign-in went through. A handler named `open` is called with
// null each time the stream (re)connects, which is when a page rereads what
// it may have missed. ywebV1EventsLive() says whether the stream is open
// right now; a page polls what it follows while it is not.

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
	if (opts.raw !== undefined) {
		// bytes as they are (a file or a piece of one), not a document
		headers['Content-Type'] = 'application/octet-stream';
		init.body = opts.raw;
	} else if (opts.body !== undefined) {
		headers['Content-Type'] = 'application/json';
		init.body = JSON.stringify(opts.body);
	}
	if (method !== 'GET' && _ywebV1Csrf)
		headers[_ywebV1Csrf.header] = _ywebV1Csrf.token;
	return fetch(_ywebV1Url(path, opts.query), init);
}

async function _ywebV1Error(resp, method, path) {
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

// A picture (or any other file) as a Blob, for a screenshot: the same
// sign-in on 403 as ywebV1, and a refusal throws the same Error.
// opts.quiet leaves the sign-in out and throws on a 403 (a probe at load).
async function ywebV1Blob(path, query, opts) {
	opts = opts || {};
	var url = _ywebV1Url(path, query);
	var get = function() { return fetch(url, {credentials: 'same-origin', cache: 'no-store'}); };
	var resp = await get();
	if (resp.status === 403 && !opts.quiet && (await ywebV1Session() || await ywebV1SignIn()))
		resp = await get();
	if (!resp.ok) throw await _ywebV1Error(resp, 'GET', path);
	return resp.blob();
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
						// what was signed into can be ended from the header
						if (typeof ywebUpdateLogoutBtn === 'function')
							ywebUpdateLogoutBtn(true);
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

// The legacy helpers (/api/v4, the EPG dialog, recording, streaming) take a
// channel id with all 16 hex digits; v1 writes one without leading zeros.
function ywebLegacyChannelId(id) {
	return ('0000000000000000' + String(id)).slice(-16);
}

// Sends a request that puts something on the television (a zap, a
// recording) and asks about the two refusals the user can lift: the movie
// player playing a file (stop_playback) and the box in standby (wake), then
// sends it again. texts names the sheet's title, its button and the two
// questions. Resolves true once the box took it, false when the user said
// no; any other refusal throws as ywebV1 does.
async function ywebV1Lifting(path, body, texts) {
	body = Object.assign({}, body);
	for (;;) {
		try {
			await ywebV1('POST', path, {body: body});
			return true;
		} catch (e) {
			if (e.status === 409 && e.code === 'playback-running' && !body.stop_playback) {
				if (!await _ywebV1Ask(texts.stop, texts.title, texts.ok))
					return false;
				body.stop_playback = true;
				continue;
			}
			if (e.status === 409 && e.code === 'box-in-standby' && !body.wake) {
				if (!await _ywebV1Ask(texts.wake, texts.title, texts.ok))
					return false;
				body.wake = true;
				continue;
			}
			throw e;
		}
	}
}

// Plays a channel through POST /api/v1/zap, asking as above.
function ywebV1Zap(channelId) {
	return ywebV1Lifting('/zap', {channel_id: channelId}, {
		title: ywebL('v1.zap_title', 'Switch channel'),
		ok: ywebL('v1.zap_ok', 'Switch'),
		stop: ywebL('v1.zap_stop_playback', 'The player is playing a file. Stop it and switch to the channel?'),
		wake: ywebL('v1.zap_wake', 'The box is in standby. Switch it on and play the channel?')
	});
}

// A yes or no in a sheet (yweb-dom.js); without the helper layer the
// browser's own confirm.
function _ywebV1Ask(text, title, ok) {
	if (typeof ywebSheet !== 'function')
		return Promise.resolve(window.confirm(text));
	return new Promise(function(resolve) {
		ywebSheet({
			title: title,
			body: ywebEl('p', {}, text),
			actions: [
				{label: ywebL('msg.cancel', 'Cancel'), value: false},
				{label: ok, value: true, primary: true}
			],
			onClose: function(result) { resolve(result === true); }
		});
	});
}

// One EventSource for the page; handlers keyed by event name.
function ywebV1Events(handlers) {
	if (!window.EventSource) return null;
	_ywebV1Stream = {handlers: handlers, es: null, closed: false};
	return _ywebV1Open();
}

function ywebV1EventsLive() {
	return !!(window.EventSource && _ywebV1Stream && _ywebV1Stream.es &&
		_ywebV1Stream.es.readyState === EventSource.OPEN);
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

// ---- EPG dialog, recording and streaming over /api/v1 -------------------
// Shared by the dashboard, the channel list and the timer list. Channel ids
// are v1's: hexadecimal without leading zeros.

// EPG Info Modal HTML Template
const YWEB_EPG_MODAL_HTML = `
<div class="yweb-epg-modal-backdrop" id="ywebEpgModal" onclick="ywebCloseEpgModal(event)">
	<div class="yweb-epg-modal" onclick="event.stopPropagation()">
		<div class="yweb-epg-modal-header">
			<h3 id="ywebEpgModalChannel">--</h3>
			${_ywebCloseBtn('ywebCloseEpgModal()')}
		</div>
		<div class="yweb-epg-modal-body">
			<div class="yweb-epg-current">
				<div class="yweb-epg-title" id="ywebEpgCurrentTitle">Lade...</div>
				<div class="yweb-epg-time" id="ywebEpgCurrentTime">--:-- - --:--</div>
				<div class="yweb-epg-progress">
					<div class="yweb-epg-progress-bar" id="ywebEpgProgressBar" style="width: 0%"></div>
				</div>
				<div class="yweb-epg-description" id="ywebEpgCurrentDesc"></div>
			</div>
			<div class="yweb-epg-next" id="ywebEpgNextSection" style="display: none;">
				<div class="yweb-epg-next-label">Danach</div>
				<div class="yweb-epg-next-title" id="ywebEpgNextTitle">--</div>
				<div class="yweb-epg-next-time" id="ywebEpgNextTime">--:-- - --:--</div>
			</div>
		</div>
		<div class="yweb-epg-modal-footer">
			<button class="yweb-btn" onclick="ywebCloseEpgModal()">Schließen</button>
			<button class="yweb-btn" id="ywebEpgRecBtn" onclick="ywebEpgModalToggleRec()" style="color:var(--yweb-danger,#ef4444)">
				<svg viewBox="0 0 24 24" fill="currentColor" style="width:1rem;height:1rem;margin-right:0.3rem">
					<circle cx="12" cy="12" r="8"/>
				</svg>
				<span>Aufnehmen</span>
			</button>
			<button class="yweb-btn primary" onclick="ywebStreamChannel()">
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;margin-right:0.3rem">
					<path d="M5 17H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/>
					<polygon points="12,15 17,21 7,21"/>
				</svg>
				Stream
			</button>
		</div>
	</div>
</div>
`;

// The channel and the guide event the dialog shows, for its buttons
let ywebEpgModalChannelId = null;
let ywebEpgModalChannelName = null;
let ywebEpgModalEvent = null;
let ywebEpgModalIsRecording = false;
let _ywebEpgModalToken = 0;

function _ywebClock(sec) {
	return ywebFormatTime(new Date(sec * 1000));
}

// A guide read that finds nothing is an answer, not a failure
function _ywebV1Maybe(path, query) {
	return ywebV1('GET', path, {query: query}).catch(function(e) {
		if (e.status === 404) return null;
		throw e;
	});
}

// What runs now on a channel, its long text and what follows
async function ywebShowEpgModal(channelId, channelName) {
	if (!document.getElementById('ywebEpgModal'))
		document.body.insertAdjacentHTML('beforeend', YWEB_EPG_MODAL_HTML);

	const modal = document.getElementById('ywebEpgModal');
	const titleEl = document.getElementById('ywebEpgCurrentTitle');
	const timeEl = document.getElementById('ywebEpgCurrentTime');
	const descEl = document.getElementById('ywebEpgCurrentDesc');
	const progressBar = document.getElementById('ywebEpgProgressBar');
	const channelEl = document.getElementById('ywebEpgModalChannel');
	const nextSection = document.getElementById('ywebEpgNextSection');
	const nextTitleEl = document.getElementById('ywebEpgNextTitle');
	const nextTimeEl = document.getElementById('ywebEpgNextTime');

	const token = ++_ywebEpgModalToken;
	ywebEpgModalChannelId = channelId;
	ywebEpgModalChannelName = channelName || ywebL('epg.programme', 'Programme');
	ywebEpgModalEvent = null;

	channelEl.textContent = ywebEpgModalChannelName;
	titleEl.textContent = ywebL('msg.loading', 'Loading...');
	timeEl.textContent = '--:-- - --:--';
	descEl.textContent = '';
	progressBar.style.width = '0%';
	nextSection.style.display = 'none';

	modal.classList.add('open');
	document.body.style.overflow = 'hidden';

	ywebEpgModalUpdateRecBtn(false);
	ywebCheckRecordingForChannel(channelId).then(function(rec) {
		if (token === _ywebEpgModalToken) ywebEpgModalUpdateRecBtn(!!rec);
	});

	try {
		const ev = await _ywebV1Maybe('/epg/current', {channel: channelId});
		if (token !== _ywebEpgModalToken) return;	// opened for another channel meanwhile
		if (!ev) {
			titleEl.textContent = ywebL('epg.nodata', 'No EPG data available');
			return;
		}
		ywebEpgModalEvent = ev;
		const end = ev.start + ev.duration;
		titleEl.textContent = ev.title || '--';
		timeEl.textContent = _ywebClock(ev.start) + ' - ' + _ywebClock(end);
		descEl.textContent = ev.description || '';
		if (ev.duration > 0) {
			const pct = Math.min(100, Math.max(0, (Date.now() / 1000 - ev.start) / ev.duration * 100));
			progressBar.style.width = pct + '%';
		}

		const full = await _ywebV1Maybe('/epg/event', {id: ev.id, start: ev.start}).catch(function() { return null; });
		if (token !== _ywebEpgModalToken) return;
		if (full)
			descEl.textContent = [full.description, full.long_description].filter(Boolean).join('\n\n');

		const win = await _ywebV1Maybe('/epg', {channel: channelId, from: end, to: end + 6 * 3600}).catch(function() { return null; });
		if (token !== _ywebEpgModalToken) return;
		const next = win && (win.items || []).find(function(e) { return e.start >= end; });
		if (next) {
			nextTitleEl.textContent = next.title || '--';
			nextTimeEl.textContent = _ywebClock(next.start) + ' - ' + _ywebClock(next.start + next.duration);
			nextSection.style.display = 'block';
		}
	} catch (e) {
		if (token !== _ywebEpgModalToken) return;
		titleEl.textContent = ywebL('msg.load_fail', 'Loading failed');
		console.error('Failed to load EPG:', e);
	}
}

function ywebCloseEpgModal(event) {
	if (event && event.target !== event.currentTarget) return;
	const modal = document.getElementById('ywebEpgModal');
	if (modal) modal.classList.remove('open');
	document.body.style.overflow = '';
	// what it showed is no event a later Rec elsewhere may take
	ywebEpgModalEvent = null;
}

// A playlist for a player on this computer: the box's stream server with the
// channel, which wants the id with all 16 digits
function ywebStreamChannel(channelId, channelName) {
	channelId = channelId || ywebEpgModalChannelId;
	channelName = channelName || ywebEpgModalChannelName;
	if (!channelId) return;
	const plist = "#EXTM3U\n#EXTINF:-1," + channelName + "\nhttp://" + window.location.hostname + ":31339/id=" + ywebLegacyChannelId(channelId) + "\n";
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
async function ywebRecordEvent(channelId, channelName, ev) {
	channelId = channelId || ywebEpgModalChannelId;
	channelName = channelName || ywebEpgModalChannelName;
	if (!ev && channelId === ywebEpgModalChannelId) ev = ywebEpgModalEvent;
	if (!channelId) return false;
	try {
		if (!ev) ev = await _ywebV1Maybe('/epg/current', {channel: channelId});
		if (!ev) {
			ywebShowToast(ywebL('epg.nodata', 'No EPG data available'), true);
			return false;
		}
		const minute = Math.floor(Date.now() / 60000) * 60;
		const body = {kind: 'record', channel_id: channelId, start: Math.max(ev.start, minute),
			stop: ev.start + ev.duration, title: ev.title || ''};
		if (ev.id) {
			body.epg_id = ev.id;
			body.epg_start = ev.start;
		}
		await ywebV1('POST', '/timers', {body: body});
		ywebShowToast(ywebL('rec.planned', 'Recording scheduled') + ': ' + (channelName || ''));
		return true;
	} catch (e) {
		console.error('Record failed:', e);
		ywebShowToast(ywebL('msg.error', 'Error') + ': ' + e.message, true);
		return false;
	}
}

// Ends a recording through DELETE /api/v1/recordings/{id}; without an id the
// one the box is recording, its timeshift left out. The id is the timer the
// recording carries, so a timer list can pass its own.
async function ywebStopRecording(recordingId) {
	try {
		if (!recordingId) {
			const r = await ywebV1('GET', '/recordings');
			const rec = ((r && r.items) || []).find(function(it) { return !it.timeshift; });
			if (!rec) {
				ywebShowToast(ywebL('rec.stop_fail', 'Stop failed'), true);
				return false;
			}
			recordingId = rec.id;
		}
		await ywebV1('DELETE', '/recordings/' + encodeURIComponent(recordingId));
		ywebShowToast(ywebL('rec.stopped', 'Recording stopped'));
		return true;
	} catch (e) {
		ywebShowToast(ywebL('rec.stop_fail', 'Stop failed') + ': ' + e.message, true);
		return false;
	}
}

// EPG Modal: toggle between Record and Stop
async function ywebEpgModalToggleRec() {
	if (ywebEpgModalIsRecording) {
		// this channel's recording and no other: one that ended since the
		// dialog opened leaves nothing to stop here
		const rec = await ywebCheckRecordingForChannel(ywebEpgModalChannelId);
		if (!rec || await ywebStopRecording(rec.id))
			ywebEpgModalUpdateRecBtn(false);
	} else if (await ywebRecordEvent()) {
		ywebCheckRecordingForChannel(ywebEpgModalChannelId).then(function(rec) { ywebEpgModalUpdateRecBtn(!!rec); });
	}
}

function ywebEpgModalUpdateRecBtn(isRecording) {
	ywebEpgModalIsRecording = isRecording;
	const btn = document.getElementById('ywebEpgRecBtn');
	if (!btn) return;
	const label = btn.querySelector('span');
	if (label) label.textContent = isRecording ? ywebL('rec.stop', 'Stop recording') : ywebL('rec.start', 'Record');
	btn.style.animation = isRecording ? 'yweb-pulse 1.5s infinite' : '';
}

// The recording the box is making of a channel, its timeshift left out, or null
async function ywebCheckRecordingForChannel(channelId) {
	try {
		const r = await ywebV1('GET', '/recordings');
		return ((r && r.items) || []).find(function(it) {
			return !it.timeshift && it.channel_id === channelId;
		}) || null;
	} catch (e) {
		return null;
	}
}
