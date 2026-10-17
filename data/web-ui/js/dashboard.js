// The script of dashboard.html.

// Show EPG info for current channel
function showCurrentEpgInfo() {
	if (dash.channel)
		webuiShowEpgModal(dash.channel.id, dash.channel.name);
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

// Update gauge - circumference = 2 * PI * 16 = 100.5
function updateGauge(id, percent, colorClass) {
	const circumference = 100.5;
	const offset = circumference - (percent / 100 * circumference);
	const gauge = document.getElementById(id);
	if (gauge) {
		gauge.setAttribute('stroke-dashoffset', offset);
		gauge.classList.remove('ok', 'warning', 'danger');
		if (colorClass) {
			gauge.classList.add(colorClass);
		} else {
			// Default: percentage-based coloring
			if (percent >= 90) gauge.classList.add('danger');
			else if (percent >= 70) gauge.classList.add('warning');
			else gauge.classList.add('ok');
		}
	}
}

// Get color class for CPU load (not percentage-based)
function getCpuColorClass(load) {
	if (load >= 4.0) return 'danger';   // Very high load
	if (load >= 2.0) return 'warning';  // Moderate load
	return 'ok';                         // Normal
}

// ---- Following the box over /api/v1 ----------------------------------
// Channel, guide, recordings, timers, mode, standby and playback follow
// the event stream (/api/v1/events) instead of asking on a timer: each
// event rereads the one card it concerns, and a card only touches what
// changed. What v1 has no event for (system, network, tuner signal, stream
// details) is still asked on a timer, and only while the page is visible.

// set once the texts are in (Init below)
let NO_CHANNEL, NO_EPG, NO_TUNER, RECORDING;

const dash = {
	channel: null,		// {id, name, kind} of the current channel, null for none
	shown: null,		// the channel key the card shows, so a reread of the same one changes nothing
	now: null,		// the guide event running on the current channel
	next: null,		// and the one after it
	recordings: [],		// what the box records, its timeshift left out
	timers: [],
	playback: null,
	playbackAt: 0,		// when the playback state was read, to count its position on
	names: Object.create(null),	// channel id -> name, for timers and recordings
	naming: Object.create(null),	// channel id -> the read of its name under way
	channelRead: 0			// counts channel reads, so an older answer arriving last is dropped
};

// A read that fails is no answer: the card keeps what it shows or says
// there is nothing. A refusal for want of a sign-in opens the dialog once
// (webui-v1.js) and the read is sent again.
function dashGet(path, query) {
	return webuiV1('GET', path, query ? {query: query} : undefined).catch(function() { return null; });
}

function clock(sec) {
	return sec ? webuiFormatTime(new Date(sec * 1000)) : '--:--';
}

function dayAndClock(sec) {
	const d = new Date(sec * 1000);
	return {date: webuiPad2(d.getDate()) + '.' + webuiPad2(d.getMonth() + 1) + '.', time: webuiFormatTime(d)};
}

function duration(sec) {
	sec = Math.max(0, Math.floor(sec));
	const h = Math.floor(sec / 3600), m = Math.floor(sec / 60) % 60, s = sec % 60;
	return (h > 0 ? h + ':' + webuiPad2(m) : String(m)) + ':' + webuiPad2(s);
}

function remaining(stopSec) {
	const now = Math.floor(Date.now() / 1000);
	if (!stopSec || stopSec <= now) return '';
	const min = Math.floor((stopSec - now) / 60), hr = Math.floor(min / 60);
	return hr > 0 ? hr + 'h ' + (min % 60) + 'm' : min + ' min';
}

// The names of the channels timers and recordings name by id, each asked
// once, also while two cards ask at the same time.
async function ensureNames(ids) {
	await Promise.all(ids.map(function(id) {
		if (!id || id in dash.names) return null;
		if (!dash.naming[id]) {
			dash.naming[id] = dashGet('/channels/' + encodeURIComponent(id)).then(function(ch) {
				dash.names[id] = (ch && ch.name) || '';
				delete dash.naming[id];
			});
		}
		return dash.naming[id];
	}));
}

// takeMode: whether the mode is read off the channel's kind, which it is
// on the first read and on a zap (a zap to the other kind changes the
// mode). Otherwise the mode event says it: a box switched to TV whose
// zap to a TV channel did not go through still plays the radio channel.
async function refreshChannel(takeMode) {
	const read = ++dash.channelRead;
	const ch = await dashGet('/channels/current');
	if (read !== dash.channelRead) return;	// a later read was started: its answer counts
	if (!ch || !ch.id) {
		dash.channel = null;
		showChannel();
		dash.now = dash.next = null;
		showGuide();
		return;
	}
	dash.channel = {id: ch.id, name: ch.name || '', kind: ch.kind || ''};
	dash.names[ch.id] = ch.name || '';
	if (takeMode)
		setMode(ch.kind === 'radio' || ch.kind === 'webradio' ? 'radio' : 'tv');
	showChannel();
	refreshGuide();
}

// Rebuilt only when the channel is another one, so a reread does not
// reload the logo. The name comes unfiltered from the channel list and
// goes in as text.
function showChannel() {
	const ch = dash.channel;
	const key = ch ? ch.id + '\n' + ch.name : '';
	if (key === dash.shown) return;
	dash.shown = key;
	const chEl = document.getElementById('current-channel');
	if (!chEl) return;
	if (!ch) {
		chEl.classList.remove('has-logo');
		chEl.textContent = NO_CHANNEL;
		setText('channel-preview', NO_CHANNEL);
		return;
	}
	const name = ch.name || '--';
	// with a logo the name steps back (has-logo); a logo that does not
	// load leaves the name at full size
	const logo = webuiEl('img', {className: 'webui-status-channel-logo',
		attrs: {src: '/api/v1/channels/' + encodeURIComponent(ch.id) + '/logo', alt: ''},
		on: {error: function() { chEl.classList.remove('has-logo'); this.remove(); }}});
	chEl.classList.add('has-logo');
	chEl.replaceChildren(logo, webuiEl('span', {className: 'webui-status-channel-name'}, name));
	setText('channel-preview', name);
}

// What runs now on the current channel and what follows. Asked again on
// a zap, when the guide says it has news for this channel, and when the
// running event is over; the progress in between is counted here.
async function refreshGuide() {
	const ch = dash.channel;
	if (!ch) return;
	const now = await dashGet('/epg/current', {channel: ch.id});
	if (dash.channel !== ch) return;	// zapped meanwhile
	const from = (now && now.start) ? now.start + now.duration : Math.floor(Date.now() / 1000);
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

function showGuide() {
	const ev = dash.now;
	if (ev) {
		setText('current-epg', ev.title || '--');
		setText('epg-start', clock(ev.start));
		setText('epg-end', clock(ev.start + ev.duration));
	} else {
		setText('current-epg', dash.channel ? NO_EPG : '--');
		setText('epg-start', '--:--');
		setText('epg-end', '--:--');
		withEl('epg-progress', function(el) { el.value = 0; });
	}
	setText('next-epg', dash.next ? clock(dash.next.start) + ' ' + (dash.next.title || '--') : '--');
	tickGuide();
}

function tickGuide() {
	const ev = dash.now;
	if (!ev) return;
	const t = Date.now() / 1000;
	if (t >= ev.start + ev.duration) {
		// over: the next one runs now if it has begun, and the guide is asked again
		dash.now = (dash.next && dash.next.start <= t) ? dash.next : null;
		dash.next = null;
		showGuide();
		refreshGuide();
		return;
	}
	const pct = ev.duration > 0 ? Math.min(100, Math.max(0, Math.round(100 * (t - ev.start) / ev.duration))) : 0;
	withEl('epg-progress', function(el) { if (el.value !== pct) el.value = pct; });
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
	withEl('play-info', function(el) { el.classList.toggle('webui-hidden', !active); });
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
	setText('play-icon', (p.paused || p.state === 'paused') ? '❚❚' : '▶');
}

// Load System Info: /api/v1/system/info in the old shape (webui-header.js)
function loadSystemInfo() {
	webuiSystemInfo(false).then(function(data) {
		if (!data) return;

		// Uptime
		setText('sys-uptime', formatUptime(data.uptime || 0));

		// Temperature
		if (data.temperature !== null && data.temperature !== undefined) {
			withEl('temp-value', function(el) {
				el.textContent = data.temperature + '°C';
				el.classList.toggle('hot', data.temperature >= 70);
			});
		} else {
			withEl('sys-temp', function(el) { el.style.display = 'none'; });
		}

		// CPU - show real CPU usage percentage
		if (data.cpu && data.cpu.percent !== undefined) {
			const pct = data.cpu.percent;
			setText('cpu-percent', pct + '%');
			setText('cpu-load', data.cpu.cores + ' ' + webuiL('dash.cores', 'cores'));
			updateGauge('gauge-cpu', pct);
		} else if (data.load && data.load.length >= 1) {
			// Fallback to load average if cpu.percent not available
			const load1 = data.load[0];
			const gaugeFill = Math.min(100, Math.round(load1 * 12.5));
			setText('cpu-percent', load1.toFixed(2));
			setText('cpu-load', '1m Load');
			updateGauge('gauge-cpu', gaugeFill, getCpuColorClass(load1));
		}

		// Memory - show used/total
		if (data.memory) {
			const pct = data.memory.percent || 0;
			setText('ram-percent', pct + '%');
			setText('ram-info', formatKB(data.memory.used) + '/' + formatKB(data.memory.total));
			updateGauge('gauge-ram', pct);
		}

		// Disk - show free/total
		if (data.disk) {
			const pct = data.disk.percent || 0;
			setText('disk-percent', pct + '%');
			setText('disk-info', formatKB(data.disk.free) + ' ' + webuiL('dash.free', 'free'));
			updateGauge('gauge-disk', pct);
		}

		// System preview for collapsed mobile view
		let sysPreviewParts = [];
		if (data.temperature !== null && data.temperature !== undefined) {
			sysPreviewParts.push(data.temperature + '°C');
		}
		if (data.cpu && data.cpu.percent !== undefined) {
			sysPreviewParts.push('CPU ' + data.cpu.percent + '%');
		}
		if (data.memory && data.memory.percent !== undefined) {
			sysPreviewParts.push('RAM ' + data.memory.percent + '%');
		}
		setText('system-preview', sysPreviewParts.join(' · ') || '--');

		// Box name from hostname (sysinfo gives real hostname);
		// keep the previous text when neither field is present
		if (data.box && data.box.name) {
			setText('sys-box', data.box.name);
		} else if (data.hostname) {
			setText('sys-box', data.hostname);
		}

		// Kernel - short version
		if (data.kernel) {
			const k = data.kernel.split(' ')[0] || data.kernel;
			setText('sys-kernel', k);
		}

		// Hostname in network card header
		if (data.hostname) {
			setText('net-hostname', data.hostname);
		}

		// Network interfaces - filter to show only relevant ones
		if (data.interfaces && data.interfaces.length > 0) {
			// Filter: only interfaces with IP or physical interfaces
			const relevant = data.interfaces.filter(function(iface) {
				// Skip veth (docker), br- without IP
				if (iface.name.startsWith('veth')) return false;
				if (iface.name.startsWith('br-') && !iface.ip) return false;
				return true;
			}).slice(0, 6); // Max 6

			// Network preview - show primary IP
			const primaryNet = relevant.find(function(i) {
				return i.ip && /^(eth|en)/.test(i.name);
			}) || relevant.find(function(i) { return i.ip; });
			if (primaryNet && primaryNet.ip) {
				setText('network-preview', primaryNet.ip);
			}

			// interface names and addresses go in as text
			const netList = document.getElementById('net-interfaces');
			if (netList && relevant.length === 0) {
				netList.replaceChildren(webuiEl('div', {className: 'webui-text-muted'}, webuiL('si.no_ifaces', 'No interfaces')));
			} else if (netList) {
				webuiSync(netList, relevant, function(iface) { return iface.name; }, function(iface) {
					const isWlan = /^(wlan|wl)/.test(iface.name);
					const icon = isWlan ?
						'<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8.111 16.404a5.5 5.5 0 017.778 0M12 20h.01m-7.08-7.071c3.904-3.905 10.236-3.905 14.141 0M1.394 9.393c5.857-5.857 15.355-5.857 21.213 0"/></svg>' :
						'<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 12h14M5 12a2 2 0 01-2-2V6a2 2 0 012-2h14a2 2 0 012 2v4a2 2 0 01-2 2M5 12a2 2 0 00-2 2v4a2 2 0 002 2h14a2 2 0 002-2v-4a2 2 0 00-2-2"/></svg>';
					const macStr = iface.mac ? String(iface.mac).toUpperCase() : '';
					return webuiEl('div', {className: 'webui-net-item'},
						webuiEl('div', {className: 'webui-net-icon'}, webuiMarkup(icon)),
						webuiEl('div', {className: 'webui-net-info'},
							webuiEl('div', {className: 'webui-net-name'}, iface.name),
							webuiEl('div', {className: 'webui-net-ip'}, iface.ip || webuiL('dash.no_ip', 'no IP')),
							macStr ? webuiEl('div', {className: 'webui-net-mac'}, macStr) : null),
						webuiEl('div', {className: 'webui-net-status' + (iface.up ? ' up' : '')}));
				});
			}
		}
	});

}

// Software versions, which do not change while the page is open
function loadVersions() {
	// the reading the header and the footer already made
	webuiSystemInfoOnce().then(webuiVersions).then(function(data) {
		if (data) {
			if (data.webui) setText('webui-version', data.webui);
			if (data.api) setText('api-version', data.api);
			if (data.copyright && data.copyright.text)
				setText('webui-footer-copyright', data.copyright.text);
		}
	});
}

// What the box records, from GET /api/v1/recordings; the time left comes
// from the timer that carries the recording.
async function refreshRecordings() {
	const r = await dashGet('/recordings');
	dash.recordings = ((r && r.items) || []).filter(function(it) { return !it.timeshift; });
	await ensureNames(dash.recordings.map(function(it) { return it.channel_id; }));
	showRecording();
}

function showRecording() {
	const rec = dash.recordings[0] || null;
	const isRecording = !!rec;
	withEl('rec-indicator', function(el) { el.classList.toggle('webui-hidden', !isRecording); });
	withEl('rec-info', function(el) { el.classList.toggle('webui-hidden', !isRecording); });

	const recBtn = document.getElementById('rec-btn');
	if (recBtn) {
		recBtn.classList.toggle('recording', isRecording);
		recBtn.querySelector('span').textContent = isRecording ? 'Stop' : 'Rec';
		recBtn.title = isRecording ? webuiL('dash.record_stop', 'Stop recording') : webuiL('dash.record', 'Record');
	}
	if (!rec) return;

	const channel = dash.names[rec.channel_id] || '';
	const timer = dash.timers.find(function(t) { return t.id === rec.id; });
	setText('rec-title', rec.title || channel || RECORDING);
	setText('rec-channel', channel);
	setText('rec-remaining', (timer && remaining(timer.stop)) || '--:--');
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
		webuiShowToast(webuiL('sb.fail', 'Standby failed') + ': ' + e.message, true);
	}
}

async function sendStandby() {
	try {
		const st = await webuiV1('GET', '/system/standby');
		const inStandby = !!(st && st.on);
		if (!inStandby) {
			webuiShowConfirm('Standby', webuiLH('sb.confirm', 'Put the box into standby?'), 'Standby', function() { return toggleStandby(false); }, {variant: 'ok'});
			return;
		}
		await toggleStandby(true);
	} catch (e) {
		webuiShowToast(webuiL('sb.fail', 'Standby failed') + ': ' + e.message, true);
	}
}

// TV/Radio Mode: read off the current channel and the mode event
var currentMode = 'tv';

function setMode(mode) {
	if (mode === currentMode) return;
	currentMode = mode;
	updateModeUI();
}

async function switchMode() {
	const newMode = currentMode === 'tv' ? 'radio' : 'tv';
	withEl('mode-switch-btn', function(el) { el.disabled = true; el.style.opacity = '0.6'; });
	try {
		await webuiV1('POST', '/mode', {body: {mode: newMode}});
		setMode(newMode);
	} catch (e) {
		webuiShowToast(e.message, true);
	}
	withEl('mode-switch-btn', function(el) { el.disabled = false; el.style.opacity = '1'; });
}

function updateModeUI() {
	const isRadio = currentMode === 'radio';

	// Update current mode display
	setText('mode-current-text', isRadio ? 'Radio' : 'TV');
	withEl('mode-current-icon-tv', function(el) { el.style.display = isRadio ? 'none' : ''; });
	withEl('mode-current-icon-radio', function(el) { el.style.display = isRadio ? '' : 'none'; });

	// Update switch button text
	setText('mode-switch-text', isRadio ? webuiL('dash.to_tv', 'Switch to TV') : webuiL('dash.to_radio', 'Switch to radio'));

	// Actions preview - show current mode
	setText('actions-preview', isRadio ? webuiL('dash.radio_mode', 'Radio mode') : webuiL('dash.tv_mode', 'TV mode'));

	// In radio mode: clear video-specific fields
	if (isRadio) {
		withEl('stream-resolution', function(el) { el.textContent = '--'; el.classList.remove('hd', 'sd'); });
		setText('stream-fps', '--');
		setText('stream-preview', 'Radio');
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

async function refreshTimers() {
	const r = await dashGet('/timers');
	if (!r) {
		withEl('timer-list', function(el) {
			el.replaceChildren(webuiEl('div', {className: 'webui-timer-empty'}, webuiL('timer.unavail', 'Timers not available')));
		});
		setText('timer-preview', '--');
		return;
	}
	dash.timers = r.items || [];
	await ensureNames(dash.timers.map(function(t) { return t.channel_id; }));
	showTimers();
	showRecording();
}

// Running recordings first, then the next scheduled timers. Titles and
// channel names come from the guide and go in as text.
function showTimers() {
	const container = document.getElementById('timer-list');
	if (!container) return;
	const now = Math.floor(Date.now() / 1000);
	const running = dash.timers.filter(isRunningRecording);
	const future = dash.timers.filter(function(t) {
		return t.state !== 2 && t.start > now;
	}).sort(function(a, b) { return a.start - b.start; })
		.slice(0, Math.max(3 - running.length, 1));

	if (running.length === 0 && future.length === 0) {
		container.replaceChildren(webuiEl('div', {className: 'webui-timer-empty'}, webuiL('dash.no_timers', 'No timers programmed')));
		setText('timer-preview', webuiL('dash.no_timers_short', 'No timers'));
		return;
	}
	if (running.length > 0) {
		setText('timer-preview', '● ' + webuiL('bc.rec_running', 'recording running'));
	} else {
		const at = dayAndClock(future[0].start);
		setText('timer-preview', at.date + ' ' + at.time);
	}

	const rows = running.map(function(t) { return {timer: t, running: true}; })
		.concat(future.map(function(t) { return {timer: t, running: false}; }));
	webuiSync(container, rows, function(row) {
		return (row.running ? 'run-' : 'next-') + row.timer.id;
	}, function(row) {
		const timer = row.timer;
		const channel = dash.names[timer.channel_id] || '';
		const n = timerTypeNumber(timer);
		const label = webuiGetTimerType({type_number: n});
		if (row.running) {
			const left = remaining(timer.stop);
			return webuiEl('div', {className: 'webui-timer-item running'},
				webuiEl('div', {className: 'webui-timer-icon record'}, webuiMarkup('<svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/></svg>')),
				webuiEl('div', {className: 'webui-timer-info'},
					webuiEl('div', {className: 'webui-timer-title'}, timer.title || channel || RECORDING),
					channel ? webuiEl('div', {className: 'webui-timer-channel'}, channel) : null),
				left ? webuiEl('div', {className: 'webui-timer-remaining'}, left) : null);
		}
		const at = dayAndClock(timer.start);
		return webuiEl('div', {className: 'webui-timer-item'},
			// the icon by the type's own name; the label is translated
			webuiMarkup(webuiGetTimerIconHtml(WEBUI_TIMER_TYPES[n] || '', 'webui-timer-icon')),
			webuiEl('div', {className: 'webui-timer-info'},
				webuiEl('div', {className: 'webui-timer-title'}, timer.title || channel || label),
				channel ? webuiEl('div', {className: 'webui-timer-channel'}, channel) : null),
			webuiEl('div', {className: 'webui-timer-time'},
				webuiEl('div', {className: 'webui-timer-date'}, at.date),
				webuiEl('div', {className: 'webui-timer-hour'}, at.time)));
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
		'record-stop': function() { refreshTimers().then(refreshRecordings); },
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

// Signal quality class based on percentage
function getSignalQuality(percent) {
	if (percent >= 80) return 'excellent';
	if (percent >= 60) return 'good';
	if (percent >= 40) return 'moderate';
	if (percent >= 20) return 'poor';
	return 'bad';
}

// BER quality class (lower is better)
function getBerQuality(ber) {
	if (ber === 0) return 'ok';
	if (ber < 100) return 'ok';
	if (ber < 1000) return 'warning';
	return 'error';
}

// Update signal bar
function updateSignalBar(barId, valueId, percent) {
	const bar = document.getElementById(barId);
	const value = document.getElementById(valueId);
	const quality = getSignalQuality(percent);

	if (bar) {
		bar.style.width = percent + '%';
		bar.className = 'webui-signal-bar ' + quality;
	}
	if (value) {
		value.textContent = percent + '%';
		value.className = 'webui-signal-value ' + quality;
	}
}

// The tuners with what each receiving one measures (GET /tuner/frontends):
// the live one shows the channel the box plays, another one in use is held
// by something else, most often a recording. The readings are raw, out of
// full_scale. A read that fails keeps what the card shows.
async function loadTunerStatus() {
	const got = await dashGet('/tuner/frontends');
	if (!got || !Array.isArray(got.items)) return;
	renderTunerList(got.items.map(function(fe) {
		const sig = fe.signal || {};
		const scale = Number(sig.full_scale) || 0;
		function percent(v) { return fe.measured && scale > 0 ? Math.round(Number(v || 0) * 100 / scale) : 0; }
		return {
			id: fe.adapter + ':' + fe.number,
			name: fe.name || '',
			// the kind it receives; which delivery systems it takes the box does not say
			type: fe.satellite ? 'DVB-S' : fe.cable ? 'DVB-C' : fe.terrestrial ? 'DVB-T' : 'DVB',
			sig: percent(sig.strength),
			snr: percent(sig.snr),
			channel: fe.live && dash.channel ? dash.channel.name : '',
			state: fe.live ? 'viewing' : fe.in_use ? 'busy' : 'idle'
		};
	}));
}

// Render multi-tuner list
function renderTunerList(tuners) {
	const container = document.getElementById('tuner-list');
	if (!container) return;
	// a box without a tuner says so rather than showing an empty card
	if (!tuners.length) {
		container.replaceChildren(webuiEl('div', {className: 'webui-tuner-empty'}, NO_TUNER));
		// the icon greyed as for idle tuners, the preview saying why
		updateTunerPreview(tuners, 0, 0, 0);
		setText('tuner-preview', NO_TUNER);
		return;
	}

	const rows = [];
	let activeTuners = 0;
	let totalSig = 0;
	let totalSnr = 0;

	tuners.forEach(function(tuner) {
		const sig = Math.max(0, Math.min(100, Number(tuner.sig) || 0));
		const snr = Math.max(0, Math.min(100, Number(tuner.snr) || 0));
		const sigQuality = getSignalQuality(sig);
		const snrQuality = getSignalQuality(snr);
		const sigColor = getSignalColor(sig);
		const snrColor = getSignalColor(snr);

		// Short name for tuner
		let shortName = tuner.name || 'Tuner ' + tuner.id;
		if (shortName.length > 25) {
			shortName = shortName.substring(0, 22) + '...';
		}

		// State label translation
		const stateLabels = {
			'viewing': 'Live',
			'busy': 'Busy',
			'idle': 'Idle'
		};
		const stateLabel = stateLabels[tuner.state] || tuner.state;

		// Track active tuners for preview
		if (tuner.state !== 'idle') {
			activeTuners++;
			totalSig += sig;
			totalSnr += snr;
		}

		rows.push({tuner: tuner, shortName: shortName, stateLabel: stateLabel, sig: sig, snr: snr,
			sigColor: sigColor, snrColor: snrColor});
	});

	// the channel name comes from the broadcast and goes in as text; the
	// state only as a class it is known to be
	webuiSync(container, rows, function(row, i) {
		return row.tuner.id !== undefined ? row.tuner.id : '#' + i;
	}, function(row) {
		const tuner = row.tuner;
		const knownState = ['viewing', 'busy', 'idle'].indexOf(tuner.state) >= 0;
		function signal(label, value, color) {
			return webuiEl('div', {className: 'webui-tuner-signal'},
				webuiEl('div', {className: 'webui-tuner-signal-header'},
					webuiEl('span', {className: 'webui-tuner-signal-label'}, label),
					webuiEl('span', {className: 'webui-tuner-signal-value', style: {color: color}}, value + '%')),
				webuiEl('div', {className: 'webui-tuner-signal-bar'},
					webuiEl('div', {className: 'webui-tuner-signal-fill', style: {width: value + '%', background: color}})));
		}
		return webuiEl('div', {className: 'webui-tuner-item'},
			webuiEl('div', {className: 'webui-tuner-header'},
				webuiEl('span', {className: 'webui-tuner-name', title: tuner.name || ''}, row.shortName),
				webuiEl('span', {className: 'webui-tuner-type'}, tuner.type || 'DVB'),
				webuiEl('span', {className: 'webui-tuner-state' + (knownState ? ' ' + tuner.state : '')}, row.stateLabel)),
			tuner.channel ? webuiEl('div', {className: 'webui-tuner-channel'}, tuner.channel) : null,
			webuiEl('div', {className: 'webui-tuner-signals'},
				signal('SIG', row.sig, row.sigColor),
				signal('SNR', row.snr, row.snrColor)));
	});

	// Update preview and icon
	updateTunerPreview(tuners, activeTuners, totalSig, totalSnr);
}

// Get signal color for inline styles
function getSignalColor(percent) {
	if (percent >= 80) return '#22c55e';
	if (percent >= 60) return '#84cc16';
	if (percent >= 40) return '#eab308';
	if (percent >= 20) return '#f97316';
	return '#ef4444';
}

// Update tuner card preview and icon
function updateTunerPreview(tuners, activeTuners, totalSig, totalSnr) {
	const previewEl = document.getElementById('tuner-preview');
	const iconEl = document.getElementById('tuner-status-icon');

	// Preview text
	if (previewEl) {
		if (activeTuners > 0) {
			const avgSig = Math.round(totalSig / activeTuners);
			const avgSnr = Math.round(totalSnr / activeTuners);
			previewEl.textContent = activeTuners + '/' + tuners.length + ' aktiv';
		} else {
			previewEl.textContent = tuners.length + ' Tuner';
		}
	}

	// Icon color based on active tuner status
	if (iconEl) {
		const svg = iconEl.querySelector('svg');
		if (svg) {
			if (activeTuners > 0) {
				const avgSig = totalSig / activeTuners;
				const avgSnr = totalSnr / activeTuners;
				const avgQuality = (avgSig + avgSnr) / 2;
				if (avgQuality >= 60) {
					svg.style.color = '#22c55e';
				} else if (avgQuality >= 40) {
					svg.style.color = '#eab308';
				} else {
					svg.style.color = '#ef4444';
				}
			} else {
				svg.style.color = 'var(--pico-muted-color)';
			}
		}
	}
}

// The names the sound codecs go by on the card
const SOUND_CODECS = {mp2: 'MPEG', ac3: 'AC3', eac3: 'E-AC3', aac: 'AAC', aacplus: 'HE-AAC',
	dts: 'DTS', dtshd: 'DTS-HD', lpcm: 'LPCM'};

// The picture as the decoder shows it (GET /playback/picture); the sound
// as the selected track of the running channel and the channel's packet
// ids (GET /channels/current). A picture read that fails keeps what the
// card shows; sound and packet ids go with a channel that cannot be read,
// which is also what a playing recording or file answers.
async function loadStreamInfo() {
	const both = await Promise.all([dashGet('/playback/picture'), dashGet('/channels/current')]);
	const pic = both[0], ch = both[1];
	if (pic) {
		const shown = !!pic.shown && pic.width > 0 && pic.height > 0;
		updateStreamInfoUI(shown ? pic.width + 'x' + pic.height : '', shown ? pic.width : 0,
			shown ? pic.height : 0, shown ? pic.aspect : '', shown && pic.frame_rate ? pic.frame_rate + 'fps' : '',
			soundOf(ch));
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


// Update stream info UI
function updateStreamInfoUI(resolution, width, height, aspect, fps, audio) {
	// In radio mode: only show audio, hide video-specific info
	if (currentMode === 'radio') {
		withEl('stream-resolution', function(el) { el.textContent = '--'; el.classList.remove('hd', 'sd'); });
		setText('stream-aspect', '--');
		setText('stream-fps', '--');
		setText('stream-preview', 'Radio');
	} else {
		const resEl = document.getElementById('stream-resolution');
		if (resEl) {
			resEl.textContent = resolution || '--';
			resEl.classList.remove('hd', 'sd');
			if (height > 0) {
				resEl.classList.add(height >= 720 ? 'hd' : 'sd');
				const badge = height >= 720 ? 'HD' : 'SD';
				setText('stream-preview', resolution + ' ' + badge);
			} else {
				setText('stream-preview', resolution || '--');
			}
		}
		setText('stream-aspect', aspect || '--');
		setText('stream-fps', fps || '--');
	}
	setText('stream-audio', audio || '--');
}


// M5d: Restreihen-Balancer — Logik lebt seit N2a geteilt in
// webui-header.js (webuiBalanceGrid), Tools nutzt sie ebenfalls.
function webuiBalanceDashboard() {
	webuiBalanceGrid('.webui-dashboard', {wideClass: 'dash-wide'});
}

// Init, once the texts are in
webuiAfterTexts(function() {
	NO_CHANNEL = webuiL('dash.no_channel', 'No channel');
	NO_EPG = webuiL('dash.no_epg', 'No EPG data');
	NO_TUNER = webuiL('dash.no_tuner', 'No tuner');
	RECORDING = webuiL('bc.recording', 'Recording');
	webuiInitMobileAccordion({
		cardOrder: ['card-actions', 'card-channel', 'card-system', 'card-network', 'card-timer', 'card-tuner', 'card-stream'],
		storageKey: 'webui-active-card',
		defaultCard: 'card-actions',
		breakpoint: 768,
		indicatorId: 'card-indicator',
		containerSelector: '.webui-dashboard',
		mode: 'expanded'
	});

	refreshEvented();
	followTheBox();
	loadVersions();
	loadSystemInfo();
	loadTunerStatus();
	loadStreamInfo();

	// M5d: initial packen, bei Resize neu, und wenn Karten dazukommen
	// oder verschwinden (Extension-Injektion laeuft nach webuiInit)
	webuiBalanceDashboard();
	window.addEventListener('resize', webuiBalanceDashboard);
	new MutationObserver(webuiBalanceDashboard)
		.observe(document.querySelector('.webui-dashboard'), {childList: true});

	// No event for these: asked on a timer, while visible
	whileVisible(30000, loadSystemInfo);
	whileVisible(5000, loadTunerStatus);
	whileVisible(30000, loadStreamInfo);
	// Without an open event stream the evented cards are asked as well
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
			return new Promise(resolve => setTimeout(resolve, 800));
		});
	}
});
