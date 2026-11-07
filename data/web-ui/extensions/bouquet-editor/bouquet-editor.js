// The script of the bouquet editor's page (extension bouquet-editor).

// The editor speaks /api/v1: a bouquet is read with its position (id)
// and its hidden/locked state, and changed by its name; the box saves
// every change at once and announces it as an event. A name more than
// one bouquet carries addresses none of them, so such rows can be read
// but not changed here.
var bqeBouquets = [];      // [{id, name, hidden, locked, tv_count, radio_count}]
var bqeShown = [];         // the bouquets the list shows, as it was drawn
var bqeSel = null;         // {id, name} of the chosen bouquet
var bqeChan = [];          // working copy of its channels [{id, name}]
var bqeChanOf = null;      // {sel, mode} the working copy was read for
var bqeChanReading = false;
var bqeAllChan = [];       // every channel of the mode
var bqeAllOf = null;       // the mode bqeAllChan was read for
var bqeAllWhy = null;      // why the last read of every channel failed, else null
var bqeChanWhy = null;     // why the last read of the chosen bouquet's failed, else null
var bqeBouquetsRead = false;	// the bouquets were read once
var bqeBouquetsWhy = null;	// why the last read of them failed, else null
var bqeChanDirty = false;
var bqeApplying = false;
var bqeReloadTimer = null;
var bqeForeignSeen = false;	// a change from elsewhere since the last reload
var bqeOwn = {sent: 0, early: 0, due: []};	// see bqeOwnRequest
var bqeModeShown = 'TV';
// the latest read of each list; an answer to an older one is dropped,
// it may come last and would put back what a newer one replaced
var bqeReads = {bouquets: 0, all: 0, chan: 0};

function esc(t) { return escapeHtml(String(t == null ? '' : t)); }

// M5-Komponenten: 12px-Icons fuer die Listen-Miniknoepfe
// (CSS .bqe-mini svg dimensioniert)
function sIcon(p) { return '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + p + '<\/svg>'; }
var I_UP = sIcon('<polyline points="18 15 12 9 6 15"/>');
var I_DOWN = sIcon('<polyline points="6 9 12 15 18 9"/>');
var I_EDIT = sIcon('<path d="M17 3a2.85 2.83 0 1 1 4 4L7.5 20.5 2 22l1.5-5.5Z"/>');
var I_EYE = sIcon('<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/>');
var I_KEY = sIcon('<rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>');
var I_X = sIcon('<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>');
var I_PLUS = sIcon('<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>');
var I_EYEOFF = sIcon('<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/><line x1="1" y1="1" x2="23" y2="23"/>');
var I_AUTO = sIcon('<path d="M21 12a9 9 0 0 1-15.5 6.2L3 16"/><path d="M3 12a9 9 0 0 1 15.5-6.2L21 8"/><path d="M3 21v-5h5"/><path d="M21 3v5h-5"/>');
var I_TWIN = sIcon('<rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>');

function bqeMode() {
	return document.getElementById('bqeMode').value === 'RADIO' ? 'radio' : 'tv';
}

function bqeById(id) {
	for (var i = 0; i < bqeBouquets.length; i++)
		if (bqeBouquets[i].id === id) return bqeBouquets[i];
	return null;
}

function bqeShared(name) {
	var n = 0;
	for (var i = 0; i < bqeBouquets.length; i++)
		if (bqeBouquets[i].name === name && ++n > 1) return true;
	return false;
}

// A bouquet the box builds itself (editable false: the one of the
// channels no other bouquet holds, WebTV, web radio) is shown and never
// changed, as the box would make it again.
function bqeFixed(b) {
	return !!b && b.editable === false;
}

function bqeSelFixed() {
	return !!bqeSel && bqeFixed(bqeById(bqeSel.id));
}

function bqeFixedText() {
	return webuiL('bqe.fixed', 'The box builds this bouquet itself - it cannot be changed here');
}

function bqePath(name, sub) {
	return '/bouquets/' + encodeURIComponent(name) + (sub ? '/' + sub : '');
}

// After a reload the chosen bouquet is found again: by its name when
// only one carries it (a move changes its position), else where it was.
// While it is being renamed, its new name counts as well.
function bqeFindSelection() {
	if (!bqeSel) return;
	var names = bqeSel.renaming ? [bqeSel.name, bqeSel.renaming] : [bqeSel.name];
	for (var k = 0; k < names.length; k++) {
		var hits = bqeBouquets.filter(function(b) { return b.name === names[k]; });
		if (hits.length === 1) { bqeSel.id = hits[0].id; bqeSel.name = hits[0].name; return; }
	}
	var b = bqeById(bqeSel.id);
	if (!b || names.indexOf(b.name) < 0) {
		// gone, and its working copy with it
		bqeSel = null;
		bqeChanClear();
	}
}

// The working copy has been read for the chosen bouquet in the shown
// mode: only then may it be applied, or edited (bqeEditable).
function bqeChanReady() {
	return !!(bqeSel && bqeChanOf && bqeChanOf.sel === bqeSel && bqeChanOf.mode === bqeMode());
}

// A bouquet whose name another one carries cannot be applied, and one
// the box builds itself takes no change, so neither list is edited.
function bqeEditable() {
	return bqeChanReady() && !bqeShared(bqeSel.name) && !bqeSelFixed();
}

// The working copy goes; nothing can be edited until the chosen
// bouquet's list has been read.
function bqeChanClear() {
	bqeChan = [];
	bqeChanOf = null;
	bqeChanReading = !!bqeSel;
	setDirty(false);
}

function setDirty(d) {
	bqeChanDirty = d;
	document.getElementById('bqeDirty').classList.toggle('on', d);
	document.querySelector('#bqeViews [data-view="channels"]').classList.toggle('bqe-unsaved', d);
	// Uebernehmen hervorheben, solange die Arbeitskopie abweicht
	document.getElementById('bqeApplyBtn').classList.toggle('attn', d);
	document.getElementById('bqeDiscardBtn').disabled = !d;
	document.getElementById('bqeActsDiscard').disabled = !d;
	// The phone's pair goes with the changes, and a focus in it goes to the
	// view shown; so does one the pair let drop to the page, which a button
	// does when it is switched off while Apply runs.
	var acts = document.getElementById('bqeActs');
	var at = document.activeElement;
	var had = !d && bqePhone.matches && acts.classList.contains('on') &&
		(!at || at === document.body || acts.contains(at));
	acts.classList.toggle('on', d);
	if (had)
		document.querySelector('#bqeViews [aria-pressed="true"]').focus();
	bqeUpdateApply();
}

// "Apply" sends the working copy: only one that was read for the chosen
// bouquet in the shown mode and changed since, and one at a time.
function bqeUpdateApply() {
	var shared = !!(bqeSel && bqeShared(bqeSel.name));
	var fixed = bqeSelFixed();
	['bqeApplyBtn', 'bqeActsApply'].forEach(function(id) {
		var apply = document.getElementById(id);
		apply.disabled = shared || fixed || bqeApplying || !bqeChanDirty || !bqeChanReady();
		apply.title = shared ? webuiL('bqe.shared', 'More than one bouquet carries this name - rename one of them on the box first') :
			fixed ? bqeFixedText() : webuiL('bqe.apply_title', 'Take over the channel list of this bouquet');
	});
}

function bqeShowStale(on) {
	document.getElementById('bqeStale').hidden = !on;
}

// Asks before unapplied channel edits are thrown away; go() runs when
// there are none or when the user agrees, cancelling changes nothing.
function bqeUnlessDirty(go) {
	if (!bqeChanDirty) { go(); return; }
	webuiShowConfirm(webuiL('bqe.drop_title', 'Discard channel changes?'), webuiLH('bqe.drop_msg', 'The channel list has changes not applied yet. They will be lost.'), webuiL('bqe.discard', 'Discard'),
		function() { setDirty(false); go(); });
}

// A list drawn again loses the keyboard focus; it goes back to the row
// that had it, and to the same button of that row.
function bqeFocusIn(ul) {
	var el = document.activeElement;
	if (!el || !ul.contains(el)) return null;
	var btn = el.closest('button[data-act]');
	return {li: el.closest('li'), act: btn ? btn.dataset.act : null};
}
function bqeFocusRow(li, act) {
	if (!li) return;
	var btn = act ? li.querySelector('button[data-act="' + act + '"]') : null;
	if (btn && !btn.disabled) btn.focus();
	else if (li.tabIndex >= 0) li.focus();
}

/* Our own changes announce themselves as well, one event for each
   change that went through, and the event may come before the answer
   does. That many events are taken as ours and the rest as coming
   from elsewhere. An event still owed after a few seconds was lost
   (the stream was down) and is no longer waited for. A request the
   box refused for want of a sign-in changes nothing, so it does not
   count while the sign-in is open, only once it is sent again. */
async function bqeOwnRequest(method, path, opts) {
	var counted = true;
	var through = false;
	bqeOwn.sent++;
	var o = Object.assign({}, opts, {
		refused: function() {
			counted = false;
			bqeOwn.sent--;
			if (bqeOwn.early > bqeOwn.sent) bqeOwn.early = bqeOwn.sent;
		},
		retried: function() { counted = true; bqeOwn.sent++; }
	});
	try {
		var r = await webuiV1(method, path, o);
		through = true;
		return r;
	} finally {
		if (counted) {
			bqeOwn.sent--;
			if (bqeOwn.early > 0) bqeOwn.early--;
			else if (through) bqeOwn.due.push(Date.now() + 5000);
		}
	}
}

// true when a bouquets-changed event is one of our own
function bqeOwnEvent() {
	var now = Date.now();
	bqeOwn.due = bqeOwn.due.filter(function(t) { return t > now; });
	if (bqeOwn.due.length) { bqeOwn.due.shift(); return true; }
	if (bqeOwn.sent > bqeOwn.early) { bqeOwn.early++; return true; }
	return false;
}

// Everything from the box again. Unapplied channel edits stay, unless
// discard is asked for: then the chosen bouquet's list is read too.
// The banner stays as long as edits are kept: what it warns of holds.
// After own changes only (ownOnly) the chosen bouquet's channels are
// not read again: hiding, locking, moving, renaming, adding or deleting
// a bouquet leaves them as they are, and "Apply" reads them itself.
async function bqeReloadAll(discard, ownOnly) {
	if (discard) {
		bqeChanClear();
		bqeRenderChannels();
		bqeRenderAll();
	}
	if (!bqeChanDirty) bqeShowStale(false);
	await Promise.all([bqeLoadBouquets(), bqeLoadAllChannels()]);
	if (!bqeSel) bqeChanClear();
	else if (!bqeChanDirty && !(ownOnly && bqeChanReady())) await bqeLoadChannels();
	bqeRenderBouquets();
	bqeRenderChannels();
	bqeRenderAll();
	if (!bqeChanDirty) bqeShowStale(false);
}

// A read that fails leaves the list as it was: an empty one would drop
// the chosen bouquet and, with it, unapplied edits.
async function bqeLoadBouquets() {
	var read = ++bqeReads.bouquets;
	var list = null;
	// a list never read shows that it is being read, not an earlier failure
	bqeBouquetsWhy = null;
	if (!bqeBouquetsRead) bqeRenderBouquets();
	try {
		var resp = await webuiV1('GET', '/bouquets');
		list = (resp && resp.items) || [];
	} catch (e) {
		if (read === bqeReads.bouquets) {
			bqeBouquetsWhy = e;
			webuiToastError(webuiL('nav.bouquets', 'Bouquets'), e);
		}
	}
	if (read !== bqeReads.bouquets) return;
	if (list === null) {
		// never read: the list says why instead of "no bouquets"
		if (!bqeBouquetsRead) bqeRenderBouquets();
		return;
	}
	bqeBouquetsRead = true;
	bqeBouquetsWhy = null;
	bqeBouquets = list;
	bqeFindSelection();
	bqeRenderBouquets();
	bqeRenderChannels();
}

async function bqeLoadAllChannels() {
	var read = ++bqeReads.all;
	var mode = bqeMode();
	var list = null;
	// a failure of another read is not this one's: loading, until it answers
	bqeAllWhy = null;
	if (bqeAllOf !== mode) bqeRenderAll();
	try {
		list = (await webuiV1All('/channels', {mode: mode}))
			.map(function(c) { return {id: c.id, name: c.name}; });
	} catch (e) {
		if (read === bqeReads.all) {
			bqeAllWhy = e;
			webuiToastError(webuiL('bqe.all_channels', 'All channels'), e);
		}
	}
	if (read !== bqeReads.all) return;
	if (list === null) {
		bqeRenderAll();
		return;
	}
	bqeAllWhy = null;
	bqeAllChan = list;
	bqeAllOf = mode;
	bqeRenderAll();
}

// The chosen bouquet's channels in the shown mode, as the working copy.
// A read that fails keeps a copy that belongs to the same bouquet and
// mode and leaves none otherwise; a copy edited while it was read
// again stays, and the banner says the box may have changed.
async function bqeLoadChannels() {
	var read = ++bqeReads.chan;
	var sel = bqeSel;
	var mode = bqeMode();
	var list = null;
	bqeChanReading = true;
	bqeChanWhy = null;
	if (!bqeChanReady()) bqeRenderChannels();
	try {
		list = !sel ? [] : (await webuiV1All('/channels', {bouquet: sel.id, mode: mode}))
			.map(function(c) { return {id: c.id, name: c.name}; });
	} catch (e) {
		if (read === bqeReads.chan) {
			bqeChanWhy = e;
			webuiToastError(webuiL('bqe.channels', 'Channels'), e);
		}
	}
	if (read !== bqeReads.chan) return;
	bqeChanReading = false;
	if (sel !== bqeSel) {
		// the bouquet it was read for is no longer the chosen one
	} else if (list === null) {
		if (!bqeChanReady()) { bqeChan = []; bqeChanOf = null; setDirty(false); }
	} else if (bqeChanDirty && bqeChanReady()) {
		bqeShowStale(true);
	} else {
		bqeChan = list;
		bqeChanOf = sel ? {sel: sel, mode: mode} : null;
		setDirty(false);
	}
	bqeRenderChannels();
}

function bqeRenderBouquets() {
	var ul = document.getElementById('bqeBouquets');
	var had = bqeFocusIn(ul);
	var was = null;
	if (had && had.li)
		for (var k = 0; k < bqeShown.length; k++)
			if (String(bqeShown[k].id) === had.li.dataset.id) { was = bqeShown[k]; break; }
	document.getElementById('bqeBqCount').textContent = bqeBouquetsRead ? bqeBouquets.length + '' : '';
	var radio = bqeMode() === 'radio';
	var html = '';
	bqeBouquets.forEach(function(b) {
		var n = radio ? b.radio_count : b.tv_count;
		var shared = bqeShared(b.name);
		var fixed = bqeFixed(b);
		// a row that takes no change says why on each of its buttons
		var why = shared ? webuiL('bqe.shared', 'More than one bouquet carries this name - rename one of them on the box first') :
			fixed ? bqeFixedText() : '';
		var off = why ? ' disabled title="' + escapeHtml(why) + '"' : '';
		// a switch keeps its name and says its state by aria-pressed; only
		// the hint for the mouse says what a click does next
		function btn(act, cls, title, icon, pressed, name) {
			return '<button type="button" class="bqe-mini' + cls + '" data-act="' + act + '"' +
				(pressed === undefined ? '' : ' aria-pressed="' + pressed + '"') +
				(why ? off : ' title="' + escapeHtml(title) + '"') + ' aria-label="' + escapeHtml(name || title) + '">' + icon + '</button>';
		}
		function flag(text, icon, kind) {
			return '<span role="img"' + (kind ? ' data-flag="' + kind + '"' : '') + ' title="' + escapeHtml(text) + '" aria-label="' + escapeHtml(text) + '">' + icon + '</span>';
		}
		var sel = bqeSel && b.id === bqeSel.id;
		html += '<li tabindex="0" data-id="' + esc(b.id) + '"' + (sel ? ' class="sel" aria-current="true"' : '') + '>' +
			'<span class="nm">' + esc(b.id) + ' ' + esc(b.name) + '</span>' +
			'<span class="flags">' +
				(shared ? flag(webuiL('bqe.shared', 'More than one bouquet carries this name - rename one of them on the box first'), I_TWIN) : '') +
				(fixed ? flag(bqeFixedText(), I_AUTO, 'fixed') : '') +
				(b.hidden ? flag(webuiL('bqe.hidden', 'hidden'), I_EYEOFF) : '') +
				(b.locked ? flag(webuiL('bqe.locked', 'locked'), I_KEY) : '') +
				' ' + n + '</span>' +
			btn('up', '', webuiL('bqe.t_up', 'up'), I_UP) +
			btn('down', '', webuiL('bqe.t_down', 'down'), I_DOWN) +
			btn('rename', '', webuiL('bqe.t_rename', 'rename'), I_EDIT) +
			btn('hidden', b.hidden ? ' on' : '', b.hidden ? webuiL('bqe.t_show', 'show') : webuiL('bqe.t_hide', 'hide'), b.hidden ? I_EYEOFF : I_EYE, b.hidden ? 'true' : 'false',
				webuiL('bqe.hidden', 'hidden')) +
			btn('locked', b.locked ? ' on' : '', b.locked ? webuiL('bqe.t_unlock', 'unlock') : webuiL('bqe.t_lock_on', 'lock (parental control)'), I_KEY, b.locked ? 'true' : 'false',
				webuiL('bqe.locked', 'locked')) +
			btn('del', '', webuiL('bqe.t_del', 'delete'), I_X) +
			'</li>';
	});
	ul.innerHTML = html || '<li class="bqe-empty">' + webuiLH('bqe.no_bouquets', 'no bouquets') + '</li>';
	// not read yet, or not at all: that, and not "no bouquets"
	if (!html && !bqeBouquetsRead)
		bqeStateIn(ul, bqeBouquetsWhy ? webuiStateFor(bqeBouquetsWhy, {retry: bqeLoadBouquets})
			: webuiState('loading', webuiL('msg.loading', 'Loading...')));
	bqeShown = bqeBouquets;
	if (was) {
		// by its name when that is its own, else by its position
		var same = bqeBouquets.filter(function(b) { return b.name === was.name; });
		var at = same.length === 1 ? same[0].id : was.id;
		bqeFocusRow(ul.querySelector('li[data-id="' + at + '"]'), had.act);
	}
}

// One handler for the bouquet list: a click on a row picks it, a click
// on one of its buttons acts on it; Enter or Space on a row picks it.
function bqeBouquetEvent(e) {
	var li = e.target.closest('li[data-id]');
	if (!li) return;
	var b = bqeById(parseInt(li.dataset.id, 10));
	if (!b) return;
	var btn = e.target.closest('button[data-act]');
	if (e.type === 'keydown') {
		if (btn || (e.key !== 'Enter' && e.key !== ' ')) return;
		e.preventDefault();
	}
	if (!btn) { bqeSelect(b); return; }
	e.stopPropagation();
	if (btn.disabled) return;
	var act = btn.dataset.act;
	if (act === 'up' || act === 'down') bqeMove(b, act);
	else if (act === 'rename') bqeRename(b);
	else if (act === 'hidden' || act === 'locked') bqeToggle(b, act);
	else if (act === 'del') bqeDelete(b);
}

function bqeRenderChannels() {
	var ul = document.getElementById('bqeChannels');
	var ready = bqeChanReady();
	var off = bqeEditable() ? '' : ' disabled';
	bqeUpdateApply();
	document.getElementById('bqeFixed').hidden = !bqeSelFixed();
	document.getElementById('bqeChTitle').textContent = !bqeSel ? webuiL('bqe.channels', 'Channels') :
		(webuiL('bqe.channels', 'Channels') + ': ' + bqeSel.name + (ready ? ' (' + bqeChan.length + ')' : ''));
	var html = '';
	bqeChan.forEach(function(c, i) {
		html += '<li draggable="true" ondragstart="bqeDragMove(event, ' + i + ')"' +
			' ondragover="bqeRowOver(event, ' + i + ')" ondragleave="bqeRowLeave(event)" ondrop="bqeRowDrop(event, ' + i + ')">' +
			'<span class="flags">' + (i + 1) + '</span>' +
			'<span class="nm">' + esc(c.name) + '</span>' +
			'<button type="button" class="bqe-mini" data-act="up"' + off + ' title="' + webuiLH('bqe.t_up', 'up') + '" aria-label="' + webuiLH('bqe.t_up', 'up') + '" onclick="bqeChMove(' + i + ', -1)">' + I_UP + '</button>' +
			'<button type="button" class="bqe-mini" data-act="down"' + off + ' title="' + webuiLH('bqe.t_down', 'down') + '" aria-label="' + webuiLH('bqe.t_down', 'down') + '" onclick="bqeChMove(' + i + ', 1)">' + I_DOWN + '</button>' +
			'<button type="button" class="bqe-mini" data-act="remove"' + off + ' title="' + webuiLH('bqe.t_remove', 'remove') + '" aria-label="' + webuiLH('bqe.t_remove', 'remove') + '" onclick="bqeChRemove(' + i + ')">' + I_X + '</button></li>';
	});
	ul.innerHTML = html || '<li class="bqe-empty">' +
		(!bqeSel ? webuiLH('bqe.pick_left', 'Pick a bouquet on the left') : ready ? webuiLH('bqe.empty_drop', 'empty — drag channels here from the right list or add them with +') :
		 bqeChanReading ? '' : webuiLH('bqe.not_read', 'Channel list not read - choose the bouquet again')) + '</li>';
	if (!html && bqeSel && !ready && bqeChanReading)
		bqeStateIn(ul, webuiState('loading', webuiL('msg.loading', 'Loading...')));
	else if (!html && bqeSel && !ready && bqeChanWhy)
		bqeStateIn(ul, webuiStateFor(bqeChanWhy, {retry: bqeLoadChannels}));
}

// a list's state (loading, failed) as its one item (this page builds its
// elements itself, without webui-dom.js)
function bqeStateIn(ul, block) {
	var li = document.createElement('li');
	li.className = 'bqe-state';
	li.appendChild(block);
	ul.replaceChildren(li);
}

// Channel names go into data attributes and are read back from there,
// never into the script of an attribute.
function bqeRenderAll() {
	var f = (document.getElementById('bqeFilter').value || '').toLowerCase();
	var can = bqeEditable() && bqeAllOf === bqeMode() ? '' : ' disabled';
	// an add that is off says why, not "add"
	var addTitle = !can ? webuiLH('bqe.t_add', 'add') :
		!bqeSel ? webuiLH('bqe.pick_left', 'Pick a bouquet on the left') :
		bqeShared(bqeSel.name) ? webuiLH('bqe.shared', 'More than one bouquet carries this name - rename one of them on the box first') :
		bqeSelFixed() ? escapeHtml(bqeFixedText()) :
		webuiLH('msg.loading', 'Loading...');
	var inBq = {};
	bqeChan.forEach(function(c) { inBq[c.id] = true; });
	var html = '';
	var shown = 0;
	var hits = 0;
	for (var i = 0; i < bqeAllChan.length; i++) {
		var c = bqeAllChan[i];
		if (f && String(c.name).toLowerCase().indexOf(f) === -1) continue;
		hits++;
		if (shown >= 400) continue; // Rest nur zaehlen
		shown++;
		html += '<li draggable="true" data-id="' + esc(c.id) + '" data-name="' + esc(c.name) + '">' +
			'<span class="nm">' + esc(c.name) + '</span>' +
			(inBq[c.id] ? '<span class="flags">' + webuiLH('bqe.in_bouquet', 'in bouquet') + '</span>' :
			'<button type="button" class="bqe-mini" data-act="add"' + can + ' title="' + addTitle + '" aria-label="' + webuiLH('bqe.t_add', 'add') + '">' + I_PLUS + '</button>') +
			'</li>';
	}
	if (hits > shown)
		html += '<li class="bqe-empty">+' + (hits - shown) + ' ' + webuiLH('bqe.more_hits', 'more matches — narrow the search') + '</li>';
	document.getElementById('bqeAllCount').textContent =
		f ? (hits + '/' + bqeAllChan.length) : String(bqeAllChan.length);
	var all = document.getElementById('bqeAll');
	all.innerHTML = html || '<li class="bqe-empty">' + webuiLH('bqe.no_hits', 'no matches') + '</li>';
	// the mode's channels not read yet, or not at all
	if (!html && bqeAllOf !== bqeMode())
		bqeStateIn(all, bqeAllWhy ? webuiStateFor(bqeAllWhy, {retry: bqeLoadAllChannels})
			: webuiState('loading', webuiL('msg.loading', 'Loading...')));
}

function bqeAllEvent(e) {
	var li = e.target.closest('li[data-id]');
	if (!li) return;
	if (e.type === 'dragstart') { bqeDragAdd(e, li.dataset.id, li.dataset.name); return; }
	if (e.type === 'click' && !e.target.closest('button[data-act="add"]')) return;
	bqeChAdd(li.dataset.id, li.dataset.name);
}

// On a phone one list shows at a time, the one the switch above them
// names; choosing a bouquet there goes on to its channels.
var bqePhone = window.matchMedia('(max-width: 760px)');

function bqeView(v) {
	document.querySelector('.bqe-grid').dataset.view = v;
	document.querySelectorAll('#bqeViews button').forEach(function(b) {
		b.setAttribute('aria-pressed', b.dataset.view === v ? 'true' : 'false');
	});
}

function bqeToChannels() {
	if (!bqePhone.matches)
		return;
	var from = document.activeElement;
	bqeView('channels');
	// the row chosen is hidden with its list now: the focus goes to the switch
	if (from && from.closest && from.closest('[data-pane="bouquets"]'))
		document.querySelector('#bqeViews [data-view="channels"]').focus();
}

// Choosing the bouquet shown again reads its list once more when that
// read had failed.
function bqeSelect(b) {
	if (bqeSel && b.id === bqeSel.id && (bqeChanReady() || bqeChanReading)) {
		bqeToChannels();
		return;
	}
	bqeUnlessDirty(async function() {
		bqeSel = {id: b.id, name: b.name};
		bqeToChannels();
		bqeChanClear();
		bqeRenderBouquets();
		var reading = bqeLoadChannels();
		bqeRenderChannels();
		bqeRenderAll();
		await reading;
		bqeRenderAll();
	});
}

function bqeModeChanged() {
	var sel = document.getElementById('bqeMode');
	var wanted = sel.value;
	sel.value = bqeModeShown;
	bqeUnlessDirty(function() {
		sel.value = bqeModeShown = wanted;
		// neither list is one of this mode until it has been read
		bqeAllChan = [];
		bqeAllOf = null;
		bqeReloadAll(true);
	});
}

// ---- Drag & Drop: Kanaele im Bouquet sortieren und aus
// "Alle Kanaele" herueberziehen (Arbeitskopie, "Uebernehmen"
// speichert). Knoepfe bleiben als Touch-Fallback.
var bqeDrag = null; // {type:'move', from} | {type:'add', id, name}

function bqeDragMove(e, i) {
	bqeDrag = { type: 'move', from: i };
	e.dataTransfer.effectAllowed = 'move';
	try { e.dataTransfer.setData('text/plain', 'bqe'); } catch (err) {}
}

function bqeDragAdd(e, id, name) {
	bqeDrag = { type: 'add', id: id, name: name };
	e.dataTransfer.effectAllowed = 'copy';
	try { e.dataTransfer.setData('text/plain', 'bqe'); } catch (err) {}
}

function bqeDropOk() {
	return !!bqeDrag && bqeEditable() && (bqeDrag.type === 'move' || bqeAllOf === bqeMode());
}

function bqeRowOver(e, i) {
	if (!bqeDropOk()) return;
	e.preventDefault();
	e.stopPropagation();
	var r = e.currentTarget.getBoundingClientRect();
	var below = (e.clientY - r.top) > r.height / 2;
	e.currentTarget.classList.toggle('drop-top', !below);
	e.currentTarget.classList.toggle('drop-bottom', below);
	e.dataTransfer.dropEffect = bqeDrag.type === 'move' ? 'move' : 'copy';
}

function bqeRowLeave(e) {
	e.currentTarget.classList.remove('drop-top', 'drop-bottom');
}

function bqeRowDrop(e, i) {
	if (!bqeDropOk()) return;
	e.preventDefault();
	e.stopPropagation();
	var r = e.currentTarget.getBoundingClientRect();
	var below = (e.clientY - r.top) > r.height / 2;
	e.currentTarget.classList.remove('drop-top', 'drop-bottom');
	bqeInsertAt(below ? i + 1 : i);
}

function bqeListOver(e) {
	if (!bqeDropOk()) return;
	e.preventDefault();
	e.currentTarget.classList.add('drop-hover');
	e.dataTransfer.dropEffect = bqeDrag.type === 'move' ? 'move' : 'copy';
}

function bqeListLeave(e) {
	if (e.target === e.currentTarget)
		e.currentTarget.classList.remove('drop-hover');
}

function bqeListDrop(e) {
	if (!bqeDropOk()) return;
	e.preventDefault();
	e.currentTarget.classList.remove('drop-hover');
	bqeInsertAt(bqeChan.length); // freier Bereich = ans Ende
}

function bqeInsertAt(idx) {
	if (bqeDrag.type === 'move') {
		var it = bqeChan.splice(bqeDrag.from, 1)[0];
		if (idx > bqeDrag.from) idx--;
		bqeChan.splice(idx, 0, it);
	} else {
		var id = bqeDrag.id;
		if (bqeChan.some(function(c) { return c.id === id; })) { bqeDrag = null; return; }
		bqeChan.splice(idx, 0, { id: bqeDrag.id, name: bqeDrag.name });
	}
	bqeDrag = null;
	setDirty(true);
	bqeRenderChannels();
	bqeRenderAll();
}

document.addEventListener('dragend', function() {
	bqeDrag = null;
	document.querySelectorAll('.drop-top, .drop-bottom').forEach(function(el) {
		el.classList.remove('drop-top', 'drop-bottom');
	});
	document.getElementById('bqeChannels').classList.remove('drop-hover');
});

// ---- Kanal-Operationen (Arbeitskopie, "Uebernehmen" speichert sie);
// nur an einer Arbeitskopie, die fuer dieses Bouquet gelesen ist
function bqeChMove(i, d) {
	var j = i + d;
	if (!bqeEditable() || j < 0 || j >= bqeChan.length) return;
	var ul = document.getElementById('bqeChannels');
	var had = bqeFocusIn(ul);
	var t = bqeChan[i]; bqeChan[i] = bqeChan[j]; bqeChan[j] = t;
	setDirty(true); bqeRenderChannels();
	if (had) bqeFocusRow(ul.children[j], had.act);
}
function bqeChRemove(i) {
	if (!bqeEditable()) return;
	var ul = document.getElementById('bqeChannels');
	var had = bqeFocusIn(ul);
	bqeChan.splice(i, 1);
	setDirty(true); bqeRenderChannels(); bqeRenderAll();
	if (had) bqeFocusRow(ul.children[Math.min(i, bqeChan.length - 1)], 'remove');
}
function bqeChAdd(id, name) {
	if (!bqeSel) { webuiShowToast(webuiL('bqe.pick_first', 'Pick a bouquet first'), true); return; }
	if (!bqeEditable() || bqeAllOf !== bqeMode()) return;
	if (bqeChan.some(function(c) { return c.id === id; })) return;
	bqeChan.push({id: id, name: name});
	setDirty(true); bqeRenderChannels(); bqeRenderAll();
}

// The channel list of the chosen bouquet in the shown mode, as a whole;
// the other mode's half of the bouquet stays as it is. A list that
// leaves out a channel no other bouquet holds would take that channel
// off the box, so the box refuses it until the user has said so here.
async function bqeApplyChannels(remove, fromQuestion) {
	if (!bqeSel) { webuiShowToast(webuiL('bqe.none_picked', 'No bouquet selected'), true); return; }
	if (bqeApplying || !bqeChanDirty || !bqeChanReady() || bqeShared(bqeSel.name) || bqeSelFixed()) return;
	// while the question is open, only its answer sends the list
	if (!fromQuestion && document.querySelector('.webui-confirm-backdrop')) return;
	bqeApplying = true;
	bqeUpdateApply();
	// the list as sent, so that a confirmation answers for it and no other
	var sent = {sel: bqeSel, mode: bqeMode(), ids: bqeChan.map(function(c) { return c.id; })};
	try {
		await bqeOwnRequest('PUT', bqePath(bqeSel.name, 'channels'), {
			query: {mode: sent.mode, remove_from_box: remove ? 'true' : null},
			body: sent.ids
		});
		setDirty(false);
		webuiShowToast(webuiL('bqe.applied', 'Channel list applied and saved on the box'));
		bqeApplying = false;
		await bqeReloadAll();
	} catch (e) {
		if (e.code === 'channel-would-be-removed' && !remove)
			bqeAskRemove(e.message, sent);
		else
			webuiToastError(webuiL('msg.error', 'Error'), e);
	} finally {
		bqeApplying = false;
		bqeRenderChannels();
	}
}

// The refusal names by id the channels the box would forget; the
// channel list of the mode has their names, and an id it does not know
// is shown as it came. A refusal in another shape is shown as it came.
function bqeAskRemove(detail, sent) {
	var m = /holds (.+?)( and more)?, so the box/.exec(String(detail));
	// a refusal in another shape names no channels here: the question
	// stands without them, never in the box's own English
	var which = !m ? '' : m[1].split(', ').map(function(id) {
		for (var i = 0; i < bqeAllChan.length; i++)
			if (bqeAllChan[i].id === id) return esc(bqeAllChan[i].name);
		return esc(id);
	}).join(', ') + (m[2] ? ', …' : '');
	webuiShowConfirm(webuiL('bqe.gone_title', 'Remove channels from the box?'), webuiLH('bqe.gone_msg', 'No other bouquet holds these channels, which the list leaves out. The box forgets a channel that leaves its last bouquet, and only a channel scan brings it back:') +
		(which ? '<br><strong>' + which + '<\/strong>' : ''),
		webuiL('bqe.gone_btn', 'Remove from the box'), function() {
			// a list that changed since it was refused is asked about again
			var same = bqeSel === sent.sel && bqeMode() === sent.mode &&
				bqeChan.length === sent.ids.length &&
				bqeChan.every(function(c, i) { return c.id === sent.ids[i]; });
			return bqeApplyChannels(same, true);
		});
}

function bqeDiscard() {
	if (bqeSel) bqeReloadAll(true);
}

// ---- Bouquet-Operationen: each one is saved on the box at once; the
// lists are read again, the working copy of the channels stays
async function bqeChange(method, path, body) {
	try {
		await bqeOwnRequest(method, path, body === undefined ? {} : {body: body});
		return true;
	} catch (e) {
		webuiToastError(webuiL('msg.error', 'Error'), e);
		return false;
	}
}

// A name the box refuses (one taken already) stays in the prompt, with
// what was typed and why: the refusal is the prompt's, not a toast.
function bqeAddBouquet() {
	webuiShowPrompt(webuiL('bqe.new', 'New bouquet'), webuiL('bqe.new_name', 'Name of the new bouquet'), '', async function(name) {
		await bqeOwnRequest('POST', '/bouquets', {body: {name: name}});
		webuiShowToast(webuiL('bqe.created', 'Bouquet created'));
		await bqeLoadBouquets();
	});
}

function bqeRename(b) {
	webuiShowPrompt(b.name, webuiL('bqe.rename_name', 'New name'), b.name, async function(name) {
		if (name === b.name) return;
		var mine = bqeSel && bqeSel.name === b.name ? bqeSel : null;
		if (mine) mine.renaming = name;
		try {
			await bqeOwnRequest('PUT', bqePath(b.name, 'name'), {body: {name: name}});
		} finally {
			if (mine) delete mine.renaming;
		}
		if (mine && bqeSel === mine) mine.name = name;
		webuiShowToast(webuiL('bqe.renamed', 'Renamed'));
		await bqeLoadBouquets();
	});
}

async function bqeMove(b, dir) {
	if (await bqeChange('PUT', bqePath(b.name, 'position'), {direction: dir}))
		await bqeLoadBouquets();
}

async function bqeToggle(b, flag) {
	if (await bqeChange('PUT', bqePath(b.name, flag), {on: !b[flag]}))
		await bqeLoadBouquets();
}

// The list is read again either way: a delete the box refused may
// mean the bouquet went or changed meanwhile.
function bqeDelete(b) {
	webuiShowConfirm(webuiL('bqe.del_title', 'Delete bouquet'), webuiLH('bqe.del_msg', 'Really delete this bouquet:') + ' "' + esc(b.name) + '"?', webuiL('bqe.del_btn', 'Delete'),
		async function() {
			var done = await bqeChange('DELETE', bqePath(b.name));
			if (done && bqeSel && bqeSel.name === b.name) { bqeSel = null; bqeChanClear(); }
			await bqeLoadBouquets();
			bqeRenderChannels();
			bqeRenderAll();
			if (done) webuiShowToast(webuiL('bqe.deleted', 'Deleted'));
		});
}

// The box announces every change of the bouquets. Our own ones read the
// lists again and keep the working copy; another one reloads, or, with
// channel edits not applied, asks first. Asked again when the timer
// runs out: editing may have begun meanwhile.
function bqeBouquetsChanged() {
	var own = bqeOwnEvent();
	if (!own && bqeChanDirty) { bqeShowStale(true); return; }
	if (!own) bqeForeignSeen = true;
	clearTimeout(bqeReloadTimer);
	bqeReloadTimer = setTimeout(function() {
		var ownOnly = !bqeForeignSeen;
		bqeForeignSeen = false;
		if (own || !bqeChanDirty) bqeReloadAll(false, ownOnly);
		else bqeShowStale(true);
	}, 300);
}

// Leaving the page would drop unapplied channel edits; the browser asks.
window.addEventListener('beforeunload', function(e) {
	if (!bqeChanDirty) return;
	e.preventDefault();
	e.returnValue = '';
});

document.getElementById('bqeViews').addEventListener('click', function(e) {
	var b = e.target.closest('button[data-view]');
	if (b) bqeView(b.dataset.view);
});
document.getElementById('bqeBouquets').addEventListener('click', bqeBouquetEvent);
document.getElementById('bqeBouquets').addEventListener('keydown', bqeBouquetEvent);
// each list is one stop of Tab: the arrows go from row to row and to the
// buttons of a row as they lie, Enter picks a bouquet or presses a button
webuiArrowKeys(document.getElementById('bqeBouquets'), 'li[data-id], button[data-act]', '', {near: true});
webuiArrowKeys(document.getElementById('bqeChannels'), 'button', '', {near: true});
webuiArrowKeys(document.getElementById('bqeAll'), 'button', '', {near: true});
// the boxes that scroll the lists are no stop of their own (Firefox would
// make them one, without a ring); the list's stop scrolls them
['bqeBouquets', 'bqeChannels', 'bqeAll'].forEach(function(id) { document.getElementById(id).parentElement.tabIndex = -1; });
// the phone's choice of the list shown: one stop, on the list shown
webuiArrowKeys(document.getElementById('bqeViews'), 'button');
document.getElementById('bqeAll').addEventListener('click', bqeAllEvent);
document.getElementById('bqeAll').addEventListener('dblclick', bqeAllEvent);
document.getElementById('bqeAll').addEventListener('dragstart', bqeAllEvent);
// once the texts are in, which the lists are drawn with; the header builds
// itself from its wrapper
webuiExtTexts('bouquet-editor').then(function() {
	webuiV1Events({'bouquets-changed': bqeBouquetsChanged});
	bqeReloadAll(true);
});
