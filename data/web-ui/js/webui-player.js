// webui-player.js - a web channel played in the page
//
// Copyright (C) 2026 Thilo Graf
// License: GPL-2.0-or-later
//
// The box fetches the channel on the browser's behalf: its playlist, with
// every address in it replaced by one of the box's own
// (GET /api/v1/webtv/{id}/playlist.m3u8), or, for a channel that is a stream
// rather than a playlist, the stream itself (GET /api/v1/webtv/{id}/stream).
// hls.js (js/hls.min.js, loaded on first use) turns the playlist into what
// the media element plays; a browser without MediaSource that plays HLS by
// itself is handed the playlist directly. Both routes ask for a session or a
// token that may write, so the player asks for a sign-in first where the
// page's session does not reach that far.
//
// Pausing ends every fetch, whichever way the channel plays, and playing on
// starts again at the live edge: a paused live channel would otherwise keep
// the box fetching for nobody. Closing ends everything.
//
// hls.js fetches whatever a playlist names, with the page's session, so it
// is held to the relay of this channel: an address that is not the channel's
// playlist or a part the box minted is not requested at all, and the features
// that fetch addresses of their own (interstitials, content steering) and the
// low latency directives the relay does not take are switched off.
//
// Loaded on the first click by webuiPlayInBrowser (webui-v1.js); needs
// webui-header.js, webui-dom.js and webui-v1.js.

// The one player of the page, built on its first opening.
var _webuiPlayer = null;

var WEBUI_PLAYER_CLOSE_SVG = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true" focusable="false"><path d="M6 18L18 6M6 6l12 12"/></svg>';

function _webuiPlayerUrl(id, what) {
	return '/api/v1/webtv/' + encodeURIComponent(id) + '/' + what;
}

function _webuiPlayerBuild() {
	var p = {channel: null, media: null, hls: null, mode: '', url: '', paused: false,
		quiet: false, resuming: false, token: 0, recovered: false, release: null, probes: []};
	p.title = webuiEl('h3', {id: 'webuiPlayerTitle'});
	var close = p.close = webuiEl('button', {type: 'button', className: 'webui-modal-close',
		attrs: {'aria-label': webuiL('play.close', 'Close player')},
		on: {click: function() { webuiPlayerClose(); }}}, webuiMarkup(WEBUI_PLAYER_CLOSE_SVG));
	p.stage = webuiEl('div', {className: 'webui-player-stage'});
	p.state = webuiEl('p', {className: 'webui-player-state', attrs: {role: 'status'}});
	p.resume = webuiEl('button', {type: 'button', className: 'webui-btn primary', hidden: true,
		on: {click: function() { _webuiPlayerResume(p); }}}, webuiL('play.resume', 'Resume'));
	p.retry = webuiEl('button', {type: 'button', className: 'webui-btn', hidden: true,
		on: {click: function() { _webuiPlayerStart(p); }}}, webuiL('play.retry', 'Try again'));
	p.dialog = webuiEl('div', {className: 'webui-epg-modal webui-player',
		on: {click: function(e) { e.stopPropagation(); }}},
		webuiEl('div', {className: 'webui-epg-modal-header'}, p.title, close),
		webuiEl('div', {className: 'webui-epg-modal-body webui-player-body'},
			p.stage,
			webuiEl('div', {className: 'webui-player-foot'}, p.state, p.resume, p.retry)));
	p.backdrop = webuiEl('div', {className: 'webui-epg-modal-backdrop webui-player-backdrop', id: 'webuiPlayer',
		on: {click: function(e) { if (e.target === p.backdrop) webuiPlayerClose(); }}}, p.dialog);
	document.body.appendChild(p.backdrop);
	return p;
}

/* What the player says under the picture: a text, and which of its two
   buttons goes with it ('resume', 'retry' or none). An error is announced.
   A button that goes away with the focus on it hands the focus on to the
   picture, or to the close button, so it stays in the player. */
function _webuiPlayerSay(p, text, button, error) {
	var had = document.activeElement;
	p.state.textContent = text || '';
	p.state.setAttribute('role', error ? 'alert' : 'status');
	p.state.classList.toggle('error', !!error);
	p.resume.hidden = button !== 'resume';
	p.retry.hidden = button !== 'retry';
	if ((had === p.resume && p.resume.hidden) || (had === p.retry && p.retry.hidden))
		(p.media || p.close).focus();
}

/* Whether an address is on this channel's relay: its playlist or a part the
   box minted, on this origin, with nothing after the path. Nothing else is
   fetched for the player. */
function _webuiPlayerOnRelay(p, url) {
	var u;
	try {
		u = new URL(url, location.href);
	} catch (e) {
		return false;
	}
	if (u.origin !== location.origin || u.search || u.hash)
		return false;
	var base = '/api/v1/webtv/' + encodeURIComponent(p.channel.id) + '/';
	if (u.pathname.indexOf(base) !== 0)
		return false;
	var rest = u.pathname.substring(base.length);
	return rest === 'playlist.m3u8' || /^part\/[0-9a-f]{32}$/.test(rest);
}

// The diagnostic requests in flight are given up: a pause, a close and a new
// start leave none behind.
function _webuiPlayerDropProbes(p) {
	p.probes.splice(0).forEach(function(ctl) { ctl.abort(); });
}

/* Ends every fetch and leaves the element empty. quiet keeps the element's
   own events out of the player's reading while it is being emptied. */
function _webuiPlayerStop(p) {
	_webuiPlayerDropProbes(p);
	p.quiet = true;
	if (p.hls) {
		p.hls.destroy();
		p.hls = null;
	}
	if (p.media) {
		p.media.pause();
		p.media.removeAttribute('src');
		p.media.load();
	}
	p.quiet = false;
}

// Everything stops and the element goes: what it reports afterwards is about
// nothing the player still shows.
function _webuiPlayerLetGo(p) {
	p.resuming = false;
	_webuiPlayerStop(p);
	p.stage.replaceChildren();
	p.media = null;
}

/* Plays, and where the browser wants a click of its own before it plays with
   sound, says so the way a pause does, with Resume to click. */
function _webuiPlayerPlay(p, m) {
	var started = m.play();
	if (started && started.catch)
		started.catch(function(e) {
			if (e && e.name === 'NotAllowedError' && p.media === m) {
				p.resuming = false;
				_webuiPlayerPaused(p, m);
			}
		});
}

// The element for this channel: a picture for television, sound for radio.
function _webuiPlayerMedia(p) {
	var radio = p.channel.kind === 'webradio';
	var m = webuiEl(radio ? 'audio' : 'video', {className: 'webui-player-media',
		attrs: {controls: '', playsinline: '', 'aria-label': p.channel.name}});
	m.addEventListener('pause', function() { _webuiPlayerPaused(p, m); });
	m.addEventListener('play', function() { _webuiPlayerPlayed(p, m); });
	m.addEventListener('playing', function() {
		if (p.media !== m || p.quiet) return;
		p.resuming = false;
		_webuiPlayerSay(p, '', '');
	});
	m.addEventListener('ended', function() {
		if (p.media !== m || p.quiet) return;
		_webuiPlayerLetGo(p);
		_webuiPlayerSay(p, webuiL('play.ended', 'The stream ended'), 'retry');
	});
	m.addEventListener('error', function() { _webuiPlayerMediaError(p, m); });
	p.stage.classList.toggle('radio', radio);
	p.stage.replaceChildren(m);
	p.media = m;
	return m;
}

/* Why an address of the relay does not play, asked of the box itself: the
   refusal it answers, or null where it answers (a stream the browser cannot
   decode). An answer that is not a refusal is cut off at once, so the box's
   fetch for it ends with it. The request is the player's, given up by a pause,
   a close and a new start. */
async function _webuiPlayerWhy(p, url) {
	var ctl = typeof AbortController === 'function' ? new AbortController() : null;
	if (ctl)
		p.probes.push(ctl);
	try {
		var resp = await webuiFetchBox(url, {credentials: 'same-origin', cache: 'no-store',
			signal: ctl ? ctl.signal : undefined});
		if (resp.ok) {
			if (ctl) ctl.abort();
			return null;
		}
		return await _webuiV1Error(resp, 'GET', url);
	} catch (e) {
		return e;
	} finally {
		var at = p.probes.indexOf(ctl);
		if (at >= 0)
			p.probes.splice(at, 1);
	}
}

// Whether what a request learnt still matters: the same start, not paused.
function _webuiPlayerCurrent(p, token) {
	return token === p.token && !p.paused;
}

// A failure: everything stops, and the reason with a way to try again.
function _webuiPlayerFail(p, err) {
	_webuiPlayerLetGo(p);
	_webuiPlayerSay(p, err ? webuiErrText(err) : webuiL('play.no_codec', 'This browser cannot play this stream'),
		'retry', true);
}

// The channel's address as a stream, for a channel the box says is no playlist.
function _webuiPlayerStream(p, token) {
	if (!_webuiPlayerCurrent(p, token)) return;
	_webuiPlayerStop(p);
	p.mode = 'stream';
	p.url = _webuiPlayerUrl(p.channel.id, 'stream');
	var m = _webuiPlayerMedia(p);
	m.src = p.url;
	_webuiPlayerPlay(p, m);
}

// A playlist the box could not hand over: a stream instead, or the reason.
async function _webuiPlayerNoPlaylist(p, token) {
	var why = await _webuiPlayerWhy(p, _webuiPlayerUrl(p.channel.id, 'playlist.m3u8'));
	if (!_webuiPlayerCurrent(p, token)) return;
	if (why && why.code === 'not-a-playlist') {
		_webuiPlayerStream(p, token);
		return;
	}
	_webuiPlayerFail(p, why);
}

async function _webuiPlayerMediaError(p, m) {
	if (p.media !== m || p.quiet) return;
	var token = p.token;
	if (p.mode === 'native') {
		await _webuiPlayerNoPlaylist(p, token);
		return;
	}
	if (p.mode !== 'stream') return;
	var why = await _webuiPlayerWhy(p, p.url);
	if (_webuiPlayerCurrent(p, token)) _webuiPlayerFail(p, why);
}

function _webuiPlayerHlsError(p, token, data) {
	if (!_webuiPlayerCurrent(p, token) || p.quiet || !data) return;
	var named = [data.url, data.context && data.context.url,
		data.frag && data.frag.decryptdata && data.frag.decryptdata.uri,
		data.frag && data.frag.url].filter(Boolean);
	// an address off the relay was never asked for, and is not asked about:
	// the playlist that names it does not play here, retried or not
	if (named.some(function(u) { return !_webuiPlayerOnRelay(p, u); })) {
		_webuiPlayerFail(p, {status: 0, message: webuiL('play.off_relay', 'This stream names an address the box does not relay')});
		return;
	}
	if (!data.fatal) return;
	var d = Hls.ErrorDetails;
	if (data.details === d.MANIFEST_LOAD_ERROR || data.details === d.MANIFEST_PARSING_ERROR) {
		_webuiPlayerNoPlaylist(p, token);
		return;
	}
	if (data.details === d.MANIFEST_INCOMPATIBLE_CODECS_ERROR ||
	    data.details === d.BUFFER_ADD_CODEC_ERROR ||
	    data.details === d.BUFFER_INCOMPATIBLE_CODECS_ERROR) {
		_webuiPlayerFail(p, null);
		return;
	}
	if (data.type === Hls.ErrorTypes.MEDIA_ERROR && !p.recovered) {
		p.recovered = true;
		p.hls.recoverMediaError();
		return;
	}
	if (data.type === Hls.ErrorTypes.MEDIA_ERROR) {
		_webuiPlayerFail(p, null);
		return;
	}
	var url = named[0] || '';
	if (!url) {
		_webuiPlayerFail(p, {status: (data.response && data.response.code) || 0, message: ''});
		return;
	}
	_webuiPlayerWhy(p, url).then(function(why) {
		if (_webuiPlayerCurrent(p, token))
			_webuiPlayerFail(p, why || {status: (data.response && data.response.code) || 0, message: ''});
	});
}

// hls.js on the element m, from the channel's playlist, at the live edge.
function _webuiPlayerHlsOn(p, token, m) {
	var hls = new Hls({
		enableWorker: false,
		lowLatencyMode: false,
		enableInterstitialPlayback: false,
		interstitialsController: null,
		contentSteeringController: null,
		xhrSetup: function(xhr, url) {
			if (!_webuiPlayerOnRelay(p, url))
				throw new Error('not an address of the relay');
		}
	});
	p.hls = hls;
	hls.on(Hls.Events.ERROR, function(ev, data) { _webuiPlayerHlsError(p, token, data); });
	hls.on(Hls.Events.MANIFEST_PARSED, function() {
		if (token === p.token) _webuiPlayerPlay(p, m);
	});
	hls.attachMedia(m);
	hls.loadSource(p.url);
}

function _webuiPlayerHls(p, token) {
	p.mode = 'hls';
	p.url = _webuiPlayerUrl(p.channel.id, 'playlist.m3u8');
	_webuiPlayerHlsOn(p, token, _webuiPlayerMedia(p));
}

function _webuiPlayerNative(p) {
	p.mode = 'native';
	p.url = _webuiPlayerUrl(p.channel.id, 'playlist.m3u8');
	var m = _webuiPlayerMedia(p);
	m.src = p.url;
	_webuiPlayerPlay(p, m);
}

// Whether this page may have the box fetch: a session or a token that may
// write, else the sign-in, which may be declined.
async function _webuiPlayerMayFetch() {
	try {
		var resp = await webuiFetchBox('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
		var s = resp.ok ? await resp.json() : null;
		if (s && (s.level === 'write' || s.level === 'system') && !s.password_change_required)
			return true;
	} catch (e) { /* asked for below */ }
	return webuiV1SignIn();
}

async function _webuiPlayerStart(p) {
	var token = ++p.token;
	_webuiPlayerLetGo(p);
	p.paused = false;
	p.recovered = false;
	_webuiPlayerSay(p, webuiL('play.loading', 'Starting playback…'), '');

	if (!await _webuiPlayerMayFetch()) {
		if (token === p.token)
			_webuiPlayerSay(p, webuiL('err.sign_in', 'Only after signing in'), 'retry', true);
		return;
	}
	if (token !== p.token) return;

	var probe = document.createElement(p.channel.kind === 'webradio' ? 'audio' : 'video');
	var nativeHls = !!probe.canPlayType && probe.canPlayType('application/vnd.apple.mpegurl') !== '';
	if (typeof MediaSource === 'undefined' && nativeHls) {
		_webuiPlayerNative(p);
		return;
	}
	try {
		if (typeof Hls === 'undefined')
			await webuiLoadScript('/js/hls.min.js?v=1');
	} catch (e) {
		if (token === p.token) _webuiPlayerFail(p, e);
		return;
	}
	if (token !== p.token) return;
	if (Hls.isSupported()) {
		_webuiPlayerHls(p, token);
	} else if (nativeHls) {
		_webuiPlayerNative(p);
	} else {
		// no MediaSource at all: a stream is the one thing left to try
		_webuiPlayerNoPlaylist(p, token);
	}
}

/* Paused by the user: every fetch ends. hls.js stops loading and keeps what
   it holds; a playlist or a stream the element fetches itself is let go. */
function _webuiPlayerPaused(p, m) {
	/* An element that failed stops by itself, and one being started again can
	   still report the pause its emptying caused: neither is the user's. */
	if (p.media !== m || p.quiet || p.paused || p.resuming || m.ended || m.error) return;
	p.paused = true;
	_webuiPlayerDropProbes(p);
	if (p.hls) {
		p.hls.stopLoad();
	} else {
		p.quiet = true;
		m.removeAttribute('src');
		m.load();
		p.quiet = false;
	}
	_webuiPlayerSay(p, webuiL('play.paused', 'Paused – playback resumes live'), 'resume');
}

// Played on, from the element's own control or from Resume: live again.
function _webuiPlayerPlayed(p, m) {
	if (p.media !== m || p.quiet || !p.paused) return;
	_webuiPlayerResume(p);
}

function _webuiPlayerResume(p) {
	if (!p.paused || !p.media) return;
	p.paused = false;
	p.resuming = true;
	_webuiPlayerSay(p, webuiL('play.loading', 'Starting playback…'), '');
	if (p.hls) {
		/* A new hls.js on the same element starts at the live edge; the paused
		   one would go on from where it stopped, however far behind that is. */
		p.quiet = true;
		p.hls.destroy();
		p.hls = null;
		p.quiet = false;
		_webuiPlayerHlsOn(p, p.token, p.media);
		return;
	}
	if (!p.media.getAttribute('src'))
		p.media.src = p.url;
	_webuiPlayerPlay(p, p.media);
}

/* Opens the player for a web channel ({id, name, kind}) and starts it. */
function webuiPlayerOpen(channel) {
	if (!_webuiPlayer)
		_webuiPlayer = _webuiPlayerBuild();
	var p = _webuiPlayer;
	p.channel = {id: String(channel.id), name: channel.name || '', kind: channel.kind || 'webtv'};
	p.title.textContent = p.channel.name || webuiL('play.in_browser', 'Play in browser');
	p.backdrop.classList.add('open');
	document.body.style.overflow = 'hidden';
	if (p.release)
		p.release();
	p.release = webuiDialogA11y(p.dialog, {close: function() { webuiPlayerClose(); }});
	_webuiPlayerStart(p);
}

// Ends playback and every fetch, and closes the player.
function webuiPlayerClose() {
	var p = _webuiPlayer;
	if (!p) return;
	++p.token;
	_webuiPlayerLetGo(p);
	p.backdrop.classList.remove('open');
	document.body.style.overflow = '';
	if (p.release) {
		p.release();
		p.release = null;
	}
}
