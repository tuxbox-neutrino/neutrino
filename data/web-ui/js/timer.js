// The script of timer.html.

// The timer list over /api/v1: GET /api/v1/timers, kept current by the
// timer and record events; made with POST, changed with PATCH (times
// and repetition only), removed with DELETE. Titles, channel names and
// folders go in as text.
let allTimers = [];
let currentFilter = 'all';
let currentMode = 'tv';
const names = Object.create(null);	// channel id -> name

const TIMER_TYPE_LABELS = WEBUI_TIMER_TYPE_LABELS;
const TIMER_STATE_LABELS = WEBUI_TIMER_STATE_LABELS;
// The kinds v1 names, as the numbers the shared labels, icons and filters use
const KIND_TYPES = {'shutdown': 1, 'zapto': 3, 'standby': 4, 'record': 5, 'remind': 6,
	'sleeptimer': 7, 'exec-plugin': 8, 'immediate-record': 9};
const TYPE_KINDS = {1: 'shutdown', 3: 'zapto', 4: 'standby', 5: 'record', 6: 'remind',
	7: 'sleeptimer', 8: 'exec-plugin'};
const REPEAT_NAMES = {0: 'once', 1: 'daily', 2: 'weekly', 3: 'biweekly', 4: 'fourweekly', 5: 'monthly'};
const WEEKDAY_NAMES = [[512, 'Mo'], [1024, 'Di'], [2048, 'Mi'], [4096, 'Do'], [8192, 'Fr'], [16384, 'Sa'], [32768, 'So']];

function typeNumber(t) {
	return KIND_TYPES[t.kind] || parseInt(t.kind, 10) || 0;
}

function repeatText(t) {
	const r = t.repeat || 0;
	if (r >= 256) {
		const days = WEEKDAY_NAMES.filter(d => (r & d[0]) !== 0).map(d => d[1]);
		return (WEBUI_REPEAT_LABELS.weekly || 'Weekly') + ' (' + days.join(', ') + ')';
	}
	return WEBUI_REPEAT_LABELS[REPEAT_NAMES[r] || 'once'] || '';
}

function dateOf(sec) {
	const d = new Date(sec * 1000);
	return webuiPad2(d.getDate()) + '.' + webuiPad2(d.getMonth() + 1) + '.' + d.getFullYear();
}
function clock(sec) { return webuiFormatTime(new Date(sec * 1000)); }

// A timer without a channel names "0"
function channelOf(t) {
	return t.channel_id && t.channel_id !== '0' ? t.channel_id : '';
}

// The names of the channels the timers name, each asked once, also
// while two reads of the list overlap
const naming = Object.create(null);
async function ensureNames(ids) {
	await Promise.all(ids.map(id => {
		if (!id || id in names) return null;
		if (!naming[id]) {
			naming[id] = webuiV1('GET', '/channels/' + encodeURIComponent(id)).catch(() => null).then(ch => {
				names[id] = (ch && ch.name) || '';
				delete naming[id];
			});
		}
		return naming[id];
	}));
}

let timersRead = 0;	// an older answer arriving after a newer one is dropped
async function loadTimers() {
	const read = ++timersRead;
	const list = document.getElementById('timerList');
	try {
		const r = await webuiV1('GET', '/timers');
		const items = (r && r.items) || [];
		await ensureNames(items.map(channelOf));
		if (read !== timersRead) return;
		allTimers = items;
		updateFilterCounts();
		renderTimerList();
	} catch (e) {
		if (read !== timersRead) return;	// a newer read has the list
		console.error('Failed to load timers:', e);
		list.replaceChildren(webuiEl('li', {className: 'webui-empty'}, webuiL('timer.unavail', 'Timers not available')));
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
	document.querySelectorAll('.webui-filter-item').forEach(item => {
		item.classList.toggle('active', item.dataset.filter === filter);
	});
	const activeItem = document.querySelector('.webui-filter-item.active .webui-filter-name');
	const preview = document.getElementById('filterPreview');
	if (activeItem && preview) preview.textContent = activeItem.textContent;
	renderTimerList();
	if (window.innerWidth <= 800) selectCard('card-timers');
}

const SVG_EXPAND = '<svg class="webui-timer-expand-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6,9 12,15 18,9"/></svg>';
const SVG_EDIT = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 4H4a2 2 0 00-2 2v14a2 2 0 002 2h14a2 2 0 002-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 013 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>';
const SVG_STOP = '<svg viewBox="0 0 24 24" fill="currentColor" style="color:var(--webui-danger)"><rect x="6" y="6" width="12" height="12" rx="1"/></svg>';
const SVG_DELETE = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3,6 5,6 21,6"/><path d="M19,6v14a2,2 0 0,1-2,2H7a2,2 0 0,1-2-2V6m3,0V4a2,2 0 0,1,2-2h4a2,2 0 0,1,2,2v2"/></svg>';

function renderTimerList() {
	const list = document.getElementById('timerList');
	const countEl = document.getElementById('timerCount');
	const previewEl = document.getElementById('timerPreview');
	const timers = getFilteredTimers();

	// running first, then the nearest start
	timers.sort((a, b) => {
		if (a.state === 2 && b.state !== 2) return -1;
		if (b.state === 2 && a.state !== 2) return 1;
		return a.start - b.start;
	});
	countEl.textContent = timers.length + ' Timer';

	if (timers.length === 0) {
		list.replaceChildren(webuiEl('li', {className: 'webui-empty'}, webuiL('tm.none_found', 'No timers found')));
		previewEl.textContent = webuiL('dash.no_timers_short', 'No timers');
		return;
	}

	const now = Math.floor(Date.now() / 1000);
	const nextTimer = timers.find(t => t.start > now);
	previewEl.textContent = nextTimer ? dateOf(nextTimer.start) + ' ' + clock(nextTimer.start) : timers.length + ' Timer';
	webuiSync(list, timers, t => String(t.id), timerRow);
}

function detail(label, value) {
	return [webuiEl('span', {className: 'webui-timer-detail-label'}, label + ':'),
		webuiEl('span', {className: 'webui-timer-detail-value'}, value)];
}

function timerRow(timer) {
	const typeNum = typeNumber(timer);
	const state = timer.state || 0;
	const channel = names[channelOf(timer)] || '';
	const typeLabel = TIMER_TYPE_LABELS[typeNum] || WEBUI_TIMER_TYPES[typeNum] || String(timer.kind);
	const title = timer.title || channel || typeLabel;
	const stateLabel = TIMER_STATE_LABELS[state] || '';
	const rep = repeatText(timer);
	const timeRange = clock(timer.start) + (timer.stop ? ' - ' + clock(timer.stop) : '');
	const isRecording = typeNum === 5 || typeNum === 9;

	const badges = [];
	if (timer.repeat) badges.push(webuiEl('span', {className: 'webui-timer-badge webui-timer-badge-repeat'}, rep));
	if (state === 2) badges.push(webuiEl('span', {className: 'webui-timer-badge webui-timer-badge-state running'}, stateLabel));
	else if (state >= 3) badges.push(webuiEl('span', {className: 'webui-timer-badge webui-timer-badge-state finished'}, stateLabel));

	const actions = [];
	if (state < 2)
		actions.push(actionButton('webui-btn webui-btn-edit', SVG_EDIT, webuiL('tm.edit', 'Edit'), webuiL('tm.edit_title', 'Edit timer'), () => editTimer(timer.id)));
	if (state === 2 && isRecording)
		actions.push(actionButton('webui-btn webui-btn-stop', SVG_STOP, webuiL('tm.stop', 'Stop'), webuiL('dash.record_stop', 'Stop recording'), () => stopRecording(timer.id, title)));
	else
		actions.push(actionButton('webui-btn', SVG_DELETE, webuiL('fm.delete', 'Delete'), webuiL('tm.delete_title', 'Delete timer'), () => deleteTimer(timer.id, title)));

	const details = [].concat(
		detail(webuiL('tm.type', 'Type'), typeLabel),
		detail('Status', stateLabel),
		detail('Start', dateOf(timer.start) + ' ' + clock(timer.start)),
		timer.stop ? detail(webuiL('tm.end', 'End'), dateOf(timer.stop) + ' ' + clock(timer.stop)) : [],
		timer.announce ? detail(webuiL('tm.announce', 'Announcement'), dateOf(timer.announce) + ' ' + clock(timer.announce)) : [],
		detail(webuiL('tm.repeat', 'Repeat'), rep + (timer.repeat && timer.repeat_count ? ' (' + timer.repeat_count + ')' : '')),
		channel ? detail(webuiL('dash.channels', 'Channels'), channel) : [],
		channelOf(timer) ? detail(webuiL('tm.channel_id', 'Channel ID'), timer.channel_id) : [],
		timer.recording_dir ? detail(webuiL('tm.rec_dir', 'Recording folder'), timer.recording_dir) : [],
		timer.epg_id && timer.epg_id !== '0' ? detail('EPG-ID', timer.epg_id) : [],
		detail('Timer-ID', String(timer.id)));

	return webuiEl('li', {className: 'webui-timer-row' + (state === 2 ? ' state-running' : (state >= 3 ? ' state-finished' : '')),
		data: {id: String(timer.id)}, on: {click: toggleTimerDetail}},
		webuiEl('div', {className: 'webui-timer-main'},
			webuiMarkup(webuiGetTimerIconHtml(WEBUI_TIMER_TYPES[typeNum] || '')),
			webuiMarkup(SVG_EXPAND),
			webuiEl('div', {className: 'webui-timer-row-info'},
				webuiEl('div', {className: 'webui-timer-row-title'}, title),
				webuiEl('div', {className: 'webui-timer-row-meta'},
					channel ? webuiEl('span', {className: 'webui-timer-row-channel'}, channel) : null,
					badges)),
			webuiEl('div', {className: 'webui-timer-row-time'},
				webuiEl('div', {className: 'webui-timer-row-date'}, dateOf(timer.start)),
				webuiEl('div', {className: 'webui-timer-row-hours'}, timeRange)),
			webuiEl('div', {className: 'webui-timer-actions', on: {click: e => e.stopPropagation()}}, actions)),
		webuiEl('div', {className: 'webui-timer-detail', id: 'detail-' + timer.id},
			webuiEl('div', {className: 'webui-timer-detail-grid'}, details)));
}

function actionButton(cls, svg, label, title, onClick) {
	return webuiEl('button', {className: cls, title: title, on: {click: onClick}},
		webuiMarkup(svg), webuiEl('span', {className: 'btn-label'}, label));
}

function toggleTimerDetail(event) {
	if (event.target.closest('.webui-timer-actions')) return;
	const row = event.target.closest('.webui-timer-row');
	if (!row) return;
	const wasExpanded = row.classList.contains('row-expanded');
	document.querySelectorAll('.webui-timer-row.row-expanded').forEach(r => r.classList.remove('row-expanded'));
	if (!wasExpanded) row.classList.add('row-expanded');
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
	if (await ask(webuiL('dash.record_stop', 'Stop recording'), webuiL('bc.recording', 'Recording') + ' "' + title + '" ' + webuiL('tm.stop_confirm', 'really stop?'), webuiL('tm.stop', 'Stop'), true))
		await webuiStopRecording(id);	// the record event reads the list again
}

async function deleteTimer(id, title) {
	if (!await ask(webuiL('tm.delete_title', 'Delete timer'), 'Timer "' + title + '" ' + webuiL('fm.delete_confirm', 'really delete?'), webuiL('fm.delete', 'Delete'), true))
		return;
	try {
		await webuiV1('DELETE', '/timers/' + encodeURIComponent(id));
		await loadTimers();
	} catch (e) {
		webuiToastError('', e);
	}
}

// Initialize: the list follows the timer and record events; without
// an open stream it is read every minute
webuiAfterTexts(function() {
	loadTimers();
	webuiV1Events({
		'timer-changed': loadTimers,
		'record-start': loadTimers,
		'record-stop': loadTimers
	});
	setInterval(() => { if (!webuiV1EventsLive()) loadTimers(); }, 60000);
});
webuiV1('GET', '/channels/current').then(ch => {
	currentMode = ch && (ch.kind === 'radio' || ch.kind === 'webradio') ? 'radio' : 'tv';
}).catch(() => {});

webuiAfterTexts(function() {
	webuiInitMobileAccordion({
		cardOrder: ['card-filter', 'card-timers'],
		storageKey: 'webui-timer-card',
		defaultCard: 'card-timers',
		breakpoint: 800,
		indicatorId: 'cardIndicator',
		containerSelector: '.webui-timer-grid',
		mode: 'collapsed'
	});
});

function isAtScrollTop() {
	const expandedCard = document.querySelector('.webui-card:not(.collapsed)');
	if (expandedCard) {
		const cardBody = expandedCard.querySelector('.webui-card-body');
		if (cardBody && cardBody.scrollTop > 5) return false;
	}
	const main = document.querySelector('.webui-timer-grid');
	if (main && main.scrollTop > 5) return false;
	return true;
}

if (typeof webuiSetRefreshCallback === 'function') {
	webuiSetRefreshCallback(function() {
		if (!isAtScrollTop()) return Promise.resolve();
		return loadTimers();
	});
}

if (typeof window.webuiIsAtScrollTop === 'undefined') {
	window.webuiIsAtScrollTop = isAtScrollTop;
}

// ===== Timer Modal =====
let timerModalEditId = null;
let cachedRecDirs = null;

function buildTimerModalHtml() {
	const now = new Date();
	const todayStr = webuiFormatDateInput(now);
	const nowTime = webuiFormatTime(now);
	const stop = new Date(now.getTime() + 2 * 3600000);
	const stopDate = webuiFormatDateInput(stop);
	const stopTime = webuiFormatTime(stop);

	return `
	<div class="webui-modal-header">
		<h3 id="timerModalTitle">${webuiLH('tm.new', 'New timer')}</h3>
		<button class="webui-modal-close" onclick="closeTimerModal()">
			<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 6L6 18M6 6l12 12"/></svg>
		</button>
	</div>
	<div class="webui-modal-body">
		<div class="webui-form-section">
			<div class="webui-form-row">
				<label for="tmType">${webuiLH('tm.type', 'Type')}</label>
				<select id="tmType" onchange="onTimerTypeChange()" data-fixed>
					<option value="5">${webuiLH('bc.recording', 'Recording')}</option>
					<option value="3">${webuiLH('tm.zap', 'Zap')}</option>
					<option value="4">Standby</option>
					<option value="6">${webuiLH('tm.remind', 'Reminder')}</option>
					<option value="7">Sleeptimer</option>
					<option value="8">Plugin</option>
					<option value="1">Shutdown</option>
				</select>
			</div>
		</div>

		<div class="webui-form-section">
			<div class="webui-form-section-title">Zeiten</div>
			<div class="webui-form-row">
				<label for="tmAlDate">Start</label>
				<div class="webui-time-group">
					<input type="date" id="tmAlDate" value="${todayStr}">
					<input type="time" id="tmAlTime" value="${nowTime}">
				</div>
			</div>
			<div class="webui-form-row" id="tmStopRow">
				<label for="tmStDate">${webuiLH('tm.end', 'End')}</label>
				<div class="webui-time-group">
					<input type="date" id="tmStDate" value="${stopDate}">
					<input type="time" id="tmStTime" value="${stopTime}">
				</div>
			</div>
		</div>

		<div class="webui-form-section">
			<div class="webui-form-section-title">${webuiLH('tm.repeat', 'Repeat')}</div>
			<div class="webui-form-row">
				<label for="tmRepeat">${webuiLH('bqe.mode', 'Mode')}</label>
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
			<div class="webui-form-row webui-form-hidden" id="tmWeekdaysRow">
				<label>${webuiLH('tm.days', 'Days')}</label>
				<div class="webui-form-weekdays">
					<label><input type="checkbox" name="tmWd" value="512"><span>Mo</span></label>
					<label><input type="checkbox" name="tmWd" value="1024"><span>Di</span></label>
					<label><input type="checkbox" name="tmWd" value="2048"><span>Mi</span></label>
					<label><input type="checkbox" name="tmWd" value="4096"><span>Do</span></label>
					<label><input type="checkbox" name="tmWd" value="8192"><span>Fr</span></label>
					<label><input type="checkbox" name="tmWd" value="16384"><span>Sa</span></label>
					<label><input type="checkbox" name="tmWd" value="32768"><span>So</span></label>
				</div>
			</div>
			<div class="webui-form-row webui-form-hidden" id="tmRepcountRow">
				<label for="tmRepcount">${webuiLH('tm.count', 'Count')}</label>
				<input type="number" id="tmRepcount" min="0" max="999" value="0" placeholder="${webuiLH('tm.unlimited', '0 = unlimited')}">
			</div>
		</div>

		<div class="webui-form-section" id="tmChannelSection">
			<div class="webui-form-section-title">${webuiLH('msg.channel', 'Channel')}</div>
			<div class="webui-form-row">
				<label for="tmBouquet">Bouquet</label>
				<select id="tmBouquet" onchange="onBouquetChange()" data-fixed>
					<option value="">${webuiLH('msg.loading', 'Loading...')}</option>
				</select>
			</div>
			<div class="webui-form-row">
				<label for="tmChannel">${webuiLH('dash.channels', 'Channels')}</label>
				<select id="tmChannel" data-fixed>
					<option value="">${webuiLH('timer.choose_bq', 'Choose bouquet...')}</option>
				</select>
			</div>
		</div>

		<div class="webui-form-section" id="tmRecordSection">
			<div class="webui-form-section-title">Aufnahme-Optionen</div>
			<div class="webui-form-row">
				<label for="tmRecDir">${webuiLH('fm.folder', 'Folder')}</label>
				<select id="tmRecDir" data-fixed onchange="onRecDirChange(this)">
					<option value="">Standard</option>
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
			<div class="webui-form-section-title">Standby</div>
			<div class="webui-form-row">
				<label>${webuiLH('bqe.mode', 'Mode')}</label>
				<select id="tmStandbyOn" data-fixed>
					<option value="1">Einschalten</option>
					<option value="0">Ausschalten</option>
				</select>
			</div>
		</div>

		<div class="webui-form-section webui-form-hidden" id="tmMessageSection">
			<div class="webui-form-section-title">Erinnerung</div>
			<div class="webui-form-row">
				<label for="tmMessage">${webuiLH('tools.message', 'Message')}</label>
				<input type="text" id="tmMessage" maxlength="30" placeholder="Nachricht eingeben..." data-fixed>
			</div>
		</div>

		<div class="webui-form-section webui-form-hidden" id="tmPluginSection">
			<div class="webui-form-section-title">Plugin</div>
			<div class="webui-form-row">
				<label for="tmPlugin">Name</label>
				<input type="text" id="tmPlugin" maxlength="30" placeholder="Plugin-Name" data-fixed>
			</div>
		</div>
	</div>
	<div class="webui-modal-footer">
		<button onclick="closeTimerModal()">${webuiLH('tm.cancel', 'Cancel')}</button>
		<button class="btn-primary" id="tmSubmitBtn" onclick="submitTimer()">${webuiLH('bqe.save', 'Save')}</button>
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
		document.getElementById('timerModalTitle').textContent = 'Timer bearbeiten';
		document.getElementById('tmSubmitBtn').textContent = webuiL('tm.change', 'Change');
		populateEditData(editTimer);
	}
	backdrop.addEventListener('keydown', function(e) {
		if (e.key === 'Escape') closeTimerModal();
	});
}

function closeTimerModal() {
	const backdrop = document.getElementById('timerModalBackdrop');
	if (backdrop) backdrop.remove();
	timerModalEditId = null;
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
		const items = [webuiEl('option', {value: ''}, 'Standard')]
			.concat(cachedRecDirs.map(dir => webuiEl('option', {value: dir}, dir)));
		if (cachedRecPartial)
			items.push(webuiEl('option', {value: '', disabled: true}, '\u2026 ' + webuiL('tm.dir_cut', 'the box shortened this list')));
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
			{label: 'OK', primary: true, onClick: function() {
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

async function submitTimer() {
	const btn = document.getElementById('tmSubmitBtn');
	if (!btn) return;
	const type = document.getElementById('tmType').value;
	const alDate = document.getElementById('tmAlDate').value;
	const alTime = document.getElementById('tmAlTime').value;
	const stDate = document.getElementById('tmStDate').value;
	const stTime = document.getElementById('tmStTime').value;

	if (!alDate || !alTime) {
		webuiShowToast(webuiL('tm.need_start', 'Please set a start time'), true);
		return;
	}
	const needsStop = type === '5';
	if (needsStop && (!stDate || !stTime)) {
		webuiShowToast(webuiL('tm.need_end', 'Please set an end time'), true);
		return;
	}
	const needsChannel = type === '5' || type === '3';
	const channelId = document.getElementById('tmChannel') ? document.getElementById('tmChannel').value : '';
	if (!timerModalEditId && needsChannel && !channelId) {
		webuiShowToast(webuiL('tm.need_channel', 'Please pick a channel'), true);
		return;
	}

	const repeat = repeatValue();
	if (repeat === null) {
		webuiShowToast(webuiL('tm.need_day', 'Please tick at least one day'), true);
		return;
	}
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

	btn.textContent = '...';
	btn.disabled = true;
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
		webuiToastError(webuiL('tm.save_fail', 'Saving failed'), e);
		btn.textContent = timerModalEditId ? webuiL('tm.change', 'Change') : webuiL('bqe.save', 'Save');
		btn.disabled = false;
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
	if (cSel) options(cSel, [webuiEl('option', {value: channelId}, names[channelId] || channelId)]);
}
