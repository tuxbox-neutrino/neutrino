// The script of the network filesystems' page (extension netfs).

/* The box's two files for network filesystems, as /api/v1/storage/netfs names
   them: what is mounted when the box starts (/var/etc/fstab), and what the
   automounter offers under /mnt/autofs once somebody looks there
   (/var/etc/auto.net). Eight slots each, the empty ones among them. */
var NETFS_TABLES = ['fstab', 'automount'];
// the file shown; every form and every request carries its own, so one
// that answers after a switch is dropped instead of landing in the other
var netfsTable = 'fstab';
// counts the reads of a table, so only the last one asked for is drawn
var netfsShown = 0;
// where something is mounted now, from /api/v1/storage/mounts
var netfsMounted = {};
// the table as last answered
var netfsLast = null;

var NETFS_CHEVRON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>';
var NETFS_FOLDER = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle"><path d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z"/></svg>';

// where an entry lands: a directory for the first file, a name under
// /mnt/autofs for the second
function netfsWhere(table, e) {
	var where = table === 'automount' ? '/mnt/autofs/' + e.local_dir : e.local_dir;
	// as the kernel names a mount point: without a slash at the end
	return where.length > 1 ? where.replace(/\/+$/, '') : where;
}

function netfsSummary(table, e) {
	if (!e.host)
		return webuiL('netfs.empty', 'empty');
	var state = !e.active ? webuiL('netfs.inactive', 'inactive') :
		netfsMounted[netfsWhere(table, e)] ? webuiL('netfs.mounted', 'mounted') :
		webuiL('netfs.not_mounted', 'not mounted');
	return e.host + ':' + e.remote_dir + ' → ' + netfsWhere(table, e) + ' · ' + state;
}

// a row of the form; forId ties the label to its control, and a row
// without a caption keeps the place of one without being a label
function netfsRow(label, control, forId) {
	return webuiEl('div', {className: 'webui-form-row'},
		label ? webuiEl('label', {htmlFor: forId || null}, label) : webuiEl('span', {}), control);
}

function netfsFields(table, e) {
	var i = e.slot;
	var id = function(key) { return 'netfs_' + key + '_' + i; };
	var input = function(key, value, type) {
		return webuiEl('input', {type: type || 'text', id: id(key), value: value || ''});
	};
	var type = webuiEl('select', {id: id('type')},
		webuiEl('option', {value: 'nfs'}, 'NFS'),
		webuiEl('option', {value: 'cifs'}, 'CIFS (Windows, Samba)'));
	type.value = e.type || 'nfs';
	var pass = input('password', '', 'password');
	pass.autocomplete = 'new-password';
	if (e.has_password)
		pass.placeholder = webuiL('netfs.pass_kept', 'stored – leave empty to keep it');
	var credentials = webuiEl('div', {className: 'netfs-cifs' + (type.value === 'cifs' ? ' visible' : '')},
		netfsRow(webuiL('netfs.user', 'User'), input('user', e.user), id('user')),
		netfsRow(webuiL('netfs.password', 'Password'), pass, id('password')),
		e.has_password ? netfsRow('', webuiEl('small', {className: 'netfs-note'},
			webuiL('netfs.pass_note', 'A stored password comes off when the entry is emptied.'))) : null);
	type.addEventListener('change', function() {
		credentials.classList.toggle('visible', type.value === 'cifs');
	});
	var local = input('local_dir', e.local_dir);
	var where = table === 'automount' ? local : webuiEl('span', {className: 'netfs-pick'}, local, ' ',
		webuiEl('button', {type: 'button', className: 'webui-btn', title: webuiL('netfs.pick', 'Choose folder'),
			attrs: {'aria-label': webuiL('netfs.pick', 'Choose folder')},
			on: {click: function() {
				webuiShowPathPicker(local.value || '/media', function(p) { local.value = p; });
			}}}, webuiMarkup(NETFS_FOLDER)));
	var active = webuiEl('input', {type: 'checkbox', id: id('active'), checked: !!e.active});
	return [
		netfsRow(webuiL('netfs.active', 'Active'), webuiEl('label', {className: 'webui-toggle'},
			active, webuiEl('span', {className: 'webui-toggle-slider'})), id('active')),
		netfsRow(webuiL('netfs.type', 'Type'), type, id('type')),
		netfsRow(webuiL('netfs.host', 'Server'), input('host', e.host), id('host')),
		netfsRow(webuiL('netfs.remote_dir', 'Directory or share on the server'), input('remote_dir', e.remote_dir), id('remote_dir')),
		netfsRow(table === 'automount' ? webuiL('netfs.local_name', 'Name under /mnt/autofs') :
			webuiL('netfs.local_dir', 'Local directory'), where, id('local_dir')),
		netfsRow(webuiL('netfs.options', 'Options'), input('options', e.options), id('options')),
		credentials,
		netfsRow('', webuiEl('span', {className: 'webui-row-actions'},
			webuiEl('button', {type: 'button', className: 'webui-btn primary',
				on: {click: function() { netfsSave(table, i); }}}, webuiL('netfs.save', 'Save')), ' ',
			e.host ? webuiEl('button', {type: 'button', className: 'webui-btn netfs-clear',
				on: {click: function() { netfsClear(table, i); }}}, webuiL('netfs.clear', 'Empty')) : null))
	];
}

function netfsSlot(table, e, open) {
	var slot = webuiEl('div', {className: 'netfs-slot' + (open ? ' open' : ''), id: 'netfs-slot-' + e.slot});
	var head = webuiEl('button', {type: 'button', className: 'netfs-slot-head',
		attrs: {'aria-expanded': open ? 'true' : 'false', 'aria-controls': 'netfs_body_' + e.slot},
		on: {click: function() {
			var now = slot.classList.toggle('open');
			head.setAttribute('aria-expanded', now ? 'true' : 'false');
		}}},
		// counted from one, as people count; the box numbers its entries not
		webuiEl('span', {}, webuiL('netfs.entry', 'Entry') + ' ' + (e.slot + 1) + ' ',
			webuiEl('span', {className: 'netfs-summary', id: 'netfs_summary_' + e.slot}, netfsSummary(table, e))),
		webuiMarkup(NETFS_CHEVRON));
	slot.append(head, webuiEl('fieldset', {className: 'netfs-slot-body', id: 'netfs_body_' + e.slot}, netfsFields(table, e)));
	return slot;
}

// the tabs as the table shown: one stop in the tab order, the panel named
function netfsTabs() {
	document.querySelectorAll('[data-netfs-table]').forEach(function(b) {
		var on = b.getAttribute('data-netfs-table') === netfsTable;
		b.setAttribute('aria-selected', on ? 'true' : 'false');
		b.tabIndex = on ? 0 : -1;
		if (on)
			document.getElementById('netfs-slots').setAttribute('aria-labelledby', b.id);
	});
	document.getElementById('netfs-fstab-actions').hidden = netfsTable !== 'fstab';
	document.getElementById('netfs-automount-actions').hidden = netfsTable !== 'automount';
}

async function netfsReadMounts() {
	netfsMounted = {};
	try {
		var m = await webuiV1('GET', '/storage/mounts');
		((m && m.items) || []).forEach(function(x) { netfsMounted[x.id] = true; });
	} catch (e) { /* without it the summaries leave the state out */ }
}

/* After a write the box answers the whole table: the slot written to is drawn
   again from it, the others only get their summary, so what was typed into
   them and not saved yet stays. */
function netfsUpdate(table, r, acted) {
	((r && r.items) || []).forEach(function(e) {
		if (e.slot === acted) {
			var old = document.getElementById('netfs-slot-' + e.slot);
			if (old) old.replaceWith(netfsSlot(table, e, true));
			return;
		}
		var summary = document.getElementById('netfs_summary_' + e.slot);
		if (summary) summary.textContent = netfsSummary(table, e);
	});
	netfsLast = r;
}

function netfsShowFile(r) {
	document.getElementById('netfs-file').textContent = (r && r.path) || '';
	var bad = (r && r.unreadable_lines) || 0;
	var warn = document.getElementById('netfs-unreadable');
	warn.hidden = bad === 0;
	warn.textContent = webuiL('netfs.unreadable', '%s lines of the file could not be read. Saving an entry keeps the readable entries and drops those lines.').replace('%s', bad);
}

async function netfsLoad() {
	var table = netfsTable;
	var mine = ++netfsShown;
	var list = document.getElementById('netfs-slots');
	netfsTabs();
	// the first reading, and one after a failure: the shared loading state
	if (!list.querySelector('.netfs-slot'))
		list.replaceChildren(webuiState('loading', webuiL('msg.loading', 'Loading...')));
	try {
		var r = await webuiV1('GET', '/storage/netfs/' + table);
		if (mine !== netfsShown) return;
		netfsLast = r;
		netfsShowFile(r);
		list.replaceChildren.apply(list, ((r && r.items) || []).map(function(e) { return netfsSlot(table, e, false); }));
	} catch (e) {
		if (mine !== netfsShown) return;
		list.replaceChildren(webuiStateFor(e, {retry: netfsLoad}));
		return;
	}
	/* What is mounted only after the forms: a share that is slow to say how
	   large it is holds up the summaries, not the page. */
	await netfsReadMounts();
	if (mine !== netfsShown || !netfsLast) return;
	((netfsLast && netfsLast.items) || []).forEach(function(e) {
		var summary = document.getElementById('netfs_summary_' + e.slot);
		if (summary) summary.textContent = netfsSummary(table, e);
	});
}

/* The forms of the other file go at once, before its own are read: nothing
   typed into one can then be sent while the other is shown. */
function netfsSwitch(table) {
	if (NETFS_TABLES.indexOf(table) < 0 || table === netfsTable) return;
	netfsTable = table;
	netfsLast = null;
	history.replaceState(null, '', table === 'fstab' ? location.pathname : '#' + table);
	netfsOutput('');
	netfsShowFile(null);
	document.getElementById('netfs-slots').replaceChildren(
		webuiState('loading', webuiL('msg.loading', 'Loading...')));
	netfsLoad();
}

// whether what is shown is still the reading a request was sent from: not
// after a switch to the other file, nor after one back again
function netfsStill(table, shown) {
	return table === netfsTable && shown === netfsShown;
}

// a slot's own form is locked while the box answers for it; an answer that
// comes after the forms were drawn anew is dropped, so it overwrites nothing
// typed into them since
async function netfsWrite(table, i, request) {
	var shown = netfsShown;
	var body = document.getElementById('netfs_body_' + i);
	if (body) body.disabled = true;
	try {
		var r = await request();
		if (!netfsStill(table, shown)) return true;
		netfsShowFile(r);
		netfsUpdate(table, r, i);
		// the slot is drawn anew, the focus it held went with it: to its
		// head, once a question that asked for this is gone as well
		setTimeout(function() {
			var at = document.activeElement;
			var head = document.querySelector('#netfs-slot-' + i + ' .netfs-slot-head');
			if (head && (!at || at === document.body)) head.focus();
		}, 0);
		return true;
	} catch (e) {
		webuiToastError(webuiL('msg.error', 'Error'), e);
		if (body && netfsStill(table, shown)) body.disabled = false;
		return false;
	}
}

async function netfsSave(table, i) {
	var val = function(key) { return document.getElementById('netfs_' + key + '_' + i).value.trim(); };
	var body = {
		active: document.getElementById('netfs_active_' + i).checked,
		type: val('type'), host: val('host'), remote_dir: val('remote_dir'),
		local_dir: val('local_dir'), options: val('options')
	};
	if (body.type === 'cifs')
		body.user = val('user');
	/* Typed: the new one. Left empty: the stored one stays, which is what the
	   box does with a password left out; an empty one is never sent. */
	var pass = document.getElementById('netfs_password_' + i).value;
	if (body.type === 'cifs' && pass !== '')
		body.password = pass;
	if (await netfsWrite(table, i, function() {
		return webuiV1('PUT', '/storage/netfs/' + table + '/' + i, {body: body});
	}))
		webuiShowToast(table === 'automount' ?
			webuiL('netfs.saved_automount', 'Entry saved. The automounter offers it once it has been restarted.') :
			webuiL('netfs.saved', 'Entry saved'));
}

function netfsClear(table, i) {
	webuiShowConfirm(webuiL('netfs.clear_title', 'Empty entry %s?').replace('%s', i + 1),
		webuiLH('netfs.clear_text', 'The entry is taken out of the file, its password with it. Nothing that is mounted is unmounted.'),
		webuiL('netfs.clear', 'Empty'),
		async function() {
			if (await netfsWrite(table, i, function() {
				return webuiV1('DELETE', '/storage/netfs/' + table + '/' + i);
			}))
				webuiShowToast(webuiL('netfs.cleared', 'Entry emptied'));
		});
}

function netfsOutput(text) {
	var box = document.getElementById('netfs-output');
	box.hidden = !text;
	document.getElementById('netfs-output-text').textContent = text;
}

// one of the box's own scripts: its output shown as it wrote it, the
// summaries read again, both only while its reading is still the one shown
async function netfsRun(btn, table, path) {
	var shown = netfsShown;
	btn.disabled = true;
	try {
		var r = await webuiV1('POST', path);
		await netfsReadMounts();
		if (netfsStill(table, shown)) {
			netfsOutput((r && r.output) || webuiL('netfs.no_output', '(no output)'));
			if (netfsLast) netfsUpdate(table, netfsLast, -1);
		}
		webuiShowToast(webuiL('netfs.done', 'Done'));
	} catch (e) {
		webuiToastError(webuiL('msg.error', 'Error'), e);
	}
	btn.disabled = false;
}

function netfsMountAll(btn) {
	netfsRun(btn, 'fstab', '/storage/netfs/fstab/mount');
}

function netfsUnmountAll(btn) {
	webuiShowConfirm(webuiL('netfs.umount_title', 'Unmount all network filesystems?'),
		webuiLH('netfs.umount_text', 'Every network filesystem the box has mounted is unmounted, also one this file does not name. A recording on one loses it.'),
		webuiL('netfs.umount_all', 'Unmount all'),
		function() { return netfsRun(btn, 'fstab', '/storage/netfs/fstab/unmount'); });
}

function netfsReload(btn) {
	netfsRun(btn, 'automount', '/storage/netfs/automount/reload');
}

(function() {
	var asked = location.hash.replace('#', '');
	if (NETFS_TABLES.indexOf(asked) >= 0)
		netfsTable = asked;
	var tabs = Array.prototype.slice.call(document.querySelectorAll('[data-netfs-table]'));
	tabs.forEach(function(b, n) {
		b.addEventListener('click', function() { netfsSwitch(b.getAttribute('data-netfs-table')); });
		// arrows, Home and End move between the tabs and show the one reached
		b.addEventListener('keydown', function(ev) {
			var to = {ArrowRight: n + 1, ArrowLeft: n - 1, Home: 0, End: tabs.length - 1}[ev.key];
			if (to === undefined) return;
			ev.preventDefault();
			var t = tabs[(to + tabs.length) % tabs.length];
			t.focus();
			netfsSwitch(t.getAttribute('data-netfs-table'));
		});
	});
})();

webuiExtTexts('netfs').then(netfsLoad);
