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

// The head's number of set timers comes from this page's reads.
webuiTimersFollowed();

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

// The bouquets of the list the box plays from, hidden and empty ones left out
function bouquetsOf(r) {
	return ((r && r.items) || []).filter(b =>
		!b.hidden && (currentMode === 'radio' ? b.radio_count : b.tv_count) > 0);
}

// The bouquet to show first: the one holding the channel the box plays,
// else the first; opts as webuiV1's
async function startBouquet(bouquets, opts) {
	if (currentChannelId) {
		const holding = await webuiV1('GET', '/bouquets', Object.assign({query: {holds: currentChannelId}}, opts)).catch(() => null);
		const found = holding && (holding.items || []).find(x => bouquets.some(b => b.id === x.id));
		if (found) return String(found.id);
	}
	return bouquets.length ? String(bouquets[0].id) : '';
}

// The choice of bouquets, drawn anew only where it changed
function showBouquetChoice(bouquets, value) {
	const select = document.getElementById('bouquetSelect');
	const want = bouquets.map(b => b.id + '\u0000' + b.name).join('\u0001');
	const had = Array.from(select.options, o => o.value + '\u0000' + o.textContent).join('\u0001');
	if (want !== had)
		select.replaceChildren.apply(select, bouquets.map(b => webuiEl('option', {value: String(b.id)}, b.name)));
	select.value = value;
	currentBouquet = value;
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
		const bouquets = bouquetsOf(r);
		showBouquetChoice(bouquets, await startBouquet(bouquets));
	} catch (e) {
		bouquetError = e;
		select.replaceChildren(webuiEl('option', {value: ''}, webuiL('epg.load_fail', 'Loading failed')));
	}
}

let timersRead = 0;	// an older answer arriving after a newer one is dropped
async function loadTimers() {
	const read = ++timersRead;
	try {
		const r = await webuiV1('GET', '/timers');
		if (read !== timersRead) return;
		timers = (r && r.items) || [];
		webuiTimersSeen(timers);
	} catch (e) {
		if (read !== timersRead) return;
		webuiTimersSeen(null);
	}
}

// The active timers belonging to this programme, including manual timers.
function timerFor(channelId, ev) {
	return webuiTimersForEvent(timers, channelId, ev);
}

function nowPercentOf(start, now) {
	return ((now - start) / 60) / TOTAL_MINUTES * 100;
}

// A channel's row of the grid: its number and logo, a click on them zaps,
// and its programmes, filled in when its guide arrives
function gridRow(channel) {
	const programs = webuiEl('div', {className: 'webui-epg-programs'});
	gridRows[channel.id] = {channel: channel, el: programs, events: null};
	const zapLabel = webuiL('epg.zap_to', 'Switch to') + ' ' + channel.name;
	return webuiEl('div', {className: 'webui-epg-row' + (channel.id === currentChannelId ? ' playing' : ''), data: {id: channel.id}},
		webuiEl('div', {className: 'webui-epg-channel-name', title: zapLabel, tabIndex: 0,
			attrs: {role: 'button', 'aria-label': zapLabel},
			on: {click: () => zapTo(channel.id), keydown: ev => pressed(ev, () => zapTo(channel.id))}},
			// its number and its logo, its name where it has none
			channel.number > 0 ? webuiEl('span', {className: 'webui-epg-channel-num'}, String(channel.number)) : null,
			webuiChannelLogo(channel.id, channel.name)),
		programs);
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
	// without bouquets the timers are not read here: the head reads them
	if (bouquetError) {
		grid.replaceChildren(webuiStateFor(bouquetError, {retry: () => loadBouquets().then(loadEpg)}));
		webuiTimersSeen(null);
		return;
	}
	if (!currentBouquet) {
		grid.replaceChildren(webuiState('empty', webuiL('bq.no_channels', 'No channels found'), ''));
		webuiTimersSeen(null);
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

		// tabindex -1: Firefox would make the box that scrolls the guide a
		// stop of its own; a click into it and an arrow go to the stop. The
		// times are its first row, so they stay above the rows and move
		// sideways with them where the grid is wider than the screen.
		const rows = webuiEl('div', {className: 'webui-epg-channels', tabIndex: -1, style: {position: 'relative'}},
			header, webuiEl('div', {className: 'webui-epg-now-line', hidden: true}));
		channels.forEach(channel => rows.appendChild(gridRow(channel)));
		grid.style.setProperty('--epg-minutes', String(TOTAL_MINUTES));
		grid.style.setProperty('--epg-slot-minutes', String(MINUTES_PER_SLOT));
		grid.replaceChildren(rows);
		// the guide is one stop of Tab: the arrows go from channel to
		// channel and from programme to programme as they lie on the
		// screen, Enter opens one; Tab starts on what runs now on the
		// channel shown
		webuiArrowKeys(rows, '.webui-epg-channel-name, .webui-epg-program', '', {near: true,
			start: ['.webui-epg-row.playing .webui-epg-program.current', '.webui-epg-program.current']});
		drawn = true;
		gridWindow = {start: startTime, stop: stopTime};
		placeNow(now);
		startAtNow(rows, now);

		await fillGrid(token, currentBouquet, startTime, stopTime);
	} catch (e) {
		if (token !== gridToken) return;
		// rows already drawn stay; the grid as a whole says why it could not
		if (drawn)
			webuiToastError(webuiL('epg.load_fail', 'Loading failed'), e);
		else
			grid.replaceChildren(webuiStateFor(e, {retry: loadEpg}));
	}
}

// The guide for the rows of the grid shown, a page of channels at a time,
// each row drawn as its page arrives; a channel the guide gave no row for
// has nothing in the window. Stops quietly once another grid was begun
// (token), once the bouquet shown is not the one read (its number changed:
// it is read again under the new one) or, where it is given, once
// superseded() says that a newer read of the same rows was. A row drawn
// after it began (a channel the bouquet gained) is left to its own read.
async function fillGrid(token, bouquet, startTime, stopTime, superseded) {
	let cursor = null;
	const filled = new Set();
	const known = new Set(Object.values(gridRows));
	do {
		const query = {bouquet: bouquet, from: startTime, to: stopTime, limit: 20};
		if (cursor) query.cursor = cursor;
		const page = await webuiV1('GET', '/epg/grid', {query: query});
		if (token !== gridToken || bouquet !== currentBouquet || (superseded && superseded())) return;
		((page && page.items) || []).forEach(row => {
			const target = gridRows[row.channel_id];
			if (!target) return;
			target.events = row.events || [];
			fillRow(target);
			filled.add(row.channel_id);
		});
		cursor = page && page.next_cursor;
	} while (cursor);
	Object.keys(gridRows).forEach(id => {
		if (!filled.has(id) && known.has(gridRows[id])) {
			gridRows[id].events = [];
			fillRow(gridRows[id]);
		}
	});
}

// The guide of the grid shown, read again in place: the rows keep what
// they show until their page arrives, and a read that fails leaves them
// as they are. Only the newest of such reads draws: an older one, still on
// its way when a newer began, stops, so its pages cannot land last.
let gridReread = 0;
async function refreshGrid() {
	const win = gridWindow;
	if (!win) return;
	const read = ++gridReread;
	try {
		await fillGrid(gridToken, currentBouquet, win.start, win.stop, () => read !== gridReread);
	} catch (e) { /* the rows stay as they are */ }
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
// nothing for says so. A zap timer frames its programme in green, a
// recording scheduled in yellow; a recording running keeps the usual frame
// and says so by its mark.
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
		const status = webuiEventTimerMark(timer, true);
		const kind = webuiEventTimerMarkKind(timer);
		const name = [ev.title || '', clock(ev.start) + ' – ' + clock(evStop), channel.name, status ? status.textContent : ''].filter(Boolean).join(', ');
		// under ten minutes of the four hours shown: too narrow for its text
		const tiny = widthPercent < 10 / TOTAL_MINUTES * 100 ? ' tiny' : '';
		const framed = kind === 'zap' ? ' timer-marked timer-zap' : kind === 'record' ? ' timer-marked timer-record' : '';
		boxes.push(webuiEl('div', {className: 'webui-epg-program' + tiny + framed,
			tabIndex: 0, attrs: {role: 'button', 'aria-label': name}, data: {start: String(ev.start), stop: String(evStop)},
			style: {left: leftPercent + '%', width: 'calc(' + widthPercent + '% - 2px)'},
			on: {click: () => showProgramDetail(channel, ev), keydown: k => pressed(k, () => showProgramDetail(channel, ev))}},
			webuiEl('div', {className: 'webui-epg-program-title'}, ev.title || ''),
			webuiEl('div', {className: 'webui-epg-program-time'}, clock(ev.start)),
			webuiEl('div', {className: 'webui-epg-program-status'}, status || '')));
	});
	if (!boxes.length)
		boxes.push(webuiEl('div', {className: 'webui-epg-none'}, webuiL('epg.none', 'No programme data')));
	// cells drawn anew take the focus away: it goes to the cell of the same
	// programme, shown as before
	const at = document.activeElement;
	const kept = at && el.contains(at) && at.dataset ? at.dataset.start : undefined;
	let shown = true;
	if (kept !== undefined)
		try { shown = at.matches(':focus-visible'); } catch (e) { /* an old browser: shown */ }
	el.replaceChildren.apply(el, boxes);
	markNow(el, now);
	if (kept !== undefined) {
		const again = el.querySelector('.webui-epg-program[data-start="' + CSS.escape(kept) + '"]');
		if (again)
			again.focus({preventScroll: true, focusVisible: shown});
	}
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
   screen, by its share of the programme area. The line runs down all the
   rows, those scrolled into sight as well. */
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
	// measured without the line, which counts for the height scrolled over
	const line = document.querySelector('.webui-epg-now-line'), rows = line && line.parentElement;
	if (rows) {
		line.style.height = '';
		line.style.height = rows.scrollHeight + 'px';
	}
}

/* A grid wider than its box (on a phone, an hour to its width) starts at
   now, with a quarter of an hour before it, where the window holds now,
   else at its start */
function startAtNow(rows, now) {
	const win = gridWindow, programs = rows.querySelector('.webui-epg-programs');
	if (!win || !programs || rows.scrollWidth <= rows.clientWidth + 1)
		return;
	const into = now >= win.start && now < win.stop ? Math.max(0, (now - win.start) / 60 - 15) : 0;
	rows.scrollLeft = programs.clientWidth * into / TOTAL_MINUTES;
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

// The channel the box plays, after a zap from anywhere. A read that failed,
// or that names no channel (standby, a recording on the player, a zap under
// way), leaves the mark as it is.
let zapRead = 0;	// an older answer arriving after a newer one is dropped
async function zapped() {
	const read = ++zapRead;
	let ch = null;
	try {
		ch = await webuiV1('GET', '/channels/current');
	} catch (e) { /* kept */ }
	if (read !== zapRead || !ch || !ch.id) return;
	currentChannelId = ch.id;
	document.querySelectorAll('.webui-epg-row').forEach(row => row.classList.toggle('playing', row.dataset.id === currentChannelId));
}

// Everything the events of the stream keep current, read again once: the
// timer marks, the channel the box plays and the guide of the rows
function streamBack() {
	timersChanged();
	zapped();
	refreshGrid();
}

/* The bouquets changed: at the TV, in the bouquet editor or in another tab
   (bouquets-changed), or perhaps while the page had no stream (webuiFollow).
   The choice of bouquets is read again and the bouquet shown found again by
   its name, as a move or a delete before it changes its number; its channels
   are read again and laid over the grid's rows in place (syncGrid). A
   bouquet that is gone makes way for the one holding the channel the box
   plays, whose grid is drawn then. A read that fails leaves the choice and
   the grid as they are. One reading at a time: news meanwhile, or during the
   first load, make one more after it. */
let bouquetsBusy = null;
let bouquetsAgain = false;
let bouquetsLoading = true;	// the first load of the page is on its way
function bouquetsChanged() {
	if (bouquetsBusy || bouquetsLoading) {
		bouquetsAgain = true;
		return bouquetsBusy || undefined;
	}
	bouquetsBusy = (async () => {
		let ok = true;
		try {
			do {
				bouquetsAgain = false;
				ok = await rereadBouquets();
			} while (bouquetsAgain);
		} finally {
			bouquetsBusy = null;
		}
		return ok;
	})();
	return bouquetsBusy;
}

/* The bouquet shown before (was, {id, name}) among those read now: the same
   one at its place; else the one of its name, moved, or moved up as one
   before it went (of several of that name, the one nearest its place);
   else the one at its place under a name none had before, renamed. null
   when it is gone. names: the names before. */
function sameBouquet(bouquets, was, names) {
	if (!was)
		return null;
	const near = (a, b) => Math.abs(a.id - was.id) - Math.abs(b.id - was.id);
	return bouquets.find(b => String(b.id) === String(was.id) && b.name === was.name) ||
		bouquets.filter(b => b.name === was.name).sort(near)[0] ||
		bouquets.find(b => String(b.id) === String(was.id) && !names.has(b.name)) || null;
}

async function rereadBouquets() {
	// the last read of the bouquets failed, and the grid says so: all anew
	if (bouquetError) {
		await loadBouquets();
		await loadEpg();
		return !bouquetError;
	}
	const select = document.getElementById('bouquetSelect');
	const token = gridToken, mode = currentMode;
	const opt = select.selectedOptions && select.selectedOptions[0];
	const was = opt ? {id: opt.value, name: opt.textContent} : null;
	const names = new Set(Array.from(select.options, o => o.textContent));
	let r;
	try {
		r = await webuiV1('GET', '/bouquets', {once: true});
	} catch (e) {
		return false;
	}
	// another bouquet or time chosen meanwhile: that grid is the newer, and
	// the bouquets are read once more for it
	if (token !== gridToken || mode !== currentMode) {
		bouquetsAgain = true;
		return true;
	}
	const bouquets = bouquetsOf(r);
	const same = sameBouquet(bouquets, was, names);
	if (!same) {
		const start = await startBouquet(bouquets, {once: true});
		if (token !== gridToken || mode !== currentMode) {
			bouquetsAgain = true;
			return true;
		}
		showBouquetChoice(bouquets, start);
		loadEpg();
		return true;
	}
	const moved = String(same.id) !== was.id;
	showBouquetChoice(bouquets, String(same.id));
	const ok = await syncGrid(token);
	// under a new number: what was read under the old one (on the stream's
	// return, by a fill still on its way) is read again
	if (moved && token === gridToken)
		refreshGrid();
	return ok;
}

/* The channels of the bouquet shown, read again and laid over the grid's
   rows in place: a row that stays keeps its node, its guide and the focus
   on one of its cells, its number put right; rows of new channels, or of a
   channel whose name changed, are drawn and get their guide; the rows of
   channels gone leave. A grid that showed no rows (the bouquet was empty,
   or a read failed) is drawn whole. */
async function syncGrid(token) {
	let channels;
	try {
		channels = await webuiV1All('/channels', {mode: currentMode, bouquet: currentBouquet}, {once: true});
	} catch (e) {
		return false;
	}
	if (token !== gridToken) return true;
	const rows = document.querySelector('#epgGrid .webui-epg-channels');
	if (!rows || !gridWindow || !channels.length) {
		loadEpg();
		return true;
	}
	const at = document.activeElement;
	const atRow = at && rows.contains(at) ? at.closest('.webui-epg-row') : null;
	const atStart = atRow && at.dataset ? at.dataset.start : undefined;
	let shown = true;
	if (atRow)
		try { shown = at.matches(':focus-visible'); } catch (e) { /* an old browser: shown */ }
	const old = Object.create(null);
	rows.querySelectorAll(':scope > .webui-epg-row').forEach(row => { old[row.dataset.id] = row; });
	const ids = new Set(channels.map(ch => ch.id));
	const news = [];
	const order = channels.map(ch => {
		const row = old[ch.id], kept = gridRows[ch.id];
		if (row && kept && kept.channel.name === ch.name) {
			delete old[ch.id];
			// the same object, which the row's cells hand to the dialog
			Object.assign(kept.channel, ch);
			const num = row.querySelector('.webui-epg-channel-num');
			const n = ch.number > 0 ? String(ch.number) : '';
			if (num && num.textContent !== n) num.textContent = n;
			return row;
		}
		news.push(ch.id);
		return gridRow(ch);
	});
	Object.keys(old).forEach(id => {
		old[id].remove();
		if (!ids.has(id)) delete gridRows[id];
	});
	const live = rows.getElementsByClassName('webui-epg-row');
	order.forEach((row, i) => {
		if (live[i] !== row) rows.insertBefore(row, live[i] || null);
	});
	// a row moved takes the focus with it: back on the cell of the same
	// programme, or on the channel, shown as before
	if (atRow && document.activeElement !== at) {
		const again = rows.querySelector('.webui-epg-row[data-id="' + CSS.escape(atRow.dataset.id) + '"]');
		const cell = !again ? null : atStart !== undefined ?
			again.querySelector('.webui-epg-program[data-start="' + CSS.escape(atStart) + '"]') : again.querySelector('.webui-epg-channel-name');
		if (cell) cell.focus({preventScroll: true, focusVisible: shown});
	}
	placeNow(webuiBoxSec());
	if (news.length) {
		news.forEach(id => newsFor.add(id));
		clearTimeout(newsSoon);
		refreshRows();
	}
	return true;
}

// A programme in the shared dialog, the marks of the grid following a
// timer set from it. The dialog gives the focus back to the cell it was
// opened from; rows drawn anew while it was open (a timer set from it, news
// of the guide, a return to the tab) took that cell away, so the cell of the
// same programme takes the focus then.
let programOpener = null;	// {channel, start} of the cell the dialog was opened from
let programWatch = null;
function showProgramDetail(channel, ev) {
	programOpener = {channel: channel.id, start: String(ev.start)};
	webuiShowEpgModal(channel.id, channel.name, {event: ev, number: channel.number, kind: channel.kind || '',
		onTimer: () => { if (!webuiV1EventsLive()) timersChanged(); }});
	const modal = document.getElementById('webuiEpgModal');
	if (!modal || programWatch)
		return;
	programWatch = new MutationObserver(() => {
		if (modal.classList.contains('open') || !programOpener)
			return;
		const from = programOpener;
		programOpener = null;
		// the dialog gave it back where it could; what is left is still in the hidden dialog, or nowhere
		const at = document.activeElement;
		if (at && at !== document.body && !modal.contains(at))
			return;
		const row = gridRows[from.channel];
		const cell = row && row.el.querySelector('.webui-epg-program[data-start="' + CSS.escape(from.start) + '"]');
		if (cell)
			cell.focus({preventScroll: true});
	});
	programWatch.observe(modal, {attributes: true, attributeFilter: ['class']});
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
		zap: zapped,
		'bouquets-changed': bouquetsChanged
	});
	// what a gap of the stream may have missed, read again once the stream
	// is back and, without one, when the page comes into view (webuiFollow);
	// the guide's first read below brings all of it
	webuiFollow({reread: streamBack, first: false});
	webuiFollow({reread: bouquetsChanged, first: false});
	setInterval(tickNow, 60000);
	window.addEventListener('resize', () => placeNow(webuiBoxSec()));
	// until the bouquets are in, the guide says it loads (loadEpg does the same)
	document.getElementById('epgGrid').replaceChildren(webuiState('loading', webuiL('epg.loading', 'Loading EPG data...'), ''));
	return loadBouquets().then(loadEpg).finally(() => {
		bouquetsLoading = false;
		if (bouquetsAgain) bouquetsChanged();
	});
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return loadEpg();
	});
}
