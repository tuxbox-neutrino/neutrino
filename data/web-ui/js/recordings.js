// The script of recordings.html.

// The finished recordings over /api/v1/recordings/archive: a page at a
// time, by folder, title words, whether they have a thumbnail, and order;
// as tiles with their cover or their channel's logo, or as rows of a list;
// the one chosen with its details and actions in a column beside them on a
// wide screen, in a sheet on a narrower one. Play on TV turns into Stop while
// the TV plays the recording, on a box that has the route to stop it; while
// the TV plays a recording or a file, Play on TV of every other recording
// waits and says why. Titles and texts come from the recording's metadata and
// go in as text.
const PAGE = 30;
const SVG_FILM = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M7 4v16M17 4v16M2 9h5M2 15h5M17 9h5M17 15h5"/></svg>';
const SVG_PLAY = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polygon points="5,3 19,12 5,21"/></svg>';
const SVG_PLAY_FILLED = '<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><polygon points="6 4 20 12 6 20"/></svg>';
const SVG_STOP = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><rect x="5" y="5" width="14" height="14" rx="1.5"/></svg>';
const SVG_STOP_FILLED = '<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><rect x="6" y="6" width="12" height="12" rx="1.5"/></svg>';
const SVG_INFO = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>';
const SVG_LIST = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/></svg>';
const SVG_DOWN = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3v12M7 10l5 5 5-5M4 21h16"/></svg>';
const SVG_DELETE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polyline points="3,6 5,6 21,6"/><path d="M19,6v14a2,2 0 0,1-2,2H7a2,2 0 0,1-2-2V6m3,0V4a2,2 0 0,1,2-2h4a2,2 0 0,1,2,2v2"/></svg>';

let source = '';		// the library directory shown, '' for all
let nextOffset = 0;		// where the next page begins, null after the last
let listToken = 0;		// a newer read makes an older answer stale
let partialRetry = null;
let partialTries = 0;		// a directory still read the first time is asked about a few times, not for ever
let pagesShown = 0;		// pages on screen, so a reread does not throw away what "more" loaded
let sourcesShown = null;	// nothing drawn yet: an empty list is drawn too
let shown = [];			// the recordings on screen, drawn again when the view changes
let chosen = null;		// the id of the recording in the column beside the list
let detailToken = 0;		// a newer choice makes an older answer stale
let view = 'tiles';		// tiles or list, as this browser last left it
let playingId = null;		// the recording the TV plays, from the list, the details and the playback events
let tvPlays = null;		// what the TV's player plays, a recording or a file ({source, id, title}), else null
let playbackSeen = 0;		// counts reads and events of the playback, so an older read does not undo a newer event
let stopping = null;		// {id, timer} from a press of Stop until the playback ends
// how long a stop the box took on waits for the playback event before the
// state is read once
const STOP_WAIT = 8000;
try {
	if (localStorage.getItem('webui-media-view') === 'list') view = 'list';
} catch (e) { /* tiles then */ }

function archive(id) {
	return '/recordings/archive/' + encodeURIComponent(id);
}

function when(sec) {
	const d = new Date(sec * 1000);
	return webuiPad2(d.getDate()) + '.' + webuiPad2(d.getMonth() + 1) + '.' + d.getFullYear() + ' ' + webuiFormatTime(d);
}

function length(sec) {
	if (!sec) return '';
	const h = Math.floor(sec / 3600), m = Math.floor(sec / 60) % 60;
	return h > 0 ? h + ':' + webuiPad2(m) + ' h' : m + ' min';
}

function bytes(n) {
	return n ? webuiFormatKB(Math.round(n / 1024)) : '';
}

// whether the column beside the list is there (a wide screen)
function besideShown() {
	const d = document.getElementById('mediaDetail');
	return !!d && getComputedStyle(d).display !== 'none';
}

// The control of a recording in the list that has the focus, as
// {id, cls}, so the same control of the same recording takes it again once
// the list is drawn anew
const FOCUS_KEPT = ['webui-media-play', 'webui-media-open', 'webui-menu-btn'];
function focusInList(list) {
	const at = document.activeElement;
	const li = at && at !== list && list.contains(at) ? at.closest('li[data-id]') : null;
	const cls = li ? FOCUS_KEPT.find(c => at.classList.contains(c)) : null;
	return cls ? {id: li.dataset.id, cls: cls} : null;
}

function focusAgain(list, kept) {
	if (!kept || document.activeElement !== document.body) return;
	const li = Array.prototype.find.call(list.children, e => e.dataset.id === kept.id);
	const el = li && li.querySelector('.' + kept.cls);
	if (el) el.focus({preventScroll: true});
}

/* One page of the list; more=true appends the next page. quiet, for a read
   the page makes of itself (the player moved, a recording ended), keeps the
   list shown until the answer comes and the focus where it was. */
async function loadList(more, quiet) {
	const token = ++listToken;
	const list = document.getElementById('mediaList');
	const moreBtn = document.getElementById('mediaMore');
	if (!more) {
		nextOffset = 0;
		if (!quiet || !shown.length)
			list.replaceChildren(webuiEl('li', {}, webuiState('loading', webuiL('media.loading', 'Loading recordings...'), '')));
	}
	if (nextOffset === null) return;

	const query = {offset: nextOffset, limit: PAGE, sort: document.getElementById('mediaSort').value};
	const order = document.getElementById('mediaOrder').value;
	if (order) query.order = order;
	const words = document.getElementById('mediaSearch').value.trim();
	if (words) query.title = words;
	if (source) query.source = source;
	// with or without a thumbnail: filtered by the box, so "more" goes on
	// with the same filter
	const cover = document.getElementById('mediaCover').value;
	if (cover) query.cover = cover;

	let page;
	try {
		page = await webuiV1('GET', '/recordings/archive', {query: query});
	} catch (e) {
		if (token !== listToken) return;
		// "more" that fails keeps the pages shown and says so; a first page
		// says why the list is empty, and the folders still offer "all"
		if (more) {
			webuiToastError(webuiL('msg.load_fail', 'Loading failed'), e);
			return;
		}
		list.replaceChildren(webuiEl('li', {}, webuiStateFor(e, {retry: () => loadList(false)})));
		moreBtn.hidden = true;
		shown = [];
		if (sourcesShown === null) showSources([]);
		return;
	}
	if (token !== listToken) return;

	const items = (page && page.items) || [];
	pagesShown = more ? pagesShown + 1 : 1;
	sawPlaying(items);
	const kept = more ? null : focusInList(list);
	if (!more) {
		list.replaceChildren();
		shown = [];
	}
	items.forEach(rec => {
		shown.push(rec);
		list.appendChild(itemFor(rec));
	});
	focusAgain(list, kept);
	if (!more && items.length === 0) {
		// the filter on the thumbnail named, as it is the likeliest reason
		const none = cover === 'true' ? webuiL('media.none_with_cover', 'No recordings with a thumbnail') :
			cover === 'false' ? webuiL('media.none_without_cover', 'No recordings without a thumbnail') :
			webuiL('media.none', 'No recordings found');
		const empty = webuiEl('li', {}, webuiState('empty', none,
			words ? webuiL('media.none_search', 'Nothing matches these words.') : ''));
		list.appendChild(empty);
		// no recordings at all: a folder that is not there says why
		if (!words && !source && !cover)
			webuiMissingRecordFolders().then(r => {
				if (!r.missing.length || token !== listToken || !empty.isConnected) return;
				const why = r.partial ? webuiL('media.folder_slow', 'No answer in time, the drive or network share hangs or is not attached: %s') :
					webuiL('media.folder_missing', 'Not there now, the drive or network share not attached: %s');
				empty.replaceChildren(webuiState('empty', webuiL('media.none', 'No recordings found'), why.replace('%s', r.missing.join(', '))));
			});
	}

	nextOffset = (page && typeof page.next_offset === 'number') ? page.next_offset : null;
	moreBtn.hidden = nextOffset === null;
	const total = (page && page.total) || 0;
	document.getElementById('mediaCount').textContent = (total === 1 ? webuiL('media.count_one', '%s recording') :
		webuiL('media.count', '%s recordings')).replace('%s', total);
	showSources((page && page.sources) || []);

	// the one chosen stays while the list has it; else the column says to choose
	if (!more && (chosen === null || !shown.some(r => r.id === chosen))) {
		chosen = null;
		showBeside(null);
	}

	// a directory still being read the first time: asked again a few
	// times, further apart each time, while only the first page is shown
	document.getElementById('mediaPartial').hidden = !(page && page.partial);
	if (!(page && page.partial)) partialTries = 0;
	else if (!partialRetry && partialTries < 5 && pagesShown === 1) {
		partialRetry = setTimeout(() => {
			partialRetry = null;
			if (pagesShown === 1) loadList(false);	// "more" was pressed meanwhile: keep its pages
		}, 3000 * ++partialTries);
	}
}

// The player moved: only the first page is read again, and not while
// somebody has paged further down
let playbackSoon = null;
function playbackChanged() {
	if (pagesShown > 1 || playbackSoon) return;
	playbackSoon = setTimeout(() => { playbackSoon = null; loadList(false, true); }, 1000);
}

function titleOf(rec) {
	return rec.title || rec.channel || rec.id;
}

/* What plays, as an answer says it: the recording an item of a list or the
   details marks playing; an item that says it does not ends the mark it
   held, and a stop that waits for that. */
function sawPlaying(items) {
	const playing = items.find(r => r.playing);
	if (playing) {
		playingId = playing.id;
		tvPlays = {source: 'recording', id: playing.id, title: titleOf(playing)};
	} else if (items.some(r => r.id === playingId)) {
		playingId = null;
		if (tvPlays && tvPlays.source === 'recording') tvPlays = null;
	}
	// a recording that started from elsewhere is named once an answer has it
	const named = tvPlays && tvPlays.source === 'recording' && !tvPlays.title ? items.find(r => r.id === tvPlays.id) : null;
	if (named) tvPlays.title = titleOf(named);
	if (stopping && stopping.id !== playingId)
		stopDone(stopping);
	syncPlay();
}

/* What the TV's player plays, as GET /api/v1/playback says it: read as the
   page opens, after the event stream was away (it may have missed an end)
   and where a stop waits too long for its event. A box that does not answer
   leaves what is known; resolves whether it answered. */
async function readPlayback() {
	const read = ++playbackSeen;
	const now = await _webuiV1Read('/playback').catch(() => null);
	if (!now || read !== playbackSeen) return false;
	tvPlays = webuiPlaybackOf(now);
	playingId = tvPlays && tvPlays.source === 'recording' ? tvPlays.id : null;
	if (stopping && stopping.id !== playingId)
		stopDone(stopping);
	syncPlay();
	return true;
}

/* The playback event names its state, what plays and, for a recording, its
   id ("playing recording <id>", "stopped recording <id>", "playing file"):
   the buttons follow it at once, and a stop waiting for it is done once the
   recording it stopped no longer plays. A recording the page does not show
   is named by one read of what plays. */
function playbackEvent(data) {
	const ev = webuiPlaybackEvent(data);
	if (ev) {
		playbackSeen++;
		if (ev.stopped) {
			if (!ev.id || ev.id === playingId) playingId = null;
			if (!ev.id || !tvPlays || tvPlays.source !== 'recording' || tvPlays.id === ev.id) tvPlays = null;
		} else if (ev.source === 'recording' && ev.id) {
			playingId = ev.id;
			if (!tvPlays || tvPlays.id !== ev.id) {
				const rec = shown.find(r => r.id === ev.id);
				tvPlays = {source: 'recording', id: ev.id, title: rec ? titleOf(rec) : ''};
				if (!rec) readPlayback();
			}
		} else if (ev.source === 'file') {
			playingId = null;
			if (!tvPlays || tvPlays.source !== 'file') tvPlays = {source: 'file', id: '', title: ''};
		} else {
			// live television or its timeshift: the player plays no file
			playingId = null;
			tvPlays = null;
		}
		if (stopping && stopping.id !== playingId)
			stopDone(stopping);
		syncPlay();
	}
	playbackChanged();
}

/* A recording's picture: its cover, else its channel's logo on the dark
   tile, else the film symbol. Only the picture is hidden from a screen
   reader; what stands on it (length, playing) is read. */
function picture(rec) {
	const box = webuiEl('div', {className: 'webui-media-thumb'});
	const fallback = function() {
		box.replaceChildren(rec.channel_id ? webuiChannelLogo(rec.channel_id, (rec.channel || '').slice(0, 12), 'webui-media-logo')
			: webuiMarkup(SVG_FILM));
		box.classList.add('no-cover');
		if (box._webuiOver) box.append.apply(box, box._webuiOver);
	};
	// a recording the box says has no cover is not asked for one
	if (rec.cover === false) {
		fallback();
		return box;
	}
	box.appendChild(webuiEl('img', {attrs: {src: '/api/v1' + archive(rec.id) + '/cover', alt: '', loading: 'lazy'},
		on: {error: fallback}}));
	return box;
}

// what stands on a picture stays when its cover falls back to the logo
function onPicture(box, parts) {
	box._webuiOver = parts.filter(Boolean);
	box.append.apply(box, box._webuiOver);
	return box;
}

// A click or a tap on a recording's picture in the list opens its details,
// as its title does; a button on the picture keeps its own action. The
// title stays the way there by keyboard.
function opensDetails(box, rec) {
	box.classList.add('opens');
	box.addEventListener('click', e => {
		if (!e.target.closest('button')) choose(rec);
	});
	return box;
}

function playingPill() {
	return webuiEl('span', {className: 'webui-pill ok'}, webuiL('media.playing', 'Playing'));
}

function itemFor(rec) {
	return view === 'list' ? recordingRow(rec) : recordingTile(rec);
}

// The title opens the recording's details: a button that reads as one
function openButton(rec, title, meta, pill) {
	return webuiEl('button', {type: 'button', className: 'webui-media-open', attrs: {'aria-label': webuiL('tm.details', 'Details') + ': ' + title},
		on: {click: () => choose(rec)}},
		webuiEl('span', {className: 'webui-media-title'}, title, pill ? playingPill() : null),
		webuiEl('span', {className: 'webui-media-meta'}, meta));
}

// One recording as a tile: its picture with the length, whether it plays
// and Play on TV on it; its title and what it is below
function recordingTile(rec) {
	const title = titleOf(rec);
	const meta = [rec.channel, when(rec.start), bytes(rec.size)].filter(Boolean).join(' · ');
	const pic = onPicture(picture(rec), [
		rec.playing ? playingPill() : null,
		rec.duration ? webuiEl('span', {className: 'webui-media-len'}, length(rec.duration)) : null,
		playButton(rec.id, title, 'tile', null)]);
	return webuiEl('li', {className: 'webui-media-tile' + (rec.id === chosen ? ' chosen' : ''), data: {id: rec.id},
		attrs: rec.id === chosen ? {'aria-current': 'true'} : {}},
		opensDetails(pic, rec), openButton(rec, title, meta, false));
}

// One recording as a row: its picture, its title and what it is, Play on
// TV and the menu with the others
function recordingRow(rec) {
	const title = titleOf(rec);
	const meta = [rec.channel, when(rec.start), length(rec.duration), bytes(rec.size)].filter(Boolean).join(' · ');
	const base = '/api/v1' + archive(rec.id);
	return webuiEl('li', {className: 'webui-media-row' + (rec.id === chosen ? ' chosen' : ''), data: {id: rec.id},
		attrs: rec.id === chosen ? {'aria-current': 'true'} : {}},
		opensDetails(onPicture(picture(rec), []), rec),
		webuiEl('div', {className: 'webui-media-info'}, openButton(rec, title, meta, rec.playing)),
		webuiEl('div', {className: 'webui-media-actions'},
			playButton(rec.id, title, 'row', null),
			webuiMenuButton(webuiL('bq.more', 'More for %s').replace('%s', title), [
				{label: webuiL('tm.details', 'Details'), icon: SVG_INFO, onClick: () => choose(rec)},
				{label: webuiL('media.playlist', 'Playlist (M3U)'), icon: SVG_LIST, onClick: () => fetchTo(base + '/playlist.m3u')},
				{label: webuiL('media.download', 'Download'), icon: SVG_DOWN, onClick: () => fetchTo(base + '/file')},
				null,
				{label: webuiL('media.delete', 'Delete'), icon: SVG_DELETE, danger: true, onClick: () => remove(rec)}])));
}

/* Play on TV, or Stop while the TV plays the recording: one button that
   changes in place, so it keeps the focus. kind 'tile' is the symbol on the
   picture, 'row' the symbol and the word of a row, 'act' the details'
   action; title names the recording to a screen reader in the list. leave()
   runs before Play, which takes the details' sheet away. */
function playButton(id, title, kind, leave) {
	const cls = kind === 'tile' ? 'webui-media-tile-play webui-media-play' :
		kind === 'row' ? 'webui-btn webui-media-play' : 'webui-btn primary webui-media-act-play';
	const btn = webuiEl('button', {type: 'button', className: cls, data: {play: id, kind: kind, title: title},
		on: {click: () => playOrStop(id, leave)}});
	showPlay(btn);
	return btn;
}

function showPlay(btn) {
	const id = btn.dataset.play, kind = btn.dataset.kind;
	// Stop only where the box has the route; until its description is read,
	// as it most likely has
	const can = webuiV1OffersNow('POST', '/playback/stop');
	if (tvPlays && can === null)
		askStopRoute();
	const stops = id === playingId && can !== false;
	const word = stops ? webuiL('media.stop', 'Stop') : webuiL('media.play', 'Play on TV');
	const named = stops ? webuiL('media.stop_on_tv', 'Stop on TV') : webuiL('media.play', 'Play on TV');
	if (btn.classList.contains('stops') !== stops || !btn.firstChild) {
		btn.classList.toggle('stops', stops);
		if (kind === 'tile')
			btn.replaceChildren(webuiMarkup(stops ? SVG_STOP_FILLED : SVG_PLAY_FILLED));
		else
			btn.replaceChildren(webuiMarkup(stops ? SVG_STOP : SVG_PLAY), webuiEl('span', {}, word));
	}
	if (kind !== 'act') btn.setAttribute('aria-label', named + ': ' + btn.dataset.title);
	// while the TV plays something else, Play waits and says why
	webuiPlayLock(btn, lockFor(id), kind === 'tile' ? named : '');
	// busy from the press until the box has ended the playback, without a
	// second press meanwhile; the mark takes the colour and the position the
	// button has in the page, so one built while a stop waits is marked once
	// it is in
	const busy = !!stopping && stopping.id === id;
	if (busy && !btn.isConnected) {
		queueMicrotask(() => { if (btn.isConnected) showPlay(btn); });
		return;
	}
	if (busy !== !!btn._stopMarked) {
		btn._stopMarked = busy;
		_webuiWaitMark(btn, busy);
	}
}

// Why Play on TV of a recording waits, '' where it does not: the TV plays
// a file or another recording. On a box without the route to stop it the
// box's own question about stopping stays the way.
function lockFor(id) {
	if (!tvPlays || (tvPlays.source === 'recording' && tvPlays.id === id))
		return '';
	return webuiV1OffersNow('POST', '/playback/stop') === false ? '' : webuiPlayLockText(tvPlays);
}

// every Play and Stop of the page as the playback stands now
function syncPlay() {
	document.querySelectorAll('[data-play]').forEach(showPlay);
}

/* Whether the box has the route Stop takes, asked once a Stop would be
   drawn; the buttons are drawn again once that is known. A description that
   did not come is asked for again at the next drawing, not at once. */
let stopRouteAsk = null;
function askStopRoute() {
	if (stopRouteAsk) return;
	stopRouteAsk = webuiV1Offers('POST', '/playback/stop').then(() => {
		stopRouteAsk = null;
		if (webuiV1OffersNow('POST', '/playback/stop') !== null) syncPlay();
	});
}

let lockRead = null;		// the read a press of a waiting Play makes; a press meanwhile does nothing
async function playOrStop(id, leave) {
	if (stopping && stopping.id === id) return;
	if (id === playingId && webuiV1OffersNow('POST', '/playback/stop') !== false) {
		stop(id);
		return;
	}
	// a press of a Play that waits reads what plays first, as an end may
	// have gone by without its event; still waiting, it says why and starts
	// nothing
	if (lockFor(id)) {
		if (lockRead) return;
		lockRead = readPlayback();
		await lockRead;
		lockRead = null;
		const lock = lockFor(id);
		if (lock) {
			webuiShowToast(lock);
			return;
		}
	}
	if (leave) leave();
	play(id);
}

/* Stops the recording on the TV (POST /api/v1/playback/stop with its id).
   202 says the box took it on: the button stays busy until the playback
   event says the recording ended, and where none comes in time, what plays
   is read once. 204 says nothing played it: Play again. A request the box
   did not take leaves Stop to press again. A box that has no such route
   (404, 405) gets Play back, without a word. */
async function stop(id) {
	const mine = stopping = {id: id, timer: 0};
	syncPlay();
	let took;
	try {
		took = await webuiV1('POST', '/playback/stop', {body: {id: id}, accepted: true});
	} catch (e) {
		if (e.status === 404 || e.status === 405) {
			webuiV1Gone('POST', '/playback/stop');
			stopDone(mine);
			return;
		}
		stopDone(mine);
		if (e.code === 'command-not-posted')
			webuiShowToast(webuiL('media.stop_not_taken', 'The box did not respond. Try again.'), true);
		else
			webuiToastError('', e);
		return;
	}
	if (stopping !== mine) return;
	if (!took) {
		// nothing played it any longer: what the TV shows now is read once
		if (playingId === id) playingId = null;
		if (tvPlays && tvPlays.source === 'recording' && tvPlays.id === id) tvPlays = null;
		stopDone(mine);
		readPlayback();
		return;
	}
	mine.timer = setTimeout(() => stopLate(mine), STOP_WAIT);
}

function stopDone(mine) {
	if (stopping !== mine) return;
	clearTimeout(mine.timer);
	stopping = null;
	syncPlay();
}

// No playback event in time: what the TV shows, read once
async function stopLate(mine) {
	if (stopping !== mine) return;
	await readPlayback();
	stopDone(mine);
}

function fetchTo(href) {
	const a = webuiEl('a', {attrs: {href: href, download: ''}});
	document.body.appendChild(a);
	a.click();
	a.remove();
}

// tiles or rows, kept for this browser; the recordings shown drawn again
function setView(v) {
	view = v;
	try {
		if (v === 'list') localStorage.setItem('webui-media-view', 'list');
		else localStorage.removeItem('webui-media-view');
	} catch (e) { /* not kept then */ }
	document.getElementById('viewTiles').setAttribute('aria-pressed', v === 'tiles' ? 'true' : 'false');
	document.getElementById('viewList').setAttribute('aria-pressed', v === 'list' ? 'true' : 'false');
	const list = document.getElementById('mediaList');
	list.className = 'webui-media-list ' + (v === 'list' ? 'rows' : 'tiles');
	if (shown.length)
		list.replaceChildren.apply(list, shown.map(itemFor));
}

// The library's directories, as the answer names them: chips above the
// list, each named by its last part, its whole path its tooltip
function showSources(sources) {
	const key = sources.join('\n');
	if (key === sourcesShown) return;
	sourcesShown = key;
	const entries = [['', webuiL('media.all', 'All recordings')]].concat(sources.map(s => [s, s.replace(/\/+$/, '').split('/').pop() || s]));
	const chips = document.getElementById('sourceChips');
	chips.replaceChildren.apply(chips, entries.map(e => webuiEl('button', {type: 'button', data: {source: e[0]},
		title: e[0] || webuiL('media.all', 'All recordings'),
		attrs: {'aria-pressed': e[0] === source ? 'true' : 'false'}, on: {click: () => chooseSource(e[0])}}, e[1])));
	webuiArrowKeys(chips, 'button');
}

function chooseSource(value) {
	source = value;
	document.querySelectorAll('#sourceChips button').forEach(b => {
		b.setAttribute('aria-pressed', (b.dataset.source || '') === value ? 'true' : 'false');
	});
	loadList(false);
}

// The facts of a recording's details, as rows of a list
function factsOf(rec) {
	const facts = [];
	function fact(label, value) {
		if (value === undefined || value === null || value === '') return;
		facts.push(webuiEl('dt', {}, label), webuiEl('dd', {}, String(value)));
	}
	fact(webuiL('msg.channel', 'Channel'), rec.channel);
	fact(webuiL('tm.start', 'Start'), when(rec.start));
	fact(webuiL('media.sort_duration', 'Length'), length(rec.duration));
	fact(webuiL('media.sort_size', 'Size'), bytes(rec.size));
	fact(webuiL('media.year', 'Year'), rec.year);
	fact(webuiL('media.country', 'Country'), rec.country);
	if (rec.rating) fact(webuiL('media.rating', 'Rating'), (rec.rating / 10).toFixed(1));
	if (rec.age && rec.age !== 99) fact(webuiL('media.age', 'Approved from'), rec.age);
	if (rec.audio && rec.audio.length) fact(webuiL('media.audio', 'Sound tracks'), rec.audio.join(', '));
	return webuiEl('dl', {className: 'webui-media-detail-facts'}, facts);
}

function textOf(rec) {
	const text = [rec.description, rec.long_description].filter(Boolean).join('\n\n');
	return text ? webuiEl('div', {className: 'webui-media-detail-text'}, text) : null;
}

// A recording chosen: in the column beside the list, marked there, or in
// a sheet where the screen has no room for the column
function choose(rec) {
	if (!besideShown()) {
		showDetail(rec.id);
		return;
	}
	chosen = rec.id;
	document.querySelectorAll('#mediaList > li[data-id]').forEach(li => {
		const on = li.dataset.id === rec.id;
		li.classList.toggle('chosen', on);
		if (on) li.setAttribute('aria-current', 'true');
		else li.removeAttribute('aria-current');
	});
	showBeside(rec.id);
}

// The column beside the list: the chosen recording's picture, title, facts
// and text, Play on TV and the other actions; without one, how to choose
async function showBeside(id) {
	const box = document.getElementById('mediaDetail');
	if (!box) return;
	const token = ++detailToken;
	if (!id) {
		box.replaceChildren(webuiState('empty', webuiL('media.choose', 'No recording chosen'),
			webuiL('media.choose_hint', 'Choose a recording to see its details here.')));
		return;
	}
	box.replaceChildren(webuiState('loading', webuiL('msg.loading', 'Loading...'), ''));
	let rec;
	try {
		rec = await webuiV1('GET', archive(id));
	} catch (e) {
		if (token === detailToken)
			box.replaceChildren(webuiStateFor(e, {retry: () => showBeside(id)}));
		return;
	}
	if (token !== detailToken) return;
	sawPlaying([rec]);
	const title = titleOf(rec);
	const sub = [rec.channel, rec.start ? when(rec.start) : '', length(rec.duration)].filter(Boolean).join(' · ');
	box.replaceChildren(
		onPicture(picture(rec), [rec.playing ? playingPill() : null,
			rec.duration ? webuiEl('span', {className: 'webui-media-len'}, length(rec.duration)) : null]),
		webuiEl('div', {className: 'webui-media-detail-body'},
			webuiEl('h2', {}, title),
			sub ? webuiEl('p', {className: 'webui-media-detail-sub'}, sub) : null,
			factsOf(rec),
			textOf(rec),
			detailActs(rec, null)));
}

/* The actions of a recording's details, the same in the column and in the
   sheet: Play on TV (Stop while it plays) across, Playlist and Download side
   by side, Delete across below them, each with its symbol and its word.
   leave() runs before Play and Delete, which take the sheet away; Stop
   leaves it open. */
function detailActs(rec, leave) {
	const base = '/api/v1' + archive(rec.id);
	const act = function(cls, svg, label, onClick) {
		return webuiEl('button', {type: 'button', className: 'webui-btn' + (cls ? ' ' + cls : ''), on: {click: onClick}},
			webuiMarkup(svg), webuiEl('span', {}, label));
	};
	const away = fn => () => { if (leave) leave(); fn(); };
	return webuiEl('div', {className: 'webui-media-detail-acts'},
		playButton(rec.id, titleOf(rec), 'act', leave),
		act('', SVG_LIST, webuiL('media.playlist', 'Playlist (M3U)'), () => fetchTo(base + '/playlist.m3u')),
		act('', SVG_DOWN, webuiL('media.download', 'Download'), () => fetchTo(base + '/file')),
		act('webui-media-delete', SVG_DELETE, webuiL('media.delete', 'Delete'), away(() => remove(rec))));
}

// The details of one recording in a sheet: its picture, facts and text,
// the actions at its foot in sight while the text scrolls; Close is the
// sheet's cross and Escape
async function showDetail(id) {
	let rec;
	try {
		rec = await webuiV1('GET', archive(id));
	} catch (e) {
		webuiToastError('', e);
		return;
	}
	sawPlaying([rec]);
	const base = '/api/v1' + archive(id);
	let sheet = null;
	sheet = webuiSheet({
		title: rec.title || rec.channel || rec.id,
		body: [
			rec.cover ? webuiEl('img', {className: 'webui-media-detail-cover', attrs: {src: base + '/cover', alt: ''}}) : null,
			factsOf(rec),
			textOf(rec),
			detailActs(rec, () => { if (sheet) sheet.close('close'); })
		]
	});
	sheet.dialog.classList.add('webui-media-sheet');
}

// On the TV: a file the player plays, or a box in standby, is asked about first
const play = webuiPlayRecording;

// Delete with a question; the box says why it cannot (recording,
// playing, a share it may not write)
function remove(rec) {
	webuiSheet({
		title: rec.title || rec.channel || rec.id,
		body: webuiEl('p', {}, webuiL('media.delete_confirm', 'Delete this recording with its file?')),
		actions: [
			{label: webuiL('tm.cancel', 'Cancel'), value: false},
			{label: webuiL('media.delete', 'Delete'), value: true, primary: true, danger: true}
		],
		onClose: async result => {
			if (result !== true) return;
			try {
				await webuiV1('DELETE', archive(rec.id));
				webuiShowToast(webuiL('media.deleted', 'Recording deleted'));
				if (chosen === rec.id) chosen = null;
				loadList(false);
			} catch (e) {
				webuiToastError('', e);
			}
		}
	});
}

/* The bar and the column beside the list stay in sight while the list
   scrolls: the column sticks below the bar and is as high as the room left
   under it, so its actions stay reachable however long its text. Both
   heights change with the window and with the bar's wrapping. */
function fitSticky() {
	const page = document.querySelector('.webui-media');
	const bar = document.querySelector('.webui-media-bar');
	const main = page && page.closest('main');
	if (!bar || !main) return;
	const m = getComputedStyle(main);
	page.style.setProperty('--webui-media-bar-h', bar.offsetHeight + 'px');
	page.style.setProperty('--webui-media-room', (main.clientHeight - parseFloat(m.paddingTop) - parseFloat(m.paddingBottom)) + 'px');
}
if (typeof ResizeObserver === 'function') {
	const fit = new ResizeObserver(fitSticky);
	fit.observe(document.querySelector('.webui-media-bar'));
	fit.observe(document.querySelector('main'));
} else {
	window.addEventListener('resize', fitSticky);
	fitSticky();
}

// Controls: a search waits for the typing to pause
let searchTimer = null;
document.getElementById('mediaSearch').addEventListener('input', () => {
	clearTimeout(searchTimer);
	searchTimer = setTimeout(() => loadList(false), 400);
});
document.getElementById('mediaSort').addEventListener('change', () => loadList(false));
document.getElementById('mediaOrder').addEventListener('change', () => loadList(false));
document.getElementById('mediaCover').addEventListener('change', () => loadList(false));
document.getElementById('mediaMore').addEventListener('click', () => loadList(true));
document.getElementById('viewTiles').addEventListener('click', () => setView('tiles'));
document.getElementById('viewList').addEventListener('click', () => setView('list'));

// once the texts are in: a finished recording joins the library; a
// playback moves the "playing" mark
webuiAfterTexts(function() {
	setView(view);
	// the recordings are one stop of Tab: the arrows go from tile to tile
	// (or row to row) and to their buttons as they lie, Enter opens the
	// details or presses the button
	webuiArrowKeys(document.getElementById('mediaList'), '.webui-media-open, .webui-media-play, .webui-menu-btn', '',
		{near: true, start: '.webui-media-open'});
	// tiles or list: one stop, on the view shown
	webuiArrowKeys(document.getElementById('viewTiles').parentElement, 'button');
	loadList(false);
	// what the TV plays: read now, and again whenever the event stream
	// comes back after a gap
	readPlayback();
	let opened = 0;
	webuiV1Events({
		open: () => { if (opened++) readPlayback(); },
		'record-stop': () => loadList(false, true),
		playback: playbackEvent
	});
	// without the event stream (refused, or none in this browser) what plays
	// is read every 30 s while the page is visible
	setInterval(() => {
		if (!webuiV1EventsLive() && document.visibilityState === 'visible') readPlayback();
	}, 30000);
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return Promise.all([loadList(false), readPlayback()]);
	});
}
