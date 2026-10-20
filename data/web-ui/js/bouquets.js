// The script of bouquets.html.

// The channel list over /api/v1: bouquets, their channels and what each
// is showing now. Names, titles and texts come from the broadcast and go
// in as text (webui-dom.js); nothing here is built as markup.
let currentChannelId = null;	// the v1 id of the channel the box plays
let currentMode = 'tv';		// the list shown: the one the box plays from
let currentBouquetNumber = '';	// the v1 bouquet id, '' for all channels
let bouquetsData = [];		// the bouquets of that list, hidden ones left out
let logosOffered = true;	// false once the box says it hands no logos out

const SVG_PLAY = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5,3 19,12 5,21"/></svg>';
const SVG_GLOBE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/></svg>';
const SVG_EXPAND = '<svg class="webui-expand-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6,9 12,15 18,9"/></svg>';
const SVG_INFO = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>';
const SVG_REC = '<svg viewBox="0 0 24 24" fill="currentColor"><circle cx="12" cy="12" r="8"/></svg>';
const SVG_STREAM = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 17H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/><polygon points="12,15 17,21 7,21"/></svg>';

function clock(sec) {
	return webuiFormatTime(new Date(sec * 1000));
}

function rowOf(channelId) {
	return document.querySelector('.webui-channel-row[data-id="' + CSS.escape(String(channelId)) + '"]');
}

// The channel the box plays, and with it which list to show
async function loadCurrentChannel() {
	const ch = await webuiV1('GET', '/channels/current').catch(() => null);
	currentChannelId = ch ? ch.id : null;
	currentMode = ch && (ch.kind === 'radio' || ch.kind === 'webradio') ? 'radio' : 'tv';
}

// The bouquets of that list, and the one holding the current channel
async function loadBouquets() {
	const list = document.getElementById('bouquetList');
	try {
		const r = await webuiV1('GET', '/bouquets');
		bouquetsData = ((r && r.items) || []).filter(b =>
			!b.hidden && (currentMode === 'radio' ? b.radio_count : b.tv_count) > 0);

		let active = '';
		if (currentChannelId) {
			const holding = await webuiV1('GET', '/bouquets', {query: {holds: currentChannelId}}).catch(() => null);
			const found = holding && (holding.items || []).find(x => bouquetsData.some(b => b.id === x.id));
			if (found) active = String(found.id);
		}
		renderBouquetList(active || (bouquetsData[0] ? String(bouquetsData[0].id) : ''));
	} catch (e) {
		console.error('Failed to load bouquets:', e);
		list.replaceChildren(webuiEl('li', {className: 'webui-empty'}, webuiL('msg.load_fail', 'Loading failed')));
	}
}

function renderBouquetList(activeBouquet) {
	const list = document.getElementById('bouquetList');
	const items = [webuiEl('li', {className: 'webui-nav-item webui-bouquet-item' + (activeBouquet === '' ? ' active' : ''),
		on: {click: () => selectBouquet('')}},
		webuiMarkup(SVG_GLOBE),
		webuiEl('span', {className: 'webui-nav-name webui-bouquet-name'}, webuiL('bq.all_channels', 'All channels')))];
	bouquetsData.forEach(b => {
		items.push(webuiEl('li', {className: 'webui-nav-item webui-bouquet-item' + (String(activeBouquet) === String(b.id) ? ' active' : ''),
			data: {bouquet: String(b.id)}, on: {click: () => selectBouquet(String(b.id))}},
			webuiMarkup(SVG_PLAY),
			webuiEl('span', {className: 'webui-nav-name webui-bouquet-name'}, b.name)));
	});
	list.replaceChildren.apply(list, items);
	currentBouquetNumber = activeBouquet;

	setTimeout(() => {
		const activeItem = list.querySelector('.webui-bouquet-item.active');
		if (activeItem) scrollToElementInContainer(activeItem, list);
	}, 200);

	updateBouquetPreview();
	loadChannels(activeBouquet);
}

function selectBouquet(bouquet) {
	document.querySelectorAll('.webui-bouquet-item').forEach(item => {
		const itemBouquet = item.dataset.bouquet || '';
		item.classList.toggle('active', itemBouquet === bouquet);
	});
	currentBouquetNumber = bouquet;
	updateBouquetPreview();
	loadChannels(bouquet);
	// Mobile: auto-switch to channel card for better UX
	if (window.innerWidth <= 800) selectCard('card-channels');
}

function channelRow(ch) {
	const isCurrent = ch.id === currentChannelId;
	const placeholder = webuiEl('div', {className: 'webui-icon-placeholder'}, currentMode === 'radio' ? 'Radio' : 'TV');
	const row = webuiEl('li', {className: 'webui-channel-row' + (isCurrent ? ' current' : ''),
		data: {id: ch.id}, on: {click: event => toggleChannelDetail(event, ch.id)}},
		webuiEl('div', {className: 'webui-channel-main'},
			placeholder,
			webuiMarkup(SVG_EXPAND),
			webuiEl('div', {className: 'webui-channel-info'},
				webuiEl('div', {className: 'webui-channel-name'}, ch.name),
				webuiEl('div', {className: 'webui-channel-epg'}),
				webuiEl('div', {className: 'webui-channel-progress'}, webuiEl('div', {className: 'webui-channel-progress-bar'}))),
			webuiEl('div', {className: 'webui-channel-actions', on: {click: event => event.stopPropagation()}},
				actionButton(SVG_PLAY, webuiL('epg.zap', 'Zap'), webuiL('bq.zap_title', 'Switch channel'), () => zapTo(ch.id)),
				actionButton(SVG_INFO, 'Info', webuiL('dash.epg_info', 'Programme info'), () => showEpg(ch.id, ch.name)),
				actionButton(SVG_REC, 'Rec', webuiL('bq.rec_title', 'Record programme'), () => webuiRecordEvent(ch.id, ch.name, epgNow[ch.id]), true),
				actionButton(SVG_STREAM, 'Stream', webuiL('bq.stream_title', 'Start stream'), () => webuiStreamChannel(ch.id, ch.name)))),
		webuiEl('div', {className: 'webui-channel-detail', id: 'detail-' + ch.id},
			webuiEl('div', {className: 'webui-channel-detail-loading'}, webuiL('bq.tap_info', 'Tap to load programme info...'))));
	// a logo only while the box hands them out; one that fails leaves the placeholder text
	if (logosOffered) {
		const img = webuiEl('img', {attrs: {src: '/api/v1/channels/' + encodeURIComponent(ch.id) + '/logo', alt: '', loading: 'lazy'},
			on: {error: function() {
				row.classList.remove('has-logo');
				placeholder.textContent = currentMode === 'radio' ? 'Radio' : 'TV';
			}}});
		placeholder.replaceChildren(img);
		row.classList.add('has-logo');
	}
	return row;
}

function actionButton(svg, label, title, onClick, danger) {
	return webuiEl('button', {className: 'webui-btn', title: title, style: danger ? {color: 'var(--webui-danger)'} : undefined,
		on: {click: onClick}},
		webuiMarkup(svg),
		webuiEl('span', {className: 'btn-label'}, label));
}

// Whether the box hands logos out at all, asked once with the first
// channel: a list of hundreds would otherwise ask for each in vain.
async function checkLogos(channelId) {
	try {
		const r = await fetch('/api/v1/channels/' + encodeURIComponent(channelId) + '/logo', {credentials: 'same-origin'});
		if (r.status === 404) {
			const p = await r.json().catch(() => null);
			if (p && p.type === '/errors/logos-not-offered') logosOffered = false;
		}
	} catch (e) { /* asked again on the next list */ }
}

let channelsToken = 0;
async function loadChannels(bouquet) {
	const token = ++channelsToken;
	const list = document.getElementById('channelList');
	const countEl = document.getElementById('channelCount');
	const titleEl = document.getElementById('currentBouquetName');
	list.replaceChildren(webuiEl('li', {attrs: {'aria-busy': 'true'}}, webuiL('bq.loading_channels', 'Loading channels...')));

	if (bouquet === '') {
		titleEl.textContent = webuiL('bq.all_channels', 'All channels');
	} else {
		const bq = bouquetsData.find(b => String(b.id) === String(bouquet));
		titleEl.textContent = bq ? bq.name : webuiL('dash.channels', 'Channels');
	}

	try {
		const query = {mode: currentMode};
		if (bouquet) query.bouquet = bouquet;
		const channels = await webuiV1All('/channels', query);
		if (token !== channelsToken) return;	// another bouquet was chosen meanwhile

		countEl.textContent = channels.length + ' ' + webuiL('bq.channels_unit', 'channels');
		if (channels.length === 0) {
			list.replaceChildren(webuiEl('li', {className: 'webui-empty'}, webuiL('bq.no_channels', 'No channels found')));
			return;
		}
		if (logosOffered) await checkLogos(channels[0].id);
		if (token !== channelsToken) return;

		list.replaceChildren.apply(list, channels.map(channelRow));

		// Scroll to current channel in container (after layout is complete)
		setTimeout(() => {
			const currentRow = list.querySelector('.current');
			const cardBody = list.parentElement;
			if (currentRow && cardBody) {
				const scrollTo = currentRow.offsetTop - (cardBody.clientHeight / 2) + (currentRow.offsetHeight / 2);
				cardBody.scrollTop = Math.max(0, scrollTo);
			}
		}, 300);

		updateChannelPreview();
		loadEpgInfo(channels, bouquet, token);
	} catch (e) {
		if (token !== channelsToken) return;
		list.replaceChildren(webuiEl('li', {className: 'webui-empty'}, webuiL('msg.load_fail', 'Loading failed')));
		console.error('Failed to load channels:', e);
	}
}

// What each channel shows now, from the guide grid: a page of 20 channels
// per request, for the bouquet or for the named channels of the whole list.
const epgNow = {};	// channel id -> the event running now
let progressUpdateInterval = null;

async function loadEpgInfo(channels, bouquet, token) {
	const now = Math.floor(Date.now() / 1000);
	const pages = [];
	if (bouquet) {
		pages.push({bouquet: bouquet});
	} else {
		for (let i = 0; i < channels.length; i += 20)
			pages.push({channels: channels.slice(i, i + 20).map(ch => ch.id).join(',')});
	}
	for (const base of pages) {
		let cursor = null;
		do {
			if (token !== channelsToken) return;
			const query = Object.assign({from: now, to: now + 60, limit: 20}, base);
			if (cursor) query.cursor = cursor;
			let page;
			try {
				page = await webuiV1('GET', '/epg/grid', {query: query});
			} catch (e) {
				break;	// no guide for this page; the rows stay without one
			}
			if (token !== channelsToken) return;
			((page && page.items) || []).forEach(row => showNow(row.channel_id, row.events || [], now));
			cursor = page && page.next_cursor;
		} while (cursor);
	}
	startProgressUpdateTimer();
}

function showNow(channelId, events, now) {
	const ev = events.find(e => e.start <= now && now < e.start + e.duration);
	const row = rowOf(channelId);
	if (!ev || !row) return;
	epgNow[channelId] = ev;
	row.querySelector('.webui-channel-epg').textContent =
		clock(ev.start) + '-' + clock(ev.start + ev.duration) + ' ' + (ev.title || '');
	updateChannelProgress(channelId);
}

function updateChannelProgress(channelId) {
	const ev = epgNow[channelId];
	const row = rowOf(channelId);
	if (!ev || !row) return;
	const container = row.querySelector('.webui-channel-progress');
	const bar = row.querySelector('.webui-channel-progress-bar');
	const now = Date.now() / 1000;
	const percent = Math.min(100, Math.max(0, (now - ev.start) / ev.duration * 100));
	bar.style.width = percent + '%';
	container.classList.add('active');
}

function updateAllProgress() {
	const now = Date.now() / 1000;
	for (const id of Object.keys(epgNow)) {
		if (now >= epgNow[id].start + epgNow[id].duration) {
			delete epgNow[id];
			continue;
		}
		updateChannelProgress(id);
	}
}

function startProgressUpdateTimer() {
	if (!progressUpdateInterval)
		progressUpdateInterval = setInterval(updateAllProgress, 30000);
}

function markCurrent() {
	document.querySelectorAll('.webui-channel-row').forEach(row => {
		row.classList.toggle('current', row.dataset.id === currentChannelId);
	});
	updateChannelPreview();
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

function showEpg(channelId, channelName) {
	webuiShowEpgModal(channelId, channelName);
}

async function toggleChannelDetail(event, channelId) {
	if (event.target.closest('.webui-channel-actions')) return;
	const row = rowOf(channelId);
	if (!row) return;
	const detailEl = document.getElementById('detail-' + channelId);
	if (row.classList.contains('row-expanded')) {
		row.classList.remove('row-expanded');
		return;
	}
	document.querySelectorAll('.webui-channel-row.row-expanded').forEach(r => r.classList.remove('row-expanded'));
	row.classList.add('row-expanded');
	if (detailEl && detailEl.querySelector('.webui-channel-detail-loading'))
		await loadChannelDetail(channelId, detailEl);
}

// The long text of what runs now
async function loadChannelDetail(channelId, detailEl) {
	function content(text, muted) {
		detailEl.replaceChildren(webuiEl('div', {className: 'webui-channel-detail-content'},
			webuiEl('div', {className: 'webui-channel-detail-desc',
				style: muted ? {'font-style': 'italic', color: 'var(--pico-muted-color)'} : undefined}, text)));
	}
	try {
		let ev = epgNow[channelId];
		if (!ev) ev = await webuiV1('GET', '/epg/current', {query: {channel: channelId}}).catch(() => null);
		if (!ev) {
			content(webuiL('bq.no_epg', 'No EPG data available'));
			return;
		}
		const full = await webuiV1('GET', '/epg/event', {query: {id: ev.id, start: ev.start}}).catch(() => ev);
		const desc = [full.description, full.long_description].filter(Boolean).join('\n\n');
		if (desc) content(desc);
		else content(webuiL('epg.no_desc', 'No description available'), true);
	} catch (e) {
		console.error('Failed to load channel detail:', e);
		content(webuiL('msg.load_fail', 'Loading failed'));
	}
}

function scrollToElementInContainer(element, listElement) {
	if (!element) return;
	const scrollParent = listElement ? listElement.parentElement : null;
	if (scrollParent && scrollParent.scrollHeight > scrollParent.clientHeight) {
		const containerTop = scrollParent.getBoundingClientRect().top;
		const elementTop = element.getBoundingClientRect().top;
		const relativeTop = elementTop - containerTop + scrollParent.scrollTop;
		scrollParent.scrollTop = Math.max(0, relativeTop - (scrollParent.clientHeight / 2) + (element.offsetHeight / 2));
	} else {
		element.scrollIntoView({ block: 'center', behavior: 'instant' });
	}
}

// ===== Mobile Accordion =====

function updateBouquetPreview() {
	const preview = document.getElementById('bouquetPreview');
	if (preview && bouquetsData.length > 0) {
		const active = bouquetsData.find(b => String(b.id) === String(currentBouquetNumber));
		preview.textContent = active ? active.name : bouquetsData.length + ' Bouquets';
	}
}

function updateChannelPreview() {
	const preview = document.getElementById('channelPreview');
	const count = document.getElementById('channelCount');
	if (preview) {
		const currentRow = document.querySelector('.webui-channel-row.current .webui-channel-name');
		if (currentRow) preview.textContent = currentRow.textContent;
		else if (count) preview.textContent = count.textContent;
	}
}

// Initialize once the texts are in; a zap from anywhere else (remote
// control, timer) moves the mark
webuiAfterTexts(() => loadCurrentChannel().then(() => loadBouquets()));
let zapRead = 0;	// an older answer arriving after a newer one is dropped
webuiAfterTexts(() => webuiV1Events({
	zap: async function() {
		const read = ++zapRead;
		const mode = currentMode;
		const ch = await webuiV1('GET', '/channels/current').catch(() => null);
		if (read !== zapRead) return;
		currentChannelId = ch ? ch.id : null;
		currentMode = ch && (ch.kind === 'radio' || ch.kind === 'webradio') ? 'radio' : 'tv';
		if (currentMode !== mode) loadBouquets();	// the box switched lists
		else markCurrent();
	}
}));

webuiAfterTexts(function() {
	webuiInitMobileAccordion({
		cardOrder: ['card-bouquets', 'card-channels'],
		storageKey: 'webui-bouquets-card',
		defaultCard: 'card-channels',
		breakpoint: 800,
		indicatorId: 'cardIndicator',
		containerSelector: '.webui-bouquets-grid',
		mode: 'collapsed'
	});
});

// Custom pull-to-refresh for bouquets page
// Check if expanded card's body is at top before allowing refresh
function isAtScrollTop() {
	const expandedCard = document.querySelector('.webui-card:not(.collapsed)');
	if (expandedCard) {
		const cardBody = expandedCard.querySelector('.webui-card-body');
		if (cardBody && cardBody.scrollTop > 5) return false;
	}
	const main = document.querySelector('.webui-bouquets-grid');
	if (main && main.scrollTop > 5) return false;
	return true;
}

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		if (!isAtScrollTop()) return Promise.resolve();
		return loadCurrentChannel().then(() => loadBouquets());
	});
}

if (typeof window.webuiIsAtScrollTop === 'undefined') {
	window.webuiIsAtScrollTop = isAtScrollTop;
}
