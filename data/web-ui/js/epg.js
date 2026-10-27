// The script of epg.html.

// The programme grid over /api/v1: the bouquet's channels, then the
// guide window for 20 of them per request (GET /api/v1/epg/grid),
// filled in as the pages arrive. Titles and texts come from the
// broadcast and go in as text. The grid follows the box: news of the
// guide fill the rows they concern again, a timer set or taken away moves
// the marks, a zap moves the channel the box plays, and what runs now and
// the line of now move on by the minute.
const MINUTES_PER_SLOT = 30;
const DISPLAY_HOURS = 4;
const TOTAL_MINUTES = DISPLAY_HOURS * 60;

let currentBouquet = '';
let currentMode = 'tv';
let currentStartTime = null;
let currentChannelId = null;
let bouquetError = null;	// why the bouquets could not be read, null once they were
let timers = [];
let gridToken = 0;
let gridRows = Object.create(null);	// channel id -> {channel, el, events}, for the grid shown
let gridWindow = null;		// {start, stop} of the grid shown

const formatTime = webuiFormatTime;
const clock = webuiClock;

function initTimeSelect() {
	const select = document.getElementById('timeSelect');
	const now = new Date(webuiBoxNow());
	now.setMinutes(0, 0, 0);
	const options = [];
	for (let i = 0; i < 24; i++) {
		const time = new Date(now.getTime() + i * 3600000);
		options.push(webuiEl('option', {value: String(Math.floor(time.getTime() / 1000))},
			webuiPad2(time.getHours()) + ':00'));
	}
	select.replaceChildren.apply(select, options);
	select.selectedIndex = 0;
	currentStartTime = parseInt(select.value);
}

// The bouquets of the list the box plays from, hidden and empty ones left out;
// shown first is the one holding the channel the box plays, as in the channel list
async function loadBouquets() {
	const select = document.getElementById('bouquetSelect');
	bouquetError = null;
	try {
		const ch = await webuiV1('GET', '/channels/current').catch(() => null);
		currentChannelId = ch ? ch.id : null;
		currentMode = ch && (ch.kind === 'radio' || ch.kind === 'webradio') ? 'radio' : 'tv';
		const r = await webuiV1('GET', '/bouquets');
		const bouquets = ((r && r.items) || []).filter(b =>
			!b.hidden && (currentMode === 'radio' ? b.radio_count : b.tv_count) > 0);
		select.replaceChildren.apply(select, bouquets.map(b => webuiEl('option', {value: String(b.id)}, b.name)));
		let found = null;
		if (ch && ch.id) {
			const holding = await webuiV1('GET', '/bouquets', {query: {holds: ch.id}}).catch(() => null);
			found = holding && (holding.items || []).find(x => bouquets.some(b => b.id === x.id));
		}
		currentBouquet = found ? String(found.id) : (bouquets.length ? String(bouquets[0].id) : '');
		select.value = currentBouquet;
	} catch (e) {
		bouquetError = e;
		select.replaceChildren(webuiEl('option', {value: ''}, webuiL('epg.load_fail', 'Loading failed')));
	}
}

async function loadTimers() {
	try {
		const r = await webuiV1('GET', '/timers');
		timers = (r && r.items) || [];
	} catch (e) {
		timers = [];
	}
}

// The timer that covers an event: a recording from before its start to
// after its end, or a zap timer while it runs (one set on a running
// event starts at the minute it was set)
function timerFor(channelId, ev) {
	return timers.find(t => t.channel_id === channelId &&
		((t.kind === 'record' && t.start <= ev.start && t.stop >= ev.start + ev.duration) ||
		 (t.kind === 'zapto' && t.start >= ev.start && t.start < ev.start + ev.duration)));
}

function nowPercentOf(start, now) {
	return ((now - start) / 60) / TOTAL_MINUTES * 100;
}

async function loadEpg() {
	const token = ++gridToken;
	const grid = document.getElementById('epgGrid');
	grid.replaceChildren(webuiState('loading', webuiL('epg.loading', 'Loading EPG data...'), ''));
	gridRows = Object.create(null);
	gridWindow = null;

	currentBouquet = document.getElementById('bouquetSelect').value;
	currentStartTime = parseInt(document.getElementById('timeSelect').value);
	const startTime = currentStartTime;
	const stopTime = startTime + DISPLAY_HOURS * 3600;
	if (bouquetError) {
		grid.replaceChildren(webuiStateFor(bouquetError, {retry: () => loadBouquets().then(loadEpg)}));
		return;
	}
	if (!currentBouquet) {
		grid.replaceChildren(webuiState('empty', webuiL('bq.no_channels', 'No channels found'), ''));
		return;
	}

	let drawn = false;
	try {
		await loadTimers();
		const channels = await webuiV1All('/channels', {mode: currentMode, bouquet: currentBouquet});
		if (token !== gridToken) return;
		if (!channels.length) {
			grid.replaceChildren(webuiState('empty', webuiL('bq.no_channels', 'No channels found'), ''));
			return;
		}

		const now = webuiBoxSec();
		await webuiLogosOffered(channels[0].id);
		if (token !== gridToken) return;

		const slots = [];
		for (let i = 0; i < TOTAL_MINUTES / MINUTES_PER_SLOT; i++)
			slots.push(webuiEl('div', {className: 'webui-epg-time-slot'}, clock(startTime + i * MINUTES_PER_SLOT * 60)));
		slots.push(webuiEl('div', {className: 'webui-epg-now-marker', hidden: true}));
		const header = webuiEl('div', {className: 'webui-epg-time-header', attrs: {'aria-hidden': 'true'}},
			webuiEl('div', {className: 'webui-epg-channel-header'}, webuiL('msg.channel', 'Channel')),
			webuiEl('div', {className: 'webui-epg-times'}, slots));

		const rows = webuiEl('div', {className: 'webui-epg-channels', style: {position: 'relative'}},
			webuiEl('div', {className: 'webui-epg-now-line', hidden: true}));
		channels.forEach(channel => {
			const programs = webuiEl('div', {className: 'webui-epg-programs'});
			gridRows[channel.id] = {channel: channel, el: programs, events: null};
			const zapLabel = webuiL('epg.zap_to', 'Switch to') + ' ' + channel.name;
			rows.appendChild(webuiEl('div', {className: 'webui-epg-row' + (channel.id === currentChannelId ? ' playing' : ''), data: {id: channel.id}},
				webuiEl('div', {className: 'webui-epg-channel-name', title: zapLabel, tabIndex: 0,
					attrs: {role: 'button', 'aria-label': zapLabel},
					on: {click: () => zapTo(channel.id), keydown: ev => pressed(ev, () => zapTo(channel.id))}},
					// its number and its logo, its name where it has none
					channel.number > 0 ? webuiEl('span', {className: 'webui-epg-channel-num'}, String(channel.number)) : null,
					webuiChannelLogo(channel.id, channel.name)),
				programs));
		});
		grid.replaceChildren(header, rows);
		drawn = true;
		gridWindow = {start: startTime, stop: stopTime};
		placeNow(now);

		// the guide, a page of channels at a time
		let cursor = null;
		const filled = new Set();
		do {
			const query = {bouquet: currentBouquet, from: startTime, to: stopTime, limit: 20};
			if (cursor) query.cursor = cursor;
			const page = await webuiV1('GET', '/epg/grid', {query: query});
			if (token !== gridToken) return;
			((page && page.items) || []).forEach(row => {
				const target = gridRows[row.channel_id];
				if (!target) return;
				target.events = row.events || [];
				fillRow(target);
				filled.add(row.channel_id);
			});
			cursor = page && page.next_cursor;
		} while (cursor);
		// a channel the guide gave no row for has nothing in the window
		Object.keys(gridRows).forEach(id => {
			if (!filled.has(id)) {
				gridRows[id].events = [];
				fillRow(gridRows[id]);
			}
		});
	} catch (e) {
		if (token !== gridToken) return;
		// rows already drawn stay; the grid as a whole says why it could not
		if (drawn)
			webuiToastError(webuiL('epg.load_fail', 'Loading failed'), e);
		else
			grid.replaceChildren(webuiStateFor(e, {retry: loadEpg}));
	}
}

// Enter or Space on a cell that acts as a button
function pressed(ev, fn) {
	if (ev.key === 'Enter' || ev.key === ' ') {
		ev.preventDefault();
		fn();
	}
}

// The programmes of a row in the window, each a button that names it,
// its time, its channel and a timer set on it; a row the guide knows
// nothing for says so
function fillRow(target) {
	const channel = target.channel, el = target.el, events = target.events || [];
	const win = gridWindow;
	if (!win) return;
	const now = webuiBoxSec();
	const boxes = [];
	events.forEach(ev => {
		const evStop = ev.start + ev.duration;
		if (evStop <= win.start || ev.start >= win.stop) return;
		const displayStart = Math.max(ev.start, win.start);
		const displayStop = Math.min(evStop, win.stop);
		const leftPercent = ((displayStart - win.start) / 60) / TOTAL_MINUTES * 100;
		const widthPercent = ((displayStop - displayStart) / 60) / TOTAL_MINUTES * 100;
		const timer = timerFor(channel.id, ev);
		const mark = timer ? (timer.kind === 'record' ? webuiL('epg.timer_rec_mark', 'recording timer set') : webuiL('epg.timer_zap_mark', 'zap timer set')) : '';
		const name = [ev.title || '', clock(ev.start) + ' – ' + clock(evStop), channel.name, mark].filter(Boolean).join(', ');
		// under ten minutes of the four hours shown: too narrow for its text
		const tiny = widthPercent < 10 / TOTAL_MINUTES * 100 ? ' tiny' : '';
		boxes.push(webuiEl('div', {className: 'webui-epg-program' + tiny + (timer ? (timer.kind === 'record' ? ' timer-rec' : ' timer-zap') : ''),
			tabIndex: 0, attrs: {role: 'button', 'aria-label': name}, data: {start: String(ev.start), stop: String(evStop)},
			style: {left: leftPercent + '%', width: 'calc(' + widthPercent + '% - 2px)'},
			on: {click: () => showProgramDetail(channel, ev), keydown: k => pressed(k, () => showProgramDetail(channel, ev))}},
			webuiEl('div', {className: 'webui-epg-program-title'}, ev.title || ''),
			webuiEl('div', {className: 'webui-epg-program-time'}, clock(ev.start))));
	});
	if (!boxes.length)
		boxes.push(webuiEl('div', {className: 'webui-epg-none'}, webuiL('epg.none', 'No programme data')));
	el.replaceChildren.apply(el, boxes);
	markNow(el, now);
}

// What runs now in a row: marked, with how far it has run
function markNow(scope, now) {
	scope.querySelectorAll('.webui-epg-program').forEach(box => {
		const start = Number(box.dataset.start), stop = Number(box.dataset.stop);
		const current = now >= start && now < stop;
		box.classList.toggle('current', current);
		if (current)
			box.style.setProperty('--progress', Math.round((now - start) / (stop - start) * 100) + '%');
		else
			box.style.removeProperty('--progress');
	});
}

/* The line of now and its mark in the head, where now lies over the
   programmes: past the channel column, whose width differs with the
   screen, by its share of the programme area */
function placeNow(now) {
	const win = gridWindow;
	const programs = document.querySelector('.webui-epg-programs');
	if (!win || !programs) return;
	const inWindow = now >= win.start && now < win.stop;
	const x = programs.offsetLeft + programs.clientWidth * nowPercentOf(win.start, now) / 100;
	document.querySelectorAll('.webui-epg-now-line, .webui-epg-now-marker').forEach(el => {
		el.hidden = !inWindow;
		el.style.left = x + 'px';
	});
}

// By the minute: the line of now and what runs now move on
function tickNow() {
	if (!gridWindow) return;
	const now = webuiBoxSec();
	placeNow(now);
	markNow(document.getElementById('epgGrid'), now);
}

// The rows whose guide has news, asked again once the news settle
let newsSoon = null;
const newsFor = new Set();
function epgUpdated(d) {
	if (!d || !d.channel_id || !gridWindow) return;
	// the guide names a channel in the lower 48 bits of its id
	const low = webuiLegacyChannelId(d.channel_id).slice(-12).toLowerCase();
	Object.keys(gridRows).forEach(id => {
		if (webuiLegacyChannelId(id).slice(-12).toLowerCase() === low) newsFor.add(id);
	});
	if (!newsFor.size || newsSoon) return;
	newsSoon = setTimeout(refreshRows, 2000);
}

async function refreshRows() {
	newsSoon = null;
	const ids = Array.from(newsFor);
	newsFor.clear();
	const token = gridToken, win = gridWindow;
	if (!win) return;
	for (let i = 0; i < ids.length; i += 20) {
		const page = await webuiV1('GET', '/epg/grid', {query: {from: win.start, to: win.stop, limit: 20,
			channels: ids.slice(i, i + 20).join(',')}}).catch(() => null);
		if (token !== gridToken) return;
		((page && page.items) || []).forEach(row => {
			const target = gridRows[row.channel_id];
			if (!target) return;
			target.events = row.events || [];
			fillRow(target);
		});
	}
}

// A timer set or taken away anywhere moves the marks
async function timersChanged() {
	await loadTimers();
	Object.keys(gridRows).forEach(id => { if (gridRows[id].events) fillRow(gridRows[id]); });
}

// The channel the box plays, after a zap from anywhere
async function zapped() {
	const ch = await webuiV1('GET', '/channels/current').catch(() => null);
	currentChannelId = ch ? ch.id : null;
	document.querySelectorAll('.webui-epg-row').forEach(row => row.classList.toggle('playing', row.dataset.id === currentChannelId));
}

// A programme in the shared dialog, the marks of the grid following a
// timer set from it
function showProgramDetail(channel, ev) {
	webuiShowEpgModal(channel.id, channel.name, {event: ev, number: channel.number,
		onTimer: () => { if (!webuiV1EventsLive()) timersChanged(); }});
}

// Zap: a file the player plays, or a box in standby, is asked about first
async function zapTo(channelId) {
	try {
		const done = await webuiV1Zap(channelId);
		if (done) {
			currentChannelId = channelId;
			document.querySelectorAll('.webui-epg-row').forEach(row => row.classList.toggle('playing', row.dataset.id === channelId));
		}
		return done;
	} catch (e) {
		webuiToastError('', e);
		return false;
	}
}

function changeTime(hours) {
	const select = document.getElementById('timeSelect');
	select.selectedIndex = Math.max(0, Math.min(select.options.length - 1, select.selectedIndex + hours));
	loadEpg();
}

document.getElementById('bouquetSelect').addEventListener('change', loadEpg);
document.getElementById('timeSelect').addEventListener('change', loadEpg);

// once the texts are in, which the grid is drawn with
webuiAfterTexts(function() {
	initTimeSelect();
	webuiV1Events({
		'epg-updated': epgUpdated,
		'timer-changed': timersChanged,
		zap: zapped
	});
	setInterval(tickNow, 60000);
	window.addEventListener('resize', () => placeNow(webuiBoxSec()));
	return loadBouquets().then(loadEpg);
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return loadEpg();
	});
}
