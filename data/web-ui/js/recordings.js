// The script of recordings.html.

// The finished recordings over /api/v1/recordings/archive: a page at a
// time, by source, title words and order; details, play on the TV,
// playlist and file, and delete. Titles and texts come from the
// recording's metadata and go in as text.
const PAGE = 30;
const SVG_FILM = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M7 4v16M17 4v16M2 9h5M2 15h5M17 9h5M17 15h5"/></svg>';
const SVG_PLAY = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5,3 19,12 5,21"/></svg>';
const SVG_INFO = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>';
const SVG_DELETE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3,6 5,6 21,6"/><path d="M19,6v14a2,2 0 0,1-2,2H7a2,2 0 0,1-2-2V6m3,0V4a2,2 0 0,1,2-2h4a2,2 0 0,1,2,2v2"/></svg>';

let source = '';		// the library directory shown, '' for all
let nextOffset = 0;		// where the next page begins, null after the last
let listToken = 0;		// a newer read makes an older answer stale
let partialRetry = null;
let partialTries = 0;		// a directory still read the first time is asked about a few times, not for ever
let pagesShown = 0;		// pages on screen, so a reread does not throw away what "more" loaded
let sourcesShown = null;	// nothing drawn yet: an empty list is drawn too

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

// One page of the list; more=true appends the next page
async function loadList(more) {
	const token = ++listToken;
	const list = document.getElementById('mediaList');
	const moreBtn = document.getElementById('mediaMore');
	if (!more) {
		nextOffset = 0;
		list.replaceChildren(webuiEl('li', {attrs: {'aria-busy': 'true'}}, webuiL('media.loading', 'Loading recordings...')));
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
		list.replaceChildren(webuiEl('li', {className: 'webui-empty'}, webuiL('msg.load_fail', 'Loading failed') + ': ' + e.message));
		moreBtn.hidden = true;
		return;
	}
	if (token !== listToken) return;

	const items = (page && page.items) || [];
	pagesShown = more ? pagesShown + 1 : 1;
	if (!more) list.replaceChildren();
	items.forEach(rec => list.appendChild(recordingRow(rec)));
	if (!more && items.length === 0)
		list.appendChild(webuiEl('li', {className: 'webui-empty'}, webuiL('media.none', 'No recordings found')));

	nextOffset = (page && typeof page.next_offset === 'number') ? page.next_offset : null;
	moreBtn.hidden = nextOffset === null;
	const total = (page && page.total) || 0;
	document.getElementById('mediaCount').textContent = String(total);
	document.getElementById('mediaPreview').textContent = total + ' ' + webuiL('nav.recordings', 'Recordings');
	showSources((page && page.sources) || []);

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

function recordingRow(rec) {
	const cover = webuiEl('div', {className: 'webui-media-cover'},
		webuiEl('img', {attrs: {src: '/api/v1' + archive(rec.id) + '/cover', alt: '', loading: 'lazy'},
			on: {error: function() { cover.replaceChildren(webuiMarkup(SVG_FILM)); }}}));
	const meta = [rec.channel, when(rec.start), length(rec.duration), bytes(rec.size)].filter(Boolean).join(' · ');
	return webuiEl('li', {className: 'webui-media-row', data: {id: rec.id}, on: {click: () => showDetail(rec.id)}},
		cover,
		webuiEl('div', {className: 'webui-media-info'},
			webuiEl('div', {className: 'webui-media-title'}, rec.title || rec.channel || rec.id,
				rec.playing ? webuiEl('span', {className: 'webui-media-badge'}, webuiL('media.playing', 'Playing')) : null),
			webuiEl('div', {className: 'webui-media-meta'}, meta)),
		webuiEl('div', {className: 'webui-media-actions', on: {click: e => e.stopPropagation()}},
			button(SVG_PLAY, webuiL('media.play', 'Play on TV'), () => play(rec.id)),
			button(SVG_INFO, 'Info', () => showDetail(rec.id)),
			button(SVG_DELETE, webuiL('media.delete', 'Delete'), () => remove(rec), true)));
}

function button(svg, title, onClick, danger) {
	return webuiEl('button', {type: 'button', className: 'webui-btn', title: title,
		attrs: {'aria-label': title}, style: danger ? {color: 'var(--webui-danger)'} : undefined,
		on: {click: onClick}}, webuiMarkup(svg));
}

// The library's directories, as the answer names them
function showSources(sources) {
	const key = sources.join('\n');
	if (key === sourcesShown) return;
	sourcesShown = key;
	const list = document.getElementById('sourceList');
	const item = (value, label) => webuiEl('li', {className: 'webui-nav-item webui-filter-item' + (value === source ? ' active' : ''),
		data: {source: value}, on: {click: () => chooseSource(value, label)}},
		webuiEl('span', {className: 'webui-nav-name webui-filter-name'}, label));
	list.replaceChildren.apply(list, [item('', webuiL('media.all', 'All recordings'))].concat(sources.map(s => item(s, s))));
}

function chooseSource(value, label) {
	source = value;
	document.querySelectorAll('#sourceList .webui-filter-item').forEach(li => {
		li.classList.toggle('active', (li.dataset.source || '') === value);
	});
	document.getElementById('sourcePreview').textContent = label;
	loadList(false);
	if (window.innerWidth <= 800) selectCard('card-recordings');
}

// The details of one recording in a sheet
async function showDetail(id) {
	let rec;
	try {
		rec = await webuiV1('GET', archive(id));
	} catch (e) {
		webuiShowToast(e.message, true);
		return;
	}
	const facts = [];
	function fact(label, value) {
		if (value === undefined || value === null || value === '') return;
		facts.push(webuiEl('dt', {}, label), webuiEl('dd', {}, String(value)));
	}
	fact(webuiL('dash.channels', 'Channels'), rec.channel);
	fact('Start', when(rec.start));
	fact(webuiL('media.sort_duration', 'Length'), length(rec.duration));
	fact(webuiL('media.sort_size', 'Size'), bytes(rec.size));
	fact(webuiL('media.year', 'Year'), rec.year);
	fact(webuiL('media.country', 'Country'), rec.country);
	if (rec.rating) fact(webuiL('media.rating', 'Rating'), (rec.rating / 10).toFixed(1));
	if (rec.age && rec.age !== 99) fact(webuiL('media.age', 'Approved from'), rec.age);
	if (rec.audio && rec.audio.length) fact(webuiL('media.audio', 'Sound tracks'), rec.audio.join(', '));
	const text = [rec.description, rec.long_description].filter(Boolean).join('\n\n');
	const base = '/api/v1' + archive(id);
	webuiSheet({
		title: rec.title || rec.channel || rec.id,
		body: [
			rec.cover ? webuiEl('img', {className: 'webui-media-detail-cover', attrs: {src: base + '/cover', alt: ''}}) : null,
			webuiEl('dl', {className: 'webui-media-detail-facts'}, facts),
			text ? webuiEl('div', {className: 'webui-media-detail-text'}, text) : null,
			webuiEl('div', {className: 'webui-media-detail-links'},
				webuiEl('a', {attrs: {href: base + '/playlist.m3u', download: ''}}, webuiL('media.playlist', 'Playlist (M3U)')),
				webuiEl('a', {attrs: {href: base + '/file', download: ''}}, webuiL('media.download', 'Download file')))
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
async function play(id) {
	try {
		const ok = await webuiV1Lifting(archive(id) + '/play', {}, {
			title: webuiL('media.ask_title', 'Play recording'),
			ok: webuiL('media.play', 'Play on TV'),
			stop: webuiL('media.ask_stop', 'The player is playing a file. Stop it and play this recording?'),
			wake: webuiL('media.ask_wake', 'The box is in standby. Switch it on and play this recording?')
		});
		if (ok) webuiShowToast(webuiL('media.played', 'Playback started'));
	} catch (e) {
		webuiShowToast(e.message, true);
	}
}

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
				loadList(false);
			} catch (e) {
				webuiShowToast(e.message, true);
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

// once the texts are in: a finished recording joins the library; a
// playback moves the "playing" mark
webuiAfterTexts(function() {
	loadList(false);
	webuiV1Events({
		'record-stop': () => loadList(false),
		playback: playbackChanged
	});
});

webuiAfterTexts(function() {
	webuiInitMobileAccordion({
		cardOrder: ['card-sources', 'card-recordings'],
		storageKey: 'webui-media-card',
		defaultCard: 'card-recordings',
		breakpoint: 800,
		indicatorId: 'cardIndicator',
		containerSelector: '.webui-media-grid',
		mode: 'collapsed'
	});
});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return loadList(false);
	});
}
