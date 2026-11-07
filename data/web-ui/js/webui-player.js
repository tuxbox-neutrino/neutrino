// webui-player.js - a channel played in the page
//
// Copyright (C) 2026 Thilo Graf
// License: GPL-2.0-or-later
//
// A web channel: the box fetches it on the browser's behalf, its playlist,
// with every address in it replaced by one of the box's own
// (GET /api/v1/webtv/{id}/playlist.m3u8), or, for a channel that is a stream
// rather than a playlist, the stream itself (GET /api/v1/webtv/{id}/stream).
// hls.js (js/hls.min.js, loaded on first use) turns the playlist into what
// the media element plays; a browser without MediaSource that plays HLS by
// itself is handed the playlist directly.
//
// A channel from the box's tuners: the box converts it for the browser
// (GET /api/v1/stream/browser/{id}), the picture as it is and the sound to
// AAC where a browser would not decode it. A channel with a picture comes as
// a transport stream, which mpegts.js (js/mpegts.js, loaded on first use)
// plays; radio comes as AAC alone, which an audio element plays by itself.
// The box starts it for the page's session only where the request shows it
// came from a page of the box: the fetch of a picture sends the session's
// token, and the audio element of a radio channel, which can send no header,
// a ticket in its address that stands in for that token
// (POST /api/v1/session/ticket).
// The page reads the channel first, for what it carries and which sound to
// take, and asks with a HEAD whether the box would start the stream before
// it fetches it: a HEAD starts nothing and takes no tuner. A fetch that
// fails and a stream that ends are asked about the same way, since neither
// says why. Where the channel does not play here, the player offers its
// playlist for a player on this computer.
//
// Both kinds of route ask for a session or a token that may write, so the
// player asks for a sign-in first where the page's session does not reach
// that far.
//
// Pausing ends every fetch, whichever way the channel plays, and playing on
// starts again at the live edge: a paused live channel would otherwise keep
// the box fetching for nobody, and a converted one would keep a tuner and
// one of the box's conversions. Closing ends everything.
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

/* What the box refuses a converted channel with, by the status of its answer
   to a HEAD, which has no body to read the reason from. On that route each
   of these statuses stands for one refusal; a 503 is the box converting as
   many streams as it can where it names a time to wait, and otherwise the
   box not learning in time whether a tuner is free. */
var WEBUI_PLAYER_REFUSALS = {
	404: 'no-such-channel',
	409: 'no-free-tuner',
	500: 'no-supervision',
	501: 'not-playable-in-browser'
};

// How often, and how far apart, a channel the box had not tuned is read again
// once its stream starts, for what it carries.
var WEBUI_PLAYER_LEARN_TIMES = 15;
var WEBUI_PLAYER_LEARN_MS = 2000;

function _webuiPlayerUrl(id, what) {
	return '/api/v1/webtv/' + encodeURIComponent(id) + '/' + what;
}

// Whether a channel comes from the internet rather than from the tuners.
function _webuiPlayerWeb(kind) {
	return kind === 'webtv' || kind === 'webradio';
}

function _webuiPlayerBuild() {
	var p = {channel: null, media: null, hls: null, ts: null, mode: '', url: '', paused: false,
		quiet: false, resuming: false, token: 0, recovered: false, release: null, probes: [],
		streams: null, note: '', started: false, drained: false, learn: 0};
	p.title = webuiEl('h3', {id: 'webuiPlayerTitle'});
	var close = p.close = webuiEl('button', {type: 'button', className: 'webui-modal-close',
		attrs: {'aria-label': webuiL('play.close', 'Close player')},
		on: {click: function() { webuiPlayerClose(); }}}, webuiMarkup(WEBUI_PLAYER_CLOSE_SVG));
	p.stage = webuiEl('div', {className: 'webui-player-stage'});
	p.reason = webuiEl('span', {className: 'webui-player-reason'});
	p.hint = webuiEl('span', {className: 'webui-player-hint', hidden: true});
	p.state = webuiEl('p', {className: 'webui-player-state', attrs: {role: 'status'}}, p.reason, p.hint);
	p.resume = webuiEl('button', {type: 'button', className: 'webui-btn primary', hidden: true,
		on: {click: function() { _webuiPlayerResume(p); }}}, webuiL('play.resume', 'Resume'));
	p.retry = webuiEl('button', {type: 'button', className: 'webui-btn', hidden: true,
		on: {click: function() { _webuiPlayerStart(p); }}}, webuiL('play.retry', 'Try again'));
	p.external = webuiEl('button', {type: 'button', className: 'webui-btn webui-player-external', hidden: true,
		on: {click: function() { webuiOpenInPlayer(p.channel.id); }}}, webuiL('play.external', 'Open in a player (M3U)'));
	p.dialog = webuiEl('div', {className: 'webui-epg-modal webui-player',
		on: {click: function(e) { e.stopPropagation(); }}},
		webuiEl('div', {className: 'webui-epg-modal-header'}, p.title, close),
		webuiEl('div', {className: 'webui-epg-modal-body webui-player-body'},
			p.stage,
			webuiEl('div', {className: 'webui-player-foot'}, p.state, p.resume, p.retry, p.external)));
	p.backdrop = webuiEl('div', {className: 'webui-epg-modal-backdrop webui-player-backdrop', id: 'webuiPlayer',
		on: {click: function(e) { if (e.target === p.backdrop) webuiPlayerClose(); }}}, p.dialog);
	document.body.appendChild(p.backdrop);
	return p;
}

/* What the player says under the picture: a text, a hint below it, and
   which buttons go with it: 'resume'; 'retry', which comes with the
   channel's playlist for a player on this computer where the channel is one
   of the tuners'; 'retry-only', where the playlist would meet the same
   refusal; 'external', the playlist alone as the way out, for a channel no
   browser plays or beside the start of one the channel list says sends
   MPEG-2; or none. An error is announced. A button that goes away
   with the focus on it hands the focus on to the picture, or to the close
   button, so it stays in the player. */
function _webuiPlayerSay(p, text, button, error, hint) {
	var had = document.activeElement;
	p.reason.textContent = text || '';
	p.hint.textContent = hint || '';
	p.hint.hidden = !hint;
	p.state.setAttribute('role', error ? 'alert' : 'status');
	p.state.classList.toggle('error', !!error);
	p.resume.hidden = button !== 'resume';
	p.retry.hidden = button !== 'retry' && button !== 'retry-only';
	p.external.hidden = !(button === 'external' || (button === 'retry' && !_webuiPlayerWeb(p.channel.kind)));
	p.external.classList.toggle('primary', button === 'external');
	if ([p.resume, p.retry, p.external].some(function(b) { return had === b && b.hidden; }))
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
	if (p.ts) {
		p.ts.destroy();
		p.ts = null;
	}
	if (p.media) {
		p.media.pause();
		p.media.removeAttribute('src');
		p.media.load();
	}
	p.drained = false;
	p.quiet = false;
}

// Everything stops and the element goes: what it reports afterwards is about
// nothing the player still shows.
function _webuiPlayerLetGo(p) {
	p.resuming = false;
	clearTimeout(p.learn);
	p.learn = 0;
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
	var radio = p.channel.kind === 'webradio' || p.channel.kind === 'radio';
	var m = webuiEl(radio ? 'audio' : 'video', {className: 'webui-player-media',
		attrs: {controls: '', playsinline: '', 'aria-label': p.channel.name}});
	m.addEventListener('pause', function() { _webuiPlayerPaused(p, m); });
	m.addEventListener('play', function() { _webuiPlayerPlayed(p, m); });
	m.addEventListener('playing', function() {
		if (p.media !== m || p.quiet) return;
		p.resuming = false;
		p.started = true;
		_webuiPlayerSay(p, p.note, '');
	});
	m.addEventListener('ended', function() {
		if (p.media !== m || p.quiet) return;
		if (p.streams) {
			_webuiPlayerOver(p, p.token);
			return;
		}
		_webuiPlayerLetGo(p);
		_webuiPlayerSay(p, webuiL('play.ended', 'The stream ended'), 'retry');
	});
	// what the box sent before it ended a converted stream has played out
	m.addEventListener('waiting', function() {
		if (p.media === m && !p.quiet && p.drained)
			_webuiPlayerOver(p, p.token);
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
			headers: webuiV1SessionHeaders(), signal: ctl ? ctl.signal : undefined});
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

/* Whether the box would start the converted stream now, asked with a HEAD,
   which starts nothing: null where it would, else the refusal, its code told
   by the status. Given up like the requests above. */
async function _webuiPlayerAsk(p, url) {
	var ctl = typeof AbortController === 'function' ? new AbortController() : null;
	if (ctl)
		p.probes.push(ctl);
	try {
		var resp = await webuiFetchBox(url, {method: 'HEAD', credentials: 'same-origin', cache: 'no-store',
			signal: ctl ? ctl.signal : undefined});
		if (resp.ok)
			return null;
		var err = new Error('');
		err.status = resp.status;
		err.code = WEBUI_PLAYER_REFUSALS[resp.status] || '';
		if (resp.status === 503)
			err.code = resp.headers.get('Retry-After') ? 'too-many-conversions' : 'upstream-unreachable';
		return err;
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

/* A failure: everything stops, and the reason with a way on. A channel no
   browser plays is told by its name and, where that is the reason, by the
   coding of its picture, with its playlist as the way out and nothing to try
   again; the coding is noted, so the page offers the channel that way from
   now on. No free tuner is explained, with a way to try again once the TV
   has moved; the playlist would be refused the same way. */
function _webuiPlayerFail(p, err) {
	_webuiPlayerLetGo(p);
	if (err && err.code === 'not-playable-in-browser') {
		var video = p.streams ? p.streams.video : '';
		var named = video && ['h264', 'hevc', 'none', 'unknown'].indexOf(video) < 0;
		if (named)
			webuiNoteVideo(p.channel.id, video);
		_webuiPlayerSay(p, webuiUnplayableText(p.channel.name, named ? video : ''), 'external', true,
			webuiL('play.unplayable_hint', 'Open the channel in a player instead (M3U).'));
		return;
	}
	if (err && err.code === 'no-free-tuner') {
		_webuiPlayerSay(p, webuiErrText(err), 'retry-only', true,
			webuiL('play.no_tuner_hint', 'The TV picture or a recording holds the tuner. A channel on the same transponder still plays, and in standby any channel does, except one being recorded.'));
		return;
	}
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
	if (p.mode === 'adts') {
		_webuiPlayerOver(p, token);
		return;
	}
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
		if (s && (s.level === 'write' || s.level === 'system') && !s.password_change_required) {
			// the converted stream sends the session's token (webuiV1SessionHeaders)
			webuiV1KeepSession(s);
			return true;
		}
	} catch (e) { /* asked for below */ }
	return webuiV1SignIn();
}

// A web channel: hls.js, the browser's own HLS, or the channel as a stream.
async function _webuiPlayerWebStart(p, token) {
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

/* What the box is asked to send for a channel from the tuners, from what it
   knows of the channel: the picture's coding ('none' for radio), the sound's,
   and the sound track by its pid. The track is an AAC one in the language of
   the selected track, which the box passes on as it is, else the selected
   one, which it converts. A channel the box has not tuned since it started
   lists no tracks, and its sound is 'unknown': the box then converts the
   first track, whatever it is, and its picture is 'unknown' unless the box's
   list of channels names it or the channel's hint is a coding the box read
   off the channel before (source stream). */
function _webuiPlayerStreams(ch, radio) {
	var tracks = ch && ch.streams_known && Array.isArray(ch.audio) ? ch.audio : [];
	var chosen = tracks.filter(function(a) { return a.selected; })[0] || tracks[0] || null;
	if (chosen && chosen.codec !== 'aac') {
		var aac = tracks.filter(function(a) {
			return a.codec === 'aac' && a.description === chosen.description;
		})[0];
		if (aac) chosen = aac;
	}
	var video = ch && ch.video ? ch.video.codec : '';
	if ((!video || video === 'unknown') && ch && ch.video_codec_hint_source === 'stream')
		video = ch.video_codec_hint;
	return {
		radio: radio,
		video: radio ? 'none' : (video && video !== 'none' ? video : 'unknown'),
		audio: chosen && chosen.codec ? chosen.codec : 'unknown',
		apid: chosen && chosen.pid ? chosen.pid : 0
	};
}

/* Whether this browser plays what the box would send: AAC in an audio
   element for radio, else a transport stream read as it arrives and played
   through MediaSource, with an H.264 or H.265 picture; a picture the box has
   not read yet is taken for H.264, the coding of most channels. A picture no
   browser plays is the box's to refuse, and it says so. */
function _webuiPlayerCanPlay(s) {
	if (s.radio) {
		var a = document.createElement('audio');
		return !!a.canPlayType && a.canPlayType('audio/aac') !== '';
	}
	var ms = window.ManagedMediaSource || window.MediaSource;
	if (!ms || !ms.isTypeSupported || !window.fetch || typeof ReadableStream === 'undefined')
		return false;
	if (s.video === 'hevc')
		return ms.isTypeSupported('video/mp4; codecs="hvc1.1.6.L93.B0,mp4a.40.2"');
	if (s.video === 'h264' || s.video === 'unknown')
		return ms.isTypeSupported('video/mp4; codecs="avc1.42E01E,mp4a.40.2"');
	return true;
}

function _webuiPlayerBrowserUrl(id, s) {
	var url = '/api/v1/stream/browser/' + encodeURIComponent(id) +
		'?video=' + encodeURIComponent(s.video) + '&audio=' + encodeURIComponent(s.audio);
	return s.apid ? url + '&apid=' + s.apid : url;
}

/* The converted stream on the element m, from the live edge: through
   mpegts.js for a picture, by the element itself for radio. mpegts.js reports
   every failure of its own, and the box ending the stream as the end of
   what it loads; what was sent before plays out first. A picture the box has
   not read yet is read again while the stream starts, at the first start and
   after a pause alike. */
function _webuiPlayerLive(p, token, m) {
	p.started = false;
	p.drained = false;
	if (p.streams.radio) {
		p.mode = 'adts';
		_webuiPlayerRadio(p, token, m);
		return;
	}
	p.mode = 'ts';
	/* The session's token goes with the fetch: the box starts the conversion
	   for a session only where the request shows it came from a page of the
	   box, and a browser on a plain address sends no Sec-Fetch-Site and may be
	   set to send no Referer. */
	var ts = mpegts.createPlayer({type: 'mpegts', isLive: true, cors: false, url: p.url},
		{enableStashBuffer: false, liveBufferLatencyChasing: true, headers: webuiV1SessionHeaders()});
	p.ts = ts;
	ts.on(mpegts.Events.ERROR, function() {
		if (p.ts === ts && !p.quiet) _webuiPlayerOver(p, token);
	});
	ts.on(mpegts.Events.LOADING_COMPLETE, function() {
		if (p.ts !== ts || p.quiet) return;
		p.drained = true;
		if (!p.started || m.ended)
			_webuiPlayerOver(p, token);
	});
	ts.attachMediaElement(m);
	ts.load();
	_webuiPlayerPlay(p, m);
	if (p.streams.video === 'unknown')
		_webuiPlayerLearn(p, token, WEBUI_PLAYER_LEARN_TIMES);
}

/* A converted stream that failed or ended: everything stops, and the box is
   asked with a HEAD why, since neither the failed fetch nor the end says it.
   A refusal is the reason; where the box would start the stream again, the
   stream ended, or did not start. */
async function _webuiPlayerOver(p, token) {
	if (!_webuiPlayerCurrent(p, token)) return;
	var started = p.started;
	_webuiPlayerLetGo(p);
	var why = await _webuiPlayerAsk(p, p.url);
	if (!_webuiPlayerCurrent(p, token)) return;
	_webuiPlayerFail(p, why || {status: 0, message: started
		? webuiL('play.ended', 'The stream ended')
		: webuiL('play.start_failed', 'The stream could not be started')});
}

/* Radio plays in an audio element, which sends no header of its own: its
   address carries a ticket in place of the session's token, which the box
   hands a page of its own for this stream and this session alone, for a
   minute. Asked for at every start, a pause having ended the conversion
   before. Without a session there is no token to stand in for, and the box
   asks for none. What the answer finds is acted on only for the start that
   asked for it, the element included. */
async function _webuiPlayerRadio(p, token, m) {
	var url = p.url;
	if (Object.keys(webuiV1SessionHeaders()).length) {
		var got;
		try {
			got = await webuiV1('POST', '/session/ticket', {body: {path: url.split('?')[0]}, mark: false});
		} catch (e) {
			got = e;
		}
		if (!_webuiPlayerCurrent(p, token) || p.media !== m) return;
		if (got instanceof Error) {
			_webuiPlayerFail(p, got);
			return;
		}
		if (got && got.ticket)
			url += '&ticket=' + encodeURIComponent(got.ticket);
	}
	m.src = url;
	_webuiPlayerPlay(p, m);
}

/* A channel the box had not tuned, read again while its stream starts,
   until the box has learnt what it carries: a picture this browser does not
   play ends the stream with the reason, rather than leaving the sound to
   play under a black picture. Not while paused: the box holds no tuner for
   the channel then and learns nothing. One timer at most, for the stream it
   was started with: one that ended, failed or was played on after a pause
   while the channel was being read is not asked about again. The picture
   once learnt is kept, so playing on after a pause does not ask again. */
function _webuiPlayerLearn(p, token, times) {
	clearTimeout(p.learn);
	p.learn = setTimeout(async function() {
		p.learn = 0;
		var ts = p.ts;
		if (!_webuiPlayerCurrent(p, token) || !ts) return;
		var ch = await _webuiV1Read('/channels/' + encodeURIComponent(p.channel.id));
		if (!_webuiPlayerCurrent(p, token) || p.ts !== ts) return;
		var s = ch && ch.streams_known ? _webuiPlayerStreams(ch, false) : null;
		if (!s || s.video === 'unknown') {
			if (times > 1) _webuiPlayerLearn(p, token, times - 1);
			return;
		}
		p.streams.video = s.video;
		webuiNoteVideo(p.channel.id, s.video);
		if (s.video !== 'h264' && s.video !== 'hevc')
			_webuiPlayerFail(p, {status: 501, code: 'not-playable-in-browser', message: ''});
		else if (!_webuiPlayerCanPlay(s))
			_webuiPlayerFail(p, null);
	}, WEBUI_PLAYER_LEARN_MS);
}

/* A channel from the tuners: read, checked against what this browser plays,
   asked about with a HEAD, then played. A channel that turns out to be a web
   channel goes the way of those. */
async function _webuiPlayerTunerStart(p, token) {
	var ch;
	try {
		ch = await webuiV1('GET', '/channels/' + encodeURIComponent(p.channel.id));
	} catch (e) {
		if (token === p.token) _webuiPlayerFail(p, e);
		return;
	}
	if (token !== p.token) return;
	if (ch && _webuiPlayerWeb(ch.kind)) {
		p.channel.kind = ch.kind;
		await _webuiPlayerWebStart(p, token);
		return;
	}
	if (ch && ch.kind)
		p.channel.kind = ch.kind;
	p.streams = _webuiPlayerStreams(ch, p.channel.kind === 'radio');
	if (!p.streams.radio)
		webuiNoteVideo(p.channel.id, p.streams.video);
	if (!_webuiPlayerCanPlay(p.streams)) {
		_webuiPlayerFail(p, null);
		return;
	}
	p.url = _webuiPlayerBrowserUrl(p.channel.id, p.streams);
	var why = await _webuiPlayerAsk(p, p.url);
	if (token !== p.token) return;
	if (why) {
		_webuiPlayerFail(p, why);
		return;
	}
	if (!p.streams.radio) {
		try {
			if (typeof mpegts === 'undefined')
				await webuiLoadScript('/js/mpegts.js?v=1');
		} catch (e) {
			if (token === p.token) _webuiPlayerFail(p, e);
			return;
		}
		if (token !== p.token) return;
		// it logs every step to the console; what goes wrong the player says
		mpegts.LoggingControl.enableAll = false;
		if (!mpegts.getFeatureList().mseLivePlayback) {
			_webuiPlayerFail(p, null);
			return;
		}
	}
	p.note = p.streams.audio !== 'aac' ? webuiL('play.sound_converted', 'Sound converted to AAC') : '';
	_webuiPlayerLive(p, token, _webuiPlayerMedia(p));
}

async function _webuiPlayerStart(p) {
	var token = ++p.token;
	_webuiPlayerLetGo(p);
	p.paused = false;
	p.recovered = false;
	p.streams = null;
	p.note = '';
	// a channel the channel list says sends MPEG-2 is tried, the way to a
	// player beside it while it starts
	var doubt = webuiPlayDoubt(p.channel.id, p.channel.name);
	if (doubt && doubt.sure)
		doubt = null;
	_webuiPlayerSay(p, webuiL('play.loading', 'Starting playback…'), doubt ? 'external' : '', false, doubt ? doubt.text : '');

	if (!await _webuiPlayerMayFetch()) {
		if (token === p.token)
			_webuiPlayerSay(p, webuiL('err.sign_in', 'Only after signing in'), 'retry', true);
		return;
	}
	if (token !== p.token) return;
	if (_webuiPlayerWeb(p.channel.kind))
		await _webuiPlayerWebStart(p, token);
	else
		await _webuiPlayerTunerStart(p, token);
}

/* Paused by the user: every fetch ends, reading the channel again included.
   hls.js stops loading and keeps what it holds; mpegts.js goes, and with it
   the box's conversion and its tuner; a playlist or a stream the element
   fetches itself is let go. */
function _webuiPlayerPaused(p, m) {
	/* An element that failed stops by itself, and one being started again can
	   still report the pause its emptying caused: neither is the user's. */
	if (p.media !== m || p.quiet || p.paused || p.resuming || m.ended || m.error) return;
	p.paused = true;
	clearTimeout(p.learn);
	p.learn = 0;
	_webuiPlayerDropProbes(p);
	if (p.hls) {
		p.hls.stopLoad();
	} else {
		p.quiet = true;
		if (p.ts) {
			p.ts.destroy();
			p.ts = null;
		}
		m.removeAttribute('src');
		m.load();
		p.drained = false;
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
	if (p.mode === 'ts' || p.mode === 'adts') {
		// a new conversion: the box ended the last one with the pause
		_webuiPlayerLive(p, p.token, p.media);
		return;
	}
	if (!p.media.getAttribute('src'))
		p.media.src = p.url;
	_webuiPlayerPlay(p, p.media);
}

/* Opens the player for a channel ({id, name, kind}) and starts it. A kind
   that is not given is read with the channel. */
function webuiPlayerOpen(channel) {
	if (!_webuiPlayer)
		_webuiPlayer = _webuiPlayerBuild();
	var p = _webuiPlayer;
	p.channel = {id: String(channel.id), name: channel.name || '', kind: channel.kind || ''};
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
