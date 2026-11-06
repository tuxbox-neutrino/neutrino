// The script of recordings.html.

// The finished recordings over /api/v1/recordings/archive: a page at a
// time, by folder, title words and order; as tiles with their cover or
// their channel's logo, or as rows of a list; the one chosen with its
// details and actions in a column beside them on a wide screen, in a sheet
// on a narrower one. Titles and texts come from the recording's metadata
// and go in as text.
const PAGE = 30;
const SVG_FILM = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M7 4v16M17 4v16M2 9h5M2 15h5M17 9h5M17 15h5"/></svg>';
const SVG_PLAY = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polygon points="5,3 19,12 5,21"/></svg>';
const SVG_PLAY_FILLED = '<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><polygon points="6 4 20 12 6 20"/></svg>';
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

// One page of the list; more=true appends the next page
async function loadList(more) {
	const token = ++listToken;
	const list = document.getElementById('mediaList');
	const moreBtn = document.getElementById('mediaMore');
	if (!more) {
		nextOffset = 0;
		list.replaceChildren(webuiEl('li', {}, webuiState('loading', webuiL('media.loading', 'Loading recordings...'), '')));
	}
	if (nextOffset === null) return;

	const query = {offset: nextOffset, limit: PAGE, sort: document.getElementById('mediaSort').value};
	const order = document.getElementById('mediaOrder').value;
	if (order) query.order = order;
	const words = document.getElementById('mediaSearch').value.trim();
	if (words) query.title = words;
	if (source) query.source = source;

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
	if (!more) {
		list.replaceChildren();
		shown = [];
	}
	items.forEach(rec => {
		shown.push(rec);
		list.appendChild(itemFor(rec));
	});
	if (!more && items.length === 0) {
		const empty = webuiEl('li', {}, webuiState('empty', webuiL('media.none', 'No recordings found'),
			words ? webuiL('media.none_search', 'Nothing matches these words.') : ''));
		list.appendChild(empty);
		// no recordings at all: a folder that is not there says why
		if (!words && !source)
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
	playbackSoon = setTimeout(() => { playbackSoon = null; loadList(false); }, 1000);
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
	const title = rec.title || rec.channel || rec.id;
	const meta = [rec.channel, when(rec.start), bytes(rec.size)].filter(Boolean).join(' · ');
	const pic = onPicture(picture(rec), [
		rec.playing ? playingPill() : null,
		rec.duration ? webuiEl('span', {className: 'webui-media-len'}, length(rec.duration)) : null,
		webuiEl('button', {type: 'button', className: 'webui-media-tile-play webui-media-play', title: webuiL('media.play', 'Play on TV'),
			attrs: {'aria-label': webuiL('media.play', 'Play on TV') + ': ' + title}, on: {click: () => play(rec.id)}},
			webuiMarkup(SVG_PLAY_FILLED))]);
	return webuiEl('li', {className: 'webui-media-tile' + (rec.id === chosen ? ' chosen' : ''), data: {id: rec.id},
		attrs: rec.id === chosen ? {'aria-current': 'true'} : {}},
		pic, openButton(rec, title, meta, false));
}

// One recording as a row: its picture, its title and what it is, Play on
// TV and the menu with the others
function recordingRow(rec) {
	const title = rec.title || rec.channel || rec.id;
	const meta = [rec.channel, when(rec.start), length(rec.duration), bytes(rec.size)].filter(Boolean).join(' · ');
	const base = '/api/v1' + archive(rec.id);
	return webuiEl('li', {className: 'webui-media-row' + (rec.id === chosen ? ' chosen' : ''), data: {id: rec.id},
		attrs: rec.id === chosen ? {'aria-current': 'true'} : {}},
		onPicture(picture(rec), []),
		webuiEl('div', {className: 'webui-media-info'}, openButton(rec, title, meta, rec.playing)),
		webuiEl('div', {className: 'webui-media-actions'},
			webuiEl('button', {type: 'button', className: 'webui-btn webui-media-play',
				attrs: {'aria-label': webuiL('media.play', 'Play on TV') + ': ' + title}, on: {click: () => play(rec.id)}},
				webuiMarkup(SVG_PLAY), webuiEl('span', {}, webuiL('media.play', 'Play on TV'))),
			webuiMenuButton(webuiL('bq.more', 'More for %s').replace('%s', title), [
				{label: webuiL('tm.details', 'Details'), icon: SVG_INFO, onClick: () => choose(rec)},
				{label: webuiL('media.playlist', 'Playlist (M3U)'), icon: SVG_LIST, onClick: () => fetchTo(base + '/playlist.m3u')},
				{label: webuiL('media.download', 'Download'), icon: SVG_DOWN, onClick: () => fetchTo(base + '/file')},
				null,
				{label: webuiL('media.delete', 'Delete'), icon: SVG_DELETE, danger: true, onClick: () => remove(rec)}])));
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
	const title = rec.title || rec.channel || rec.id;
	const base = '/api/v1' + archive(id);
	const sub = [rec.channel, rec.start ? when(rec.start) : '', length(rec.duration)].filter(Boolean).join(' · ');
	const act = function(cls, svg, label, onClick) {
		return webuiEl('button', {type: 'button', className: 'webui-btn' + (cls ? ' ' + cls : ''), on: {click: onClick}},
			webuiMarkup(svg), webuiEl('span', {}, label));
	};
	box.replaceChildren(
		onPicture(picture(rec), [rec.playing ? playingPill() : null,
			rec.duration ? webuiEl('span', {className: 'webui-media-len'}, length(rec.duration)) : null]),
		webuiEl('div', {className: 'webui-media-detail-body'},
			webuiEl('h2', {}, title),
			sub ? webuiEl('p', {className: 'webui-media-detail-sub'}, sub) : null,
			factsOf(rec),
			textOf(rec),
			webuiEl('div', {className: 'webui-media-detail-acts'},
				act('primary', SVG_PLAY, webuiL('media.play', 'Play on TV'), () => play(id)),
				act('', SVG_LIST, webuiL('media.playlist', 'Playlist (M3U)'), () => fetchTo(base + '/playlist.m3u')),
				act('', SVG_DOWN, webuiL('media.download', 'Download'), () => fetchTo(base + '/file')),
				act('webui-media-delete', SVG_DELETE, webuiL('media.delete', 'Delete'), () => remove(rec)))));
}

// The details of one recording in a sheet
async function showDetail(id) {
	let rec;
	try {
		rec = await webuiV1('GET', archive(id));
	} catch (e) {
		webuiToastError('', e);
		return;
	}
	const base = '/api/v1' + archive(id);
	webuiSheet({
		title: rec.title || rec.channel || rec.id,
		body: [
			rec.cover ? webuiEl('img', {className: 'webui-media-detail-cover', attrs: {src: base + '/cover', alt: ''}}) : null,
			factsOf(rec),
			textOf(rec),
			webuiEl('div', {className: 'webui-media-detail-links'},
				webuiEl('a', {attrs: {href: base + '/playlist.m3u', download: ''}}, webuiL('media.playlist', 'Playlist (M3U)')),
				webuiEl('a', {attrs: {href: base + '/file', download: ''}}, webuiL('media.download', 'Download')))
		],
		actions: [
			{label: webuiL('media.delete', 'Delete'), value: 'delete', danger: true},
			{label: webuiL('msg.close', 'Close'), value: 'close'},
			{label: webuiL('media.play', 'Play on TV'), value: 'play', primary: true}
		],
		onClose: result => {
			if (result === 'play') play(id);
			else if (result === 'delete') remove(rec);
		}
	});
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

// Controls: a search waits for the typing to pause
let searchTimer = null;
document.getElementById('mediaSearch').addEventListener('input', () => {
	clearTimeout(searchTimer);
	searchTimer = setTimeout(() => loadList(false), 400);
});
document.getElementById('mediaSort').addEventListener('change', () => loadList(false));
document.getElementById('mediaOrder').addEventListener('change', () => loadList(false));
document.getElementById('mediaMore').addEventListener('click', () => loadList(true));
document.getElementById('viewTiles').addEventListener('click', () => setView('tiles'));
document.getElementById('viewList').addEventListener('click', () => setView('list'));

// once the texts are in: a finished recording joins the library; a
// playback moves the "playing" mark
webuiAfterTexts(function() {
	setView(view);
	loadList(false);
	webuiV1Events({
		'record-stop': () => loadList(false),
		playback: playbackChanged
	});
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return loadList(false);
	});
}
