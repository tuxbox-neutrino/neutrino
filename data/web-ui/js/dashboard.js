// The script of dashboard.html: the dashboard, variant A of Phase 4 (what
// runs now as a band, the tiles below it).

// The head's number of set timers comes from this page's reads.
webuiTimersFollowed();

// Show EPG info for current channel
function showCurrentEpgInfo() {
	if (dash.channel)
		webuiShowEpgModal(dash.channel.id, dash.channel.name, {number: dash.channel.number, kind: dash.channel.kind});
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
// Channel, guide, recordings, timers, mode, standby, playback and the
// sound follow the event stream (/api/v1/events) instead of asking on a
// timer: each event rereads the one tile it concerns, and a tile only
// touches what changed. What v1 has no event for (system, network, tuner
// signal, stream details) is still asked on a timer, and only while the
// page is visible.

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
	timersAsked: 0,		// counts timer reads, so an older answer arriving last is dropped
	playback: null,
	playbackAt: 0,		// when the playback state was read, to count its position on
	channelRead: 0,			// counts channel reads, so an older answer arriving last is dropped
	systemShown: false,		// the gauges showed values once
	tunerShown: false,		// and the reception tile
	recentShown: false,		// and the latest recordings
	standbyRead: 0,			// counts standby reads, so an older answer arriving last is dropped
	standby: null,			// the box is in standby: it has no mode to show; null until read
	volume: {			// the sound tile, see refreshVolume
		percent: null,		// the level the box said, 0 to 100; null until it said one
		muted: null,		// whether its sound is off; null until it said
		said: {percent: 0, muted: 0},	// counts the events of each, so a read begun before one is dropped for it
		read: 0,		// counts reads, so an older answer arriving last is dropped
		held: false,		// the slider is being moved: what the box says waits until it is let go
		chosen: null,		// the level chosen on the slider until the box says it has one
		next: null,		// a level chosen while another was on its way, sent after it
		sending: false,		// a level is on its way to the box
		sent: false,		// the box took the chosen level; its next volume event settles it
		settle: null,		// the read after a level the box did not announce
		chose: 0,		// counts levels chosen, so the read after an older one leaves a newer one alone
		muteBusy: false,	// a mute is on its way or not yet announced
		muteSettle: null,	// the read after a mute the box did not announce
		muteAsked: 0,		// counts mutes asked for, the same for the read after one
		completing: false,	// a read for the half an event left unknown is under way
		shown: false		// the tile showed values once
	}
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

/* Gauges and the list of interfaces that never showed values say they
   could not be read, in place of what they hold and beside it, as that
   carries the link to the system information, and offer to read again. A
   failure that comes again keeps the block, and the focus on its button. */
function systemFailed(failed) {
	[['#card-gauges', 'gauges'], ['#card-network > .webui-card-body', 'net-interfaces']].forEach(function(part) {
		var host = document.querySelector(part[0]);
		if (!host)
			return;
		var st = host.querySelector(':scope > .webui-state');
		if (failed && !st)
			host.prepend(webuiState('error', webuiL('state.failed', 'Could not be read'), '', {retry: loadSystemInfo}));
		else if (!failed && st)
			st.remove();
		withEl(part[1], function(el) { el.hidden = failed; });
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
		// the box has run below it; where it tells no temperature, the box's
		// name, how long it has run and its kernel as rows in its place
		withEl('gauge-temp', function(el) {
			const t = data.temperature;
			const known = t !== null && t !== undefined;
			const up = formatUptime(data.uptime || 0);
			el.classList.toggle('webui-gauge-plain', !known);
			el.querySelector('.webui-gauge-facts').hidden = known;
			el.querySelector('h3').textContent = known ? webuiL('dash.temp', 'Temperature') : webuiL('dash.system', 'System');
			if (known) {
				setGauge('gauge-temp', t, t + ' °C', webuiL('dash.up_for', 'up for %s').replace('%s', up), t >= 70 ? 'danger' : t >= 60 ? 'warn' : 'ok');
			} else {
				setText('gauge-host', data.hostname || '--');
				setText('gauge-uptime', up);
				setText('gauge-kernel', data.kernel || '--');
			}
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
	showTimers();
}

function showRecording() {
	const rec = dash.recordings[0] || null;
	const isRecording = !!rec;
	withEl('rec-info', function(el) { el.hidden = !isRecording; });
	showState();
	webuiEpgRecordingsSeen(dash.recordings);

	const recBtn = document.getElementById('rec-btn');
	if (recBtn) {
		recBtn.classList.toggle('recording', isRecording);
		recBtn.classList.add('webui-record-btn');
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

// Standby: wake a sleeping box, put an awake one to sleep after asking,
// the same way as the power key in the head
function sendStandby() {
	return webuiPowerKey();
}

// TV/Radio Mode: read off the current channel and the mode event. A box in
// standby has none (it names no current channel), so the switch shows
// neither as pressed and either one asks to switch the box on.
var currentMode = 'tv';

async function refreshStandby() {
	const read = ++dash.standbyRead;
	const got = await dashRead('/system/standby');
	if (read !== dash.standbyRead || got.error || !got.data)
		return;	// a later read, or the wake below, has the state
	dash.standby = !!got.data.on;
	updateModeUI();
}

function setMode(mode) {
	if (mode === currentMode) return;
	currentMode = mode;
	updateModeUI();
}

// The switch stays focusable while a change is under way (aria-disabled,
// not disabled), so the keyboard focus is still on it when the change or
// its question is done.
var modeBusy = false;

async function switchMode(target) {
	const newMode = target || (currentMode === 'tv' ? 'radio' : 'tv');
	// only a box known to be awake has nothing to do for the mode it shows;
	// unread, the click goes out and finds sign-in or standby on its way
	if (modeBusy || (newMode === currentMode && dash.standby === false)) return;
	const btns = ['mode-tv', 'mode-radio'].map(function(id) { return document.getElementById(id); }).filter(Boolean);
	modeBusy = true;
	btns.forEach(function(b) { b.setAttribute('aria-disabled', 'true'); });
	try {
		// a box in standby is asked about, as the other ways to put something
		// on the television are
		const done = await webuiV1Lifting('/mode', {mode: newMode}, {
			title: webuiL('dash.mode_title', 'Switch mode'),
			ok: webuiL('dash.mode_ok', 'Switch'),
			wake: newMode === 'radio'
				? webuiL('dash.mode_wake_radio', 'The box is in standby. Switch it on and change to radio?')
				: webuiL('dash.mode_wake_tv', 'The box is in standby. Switch it on and change to television?')
		});
		if (done) {
			// the box was switched on by it, if it was off
			dash.standby = false;
			dash.standbyRead++;	// a read begun before this is out of date
			currentMode = newMode;
			updateModeUI();
		}
	} catch (e) {
		webuiToastError('', e);
	}
	modeBusy = false;
	btns.forEach(function(b) { b.removeAttribute('aria-disabled'); });
}

function updateModeUI() {
	const isRadio = currentMode === 'radio';
	withEl('mode-tv', function(el) { el.setAttribute('aria-pressed', String(!dash.standby && !isRadio)); });
	withEl('mode-radio', function(el) { el.setAttribute('aria-pressed', String(!dash.standby && isRadio)); });

	// In radio mode: clear video-specific fields
	if (isRadio) {
		setText('stream-resolution', '--');
		setText('stream-aspect', '--');
		setText('stream-fps', '--');
	}
}

// ---- Volume and mute -------------------------------------------------
// How loud the box is and whether its sound is off, from GET /osd/volume
// and the volume and mute events; the slider and Mute change them over
// PUT /osd/volume and PUT /osd/mute. The box answers a change with 202,
// which says it took the wish, not that it acted on it: the tile shows
// what the box says, from its event or, where none comes, from a read a
// moment later. The box announces every level it is given, also the one
// it has, but a mute only when the sound changes. A level above 0 also
// turns a mute off; the box's mute event says so.

// how long a change waits for its event before the level is read
const VOLUME_SETTLE_MS = 2000;

function volumeLevel(v) {
	return Math.max(0, Math.min(100, Math.round(Number(v) || 0)));
}

// Read on load, after a reconnect, when the page is visible again and a
// moment after a change the box did not announce. A read of its own asks
// for no sign-in (a refusal leaves the tile as it is); the tile's Reload
// (signIn) asks for one where the box wants it.
async function refreshVolume(signIn) {
	const v = dash.volume;
	const read = ++v.read;
	const said = {percent: v.said.percent, muted: v.said.muted};
	let got = null;
	try {
		got = signIn === true ? await webuiV1('GET', '/osd/volume') : await _webuiV1Read('/osd/volume');
	} catch (e) {
		got = null;
	}
	if (read !== v.read) return;	// a later read was started: its answer counts
	if (!got || typeof got.percent !== 'number') {
		if (!v.shown) volumeState('error');
		return;
	}
	// what an event said while the read was under way is newer than the answer
	if (said.percent === v.said.percent)
		v.percent = volumeLevel(got.percent);
	if (said.muted === v.said.muted)
		v.muted = !!got.muted;
	v.shown = true;
	showVolume();
}

// The volume and mute events: what the box has now, whoever changed it
function volumeEvent(field, d) {
	const v = dash.volume;
	if (!d || typeof d.value !== 'number') return;
	v.said[field]++;
	if (field === 'percent') {
		v.percent = volumeLevel(d.value);
		// the level chosen is in, or the box has acted on it in its own way
		if (v.chosen !== null && v.next === null && (v.percent === v.chosen || v.sent))
			volumeSettled();
	} else {
		v.muted = d.value !== 0;
		volumeMuteDone();
	}
	if (v.percent !== null && v.muted !== null)
		v.shown = true;
	// an event before the first answer tells one half: both are read once,
	// the tile keeps its Reload until they are in
	else if (!v.completing) {
		v.completing = true;
		refreshVolume().then(function() { v.completing = false; });
	}
	showVolume();
}

function volumeSettled() {
	const v = dash.volume;
	v.chosen = null;
	v.sent = false;
	clearTimeout(v.settle);
	v.settle = null;
}

// A tile that never showed values says it is loading or could not read
// them, in place of the slider; null takes the block away
function volumeState(kind) {
	withEl('volume-body', function(body) {
		const st = body.querySelector(':scope > .webui-state');
		if (st && kind && st.classList.contains(kind)) return;
		const focused = !!st && st.contains(document.activeElement);
		if (st) st.remove();
		if (kind === 'loading')
			body.prepend(webuiState('loading', webuiL('msg.loading', 'Loading...'), ''));
		else if (kind === 'error')
			body.prepend(webuiState('error', webuiL('dash.volume_failed', 'Volume could not be read'), '',
				{retry: function() { refreshVolume(true); }}));
		// Reload had the focus: it goes to the slider that took its place
		else if (focused)
			withEl('volume-range', function(el) { el.focus(); });
	});
}

// The slider shows the level chosen until the box has it, else the box's;
// while it is being moved it stays where the hand has it. Shown once both
// the level and the mute are known.
function showVolume() {
	const v = dash.volume;
	const box = document.getElementById('volume-controls');
	const range = document.getElementById('volume-range');
	if (!box || !range || v.percent === null || v.muted === null) return;
	box.hidden = false;
	volumeState(null);
	if (!v.held)
		range.value = String(v.chosen !== null ? v.chosen : v.percent);
	showVolumeLevel(Number(range.value));
	box.classList.toggle('muted', !!v.muted);
	withEl('volume-mute', function(b) {
		b.setAttribute('aria-pressed', String(!!v.muted));
		if (v.muteBusy)
			b.setAttribute('aria-disabled', 'true');
		else
			b.removeAttribute('aria-disabled');
	});
	setText('volume-state', v.chosen !== null ? webuiL('dash.volume_pending', 'Applying volume')
		: v.muted ? webuiL('dash.volume_muted', 'Sound is muted') : '');
}

// The level on the slider's track and in the tile's head, also while the
// slider moves
function showVolumeLevel(level) {
	withEl('volume-range', function(el) {
		el.style.setProperty('--webui-volume-level', level + '%');
		el.setAttribute('aria-valuetext', level + ' %');
	});
	setText('volume-percent', level + ' %');
}

// A level chosen on the slider, sent once it is let go (change), never
// while it moves. One level is on its way at a time; of those chosen
// meanwhile only the newest follows it.
function setDashboardVolume(level) {
	const v = dash.volume;
	v.chose++;
	v.chosen = volumeLevel(level);
	v.sent = false;
	clearTimeout(v.settle);
	v.settle = null;
	showVolume();
	if (v.sending) {
		v.next = v.chosen;
		return;
	}
	sendVolume(v.chosen);
}

async function sendVolume(level) {
	const v = dash.volume;
	v.sending = true;
	for (;;) {
		try {
			await webuiV1('PUT', '/osd/volume', {body: {percent: level}});
		} catch (e) {
			// refused, or the sign-in cancelled: the tile shows what the box said
			v.sending = false;
			v.next = null;
			volumeSettled();
			showVolume();
			webuiToastError(webuiL('dash.volume_title', 'Volume'), e);
			return;
		}
		if (v.next === null) break;
		level = v.next;
		v.next = null;
	}
	v.sending = false;
	if (v.chosen === null) return;	// its event came before the answer
	v.sent = true;
	const chose = v.chose;
	v.settle = setTimeout(function() {
		v.settle = null;
		refreshVolume().then(function() {
			// a level chosen meanwhile is settled by its own event or read
			if (v.chose !== chose || v.sending || v.next !== null) return;
			volumeSettled();
			showVolume();
		});
	}, VOLUME_SETTLE_MS);
}

// Mute as a toggle, pressed while the sound is off. It changes when the box
// says the sound did; until then it takes no second press (aria-disabled,
// so it keeps the focus).
async function setDashboardMute() {
	const v = dash.volume;
	if (v.muteBusy || v.muted === null) return;
	v.muteBusy = true;
	showVolume();
	const said = v.said.muted;
	const asked = ++v.muteAsked;
	try {
		await webuiV1('PUT', '/osd/mute', {body: {on: !v.muted}});
	} catch (e) {
		volumeMuteDone();
		showVolume();
		webuiToastError(webuiL('dash.volume_mute', 'Mute'), e);
		return;
	}
	if (v.said.muted !== said) return;	// its event came before the answer
	// a box whose sound was so already says nothing: read once instead
	v.muteSettle = setTimeout(function() {
		v.muteSettle = null;
		refreshVolume().then(function() {
			// a mute asked for meanwhile waits for its own event or read
			if (v.muteAsked !== asked) return;
			volumeMuteDone();
			showVolume();
		});
	}, VOLUME_SETTLE_MS);
}

function volumeMuteDone() {
	const v = dash.volume;
	v.muteBusy = false;
	clearTimeout(v.muteSettle);
	v.muteSettle = null;
}

// The slider: held from the press or the first move until it is let go;
// let go without a change (moved and brought back) it shows the box again
function initVolume() {
	const v = dash.volume;
	const range = document.getElementById('volume-range');
	if (!range) return;
	volumeState('loading');
	const letGo = function() {
		if (!v.held) return;
		v.held = false;
		// after the change a release brings, if one comes
		setTimeout(showVolume, 0);
	};
	range.addEventListener('pointerdown', function() { v.held = true; });
	range.addEventListener('input', function() {
		v.held = true;
		showVolumeLevel(Number(range.value));
	});
	range.addEventListener('change', function() {
		v.held = false;
		setDashboardVolume(Number(range.value));
	});
	range.addEventListener('pointercancel', letGo);
	range.addEventListener('blur', letGo);
	window.addEventListener('pointerup', letGo);
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
	return t.state === 2 && timerTypeNumber(t) === 5 &&
		dash.recordings.some(function(rec) { return rec.id === t.id; });
}

// A read that fails keeps the timers shown; before the first answer the
// tile says why it holds none.
async function refreshTimers() {
	const read = ++dash.timersAsked;
	const got = await dashRead('/timers');
	if (read !== dash.timersAsked) return;	// a newer read has the list
	if (got.error) {
		webuiTimersSeen(null);
		if (!dash.timersRead)
			withEl('timer-list', function(el) {
				el.replaceChildren(webuiStateFor(got.error, {title: webuiL('timer.unavail', 'Timers not available'), retry: refreshTimers}));
			});
		return;
	}
	const items = (got.data && got.data.items) || [];
	await webuiEnsureNames(items.map(function(t) { return t.channel_id; }));
	if (read !== dash.timersAsked) return;
	dash.timersRead = true;
	dash.timers = items;
	webuiTimersSeen(items);
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
	refreshStandby();
	refreshChannel(true);
	refreshPlayback();
	refreshRecordings();
	refreshTimers();
	refreshVolume();
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
		standby: function() { refreshStandby(); refreshChannel(false); refreshPlayback(); webuiPowerRead(); },
		'record-start': function() { refreshTimers().then(refreshRecordings); },
		'record-stop': function() { refreshTimers().then(refreshRecordings); refreshRecent(); },
		'timer-changed': refreshTimers,
		'epg-updated': function(d) {
			if (d && dash.channel && sameLow48(d.channel_id, dash.channel.id))
				refreshGuideSoon();
		},
		playback: refreshPlayback,
		volume: function(d) { volumeEvent('percent', d); },
		mute: function(d) { volumeEvent('muted', d); }
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
	initDashboardOrder();
	NO_CHANNEL = webuiL('dash.no_channel', 'No channel');
	NO_EPG = webuiL('dash.no_epg', 'No EPG data');
	NO_TUNER = webuiL('dash.no_tuner', 'No tuner');
	RECORDING = webuiL('bc.recording', 'Recording');
	// until the first answers: the tiles that list say they load
	withEl('timer-list', function(el) { el.replaceChildren(webuiState('loading', webuiL('msg.loading', 'Loading...'), '')); });
	withEl('tuner-list', function(el) { el.replaceChildren(webuiState('loading', webuiL('msg.loading', 'Loading...'), '')); });
	withEl('recent-list', function(el) { el.replaceChildren(webuiEl('li', {}, webuiState('loading', webuiL('msg.loading', 'Loading...'), ''))); });
	withEl('net-list', function(el) { el.replaceChildren(webuiEl('li', {}, webuiState('loading', webuiL('msg.loading', 'Loading...'), ''))); });
	initVolume();

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
		if (document.visibilityState !== 'visible') return;
		dashPolls.forEach(function(fn) { fn(); });
		// the sound once more as well; without the stream the poll above has read it
		if (webuiV1EventsLive()) refreshVolume();
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

// A single DOM order serves every screen width and the reading/tab order.
// Missing extension keys stay in the saved list so a returning card keeps
// its place. Only the reserved extension namespace may name absent cards.
// Arranging is a draft: moves and Reset change the page only, Apply keeps
// the order with one write, Cancel, Escape and leaving the page put the
// kept order back.
function initDashboardOrder() {
	const grid = document.querySelector('.webui-dashboard');
	const toggle = document.getElementById('dash-arrange');
	const cancel = document.getElementById('dash-arrange-cancel');
	const reset = document.getElementById('dash-arrange-reset');
	const hint = document.getElementById('dash-arrange-hint');
	const status = document.getElementById('dash-arrange-status');
	const bar = toggle.parentElement;
	const main = grid.closest('main');
	const key = 'webui-dashboard-order-v1';
	const core = Array.from(grid.children).map(c => c.dataset.dashboardKey).filter(s => s && !s.startsWith('ext:'));
	let order = [], arranging = false, drag = null, frame = null;
	let kept = [], changed = false;	// the order kept when the draft began; whether the draft moved a card
	let storageFailed = false;
	const cards = () => Array.from(grid.children).filter(c => c.dataset.dashboardKey);
	const id = c => c.dataset.dashboardKey;
	const label = c => c.id === 'card-now' ? webuiL('dash.arrange_now', 'Now')
		: c.getAttribute('aria-label') || (c.querySelector('.webui-card-title') || c).textContent.trim();
	const normal = list => Array.isArray(list) ? list.filter((s, i) => typeof s === 'string' &&
		(core.includes(s) || /^ext:[A-Za-z0-9_-][A-Za-z0-9._-]*$/.test(s)) && list.indexOf(s) === i) : [];
	try { order = normal(JSON.parse(localStorage.getItem(key))); }
	catch (e) { storageFailed = e.name !== 'SyntaxError'; }
	const defaults = () => core.concat(cards().map(id).filter(s => !core.includes(s)).sort());
	const balance = () => webuiBalanceGrid('.webui-dashboard', {tracks: 60, cardMin: 380, maxPer: 4,
		breakpoint: 760, skip: '.webui-dash-now, .webui-dash-gauges'});
	// a browser that keeps nothing is told when a draft begins (and on
	// Apply), not with every move of the draft
	const announce = (text, warn) => {
		status.textContent = text + (warn && storageFailed ? '. ' + webuiL('dash.arrange_not_saved',
			'Order could not be saved in this browser') : '');
	};
	// the height of the draft's bar, which covers the top of the page
	const barHeight = () => main.style.setProperty('--webui-dash-bar', arranging ? bar.offsetHeight + 'px' : '0px');
	const position = c => {
		const args = [label(c), cards().indexOf(c) + 1, cards().length];
		return webuiL('dash.arrange_position', '%s, position %s of %s').replace(/%s/g, () => args.shift());
	};
	const save = clear => {
		try {
			if (clear) localStorage.removeItem(key);
			else localStorage.setItem(key, JSON.stringify(order));
			storageFailed = false;
		} catch (e) { storageFailed = true; }
	};
	// moveBefore preserves focus and embedded state where available. The
	// fallback restores focus without scrolling the page to another card.
	const place = (card, before) => {
		if (card === before || card.nextElementSibling === before) return;
		const focused = document.activeElement;
		if (grid.moveBefore) grid.moveBefore(card, before);
		else grid.insertBefore(card, before);
		if (focused && card.contains(focused) && document.activeElement !== focused)
			focused.focus({preventScroll: true});
	};
	const observe = () => {
		observer.observe(grid, {childList: true});
		cards().forEach(c => observer.observe(c, {attributes: true, attributeFilter: ['hidden']}));
	};
	const changeDOM = fn => {
		observer.disconnect();
		try { fn(); } finally { observe(); }
	};
	const buttons = () => cards().forEach((c, i, all) => {
		const controls = c.querySelector('.webui-dash-order-controls');
		controls.hidden = !arranging;
		controls.querySelector('.webui-dash-order-before').setAttribute('aria-disabled', String(i === 0));
		controls.querySelector('.webui-dash-order-after').setAttribute('aria-disabled', String(i === all.length - 1));
	});
	const move = (card, target, focus) => {
		const all = cards(), from = all.indexOf(card);
		target = Math.max(0, Math.min(all.length - 1, target));
		if (from === target) return;
		all.splice(from, 1);
		changeDOM(() => place(card, all[target] || null));
		// Replace the occupied slots, leaving missing extensions in theirs.
		const visible = cards().map(id);
		let i = 0;
		order = order.map(s => visible.includes(s) ? visible[i++] : s);
		changed = true;
		balance();
		buttons();
		if (focus) {
			focus.focus({preventScroll: true});
			card.scrollIntoView({block: 'nearest', behavior: 'instant'});
		}
		announce(position(card));
	};
	const marker = webuiEl('div', {className: 'webui-dash-drop', hidden: true, attrs: {'aria-hidden': 'true'}});
	document.body.appendChild(marker);
	const finish = cancel => {
		if (!drag) return;
		const d = drag;
		drag = null;
		cancelAnimationFrame(frame);
		d.card.classList.remove('webui-dash-dragging');
		marker.hidden = true;
		if (d.handle.hasPointerCapture(d.pointer)) d.handle.releasePointerCapture(d.pointer);
		if (!cancel && d.active && d.width === innerWidth && d.height === innerHeight)
			move(d.card, d.target, d.handle);
	};
	const dropAt = () => {
		if (!drag || !drag.active) return;
		const d = drag, all = cards().filter(c => c !== d.card && !c.hidden);
		let best = null, distance = Infinity;
		all.forEach(c => {
			const r = c.getBoundingClientRect();
			const dx = Math.max(r.left - d.x, 0, d.x - r.right);
			const dy = Math.max(r.top - d.y, 0, d.y - r.bottom);
			const dist = dx * dx + dy * dy;
			if (dist < distance) { best = c; distance = dist; }
		});
		if (!best) return;
		const r = best.getBoundingClientRect();
		const row = all.some(c => c !== best && Math.abs(c.getBoundingClientRect().top - r.top) < 2);
		const after = row ? d.x > (r.left + r.right) / 2 : d.y > (r.top + r.bottom) / 2;
		d.target = cards().filter(c => c !== d.card).indexOf(best) + (after ? 1 : 0);
		const viewport = main.getBoundingClientRect();
		const shown = Math.max(viewport.top, bar.getBoundingClientRect().bottom);
		const edge = row ? (after ? r.right : r.left) : (after ? r.bottom : r.top);
		const top = row ? Math.max(r.top, shown) : edge - 2;
		const bottom = Math.min(r.bottom, viewport.bottom);
		marker.hidden = row ? bottom <= top : edge < shown || edge > viewport.bottom;
		Object.assign(marker.style, {left: (row ? edge - 2 : r.left) + 'px', top: top + 'px',
			width: (row ? 4 : r.width) + 'px', height: (row ? Math.max(0, bottom - top) : 4) + 'px'});
	};
	const scrollDrag = () => {
		if (!drag || !drag.active) return;
		const r = main.getBoundingClientRect();
		// the bar of the draft covers the top of the page while it is open
		const top = Math.max(r.top, bar.getBoundingClientRect().bottom);
		const y = drag.y;
		const step = y < top + 48 ? -Math.min(18, (top + 48 - y) / 3)
			: y > r.bottom - 48 ? Math.min(18, (y - r.bottom + 48) / 3) : 0;
		if (step) main.scrollTop += step;
		dropAt();
		frame = requestAnimationFrame(scrollDrag);
	};
	const addControls = card => {
		if (card.querySelector('.webui-dash-order-controls')) return;
		const handle = webuiEl('button', {type: 'button', className: 'webui-btn webui-dash-order-handle',
			attrs: {'aria-label': webuiL('dash.arrange_handle', 'Move %s').replace('%s', label(card)),
				'aria-describedby': 'dash-arrange-hint'}},
			webuiEl('span', {attrs: {'aria-hidden': 'true'}}, '⠿'), label(card));
		const step = (delta, cls, text, glyph) => webuiEl('button', {type: 'button', className: 'webui-btn ' + cls,
			attrs: {'aria-label': text + ': ' + label(card)}, on: {click: e => {
				if (e.currentTarget.getAttribute('aria-disabled') === 'true') return;
				finish(true);
				move(card, cards().indexOf(card) + delta, e.currentTarget);
			}}}, webuiEl('span', {attrs: {'aria-hidden': 'true'}}, glyph));
		const controls = webuiEl('div', {className: 'webui-dash-order-controls', hidden: !arranging}, handle,
			step(-1, 'webui-dash-order-before', webuiL('dash.arrange_before', 'Move earlier'), '↑'),
			step(1, 'webui-dash-order-after', webuiL('dash.arrange_after', 'Move later'), '↓'));
		card.prepend(controls);
		handle.addEventListener('keydown', e => {
			let target = cards().indexOf(card);
			if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') target--;
			else if (e.key === 'ArrowRight' || e.key === 'ArrowDown') target++;
			else if (e.key === 'Home') target = 0;
			else if (e.key === 'End') target = cards().length - 1;
			else return;
			e.preventDefault();
			finish(true);
			move(card, target, handle);
		});
		handle.addEventListener('pointerdown', e => {
			if (!arranging || e.button !== 0 || !e.isPrimary || drag) return;
			e.preventDefault();
			handle.focus({preventScroll: true});
			drag = {card, handle, pointer: e.pointerId, x0: e.clientX, y0: e.clientY,
				x: e.clientX, y: e.clientY, width: innerWidth, height: innerHeight, active: false, target: cards().indexOf(card)};
			handle.setPointerCapture(e.pointerId);
		});
		handle.addEventListener('pointermove', e => {
			if (!drag || e.pointerId !== drag.pointer) return;
			drag.x = e.clientX; drag.y = e.clientY;
			if (!drag.active && Math.hypot(drag.x - drag.x0, drag.y - drag.y0) >= 8) {
				drag.active = true;
				card.classList.add('webui-dash-dragging');
				scrollDrag();
			}
			dropAt();
		});
		handle.addEventListener('pointerup', e => { if (drag && e.pointerId === drag.pointer) finish(false); });
		handle.addEventListener('pointercancel', () => finish(true));
		handle.addEventListener('lostpointercapture', () => finish(true));
	};
	const reconcile = () => {
		finish(true);
		changeDOM(() => {
			const all = cards();
			order = normal(order);
			defaults().forEach(s => { if (!order.includes(s)) order.push(s); });
			let at = grid.firstElementChild;
			order.forEach(s => {
				const card = all.find(c => id(c) === s);
				if (!card) return;
				addControls(card);
				place(card, at);
				at = card.nextElementSibling;
			});
		});
		buttons();
		balance();
	};
	const observer = new MutationObserver(reconcile);
	reconcile();
	// the bar's buttons are one stop of Tab, the arrows move among them
	if (typeof webuiArrowKeys === 'function')
		webuiArrowKeys(bar, '.webui-btn');
	// The one button starts a draft and then applies it, so it says what a
	// press does now; Cancel and Reset are there while the draft is open.
	const mode = on => {
		arranging = on;
		toggle.textContent = on ? webuiL('dash.arrange_apply', 'Apply') : webuiL('dash.arrange', 'Arrange cards');
		toggle.classList.toggle('primary', on);
		bar.classList.toggle('drafting', on);
		main.classList.toggle('webui-dash-drafting', on);
		cancel.hidden = reset.hidden = hint.hidden = !on;
		buttons();
		balance();
		barHeight();
	};
	// The focus on a control the end of the draft hides goes to the button.
	const end = () => {
		const focused = document.activeElement;
		const lost = focused && focused !== toggle && (bar.contains(focused) || focused.closest('.webui-dash-order-controls'));
		mode(false);
		if (lost) toggle.focus({preventScroll: true});
	};
	const discard = quiet => {
		finish(true);
		order = kept.slice();
		changed = false;
		reconcile();
		end();
		if (!quiet) announce(webuiL('dash.arrange_discarded', 'Changes discarded'));
	};
	toggle.addEventListener('click', () => {
		finish(true);
		if (!arranging) {
			kept = order.slice();
			changed = false;
			mode(true);
			announce(hint.textContent, true);
			return;
		}
		// the default order is kept as no entry, so cards added later
		// take their default places
		if (changed) save(JSON.stringify(order) === JSON.stringify(defaults()));
		const saved = changed;
		changed = false;
		end();
		if (saved && storageFailed)
			status.textContent = webuiL('dash.arrange_not_saved', 'Order could not be saved in this browser');
		else
			announce(saved ? webuiL('dash.arrange_saved', 'Order saved') : webuiL('dash.arrange_unchanged', 'Order unchanged'));
	});
	cancel.addEventListener('click', () => discard(false));
	reset.addEventListener('click', () => {
		finish(true);
		order = defaults();
		changed = true;
		reconcile();
		announce(webuiL('dash.arrange_reset_done', 'Default order restored'));
	});
	// Escape ends a drag, else the draft; not when it closes a dialog
	document.addEventListener('keydown', e => {
		if (e.key !== 'Escape') return;
		if (drag) { e.preventDefault(); finish(true); return; }
		if (!arranging || e.defaultPrevented || (e.target.closest && e.target.closest('[role="dialog"], [role="alertdialog"], dialog')))
			return;
		e.preventDefault();
		discard(false);
	});
	// a page kept for Back must not come back with the draft open
	window.addEventListener('pagehide', () => { if (arranging) discard(true); });
	window.addEventListener('blur', () => finish(true));
	window.addEventListener('resize', () => { finish(true); balance(); barHeight(); });
}
