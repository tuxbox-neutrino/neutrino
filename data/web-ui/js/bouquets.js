// The script of bouquets.html.

// The channel list over /api/v1: bouquets, their channels and what each
// is showing now. Names, titles and texts come from the broadcast and go
// in as text (webui-dom.js); nothing here is built as markup.
let currentChannelId = null;	// the v1 id of the channel the box plays
let currentMode = 'tv';		// the list shown: the one the box plays from
let currentBouquetNumber = '';	// the v1 bouquet id, '' for all channels
let bouquetsData = [];		// the bouquets of that list, hidden ones left out
let recordedIds = new Set();	// the channels the box records now
let recordedStale = false;	// the last read of them failed: they are the ones recorded before

// What the box records comes from the page's shared read, which asks for a
// sign-in where the box refuses it, as the page's own reads do.
webuiRecordingsSignIn();

const SVG_INFO = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M12 16v-4M12 8h.01"/></svg>';
const SVG_REC = '<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><circle cx="12" cy="12" r="6"/></svg>';
const SVG_PLAY = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="2" y="4" width="20" height="14" rx="2"/><path d="M10 8.5v5l4.5-2.5z"/><path d="M8 21h8"/></svg>';
const SVG_STREAM = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" aria-hidden="true"><path d="M7 5v14l11-7z"/></svg>';

const clock = webuiClock;

// the channels of the list shown, by id, for what their rows open
let channelsShown = Object.create(null);

// A channel's programme in the shared dialog: the one that runs now
function showInfo(ch) {
	webuiShowEpgModal(ch.id, ch.name, {event: epgNow[ch.id], number: ch.number, kind: ch.kind || ''});
}

function rowOf(channelId) {
	return document.querySelector('.webui-channel-row[data-id="' + CSS.escape(String(channelId)) + '"]');
}

// An entry brought into sight by scrolling the boxes it lies in, its middle
// to theirs where center is set, else just inside their edges. Not by
// scrollIntoView: Chromium lets the next Tab start where that went, so the
// first Tab of the page landed in the list instead of the menu.
function scrollIntoSight(el, center) {
	for (let box = el.parentElement; box; box = box.parentElement) {
		if (box.scrollHeight <= box.clientHeight || !/auto|scroll/.test(getComputedStyle(box).overflowY))
			continue;
		const r = el.getBoundingClientRect();
		const top = box.getBoundingClientRect().top + box.clientTop, height = box.clientHeight;
		if (center)
			box.scrollTop += r.top + r.height / 2 - (top + height / 2);
		else if (r.top < top)
			box.scrollTop += r.top - top;
		else if (r.bottom > top + height)
			box.scrollTop += Math.min(r.bottom - (top + height), r.top - top);
	}
}

// The channel the box plays, and with it which list to show
async function loadCurrentChannel() {
	const ch = await webuiV1('GET', '/channels/current').catch(() => null);
	currentChannelId = ch ? ch.id : null;
	currentMode = ch && (ch.kind === 'radio' || ch.kind === 'webradio') ? 'radio' : 'tv';
}

// What the box records now, for the rows' state, from the page's shared
// read (webuiOnRecordings)
function recordingsSeen(view) {
	recordedIds = new Set(view.items.map(it => it.channel_id));
	recordedStale = view.known && view.stale;
	markCurrent();
}

// The bouquets of the list the box plays from that hold channels of it,
// hidden ones left out
function bouquetsOf(r) {
	return ((r && r.items) || []).filter(b =>
		!b.hidden && (currentMode === 'radio' ? b.radio_count : b.tv_count) > 0);
}

// The bouquet to show first: the one holding the channel the box plays,
// else the first; opts as webuiV1's
async function startBouquet(opts) {
	if (currentChannelId) {
		const holding = await webuiV1('GET', '/bouquets', Object.assign({query: {holds: currentChannelId}}, opts)).catch(() => null);
		const found = holding && (holding.items || []).find(x => bouquetsData.some(b => b.id === x.id));
		if (found) return String(found.id);
	}
	return bouquetsData[0] ? String(bouquetsData[0].id) : '';
}

// The bouquets of that list, and the one holding the current channel; a
// list that cannot be read says why where the bouquets would stand, and
// the channels of the whole list are shown all the same
let bouquetsShown = false;	// the bouquets were drawn (not loading, not failed)
async function loadBouquets() {
	const list = document.getElementById('bouquetList');
	try {
		const r = await webuiV1('GET', '/bouquets');
		bouquetsData = bouquetsOf(r);
		renderBouquetList(await startBouquet());
	} catch (e) {
		bouquetsData = [];
		bouquetsShown = false;
		list.replaceChildren(webuiEl('li', {}, webuiStateFor(e, {retry: loadBouquets})));
		renderBouquetPick('');
		currentBouquetNumber = '';
		loadChannels('');
	}
}

// The entries of both: All channels, then the bouquets
function bouquetEntries() {
	return [{id: '', name: webuiL('bq.all_channels', 'All channels')}].concat(bouquetsData.map(b => ({id: String(b.id), name: b.name})));
}

function bouquetButtons(list, entries, activeBouquet) {
	list.replaceChildren.apply(list, entries.map(b => webuiEl('li', {},
		webuiEl('button', {type: 'button', data: {bouquet: b.id}, attrs: {'aria-current': b.id === activeBouquet ? 'true' : 'false'},
			on: {click: () => selectBouquet(b.id)}}, b.name))));
	webuiArrowKeys(list, 'button');
}

// The bouquets as a list beside the channels and as a choice above them
// (the one a phone shows); both name the bouquet shown
function renderBouquetList(activeBouquet) {
	const list = document.getElementById('bouquetList');
	bouquetButtons(list, bouquetEntries(), activeBouquet);
	bouquetsShown = true;
	renderBouquetPick(activeBouquet);
	currentBouquetNumber = activeBouquet;
	const activeItem = list.querySelector('[aria-current="true"]');
	if (activeItem) scrollIntoSight(activeItem, false);
	loadChannels(activeBouquet);
}

function renderBouquetPick(activeBouquet) {
	const sel = document.getElementById('bouquetSelect');
	if (!sel) return;
	sel.replaceChildren.apply(sel, bouquetEntries().map(b => webuiEl('option', {value: b.id}, b.name)));
	sel.value = activeBouquet;
	sel.onchange = () => selectBouquet(sel.value);
}

/* The bouquets drawn again in place, where they changed: the list beside
   the channels and the phone's choice, the bouquet shown marked and named
   above the channels. A focus on a bouquet of the list goes back to the
   same bouquet, shown as before. */
function showBouquets(activeBouquet) {
	const list = document.getElementById('bouquetList');
	const entries = bouquetEntries();
	const want = entries.map(b => b.id + '\u0000' + b.name).join('\u0001');
	const had = Array.from(list.querySelectorAll('button[data-bouquet]'), b => (b.dataset.bouquet || '') + '\u0000' + b.textContent).join('\u0001');
	if (want !== had) {
		const at = document.activeElement;
		const focused = at && list.contains(at) && at.matches('button') ? at.textContent : null;
		let shown = true;
		try { shown = at.matches(':focus-visible'); } catch (e) { /* an old browser: shown */ }
		bouquetButtons(list, entries, activeBouquet);
		const again = focused === null ? null : Array.from(list.querySelectorAll('button')).find(b => b.textContent === focused);
		if (again) again.focus({preventScroll: true, focusVisible: shown});
		renderBouquetPick(activeBouquet);
	} else {
		list.querySelectorAll('button[data-bouquet]').forEach(b =>
			b.setAttribute('aria-current', (b.dataset.bouquet || '') === activeBouquet ? 'true' : 'false'));
		const sel = document.getElementById('bouquetSelect');
		if (sel && sel.value !== activeBouquet) sel.value = activeBouquet;
	}
	currentBouquetNumber = activeBouquet;
	const bq = bouquetsData.find(b => String(b.id) === activeBouquet);
	document.getElementById('currentBouquetName').textContent = activeBouquet === '' ? webuiL('bq.all_channels', 'All channels') :
		bq ? bq.name : webuiL('nav.sub_channels', 'Channels');
}

function selectBouquet(bouquet) {
	document.querySelectorAll('#bouquetList button').forEach(b => {
		b.setAttribute('aria-current', (b.dataset.bouquet || '') === bouquet ? 'true' : 'false');
	});
	const sel = document.getElementById('bouquetSelect');
	if (sel) sel.value = bouquet;
	currentBouquetNumber = bouquet;
	loadChannels(bouquet);
}

// One channel: its number, logo, name, what runs on it, its state, Zap
// and the menu with the other actions
function channelRow(ch) {
	// the logo while the box hands them out, else the kind of channel
	const logo = webuiChannelLogo(ch.id, currentMode === 'radio' ? webuiL('dash.radio', 'Radio') : webuiL('dash.tv', 'TV'));
	const zap = webuiEl('button', {type: 'button', className: 'webui-btn webui-channel-zap',
		title: webuiL('tip.zap_to', 'Switch the box to %s').replace('%s', ch.name),
		attrs: {'aria-label': webuiL('bq.zap_title', 'Switch channel') + ': ' + ch.name}, on: {click: () => zapTo(ch.id)}},
		webuiL('bq.zap_title', 'Switch channel'));
	// a click or a tap on the logo switches as Zap does and asks the same
	// questions; Zap stays the way by keyboard, the logo is no stop of its own
	logo.addEventListener('click', () => {
		if (ch.id !== currentChannelId) zapTo(ch.id);
	});
	// playing in the browser as the channel's coding allows, when the menu opens
	const menu = webuiMenuButton(webuiL('bq.more', 'More for %s').replace('%s', ch.name), () => {
		const doubt = webuiPlayDoubt(ch.id, ch.name);
		return [
			{label: webuiL('dash.epg_info', 'Programme info'), icon: SVG_INFO, onClick: () => showInfo(ch)},
			{label: webuiL('bq.rec_title', 'Record programme'), icon: SVG_REC, rec: true, onClick: () => webuiRecordEvent(ch.id, ch.name, epgNow[ch.id])},
			{label: webuiL('play.in_browser', 'Play in browser'), icon: SVG_PLAY,
				unavailable: doubt && doubt.sure ? doubt.text : '', note: doubt ? doubt.text : '',
				onClick: () => webuiPlayInBrowser(ch.id, ch.name, ch.kind)},
			{label: webuiL('play.external', 'Open in a player (M3U)'), icon: SVG_STREAM, suggested: !!doubt,
				onClick: () => webuiOpenInPlayer(ch.id)},
			{label: webuiL('play.copy', 'Copy address'), icon: WEBUI_COPY_SVG, onClick: () => webuiCopyChannelAddress(ch.id)}
		];
	});
	channelsShown[ch.id] = ch;
	// keyed for webuiSync, which lays a list read again over the rows
	return webuiEl('li', {className: 'webui-channel-row', data: {id: ch.id, webuiKey: ch.id}},
		webuiEl('span', {className: 'webui-channel-num'}, ch.number > 0 ? String(ch.number) : ''),
		logo,
		webuiEl('span', {className: 'webui-channel-name', title: ch.name}, ch.name),
		webuiEl('span', {className: 'webui-channel-now'}),
		webuiEl('span', {className: 'webui-channel-state'}),
		webuiEl('span', {className: 'webui-channel-acts'}, zap, menu));
}

// A row's actions: what runs now (its information), Zap, the menu
const CHANNEL_ACTS = '.webui-channel-now-open, .webui-channel-acts > button';
const CHANNEL_STEP = {ArrowDown: 1, ArrowUp: -1, ArrowRight: 1, ArrowLeft: -1};

function channelActs(row) {
	return Array.from(row.querySelectorAll(CHANNEL_ACTS)).filter(b => !b.hidden && b.getClientRects().length > 0);
}

function channelActKind(el) {
	return ['webui-channel-now-open', 'webui-menu-btn', 'webui-channel-zap'].find(c => el.classList.contains(c));
}

/* change() run on the rows with the focus on a row's action kept: a row
   drawn again takes the focus away (Zap hidden on the channel now shown,
   the programme drawn anew or gone), and the keys would reach no list.
   It goes back to the same action of that channel's row, else to the
   row's first, unless something else took it meanwhile (a question).
   With only, nothing is looked at unless the focus is in that row. */
function keepChannelFocus(change, only) {
	const at = document.activeElement;
	const row = at && at.matches && at.matches(CHANNEL_ACTS) ? at.closest('.webui-channel-row') : null;
	if (!row || (only && row !== only))
		return change();
	const id = row.dataset.id, kind = channelActKind(at);
	let shown = true;
	try { shown = at.matches(':focus-visible'); } catch (e) { /* an old browser: shown */ }
	const done = change();
	const now = document.activeElement;
	if (now !== at && now && now !== document.body)
		return done;
	if (now === at && at.isConnected && channelActs(row).indexOf(at) >= 0)
		return done;
	const again = rowOf(id);
	const acts = again ? channelActs(again) : [];
	const to = acts.find(b => b.classList.contains(kind)) || acts[0];
	if (to)
		to.focus({preventScroll: true, focusVisible: shown});
	return done;
}

/* The channel list as one stop of Tab (webuiArrowKeys, which also takes
   Home and End): Up and Down move from row to row, to the same action where
   that row has it (Zap is hidden on the channel shown), else to its first;
   Left and Right among the actions of one row. Enter does what a click on
   the action does. Taken before webuiArrowKeys, which would step through
   the actions of all rows in one line. */
function channelKeys(list) {
	if (!list.dataset.keys) {
		list.dataset.keys = '1';
		list.addEventListener('keydown', e => {
			if (e.defaultPrevented || e.altKey || e.ctrlKey || e.metaKey || e.shiftKey || !(e.key in CHANNEL_STEP))
				return;
			const at = document.activeElement;
			const row = at && at.matches(CHANNEL_ACTS) ? at.closest('.webui-channel-row') : null;
			if (!row)
				return;
			const d = CHANNEL_STEP[e.key];
			let to = null;
			if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
				const own = channelActs(row);
				to = own[(own.indexOf(at) + d + own.length) % own.length];
			} else {
				const rows = Array.from(list.querySelectorAll('.webui-channel-row'));
				const kind = channelActKind(at);
				const i = rows.indexOf(row), n = rows.length;
				for (let k = 1; k < n && !to; k++) {
					const acts = channelActs(rows[((i + d * k) % n + n) % n]);
					to = acts.find(b => b.classList.contains(kind)) || acts[0] || null;
				}
			}
			// no other row to go to (a bouquet of one channel): the focus
			// stays, and the key is not handed on to step along the row
			if (to && to !== at)
				to.focus();
			e.preventDefault();
		});
	}
	webuiArrowKeys(list, CHANNEL_ACTS);
}

let channelsToken = 0;
async function loadChannels(bouquet) {
	const token = ++channelsToken;
	const list = document.getElementById('channelList');
	const countEl = document.getElementById('channelCount');
	const titleEl = document.getElementById('currentBouquetName');
	list.replaceChildren(webuiEl('li', {}, webuiState('loading', webuiL('bq.loading_channels', 'Loading channels...'), '')));
	countEl.textContent = '';

	if (bouquet === '') {
		titleEl.textContent = webuiL('bq.all_channels', 'All channels');
	} else {
		const bq = bouquetsData.find(b => String(b.id) === String(bouquet));
		titleEl.textContent = bq ? bq.name : webuiL('nav.sub_channels', 'Channels');
	}

	try {
		const query = {mode: currentMode};
		if (bouquet) query.bouquet = bouquet;
		const channels = await webuiV1All('/channels', query);
		if (token !== channelsToken) return;	// another bouquet was chosen meanwhile

		countEl.textContent = channels.length + ' ' + webuiL('bq.channels_unit', 'channels');
		if (channels.length === 0) {
			list.replaceChildren(webuiEl('li', {}, webuiState('empty', webuiL('bq.no_channels', 'No channels found'), '')));
			return;
		}
		await webuiLogosOffered(channels[0].id);
		if (token !== channelsToken) return;

		channelsShown = Object.create(null);
		list.replaceChildren.apply(list, channels.map(channelRow));
		markCurrent();
		channelKeys(list);

		// the current channel in view, once the rows are laid out
		requestAnimationFrame(() => {
			const currentRow = list.querySelector('[aria-current="true"]');
			if (currentRow) scrollIntoSight(currentRow, true);
		});

		loadEpgInfo(channels, bouquet, token);
	} catch (e) {
		if (token !== channelsToken) return;
		list.replaceChildren(webuiEl('li', {}, webuiStateFor(e, {retry: () => loadChannels(bouquet)})));
	}
}

// What each channel shows now, from the guide grid: a page of 20 channels
// per request, for the bouquet or for the named channels of the whole list.
const epgNow = {};	// channel id -> the event running now
let progressUpdateInterval = null;

// Stops once another list was begun (token) or the bouquet shown got
// another number (it is read by the channels' ids then, rereadBouquets)
async function loadEpgInfo(channels, bouquet, token) {
	const now = webuiBoxSec();
	const pages = [];
	const stale = () => token !== channelsToken || (bouquet && bouquet !== currentBouquetNumber);
	if (bouquet) {
		pages.push({bouquet: bouquet});
	} else {
		for (let i = 0; i < channels.length; i += 20)
			pages.push({channels: channels.slice(i, i + 20).map(ch => ch.id).join(',')});
	}
	for (const base of pages) {
		let cursor = null;
		do {
			if (stale()) return;
			const query = Object.assign({from: now, to: now + 60, limit: 20}, base);
			if (cursor) query.cursor = cursor;
			let page;
			try {
				page = await webuiV1('GET', '/epg/grid', {query: query});
			} catch (e) {
				break;	// no guide for this page; the rows stay without one
			}
			if (stale()) return;
			((page && page.items) || []).forEach(row => showNow(row.channel_id, row.events || [], now));
			cursor = page && page.next_cursor;
		} while (cursor);
	}
	startProgressUpdateTimer();
}

// The programme a row shows: its title, from when to when, how far it has
// run; nothing where the guide knows none
function showNow(channelId, events, now) {
	const ev = events.find(e => e.start <= now && now < e.start + e.duration);
	const row = rowOf(channelId);
	if (!row) return;
	const cell = row.querySelector('.webui-channel-now');
	if (!ev) {
		delete epgNow[channelId];
		keepChannelFocus(() => cell.replaceChildren(), row);
		return;
	}
	epgNow[channelId] = ev;
	// the programme opens its information, as a click on it did before
	const ch = channelsShown[channelId];
	keepChannelFocus(() => cell.replaceChildren(webuiEl('button', {type: 'button', className: 'webui-channel-now-open',
		attrs: {title: webuiL('dash.epg_info', 'Programme info') + ': ' + (ev.title || '')},
		on: {click: () => ch && showInfo(ch)}},
		webuiEl('span', {}, ev.title || ''),
		webuiEl('small', {}, clock(ev.start) + ' – ' + clock(ev.start + ev.duration)),
		webuiEl('div', {className: 'webui-progress', attrs: {'aria-hidden': 'true'}}, webuiEl('span')))), row);
	updateChannelProgress(channelId);
}

function updateChannelProgress(channelId) {
	const ev = epgNow[channelId];
	const row = rowOf(channelId);
	if (!ev || !row) return;
	const bar = row.querySelector('.webui-channel-now .webui-progress > span');
	if (!bar) return;
	const now = webuiBoxNow() / 1000;
	bar.style.width = Math.min(100, Math.max(0, (now - ev.start) / ev.duration * 100)) + '%';
}

// The bars move on; a programme that is over makes way for the next,
// asked for the channels it concerns
function updateAllProgress() {
	const now = webuiBoxNow() / 1000;
	const over = [];
	for (const id of Object.keys(epgNow)) {
		if (now >= epgNow[id].start + epgNow[id].duration) {
			over.push(id);
			continue;
		}
		updateChannelProgress(id);
	}
	if (over.length) refreshNow(over);
}

function startProgressUpdateTimer() {
	if (!progressUpdateInterval)
		progressUpdateInterval = setInterval(updateAllProgress, 30000);
}

// What runs now on the named channels, asked again
async function refreshNow(ids) {
	const now = webuiBoxSec();
	for (let i = 0; i < ids.length; i += 20) {
		const page = await webuiV1('GET', '/epg/grid', {query: {from: now, to: now + 60, limit: 20,
			channels: ids.slice(i, i + 20).join(',')}}).catch(() => null);
		((page && page.items) || []).forEach(row => showNow(row.channel_id, row.events || [], now));
	}
}

// The row of the channel the box plays, and the ones it records
function markCurrent() {
	keepChannelFocus(() => document.querySelectorAll('.webui-channel-row').forEach(row => {
		const current = row.dataset.id === currentChannelId;
		const recorded = recordedIds.has(row.dataset.id);
		row.setAttribute('aria-current', current ? 'true' : 'false');
		const state = row.querySelector('.webui-channel-state');
		const want = recorded ? (recordedStale ? 'rec stale' : 'rec') : current ? 'ok' : '';
		if (state.dataset.state !== want) {
			// a recording known from before a failed read says so, as the
			// pill's tooltip and in words a screen reader reads with it
			const stale = recorded && recordedStale ? webuiL('rec.status_stale', 'Recording status could not be refreshed') : '';
			state.dataset.state = want;
			state.replaceChildren(want ? webuiEl('span', {className: 'webui-pill ' + want, title: stale || null},
				recorded ? webuiL('bc.recording', 'Recording') : webuiL('bq.live', 'On now'),
				stale ? webuiEl('span', {className: 'webui-visually-hidden'}, '. ' + stale) : null) : '');
		}
		row.querySelector('.webui-channel-zap').hidden = current;
		const logo = row.querySelector('.webui-channel-logo');
		logo.classList.toggle('zaps', !current);
		logo.title = current ? '' : row.querySelector('.webui-channel-zap').title;
	}));
}

// Zap: a file the player plays, or a box in standby, is asked about first
async function zapTo(channelId) {
	try {
		if (await webuiV1Zap(channelId)) {
			currentChannelId = channelId;
			markCurrent();
		}
	} catch (e) {
		webuiToastError('', e);
	}
}

// The rows with a guide that has news, asked again once the news settle
let epgSoon = null;
const epgNews = new Set();
function epgUpdated(d) {
	if (!d || !d.channel_id) return;
	// the guide names a channel in the lower 48 bits of its id
	const low = webuiLegacyChannelId(d.channel_id).slice(-12).toLowerCase();
	document.querySelectorAll('.webui-channel-row').forEach(row => {
		if (webuiLegacyChannelId(row.dataset.id).slice(-12).toLowerCase() === low) epgNews.add(row.dataset.id);
	});
	if (!epgNews.size || epgSoon) return;
	epgSoon = setTimeout(() => {
		epgSoon = null;
		const ids = Array.from(epgNews);
		epgNews.clear();
		refreshNow(ids);
	}, 2000);
}

// The channel the box plays, read again: its row is marked, and where the
// box switched between television and radio the lists change. A read that
// failed leaves the mark and the lists as they are: it says nothing of what
// the box plays. An answer that nothing plays (404 no-running-channel: the
// box is in standby, or the player shows a recording or a file) takes the
// mark off the last channel, so that its Zap is there again, and leaves the
// lists: they would otherwise jump to television.
let zapRead = 0;	// an older answer arriving after a newer one is dropped
async function zapped() {
	const read = ++zapRead;
	const mode = currentMode;
	let ch = null;
	try {
		ch = await webuiV1('GET', '/channels/current');
	} catch (e) {
		if (read === zapRead && e.code === 'no-running-channel') {
			currentChannelId = null;
			markCurrent();
		}
		return;
	}
	if (read !== zapRead || !ch || !ch.id) return;
	currentChannelId = ch.id;
	currentMode = ch.kind === 'radio' || ch.kind === 'webradio' ? 'radio' : 'tv';
	if (currentMode !== mode) loadBouquets();	// the box switched lists
	else markCurrent();
}

/* The bouquets changed: at the TV, in the bouquet editor or in another tab
   (bouquets-changed), or perhaps while the page had no stream (webuiFollow).
   Both lists are read again and follow in place. The bouquet shown stays
   shown, found again by its name, as a move or a delete before it changes
   its number; its rows stay, with what they show of the guide and the focus
   on them. One that is gone (deleted, hidden, emptied) makes way for the one
   holding the channel the box plays, as at the start. A read that fails
   leaves the lists as they are. One reading at a time: news meanwhile, or
   during the first load, make one more after it. */
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
	// none drawn, as the last read failed: read them whole
	if (!bouquetsShown) {
		await loadBouquets();
		return bouquetsShown;
	}
	const token = channelsToken, mode = currentMode, shown = currentBouquetNumber;
	const was = bouquetsData.find(b => String(b.id) === shown);
	let r;
	try {
		r = await webuiV1('GET', '/bouquets', {once: true});
	} catch (e) {
		return false;
	}
	// another bouquet chosen meanwhile, or the box switched lists: those reads
	// are newer, and the bouquets are read once more for them
	if (token !== channelsToken || mode !== currentMode) {
		bouquetsAgain = true;
		return true;
	}
	const names = new Set(bouquetsData.map(b => b.name));
	bouquetsData = bouquetsOf(r);
	let active = '';
	if (shown !== '') {
		const same = sameBouquet(bouquetsData, was, names);
		if (!same) {
			const start = await startBouquet({once: true});
			if (token === channelsToken && mode === currentMode) renderBouquetList(start);
			else bouquetsAgain = true;
			return true;
		}
		active = String(same.id);
	}
	showBouquets(active);
	return syncChannels(active, token, active !== shown);
}

/* The channels of the bouquet shown, read again and laid over its rows in
   place (webuiSync): a row that stays keeps its node, what it shows of the
   guide and the focus, its number put right; one whose name changed is
   drawn anew. The guide is asked for the rows that are new and, where the
   bouquet got another number (moved), for those that show no programme: a
   read by the old number may have stopped before them. A list that showed
   no rows (empty, or why it failed) is drawn whole. */
async function syncChannels(bouquet, token, moved) {
	const list = document.getElementById('channelList');
	let channels;
	try {
		const query = {mode: currentMode};
		if (bouquet) query.bouquet = bouquet;
		channels = await webuiV1All('/channels', query, {once: true});
	} catch (e) {
		return false;
	}
	if (token !== channelsToken) return true;
	if (!channels.length || !list.querySelector('.webui-channel-row')) {
		loadChannels(bouquet);
		return true;
	}
	const before = new Set(Array.from(list.querySelectorAll('.webui-channel-row'), row => row.dataset.id));
	const renamed = [];
	keepChannelFocus(() => {
		webuiSync(list, channels, ch => ch.id, channelRow, (row, ch) => {
			const num = row.querySelector('.webui-channel-num');
			const n = ch.number > 0 ? String(ch.number) : '';
			if (num.textContent !== n) num.textContent = n;
			if (row.querySelector('.webui-channel-name').textContent !== ch.name) renamed.push([row, ch]);
			else channelsShown[ch.id] = Object.assign(channelsShown[ch.id] || {}, ch);
		});
		renamed.forEach(([row, ch]) => {
			const fresh = channelRow(ch);
			fresh.dataset.webuiKey = row.dataset.webuiKey;
			row.replaceWith(fresh);
		});
	});
	const ids = new Set(channels.map(ch => ch.id));
	Object.keys(epgNow).forEach(id => { if (!ids.has(id)) delete epgNow[id]; });
	document.getElementById('channelCount').textContent = channels.length + ' ' + webuiL('bq.channels_unit', 'channels');
	markCurrent();
	const news = new Set(renamed.map(([, ch]) => ch.id));
	channels.forEach(ch => { if (!before.has(ch.id) || (moved && !epgNow[ch.id])) news.add(ch.id); });
	if (news.size) {
		refreshNow(Array.from(news));
		startProgressUpdateTimer();
	}
	return true;
}

// Initialize once the texts are in; a zap from anywhere else (remote
// control, timer) moves the mark, a recording that starts or stops marks
// its row (the shared read follows the record events), news of the guide
// updates the rows it concerns, and a change of the bouquets the lists
webuiAfterTexts(() => {
	webuiOnRecordings(recordingsSeen);
	webuiV1Events({zap: zapped, 'epg-updated': epgUpdated, 'bouquets-changed': bouquetsChanged});
	// a zap the stream did not carry (it was gone, or the page hidden) is
	// read once after the stream is back and, without one, when the page
	// comes into view (webuiFollow); the read below makes the first. The
	// bouquets likewise.
	webuiFollow({reread: zapped, first: false});
	webuiFollow({reread: bouquetsChanged, first: false});
	// until the first answers: both lists say they load
	const loading = () => webuiEl('li', {}, webuiState('loading', webuiL('msg.loading', 'Loading...'), ''));
	document.getElementById('bouquetList').replaceChildren(loading());
	document.getElementById('channelList').replaceChildren(webuiEl('li', {},
		webuiState('loading', webuiL('bq.loading_channels', 'Loading channels...'), '')));
	return loadCurrentChannel().then(loadBouquets).finally(() => {
		bouquetsLoading = false;
		if (bouquetsAgain) bouquetsChanged();
	});
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return loadCurrentChannel().then(() => { webuiRecordingsRead(); return loadBouquets(); });
	});
}
