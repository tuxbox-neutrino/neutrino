// The script of timer.html.

// The timer list over /api/v1: GET /api/v1/timers, kept current by the
// timer and record events; made with POST, changed with PATCH (times
// and repetition only), removed with DELETE. Titles, channel names and
// folders go in as text.

// The head's number of set timers comes from this page's reads.
webuiTimersFollowed();

let allTimers = [];
let timersShown = false;	// the list was drawn once, so a failed read later keeps it
let currentFilter = 'all';
let openDetail = '';		// the timer whose details are open, kept across a refresh
let currentMode = 'tv';

const TIMER_TYPE_LABELS = WEBUI_TIMER_TYPE_LABELS;
const TIMER_STATE_LABELS = WEBUI_TIMER_STATE_LABELS;
// The kinds v1 names, as the numbers the shared labels, icons and filters use
const KIND_TYPES = {'shutdown': 1, 'zapto': 3, 'standby': 4, 'record': 5, 'remind': 6,
	'sleeptimer': 7, 'exec-plugin': 8, 'immediate-record': 9};
const TYPE_KINDS = {1: 'shutdown', 3: 'zapto', 4: 'standby', 5: 'record', 6: 'remind',
	7: 'sleeptimer', 8: 'exec-plugin'};
const REPEAT_NAMES = {0: 'once', 1: 'daily', 2: 'weekly', 3: 'biweekly', 4: 'fourweekly', 5: 'monthly'};
// the weekday bits of a repetition, Monday first
const WEEKDAY_BITS = [512, 1024, 2048, 4096, 8192, 16384, 32768];

// The weekdays' short names in the page's language (1 January 2024 was a Monday)
function weekdayNames() {
	const lang = document.documentElement.lang || 'en';
	return WEEKDAY_BITS.map((bit, i) => [bit, new Date(2024, 0, 1 + i).toLocaleDateString(lang, {weekday: 'short'})]);
}

// The filters: by kind, then by state; the list beside the timers and the
// choice above them on a phone are drawn from it
const FILTER_ICONS = {
	all: '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18"/>',
	'type-5': '<circle cx="12" cy="12" r="6" fill="currentColor"/>',
	'type-3': '<path d="M7 5v14l11-7z"/>',
	'type-4': '<path d="M12 3v9M6.3 6.3a8 8 0 1 0 11.4 0"/>',
	'type-6': '<path d="M18 8a6 6 0 0 0-12 0c0 7-3 9-3 9h18s-3-2-3-9M13.7 21a2 2 0 0 1-3.4 0"/>',
	'state-scheduled': '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
	'state-running': '<path d="M7 5v14l11-7z"/>',
	'state-finished': '<path d="M22 11.1V12a10 10 0 1 1-5.9-9.1M22 4 12 14l-3-3"/>'
};
function filters() {
	return {
		types: [['all', webuiL('tm.all', 'All')], ['type-5', TIMER_TYPE_LABELS[5]], ['type-3', TIMER_TYPE_LABELS[3]],
			['type-4', TIMER_TYPE_LABELS[4]], ['type-6', TIMER_TYPE_LABELS[6]]],
		states: [['state-scheduled', webuiL('tm.planned', 'Planned')], ['state-running', webuiL('tm.active', 'Active')],
			['state-finished', webuiL('tm.expired', 'Expired')]]
	};
}

function renderFilters() {
	const f = filters();
	const button = item => webuiEl('li', {}, webuiEl('button', {type: 'button', data: {filter: item[0]},
		attrs: {'aria-pressed': item[0] === currentFilter ? 'true' : 'false'}, on: {click: () => setFilter(item[0])}},
		webuiMarkup('<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + FILTER_ICONS[item[0]] + '</svg>'),
		webuiEl('span', {}, item[1]),
		webuiEl('span', {className: 'webui-filter-count', id: 'count-' + item[0]}, '0')));
	const types = document.getElementById('filterTypes');
	const states = document.getElementById('filterStates');
	types.replaceChildren.apply(types, f.types.map(button));
	states.replaceChildren.apply(states, f.states.map(button));
	webuiArrowKeys(types.parentElement, '.webui-filter-list button');
	const sel = document.getElementById('filterSelect');
	sel.replaceChildren(
		webuiEl('optgroup', {attrs: {label: webuiL('tm.type', 'Type')}}, f.types.map(i => webuiEl('option', {value: i[0]}, i[1]))),
		webuiEl('optgroup', {attrs: {label: webuiL('tm.state', 'State')}}, f.states.map(i => webuiEl('option', {value: i[0]}, i[1]))));
	sel.value = currentFilter;
	sel.onchange = () => setFilter(sel.value);
}

function typeNumber(t) {
	return KIND_TYPES[t.kind] || parseInt(t.kind, 10) || 0;
}

function repeatText(t) {
	const r = t.repeat || 0;
	if (r >= 256) {
		const days = weekdayNames().filter(d => (r & d[0]) !== 0).map(d => d[1]);
		return (WEBUI_REPEAT_LABELS.weekly || 'Weekly') + ' (' + days.join(', ') + ')';
	}
	return WEBUI_REPEAT_LABELS[REPEAT_NAMES[r] || 'once'] || '';
}

function dateOf(sec) {
	const d = new Date(sec * 1000);
	return webuiPad2(d.getDate()) + '.' + webuiPad2(d.getMonth() + 1) + '.' + d.getFullYear();
}
const clock = webuiClock;

// A timer without a channel names "0"
function channelOf(t) {
	return t.channel_id && t.channel_id !== '0' ? t.channel_id : '';
}

// A read that fails keeps a list drawn before; the first one says why
let timersRead = 0;	// an older answer arriving after a newer one is dropped
async function loadTimers() {
	const read = ++timersRead;
	const list = document.getElementById('timerList');
	try {
		const r = await webuiV1('GET', '/timers');
		const items = (r && r.items) || [];
		await webuiEnsureNames(items.map(channelOf));
		if (read !== timersRead) return;
		allTimers = items;
		timersShown = true;
		webuiTimersSeen(items);
		updateFilterCounts();
		renderTimerList();
	} catch (e) {
		if (read !== timersRead) return;	// a newer read has the list
		webuiTimersSeen(null);
		if (timersShown) {
			console.warn('timers not read again:', e.message);
			return;
		}
		list.replaceChildren(webuiEl('li', {}, webuiStateFor(e, {title: webuiL('timer.unavail', 'Timers not available'), retry: loadTimers})));
	}
}

function updateFilterCounts() {
	const counts = {all: allTimers.length};
	allTimers.forEach(t => {
		const typeNum = typeNumber(t);
		const state = t.state || 0;
		const typeKey = 'type-' + (typeNum === 9 ? 5 : typeNum);
		counts[typeKey] = (counts[typeKey] || 0) + 1;
		const stateKey = state <= 1 ? 'state-scheduled' : (state === 2 ? 'state-running' : 'state-finished');
		counts[stateKey] = (counts[stateKey] || 0) + 1;
	});
	document.querySelectorAll('.webui-filter-count').forEach(el => {
		el.textContent = counts[el.id.replace('count-', '')] || 0;
	});
}

function getFilteredTimers() {
	if (currentFilter === 'all') return allTimers.slice();
	return allTimers.filter(t => {
		const typeNum = typeNumber(t);
		const state = t.state || 0;
		if (currentFilter.startsWith('type-')) {
			const filterType = parseInt(currentFilter.split('-')[1]);
			if (filterType === 5) return typeNum === 5 || typeNum === 9;
			return typeNum === filterType;
		}
		if (currentFilter === 'state-scheduled') return state <= 1;
		if (currentFilter === 'state-running') return state === 2;
		if (currentFilter === 'state-finished') return state >= 3;
		return true;
	});
}

function setFilter(filter) {
	currentFilter = filter;
	document.querySelectorAll('.webui-filter-list button').forEach(b => {
		b.setAttribute('aria-pressed', b.dataset.filter === filter ? 'true' : 'false');
	});
	const sel = document.getElementById('filterSelect');
	if (sel) sel.value = filter;
	renderTimerList();
}

const SVG_EXPAND = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polyline points="6,9 12,15 18,9"/></svg>';
const SVG_EDIT = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M11 4H4a2 2 0 00-2 2v14a2 2 0 002 2h14a2 2 0 002-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 013 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>';
const SVG_STOP = '<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><rect x="6" y="6" width="12" height="12" rx="1"/></svg>';
const SVG_DELETE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><polyline points="3,6 5,6 21,6"/><path d="M19,6v14a2,2 0 0,1-2,2H7a2,2 0 0,1-2-2V6m3,0V4a2,2 0 0,1,2-2h4a2,2 0 0,1,2,2v2"/></svg>';

function renderTimerList() {
	const list = document.getElementById('timerList');
	const countEl = document.getElementById('timerCount');
	const timers = getFilteredTimers();

	// running first, then the nearest start
	timers.sort((a, b) => {
		if (a.state === 2 && b.state !== 2) return -1;
		if (b.state === 2 && a.state !== 2) return 1;
		return a.start - b.start;
	});
	countEl.textContent = String(timers.length);

	if (timers.length === 0) {
		list.replaceChildren(webuiEl('li', {}, currentFilter === 'all'
			? webuiState('empty', webuiL('dash.no_timers', 'No timers programmed'), webuiL('dash.no_timers_hint', 'Timers are set in the EPG or on the timer list.'),
				{action: {label: webuiL('timer.new', 'New timer'), onClick: () => openTimerModal(),
					tip: webuiL('tip.timer_new', 'A timer for a recording, a switch, standby or a reminder')}})
			: webuiState('empty', webuiL('tm.none_found', 'No timers found'), '')));
		return;
	}
	if (list.querySelector('.webui-state'))
		list.replaceChildren();
	// a row drawn anew keeps its details open, and the focus on its button
	const focused = document.activeElement && document.activeElement.closest('.webui-timer-row') ? document.activeElement : null;
	const focusRow = focused ? focused.closest('.webui-timer-row').dataset.id : '';
	const focusOnMore = !!(focused && focused.classList.contains('webui-timer-more'));
	webuiSync(list, timers, t => String(t.id), timerRow);
	const open = openDetail && list.querySelector('.webui-timer-row[data-id="' + CSS.escape(openDetail) + '"]');
	if (open) {
		open.classList.add('row-expanded');
		open.querySelector('.webui-timer-more').setAttribute('aria-expanded', 'true');
	}
	if (focusOnMore && !document.contains(focused)) {
		const again = list.querySelector('.webui-timer-row[data-id="' + CSS.escape(focusRow) + '"] .webui-timer-more');
		if (again) again.focus();
	}
}

function detail(label, value) {
	return [webuiEl('dt', {}, label), webuiEl('dd', {}, value)];
}

function timerRow(timer) {
	const typeNum = typeNumber(timer);
	const state = timer.state || 0;
	const channel = webuiNames[channelOf(timer)] || '';
	const typeLabel = TIMER_TYPE_LABELS[typeNum] || WEBUI_TIMER_TYPES[typeNum] || String(timer.kind);
	const title = timer.title || channel || typeLabel;
	const stateLabel = TIMER_STATE_LABELS[state] || '';
	const rep = repeatText(timer);
	const timeRange = clock(timer.start) + (timer.stop ? ' – ' + clock(timer.stop) : '');
	const isRecording = typeNum === 5 || typeNum === 9;

	const badges = [];
	if (timer.repeat) badges.push(webuiEl('span', {className: 'webui-pill'}, rep));
	if (state === 2) badges.push(webuiEl('span', {className: 'webui-pill ' + (isRecording || timer.kind === 'zapto' ? 'webui-timer-mark running' : 'ok')}, stateLabel));
	else if (state >= 3) badges.push(webuiEl('span', {className: 'webui-pill'}, stateLabel));

	// one main action, the others in the menu
	const actions = [];
	if (state === 2 && isRecording) {
		actions.push(actionButton('webui-btn danger', SVG_STOP, webuiL('tm.stop', 'Stop'), webuiL('dash.record_stop', 'Stop recording') + ': ' + title,
			() => stopRecording(timer.id, title)));
	} else if (state < 2) {
		actions.push(actionButton('webui-btn', SVG_EDIT, webuiL('tm.edit', 'Edit'), webuiL('tm.edit_title', 'Edit timer') + ': ' + title, () => editTimer(timer.id)));
		actions.push(webuiMenuButton(webuiL('bq.more', 'More for %s').replace('%s', title), [
			{label: webuiL('tm.delete_title', 'Delete timer'), icon: SVG_DELETE, danger: true, onClick: () => deleteTimer(timer.id, title)}]));
	} else {
		actions.push(actionButton('webui-btn', SVG_DELETE, webuiL('fm.delete', 'Delete'), webuiL('tm.delete_title', 'Delete timer') + ': ' + title,
			() => deleteTimer(timer.id, title)));
	}

	const details = [].concat(
		detail(webuiL('tm.type', 'Type'), typeLabel),
		detail(webuiL('tm.state', 'State'), stateLabel),
		detail(webuiL('tm.start', 'Start'), dateOf(timer.start) + ' ' + clock(timer.start)),
		timer.stop ? detail(webuiL('tm.end', 'End'), dateOf(timer.stop) + ' ' + clock(timer.stop)) : [],
		timer.announce ? detail(webuiL('tm.announce', 'Announcement'), dateOf(timer.announce) + ' ' + clock(timer.announce)) : [],
		detail(webuiL('tm.repeat', 'Repeat'), rep + (timer.repeat && timer.repeat_count ? ' (' + timer.repeat_count + ')' : '')),
		channel ? detail(webuiL('msg.channel', 'Channel'), channel) : [],
		channelOf(timer) ? detail(webuiL('tm.channel_id', 'Channel ID'), timer.channel_id) : [],
		timer.recording_dir ? detail(webuiL('tm.rec_dir', 'Recording folder'), timer.recording_dir) : [],
		timer.epg_id && timer.epg_id !== '0' ? detail(webuiL('tm.epg_id', 'EPG id'), timer.epg_id) : [],
		detail(webuiL('tm.timer_id', 'Timer id'), String(timer.id)));

	const detailId = 'detail-' + timer.id;
	const more = webuiEl('button', {type: 'button', className: 'webui-timer-more',
		attrs: {'aria-expanded': 'false', 'aria-controls': detailId, 'aria-label': webuiL('tm.details', 'Details') + ': ' + title},
		on: {click: toggleTimerDetail}}, webuiMarkup(SVG_EXPAND));

	return webuiEl('li', {className: 'webui-timer-row' + (state === 2 ? ' state-running' : (state >= 3 ? ' state-finished' : '')),
		data: {id: String(timer.id)}},
		webuiEl('div', {className: 'webui-timer-main'},
			webuiMarkup(webuiGetTimerIconHtml(WEBUI_TIMER_TYPES[typeNum] || '')),
			webuiEl('div', {className: 'webui-timer-row-info'},
				webuiEl('div', {className: 'webui-timer-row-title'}, title),
				webuiEl('div', {className: 'webui-timer-row-meta'},
					webuiEl('span', {className: 'webui-timer-row-channel'}, [channel, typeLabel].filter(Boolean).join(' · ')),
					badges)),
			webuiEl('div', {className: 'webui-timer-row-time'},
				webuiEl('div', {className: 'webui-timer-row-date'}, dateOf(timer.start)),
				webuiEl('div', {className: 'webui-timer-row-hours'}, timeRange)),
			webuiEl('div', {className: 'webui-timer-actions'}, actions),
			more),
		webuiEl('div', {className: 'webui-timer-detail', id: detailId},
			webuiEl('dl', {className: 'webui-kv webui-timer-detail-grid'}, details)));
}

function actionButton(cls, svg, label, name, onClick) {
	return webuiEl('button', {type: 'button', className: cls, title: name, attrs: {'aria-label': name}, on: {click: onClick}},
		webuiMarkup(svg), webuiEl('span', {className: 'btn-label'}, label));
}

// The details of one row at a time, opened and closed by its button
function toggleTimerDetail(event) {
	const btn = event.currentTarget;
	const row = btn.closest('.webui-timer-row');
	if (!row) return;
	const open = !row.classList.contains('row-expanded');
	document.querySelectorAll('.webui-timer-row.row-expanded').forEach(r => {
		r.classList.remove('row-expanded');
		const b = r.querySelector('.webui-timer-more');
		if (b) b.setAttribute('aria-expanded', 'false');
	});
	row.classList.toggle('row-expanded', open);
	btn.setAttribute('aria-expanded', open ? 'true' : 'false');
	openDetail = open ? row.dataset.id : '';
}

// The questions are sheets; a title goes in as text
function ask(heading, text, label, danger) {
	return new Promise(resolve => {
		webuiSheet({title: heading, body: webuiEl('p', {}, text),
			actions: [{label: webuiL('tm.cancel', 'Cancel'), value: false},
				{label: label, value: true, primary: true, danger: !!danger}],
			onClose: result => resolve(result === true)});
	});
}

async function stopRecording(id, title) {
	if (await ask(webuiL('dash.record_stop', 'Stop recording'), webuiL('tm.stop_ask', 'Stop the recording "%s"?').replace('%s', title), webuiL('tm.stop', 'Stop'), true))
		await webuiStopRecording(id);	// the record event reads the list again
}

async function deleteTimer(id, title) {
	if (!await ask(webuiL('tm.delete_title', 'Delete timer'), webuiL('tm.delete_ask', 'Delete the timer "%s"?').replace('%s', title), webuiL('fm.delete', 'Delete'), true))
		return;
	try {
		await webuiV1('DELETE', '/timers/' + encodeURIComponent(id));
		await loadTimers();
	} catch (e) {
		webuiToastError(webuiL('tm.delete_title', 'Delete timer'), e);
	}
}

// Initialize: the list follows the timer and record events; without
// an open stream it is read every minute
webuiAfterTexts(function() {
	renderFilters();
	document.getElementById('timerList').replaceChildren(webuiEl('li', {}, webuiState('loading', webuiL('tm.loading', 'Loading timers...'), '')));
	// the timers are one stop of Tab: Up and Down go from row to row, Left
	// and Right to the buttons of a row (Edit, More, Details)
	webuiArrowKeys(document.getElementById('timerList'), '.webui-timer-actions button, .webui-timer-more', '', {near: true});
	loadTimers();
	let opened = 0;
	webuiV1Events({
		// the first open is the page loading, every later one a reconnect
		// after a gap, which may have missed a change
		open: function() { if (opened++) loadTimers(); },
		'timer-changed': loadTimers,
		'record-start': loadTimers,
		'record-stop': loadTimers
	});
	setInterval(() => { if (!webuiV1EventsLive()) loadTimers(); }, 60000);
});
webuiV1('GET', '/channels/current').then(ch => {
	currentMode = ch && (ch.kind === 'radio' || ch.kind === 'webradio') ? 'radio' : 'tv';
}).catch(() => {});

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		return loadTimers();
	});
}

// ===== Timer Modal =====
let timerModalEditId = null;
let cachedRecDirs = null;
let releaseTimerModal = null;

function buildTimerModalHtml() {
	const now = new Date(webuiBoxNow());
	const todayStr = webuiFormatDateInput(now);
	const nowTime = webuiFormatTime(now);
	const stop = new Date(now.getTime() + 2 * 3600000);
	const stopDate = webuiFormatDateInput(stop);
	const stopTime = webuiFormatTime(stop);
	const type = n => escapeHtml(TIMER_TYPE_LABELS[n] || '');
	const days = weekdayNames().map(d => `<label><input type="checkbox" name="tmWd" value="${d[0]}"><span>${escapeHtml(d[1])}</span></label>`).join('');

	return `
	<div class="webui-modal-header">
		<h3 id="timerModalTitle">${webuiLH('tm.new', 'New timer')}</h3>
		<button type="button" class="webui-modal-close" onclick="closeTimerModal()" aria-label="${webuiLH('msg.close', 'Close')}">
			<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M18 6L6 18M6 6l12 12"/></svg>
		</button>
	</div>
	<div class="webui-modal-body">
		<div class="webui-form-section">
			<div class="webui-form-row">
				<label for="tmType">${webuiLH('tm.type', 'Type')}</label>
				<select id="tmType" onchange="onTimerTypeChange()" data-fixed>
					<option value="5">${type(5)}</option>
					<option value="3">${type(3)}</option>
					<option value="4">${type(4)}</option>
					<option value="6">${type(6)}</option>
					<option value="7">${type(7)}</option>
					<option value="8">${type(8)}</option>
					<option value="1">${type(1)}</option>
				</select>
			</div>
		</div>

		<div class="webui-form-section">
			<div class="webui-form-section-title">${webuiLH('tm.times', 'Times')}</div>
			<div class="webui-form-row">
				<label for="tmAlDate">${webuiLH('tm.start', 'Start')}</label>
				<div class="webui-time-group">
					<input type="date" id="tmAlDate" value="${todayStr}">
					<input type="time" id="tmAlTime" value="${nowTime}" aria-label="${webuiLH('tm.start_time', 'Start time')}">
				</div>
			</div>
			<div class="webui-form-row" id="tmStopRow">
				<label for="tmStDate">${webuiLH('tm.end', 'End')}</label>
				<div class="webui-time-group">
					<input type="date" id="tmStDate" value="${stopDate}">
					<input type="time" id="tmStTime" value="${stopTime}" aria-label="${webuiLH('tm.end_time', 'End time')}">
				</div>
			</div>
		</div>

		<div class="webui-form-section">
			<div class="webui-form-section-title">${webuiLH('tm.repeat', 'Repeat')}</div>
			<div class="webui-form-row">
				<label for="tmRepeat">${webuiLH('tm.how_often', 'How often')}</label>
				<select id="tmRepeat" onchange="onRepeatChange()">
					<option value="0">${webuiLH('tm.once', 'Once')}</option>
					<option value="1">${webuiLH('tm.daily', 'Daily')}</option>
					<option value="2">${webuiLH('tm.weekly', 'Weekly')}</option>
					<option value="3">${webuiLH('tm.biweekly', 'Every two weeks')}</option>
					<option value="4">${webuiLH('tm.fourweekly', 'Every four weeks')}</option>
					<option value="5">${webuiLH('tm.monthly', 'Monthly')}</option>
					<option value="256">${webuiLH('tm.weekdays', 'Weekdays')}</option>
				</select>
			</div>
			<fieldset class="webui-form-row webui-form-hidden" id="tmWeekdaysRow">
				<legend class="webui-visually-hidden">${webuiLH('tm.days', 'Days')}</legend>
				<span aria-hidden="true">${webuiLH('tm.days', 'Days')}</span>
				<div class="webui-form-weekdays">${days}</div>
			</fieldset>
			<div class="webui-form-row webui-form-hidden" id="tmRepcountRow">
				<label for="tmRepcount">${webuiLH('tm.count', 'Count')}</label>
				<input type="number" id="tmRepcount" min="0" max="999" value="0" placeholder="${webuiLH('tm.unlimited', '0 = unlimited')}" aria-describedby="tmRepcountHint">
				<span id="tmRepcountHint" hidden>${webuiLH('tm.unlimited', '0 = unlimited')}</span>
			</div>
		</div>

		<div class="webui-form-section" id="tmChannelSection">
			<div class="webui-form-section-title">${webuiLH('msg.channel', 'Channel')}</div>
			<div class="webui-form-row">
				<label for="tmBouquet">${webuiLH('nav.bouquets', 'Bouquets')}</label>
				<select id="tmBouquet" onchange="onBouquetChange()" data-fixed>
					<option value="">${webuiLH('msg.loading', 'Loading...')}</option>
				</select>
			</div>
			<div class="webui-form-row">
				<label for="tmChannel">${webuiLH('msg.channel', 'Channel')}</label>
				<select id="tmChannel" data-fixed>
					<option value="">${webuiLH('timer.choose_bq', 'Choose bouquet...')}</option>
				</select>
			</div>
		</div>

		<div class="webui-form-section" id="tmRecordSection">
			<div class="webui-form-section-title">${webuiLH('tm.rec_options', 'Recording options')}</div>
			<div class="webui-form-row">
				<label for="tmRecDir">${webuiLH('fm.folder', 'Folder')}</label>
				<select id="tmRecDir" data-fixed onchange="onRecDirChange(this)">
					<option value="">${webuiLH('tm.dir_default', 'Default')}</option>
				</select>
			</div>
			<div class="webui-form-check">
				<input type="checkbox" id="tmRecSafety" checked data-fixed>
				<label for="tmRecSafety">${webuiLH('tm.safety', 'Recording margin (before/after)')}</label>
			</div>
			<div class="webui-form-check">
				<input type="checkbox" id="tmAutoAdjust" data-fixed>
				<label for="tmAutoAdjust">${webuiLH('tm.auto_adjust', 'Adjust to EPG automatically')}</label>
			</div>
		</div>

		<div class="webui-form-section webui-form-hidden" id="tmStandbySection">
			<div class="webui-form-section-title">${type(4)}</div>
			<div class="webui-form-row">
				<label for="tmStandbyOn">${webuiLH('tm.sb_action', 'Action')}</label>
				<select id="tmStandbyOn" data-fixed>
					<option value="1">${webuiLH('tm.sb_on', 'Switch on')}</option>
					<option value="0">${webuiLH('tm.sb_off', 'Switch off')}</option>
				</select>
			</div>
		</div>

		<div class="webui-form-section webui-form-hidden" id="tmMessageSection">
			<div class="webui-form-section-title">${type(6)}</div>
			<div class="webui-form-row">
				<label for="tmMessage">${webuiLH('tools.message', 'Message')}</label>
				<input type="text" id="tmMessage" maxlength="30" placeholder="${webuiLH('tools.message_ph', 'Enter message…')}" data-fixed>
			</div>
		</div>

		<div class="webui-form-section webui-form-hidden" id="tmPluginSection">
			<div class="webui-form-section-title">${type(8)}</div>
			<div class="webui-form-row">
				<label for="tmPlugin">${webuiLH('tm.plugin_name', 'Name')}</label>
				<input type="text" id="tmPlugin" maxlength="30" placeholder="${webuiLH('tm.plugin_ph', 'Name of the plugin')}" data-fixed>
			</div>
		</div>
		<p class="webui-form-error" id="tmError" role="alert" hidden></p>
	</div>
	<div class="webui-modal-footer">
		<button type="button" onclick="closeTimerModal()">${webuiLH('tm.cancel', 'Cancel')}</button>
		<button type="button" class="btn-primary" id="tmSubmitBtn" onclick="submitTimer()">${webuiLH('bqe.save', 'Save')}</button>
	</div>`;
}

function openTimerModal(editTimer) {
	timerModalEditId = null;
	const backdrop = document.createElement('div');
	backdrop.className = 'webui-modal-backdrop open';
	backdrop.id = 'timerModalBackdrop';
	backdrop.onclick = function(e) {
		if (e.target === backdrop) closeTimerModal();
	};
	const modal = document.createElement('div');
	modal.className = 'webui-modal';
	// a template of constants and the clock; nothing from the box
	modal.innerHTML = buildTimerModalHtml();
	backdrop.appendChild(modal);
	document.body.appendChild(backdrop);

	loadBouquets();
	loadRecordDirs();
	onTimerTypeChange();

	if (editTimer) {
		timerModalEditId = editTimer.id;
		document.getElementById('timerModalTitle').textContent = webuiL('tm.edit_title', 'Edit timer');
		document.getElementById('tmSubmitBtn').textContent = webuiL('tm.change', 'Change');
		populateEditData(editTimer);
	}
	releaseTimerModal = webuiDialogA11y(modal, {close: closeTimerModal, focus: document.getElementById(editTimer ? 'tmAlDate' : 'tmType')});
}

function closeTimerModal() {
	const backdrop = document.getElementById('timerModalBackdrop');
	if (backdrop) backdrop.remove();
	timerModalEditId = null;
	if (releaseTimerModal) {
		releaseTimerModal();
		releaseTimerModal = null;
	}
}

function onTimerTypeChange() {
	const type = document.getElementById('tmType').value;
	toggle('tmStopRow', type === '5');
	toggle('tmChannelSection', type === '5' || type === '3');
	toggle('tmRecordSection', type === '5');
	toggle('tmStandbySection', type === '4');
	toggle('tmMessageSection', type === '6');
	toggle('tmPluginSection', type === '8');
}

function toggle(id, show) {
	const el = document.getElementById(id);
	if (el) el.classList.toggle('webui-form-hidden', !show);
}

function onRepeatChange() {
	const rep = document.getElementById('tmRepeat').value;
	toggle('tmWeekdaysRow', parseInt(rep) >= 256);
	toggle('tmRepcountRow', rep !== '0');
}

function options(select, items) {
	select.replaceChildren.apply(select, items);
}

// The bouquets of the list the box plays from, hidden and empty ones left out
async function loadBouquets() {
	const sel = document.getElementById('tmBouquet');
	if (!sel) return [];
	try {
		const r = await webuiV1('GET', '/bouquets');
		const bouquets = ((r && r.items) || []).filter(b =>
			!b.hidden && (currentMode === 'radio' ? b.radio_count : b.tv_count) > 0);
		options(sel, [webuiEl('option', {value: ''}, webuiL('timer.choose_bq', 'Choose bouquet...'))]
			.concat(bouquets.map(b => webuiEl('option', {value: String(b.id)}, b.name))));
		return bouquets;
	} catch (e) {
		options(sel, [webuiEl('option', {value: ''}, webuiL('msg.load_fail', 'Loading failed'))]);
		return [];
	}
}

async function onBouquetChange() {
	const bouquet = document.getElementById('tmBouquet').value;
	const sel = document.getElementById('tmChannel');
	if (!sel) return;
	if (!bouquet) {
		options(sel, [webuiEl('option', {value: ''}, webuiL('timer.choose_bq', 'Choose bouquet...'))]);
		return;
	}
	options(sel, [webuiEl('option', {value: ''}, webuiL('msg.loading', 'Loading...'))]);
	try {
		const channels = await webuiV1All('/channels', {mode: currentMode, bouquet: bouquet});
		options(sel, channels.length ? channels.map(ch => webuiEl('option', {value: ch.id}, ch.name))
			: [webuiEl('option', {value: ''}, webuiL('bq.no_channels', 'No channels found'))]);
	} catch (e) {
		options(sel, [webuiEl('option', {value: ''}, webuiL('msg.load_fail', 'Loading failed'))]);
	}
}

// The folders a recording can go to, from GET /api/v1/recordings/folders:
// the box's roots and what lies below them, those on a disc that is not
// attached left out. A list the box cut short says so, and any other
// folder can be typed in.
const REC_DIR_OTHER = ':other';	// never a path: those start with /
let cachedRecPartial = false;
async function loadRecordDirs() {
	const sel = document.getElementById('tmRecDir');
	if (!sel) return;
	try {
		if (!cachedRecDirs) {
			const data = await webuiV1('GET', '/recordings/folders');
			cachedRecDirs = ((data && data.items) || []).filter(f => f.present).map(f => f.path);
			cachedRecPartial = !!(data && data.partial);
		}
		const items = [webuiEl('option', {value: ''}, webuiL('tm.dir_default', 'Default'))]
			.concat(cachedRecDirs.map(dir => webuiEl('option', {value: dir}, dir)));
		if (cachedRecPartial)
			items.push(webuiEl('option', {value: '', disabled: true}, '… ' + webuiL('tm.dir_cut', 'the box shortened this list')));
		items.push(webuiEl('option', {value: REC_DIR_OTHER}, webuiL('tm.dir_other', 'Other folder…')));
		options(sel, items);
		if (sel.dataset.want) offerDir(sel, sel.dataset.want);
	} catch (e) {
		// keep the default option
	}
	sel.dataset.last = sel.value;
}

// "Other folder": asks for a path on the box and offers it in the list
function onRecDirChange(sel) {
	if (sel.value !== REC_DIR_OTHER) {
		sel.dataset.last = sel.value;
		return;
	}
	const input = webuiEl('input', {type: 'text', value: sel.dataset.last || '/', attrs: {'aria-label': webuiL('tm.dir_ask', 'Folder on the box')}});
	const back = function() { sel.value = sel.dataset.last || ''; };
	webuiSheet({
		title: webuiL('tm.dir_ask', 'Folder on the box'),
		body: webuiEl('div', {}, input),
		actions: [
			{label: webuiL('msg.cancel', 'Cancel'), value: false},
			{label: webuiL('msg.ok', 'OK'), primary: true, onClick: function() {
				const dir = input.value.trim();
				if (dir.charAt(0) !== '/' || dir.indexOf('/../') >= 0 || /\/\.\.$/.test(dir)) {
					webuiShowToast(webuiL('tm.dir_bad', 'Give a whole path on the box, starting with /'), true);
					return false;
				}
				offerDir(sel, dir);
				sel.dataset.last = sel.value;
			}}
		],
		onClose: function() { if (sel.value === REC_DIR_OTHER) back(); }
	});
	input.focus();
}

// Selects a timer's own folder, adding it when the list does not hold it
// (a disc not attached now), so the form shows where the timer records
function offerDir(sel, dir) {
	const bare = d => d.replace(/\/+$/, '');
	const listed = Array.from(sel.options).find(o => o.value && bare(o.value) === bare(dir));
	if (listed) {
		sel.value = listed.value;
		return;
	}
	const other = Array.from(sel.options).find(o => o.value === REC_DIR_OTHER);
	sel.insertBefore(webuiEl('option', {value: dir}, dir), other || null);
	sel.value = dir;
}

function secondsOf(dateValue, timeValue) {
	const d = new Date(dateValue + 'T' + timeValue + ':00');
	return Math.floor(d.getTime() / 1000);
}

// The repeat number of the form, and null for weekdays with no day ticked
function repeatValue() {
	const rep = parseInt(document.getElementById('tmRepeat').value) || 0;
	if (rep < 256 || document.getElementById('tmRepeat').selectedOptions[0].dataset.kept) return rep;
	let days = 0;
	document.querySelectorAll('input[name="tmWd"]:checked').forEach(cb => { days += parseInt(cb.value); });
	return days ? 256 + days : null;
}

/* What is missing, or what the box refused, said in the form: the text
   below the fields (announced), the field marked and focused. '' clears. */
function formSays(text, fieldId) {
	const box = document.getElementById('tmError');
	document.querySelectorAll('#timerModalBackdrop [aria-invalid]').forEach(el => el.removeAttribute('aria-invalid'));
	if (!box) return;
	box.textContent = text;
	box.hidden = !text;
	const field = fieldId && document.getElementById(fieldId);
	if (field) {
		field.setAttribute('aria-invalid', 'true');
		field.setAttribute('aria-describedby', 'tmError');
		field.focus();
	}
}

async function submitTimer() {
	const btn = document.getElementById('tmSubmitBtn');
	if (!btn) return;
	const type = document.getElementById('tmType').value;
	const alDate = document.getElementById('tmAlDate').value;
	const alTime = document.getElementById('tmAlTime').value;
	const stDate = document.getElementById('tmStDate').value;
	const stTime = document.getElementById('tmStTime').value;

	if (!alDate || !alTime) {
		formSays(webuiL('tm.need_start', 'Please set a start time'), !alDate ? 'tmAlDate' : 'tmAlTime');
		return;
	}
	const needsStop = type === '5';
	if (needsStop && (!stDate || !stTime)) {
		formSays(webuiL('tm.need_end', 'Please set an end time'), !stDate ? 'tmStDate' : 'tmStTime');
		return;
	}
	const needsChannel = type === '5' || type === '3';
	const channelId = document.getElementById('tmChannel') ? document.getElementById('tmChannel').value : '';
	if (!timerModalEditId && needsChannel && !channelId) {
		formSays(webuiL('tm.need_channel', 'Please pick a channel'), document.getElementById('tmBouquet').value ? 'tmChannel' : 'tmBouquet');
		return;
	}

	const repeat = repeatValue();
	if (repeat === null) {
		formSays(webuiL('tm.need_day', 'Please tick at least one day'), null);
		const first = document.querySelector('input[name="tmWd"]');
		if (first) first.focus();
		return;
	}
	formSays('');
	const repcount = parseInt(document.getElementById('tmRepcount').value) || 0;
	const body = {start: secondsOf(alDate, alTime), repeat: repeat, repeat_count: repeat ? repcount : 0};
	if (needsStop) body.stop = secondsOf(stDate, stTime);

	if (!timerModalEditId) {
		body.kind = TYPE_KINDS[type];
		if (needsChannel) body.channel_id = channelId;
		if (type === '5') {
			const recDir = document.getElementById('tmRecDir').value;
			if (recDir.charAt(0) === '/') body.recording_dir = recDir;
			body.recording_safety = document.getElementById('tmRecSafety').checked;
			body.auto_adjust = document.getElementById('tmAutoAdjust').checked;
		} else if (type === '4') {
			body.standby_on = document.getElementById('tmStandbyOn').value === '1';
		} else if (type === '6') {
			body.title = document.getElementById('tmMessage').value || '';
		} else if (type === '8') {
			body.title = document.getElementById('tmPlugin').value || '';
		}
	}

	btn.disabled = true;
	btn.setAttribute('aria-busy', 'true');
	try {
		if (timerModalEditId)
			await webuiV1('PATCH', '/timers/' + encodeURIComponent(timerModalEditId), {body: body});
		else
			await webuiV1('POST', '/timers', {body: body});
		const changed = !!timerModalEditId;
		closeTimerModal();
		webuiShowToast(changed ? webuiL('tm.changed', 'Timer changed') : webuiL('tm.created', 'Timer created'));
		await loadTimers();
	} catch (e) {
		// the form stays as it was filled, and says why
		btn.disabled = false;
		btn.removeAttribute('aria-busy');
		if (!e.cancelled)
			formSays(webuiL('tm.save_fail', 'Saving failed') + ': ' + webuiErrText(e), null);
	}
}

function editTimer(timerId) {
	const timer = allTimers.find(t => t.id === timerId);
	if (!timer) {
		webuiShowToast(webuiL('tm.not_found', 'Timer not found'), true);
		return;
	}
	openTimerModal(timer);
}

// The form opened on a timer: what PATCH can change (times, repetition)
// stays open, the rest is shown and fixed. Another kind, channel or
// text is a new timer.
function populateEditData(timer) {
	const typeNum = typeNumber(timer);
	document.getElementById('tmType').value = String(typeNum === 9 ? 5 : typeNum);
	onTimerTypeChange();

	const start = new Date(timer.start * 1000);
	document.getElementById('tmAlDate').value = webuiFormatDateInput(start);
	document.getElementById('tmAlTime').value = webuiFormatTime(start);
	if (timer.stop) {
		const stop = new Date(timer.stop * 1000);
		document.getElementById('tmStDate').value = webuiFormatDateInput(stop);
		document.getElementById('tmStTime').value = webuiFormatTime(stop);
	}

	const rep = timer.repeat || 0;
	const repSel = document.getElementById('tmRepeat');
	repSel.value = String(rep >= 256 ? 256 : rep);
	if (repSel.value === '') {
		// a repetition this form has no row for (set at the box): kept as
		// it is and named, rather than turned into "once"
		repSel.appendChild(webuiEl('option', {value: String(rep), data: {kept: '1'}}, '#' + rep));
		repSel.value = String(rep);
	}
	document.querySelectorAll('input[name="tmWd"]').forEach(cb => {
		cb.checked = rep >= 256 && (rep & parseInt(cb.value)) !== 0;
	});
	onRepeatChange();
	if (timer.repeat_count) document.getElementById('tmRepcount').value = String(timer.repeat_count);

	if (channelOf(timer)) selectChannelById(timer.channel_id);
	if (timer.recording_dir) {
		// the folders may be listed already, or arrive later
		const dirSel = document.getElementById('tmRecDir');
		dirSel.dataset.want = timer.recording_dir;
		offerDir(dirSel, timer.recording_dir);
	}
	if (timer.kind === 'remind') document.getElementById('tmMessage').value = timer.title || '';
	if (timer.kind === 'exec-plugin') document.getElementById('tmPlugin').value = timer.title || '';
	if (timer.kind === 'standby') document.getElementById('tmStandbyOn').value = timer.standby_on ? '1' : '0';

	document.querySelectorAll('#timerModalBackdrop [data-fixed]').forEach(el => { el.disabled = true; });
}

// The bouquet holding the channel, asked of the box
async function selectChannelById(channelId) {
	const shown = await loadBouquets();
	const holding = await webuiV1('GET', '/bouquets', {query: {holds: channelId}}).catch(() => null);
	const found = holding && (holding.items || []).find(x => shown.some(b => b.id === x.id));
	const cSel = document.getElementById('tmChannel');
	if (found) {
		document.getElementById('tmBouquet').value = String(found.id);
		await onBouquetChange();
		if (cSel) cSel.value = channelId;
		if (cSel && cSel.value === channelId) return;
	}
	if (cSel) options(cSel, [webuiEl('option', {value: channelId}, webuiNames[channelId] || channelId)]);
}
