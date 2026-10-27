// The script of dashboard.html: the overview, variant A of Phase 4 (what
// runs now as a band, the tiles below it).

// Show EPG info for current channel
function showCurrentEpgInfo() {
	if (dash.channel)
		webuiShowEpgModal(dash.channel.id, dash.channel.name, {number: dash.channel.number});
}

// Format helpers — use shared versions from webui-header.js
var formatKB = webuiFormatKB;
var formatUptime = webuiFormatUptime;

// DOM micro-helpers replacing the previous jQuery calls. Null-safe:
// a missing id is a silent no-op, like an empty jQuery set (the
// versions handler e.g. writes to an id the markup does not have).
function withEl(id, fn) {
	var el = document.getElementById(id);
	if (el) fn(el);
}
function setText(id, str) {
	withEl(id, function(el) { el.textContent = str; });
}

/* A gauge of the box's load: its ring filled to percent, the value in it
   (also the ring's name for a screen reader), a line below its own name,
   and the warning colours from 70 and 90 percent on, or the level asked
   for ('ok' green, '' the accent). */
function setGauge(id, percent, value, sub, level) {
	withEl(id, function(el) {
		var pct = Math.max(0, Math.min(100, Math.round(percent || 0)));
		el.querySelector('.fill').style.strokeDasharray = (226.2 * pct / 100).toFixed(1) + ' 226.2';
		var ring = el.querySelector('.webui-gauge-ring');
		ring.querySelector('b').textContent = value;
		ring.setAttribute('aria-label', value);
		el.querySelector('p').textContent = sub || '';
		el.classList.remove('ok', 'warn', 'danger', 'empty');
		var cls = level !== undefined ? level : pct >= 90 ? 'danger' : pct >= 70 ? 'warn' : '';
		if (cls) el.classList.add(cls);
		if (pct === 0) el.classList.add('empty');
	});
}

// The CPU's load, which is no percentage: its own levels
function getCpuLevel(load) {
	if (load >= 4.0) return 'danger';
	if (load >= 2.0) return 'warn';
	return '';
}

// ---- Following the box over /api/v1 ----------------------------------
// Channel, guide, recordings, timers, mode, standby and playback follow
// the event stream (/api/v1/events) instead of asking on a timer: each
// event rereads the one tile it concerns, and a tile only touches what
// changed. What v1 has no event for (system, network, tuner signal, stream
// details) is still asked on a timer, and only while the page is visible.

// set once the texts are in (Init below)
let NO_CHANNEL, NO_EPG, NO_TUNER, RECORDING;

const dash = {
	channel: null,		// {id, name, kind, number} of the current channel, null for none
	shown: null,		// the channel key the band shows, so a reread of the same one changes nothing
	now: null,		// the guide event running on the current channel
	next: null,		// and the one after it
	recordings: [],		// what the box records, its timeshift left out
	timers: [],
	timersRead: false,	// whether the timers were read once, so a failure later keeps them
	playback: null,
	playbackAt: 0,		// when the playback state was read, to count its position on
	channelRead: 0,			// counts channel reads, so an older answer arriving last is dropped
	systemShown: false,		// the gauges showed values once
	tunerShown: false,		// and the reception tile
	recentShown: false		// and the latest recordings
};

// A read that fails is no answer: the tile keeps what it shows or says
// there is nothing. A refusal for want of a sign-in opens the dialog once
// (webui-v1.js) and the read is sent again.
function dashGet(path, query) {
	return webuiV1('GET', path, query ? {query: query} : undefined).catch(function() { return null; });
}

// The same, with the refusal kept, for a tile that says why it is empty
function dashRead(path, query) {
	return webuiV1('GET', path, query ? {query: query} : undefined).then(
		function(data) { return {data: data}; },
		function(e) { return {error: e}; });
}

const clock = webuiClock;

// "today 20:15", "tomorrow 06:00", else the date and the time
function dayAndClock(sec) {
	const d = new Date(sec * 1000);
	const today = new Date(webuiBoxNow());
	const tomorrow = new Date(today.getFullYear(), today.getMonth(), today.getDate() + 1);
	const same = function(a, b) { return a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth() && a.getDate() === b.getDate(); };
	const day = same(d, today) ? webuiL('dash.today', 'today') : same(d, tomorrow) ? webuiL('dash.tomorrow', 'tomorrow')
		: webuiPad2(d.getDate()) + '.' + webuiPad2(d.getMonth() + 1) + '.';
	return day + ' ' + webuiFormatTime(d);
}

function duration(sec) {
	sec = Math.max(0, Math.floor(sec));
	const h = Math.floor(sec / 3600), m = Math.floor(sec / 60) % 60, s = sec % 60;
	return (h > 0 ? h + ':' + webuiPad2(m) : String(m)) + ':' + webuiPad2(s);
}

// what is left until stopSec, as "1 h 20 min" or "18 min"; '' when over
function remaining(stopSec) {
	const now = webuiBoxSec();
	if (!stopSec || stopSec <= now) return '';
	const min = Math.max(1, Math.round((stopSec - now) / 60)), hr = Math.floor(min / 60);
	return hr > 0 ? hr + ' h ' + (min % 60) + ' min' : min + ' min';
}

// takeMode: whether the mode is read off the channel's kind, which it is
// on the first read and on a zap (a zap to the other kind changes the
// mode). Otherwise the mode event says it: a box switched to TV whose
// zap to a TV channel did not go through still plays the radio channel.
async function refreshChannel(takeMode) {
	const read = ++dash.channelRead;
	const got = await dashRead('/channels/current');
	if (read !== dash.channelRead) return;	// a later read was started: its answer counts
	const ch = got.data;
	// a channel the box cannot name is not "no channel": the band says why
	if (got.error && got.error.status !== 404) {
		dash.channel = null;
		dash.shown = null;
		dash.now = dash.next = null;
		setText('now-channel', '--');
		setText('now-show', webuiErrText(got.error));
		setText('now-meta', '');
		withEl('now-progress', function(el) { el.hidden = true; });
		return;
	}
	if (!ch || !ch.id) {
		dash.channel = null;
		showChannel();
		dash.now = dash.next = null;
		showGuide();
		return;
	}
	dash.channel = {id: ch.id, name: ch.name || '', kind: ch.kind || '', number: ch.number || 0};
	webuiNames[ch.id] = ch.name || '';
	if (takeMode)
		setMode(ch.kind === 'radio' || ch.kind === 'webradio' ? 'radio' : 'tv');
	showChannel();
	refreshGuide();
}

// Rebuilt only when the channel is another one, so a reread does not
// reload the logo. The name comes unfiltered from the channel list and
// goes in as text; a logo that does not load leaves the TV symbol.
function showChannel() {
	const ch = dash.channel;
	const key = ch ? ch.id + '\n' + ch.name : '';
	if (key === dash.shown) return;
	dash.shown = key;
	const logoEl = document.getElementById('now-logo');
	if (!logoEl) return;
	if (!logoEl._symbol)
		logoEl._symbol = logoEl.innerHTML;
	logoEl.innerHTML = logoEl._symbol;
	if (!ch) {
		setText('now-channel', NO_CHANNEL);
		setText('now-number', '');
		return;
	}
	setText('now-channel', ch.name || '--');
	setText('now-number', ch.number ? String(ch.number) : '');
	// the symbol stays until the logo is there, and for good without one
	webuiEl('img', {attrs: {src: '/api/v1/channels/' + encodeURIComponent(ch.id) + '/logo', alt: ''},
		on: {load: function() { if (dash.shown === key) logoEl.replaceChildren(this); }}});
}

// What runs now on the current channel and what follows. Asked again on
// a zap, when the guide says it has news for this channel, and when the
// running event is over; the progress in between is counted here.
async function refreshGuide() {
	const ch = dash.channel;
	if (!ch) return;
	const now = await dashGet('/epg/current', {channel: ch.id});
	if (dash.channel !== ch) return;	// zapped meanwhile
	const from = (now && now.start) ? now.start + now.duration : webuiBoxSec();
	const win = await dashGet('/epg', {channel: ch.id, from: from, to: from + 12 * 3600});
	if (dash.channel !== ch) return;
	dash.now = (now && now.start) ? now : null;
	dash.next = null;
	const items = (win && win.items) || [];
	for (let i = 0; i < items.length; i++) {
		if (items[i].start >= from) {
			dash.next = items[i];
			break;
		}
	}
	showGuide();
}

// The band's programme: its title, from when to when, what is left and
// what follows, and how far it has run
function showGuide() {
	const ev = dash.now;
	withEl('now-show', function(el) { el.closest('.webui-now').classList.toggle('no-epg', !ev); });
	if (ev) {
		setText('now-show', ev.title || '--');
	} else {
		setText('now-show', dash.channel ? NO_EPG : '');
		withEl('now-progress', function(el) { el.hidden = true; });
	}
	tickGuide();
}

function tickGuide() {
	const ev = dash.now;
	const next = dash.next ? webuiL('dash.then', 'then: %s').replace('%s', dash.next.title || '--') : '';
	if (!ev) {
		setText('now-meta', next);
		return;
	}
	const t = webuiBoxNow() / 1000;
	if (t >= ev.start + ev.duration) {
		// over: the next one runs now if it has begun, and the guide is asked again
		dash.now = (dash.next && dash.next.start <= t) ? dash.next : null;
		dash.next = null;
		showGuide();
		refreshGuide();
		return;
	}
	const left = remaining(ev.start + ev.duration);
	setText('now-meta', [clock(ev.start) + ' – ' + clock(ev.start + ev.duration),
		left ? webuiL('dash.left', '%s left').replace('%s', left) : '', next].filter(Boolean).join(' · '));
	const pct = ev.duration > 0 ? Math.min(100, Math.max(0, Math.round(100 * (t - ev.start) / ev.duration))) : 0;
	withEl('now-progress', function(el) {
		el.hidden = false;
		el.setAttribute('aria-valuenow', String(pct));
		el.firstElementChild.style.width = pct + '%';
	});
}

// The guide event says which channel in the lower 48 bits of its id.
function sameLow48(a, b) {
	return webuiLegacyChannelId(a).slice(-12).toLowerCase() === webuiLegacyChannelId(b).slice(-12).toLowerCase();
}

let guideSoon = null;
function refreshGuideSoon() {
	if (guideSoon) return;
	guideSoon = setTimeout(function() { guideSoon = null; refreshGuide(); }, 1000);
}

// A recording or a file the player shows, from GET /api/v1/playback and
// the playback event; the position is counted on while it plays.
async function refreshPlayback() {
	dash.playback = await dashGet('/playback');
	dash.playbackAt = Date.now();
	showPlayback();
}

function playing() {
	const p = dash.playback;
	return !!(p && (p.source === 'recording' || p.source === 'file'));
}

function showPlayback() {
	const p = dash.playback;
	const active = playing();
	withEl('play-info', function(el) { el.hidden = !active; });
	showState();
	if (!active) return;
	const rec = p.recording || {}, file = p.file || {};
	setText('play-title', (p.source === 'recording' ? (rec.title || rec.channel) : (file.title || file.name)) || '--');
	setText('play-source', p.source === 'recording'
		? webuiL('dash.play_recording', 'Playing a recording') + (rec.channel ? ' · ' + rec.channel : '')
		: webuiL('dash.play_file', 'Playing a file'));
	let pos = p.position || 0;
	if (p.state === 'playing' && !p.paused)
		pos += Math.floor((Date.now() - dash.playbackAt) / 1000) * (p.speed || 1);
	if (p.duration && pos > p.duration) pos = p.duration;
	setText('play-time', duration(pos) + (p.duration ? ' / ' + duration(p.duration) : ''));
	setText('play-state', (p.paused || p.state === 'paused') ? webuiL('dash.paused', 'Paused') : webuiL('dash.playback', 'Playback'));
}

// The pill beside the channel: a recording, else a playback, else nothing
function showState() {
	withEl('now-state', function(el) {
		const rec = dash.recordings.length > 0;
		const play = playing();
		el.hidden = !rec && !play;
		el.className = 'webui-pill ' + (rec ? 'rec' : 'ok');
		el.textContent = rec ? RECORDING : play ? webuiL('dash.playback', 'Playback') : '';
	});
}

/* Gauges that never showed values say they could not be read, in place of
   the gauges (they carry the link to the system information), and offer to
   read again. */
function systemFailed(failed) {
	withEl('card-gauges', function(sec) {
		var st = sec.querySelector(':scope > .webui-state');
		if (failed && !st)
			sec.prepend(webuiState('error', webuiL('state.failed', 'Could not be read'), '', {retry: loadSystemInfo}));
		else if (!failed && st)
			st.remove();
		withEl('gauges', function(g) { g.hidden = failed; });
	});
}

// Load System Info: /api/v1/system/info in the old shape (webui-header.js);
// gauges that never showed values say why they hold none
function loadSystemInfo() {
	webuiSystemInfo({interfaces: true}).then(function(data) {
		systemFailed(!data && !dash.systemShown);
		if (!data) return;
		dash.systemShown = true;

		// CPU - show real CPU usage percentage, else the load
		if (data.cpu && data.cpu.percent !== undefined) {
			setGauge('gauge-cpu', data.cpu.percent, data.cpu.percent + ' %',
				data.cpu.cores ? data.cpu.cores + ' ' + webuiL('dash.cores', 'cores') : '');
		} else if (data.load && data.load.length >= 1) {
			const load1 = data.load[0];
			setGauge('gauge-cpu', Math.min(100, Math.round(load1 * 12.5)), load1.toFixed(2),
				webuiL('dash.load_1m', 'Load (1 min)'), getCpuLevel(load1));
		}

		// Memory - used of the total
		if (data.memory)
			setGauge('gauge-ram', data.memory.percent || 0, (data.memory.percent || 0) + ' %',
				formatKB(data.memory.used) + ' / ' + formatKB(data.memory.total));

		// Disk - what is free
		if (data.disk)
			setGauge('gauge-disk', data.disk.percent || 0, (data.disk.percent || 0) + ' %',
				formatKB(data.disk.free) + ' ' + webuiL('dash.free', 'free'));

		// The temperature, warm from 60 and hot from 70 °C on, and how long
		// the box has run below it; where it tells no temperature, that time
		// in its place
		withEl('gauge-temp', function(el) {
			const t = data.temperature;
			const known = t !== null && t !== undefined;
			const up = formatUptime(data.uptime || 0);
			el.classList.toggle('webui-gauge-plain', !known);
			el.querySelector('h3').textContent = known ? webuiL('dash.temp', 'Temperature') : webuiL('dash.uptime', 'Up for');
			if (known)
				setGauge('gauge-temp', t, t + ' °C', webuiL('dash.up_for', 'up for %s').replace('%s', up), t >= 70 ? 'danger' : t >= 60 ? 'warn' : 'ok');
			else
				setGauge('gauge-temp', 0, up, '', '');
		});

		// Hostname in network tile's head
		if (data.hostname)
			setText('net-hostname', data.hostname);

		// Network interfaces - only the relevant ones; none at all is said
		// as such rather than left empty
		const netList = document.getElementById('net-list');
		if (!netList || !data.interfaces) return;
		const relevant = data.interfaces.filter(function(iface) {
			// Skip veth (docker), br- without IP
			if (iface.name.startsWith('veth')) return false;
			if (iface.name.startsWith('br-') && !iface.ip) return false;
			return true;
		}).slice(0, 6); // Max 6
		if (!relevant.length) {
			netList.replaceChildren(webuiEl('li', {}, webuiState('empty', webuiL('si.no_ifaces', 'No interfaces'), '')));
			return;
		}
		// interface names and addresses go in as text
		webuiSync(netList, relevant, function(iface) { return iface.name; }, function(iface) {
			const macStr = iface.mac ? String(iface.mac).toUpperCase() : '';
			return webuiEl('li', {className: 'webui-dash-row webui-net-row'},
				webuiEl('b', {}, iface.name),
				webuiEl('span', {}, iface.ip || webuiL('dash.no_ip', 'no IP'), macStr ? webuiEl('small', {}, macStr) : null),
				webuiEl('span', {className: 'webui-pill' + (iface.up ? ' ok' : '')}, iface.up ? webuiL('dash.up', 'up') : webuiL('dash.down', 'down')));
		});
	});
}

// The latest recordings, from GET /api/v1/recordings/archive: the three
// newest, each with Play on TV; read again when a recording ends. A list
// that never showed says why it is empty.
async function refreshRecent() {
	const list = document.getElementById('recent-list');
	if (!list) return;
	const got = await dashRead('/recordings/archive', {limit: 3, sort: 'start', order: 'desc'});
	if (got.error) {
		if (!dash.recentShown)
			list.replaceChildren(webuiEl('li', {}, webuiStateFor(got.error, {retry: refreshRecent})));
		return;
	}
	dash.recentShown = true;
	const page = got.data || {};
	const items = page.items || [];
	setText('recent-count', page.total ? String(page.total) : '');
	if (!items.length) {
		list.replaceChildren(webuiEl('li', {}, webuiState('empty', webuiL('dash.recent_none', 'No recordings yet'), '')));
		return;
	}
	// titles and channels come from the recordings and go in as text
	webuiSync(list, items, function(r) { return r.id; }, function(r) {
		const title = r.title || r.channel || r.id;
		const len = r.duration ? Math.round(r.duration / 60) + ' min' : '';
		return webuiEl('li', {className: 'webui-dash-row webui-recent-row'},
			webuiEl('b', {}, r.start ? dayAndClock(r.start) : ''),
			webuiEl('span', {}, title, webuiEl('small', {}, [r.channel, len].filter(Boolean).join(' · '))),
			webuiEl('button', {type: 'button', className: 'webui-btn', title: webuiL('media.play', 'Play on TV'),
				attrs: {'aria-label': webuiL('media.play', 'Play on TV') + ': ' + title},
				on: {click: function() { webuiPlayRecording(r.id); }}},
				webuiMarkup('<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><polygon points="6 4 20 12 6 20"/></svg>')));
	});
}

// What the box records, from GET /api/v1/recordings; the time left comes
// from the timer that carries the recording.
async function refreshRecordings() {
	const r = await dashGet('/recordings');
	dash.recordings = ((r && r.items) || []).filter(function(it) { return !it.timeshift; });
	await webuiEnsureNames(dash.recordings.map(function(it) { return it.channel_id; }));
	showRecording();
}

function showRecording() {
	const rec = dash.recordings[0] || null;
	const isRecording = !!rec;
	withEl('rec-info', function(el) { el.hidden = !isRecording; });
	showState();

	const recBtn = document.getElementById('rec-btn');
	if (recBtn) {
		recBtn.classList.toggle('recording', isRecording);
		recBtn.classList.toggle('primary', !isRecording);
		recBtn.classList.toggle('danger', isRecording);
		recBtn.querySelector('span').textContent = isRecording ? webuiL('dash.record_stop', 'Stop recording') : webuiL('dash.record', 'Record');
	}
	if (!rec) return;

	const channel = webuiNames[rec.channel_id] || '';
	const timer = dash.timers.find(function(t) { return t.id === rec.id; });
	const left = timer && remaining(timer.stop);
	setText('rec-title', rec.title || channel || RECORDING);
	setText('rec-channel', rec.title && channel ? channel : '');
	setText('rec-remaining', left ? webuiL('dash.left', '%s left').replace('%s', left) : '');
}

// Recording and stopping go through the helpers the EPG dialog and the
// channel list share (webui-v1.js): what runs now is recorded, the
// recording shown is ended. What they change comes back as record and
// timer events.
function toggleRecording() {
	const btn = document.getElementById('rec-btn');
	if (!btn) return;
	if (btn.classList.contains('recording'))
		webuiStopRecording(dash.recordings[0] ? dash.recordings[0].id : null);
	else if (dash.channel)
		webuiRecordEvent(dash.channel.id, dash.channel.name, dash.now);
	// without the event stream nothing would say so
	setTimeout(function() { if (!webuiV1EventsLive()) refreshEvented(); }, 1500);
}

// Standby: wake a sleeping box, put an awake one to sleep after asking
async function toggleStandby(inStandby) {
	try {
		await webuiV1('POST', '/system/standby', {body: {on: !inStandby}});
		webuiShowToast(inStandby ? webuiL('sb.wake', 'Waking the box') : webuiL('sb.sleep', 'Box goes to standby'));
	} catch (e) {
		webuiToastError(webuiL('sb.fail', 'Standby failed'), e);
	}
}

async function sendStandby() {
	try {
		const st = await webuiV1('GET', '/system/standby');
		const inStandby = !!(st && st.on);
		if (!inStandby) {
			webuiShowConfirm(webuiL('dash.standby', 'Standby'), webuiLH('sb.confirm', 'Put the box into standby?'), webuiL('dash.standby', 'Standby'),
				function() { return toggleStandby(false); }, {variant: 'ok'});
			return;
		}
		await toggleStandby(true);
	} catch (e) {
		webuiToastError(webuiL('sb.fail', 'Standby failed'), e);
	}
}

// TV/Radio Mode: read off the current channel and the mode event
var currentMode = 'tv';

function setMode(mode) {
	if (mode === currentMode) return;
	currentMode = mode;
	updateModeUI();
}

async function switchMode(target) {
	const newMode = target || (currentMode === 'tv' ? 'radio' : 'tv');
	if (newMode === currentMode) return;
	const btns = ['mode-tv', 'mode-radio'].map(function(id) { return document.getElementById(id); }).filter(Boolean);
	btns.forEach(function(b) { b.disabled = true; });
	try {
		await webuiV1('POST', '/mode', {body: {mode: newMode}});
		setMode(newMode);
	} catch (e) {
		webuiToastError('', e);
	}
	btns.forEach(function(b) { b.disabled = false; });
}

function updateModeUI() {
	const isRadio = currentMode === 'radio';
	withEl('mode-tv', function(el) { el.setAttribute('aria-pressed', String(!isRadio)); });
	withEl('mode-radio', function(el) { el.setAttribute('aria-pressed', String(isRadio)); });

	// In radio mode: clear video-specific fields
	if (isRadio) {
		setText('stream-resolution', '--');
		setText('stream-aspect', '--');
		setText('stream-fps', '--');
	}
}

// The timer kinds v1 names, as the numbers the shared labels and icons know
const TIMER_KIND_TYPES = {
	'shutdown': 1, 'zapto': 3, 'standby': 4, 'record': 5, 'remind': 6,
	'sleeptimer': 7, 'exec-plugin': 8, 'immediate-record': 9
};

function timerTypeNumber(t) {
	return TIMER_KIND_TYPES[t.kind] || parseInt(t.kind, 10) || 0;
}

function isRunningRecording(t) {
	return t.state === 2 && timerTypeNumber(t) === 5;
}

// A read that fails keeps the timers shown; before the first answer the
// tile says why it holds none.
async function refreshTimers() {
	const got = await dashRead('/timers');
	if (got.error) {
		if (!dash.timersRead)
			withEl('timer-list', function(el) {
				el.replaceChildren(webuiStateFor(got.error, {title: webuiL('timer.unavail', 'Timers not available'), retry: refreshTimers}));
			});
		return;
	}
	dash.timersRead = true;
	dash.timers = (got.data && got.data.items) || [];
	await webuiEnsureNames(dash.timers.map(function(t) { return t.channel_id; }));
	showTimers();
	showRecording();
}

// Running recordings first, then the next scheduled timers. Titles and
// channel names come from the guide and go in as text.
function showTimers() {
	const container = document.getElementById('timer-list');
	if (!container || !dash.timersRead) return;
	const now = webuiBoxSec();
	const running = dash.timers.filter(isRunningRecording);
	const coming = dash.timers.filter(function(t) { return t.state !== 2 && t.start > now; });
	const future = coming.slice().sort(function(a, b) { return a.start - b.start; })
		.slice(0, Math.max(3 - running.length, 1));
	setText('timer-count', String(running.length + coming.length));

	if (running.length === 0 && future.length === 0) {
		container.replaceChildren(webuiState('empty', webuiL('dash.no_timers', 'No timers programmed'), webuiL('dash.no_timers_hint', 'Timers are set in the EPG or on the timer list.')));
		return;
	}

	let list = container.querySelector('.webui-dash-rows');
	if (!list) {
		list = webuiEl('ul', {className: 'webui-dash-rows'});
		container.replaceChildren(list);
	}
	const rows = running.map(function(t) { return {timer: t, running: true}; })
		.concat(future.map(function(t) { return {timer: t, running: false}; }));
	webuiSync(list, rows, function(row) {
		return (row.running ? 'run-' : 'next-') + row.timer.id;
	}, function(row) {
		const timer = row.timer;
		const channel = webuiNames[timer.channel_id] || '';
		const label = webuiGetTimerType({type_number: timerTypeNumber(timer)});
		const what = [channel, label].filter(Boolean).join(' · ');
		if (row.running) {
			const left = remaining(timer.stop);
			return webuiEl('li', {className: 'webui-dash-row'},
				webuiEl('b', {}, left ? webuiL('dash.left', '%s left').replace('%s', left) : RECORDING),
				webuiEl('span', {}, timer.title || channel || RECORDING, what ? webuiEl('small', {}, what) : null),
				webuiEl('span', {className: 'webui-pill rec'}, RECORDING));
		}
		return webuiEl('li', {className: 'webui-dash-row'},
			webuiEl('b', {}, dayAndClock(timer.start)),
			webuiEl('span', {}, timer.title || channel || label, what ? webuiEl('small', {}, what) : null),
			webuiEl('span', {}));
	});
}

// Everything the event stream keeps current, read again: on reconnect
// (events may have been missed), on pull-to-refresh, and on a timer while
// the stream is not open.
function refreshEvented() {
	refreshChannel(true);
	refreshPlayback();
	refreshRecordings();
	refreshTimers();
}

function followTheBox() {
	let opened = 0;
	webuiV1Events({
		// the stream's own open event: the first one is the page loading,
		// every later one a reconnect after a gap
		open: function() { if (opened++) refreshEvented(); },
		zap: function() { refreshChannel(true); refreshPlayback(); loadStreamInfo(); },
		mode: function(d) {
			if (d && d.value === 1) setMode('tv');
			else if (d && d.value === 2) setMode('radio');
			refreshChannel(false);
			loadStreamInfo();
		},
		standby: function() { refreshChannel(false); refreshPlayback(); },
		'record-start': function() { refreshTimers().then(refreshRecordings); },
		'record-stop': function() { refreshTimers().then(refreshRecordings); refreshRecent(); },
		'timer-changed': refreshTimers,
		'epg-updated': function(d) {
			if (d && dash.channel && sameLow48(d.channel_id, dash.channel.id))
				refreshGuideSoon();
		},
		playback: refreshPlayback
	});
}

// A timer that runs only while the page is visible; all of them run once
// more when it becomes visible again.
const dashPolls = [];
function whileVisible(ms, fn) {
	dashPolls.push(fn);
	setInterval(function() {
		if (document.visibilityState === 'visible') fn();
	}, ms);
}

// how good a reading is, as the class of its meter: under 40 % a warning,
// under 20 % bad
function signalLevel(percent) {
	if (percent < 20) return 'danger';
	if (percent < 40) return 'warn';
	return '';
}

// The tuners with what each receiving one measures (GET /tuner/frontends):
// the live one shows the channel the box plays, another one in use is held
// by something else, most often a recording. The readings are raw, out of
// full_scale. A read that fails keeps what the tile shows; before the
// first answer it says why it holds none.
async function loadTunerStatus() {
	const got = await dashRead('/tuner/frontends');
	if (got.error || !got.data || !Array.isArray(got.data.items)) {
		if (!dash.tunerShown && got.error)
			withEl('tuner-list', function(el) {
				el.replaceChildren(webuiStateFor(got.error, {retry: loadTunerStatus}));
			});
		return;
	}
	dash.tunerShown = true;
	renderTunerList(got.data.items.map(function(fe, i) {
		const sig = fe.signal || {};
		const scale = Number(sig.full_scale) || 0;
		function percent(v) { return fe.measured && scale > 0 ? Math.round(Number(v || 0) * 100 / scale) : 0; }
		return {
			id: fe.adapter + ':' + fe.number,
			name: fe.name || webuiL('dash.tuner', 'Tuner') + ' ' + (i + 1),
			// the kind it receives; which delivery systems it takes the box does not say
			type: fe.satellite ? 'DVB-S' : fe.cable ? 'DVB-C' : fe.terrestrial ? 'DVB-T' : 'DVB',
			sig: percent(sig.strength),
			snr: percent(sig.snr),
			channel: fe.live && dash.channel ? dash.channel.name : '',
			state: fe.live ? 'viewing' : fe.in_use ? 'busy' : 'idle'
		};
	}));
}

// The tuners, each with its state and its two readings; the head's pill
// sums up the one the picture comes from
function renderTunerList(tuners) {
	const container = document.getElementById('tuner-list');
	if (!container) return;
	const quality = document.getElementById('tuner-quality');
	// a box without a tuner says so rather than showing an empty tile
	if (!tuners.length) {
		container.replaceChildren(webuiState('empty', NO_TUNER, ''));
		if (quality) quality.hidden = true;
		return;
	}
	const STATES = {
		viewing: ['ok', webuiL('dash.tuner_live', 'Live')],
		busy: ['warn', webuiL('dash.tuner_busy', 'In use')],
		idle: ['', webuiL('dash.tuner_idle', 'Free')]
	};
	// the channel name comes from the broadcast and goes in as text; the
	// state only as a class it is known to be
	webuiSync(container, tuners, function(t, i) {
		return t.id !== undefined ? t.id : '#' + i;
	}, function(t) {
		const st = STATES[t.state] || ['', t.state];
		const sig = Math.max(0, Math.min(100, Number(t.sig) || 0));
		const snr = Math.max(0, Math.min(100, Number(t.snr) || 0));
		const meter = function(label, value) {
			const m = webuiEl('div', {className: 'webui-meter' + (t.state !== 'idle' && signalLevel(value) ? ' ' + signalLevel(value) : '')},
				webuiEl('b', {}, label),
				webuiEl('div', {className: 'webui-progress'}, webuiEl('span', {style: {width: value + '%'}})),
				webuiEl('span', {}, value + ' %'));
			return m;
		};
		return webuiEl('div', {className: 'webui-tuner'},
			webuiEl('div', {className: 'webui-tuner-head'},
				webuiEl('b', {title: t.name}, t.name),
				webuiEl('small', {}, t.type || 'DVB'),
				webuiEl('span', {className: 'webui-pill' + (st[0] ? ' ' + st[0] : '')}, st[1])),
			t.channel ? webuiEl('div', {className: 'webui-tuner-channel'}, t.channel) : null,
			t.state !== 'idle' ? meter(webuiL('dash.signal', 'Signal'), sig) : null,
			t.state !== 'idle' ? meter(webuiL('dash.snr', 'SNR'), snr) : null);
	});
	// the live tuner's readings in the head: good, weak or bad
	if (quality) {
		const live = tuners.find(function(t) { return t.state === 'viewing'; });
		if (!live) {
			quality.hidden = true;
		} else {
			const q = (Number(live.sig) + Number(live.snr)) / 2;
			quality.hidden = false;
			quality.className = 'webui-pill ' + (q >= 60 ? 'ok' : q >= 40 ? 'warn' : 'danger');
			quality.textContent = q >= 60 ? webuiL('dash.q_good', 'good') : q >= 40 ? webuiL('dash.q_weak', 'weak') : webuiL('dash.q_bad', 'bad');
		}
	}
}

// The names the sound codecs go by on the tile
const SOUND_CODECS = {mp2: 'MPEG', ac3: 'AC3', eac3: 'E-AC3', aac: 'AAC', aacplus: 'HE-AAC',
	dts: 'DTS', dtshd: 'DTS-HD', lpcm: 'LPCM'};

// The picture as the decoder shows it (GET /playback/picture); the sound
// as the selected track of the running channel and the channel's packet
// ids (GET /channels/current). A picture read that fails keeps what the
// tile shows; sound and packet ids go with a channel that cannot be read,
// which is also what a playing recording or file answers.
async function loadStreamInfo() {
	const both = await Promise.all([dashGet('/playback/picture'), dashGet('/channels/current')]);
	const pic = both[0], ch = both[1];
	if (pic) {
		const shown = !!pic.shown && pic.width > 0 && pic.height > 0;
		updateStreamInfoUI(shown ? pic.width + ' × ' + pic.height : '', shown ? pic.height : 0,
			shown ? pic.aspect : '', shown && pic.frame_rate ? pic.frame_rate + ' fps' : '', soundOf(ch));
	} else {
		// the picture as it was, the sound from the channel read beside it
		setText('stream-audio', soundOf(ch) || '--');
	}
	updatePidsUI(ch);
}

// The selected track's codec and what the stream calls it, '' without one
function soundOf(ch) {
	const tracks = (ch && Array.isArray(ch.audio)) ? ch.audio : [];
	const t = tracks.filter(function(a) { return a.selected; })[0];
	if (!t) return '';
	const codec = SOUND_CODECS[t.codec] || '';
	const desc = t.description || '';
	// a description that names the codec already ("AC3 5.1 deu") stands alone
	if (codec && desc.toLowerCase().indexOf(codec.toLowerCase()) >= 0)
		return desc;
	return [codec, desc].filter(Boolean).join(' ');
}

// The running channel's packet ids, from its answer on /channels/current;
// none without a channel
function updatePidsUI(ch) {
	const pidsContainer = document.getElementById('stream-pids');
	if (!pidsContainer) return;
	const data = ch || {};

	const tags = [];
	function tag(label, value) {
		tags.push(webuiEl('span', {className: 'webui-pid-tag'},
			webuiEl('span', {className: 'label'}, label),
			webuiEl('span', {className: 'value'}, value)));
	}

	// Video PID
	if (data.video && data.video.pid) {
		tag('VID', data.video.pid);
	}

	// Audio PIDs
	(Array.isArray(data.audio) ? data.audio : []).forEach(function(track) {
		let label = 'AUD';
		if (track.codec === 'ac3' || track.codec === 'eac3') label = 'AC3';
		else if (track.codec === 'aac' || track.codec === 'aacplus') label = 'AAC';
		else if (track.description) {
			const desc = track.description.toLowerCase();
			if (desc.includes('eng') || desc.includes('english')) label = 'ENG';
		}
		tag(label, track.pid);
	});

	// Special PIDs
	const pids = data.pids || {};
	if (pids.teletext) {
		tag('TXT', pids.teletext);
	}
	if (pids.pmt) {
		tag('PMT', pids.pmt);
	}
	if (pids.pcr) {
		tag('PCR', pids.pcr);
	}

	pidsContainer.replaceChildren.apply(pidsContainer, tags);
}

// The picture and sound rows of the reception tile; in radio mode only
// the sound
function updateStreamInfoUI(resolution, height, aspect, fps, audio) {
	if (currentMode === 'radio') {
		setText('stream-resolution', '--');
		setText('stream-aspect', '--');
		setText('stream-fps', '--');
	} else {
		setText('stream-resolution', resolution ? resolution + (height ? ' · ' + (height >= 720 ? 'HD' : 'SD') : '') : '--');
		setText('stream-aspect', aspect || '--');
		setText('stream-fps', fps || '--');
	}
	setText('stream-audio', audio || '--');
}

// Init, once the texts are in
webuiAfterTexts(function() {
	// the tiles share their rows out evenly, the extensions' as they come,
	// down to a phone's width, where they stand one to a row
	webuiBalanceGrid('.webui-dashboard', {tracks: 60, cardMin: 380, maxPer: 4, breakpoint: 760,
		skip: '.webui-dash-now, .webui-dash-gauges', watch: true});
	NO_CHANNEL = webuiL('dash.no_channel', 'No channel');
	NO_EPG = webuiL('dash.no_epg', 'No EPG data');
	NO_TUNER = webuiL('dash.no_tuner', 'No tuner');
	RECORDING = webuiL('bc.recording', 'Recording');
	// until the first answers: the tiles that list say they load
	withEl('timer-list', function(el) { el.replaceChildren(webuiState('loading', webuiL('msg.loading', 'Loading...'), '')); });
	withEl('tuner-list', function(el) { el.replaceChildren(webuiState('loading', webuiL('msg.loading', 'Loading...'), '')); });
	withEl('recent-list', function(el) { el.replaceChildren(webuiEl('li', {}, webuiState('loading', webuiL('msg.loading', 'Loading...'), ''))); });

	refreshEvented();
	followTheBox();
	loadSystemInfo();
	loadTunerStatus();
	loadStreamInfo();
	refreshRecent();

	// No event for these: asked on a timer, while visible
	whileVisible(30000, loadSystemInfo);
	whileVisible(5000, loadTunerStatus);
	whileVisible(30000, loadStreamInfo);
	// Without an open event stream the evented tiles are asked as well
	whileVisible(30000, function() { if (!webuiV1EventsLive()) refreshEvented(); });
	// Counted here between events: guide progress, time left, playback position
	setInterval(tickGuide, 15000);
	setInterval(function() { showTimers(); showRecording(); }, 30000);
	setInterval(function() { if (playing()) showPlayback(); }, 1000);
	document.addEventListener('visibilitychange', function() {
		if (document.visibilityState === 'visible')
			dashPolls.forEach(function(fn) { fn(); });
	});

	// Register pull-to-refresh callback
	if (typeof webuiSetRefreshCallback === 'function') {
		webuiSetRefreshCallback(function() {
			refreshEvented();
			loadSystemInfo();
			loadTunerStatus();
			loadStreamInfo();
			refreshRecent();
			return new Promise(resolve => setTimeout(resolve, 800));
		});
	}
});
