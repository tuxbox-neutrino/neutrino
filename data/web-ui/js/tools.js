// The script of tools.html.

// M5-Komponenten: 14px-Icons fuer die Dialog-Buttons (Optik wie
// Settings-Panels)
function sIcon(p) { return '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' + p + '<\/svg>'; }
var I_REFRESH = sIcon('<polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/>');
var I_ROTATE = sIcon('<polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/>');
var I_POWER = sIcon('<path d="M18.36 6.64a9 9 0 1 1-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/>');
var I_REC = sIcon('<circle cx="12" cy="12" r="7"/>');
var I_STOP = sIcon('<rect x="6" y="6" width="12" height="12" rx="1"/>');
var I_LOCK = sIcon('<rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>');
var I_UNLOCK = sIcon('<rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 9.9-1"/>');

// A box action over /api/v1 ('POST /system/reboot')
async function bcDo(label, endpoint, params) {
	try {
		var m = /^(POST|PUT) (\/.*)$/.exec(endpoint);
		await webuiV1(m[1], m[2], params ? {body: params} : {});
		webuiShowToast(label + ' ' + webuiL('msg.executed', 'done'));
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
}

// Rueckfrage als webui-Dialog statt nativem confirm(); der
// Bestaetigen-Knopf wiederholt den Aktionsnamen (label)
// label and confirmText come as keys of the texts (a plain word where
// there is none), so no text travels through the markup of a button
function bcAction(label, endpoint, params, confirmText) {
	label = webuiL(label, label);
	if (confirmText)
		confirmText = webuiLH(confirmText, confirmText);	// the dialog's message is markup
	if (confirmText) {
		webuiShowConfirm(label, confirmText, label, function() { return bcDo(label, endpoint, params); });
		return;
	}
	return bcDo(label, endpoint, params);
}

async function showBoxControl() {
	var body =
		'<div class="webui-form-section"><div class="webui-form-section-title">Box</div>' +
		'<div class="webui-form-row"><label>' + webuiLH('bc.restart_gui', 'Restart Neutrino GUI') + '</label><span class="webui-row-actions">' +
		'<button type="button" class="webui-btn" onclick="bcAction(\'bc.restart_btn\', \'POST /system/restart\', null, \'bc.restart_confirm\')">' + I_REFRESH + webuiLH('bc.restart_btn', 'Restart GUI') + '</button></span></div>' +
		'<div class="webui-form-row"><label>' + webuiLH('bc.reboot', 'Reboot the box') + '</label><span class="webui-row-actions">' +
		'<button type="button" class="webui-btn danger" onclick="bcAction(\'Reboot\', \'POST /system/reboot\', null, \'bc.reboot_confirm\')">' + I_ROTATE + 'Reboot</button></span></div>' +
		'<div class="webui-form-row"><label>' + webuiLH('bc.shutdown', 'Power off the box') + '</label><span class="webui-row-actions">' +
		'<button type="button" class="webui-btn danger" onclick="bcAction(\'Shutdown\', \'POST /system/shutdown\', null, \'bc.shutdown_confirm\')">' + I_POWER + webuiLH('bc.shutdown_btn', 'Power off') + '</button></span></div></div>' +
		'<div class="webui-form-section"><div class="webui-form-section-title">' + webuiLH('bc.recording', 'Recording') + ' <small id="bcRecState" style="color:var(--pico-muted-color)"></small></div>' +
		'<div class="webui-form-row"><label>' + webuiLH('bc.instant_rec', 'Instant recording') + '</label><span class="webui-row-actions">' +
		'<button type="button" class="webui-btn" onclick="bcRecStart()">' + I_REC + 'Start</button> ' +
		'<button type="button" class="webui-btn" onclick="bcRecStop()">' + I_STOP + 'Stop</button></span></div></div>' +
		'<div class="webui-form-section" id="bcVideo" hidden><div class="webui-form-section-title">Video</div>' +
		'<div class="webui-form-row"><label for="bcVideoMode">' + webuiLH('bc.resolution', 'Resolution') + '</label><span class="webui-row-actions">' +
		'<select id="bcVideoMode"></select> ' +
		'<button type="button" class="webui-btn" onclick="bcVideoApply()">' + webuiLH('bqe.apply', 'Apply') + '</button></span></div></div>' +
		'<div class="webui-form-section"><div class="webui-form-section-title">' + webuiLH('tools.remote_sub', 'Remote control') + '</div>' +
		'<div class="webui-form-row"><label>' + webuiLH('bc.rc_at_box', 'RC at the box') + '</label><span class="webui-row-actions">' +
		'<button type="button" class="webui-btn" onclick="bcAction(\'bc.lock\', \'PUT /osd/remote\', {locked: true})">' + I_LOCK + webuiLH('bc.lock', 'Lock') + '</button> ' +
		'<button type="button" class="webui-btn" onclick="bcAction(\'bc.unlock\', \'PUT /osd/remote\', {locked: false})">' + I_UNLOCK + webuiLH('bc.unlock', 'Unlock') + '</button></span></div></div>' +
		'<div class="webui-form-section"><div class="webui-form-section-title">Zapit</div>' +
		'<div class="webui-form-row"><label>' + webuiLH('bc.reload_channels', 'Reload channel lists') + '</label><span class="webui-row-actions">' +
		'<button type="button" class="webui-btn" onclick="bcAction(\'bc.reload\', \'POST /channels/reload\', null)">' + I_REFRESH + webuiLH('bc.reload', 'Reload') + '</button></span></div>' +
		'<div class="webui-form-row"><label>' + webuiLH('bc.tuner_reset', 'Tuner reset') + '</label><span class="webui-row-actions">' +
		'<button type="button" class="webui-btn" onclick="bcAction(\'bc.tuner_reset\', \'POST /tuner/reset\', null)">' + I_ROTATE + 'Reset</button></span></div></div>';
	webuiModal('bcModal', 'Boxcontrol', body);
	bcRecState();
	bcVideoLoad();
}

// What the box records besides its shift, after GET /api/v1/recordings
async function bcRecState() {
	var el = document.getElementById('bcRecState');
	try {
		var r = await webuiV1('GET', '/recordings');
		var on = ((r && r.items) || []).some(function(it) { return !it.timeshift; });
		if (el) el.textContent = on ? '\u2022 ' + webuiL('bc.rec_running', 'recording running') : '\u2022 ' + webuiL('bc.rec_none', 'no recording');
	} catch (e) {
		if (el) el.textContent = '';
	}
}

// The record key's recording: of the channel the box shows, after asking
// whether a box in standby is to be switched on for it. The box begins it
// after the answer, so the state is read again a moment later.
async function bcRecStart() {
	try {
		var ok = await webuiV1Lifting('/recordings', {}, {
			title: webuiL('bc.instant_rec', 'Instant recording'),
			ok: 'Start',
			stop: '',
			wake: webuiL('bc.rec_wake', 'The box is in standby. Switch it on and record what it shows?')
		});
		if (!ok) return;
		webuiShowToast(webuiL('bc.recording', 'Recording') + '-Start ' + webuiL('msg.executed', 'done'));
		setTimeout(bcRecState, 1500);
	} catch (e) {
		webuiShowToast(e.code === 'recording-running' ? webuiL('bc.rec_already', 'The channel the box shows is being recorded already') :
			webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
}

// Ends the recording of the channel the box shows, else the first that
// is not its shift, as the old stop did
async function bcRecStop() {
	var id = null;
	try {
		var lists = await Promise.all([webuiV1('GET', '/recordings'), webuiV1('GET', '/channels/current')]);
		var live = lists[1] && lists[1].id ? webuiLegacyChannelId(lists[1].id) : '';
		var mine = ((lists[0] && lists[0].items) || []).find(function(it) {
			return !it.timeshift && webuiLegacyChannelId(it.channel_id) === live;
		});
		if (mine) id = mine.id;
	} catch (e) {}
	await webuiStopRecording(id);
	bcRecState();
}

// The video modes this box offers, from its settings schema, with the one
// it uses selected; the row stays hidden on a box that offers none
var bcVideoSection = null;
async function bcVideoLoad() {
	try {
		var schema = await webuiV1('GET', '/settings/schema?section=video');
		var item = ((schema && schema.items) || []).find(function(it) { return it.id === 'video_Mode'; });
		// a list several settings share is stated once, beside the items
		var lists = (schema && schema.value_lists) || {};
		if (item && item.values_from && Array.isArray(lists[item.values_from]))
			item.values = lists[item.values_from];
		if (!item || !(item.values && item.values.length)) return;
		var now = await webuiV1('GET', '/settings/' + encodeURIComponent(item.section));
		var cur = ((now && now.items) || []).find(function(it) { return it.id === 'video_Mode'; });
		var sel = document.getElementById('bcVideoMode');
		if (!sel) return;
		sel.replaceChildren();
		item.values.forEach(function(c) {
			sel.appendChild(webuiEl('option', {value: String(c.value)}, c.label || String(c.value)));
		});
		if (cur) sel.value = String(cur.value);
		bcVideoSection = item.section;
		document.getElementById('bcVideo').hidden = false;
	} catch (e) {}
}

// The box switches to the chosen mode when the setting is written
async function bcVideoApply() {
	var sel = document.getElementById('bcVideoMode');
	if (!sel || !bcVideoSection) return;
	var label = sel.options[sel.selectedIndex] ? sel.options[sel.selectedIndex].text : sel.value;
	try {
		var answer = await webuiV1('PATCH', '/settings/' + encodeURIComponent(bcVideoSection), {body: {video_Mode: sel.value}});
		var r = answer && answer.results && answer.results.video_Mode;
		if (r && r.status >= 300)
			throw new Error(r.detail || r.code || String(r.status));
		webuiShowToast(webuiL('bc.resolution', 'Resolution') + ': ' + label);
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
}
