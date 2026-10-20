// The script of epg.html.

// The programme grid over /api/v1: the bouquet's channels, then the
// guide window for 20 of them per request (GET /api/v1/epg/grid),
// filled in as the pages arrive. Titles and texts come from the
// broadcast and go in as text.
const MINUTES_PER_SLOT = 30;
const DISPLAY_HOURS = 4;
const TOTAL_MINUTES = DISPLAY_HOURS * 60;

let currentBouquet = '';
let currentMode = 'tv';
let currentStartTime = null;
let selectedProgram = null;	// {channelId, channelName, ev}
let timers = [];
let gridToken = 0;

const formatTime = webuiFormatTime;
function clock(sec) { return formatTime(new Date(sec * 1000)); }

function initTimeSelect() {
	const select = document.getElementById('timeSelect');
	const now = new Date();
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
	try {
		const ch = await webuiV1('GET', '/channels/current').catch(() => null);
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
		select.replaceChildren(webuiEl('option', {}, webuiL('epg.load_fail', 'Loading failed')));
		console.error('Failed to load bouquets:', e);
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

async function loadEpg() {
	const token = ++gridToken;
	const grid = document.getElementById('epgGrid');
	grid.replaceChildren(webuiEl('p', {attrs: {'aria-busy': 'true'},
		style: {padding: '3rem', 'text-align': 'center', color: 'var(--pico-muted-color)'}}, webuiL('epg.loading', 'Loading EPG data...')));

	currentBouquet = document.getElementById('bouquetSelect').value;
	currentStartTime = parseInt(document.getElementById('timeSelect').value);
	const startTime = currentStartTime;
	const stopTime = startTime + DISPLAY_HOURS * 3600;
	if (!currentBouquet) {
		grid.replaceChildren(webuiEl('p', {className: 'webui-empty'}, webuiL('bq.no_channels', 'No channels found')));
		return;
	}

	try {
		await loadTimers();
		const channels = await webuiV1All('/channels', {mode: currentMode, bouquet: currentBouquet});
		if (token !== gridToken) return;

		const now = Math.floor(Date.now() / 1000);
		const nowInWindow = now >= startTime && now < stopTime;
		const nowPercent = ((now - startTime) / 60) / TOTAL_MINUTES * 100;

		const slots = [];
		for (let i = 0; i < TOTAL_MINUTES / MINUTES_PER_SLOT; i++)
			slots.push(webuiEl('div', {className: 'webui-epg-time-slot'}, clock(startTime + i * MINUTES_PER_SLOT * 60)));
		if (nowInWindow)
			slots.push(webuiEl('div', {className: 'webui-epg-now-marker', style: {left: 'calc(100px + ' + nowPercent + '%)'}}));
		const header = webuiEl('div', {className: 'webui-epg-time-header'},
			webuiEl('div', {className: 'webui-epg-channel-header'}, webuiL('msg.channel', 'Channel')),
			webuiEl('div', {className: 'webui-epg-times'}, slots));

		const rows = webuiEl('div', {className: 'webui-epg-channels', style: {position: 'relative'}},
			nowInWindow ? webuiEl('div', {className: 'webui-epg-now-line', style: {left: 'calc(100px + ' + nowPercent + '%)'}}) : null);
		const programsOf = Object.create(null);
		channels.forEach(channel => {
			const programs = webuiEl('div', {className: 'webui-epg-programs'});
			programsOf[channel.id] = {channel: channel, el: programs};
			rows.appendChild(webuiEl('div', {className: 'webui-epg-row'},
				webuiEl('div', {className: 'webui-epg-channel-name', title: webuiL('epg.zap_to', 'Switch to') + ' ' + channel.name,
					on: {click: () => zapTo(channel.id)}},
					webuiEl('span', {}, channel.name)),
				programs));
		});
		grid.replaceChildren(header, rows);

		// the guide, a page of channels at a time
		let cursor = null;
		do {
			const query = {bouquet: currentBouquet, from: startTime, to: stopTime, limit: 20};
			if (cursor) query.cursor = cursor;
			const page = await webuiV1('GET', '/epg/grid', {query: query});
			if (token !== gridToken) return;
			((page && page.items) || []).forEach(row => {
				const target = programsOf[row.channel_id];
				if (target) fillRow(target.channel, target.el, row.events || [], startTime, stopTime, now);
			});
			cursor = page && page.next_cursor;
		} while (cursor);
	} catch (e) {
		if (token !== gridToken) return;
		grid.replaceChildren(webuiEl('p', {className: 'webui-empty'}, webuiL('epg.load_fail', 'Loading failed')));
		console.error('Failed to load EPG:', e);
	}
}

function fillRow(channel, el, events, startTime, stopTime, now) {
	const boxes = [];
	events.forEach(ev => {
		const evStop = ev.start + ev.duration;
		if (evStop <= startTime || ev.start >= stopTime) return;
		const displayStart = Math.max(ev.start, startTime);
		const displayStop = Math.min(evStop, stopTime);
		const leftPercent = ((displayStart - startTime) / 60) / TOTAL_MINUTES * 100;
		const widthPercent = ((displayStop - displayStart) / 60) / TOTAL_MINUTES * 100;
		const timer = timerFor(channel.id, ev);
		const isCurrent = now >= ev.start && now < evStop;
		const style = {left: leftPercent + '%', width: 'calc(' + widthPercent + '% - 2px)'};
		if (isCurrent) style['--progress'] = Math.round((now - ev.start) / ev.duration * 100) + '%';
		boxes.push(webuiEl('div', {className: 'webui-epg-program' +
			(timer ? (timer.kind === 'record' ? ' timer-rec' : ' timer-zap') : '') + (isCurrent ? ' current' : ''),
			style: style, on: {click: () => showProgramDetail(channel, ev)}},
			webuiEl('div', {className: 'webui-epg-program-title'}, ev.title || ''),
			webuiEl('div', {className: 'webui-epg-program-time'}, clock(ev.start))));
	});
	el.replaceChildren.apply(el, boxes);
}

// The detail dialog; the long text is asked for when it opens
async function showProgramDetail(channel, ev) {
	selectedProgram = {channelId: channel.id, channelName: channel.name, ev: ev};
	document.getElementById('modalTitle').textContent = ev.title || '-';
	document.getElementById('modalSubtitle').textContent = channel.name || '-';
	const minutes = Math.round(ev.duration / 60);
	document.getElementById('modalInfo').textContent =
		clock(ev.start) + ' - ' + clock(ev.start + ev.duration) + ' (' + minutes + ' Min.)';
	const descEl = document.getElementById('modalDesc');
	descEl.textContent = ev.description || '';
	document.getElementById('epgModal').classList.add('open');
	const shown = selectedProgram;
	const full = await webuiV1('GET', '/epg/event', {query: {id: ev.id, start: ev.start}}).catch(() => null);
	if (selectedProgram !== shown) return;
	const desc = full ? [full.description, full.long_description].filter(Boolean).join('\n\n') : (ev.description || '');
	descEl.textContent = desc || webuiL('epg.no_desc', 'No description available');
}

function closeModal() {
	document.getElementById('epgModal').classList.remove('open');
	selectedProgram = null;
}

// A recording of the event, through the helper the other pages share
async function setTimerRec() {
	if (!selectedProgram) return;
	if (await webuiRecordEvent(selectedProgram.channelId, selectedProgram.channelName, selectedProgram.ev)) {
		closeModal();
		loadEpg();
	}
}

// A zap timer at the event's start, or this minute for one running
async function setTimerZap() {
	if (!selectedProgram) return;
	const ev = selectedProgram.ev;
	try {
		await webuiV1('POST', '/timers', {body: {kind: 'zapto', channel_id: selectedProgram.channelId,
			start: Math.max(ev.start, Math.floor(Date.now() / 60000) * 60), epg_id: ev.id, epg_start: ev.start}});
		webuiShowToast(webuiL('epg.timer_zap_set', 'Zap timer set!'));
		closeModal();
		loadEpg();
	} catch (e) {
		webuiToastError(webuiL('epg.timer_fail', 'Setting the timer failed'), e);
	}
}

async function zapNow() {
	if (!selectedProgram) return;
	if (await zapTo(selectedProgram.channelId)) closeModal();
}

// Zap: a file the player plays, or a box in standby, is asked about first
async function zapTo(channelId) {
	try {
		return await webuiV1Zap(channelId);
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
document.getElementById('epgModal').addEventListener('click', function(e) {
	if (e.target === this) closeModal();
});

// once the texts are in, which the grid is drawn with
webuiAfterTexts(function() {
	initTimeSelect();
	return loadBouquets().then(loadEpg);
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return loadEpg();
	});
}
