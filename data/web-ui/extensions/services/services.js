// The script of the services' page (extension services).

/* The services as people know them. A name the box answers that is not here
   shows as it comes, without a line under it. */
var SVC_NAMES = {
	nfs: 'NFS', samba: 'Samba', crond: 'Cron', dropbear: 'Dropbear',
	inadyn: 'Inadyn', ushare: 'uShare', djmount: 'djmount',
	minidlna: 'MiniDLNA', xupnpd: 'xupnpd',
	fritzcallmonitor: 'FritzCallMonitor', tuxcald: 'TuxCal', tuxmaild: 'TuxMail'
};

/* The three things a service can be asked to do, as the box names them. All
   three on every row: the box says whether a process of the name runs, not
   which of them a service carries, and one started through a wrapper runs
   under another name, so a button left out on a guess could be the one that
   was needed. What a service does not carry, the box refuses and says why. */
var SVC_VERBS = [
	{verb: 'start', key: 'svc.start', text: 'Start'},
	{verb: 'stop', key: 'svc.stop', text: 'Stop'},
	{verb: 'restart', key: 'svc.restart', text: 'Restart'}
];

// the names a request is on its way for, so their buttons wait
var svcBusy = {};

function svcTitle(id) {
	return Object.prototype.hasOwnProperty.call(SVC_NAMES, id) ? SVC_NAMES[id] : id;
}

// what a service does, from the pack, or nothing for a name it does not know
function svcWhat(id) {
	var key = 'svc.d.' + id;
	var text = webuiL(key, key);
	return text === key ? null : text;
}

function svcText(d) {
	var what = svcWhat(d.id);
	return webuiEl('div', {className: 'svc-text'},
		webuiEl('div', {className: 'svc-name'}, svcTitle(d.id), ' ',
			webuiEl('small', {className: 'svc-id'}, d.id)),
		what && webuiEl('div', {className: 'svc-desc'}, what));
}

// one service this box carries: its state and the three buttons
function svcRow(d) {
	var row = webuiEl('div', {className: 'svc-row', data: {name: d.id}},
		webuiEl('span', {className: 'svc-dot', attrs: {'aria-hidden': 'true'}}),
		svcText(d),
		webuiEl('span', {className: 'svc-state'}),
		webuiEl('div', {className: 'svc-actions'}, SVC_VERBS.map(function(v) {
			return webuiEl('button', {type: 'button', className: 'webui-btn', data: {verb: v.verb},
				attrs: {'aria-label': webuiL(v.key, v.text) + ' ' + svcTitle(d.id)},
				on: {click: function() { svcDo(d.id, v.verb); }}}, webuiL(v.key, v.text));
		})));
	svcUpdate(row, d);
	return row;
}

// brings a row to what the box said, in place, so a focused button stays
function svcUpdate(row, d) {
	row.classList.toggle('running', !!d.running);
	row.querySelector('.svc-state').textContent = d.running ?
		webuiL('svc.running', 'running') : webuiL('svc.stopped', 'not running');
	var busy = !!svcBusy[d.id];
	row.setAttribute('aria-busy', busy ? 'true' : 'false');
	row.querySelectorAll('button').forEach(function(b) { b.disabled = busy; });
}

// one service the image was built without: named, so nobody goes looking
function svcMissingRow(d) {
	return webuiEl('div', {className: 'svc-row svc-missing', data: {name: d.id}}, svcText(d));
}

var svcLast = [];

function svcShow(items) {
	svcLast = items;
	var here = items.filter(function(d) { return d.installed; });
	var missing = items.filter(function(d) { return !d.installed; });
	var list = document.getElementById('svc-list');
	webuiSync(list, here, function(d) { return d.id; }, svcRow, svcUpdate);
	document.getElementById('svc-none').hidden = here.length > 0;
	webuiSync(document.getElementById('svc-missing-list'), missing,
		function(d) { return d.id; }, svcMissingRow);
	document.getElementById('svc-missing').hidden = missing.length === 0;
	document.getElementById('svc-missing-count').textContent = String(missing.length);
}

// true where the box answered; otherwise the rows stay as they were read last
async function svcLoad() {
	var note = document.getElementById('svc-note');
	try {
		var r = await webuiV1('GET', '/daemons');
		note.hidden = true;
		svcShow((r && r.items) || []);
		return true;
	} catch (e) {
		note.hidden = false;
		note.removeAttribute('data-l');
		note.textContent = webuiL('msg.error', 'Error') + ': ' + webuiErrText(e);
		return false;
	}
}

// the button that sent a verb gets the focus back once its row is free,
// unless the focus went somewhere else meanwhile
function svcRefocus(id, verb) {
	var at = document.activeElement;
	if (at && at !== document.body) return;
	document.querySelectorAll('#svc-list .svc-row').forEach(function(row) {
		var b = row.dataset.name === id && row.querySelector('button[data-verb="' + verb + '"]');
		if (b) b.focus();
	});
}

/* Sends one verb and reads the list again, which is what answers whether the
   service runs now: the box says the command returned, not that the service
   came up, and one that dies a moment later was started and is not running.
   So once more a little later. */
async function svcDo(id, verb) {
	if (svcBusy[id]) return;
	svcBusy[id] = true;
	svcShow(svcLast);
	try {
		await webuiV1('POST', '/daemons/' + encodeURIComponent(id) + '/' + verb);
	} catch (e) {
		delete svcBusy[id];
		svcShow(svcLast);
		svcRefocus(id, verb);
		webuiToastError(svcTitle(id), e);
		return;
	}
	await svcLoad();
	await new Promise(function(done) { setTimeout(done, 2000); });
	delete svcBusy[id];
	// the buttons are free again whatever the read answers; what runs is
	// said only from a read that came
	var read = await svcLoad();
	if (!read)
		svcShow(svcLast);
	svcRefocus(id, verb);
	if (!read)
		return;
	var now = svcLast.filter(function(d) { return d.id === id; })[0];
	if (now)
		webuiShowToast((now.running ? webuiL('svc.now_running', '%s is running.') :
			webuiL('svc.now_stopped', '%s is not running.')).replace('%s', svcTitle(id)));
}

webuiExtTexts('services').then(svcLoad);
