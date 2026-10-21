// The script of tools.html: the box control.

// A box action over /api/v1 ('POST /channels/reload')
async function bcDo(label, endpoint, params) {
	try {
		var m = /^(POST|PUT) (\/.*)$/.exec(endpoint);
		await webuiV1(m[1], m[2], params ? {body: params} : {});
		webuiShowToast(label + ': ' + webuiL('msg.executed', 'done'));
	} catch (e) {
		webuiToastError(label, e);
	}
}

// label comes as a key of the texts, so no text travels through the
// markup of a button
function bcAction(label, endpoint, params) {
	return bcDo(webuiL(label, label), endpoint, params);
}

/* Restarting Neutrino or the box, or switching it off: asked first, then a
   window that waits for the box to answer again and loads the page anew,
   or says the box is off. The web server goes with Neutrino, so the wait
   asks the box itself, not through the sign-in: any answer counts. */
var BC_RESTARTS = {
	gui: {endpoint: '/system/restart', title: ['bc.restart_gui', 'Restart Neutrino GUI'], ask: ['bc.restart_confirm', 'Really restart the GUI?'],
		button: ['bc.restart_btn', 'Restart GUI'], wait: ['bc.gui_wait', 'Neutrino is restarting']},
	box: {endpoint: '/system/reboot', title: ['bc.reboot', 'Reboot the box'], ask: ['bc.reboot_confirm', 'Really reboot the box?'],
		button: ['bc.reboot_btn', 'Reboot'], wait: ['bc.box_wait', 'The box is restarting']},
	off: {endpoint: '/system/shutdown', title: ['bc.shutdown', 'Power off the box'], ask: ['bc.shutdown_confirm', 'Really power off the box?'],
		button: ['bc.shutdown_btn', 'Power off']}
};

function bcRestart(kind) {
	var t = BC_RESTARTS[kind];
	webuiShowConfirm(webuiL(t.title[0], t.title[1]), webuiLH(t.ask[0], t.ask[1]), webuiL(t.button[0], t.button[1]), async function() {
		var before = await bcUptime();
		var drops = bcStreamDrops;
		await webuiV1('POST', t.endpoint);	// a refusal stays in the question
		setTimeout(function() { bcWait(kind, before, drops); }, 0);
	});
}

/* The event stream goes down with the web server, which runs inside
   Neutrino: a restart of the GUI too quick for the polls below to find the
   box away, and which leaves the box's uptime as it was, is seen there. */
var bcStreamDrops = 0;

// How long the box has been up, or null where it does not say
async function bcUptime() {
	try {
		var r = await fetch('/api/v1/system/info', {credentials: 'same-origin', cache: 'no-store'});
		var d = r.ok ? await r.json() : null;
		return d && typeof d.uptime === 'number' ? d.uptime : null;
	} catch (e) {
		return null;
	}
}

// An answer of the box at all, within three seconds: {up, uptime}
async function bcAsk() {
	var ctl = typeof AbortController === 'function' ? new AbortController() : null;
	var stop = ctl ? setTimeout(function() { ctl.abort(); }, 3000) : null;
	try {
		var r = await fetch('/api/v1/system/info', {credentials: 'same-origin', cache: 'no-store', signal: ctl ? ctl.signal : undefined});
		var d = r.ok ? await r.json().catch(function() { return null; }) : null;
		return {up: true, uptime: d && typeof d.uptime === 'number' ? d.uptime : null};
	} catch (e) {
		return {up: false, uptime: null};
	} finally {
		if (stop) clearTimeout(stop);
	}
}

function bcShowWait(title, state) {
	var modal = webuiModal('bcWait', title, '');
	modal.classList.add('webui-bc-wait');
	modal.querySelector('.webui-epg-modal-body').replaceChildren(state);
}

// the wait under way; one closed by the user, or followed by another, ends
var bcWaitRun = 0;

async function bcWait(kind, before, drops) {
	var t = BC_RESTARTS[kind];
	var run = ++bcWaitRun;
	var live = function() {
		var modal = document.getElementById('bcWait');
		return run === bcWaitRun && !!modal && modal.classList.contains('open');
	};
	if (kind === 'off') {
		bcShowWait(webuiL(t.title[0], t.title[1]), webuiState('empty', webuiL('bc.off_title', 'The box is switching off'),
			webuiL('bc.off_text', 'It is switched on again at the box or with its remote control.')));
		return;
	}
	var title = webuiL(t.title[0], t.title[1]);
	bcShowWait(title, webuiState('loading', webuiL(t.wait[0], t.wait[1]), webuiL('bc.wait_text', 'The page waits until the box answers again, and then loads anew.')));
	var away = false;
	var answered = false;
	var until = Date.now() + 180000;
	while (Date.now() < until) {
		await new Promise(function(done) { setTimeout(done, 2000); });
		if (!live())
			return;
		var a = await bcAsk();
		if (!live())
			return;
		if (!a.up || bcStreamDrops > drops)
			away = true;
		if (!a.up)
			continue;
		answered = true;
		// back: after it was away, or up for less time than before
		if (away || (before !== null && a.uptime !== null && a.uptime < before)) {
			bcShowWait(title, webuiState('empty', webuiL('bc.back', 'The box is back'), webuiL('bc.back_text', 'The page loads anew.')));
			setTimeout(function() { if (live()) location.reload(); }, 1500);
			return;
		}
	}
	// answering all along, and no sign of the restart: say that, not a failure
	if (answered)
		bcShowWait(title, webuiState('empty', webuiL('bc.answers', 'The box answers'),
			webuiL('bc.answers_text', 'Whether it restarted could not be seen from here.'),
			{retry: function() { location.reload(); }}));
	else
		bcShowWait(title, webuiState('error', webuiL('bc.not_back', 'The box has not answered for three minutes'), '',
			{retry: function() { location.reload(); }}));
}

// What the box records besides its shift, after GET /api/v1/recordings:
// a pill beside the card's title
async function bcRecState() {
	var el = document.getElementById('bcRecState');
	if (!el) return;
	try {
		var r = await webuiV1('GET', '/recordings');
		var on = ((r && r.items) || []).some(function(it) { return !it.timeshift; });
		el.hidden = false;
		el.className = 'webui-pill' + (on ? ' rec' : '');
		el.textContent = on ? webuiL('bc.rec_running', 'recording running') : webuiL('bc.rec_none', 'no recording');
	} catch (e) {
		el.hidden = true;
	}
}

// The record key's recording: of the channel the box shows, after asking
// whether a box in standby is to be switched on for it. The box begins it
// after the answer, so the state is read again a moment later.
async function bcRecStart() {
	try {
		var ok = await webuiV1Lifting('/recordings', {}, {
			title: webuiL('bc.instant_rec', 'Instant recording'),
			ok: webuiL('bc.rec_start', 'Start'),
			stop: '',
			wake: webuiL('bc.rec_wake', 'The box is in standby. Switch it on and record what it shows?')
		});
		if (!ok) return;
		webuiShowToast(webuiL('bc.rec_started', 'Recording started'));
		setTimeout(bcRecState, 1500);
	} catch (e) {
		if (e.code === 'recording-running')
			webuiShowToast(webuiL('bc.rec_already', 'The channel the box shows is being recorded already'), true);
		else
			webuiToastError(webuiL('bc.instant_rec', 'Instant recording'), e);
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
// it uses selected; the card stays hidden on a box that offers none
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
			throw webuiResultError(r);
		webuiShowToast(webuiL('bc.resolution', 'Resolution') + ': ' + label);
	} catch (e) {
		webuiToastError(webuiL('bc.resolution', 'Resolution'), e);
	}
}

webuiAfterTexts(function() {
	bcRecState();
	bcVideoLoad();
	webuiV1Events({'record-start': bcRecState, 'record-stop': bcRecState,
		error: function() { bcStreamDrops++; }});
});
