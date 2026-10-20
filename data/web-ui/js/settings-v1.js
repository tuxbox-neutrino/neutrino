// settings-v1.js - the settings the box declares, drawn from and written to
// /api/v1/settings (settings.html; loaded after webui-dom.js, webui-v1.js and
// settings-registry.js).
//
// GET /settings/schema names every setting the box has: its key, kind,
// section, label and help text in the box's language, bounds or choices,
// default, whether it needs a restart, whether it is a credential, and the
// values of other settings it depends on. The page draws one panel per
// section in the order the box names them, reads a section with
// GET /settings/{section} and writes the changed keys of a section with
// PATCH /settings/{section}. A setting the box gains appears here without a
// change to this page, and its help is the text the box shows itself.
//
// What the schema does not declare stays on the old interface
// (config=neutrino, settings-registry.js). A registry category with
// v1section is dissolved into that section: its fields the schema lacks are
// drawn below the section's own rows, if the box's file carries them. A
// category without one (personalisation) keeps its place, and a field of it
// the schema declares is drawn there as a row of this file, and only there
// (V1SET.claimed, v1setFillSlots).
//
// A field the box refused keeps what was typed, with the box's reason,
// until it is saved or discarded (V1SET.pending), also across a redraw.
// Rows read text from the box only through webuiEl, never as markup.

var V1SET = {
	ok: false,			// schema and values read
	items: Object.create(null),	// key -> schema item
	order: [],			// section ids, in the order the box names them
	bySection: Object.create(null),	// section id -> [item]
	values: Object.create(null),	// key -> value as last read
	pending: Object.create(null),	// key -> {value, error} after a refused save
	cleared: Object.create(null),	// credentials emptied here, so known to hold nothing
	claimed: Object.create(null),	// keys a kept registry category draws
	dissolved: Object.create(null),	// registry category id -> section it went into
	regLabels: Object.create(null),	// key -> registry label, for rows without one
	keys: null,			// [{code, name}] the box names for key rows, null when unread
	channels: Object.create(null),	// channel list -> promise of [{id, name}], read when first drawn
	unavailable: Object.create(null),	// keys the box declares and does not have
	saving: false,
	rereadDue: false,
	opened: false,
	reads: 0,		// reads of the values started
	applied: 0,		// the read whose values the page holds, by its start
	saves: 0,		// saves started
	helpOpen: Object.create(null)	// keys whose help is open, kept across a redraw
};

// Starts a read of every section's values; its answer is taken only if no
// save started meanwhile and no read that started later was taken already.
// A later read that fails takes nothing away from an earlier one.
async function _v1setReadFresh() {
	var mine = ++V1SET.reads;
	var saves = V1SET.saves;
	var values = await _v1setReadValues();
	if (saves !== V1SET.saves || mine < V1SET.applied)
		return null;
	V1SET.applied = mine;
	return values;
}

// sidebar group of a section (SETTINGS_GROUPS); one the page does not know
// yet goes under System
var V1SET_GROUPS = {
	video: 'av', audio: 'av', player: 'av', recording: 'av',
	display: 'disp',
	general: 'sys', misc: 'sys', parental: 'sys', cam: 'sys', hdd: 'sys', update: 'sys',
	network: 'net', weather: 'net',
	osd: 'ui', channel: 'ui', keybindings: 'ui'
};

// the order of the sections the page knows, within their group; one it does
// not know follows in the box's order
var V1SET_ORDER = ['video', 'audio', 'player', 'recording', 'display',
	'general', 'misc', 'parental', 'cam', 'hdd', 'update',
	'network', 'weather', 'osd', 'channel', 'keybindings'];

// English names of the sections the box declares today; the language files
// carry them as yw.set.sec_<id>
var V1SET_TITLES = {
	general: 'General', recording: 'Recording', audio: 'Audio',
	osd: 'On-screen display', misc: 'Miscellaneous', video: 'Video',
	channel: 'Channel list', network: 'Network', weather: 'Weather',
	keybindings: 'Keys', display: 'Front display', player: 'Players',
	parental: 'Parental lock', cam: 'CI / CAM', hdd: 'Drives', update: 'Software update'
};

var V1SET_ICONS = {
	general: regIcon('<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>'),
	recording: regIcon('<circle cx="12" cy="12" r="10"/><circle cx="12" cy="12" r="3"/>'),
	osd: regIcon('<rect x="2" y="3" width="20" height="14" rx="2" ry="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/>'),
	channel: regIcon('<line x1="8" y1="6" x2="21" y2="6"/><line x1="8" y1="12" x2="21" y2="12"/><line x1="8" y1="18" x2="21" y2="18"/><line x1="3" y1="6" x2="3.01" y2="6"/><line x1="3" y1="12" x2="3.01" y2="12"/><line x1="3" y1="18" x2="3.01" y2="18"/>'),
	network: regIcon('<path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/>'),
	weather: regIcon('<path d="M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z"/>'),
	player: regIcon('<circle cx="12" cy="12" r="10"/><polygon points="10 8 16 12 10 16 10 8"/>'),
	parental: regIcon('<rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>'),
	cam: regIcon('<rect x="1" y="4" width="22" height="16" rx="2" ry="2"/><line x1="1" y1="10" x2="23" y2="10"/>'),
	hdd: regIcon('<line x1="22" y1="12" x2="2" y2="12"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" y1="16" x2="6.01" y2="16"/><line x1="10" y1="16" x2="10.01" y2="16"/>'),
	update: regIcon('<polyline points="8 17 12 21 16 17"/><line x1="12" y1="12" x2="12" y2="21"/><path d="M20.88 18.09A5 5 0 0 0 18 9h-1.26A8 8 0 1 0 3 16.29"/>')
};
// sections that take the icon of the registry category they replace
var V1SET_ICON_FROM = {video: 'video', audio: 'audio', display: 'lcd', keybindings: 'buttons', misc: 'system'};

var V1SET_INFO_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>';
var V1SET_RESTART_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/></svg>';

function v1setTitle(section) {
	return webuiL('set.sec_' + section, V1SET_TITLES[section] ||
		section.charAt(0).toUpperCase() + section.slice(1));
}

function v1setGroup(section) {
	return V1SET_GROUPS[section] || 'sys';
}

function v1setIcon(section) {
	if (V1SET_ICONS[section])
		return V1SET_ICONS[section];
	var from = V1SET_ICON_FROM[section];
	var cat = from && SETTINGS_REGISTRY.find(function(c) { return c.id === from; });
	return (cat && cat.icon) || SETTINGS_DEFAULT_ICON;
}

// A registry category that writes neutrino.conf and is not a panel of its own
function _v1setNeutrinoCat(cat) {
	return !cat.custom && (!cat.config || cat.config === 'neutrino');
}

async function _v1setReadValues() {
	var reads = await Promise.all(V1SET.order.map(function(section) {
		return webuiV1('GET', '/settings/' + encodeURIComponent(section));
	}));
	var values = Object.create(null);
	reads.forEach(function(r) {
		((r && r.items) || []).forEach(function(it) { values[it.id] = it.value; });
	});
	return values;
}

// Reads the schema (once) and every section's values; false when the box
// has no settings interface, and the page then keeps to the registry. Once
// the schema is read it stays: a failed read of the values later keeps the
// last ones and every refused input, and says so.
async function v1setLoad() {
	var had = V1SET.ok;
	try {
		if (!had) {
			var schema = await webuiV1('GET', '/settings/schema');
			var sections = await webuiV1('GET', '/settings/sections');
			V1SET.items = Object.create(null);
			V1SET.unavailable = Object.create(null);
			V1SET.bySection = Object.create(null);
			V1SET.order = ((sections && sections.items) || []).map(function(s) { return String(s.id); });
			var lists = (schema && schema.value_lists) || {};
			((schema && schema.items) || []).forEach(function(item) {
				// a section's id goes into the sidebar's markup and the
				// address; one that is not a plain name is left out
				if (!/^[A-Za-z0-9_-]+$/.test(item.section))
					return;
				// a setting this box does not have is not shown at all, and
				// not by the old registry either (v1setDeclared)
				if (item.available === false) {
					V1SET.unavailable[item.id] = true;
					return;
				}
				// a list several rows share is stated once, beside the rows
				if (item.values_from && Array.isArray(lists[item.values_from]))
					item.values = lists[item.values_from];
				V1SET.items[item.id] = item;
				if (!V1SET.bySection[item.section]) {
					V1SET.bySection[item.section] = [];
					if (V1SET.order.indexOf(item.section) < 0)
						V1SET.order.push(item.section);
				}
				V1SET.bySection[item.section].push(item);
			});
			// a section listed without a single setting has nothing to show
			V1SET.order = V1SET.order.filter(function(s) { return V1SET.bySection[s]; });
			// the names of the remote control's keys, for the key rows; a box
			// that has none to give leaves them number fields
			if (Object.keys(V1SET.items).some(function(k) { return V1SET.items[k].type === 'key'; })) {
				try {
					var keys = await webuiV1('GET', '/settings/keys');
					V1SET.keys = (keys && Array.isArray(keys.items)) ? keys.items : null;
				} catch (e) {
					V1SET.keys = null;
				}
			}
		}
		var values = await _v1setReadFresh();
		if (values)
			V1SET.values = values;
		V1SET.ok = true;
	} catch (e) {
		if (had)
			webuiShowToast(webuiL('set.reread_fail', 'Reading the settings again failed; they show what was read before'), true);
		else
			V1SET.ok = false;
	}
	_v1setArrange();
	return V1SET.ok;
}

// Which registry categories go into a section, which keys a kept one draws
// itself, and the registry's labels for rows the box gives none.
function _v1setArrange() {
	V1SET.claimed = Object.create(null);
	V1SET.dissolved = Object.create(null);
	V1SET.regLabels = Object.create(null);
	if (!V1SET.ok)
		return;
	SETTINGS_REGISTRY.forEach(function(cat) {
		if (!_v1setNeutrinoCat(cat))
			return;
		var into = cat.v1section && V1SET.bySection[cat.v1section] ? cat.v1section : null;
		if (into)
			V1SET.dissolved[cat.id] = into;
		(cat.sections || []).forEach(function(sec) {
			sec.fields.forEach(function(f) {
				if (!f.key || !V1SET.items[f.key])
					return;
				if (!into)
					V1SET.claimed[f.key] = true;
				if (!V1SET.regLabels[f.key] && typeof f.label === 'string')
					V1SET.regLabels[f.key] = f.label;
			});
		});
	});
}

// The categories of the sidebar: the box's sections first, then what the
// registry keeps. A registry category named like a section of the box gives
// way to it (today only the ones dissolved into it are named so).
function v1setCategories() {
	var cats = [];
	var rank = function(section) {
		var i = V1SET_ORDER.indexOf(section);
		return i < 0 ? V1SET_ORDER.length + V1SET.order.indexOf(section) : i;
	};
	if (V1SET.ok)
		V1SET.order.slice().sort(function(a, b) { return rank(a) - rank(b); }).forEach(function(section) {
			cats.push({id: section, label: v1setTitle(section), group: v1setGroup(section),
				icon: v1setIcon(section), v1: true});
		});
	SETTINGS_REGISTRY.forEach(function(cat) {
		if (V1SET.ok && (V1SET.dissolved[cat.id] || V1SET.bySection[cat.id]))
			return;
		cats.push(cat);
	});
	return cats;
}

// Whether the box declares a key, shown or not: the old registry draws none
// of these, or a setting the box lacks would come back as an old field and
// be written past every check of the schema.
function v1setDeclared(key) {
	return !!(V1SET.items[key] || V1SET.unavailable[key]);
}

// ---- pairs ------------------------------------------------------------------

// The other half of a setting that is one fact in two parts and how the box
// takes the two; null for a setting in no pair the page holds both halves of.
function _v1setPair(item) {
	if (!item || !item.pair || !V1SET.items[item.pair])
		return null;
	if (item.pair_writes !== 'id' && item.pair_writes !== 'both')
		return null;
	return {partner: item.pair, writes: item.pair_writes};
}

// A start channel is one line: its named half chooses a channel and writes
// the channel's identifier, and the box writes the name from its channel
// list. The identifier's half, which has no label, is not drawn on its own.
function _v1setIdHalf(item) {
	var p = _v1setPair(item);
	return !!(p && p.writes === 'id' && !item.label);
}


// The setting a row's control writes: the identifier for the named half of a
// start channel, the row's own setting otherwise.
function _v1setWrites(item) {
	var p = _v1setPair(item);
	return (p && p.writes === 'id' && item.label) ? p.partner : item.id;
}

// an identifier as the box stores it: hexadecimal, small, no leading zeros
function _v1setChannelId(v) {
	return String(v === undefined || v === null ? '' : v).toLowerCase().replace(/^0+(?=.)/, '');
}

// Every channel of one list, read a page at a time the first time a start
// channel of that list is drawn and kept for the page's life; a read that
// fails is tried again the next time.
function _v1setChannels(kind) {
	if (!V1SET.channels[kind]) {
		V1SET.channels[kind] = (async function() {
			var out = [];
			var cursor = null;
			for (var pages = 0; pages < 200; pages++) {
				var query = {mode: kind, limit: 500};
				if (cursor)
					query.cursor = cursor;
				var page = await webuiV1('GET', '/channels', {query: query});
				((page && page.items) || []).forEach(function(c) {
					out.push({id: _v1setChannelId(c.id), name: String(c.name || '')});
				});
				cursor = page && page.next_cursor;
				if (!cursor)
					break;
			}
			return out;
		})();
		V1SET.channels[kind].catch(function() { delete V1SET.channels[kind]; });
	}
	return V1SET.channels[kind];
}

// The named half of a start channel: a choice of the channels of its list
// whose value is the identifier the box takes. What it holds is offered at
// once under the name the box keeps beside it, the list follows once read; a
// list that cannot be read leaves the identifier to be typed.
function _v1setChannelChoice(item, idKey, id) {
	var none = webuiL('set.no_channel', 'none');
	var current = _v1setChannelId(_v1setShown(V1SET.items[idKey]));
	var name = _v1setShown(item);
	var sel = webuiEl('select', {id: id, data: {v1key: idKey}});
	sel.appendChild(webuiEl('option', {value: current}, current === '0' ? none : (name || current)));
	sel.value = current;
	_v1setChannels(item.channel_kind === 'radio' ? 'radio' : 'tv').then(function(list) {
		if (!sel.isConnected)
			return;
		var keep = sel.value;
		while (sel.firstChild)
			sel.removeChild(sel.firstChild);
		sel.appendChild(webuiEl('option', {value: '0'}, none));
		var listed = keep === '0';
		list.forEach(function(c) {
			listed = listed || c.id === keep;
			sel.appendChild(webuiEl('option', {value: c.id}, c.name || c.id));
		});
		// a channel the list has lost since stays what it is, under its name
		if (!listed)
			sel.appendChild(webuiEl('option', {value: keep},
				(name || keep) + ' (' + webuiL('set.unknown_value', 'not offered') + ')'));
		sel.value = keep;
	}, function() {
		if (!sel.isConnected)
			return;
		var input = webuiEl('input', {type: 'text', id: id, value: sel.value, data: {v1key: idKey},
			attrs: {spellcheck: 'false'}});
		input.disabled = sel.disabled;
		if (sel.dataset.v1fixed)
			input.dataset.v1fixed = sel.dataset.v1fixed;
		sel.replaceWith(input);
	});
	return sel;
}

// ---- rows -----------------------------------------------------------------

function v1setId(key) {
	return 'v1_' + String(key).replace(/[^A-Za-z0-9_-]/g, '_');
}

function _v1setIsOn(v) {
	return v === '1' || v === 'true';
}

// a value as compared: a switch reads 1 or 0 whichever way the box spells it
function _v1setNorm(item, v) {
	v = (v === undefined || v === null) ? '' : String(v);
	return item.type === 'bool' ? (_v1setIsOn(v) ? '1' : '0') : v;
}

// what a row shows: the refused input while there is one, else the box's value
function _v1setShown(item) {
	var p = V1SET.pending[item.id];
	if (p)
		return p.value;
	var v = V1SET.values[item.id];
	return (v === undefined || v === null) ? '' : String(v);
}

function _v1setControlValue(el) {
	return el.type === 'checkbox' ? (el.checked ? '1' : '0') : el.value;
}

function _v1setControls() {
	return document.querySelectorAll('#settingsContent [data-v1key]');
}

function _v1setControlOf(key) {
	var list = _v1setControls();
	for (var i = 0; i < list.length; i++)
		if (list[i].dataset.v1key === key)
			return list[i];
	return null;
}

/* Settings the box offers on no screen, and so names no label for, that are
   still a choice somebody makes: our own words for them. Every other row the
   box names no label for is state it keeps for itself (the volume it is at,
   the list it showed last, a directory it used last, the theme it loaded) and
   is not drawn (v1setPanel). */
var V1SET_OWN_LABELS = {
	'epg_scan_rescan': ['set.own.epg_rescan', 'Repeat the standby EPG scan after (hours)'],
	'bigFonts': ['set.own.bigfonts', 'Event details in the large window'],
	'adzap_zapBackPeriod': ['set.own.adzap_back', 'AdZap: switch back after'],
	'infobar_show_channeldesc': ['set.own.infobar_desc', 'Channel description in the infobar'],
	'movieplayer_select_ac3_atype0': ['set.own.ac3_untyped', 'Treat an audio track of no stated kind as AC3'],
	'recording_type': ['set.own.recording_type', 'Recording'],
	'recording_stopsectionsd': ['set.own.stop_epg', 'Stop the EPG service while recording'],
	'recordingmenu.stream_pmt_pid': ['set.own.stream_pmt', 'Record the PMT PID too'],
	'recording_choose_direct_rec_dir': ['set.own.rec_dir_choice', 'Choose the folder when recording from the guide (0 no, 1 from the shares, 2 any folder)'],
	'minimode': ['set.own.minimode', 'Simple mode: lists without recording and bouquet functions']
};

function _v1setOwnLabel(id) {
	var own = Object.prototype.hasOwnProperty.call(V1SET_OWN_LABELS, id) ? V1SET_OWN_LABELS[id] : null;
	return own ? webuiL(own[0], own[1]) : '';
}

/* The two halves of a pair the box writes together (pair_writes both), named
   alike by the box because its menu has one item for them: the half below
   says what it holds. The page saves either half with the other (see the
   save below), so both stay fields of their own. */
var V1SET_PART_LABELS = {
	'weather_location': ['set.own.weather_coords', 'Weather location: coordinates']
};

function _v1setLabel(item) {
	var part = Object.prototype.hasOwnProperty.call(V1SET_PART_LABELS, item.id) ? V1SET_PART_LABELS[item.id] : null;
	var partner = part && item.pair ? V1SET.items[item.pair] : null;
	if (part && item.label && partner && partner.label === item.label)
		return webuiL(part[0], part[1]);
	return item.label || V1SET.regLabels[item.id] || _v1setOwnLabel(item.id) || item.id;
}

function _v1setChoice(sel, v) {
	for (var i = 0; i < sel.options.length; i++)
		if (sel.options[i].value === v)
			return;
	// a value the box holds but does not offer stays what it is
	sel.appendChild(webuiEl('option', {value: v},
		(v === '' ? '–' : v) + ' (' + webuiL('set.unknown_value', 'not offered') + ')'));
}

function _v1setSetControl(el, item, v) {
	v = (v === undefined || v === null) ? '' : String(v);
	if (el.type === 'checkbox') {
		el.checked = _v1setIsOn(v);
		return;
	}
	if (el.tagName === 'SELECT')
		_v1setChoice(el, v);
	el.value = v;
	// the named value's meaning beside a number follows what was put in
	if (el.list)
		el.dispatchEvent(new Event('input'));
}

// what a choice stands for: its text where the setting holds text (the box
// then says 0 as its number), else the number
function _v1setOptValue(c) {
	return (c.text !== undefined && c.text !== null) ? String(c.text) : String(c.value);
}

// The short unit a number is shown with, by the name the box gives it (the
// schema's unit). A name this page does not know shows nothing.
var V1SET_UNITS = {
	'unit.short.hour': 'h', 'unit.short.minute': 'min', 'unit.short.second': 's',
	'unit.short.millisecond': 'ms', 'unit.short.percent': '%', 'unit.short.megabyte': 'MB'
};

function _v1setUnit(item) {
	return (item.type === 'int' && item.unit && V1SET_UNITS[item.unit]) || '';
}

function _v1setDefaultText(item) {
	var d = (item.default === undefined || item.default === null) ? '' : String(item.default);
	if (item.type === 'bool')
		return _v1setIsOn(d) ? webuiL('reg.on', 'On') : webuiL('reg.off', 'Off');
	var o = (item.values || []).find(function(c) { return _v1setOptValue(c) === d; });
	if (o)
		return o.label || d;
	if (item.type === 'key' && V1SET.keys) {
		var k = V1SET.keys.find(function(n) { return String(n.code) === d; });
		if (k)
			return k.name || d;
	}
	if (d === '')
		return '–';
	var unit = _v1setUnit(item);
	return unit ? d + ' ' + unit : d;
}

// a folder the setting names (epg_dir, network_nfs_moviedir, update_dir_opkg):
// offer the folder picker beside the field; a file path gets none. The box's
// rule says so where the row has one, the name only where it has none.
function _v1setIsFolder(item) {
	if (item.text_kind)
		return item.text_kind === 'directory';
	return /(^|_)dir_|dir$/i.test(item.id);
}

// What the box's rule for a text lets the browser hold the field to: its
// length, its characters and, for digits, the keypad. The box checks the
// same rule again on every write.
function _v1setTextAttrs(item, attrs) {
	if (item.max_length > 0)
		attrs.maxlength = item.max_length;
	if (item.min_length > 0)
		attrs.minlength = item.min_length;
	if (item.allowed_chars) {
		var chars = item.allowed_chars.replace(/[\\\]\[^-]/g, '\\$&');
		attrs.pattern = '[' + chars + ']*';
		attrs.title = webuiL('set.allowed_chars', 'Allowed characters') + ': ' + item.allowed_chars;
	}
	if (item.text_kind === 'pin' || item.text_kind === 'number')
		attrs.inputmode = 'numeric';
	return attrs;
}

function _v1setControl(item, id, val) {
	var data = {v1key: item.id};
	var pair = _v1setPair(item);
	if (pair && pair.writes === 'id' && item.label)
		return _v1setChannelChoice(item, pair.partner, id);
	if (item.type === 'bool')
		return webuiEl('label', {className: 'webui-toggle'},
			webuiEl('input', {type: 'checkbox', id: id, checked: _v1setIsOn(val), data: data}),
			webuiEl('span', {className: 'webui-toggle-slider'}));
	// A choice the box has none of (an output it lacks) is shown, never written;
	// so is a listed number with an empty list. A text row whose list comes
	// empty is one the box cannot say anything about: it stays free text,
	// which the box takes then too.
	if ((item.type === 'enum' || item.listed) && !(item.values && item.values.length)) {
		return webuiEl('select', {id: id, disabled: true, data: {v1key: item.id, v1fixed: '1'}},
			webuiEl('option', {value: val}, (val === '' ? '' : val + ' \u2013 ') + webuiL('set.notavail', 'Not available')));
	}
	if (item.type === 'key' && V1SET.keys && V1SET.keys.length) {
		// a key by its name; a code the box takes but names not stays
		var keySel = webuiEl('select', {id: id, data: data});
		V1SET.keys.forEach(function(k) {
			keySel.appendChild(webuiEl('option', {value: String(k.code)}, k.name || String(k.code)));
		});
		_v1setChoice(keySel, val);
		keySel.value = val;
		return keySel;
	}
	// a number with some values named: still any number, the names as
	// suggestions; only a listed one takes nothing but its list
	if ((item.type === 'int' || item.type === 'key') && !item.listed) {
		var attrs = {};
		if (item.min !== undefined) attrs.min = item.min;
		if (item.max !== undefined) attrs.max = item.max;
		var named = (item.values || []).filter(function(c) { return c.text === undefined; });
		var unit = _v1setUnit(item);
		var unitText = unit ? webuiEl('span', {className: 'webui-set-unit', attrs: {'aria-hidden': 'true'}}, unit) : null;
		if (unit)
			attrs['aria-label'] = _v1setLabel(item) + ' (' + unit + ')';
		if (!named.length) {
			var number = webuiEl('input', {type: 'number', id: id, value: val, data: data, attrs: attrs});
			return unit ? webuiEl('span', {className: 'webui-set-number'}, number, unitText) : number;
		}
		attrs.list = id + '_names';
		var names = webuiEl('datalist', {id: id + '_names'});
		named.forEach(function(c) {
			names.appendChild(webuiEl('option', {value: String(c.value)}, c.label || String(c.value)));
		});
		// what a named value means, beside it, so -1 reads as what it stands for
		var meaning = webuiEl('span', {className: 'webui-set-named webui-text-muted', id: id + '_meaning', attrs: {'aria-live': 'polite'}});
		attrs['aria-describedby'] = id + '_meaning';
		var input = webuiEl('input', {type: 'number', id: id, value: val, data: data, attrs: attrs});
		var say = function() {
			var c = named.find(function(n) { return String(n.value) === String(input.value); });
			meaning.textContent = c && c.label ? '= ' + c.label : '';
		};
		input.addEventListener('input', say);
		say();
		return webuiEl('span', {className: unit ? 'webui-set-number' : ''}, input, unitText, meaning, names);
	}
	if (item.values && item.values.length) {
		var sel = webuiEl('select', {id: id, data: data});
		item.values.forEach(function(c) {
			var v = _v1setOptValue(c);
			sel.appendChild(webuiEl('option', {value: v}, c.label || v));
		});
		_v1setChoice(sel, val);
		sel.value = val;
		return sel;
	}
	if (item.secret) {
		// a credential reads as nothing: empty leaves it as it is, and
		// emptying it is a button of its own
		var pw = webuiEl('input', {type: 'password', id: id, value: val, data: data,
			attrs: _v1setTextAttrs(item, {autocomplete: 'new-password', placeholder: webuiL('set.secret_keep', 'unchanged')})});
		var cell = webuiEl('span', {}, pw);
		if (typeof togglePwField === 'function') {
			var eye = webuiEl('button', {type: 'button', className: 'webui-btn webui-set-act',
				attrs: {'aria-pressed': 'false', 'aria-label': webuiL('set.pw_show', 'Show password')},
				on: {click: function() { togglePwField(id, eye); }}}, webuiMarkup(EYEOFF_ICON));
			cell.appendChild(eye);
		}
		cell.appendChild(webuiEl('button', {type: 'button', className: 'webui-btn webui-set-act',
			on: {click: function() { v1setClearSecret(item); }}}, webuiL('set.secret_clear', 'Clear')));
		return cell;
	}
	var input = webuiEl('input', {type: 'text', id: id, value: val, data: data, attrs: _v1setTextAttrs(item, {})});
	if (!_v1setIsFolder(item) || typeof webuiPickInto !== 'function')
		return input;
	return webuiEl('span', {}, input, webuiEl('button', {type: 'button', className: 'webui-btn webui-set-act',
		title: webuiL('picker.title', 'Choose folder'), attrs: {'aria-label': webuiL('picker.title', 'Choose folder')},
		on: {click: function() { webuiPickInto(id, input.value || '/'); }}}, webuiMarkup(FOLDER_ICON)));
}

// A row the parental lock holds is shown and not written: its fields and
// buttons stay locked whatever its conditions say.
function _v1setLock(control) {
	var list = control.matches('[data-v1key], .webui-set-act') ? [control] :
		Array.prototype.slice.call(control.querySelectorAll('[data-v1key], .webui-set-act'));
	list.forEach(function(el) {
		el.disabled = true;
		el.dataset.v1fixed = '1';
	});
}

function v1setRow(item) {
	var id = v1setId(item.id);
	var label = _v1setLabel(item);
	var control = _v1setControl(item, id, _v1setShown(item));
	if (item.locked)
		_v1setLock(control);
	var labelCell = webuiEl('div', {className: 'webui-set-label'}, webuiEl('label', {htmlFor: id}, label));
	if (item.locked)
		labelCell.appendChild(webuiEl('span', {className: 'webui-set-locked'},
			webuiL('set.locked', 'locked by the parental lock')));
	var restart = webuiL('set.restart_title', 'Takes effect after a restart');
	if (item.needs_restart)
		labelCell.appendChild(webuiEl('span', {className: 'webui-set-restart', title: restart,
			attrs: {role: 'img', 'aria-label': restart}}, webuiMarkup(V1SET_RESTART_ICON)));

	// the help the box shows beside the setting, opened by the info button
	var help = null;
	if (item.hint_text) {
		var open = !!V1SET.helpOpen[item.id];
		help = webuiEl('div', {className: 'webui-set-help', id: id + '_help', hidden: !open},
			webuiEl('p', {}, item.hint_text),
			item.needs_restart ? webuiEl('p', {}, restart) : null,
			item.secret ? null : webuiEl('p', {className: 'webui-set-default'},
				webuiL('set.default', 'Default') + ': ' + _v1setDefaultText(item)));
		var info = webuiEl('button', {type: 'button', className: 'webui-set-info',
			title: webuiL('set.help', 'Help'),
			attrs: {'aria-expanded': open ? 'true' : 'false', 'aria-controls': id + '_help',
				'aria-label': webuiL('set.help', 'Help') + ': ' + label},
			on: {click: function() {
				help.hidden = !help.hidden;
				V1SET.helpOpen[item.id] = !help.hidden;
				info.setAttribute('aria-expanded', help.hidden ? 'false' : 'true');
			}}}, webuiMarkup(V1SET_INFO_ICON));
		labelCell.appendChild(info);
	}

	var row = webuiEl('div', {className: 'webui-form-row', data: {v1row: item.id}}, labelCell, control, help);
	var p = V1SET.pending[_v1setWrites(item)];
	if (p && p.error) {
		row.appendChild(webuiEl('div', {className: 'webui-set-error', id: id + '_err'}, p.error));
		var field = row.querySelector('[data-v1key]');
		if (field) {
			field.setAttribute('aria-invalid', 'true');
			field.setAttribute('aria-describedby', id + '_err');
		}
	}
	return row;
}

// One section of the box as a card; extra is the registry's markup for what
// the schema lacks, drawn below.
function v1setPanel(section, extra) {
	var rows = webuiEl('div', {className: 'webui-form-section'});
	(V1SET.bySection[section] || []).forEach(function(item) {
		// a setting no menu of the box names and none of our own words name
		// is state the box keeps for itself, and not a choice: not drawn
		if (!item.label && !V1SET.regLabels[item.id] && !_v1setOwnLabel(item.id))
			return;
		if (!V1SET.claimed[item.id] && !_v1setIdHalf(item))
			rows.appendChild(v1setRow(item));
	});
	var card = webuiEl('div', {className: 'settings-card'},
		webuiEl('div', {className: 'settings-card-header'}, webuiEl('h3', {}, v1setTitle(section))), rows);
	if (extra) {
		var more = webuiEl('div', {});
		more.innerHTML = extra;	// registry markup from the page source
		card.appendChild(more);
	}
	return card;
}

// a kept registry category leaves a slot for each key the schema declares
function v1setSlot(key) {
	return '<div data-v1-slot="' + escapeHtml(key) + '"></div>';
}

function v1setFillSlots(root) {
	root.querySelectorAll('[data-v1-slot]').forEach(function(slot) {
		var item = V1SET.items[slot.getAttribute('data-v1-slot')];
		if (item && !_v1setIdHalf(item))
			slot.replaceWith(v1setRow(item));
		else
			slot.remove();
	});
}

// ---- conditions -------------------------------------------------------------

function _v1setLive(key) {
	var el = _v1setControlOf(key);
	if (el)
		return _v1setControlValue(el);
	return _v1setShown(V1SET.items[key] || {id: key});
}

function _v1setHolds(c) {
	// a group holds while any one of its comparisons does
	if (c.any)
		return c.any.some(_v1setHolds);
	// A credential reads as nothing, so only what was typed into it here can
	// be judged; untouched, it is the box that judges it on the write.
	var target = V1SET.items[c.key];
	if (target && target.secret) {
		var field = _v1setControlOf(c.key);
		var typed = field ? _v1setControlValue(field) : '';
		if (c.op !== 'text-valid')
			return true;
		// emptied here, so known to hold nothing
		if (!typed && V1SET.cleared[c.key])
			return false;
		if (!typed)
			return true;
		return typed !== (c.text || '');
	}
	if (c.op === 'text-valid') {
		var text = String(_v1setLive(c.key) || '');
		return text !== '' && text !== (c.text || '');
	}
	var v = parseInt(_v1setLive(c.key), 10);
	if (isNaN(v))
		return true;	// nothing to hold it against
	var n = c.values || [];
	switch (c.op) {
	case 'eq': return v === n[0];
	case 'ne': return v !== n[0];
	case 'lt': return v < n[0];
	case 'le': return v <= n[0];
	case 'gt': return v > n[0];
	case 'ge': return v >= n[0];
	case 'in': return n.indexOf(v) >= 0;
	}
	return true;
}

// A row whose conditions do not hold has no effect: dimmed and locked, its
// help still open to read, and the title names what it waits for.
function v1setApplyConditions() {
	document.querySelectorAll('#settingsContent [data-v1row]').forEach(function(row) {
		var item = V1SET.items[row.dataset.v1row];
		if (!item || !item.conditions || !item.conditions.length)
			return;
		var failing = item.conditions.filter(function(c) { return !_v1setHolds(c); });
		var off = failing.length > 0;
		row.classList.toggle('is-disabled', off);
		if (off)
			row.title = webuiL('set.cond_off', 'No effect with the current value of') + ': ' +
				failing.reduce(function(keys, c) {
					return keys.concat(c.any ? c.any.map(function(a) { return a.key; }) : [c.key]);
				}, []).map(function(key) {
					var it = V1SET.items[key];
					return it ? _v1setLabel(it) : key;
				}).join(', ');
		else
			row.removeAttribute('title');
		row.querySelectorAll('[data-v1key]:not([data-v1fixed]), .webui-set-act:not([data-v1fixed])').forEach(function(el) { el.disabled = off; });
	});
}

// ---- changes and saving -------------------------------------------------------

// The changed rows, by section: {bySection: {section: {key: value}}, count}.
// A refused value whose panel is not drawn (the redraw after a save draws
// only the open one) is a change as well, and goes into the next save.
function v1setDirty() {
	var out = {bySection: Object.create(null), count: 0, brought: Object.create(null)};
	if (!V1SET.ok)
		return out;
	var add = function(item, v) {
		if (_v1setNorm(item, v) === _v1setNorm(item, V1SET.values[item.id]))
			return;
		if (!out.bySection[item.section])
			out.bySection[item.section] = {};
		out.bySection[item.section][item.id] = v;
		out.count++;
	};
	var drawn = Object.create(null);
	_v1setControls().forEach(function(el) {
		var item = V1SET.items[el.dataset.v1key];
		if (!item)
			return;
		drawn[item.id] = true;
		add(item, _v1setControlValue(el));
	});
	Object.keys(V1SET.pending).forEach(function(key) {
		if (!drawn[key] && V1SET.items[key])
			add(V1SET.items[key], V1SET.pending[key].value);
	});
	// The two halves of a place are taken together or not at all, so a half
	// changed brings the other as it stands.
	Object.keys(out.bySection).forEach(function(section) {
		var body = out.bySection[section];
		Object.keys(body).forEach(function(key) {
			var p = _v1setPair(V1SET.items[key]);
			if (p && p.writes === 'both' && body[p.partner] === undefined &&
			    V1SET.items[p.partner].section === section) {
				body[p.partner] = _v1setLive(p.partner);
				out.brought[p.partner] = true;
			}
		});
	});
	return out;
}

// A field the box refused that the user edits again is an ordinary field
// from then on: its value is what it shows, the old reason goes.
function v1setEdited(el) {
	var key = el && el.dataset && el.dataset.v1key;
	if (!key || !V1SET.pending[key])
		return;
	delete V1SET.pending[key];
	el.removeAttribute('aria-invalid');
	el.removeAttribute('aria-describedby');
	var row = el.closest('[data-v1row]');
	var err = row && row.querySelector('.webui-set-error');
	if (err)
		err.remove();
}

// Writes each changed section. A key the box refuses goes into
// V1SET.pending with its reason; the caller reads everything again after.
// -> {saved, failed, restart}
async function v1setSave(dirty) {
	var res = {saved: 0, failed: 0, restart: false};
	V1SET.saving = true;
	++V1SET.saves;	// a read in flight predates what this writes
	try {
		var sections = Object.keys(dirty.bySection);
		for (var i = 0; i < sections.length; i++) {
			var section = sections[i];
			var body = dirty.bySection[section];
			var keys = Object.keys(body);
			var failed = Object.create(null);
			try {
				var answer = await webuiV1('PATCH', '/settings/' + encodeURIComponent(section), {body: body});
				// 207: some keys landed, others name their refusal
				if (answer && answer.results)
					keys.forEach(function(k) {
						var r = answer.results[k];
						if (r && r.status >= 300)
							failed[k] = r.detail || r.code || String(r.status);
						// a start channel whose name the box could not store
						// is not the channel the box starts on
						var p = _v1setPair(V1SET.items[k]);
						var named = p && p.writes === 'id' ? answer.results[p.partner] : null;
						if (failed[k] === undefined && named && named.status >= 300)
							failed[k] = named.detail || named.code || String(named.status);
					});
			} catch (e) {
				// one key refused, or the request as a whole
				keys.forEach(function(k) { failed[k] = e.message; });
			}
			keys.forEach(function(k) {
				if (failed[k] !== undefined) {
					V1SET.pending[k] = {value: body[k], error: failed[k]};
					res.failed++;
					return;
				}
				delete V1SET.pending[k];
				// a credential just written holds what was sent, not nothing
				delete V1SET.cleared[k];
				// the other half of a place went along and is no change of its own
				if (!dirty.brought || !dirty.brought[k])
					res.saved++;
				if (V1SET.items[k].needs_restart)
					res.restart = true;
				// a credential reads empty again once the box holds it, so
				// it is not sent a second time; a newer input stays
				var el = V1SET.items[k].secret ? _v1setControlOf(k) : null;
				if (el && el.value === body[k])
					el.value = '';
			});
		}
	} finally {
		V1SET.saving = false;
	}
	// what changed meanwhile is read by the caller's reload or refresh
	// right after (saveSettingsNow), so no reread of its own
	V1SET.rereadDue = false;
	return res;
}

// After a save whose old-interface part failed the form stays as it is: read
// the box's values again and show the refusals of the rows that have one.
async function v1setRefresh() {
	await v1setReread();
	document.querySelectorAll('#settingsContent [data-v1row]').forEach(function(row) {
		var item = V1SET.items[row.dataset.v1row];
		if (item && (V1SET.pending[_v1setWrites(item)] || row.querySelector('.webui-set-error')))
			row.replaceWith(v1setRow(item));
	});
	v1setApplyConditions();
}

function v1setDiscard() {
	V1SET.pending = Object.create(null);
}

function _v1setAsk(text, title, ok) {
	return new Promise(function(resolve) {
		webuiSheet({
			title: title,
			body: webuiEl('p', {}, text),
			actions: [
				{label: webuiL('msg.cancel', 'Cancel'), value: false},
				{label: ok, value: true, danger: true}
			],
			onClose: function(result) { resolve(result === true); }
		});
	});
}

async function v1setClearSecret(item) {
	var ok = await _v1setAsk(webuiL('set.secret_clear_ask', 'Clear the stored value? The box then holds none.'),
		_v1setLabel(item), webuiL('set.secret_clear', 'Clear'));
	if (!ok)
		return;
	// the form is locked as for a save, so no newer input meets the answer
	if (typeof settingsBusy === 'function')
		settingsBusy(true);
	try {
		await webuiV1('POST', '/settings/secret/clear', {body: {key: item.id}});
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
		return;
	} finally {
		if (typeof settingsBusy === 'function')
			settingsBusy(false);
	}
	delete V1SET.pending[item.id];
	V1SET.cleared[item.id] = true;
	var el = _v1setControlOf(item.id);
	if (el) {
		var row = el.closest('[data-v1row]');
		row.replaceWith(v1setRow(item));
		v1setApplyConditions();
	}
	webuiShowToast(webuiL('set.secret_cleared', 'Cleared'));
	if (typeof webuiUpdateSaveBar === 'function')
		webuiUpdateSaveBar();
}

// ---- changes made elsewhere -------------------------------------------------

var _v1setRereadTimer = null;

// settings-changed carries nothing: read the sections again. A row the user
// has changed, or that the box refused, keeps what it shows.
function v1setChanged() {
	if (!V1SET.ok)
		return;
	if (V1SET.saving) {
		V1SET.rereadDue = true;
		return;
	}
	clearTimeout(_v1setRereadTimer);
	_v1setRereadTimer = setTimeout(v1setReread, 400);
}

// the stream (re)connected: the first time is the page load, which read
// everything already
function v1setStreamOpen() {
	if (V1SET.opened)
		v1setChanged();
	V1SET.opened = true;
}

async function v1setReread() {
	var fresh;
	try {
		fresh = await _v1setReadFresh();
	} catch (e) {
		return;
	}
	// a save or a read that started later decides what the page shows
	if (!fresh)
		return;
	if (V1SET.saving) {
		V1SET.rereadDue = true;
		return;
	}
	var old = V1SET.values;
	_v1setControls().forEach(function(el) {
		var key = el.dataset.v1key;
		var item = V1SET.items[key];
		if (!item || item.secret || V1SET.pending[key])
			return;
		if (_v1setNorm(item, _v1setControlValue(el)) !== _v1setNorm(item, old[key]))
			return;
		_v1setSetControl(el, item, fresh[key]);
	});
	V1SET.values = fresh;
	v1setApplyConditions();
	if (typeof webuiUpdateSaveBar === 'function')
		webuiUpdateSaveBar();
}
