// The script of settings.html.

var configData = {};
var originalValues = {};
var activeCategory = '';
var renderedPanels = {};
// Aktivierte Erweiterungen mit settings-Definition im Manifest werden
// als eigene Kategorien eingeblendet (id 'ext-<dir>', Panel ueber den
// generischen ExtConfig-Zyklus gegen config=<name>, Default webui)
var EXT_SETTINGS_CATS = {};

// M5c: Karten-Kopf ist reiner Titel — gespeichert wird zentral über
// die Dirty-State-Leiste (#settingsSaveBar) am Fuß der Content-Spalte;
// Panels mit eigener Config-Datei (ExtConfig, Erweiterungen, WoL, ...)
// behalten ihren eigenen Save-Button im Panel-Inhalt.
function makeCardHeader(label) {
	var d = document.createElement('div');
	d.className = 'settings-card-header';
	var h = document.createElement('h3');
	h.textContent = label;
	d.appendChild(h);
	return d.outerHTML;
}

function getConfigKey(key) {
	var k = key.replace(/\./g, '_dot_').replace(/-/g, '_bind_');
	return configData[k] !== undefined ? configData[k] : '';
}

function hasConfigKey(key) {
	return configData[key.replace(/\./g, '_dot_').replace(/-/g, '_bind_')] !== undefined;
}

/* An extension's action buttons register here: each runs one of that
   extension's scripts, the one its api.script names (listed under "scripts"
   in its manifest), with api.args as its arguments, over /api/v1, and says
   what the script wrote, or the success text, as a toast. */
var SETTINGS_ACTIONS = {};
async function runSettingsAction(aid, btn) {
	var f = SETTINGS_ACTIONS[aid];
	if (!f || !f.api) return;
	if (typeof f.api.script !== 'string' || !f.extDir) {
		webuiShowToast(webuiL('set.ext_action_old', 'This action is written for an older extension contract and cannot run'), true);
		return;
	}
	if (f.confirm && !confirm(f.confirm)) return;
	var orig = btn.textContent;
	btn.disabled = true;
	btn.textContent = '...';
	try {
		var args = Array.isArray(f.api.args) ? f.api.args.map(String) : [];
		var ran = await webuiV1('POST', '/webui/extensions/' + encodeURIComponent(f.extDir) + '/scripts/' + encodeURIComponent(f.api.script), {body: args});
		var res = (ran && typeof ran.output === 'string') ? ran.output : '';
		webuiShowToast(f.success || res.trim() || webuiL('set.ext_action_done', 'Done'));
		if (f.refresh) await loadSettings();
		if (typeof f.after === 'function') f.after(res);
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
	btn.textContent = orig;
}

// Hinweis fuer Felder, die ein uebergeordneter Eintrag sperrt (disabledIf)
var DEP_HINT = '';	// set once the texts are in (start below)

// Aktueller Wert eines Keys: bevorzugt das bereits gerenderte Formularfeld
// (traegt eine noch ungespeicherte Aenderung), sonst die geladene Config
function getLiveValue(key) {
	var el = document.querySelector('#settingsContent [data-key="' + key + '"]:not([data-extcfg])');
	if (!el) return getConfigKey(key);
	if (el.type === 'checkbox')
		return el.checked ? el.getAttribute('data-on') : el.getAttribute('data-off');
	return el.value;
}

// disabledIf: {key, unless} — beobachtete Felder sperren, solange der
// Observer nicht auf dem Freigabewert steht. Der Observer kann in
// derselben Gruppe stehen, darum nach jeder Aenderung neu bewerten.
function webuiApplyDeps() {
	document.querySelectorAll('#settingsContent .webui-form-row[data-dep-key]').forEach(function(r) {
		var off = getLiveValue(r.getAttribute('data-dep-key')) !== r.getAttribute('data-dep-unless');
		r.classList.toggle('is-disabled', off);
		if (off) r.setAttribute('title', DEP_HINT);
		else r.removeAttribute('title');
		r.querySelectorAll('select, input').forEach(function(el) { el.disabled = off; });
	});
}

function renderField(f, opts) {
	opts = opts || {};
	var val = opts.get ? (opts.get(f.key) || '') : (f.key ? getConfigKey(f.key) : '');
	// Leeren Wert mit dem Feld-Default vorbelegen (Key noch nie gesetzt)
	if ((val === '' || val === undefined) && f.default !== undefined)
		val = String(f.default);
	/* What a row says is text, wherever it came from (the registry, an
	   extension's manifest, a translation), and escaped here, where it is put
	   into markup; the optional (i) hint follows it (native title tooltip). */
	var labelHtml = escapeHtml(f.label) +
		(f.hint ? ' <span class="webui-hint" title="' + escapeHtml(f.hint) + '">&#9432;</span>' : '');
	var id = ('cfg_' + (opts.idPrefix || '') + String(f.key || f.id || '').replace(/\./g, '__')).replace(/[^A-Za-z0-9_-]/g, '_');	// a manifest's key too
	var ext = opts.dataConfig ? ' data-extcfg="' + escapeHtml(opts.dataConfig) + '"' : '';

	// Observer-Abhaengigkeit: Zeile wird gerendert, aber gesperrt, solange
	// der beobachtete Key nicht den Freigabewert traegt. Kein Zwangsschreiben
	// des Werts — Neutrino ignoriert den Key in dem Fall ohnehin.
	var dis = '';
	var row = '<div class="webui-form-row">';
	if (f.disabledIf) {
		var off = getLiveValue(f.disabledIf.key) !== String(f.disabledIf.unless);
		dis = off ? ' disabled' : '';
		row = '<div class="webui-form-row' + (off ? ' is-disabled' : '') + '"' +
			' data-dep-key="' + escapeHtml(String(f.disabledIf.key)) + '" data-dep-unless="' + escapeHtml(String(f.disabledIf.unless)) + '"' +
			(off ? ' title="' + escapeHtml(DEP_HINT) + '"' : '') + '>';
	}

	if (f.type === 'info') {
		var text = (f.text !== undefined) ? f.text : val;
		return row + '<label>' + labelHtml + '</label>' +
			'<span id="' + id + '" style="color:var(--pico-muted-color)">' + escapeHtml(text || '--') + '</span></div>';
	}
	if (f.type === 'action') {
		// Einzel-Action oder Button-Gruppe (f.actions) in einer Zeile
		var acts = f.actions || [f];
		var btns = '';
		acts.forEach(function(a) {
			// an extension's action runs a script of that extension and of no other
			var aid = (opts.idPrefix || '') + (a.id || a.label || f.label);
			SETTINGS_ACTIONS[aid] = opts.extDir ? Object.assign({}, a, {extDir: opts.extDir}) : a;
			btns += '<button type="button" class="webui-btn" onclick="runSettingsAction(' + escapeHtml(JSON.stringify(aid)) + ', this)">' + escapeHtml(a.button || a.label || f.label) + '</button> ';
		});
		return row + '<label>' + labelHtml + '</label><span>' + btns.trim() + '</span></div>';
	}

	if (f.type === 'select') {
		var opts = f.options.map(function(o) {
			return '<option value="' + escapeHtml(o.v) + '"' + (val === o.v ? ' selected' : '') + '>' + escapeHtml(o.l) + '</option>';
		}).join('');
		return row + '<label>' + labelHtml + '</label>' +
			'<select id="' + id + '" data-key="' + escapeHtml(f.key) + '"' + ext + dis + '>' + opts + '</select></div>';
	}
	// ACCESS-Domaene (personalize.h): nur 0 = ohne PIN und 2 = mit PIN.
	// Der Eintrag ist immer sichtbar (personalize.cpp:1006), die 0 heisst
	// hier also NICHT 'ausgeblendet'. Zur Laufzeit wertet Neutrino nur die
	// 2 aus (personalize.cpp:961) — jeder andere Wert bedeutet 'ohne PIN'.
	if (f.type === 'access') {
		var pin = (val === '2');
		return row + '<label>' + labelHtml + '</label>' +
			'<select id="' + id + '" data-key="' + escapeHtml(f.key) + '"' + ext + dis + '>' +
			'<option value="0"' + (pin ? '' : ' selected') + '>' + webuiLH('pers.no', 'No') + '</option>' +
			'<option value="2"' + (pin ? ' selected' : '') + '>' + webuiLH('pers.yes', 'Yes') + '</option>' +
			'</select></div>';
	}
	if (f.type === 'bool') {
		var isTrue = (val === 'true' || val === '1');
		return row + '<label>' + labelHtml + '</label>' +
			'<label class="webui-toggle"><input type="checkbox" id="' + id + '" data-key="' + escapeHtml(f.key) + '" data-on="true" data-off="false"' + ext + dis + (isTrue ? ' checked' : '') + '>' +
			'<span class="webui-toggle-slider"></span></label></div>';
	}
	if (f.type === 'bool01') {
		var isOne = (val === '1');
		return row + '<label>' + labelHtml + '</label>' +
			'<label class="webui-toggle"><input type="checkbox" id="' + id + '" data-key="' + escapeHtml(f.key) + '" data-on="1" data-off="0"' + ext + dis + (isOne ? ' checked' : '') + '>' +
			'<span class="webui-toggle-slider"></span></label></div>';
	}
	if (f.type === 'number') {
		var attrs = 'type="number"';
		if (f.min !== undefined) attrs += ' min="' + escapeHtml(String(f.min)) + '"';
		if (f.max !== undefined) attrs += ' max="' + escapeHtml(String(f.max)) + '"';
		return row + '<label>' + labelHtml + '</label>' +
			'<input ' + attrs + ' id="' + id + '" data-key="' + escapeHtml(f.key) + '"' + ext + dis + ' value="' + escapeHtml(val) + '"></div>';
	}
	if (f.type === 'password') {
		var pattr = f.pattern ? ' pattern="' + escapeHtml(f.pattern) + '"' : '';
		if (f.maxlength) pattr += ' maxlength="' + escapeHtml(String(f.maxlength)) + '"';
		return row + '<label>' + labelHtml + '</label><span>' +
			'<input type="password" id="' + id + '" data-key="' + escapeHtml(f.key) + '"' + ext + dis + pattr + ' autocomplete="new-password" value="' + val.replace(/"/g, '&quot;') + '">' +
			' <button type="button" class="webui-btn" aria-pressed="false" aria-label="' + webuiLH('set.pw_show', 'Show password') + '" onclick="togglePwField(\'' + id + '\', this)">' + EYEOFF_ICON + '</button></span></div>';
	}
	// text — optional mit Verzeichnis-Picker (f.picker: true)
	var pick = '';
	if (f.picker) {
		var pstart = val || '/';
		pick = ' <button type="button" class="webui-btn" title="' + webuiLH('picker.title', 'Choose folder') +
			'" onclick="webuiPickInto(\'' + id + '\', ' + escapeHtml(JSON.stringify(pstart)) + ')">' + FOLDER_ICON + '</button>';
	}
	return row + '<label>' + labelHtml + '</label><span>' +
		'<input type="text" id="' + id + '" data-key="' + escapeHtml(f.key) + '"' + ext + dis + ' value="' + val.replace(/"/g, '&quot;') + '">' + pick + '</span></div>';
}

// kleines Ordner-Icon fuer Picker-Buttons
var FOLDER_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle"><path d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z"/></svg>';

// Auge-Icons fuer Passwortfelder (Feather-Geometrie wie bouquet-editor)
var EYE_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';
var EYEOFF_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;vertical-align:middle" aria-hidden="true"><path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/><line x1="1" y1="1" x2="23" y2="23"/></svg>';

// Konstanter Button-Name, Zustand via aria-pressed (a11y-Regel).
// The icon mirrors the state: crossed-out eye while masked.
function togglePwField(id, btn) {
	var el = document.getElementById(id);
	if (!el) return;
	var show = el.type === 'password';
	el.type = show ? 'text' : 'password';
	btn.setAttribute('aria-pressed', show ? 'true' : 'false');
	btn.innerHTML = show ? EYE_ICON : EYEOFF_ICON;
}

// M5c: 14px-Icon-Wrapper fuer Panel-Buttons (Optik wie Save-Leiste)
function sIcon(paths) {
	return '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' + paths + '<\/svg>';
}
var SAVE_ICON = sIcon('<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>');
var DL_ICON = sIcon('<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>');
var UP_ICON = sIcon('<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>');
var PLAY_ICON = sIcon('<polygon points="5 3 19 12 5 21 5 3"/>');
var REFRESH_ICON = sIcon('<polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/>');
var ZAP_ICON = sIcon('<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>');
var MOUNT_ICON = sIcon('<line x1="22" y1="12" x2="2" y2="12"/><path d="M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" y1="16" x2="6.01" y2="16"/><line x1="10" y1="16" x2="10.01" y2="16"/>');
var EJECT_ICON = sIcon('<path d="M12 5l7 9H5l7-9z"/><line x1="5" y1="19" x2="19" y2="19"/>');
var RESTORE_ICON = sIcon('<polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/>');
var TRASH_ICON = sIcon('<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>');

// Markup-Helfer: gestyltes Datei-Control (.webui-file aus webui.css);
// der gewählte Dateiname landet via onchange in .webui-file-name
function makeFileControl(inputId, accept) {
	return '<label class="webui-file"><input type="file" id="' + inputId + '"' +
		(accept ? ' accept="' + accept + '"' : '') + ' onchange="webuiFileName(this)">' +
		'<span class="webui-file-btn">' + webuiLH('set.choose_file', 'Choose file') + '</span>' +
		'<span class="webui-file-name">&ndash;</span></label>';
}
// webuiFileName (onchange-Companion des Controls) liefert webui-header.js

// Rechtsbuendiger Save-Button fuer Panels mit eigenem Config-Zyklus
// (ExtConfig, Erweiterungen, WoL) — Optik wie die zentrale Save-Leiste.
// display:flex inline, weil .webui-row-actions nur innerhalb von
// .webui-form-row automatisch flext (hier steht die Zeile solo)
function makePanelSaveRow(onclick, label) {
	return '<div class="webui-row-actions" style="display:flex;gap:0.5rem;justify-content:flex-end;margin-top:0.6rem">' +
		'<button type="button" class="webui-btn primary" onclick="' + onclick + '">' +
		SAVE_ICON + (label || webuiLH('bqe.save', 'Save')) + '</button></div>';
}

// Picker oeffnen und Ergebnis in das Textfeld schreiben
function webuiPickInto(inputId, startPath) {
	webuiShowPathPicker(startPath, function(p) {
		var el = document.getElementById(inputId);
		// bubbling, so the delegated listeners (save bar, conditions) see it
		if (el) { el.value = p; el.dispatchEvent(new Event('change', {bubbles: true})); }
	});
}

// What a registry category dissolved into this section of the box adds
// to it: its fields the schema lacks, if the box's file carries them
function renderLeftovers(section) {
	var html = '';
	SETTINGS_REGISTRY.forEach(function(cat) {
		if (V1SET.dissolved[cat.id] !== section) return;
		cat.sections.forEach(function(sec) {
			var body = '';
			sec.fields.forEach(function(f) {
				if (f.key && (v1setDeclared(f.key) || !hasConfigKey(f.key))) return;
				body += renderField(f);
			});
			if (body)
				html += '<div class="webui-form-section">' +
					'<div class="webui-form-section-title">' + escapeHtml(sec.title) + '</div>' + body + '</div>';
		});
	});
	return html;
}

// A section of the box comes back as an element (settings-v1.js), every
// other category as markup
function renderCategory(catId) {
	if (EXT_SETTINGS_CATS[catId]) return renderExtConfigPanel(EXT_SETTINGS_CATS[catId]);
	if (V1SET.ok && V1SET.bySection[catId]) return v1setPanel(catId, renderLeftovers(catId));
	var cat = SETTINGS_REGISTRY.find(function(c) { return c.id === catId; });
	if (!cat) return '';

	if (cat.custom && cat.id === 'mounts') return renderMountsPanel();
	if (cat.custom && cat.id === 'plugins') return renderPluginsPanel();
	if (cat.custom && cat.id === 'backup') return renderBackupPanel();
	if (cat.custom && cat.id === 'wol') return renderWolPanel();
	if (cat.custom && cat.id === 'xmltv') return renderXmltvPanel();
	if (cat.custom && cat.id === 'zapitupload') return renderZapitUploadPanel();
	if (cat.custom && cat.id === 'extensions') return renderExtensionsPanel();
	if (cat.config && cat.config !== 'neutrino') return renderExtConfigPanel(cat);

	var html = makeCardHeader(cat.label);
	cat.sections.forEach(function(sec) {
		// Feldliste zuerst bauen, damit die Zaehlung in der Kopfzeile die
		// tatsaechlich gerenderten Felder nennt. cat.requireKeys blendet
		// Felder aus, deren Key die Box nicht kennt: Neutrino schreibt beim
		// Speichern immer die volle Tabelle (neutrino.cpp:1920), ein
		// fehlender Key heisst also 'nicht einkompiliert' (ENABLE_PIP,
		// ENABLE_UPNP, HAVE_CST_HARDWARE).
		var body = '', n = 0;
		sec.fields.forEach(function(f) {
			// a key the box declares is drawn by settings-v1.js
			if (f.key && V1SET.claimed[f.key]) { body += v1setSlot(f.key); n++; return; }
			// one the box declares and does not have is not drawn at all
			if (f.key && V1SET.unavailable[f.key]) return;
			if (cat.requireKeys && f.key && !hasConfigKey(f.key)) return;
			var fh = renderField(f);
			if (fh) { body += fh; n++; }
		});
		if (!n) return;

		// Kategorien mit vielen Gruppen (Personalisierung) klappen ihre
		// Sections auf; ohne sec.collapsible bleibt das flache Layout
		if (sec.collapsible) {
			html += '<details class="webui-form-group"' + (sec.open ? ' open' : '') + '>' +
				'<summary><span class="webui-form-group-title">' + escapeHtml(sec.title) + '</span>' +
				'<span class="webui-form-group-count">' + n + '</span></summary>' +
				'<div class="webui-form-group-body">' + body + '</div></details>';
		} else {
			html += '<div class="webui-form-section">' +
				'<div class="webui-form-section-title">' + escapeHtml(sec.title) + '</div>' +
				body + '</div>';
		}
	});

	return '<div class="settings-card">' + html + '</div>';
}

// Kategorien mit eigener Config-Datei (nhttpd.conf, webui.conf, ...):
// eigener Load/Diff/Save-Zyklus gegen config=<name>, getrennt vom
// neutrino-Hauptformular (collectValues ignoriert data-extcfg-Felder)
var EXT_CONFIG_CACHE = {};

// The web interface's own settings (webui.conf) over /api/v1, as a plain
// object; a write names only the keys to change, and an empty value takes
// a key out, which the pages read the same as a key that is not there
async function webuiSettingsRead() {
	var r = await webuiV1('GET', '/webui/settings');
	var out = {};
	((r && r.items) || []).forEach(function(it) { out[it.key] = it.value; });
	return out;
}

function webuiSettingsWrite(changes) {
	return webuiV1('PATCH', '/webui/settings', {body: changes});
}

// The web server's own settings over /api/v1: the access file and
// nhttpd.conf behind one route, by its member names. A field's value as
// the form shows it, and what a changed one is sent as.
function webserverFieldValue(cfg, key) {
	var v = cfg[key];
	return (v === undefined || v === null) ? '' : String(v);
}

function webserverBodyValue(el, val) {
	if (el.type === 'checkbox') return val === 'true';
	if (el.type === 'number') return parseInt(val, 10);
	return val;
}

/* A write answers what it means for the caller before the server is
   restarted on it: whether it restarts, and where it answers next. The page
   waits for the restart by the server's start count, which the answer
   carries from before it. A server here with a higher count has restarted
   here: done, or, where the port was to move, the new port was taken and the
   server went back to this one, which the page then says. Gone from here for
   a few seconds, a moved server is followed, under the scheme and the name
   the page was reached by. 'moving', 'restarting', or '' for a write the
   server takes without a restart. */
async function webserverAfterWrite(answer) {
	var caller = (answer && answer.caller) || {};
	var was = (answer && answer.webserver) || {};
	if (!caller.restarting) return '';
	var moving = !!caller.port_moved;
	var target = new URL(location.href);
	target.port = String(was.port);
	webuiShowToast(moving ? webuiL('set.ws_moving', 'The web server moves to') + ' ' + target.origin + '/' : webuiL('set.ws_restarting', 'The web server restarts with the new settings'));
	// what answers here, or null where nothing does
	var here = async function() {
		try {
			var r = await fetch('/api/v1/system/webserver', {credentials: 'same-origin', cache: 'no-store'});
			return r.ok ? await r.json() : {};
		} catch (e) {
			return null;
		}
	};
	var downSince = 0;
	for (var i = 0; i < 80; i++) {
		await new Promise(function(r) { setTimeout(r, 300); });
		var now = await here();
		if (now === null) {
			if (!moving) continue;
			if (!downSince) downSince = Date.now();
			else if (Date.now() - downSince > 4000) {
				location.href = target.toString();
				return 'moving';
			}
			continue;
		}
		if (typeof now.restarts === 'number' && now.restarts > was.restarts) {
			if (moving) webuiShowToast(webuiL('set.ws_port_taken', 'The new port could not be used; the web server is back on port') + ' ' + (now.bound_port || now.port), true);
			return 'restarting';
		}
	}
	return 'restarting';
}
// An extension's settings kept in one section of the box's own settings,
// "settings/<section>", read and written over /api/v1; '' for any other.
function extSettingsSection(cfgName) {
	var m = /^settings\/([A-Za-z0-9_.-]{1,64})$/.exec(String(cfgName || ''));
	return m ? m[1] : '';
}

function renderExtConfigPanel(cat) {
	setTimeout(function() { loadExtConfigPanel(cat); }, 0);
	return '<div class="settings-card">' + makeCardHeader(cat.label) +
		'<div id="extcfg-' + cat.id + '"><p aria-busy="true">' + webuiLH('msg.loading', 'Loading...') + '</p></div></div>';
}

async function loadExtConfigPanel(cat) {
	var el = document.getElementById('extcfg-' + cat.id);
	if (!el) return;
	try {
		var cfg, get;
		if (cat.config === 'webui') {
			cfg = await webuiSettingsRead();
			get = function(key) { return cfg[key] || ''; };
		} else if (cat.config === 'nhttpd') {
			cfg = await webuiV1('GET', '/system/webserver');
			get = function(key) { return webserverFieldValue(cfg, key); };
		} else if (extSettingsSection(cat.config)) {
			var read = await webuiV1('GET', '/settings/' + encodeURIComponent(extSettingsSection(cat.config)));
			cfg = {};
			((read && read.items) || []).forEach(function(it) { cfg[it.id] = String(it.value); });
			get = function(key) { return cfg[key] !== undefined ? cfg[key] : ''; };
		} else {
			throw new Error(webuiL('set.ext_config_old', 'this extension keeps its settings in a place this version no longer reaches'));
		}
		EXT_CONFIG_CACHE[cat.config] = cfg;
		var html = '';
		var savable = 0;
		cat.sections.forEach(function(sec) {
			html += '<div class="webui-form-section"><div class="webui-form-section-title">' + escapeHtml(sec.title) + '</div>';
			sec.fields.forEach(function(f) {
				if (f.key && f.type !== 'info' && f.type !== 'action') savable++;
				html += renderField(f, {get: get, dataConfig: cat.config, idPrefix: cat.id + '_', extDir: cat.dir});
			});
			html += '</div>';
		});
		// Reine Action-/Info-Panels haben nichts zu speichern
		if (savable > 0)
			html += makePanelSaveRow('saveExtConfig(' + escapeHtml(JSON.stringify(cat.id)) + ', ' + escapeHtml(JSON.stringify(cat.config)) + ', this)');
		el.innerHTML = html;
		// Passwort-Status unter dem (bewusst immer leeren) Passwortfeld:
		// der Server liefert nur die abgeleiteten Bits, nie das Passwort
		if (cat.config === 'nhttpd') {
			var pwEl = el.querySelector('#cfg_' + cat.id + '_password');
			var pwDef = cfg.password_shipped === true;
			var pwSet = cfg.has_password === true;
			var pwMsg = pwDef ? webuiLH('set.pw_default', 'Factory default password is active. Please set your own password.') : (pwSet ? webuiLH('set.pw_isset', 'A password is set. Enter a new one to change it.') : '');
			var pwRow = pwEl ? pwEl.closest('.webui-form-row') : null;
			if (pwRow && pwMsg) {
				var note = document.createElement('div');
				note.className = 'webui-form-row';
				note.innerHTML = '<label></label><small id="pw-status-note" style="color:' +
					(pwDef ? 'var(--pico-del-color)' : 'var(--pico-muted-color)') + '">' + pwMsg + '</small>';
				pwRow.insertAdjacentElement('afterend', note);
				pwEl.setAttribute('aria-describedby', 'pw-status-note');
			}
		}
	} catch (e) {
		el.innerHTML = '<div class="webui-form-row"><label>' + webuiLH('set.notavail', 'Not available') + ': ' + escapeHtml(e.message) + '</label></div>';
	}
}

async function saveExtConfig(catId, cfgName, btn) {
	var root = document.getElementById('extcfg-' + catId);
	var cfg = EXT_CONFIG_CACHE[cfgName] || {};
	// the web interface's own file and the web server go over /api/v1,
	// by their plain keys and member names
	var webui = cfgName === 'webui';
	var webserver = cfgName === 'nhttpd';
	var section = extSettingsSection(cfgName);
	if (!webui && !webserver && !section) return;
	var changes = {};
	var changed = 0;
	root.querySelectorAll('[data-key][data-extcfg]').forEach(function(el) {
		var key = el.getAttribute('data-key');
		var val = (el.type === 'checkbox') ? (el.checked ? el.getAttribute('data-on') : el.getAttribute('data-off')) : el.value;
		if (webserver) {
			// the password field opens empty and an empty one keeps the password
			if (el.type === 'password' ? val === '' : val === webserverFieldValue(cfg, key)) return;
			changes[key] = webserverBodyValue(el, val);
			changed++;
			return;
		}
		if (val !== (cfg[key] || '')) {
			changes[key] = val;
			changed++;
		}
	});
	if (changed === 0) { webuiShowToast(webuiL('set.nochanges', 'No changes')); return; }
	btn.disabled = true;
	var after = '';
	try {
		if (webui)
			await webuiSettingsWrite(changes);
		else if (webserver)
			after = await webserverAfterWrite(await webuiV1('PUT', '/system/webserver', {body: changes}));
		else {
			// 207: some keys landed, the others name their refusal
			var answer = await webuiV1('PATCH', '/settings/' + encodeURIComponent(section), {body: changes});
			var results = (answer && answer.results) || {};
			var refused = Object.keys(results).filter(function(k) { return results[k] && results[k].status >= 300; });
			if (refused.length)
				throw new Error(refused.map(function(k) { return k + ': ' + (results[k].detail || results[k].code || results[k].status); }).join('; '));
		}
		if (after === 'moving') return;
		if (after !== 'restarting') webuiShowToast(changed + ' ' + (changed > 1 ? webuiL('set.saved_many', 'settings saved') : webuiL('set.saved_one', 'setting saved')));
		await loadExtConfigPanel(SETTINGS_REGISTRY.find(function(c) { return c.id === catId; }) || EXT_SETTINGS_CATS[catId]);
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

// Backup and restore over /api/v1: the box packs its settings with its own
// backup.sh; an archive goes up in pieces to /tmp, and the box checks it,
// puts it back itself and restarts
function renderBackupPanel() {
	return '<div class="settings-card">' + makeCardHeader(webuiL('set.backup_title', 'Backup and restore')) +
		'<div class="webui-form-section"><div class="webui-form-section-title">Backup</div>' +
		'<div class="webui-form-row"><label>' + webuiLH('set.backup_label', 'Back up settings') + ' <small style="color:var(--pico-muted-color)">' + webuiLH('set.backup_hint', 'creates an archive and downloads it') + '</small></label>' +
		'<span class="webui-row-actions">' +
		'<button type="button" class="webui-btn primary" onclick="doSettingsBackup(this)">' + DL_ICON + webuiLH('set.backup_btn', 'Create backup') + '</button></span></div></div>' +
		'<div class="webui-form-section"><div class="webui-form-section-title">Restore</div>' +
		'<div class="webui-form-row"><label>' + webuiLH('set.restore_label', 'Restore an archive') + ' <small style="color:var(--pico-muted-color)">' + webuiLH('set.restore_hint', 'the box restarts by itself') + '</small></label>' +
		'<span class="webui-row-actions">' + makeFileControl('restoreFile') +
		'<button type="button" class="webui-btn danger" onclick="doSettingsRestore(this)">' + RESTORE_ICON + webuiLH('set.restore_btn', 'Restore') + '</button></span></div></div></div>';
}

async function doSettingsBackup(btn) {
	btn.disabled = true;
	try {
		webuiShowToast('Backup: ' + await webuiV1Download('/system/backup', 'settings.tar.gz'));
	} catch (e) {
		webuiShowToast(webuiL('set.backup_fail', 'Backup failed') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

// M5c: Bestätigung im geteilten webuiModal statt confirm() — die
// destruktive Aktion bekommt einen klaren Danger-Bestätigungsknopf
function doSettingsRestore(btn) {
	var inp = document.getElementById('restoreFile');
	if (!inp || !inp.files || !inp.files.length) { webuiShowToast(webuiL('inst.nofile', 'No file selected'), true); return; }
	if (!/\.(tar\.gz|tgz|tar)$/i.test(inp.files[0].name)) { webuiShowToast(webuiL('set.restore_type', 'An archive ends in .tar.gz, .tgz or .tar'), true); return; }
	webuiModal('restoreConfirmModal', webuiL('set.restore_btn', 'Restore'),
		'<p>' + webuiLH('set.restore_confirm', 'Really overwrite the settings? The box restarts right away.') + '<br><small style="color:var(--pico-muted-color)">' +
		escapeHtml(inp.files[0].name) + '</small></p>' +
		'<div class="webui-row-actions" style="justify-content:flex-end">' +
		'<button type="button" class="webui-btn ghost" onclick="webuiCloseModal(\'restoreConfirmModal\')">' + webuiLH('tm.cancel', 'Cancel') + '</button>' +
		'<button type="button" class="webui-btn danger" onclick="webuiRestoreGo(this)">' + RESTORE_ICON + webuiLH('set.restore_btn', 'Restore') + '</button></div>');
}

async function webuiRestoreGo(btn) {
	var inp = document.getElementById('restoreFile');
	if (!inp || !inp.files || !inp.files.length) return;
	btn.disabled = true;
	// a name of its own in /tmp, keeping the ending the box tells the kind by
	var f = inp.files[0];
	var ending = (/\.(tar\.gz|tgz|tar)$/i.exec(f.name) || ['', 'tar.gz'])[1].toLowerCase();
	var name = 'restore-' + Math.random().toString(36).slice(2, 10) + '.' + ending;
	var uploaded = false;
	try {
		await webuiV1UploadPieces(f, '/tmp/' + name);
		uploaded = true;
		await webuiV1('POST', '/system/restore', {body: {file: name}});
		uploaded = false;	// the box has it now, and is about to restart
		webuiCloseModal('restoreConfirmModal');
		webuiShowToast(webuiL('set.restore_started', 'The box is putting the settings back and restarts in a moment'));
	} catch (e) {
		if (uploaded && !e.status) {
			// no answer: the box may have taken the archive over and be restarting
			webuiShowToast(webuiL('set.restore_unknown', 'The box did not answer; it may already be putting the settings back and restarting'), true);
		} else {
			// a refusal; the box takes the upload away itself once it has read it
			if (uploaded)
				webuiV1('DELETE', '/storage/path', {query: {path: '/tmp/' + name}}).catch(function() {});
			webuiShowToast(webuiL('set.restore_fail', 'Restore failed') + ': ' + e.message, true);
		}
	}
	btn.disabled = false;
}

// Extension manager: discovered manifests with enable toggle, slot
// override (persisted in webui.conf, loader in webui-header.js) and
// install/update/uninstall over /api/v1/webui/extensions. The box fetches
// nothing: the signed index and an archive it offers are loaded by the
// browser and uploaded, and the box checks them.
function renderExtensionsPanel() {
	setTimeout(loadExtensionsPanel, 0);
	return '<div class="settings-card">' + makeCardHeader(webuiL('set.ext_title', 'Neutrino-Web-UI extensions')) +
		'<div id="extPanel"><p aria-busy="true">' + webuiLH('set.ext_search', 'Searching for extensions...') + '</p></div></div>';
}

// dir -> {version, url} filled by an explicit update check
var _extUpdates = {};

async function loadExtensionsPanel() {
	var el = document.getElementById('extPanel');
	if (!el) return;
	webuiForgetExtensions(); // Discovery-Cache invalidieren
	var exts = await webuiDiscoverExtensions();
	var html = '';
	if (!exts.length) {
		html = '<div class="webui-form-row"><label>' + webuiLH('set.ext_none', 'No extensions found') + ' ' +
			'<small style="color:var(--pico-muted-color)">' + webuiLH('set.ext_dir_hint', 'Directory: <webroot>/extensions/<name>/manifest.json — see README') + '</small></label></div>';
	} else {
		// 4-column grid with a header row — slot selects, toggles and
		// action buttons of all extensions align per column
		html = '<div class="webui-form-section"><div class="webui-form-section-title">' + webuiLH('set.ext_found', 'Discovered extensions') + '</div>' +
			'<div class="ext-grid">' +
			'<div class="ext-grid-head">' + webuiLH('set.ext_col_ext', 'Extension') + '</div>' +
			'<div class="ext-grid-head">Slot</div>' +
			'<div class="ext-grid-head center">' + webuiLH('set.ext_col_on', 'Active') + '</div>' +
			'<div class="ext-grid-head right">' + webuiLH('set.ext_col_action', 'Action') + '</div>' +
			'<div class="ext-grid-rule"></div>';
		exts.forEach(function(m) {
			var upd = _extUpdates[m.dir];
			html += '<div class="ext-name">' + escapeHtml(m.name) +
				(upd ? '<span class="ext-badge">v' + escapeHtml(upd.version) + '</span>' : '') +
				' <small style="color:var(--pico-muted-color)">v' + escapeHtml(m.version || '?') +
				(m.url ? ' — <a href="' + escapeHtml(m.url) + '">' + escapeHtml(m.entry) + '</a>' : ' — ' + webuiLH('set.ext_settings_only', 'settings only')) + '</small></div>' +
				'<div><select id="extslot_' + m.dir + '">' +
				['nav', 'tools', 'dashboard', 'page'].map(function(sl) {
					return '<option value="' + sl + '"' + (m.slot === sl ? ' selected' : '') + '>' + sl + '</option>';
				}).join('') + '</select></div>' +
				'<div class="ext-cell-center"><label class="webui-toggle"><input type="checkbox" id="extena_' + m.dir + '"' + (m.enabled ? ' checked' : '') + '>' +
				'<span class="webui-toggle-slider"></span></label></div>' +
				'<div class="ext-actions">' +
				(upd ? '<button type="button" class="webui-btn primary" title="' + webuiLH('set.extup_avail', 'Update available') + '" onclick="extUpdate(\'' + m.dir + '\', this)">' +
					DL_ICON + 'v' + escapeHtml(upd.version) + '</button>' : '') +
				'<button type="button" class="webui-btn danger" title="' + webuiLH('set.extuninst_btn', 'Uninstall') + '" onclick="extUninstall(\'' + m.dir + '\')">' + TRASH_ICON + '</button>' +
				'</div>';
		});
		html += '</div></div>' + makePanelSaveRow('saveExtensions(this)');
	}
	// install/update row — also shown when no extension is installed
	html += '<div class="webui-form-section"><div class="webui-form-section-title">' + webuiLH('set.extinst_section', 'Install / update') + '</div>' +
		'<div class="webui-form-row"><label>' + webuiLH('set.extinst_label', 'New extension (signed index or tar.gz)') + '</label>' +
		'<span class="webui-row-actions">' +
		'<button type="button" class="webui-btn primary" onclick="extInstallDialog()">' + UP_ICON + webuiLH('set.extinst_btn', 'Install') + '</button>' +
		'<button type="button" class="webui-btn" onclick="extCheckUpdates(this)">' + REFRESH_ICON + webuiLH('set.extup_check', 'Check for updates') + '</button>' +
		'</span></div></div>';
	el.innerHTML = html;
}

// The install dialog: an extension from the signed index, or an archive the
// user takes unsigned after a warning. The browser loads index and archive
// and uploads them; the box checks the index against its signature and the
// archive against the index (/api/v1/webui/extension-index), and fetches
// nothing. An update is an install from the index under the same name.
function extInstallDialog() {
	webuiModal('extInstallModal', webuiL('set.extinst_title', 'Install extension'),
		'<div class="webui-form-section"><div class="webui-form-section-title">' + webuiLH('set.extinst_index', 'From the signed index') + '</div>' +
		'<div id="extOffers"><div class="webui-form-row"><label>' + webuiLH('set.extinst_index_hint', 'What the index offers, checked by the box against its signature') + '</label>' +
		'<span class="webui-row-actions"><button type="button" class="webui-btn" onclick="extShowOffers(this)">' + REFRESH_ICON + webuiLH('set.extinst_index_load', 'Load list') + '</button></span></div></div></div>' +
		'<div class="webui-form-section"><div class="webui-form-section-title">' + webuiLH('set.extinst_file', 'Upload archive (tar.gz)') + '</div>' +
		'<div class="webui-form-row"><label>tar.gz <small style="color:var(--pico-del-color)">' + webuiLH('set.extinst_unsigned', 'not signed: install only an archive you trust') + '</small></label>' +
		'<span class="webui-row-actions">' + makeFileControl('extInstFile', '.tar.gz,.tgz,.tar') +
		'<button type="button" class="webui-btn primary" onclick="extInstallUpload(this)">' + UP_ICON + webuiLH('set.extinst_btn', 'Install') + '</button></span></div></div>');
}

// The browser loads it; an address that does not let a page read it is said.
async function extFetchArchive(url) {
	var r;
	try {
		r = await fetch(url, {cache: 'no-cache'});
	} catch (e) {
		throw new Error(webuiL('set.extinst_cors', 'The browser cannot load this address (it does not let a page read it). Download the archive and install it as a file.'));
	}
	if (!r.ok) throw new Error('HTTP ' + r.status);
	return r.blob();
}

// A name in /tmp no other upload has, so a cleanup never takes another's file.
function extTmpName(prefix, ending) {
	var r = new Uint32Array(2);
	crypto.getRandomValues(r);
	return prefix + Date.now().toString(36) + '-' + r[0].toString(16) + r[1].toString(16) + ending;
}

// The index up to /tmp under a name of the page's own, its bytes as the
// browser loaded them (the signature is over those); the box takes it over
// and removes it.
async function extUploadIndex(blob) {
	var name = extTmpName('ext-index-', '.json');
	try {
		await webuiV1UploadPieces(blob, '/tmp/' + name);
	} catch (e) {
		await extDropUploads([name]);
		throw e;
	}
	return name;
}

// Takes away what went up for an install that never reached the box.
function extDropUploads(names) {
	return Promise.all(names.map(function(n) {
		return webuiV1('DELETE', '/storage/path', {query: {path: '/tmp/' + n}}).catch(function() {});
	}));
}

// The archive up to /tmp, then installed from there: from the index, which
// goes up again beside it, under the name the index gives it, or as a file
// the user takes unsigned. The box takes both uploads over and removes them
// whatever comes of the install; where the request fails, the page tries
// too, and a name the box already took away is no failure.
async function extUploadAndInstall(blob, originalName, fromIndex, dir) {
	var tar = /\.tar$/i.test(String(originalName || ''));
	var body = {};
	var sent = [];
	try {
		if (fromIndex) {
			body.index = await extUploadIndex(fromIndex.blob);
			sent.push(body.index);
			body.signature = fromIndex.signature;
			body.dir = dir;
		} else {
			body.unsigned = true;
		}
		body.file = extTmpName('ext-', tar ? '.tar' : '.tar.gz');
		sent.push(body.file);
		await webuiV1UploadPieces(blob, '/tmp/' + body.file);
		return await webuiV1('POST', '/webui/extensions', {body: body});
	} catch (e) {
		await extDropUploads(sent);
		throw e;
	}
}

// update check and index are explicit user actions (no automatic calls to
// external URLs); index URL overridable via webui key ext_index_url
var EXT_INDEX_DEFAULT = 'https://raw.githubusercontent.com/tuxbox-neutrino/yweb-extensions/master/index.json';

// The index as the browser loaded it (a Blob, its bytes untouched), its
// signature beside it, and what the box read from it once the signature
// held; loaded on the user's asking.
var _extIndex = null;

async function extReadIndex(again) {
	if (_extIndex && !again) return _extIndex;
	var cfg = await webuiSettingsRead();
	var url = cfg['ext_index_url'] || EXT_INDEX_DEFAULT;
	var blob = await extFetchArchive(url);
	var signature = (await (await extFetchArchive(url + '.sig')).text()).trim();
	var name = await extUploadIndex(blob);
	var read;
	try {
		read = await webuiV1('POST', '/webui/extension-index', {body: {index: name, signature: signature}});
	} catch (e) {
		await extDropUploads([name]);
		throw e;
	}
	_extIndex = {blob: blob, signature: signature, items: (read && read.items) || []};
	return _extIndex;
}

async function extInstallUpload(btn) {
	var inp = document.getElementById('extInstFile');
	if (!inp || !inp.files || !inp.files.length) { webuiShowToast(webuiL('inst.nofile', 'No file selected'), true); return; }
	var f = inp.files[0];
	webuiShowConfirm(webuiL('set.extinst_title', 'Install extension'),
		webuiLH('set.extinst_unsigned_confirm', 'This archive is not signed. Install it only if you trust where it comes from: an extension can do everything your session can.'),
		webuiL('set.extinst_btn', 'Install'), async function() {
		btn.disabled = true;
		try {
			var res = await extUploadAndInstall(f, f.name, null);
			webuiShowToast(webuiL('set.extinst_ok', 'Installed') + ': ' + (res && res.dir));
			webuiCloseModal('extInstallModal');
			await loadExtensionsPanel();
		} catch (e) { webuiShowToast(webuiL('set.extinst_fail', 'Installation failed') + ': ' + e.message, true); }
		btn.disabled = false;
	}, {variant: 'ok'});
}

// What the index offers, each with what is installed of it: install, update
// or nothing to do. Its names are text.
async function extShowOffers(btn) {
	var box = document.getElementById('extOffers');
	btn.disabled = true;
	try {
		var idx = await extReadIndex(true);
		var have = {};
		(await webuiDiscoverExtensions()).forEach(function(m) { have[m.dir] = m.version || '0'; });
		box.textContent = '';
		if (!idx.items.length)
			box.appendChild(webuiEl('div', {className: 'webui-form-row'}, webuiEl('label', {}, webuiL('set.extinst_index_empty', 'The index offers no extensions'))));
		idx.items.forEach(function(e) {
			var label = webuiEl('label', {}, (e.name || e.id) + ' v' + e.version + ' ',
				webuiEl('small', {style: {color: 'var(--pico-muted-color)'}}, e.description || e.id));
			var action;
			if (have[e.id] !== undefined && !extVerNewer(e.version, have[e.id]))
				action = webuiEl('small', {}, webuiL('set.extinst_installed', 'installed'));
			else
				action = webuiEl('button', {type: 'button', className: 'webui-btn primary', data: {extOffer: e.id},
					on: {click: function() { extInstallOffered(e.id, this); }}},
					have[e.id] !== undefined ? webuiL('set.extinst_update', 'Update') : webuiL('set.extinst_btn', 'Install'));
			box.appendChild(webuiEl('div', {className: 'webui-form-row'}, label, webuiEl('span', {className: 'webui-row-actions'}, action)));
		});
	} catch (e) {
		webuiShowToast(webuiL('set.extup_fail', 'Update check failed') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

async function extInstallOffered(id, btn) {
	btn.disabled = true;
	try {
		var idx = await extReadIndex();
		var e = idx.items.filter(function(x) { return x.id === id; })[0];
		if (!e) throw new Error(id);
		var res = await extUploadAndInstall(await extFetchArchive(e.url), e.url.split(/[?#]/)[0], idx, e.id);
		delete _extUpdates[e.id];
		webuiShowToast(webuiL('set.extinst_ok', 'Installed') + ': ' + (res && res.dir) + ' v' + e.version);
		webuiCloseModal('extInstallModal');
		await loadExtensionsPanel();
	} catch (err) { webuiShowToast(webuiL('set.extinst_fail', 'Installation failed') + ': ' + err.message, true); }
	btn.disabled = false;
}

function extUninstall(dir) {
	var m = (_webuiExtensions || []).filter(function(x) { return x.dir === dir; })[0];
	var name = m ? m.name : dir;
	// the dialog takes markup; the name is the manifest's
	webuiShowConfirm(webuiL('set.extuninst_btn', 'Uninstall'), escapeHtml(name) + ' — ' + webuiLH('set.extuninst_confirm', 'Remove this extension from the box? Its settings keys are cleared.'),
		webuiL('set.extuninst_btn', 'Uninstall'), async function() {
		try {
			await webuiV1('DELETE', '/webui/extensions/' + encodeURIComponent(dir));
			// its keys go with it (an empty value takes a key out); the
			// extension is gone either way, and keys left over are inert
			var gone = {};
			gone['ext_' + dir + '_enabled'] = '';
			gone['ext_' + dir + '_slot'] = '';
			try { await webuiSettingsWrite(gone); } catch (eKeys) { console.warn('extension keys kept:', eKeys.message); }
			delete _extUpdates[dir];
			webuiShowToast(webuiL('set.extuninst_ok', 'Uninstalled') + ': ' + name);
			await loadExtensionsPanel();
		} catch (e) { webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true); }
	});
}

function extVerNewer(a, b) {
	var pa = String(a || '').split('.'), pb = String(b || '').split('.');
	for (var i = 0; i < Math.max(pa.length, pb.length); i++) {
		var na = parseInt(pa[i], 10) || 0, nb = parseInt(pb[i], 10) || 0;
		if (na !== nb) return na > nb;
	}
	return false;
}

async function extCheckUpdates(btn) {
	btn.disabled = true;
	try {
		var idx = await extReadIndex(true);
		var exts = await webuiDiscoverExtensions();
		_extUpdates = {};
		var found = 0;
		idx.items.forEach(function(e) {
			exts.forEach(function(m) {
				if (m.dir === e.id && extVerNewer(e.version, m.version)) {
					_extUpdates[m.dir] = {version: e.version, url: e.url};
					found++;
				}
			});
		});
		webuiShowToast(found ? (webuiL('set.extup_avail', 'Update available') + ': ' + found) : webuiL('set.extup_none', 'Everything up to date'));
		await loadExtensionsPanel();
	} catch (e) { webuiShowToast(webuiL('set.extup_fail', 'Update check failed') + ': ' + e.message, true); }
	btn.disabled = false;
}

async function extUpdate(dir, btn) {
	var u = _extUpdates[dir];
	if (!u) return;
	btn.disabled = true;
	try {
		var res = await extUploadAndInstall(await extFetchArchive(u.url), u.url.split(/[?#]/)[0], await extReadIndex(), dir);
		delete _extUpdates[dir];
		webuiShowToast(webuiL('set.extinst_ok', 'Installed') + ': ' + (res && res.dir) + ' v' + u.version);
		await loadExtensionsPanel();
	} catch (e) { webuiShowToast(webuiL('set.extinst_fail', 'Installation failed') + ': ' + e.message, true); }
	btn.disabled = false;
}

async function saveExtensions(btn) {
	var exts = await webuiDiscoverExtensions();
	var changes = {};
	exts.forEach(function(m) {
		// a folder put there by hand under a name no key can carry is left
		// out rather than failing the save of every other one
		if (!/^[A-Za-z0-9][A-Za-z0-9_.-]{0,254}$/.test(m.dir)) return;
		var ena = document.getElementById('extena_' + m.dir);
		var slot = document.getElementById('extslot_' + m.dir);
		changes['ext_' + m.dir + '_enabled'] = ena && ena.checked ? '1' : '0';
		if (slot) changes['ext_' + m.dir + '_slot'] = slot.value;
	});
	btn.disabled = true;
	try {
		await webuiSettingsWrite(changes);
		webuiShowToast(webuiL('set.ext_saved', 'Saved - reload the page to apply'));
		await loadExtensionsPanel();
	} catch (e) { webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true); }
	btn.disabled = false;
}

// Wake-on-LAN: description and MAC of up to three devices in the web
// interface's own settings (/api/v1/webui/settings); the box sends the
// wake-up packet itself (POST /api/v1/network/wake)
function renderWolPanel() {
	setTimeout(loadWolPanel, 0);
	return '<div class="settings-card">' + makeCardHeader('Wake-on-LAN') +
		'<div id="wolPanel"><p aria-busy="true">Lade…</p></div></div>';
}

async function loadWolPanel() {
	var el = document.getElementById('wolPanel');
	if (!el) return;
	try {
		var cfg = await webuiSettingsRead();
		EXT_CONFIG_CACHE['webui'] = cfg;
		var section = webuiEl('div', {className: 'webui-form-section'},
			webuiEl('div', {className: 'webui-form-section-title'}, webuiL('set.wol_devices', 'Devices')));
		[1, 2, 3].forEach(function(i) {
			section.appendChild(webuiEl('div', {className: 'webui-form-row'},
				webuiEl('label', {}, webuiL('set.wol_device', 'Device') + ' ' + i),
				webuiEl('span', {},
					webuiEl('input', {type: 'text', id: 'wol_desc_' + i, value: cfg['wol_desc_' + i] || '',
						attrs: {placeholder: webuiL('set.wol_desc_ph', 'Description')}, style: {'max-width': '140px'}}), ' ',
					webuiEl('input', {type: 'text', id: 'wol_mac_' + i, value: cfg['wol_mac_' + i] || '',
						attrs: {placeholder: 'aa:bb:cc:dd:ee:ff'}, style: {'max-width': '160px'}}), ' ',
					webuiEl('button', {type: 'button', className: 'webui-btn', on: {click: function() { wolWake(i, this); }}},
						webuiMarkup(ZAP_ICON), webuiL('set.wol_wake', 'Wake')))));
		});
		el.replaceChildren(section);
		el.insertAdjacentHTML('beforeend', makePanelSaveRow('wolSave(this)'));
	} catch (e) {
		el.replaceChildren(webuiEl('div', {className: 'webui-form-row'},
			webuiEl('label', {}, webuiL('set.notavail', 'Not available') + ': ' + e.message)));
	}
}

async function wolWake(i, btn) {
	var mac = (document.getElementById('wol_mac_' + i) || {}).value || '';
	if (!mac.trim()) { webuiShowToast(webuiL('set.wol_nomac', 'No MAC address'), true); return; }
	btn.disabled = true;
	try {
		await webuiV1('POST', '/network/wake', {body: {mac: mac.trim()}});
		webuiShowToast(webuiL('set.wol_sent', 'Wake-on-LAN sent') + ': ' + mac.trim());
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

async function wolSave(btn) {
	var cfg = EXT_CONFIG_CACHE['webui'] || {};
	var changes = {};
	var changed = 0;
	for (var i = 1; i <= 3; i++) {
		['wol_desc_' + i, 'wol_mac_' + i].forEach(function(k) {
			var v = (document.getElementById(k) || {}).value || '';
			if (v !== (cfg[k] || '')) { changes[k] = v; changed++; }
		});
	}
	if (!changed) { webuiShowToast(webuiL('set.nochanges', 'No changes')); return; }
	btn.disabled = true;
	try {
		await webuiSettingsWrite(changes);
		webuiShowToast(webuiL('msg.saved', 'Saved'));
		await loadWolPanel();
	} catch (e) { webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true); }
	btn.disabled = false;
}

// The lists the box reads web channels and XMLTV guides from, over
// /api/v1/channels/sources: one address or file per line; a save replaces
// only the lists that changed and has the box read its channels again
var XMLTV_LOADED = null;
var XMLTV_KINDS = ['webtv', 'webradio', 'xmltv'];
// a list file uploaded over one already named changes no line, and the box
// still has to read it in: the uploads finished per list, and how many of
// them a save had the box read in. Counted rather than flagged, so an upload
// that finishes while a save runs stays owed
var XMLTV_UPLOADS = {webtv: 0, webradio: 0};
var XMLTV_READ_IN = {webtv: 0, webradio: 0};

function xmltvOwed() {
	return Object.keys(XMLTV_UPLOADS).filter(function(kind) {
		return XMLTV_UPLOADS[kind] !== XMLTV_READ_IN[kind];
	});
}

function renderXmltvPanel() {
	setTimeout(loadXmltvPanel, 0);
	return '<div class="settings-card">' + makeCardHeader(webuiL('set.xmltv_title', 'WebTV, web radio and XMLTV sources')) +
		'<div id="xmltvPanel"><p aria-busy="true">Lade…</p></div></div>';
}

function xmltvSection(kind, title, upload) {
	var html = '<div class="webui-form-section"><div class="webui-form-section-title">' + title + '</div>' +
		'<textarea id="xmltv_' + kind + '" rows="4" style="width:100%" placeholder="' + webuiLH('set.xmltv_ph', 'one URL/file per line') + '">' +
		escapeHtml((XMLTV_LOADED[kind] || []).join('\n')) + '</textarea>';
	if (upload)
		html += '<div class="webui-form-row" style="margin-top:0.4rem"><label><small style="color:var(--pico-muted-color)">' + webuiLH('set.xmltv_upload', 'or upload a local source file') + '</small></label>' +
			'<span class="webui-row-actions">' + makeFileControl('xmltv_' + kind + '_file', '.xml,.m3u') +
			'<button type="button" class="webui-btn" onclick="uploadXmltv(\'' + kind + '\', this)">' + UP_ICON + webuiLH('fm.upload', 'Upload') + '</button></span></div>';
	return html + '</div>';
}

async function loadXmltvPanel() {
	var el = document.getElementById('xmltvPanel');
	if (!el) return;
	try {
		XMLTV_LOADED = await webuiV1('GET', '/channels/sources');
		el.innerHTML = xmltvSection('webtv', 'WebTV', true) +
			xmltvSection('webradio', 'WebRadio', true) +
			xmltvSection('xmltv', webuiL('set.xmltv_guide', 'XMLTV programme guides'), false) +
			'<div class="webui-form-row"><label><small style="color:var(--pico-muted-color)">' + webuiLH('set.xmltv_hint', 'Saving replaces the changed source lists and reloads the channel lists') + '</small></label>' +
			'<span class="webui-row-actions">' +
			'<button type="button" class="webui-btn primary" onclick="saveXmltv(this)">' + SAVE_ICON + webuiLH('bqe.save_reload', 'Save and reload') + '</button></span></div>';
	} catch (e) {
		el.innerHTML = '<div class="webui-form-row"><label>' + webuiLH('set.notavail', 'Not available') + ': ' + escapeHtml(e.message) + '</label></div>';
	}
}

// A list as the box takes it: the lines of the field, without blanks and
// without the spaces a paste brings along at either end
function xmltvEntries(kind) {
	var ta = document.getElementById('xmltv_' + kind);
	return ((ta && ta.value) || '').split('\n').map(function(l) { return l.trim(); })
		.filter(function(l) { return l.length > 0; });
}

// A local list file goes up in pieces into the directory the box keeps its
// own lists in, under a name no reload reads, and takes its own name only
// once it is whole; its path is then added as a line (a save names it)
async function uploadXmltv(kind, btn) {
	var inp = document.getElementById('xmltv_' + kind + '_file');
	var ta = document.getElementById('xmltv_' + kind);
	if (!inp || !inp.files || !inp.files.length) { webuiShowToast(webuiL('inst.nofile', 'No file selected'), true); return; }
	btn.disabled = true;
	var part = '';
	try {
		var f = inp.files[0];
		var dir = (kind === 'webtv' ? XMLTV_LOADED.webtv_dir : XMLTV_LOADED.webradio_dir);
		var name = f.name.replace(/[^A-Za-z0-9._-]/g, '_');
		var savedPath = dir + '/' + name;
		part = dir + '/.' + name + '.' + Math.random().toString(36).slice(2, 8) + '.upload';
		await webuiV1UploadPieces(f, part);
		await webuiV1('POST', '/storage/rename', {body: {from: part, to: savedPath, overwrite: true}});
		part = '';
		XMLTV_UPLOADS[kind]++;
		if (ta && xmltvEntries(kind).indexOf(savedPath) < 0)
			ta.value = (ta.value ? ta.value.replace(/\n*$/, '\n') : '') + savedPath;
		webuiShowToast(webuiL('fm.uploaded', 'Uploaded') + ': ' + savedPath);
		inp.value = '';
	} catch (e) {
		if (part) {
			try { await webuiV1('DELETE', '/storage/path', {query: {path: part}}); } catch (e2) {}
		}
		webuiShowToast(webuiL('fm.upload_fail', 'Upload error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

async function saveXmltv(btn) {
	var changed = XMLTV_KINDS.filter(function(kind) {
		return JSON.stringify(xmltvEntries(kind)) !== JSON.stringify(XMLTV_LOADED[kind] || []);
	});
	var owed = xmltvOwed();
	if (!changed.length && !owed.length) { webuiShowToast(webuiL('set.nochanges', 'No changes')); return; }
	if (!confirm(webuiL('set.xmltv_confirm', 'Replace the source lists and reload the channel lists?'))) return;
	// what this save answers for; an upload finishing meanwhile stays owed
	var upto = {webtv: XMLTV_UPLOADS.webtv, webradio: XMLTV_UPLOADS.webradio};
	// with nothing changed an upload's list goes as it stands: the reload that
	// comes with a written list is the box's own, which reads the guides the
	// playlists name as well, and a bare channel reload does not
	var sending = changed.length ? changed : [owed[0]];
	// every list as it stands now, before the first request: the panel can
	// be closed while one is under way, and a field gone reads as empty
	var bodies = {};
	sending.forEach(function(kind) { bodies[kind] = xmltvEntries(kind); });
	btn.disabled = true;
	try {
		// each with the reload: the box carries what came meanwhile in one go,
		// and a list taken over is read in even where a later one is refused.
		// What is compared next time is what was sent, list by list: a list
		// changed elsewhere meanwhile is not taken for an edit made here
		for (var i = 0; i < sending.length; i++) {
			var answer = await webuiV1('PUT', '/channels/sources/' + sending[i],
				{body: bodies[sending[i]], query: {reload: true}});
			XMLTV_LOADED[sending[i]] = answer[sending[i]];
		}
		XMLTV_READ_IN = upto;
		webuiShowToast(webuiL('set.xmltv_saved', 'Sources saved, channel lists reloaded'));
	} catch (e) { webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true); }
	btn.disabled = false;
}

// Zapit-Upload: Kanal-/Bouquet-XML hochladen und aktivieren
function renderZapitUploadPanel() {
	return '<div class="settings-card">' + makeCardHeader('Zapit-Upload') +
		'<div class="webui-form-section"><div class="webui-form-section-title">' + webuiLH('set.zapit_section', 'Import a channel list file') + '</div>' +
		'<div class="webui-form-row"><label>' + webuiLH('set.zapit_target', 'Target') + '</label>' +
		'<select id="zapitTarget"><option>ubouquets.xml</option><option>bouquets.xml</option><option>services.xml</option><option>myservices.xml</option></select></div>' +
		'<div class="webui-form-row"><label>' + webuiLH('fm.file', 'File') + '</label>' +
		'<span class="webui-row-actions">' + makeFileControl('zapitFile', '.xml') +
		'<button type="button" class="webui-btn primary" onclick="doZapitUpload(this)">' + UP_ICON + webuiLH('set.zapit_btn', 'Upload and activate') + '</button></span></div>' +
		'<div class="webui-form-row"><label><small style="color:var(--pico-muted-color)">' + webuiLH('set.zapit_hint', 'The channel lists are reloaded after the upload') + '</small></label>' +
		'<span id="zapitStatus" style="color:var(--pico-muted-color);font-size:0.8rem"></span></div></div></div>';
}

// The list goes up in pieces into /tmp under a name of its own, and the box
// takes it over from there: checks that it is a list of the kind chosen,
// puts it in place of its own and reads its channel lists again, so the
// uploaded list wins over what the box holds
async function doZapitUpload(btn) {
	var inp = document.getElementById('zapitFile');
	var target = (document.getElementById('zapitTarget') || {}).value;
	var status = document.getElementById('zapitStatus');
	if (!inp || !inp.files || !inp.files.length) { webuiShowToast(webuiL('inst.nofile', 'No file selected'), true); return; }
	if (!confirm(target + ' ' + webuiL('set.zapit_confirm', 'really replace?'))) return;
	btn.disabled = true;
	if (status) status.textContent = webuiL('inst.uploading', 'Uploading') + '…';
	var file = 'zapit-' + Math.random().toString(36).slice(2, 10) + '.xml';
	var uploaded = false;
	try {
		var f = inp.files[0];
		await webuiV1UploadPieces(f, '/tmp/' + file);
		uploaded = true;
		await webuiV1('POST', '/channels/files/' + target.replace(/\.xml$/, ''), {body: {file: file}});
		uploaded = false;
		if (status) status.textContent = target + ' ✓ (' + f.size + ' B)';
		webuiShowToast(target + ' ' + webuiL('set.zapit_done', 'imported, channel lists reloaded'));
		inp.value = '';
	} catch (e) {
		// the box takes an upload it looked at away itself; one it never got to stays
		if (uploaded) {
			try { await webuiV1('DELETE', '/storage/path', {query: {path: '/tmp/' + file}}); } catch (e2) {}
		}
		if (status) status.textContent = '';
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

// Plugins over /api/v1: the list, starting one, and reading the plugin
// directories again. The box starts a plugin and reads its directories
// on its own thread, after it answered. Starting and stopping services
// (nfs, samba, ...) is not part of this panel: it depends on the
// packages and the image, and comes as an extension over /api/v1/daemons.
function renderPluginsPanel() {
	setTimeout(loadPluginsPanel, 0);
	return '<div class="settings-card">' + makeCardHeader(webuiL('reg.plugins', 'Neutrino plugins')) +
		'<div id="pluginsPanel"><p aria-busy="true">' + webuiLH('set.plugins_loading', 'Loading plugins...') + '</p></div></div>';
}

function pluginRow(label, note, button) {
	return webuiEl('div', {className: 'webui-form-row'},
		webuiEl('label', {}, label, note ? ' ' : null,
			note ? webuiEl('small', {style: {color: 'var(--pico-muted-color)'}}, note) : null),
		button ? webuiEl('span', {className: 'webui-row-actions'}, button) : null);
}

function pluginButton(icon, text, onClick) {
	return webuiEl('button', {type: 'button', className: 'webui-btn', on: {click: function() { onClick(this); }}},
		webuiMarkup(icon), text);
}

async function loadPluginsPanel() {
	var el = document.getElementById('pluginsPanel');
	if (!el) return;
	try {
		var data = await webuiV1('GET', '/plugins');
		var list = ((data && data.items) || []).filter(function(pl) { return !pl.hidden; });
		var installed = webuiEl('div', {className: 'webui-form-section'},
			webuiEl('div', {className: 'webui-form-section-title'}, 'Installierte Plugins'));
		if (list.length === 0)
			installed.appendChild(pluginRow(webuiL('set.plugins_none', 'No plugins installed')));
		list.forEach(function(pl) {
			installed.appendChild(pluginRow(pl.title || pl.name, pl.description,
				pluginButton(PLAY_ICON, webuiL('set.plugins_start', 'Start'), function(btn) { startPluginByName(pl.name, btn); })));
		});
		var manage = webuiEl('div', {className: 'webui-form-section'},
			webuiEl('div', {className: 'webui-form-section-title'}, 'Verwaltung'),
			pluginRow(webuiL('set.plugins_reload', 'Re-read the plugin list'), null,
				pluginButton(REFRESH_ICON, webuiL('set.plugins_reload_btn', 'Reload'), reloadPluginList)));
		el.replaceChildren(installed, manage);
	} catch (e) {
		el.replaceChildren(pluginRow(webuiL('set.plugins_notavail', 'Plugins not available') + ': ' + e.message));
	}
}

async function startPluginByName(name, btn) {
	btn.disabled = true;
	try {
		await webuiV1('POST', '/plugins/' + encodeURIComponent(name) + '/start');
		webuiShowToast(webuiL('set.plugin_started', 'Plugin started') + ': ' + name);
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

// The box reads the directories after it answered, so the list is read
// again a moment later
async function reloadPluginList(btn) {
	btn.disabled = true;
	try {
		await webuiV1('POST', '/plugins/reload');
		await new Promise(function(r) { setTimeout(r, 1500); });
		await loadPluginsPanel();
		webuiShowToast(webuiL('set.plugins_reloaded', 'Plugin list reloaded'));
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

// The box's own network shares over /api/v1/storage/shares: the 8 slots the
// screen at the box edits, each saved, mounted and unmounted on its own.
// Not part of the save bar: a slot is written whole, and a stored password
// is never read back, so the field opens empty and is only sent when typed.
// a share's state as the page names it, asked when shown so it is in the
// page's language
function shareStateName(state) {
	return {
		mounted: webuiL('set.mnt_state_mounted', 'mounted'),
		unmounted: webuiL('set.mnt_state_unmounted', 'not mounted'),
		foreign: webuiL('set.mnt_state_foreign', 'in use (another mount)')
	}[state] || state;
}

function renderMountsPanel() {
	setTimeout(loadSharesPanel, 0);
	return '<div class="settings-card">' + makeCardHeader('Netzlaufwerke') +
		'<div id="sharesPanel"><p aria-busy="true"></p></div></div>';
}

async function loadSharesPanel() {
	var el = document.getElementById('sharesPanel');
	if (!el) return;
	try {
		var data = await webuiV1('GET', '/storage/shares');
		drawShares(el, (data && data.items) || []);
	} catch (e) {
		el.replaceChildren(webuiEl('p', {}, webuiL('msg.error', 'Error') + ': ' + e.message));
	}
}

function shareSummary(sh) {
	return (sh.host ? (sh.host + ':' + sh.remote_dir + ' → ' + sh.local_dir) : webuiL('set.mnt_unconfigured', 'not configured')) +
		' · ' + shareStateName(sh.state);
}

function shareSlot(sh, open) {
	var chevron = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 9l6 6 6-6"/></svg>';
	var slot = webuiEl('div', {className: 'mount-slot' + (open ? ' open' : ''), id: 'mount-slot-' + sh.slot},
		webuiEl('div', {className: 'mount-slot-header', on: {click: function() { slot.classList.toggle('open'); }}},
			webuiEl('span', {}, 'Slot ' + sh.slot + ' ',
				webuiEl('span', {className: 'slot-summary', id: 'share_summary_' + sh.slot}, shareSummary(sh))),
			webuiMarkup(chevron)),
		webuiEl('fieldset', {className: 'mount-slot-body', id: 'share_body_' + sh.slot,
			style: {border: '0', margin: '0', padding: '0', 'min-width': '0'}},
			// an FTP share (lufs) is set, mounted and unmounted at the box only
			sh.type === 'lufs' ? shareRow('FTP (lufs)', webuiEl('span', {}, webuiL('set.mnt_lufs_note', 'An FTP share (lufs) is set up, mounted and unmounted at the box.'))) : shareFields(sh)));
	return slot;
}

function drawShares(el, items) {
	var all = webuiEl('div', {className: 'webui-form-row'},
		webuiEl('label', {}, webuiL('set.mnt_mount_auto', 'All set to Auto-Mount')),
		webuiEl('span', {className: 'webui-row-actions'},
			webuiEl('button', {type: 'button', className: 'webui-btn primary', on: {click: function() { mountAutomaticShares(this); }}},
				webuiMarkup(MOUNT_ICON), webuiL('set.mnt_mount', 'Mount'))));
	el.replaceChildren(all, ...items.map(function(sh) { return shareSlot(sh, false); }));
}

/* After an action: the slot acted on is drawn again from what the box
   answered, the others only get their summary, so what was typed into
   them and not saved yet stays. */
function updateShares(items, acted) {
	items.forEach(function(sh) {
		if (sh.slot === acted) {
			var old = document.getElementById('mount-slot-' + sh.slot);
			if (old) old.replaceWith(shareSlot(sh, true));
			return;
		}
		var summary = document.getElementById('share_summary_' + sh.slot);
		if (summary) summary.textContent = shareSummary(sh);
	});
}

// The slot's own form is locked while the box answers for it.
async function onShare(i, request) {
	var body = document.getElementById('share_body_' + i);
	if (body) body.disabled = true;
	try {
		var res = await request();
		updateShares((res && res.items) || [], i);
		return true;
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
		if (body) body.disabled = false;
		return false;
	}
}

function shareRow(label, control) {
	return webuiEl('div', {className: 'webui-form-row'}, webuiEl('label', {}, label), control);
}

function shareFields(sh) {
	var i = sh.slot;
	var input = function(key, value, type) {
		return webuiEl('input', {type: type || 'text', id: 'share_' + key + '_' + i, value: value || ''});
	};
	var type = webuiEl('select', {id: 'share_type_' + i},
		webuiEl('option', {value: 'nfs'}, 'NFS'),
		webuiEl('option', {value: 'cifs'}, 'CIFS'),
		sh.type === 'lufs' ? webuiEl('option', {value: 'lufs'}, 'FTP (lufs)') : null);
	type.value = sh.type;
	var pass = input('password', '', 'password');
	pass.placeholder = sh.has_password ? webuiL('set.mnt_pass_kept', 'stored – leave empty to keep it') : '';
	pass.autocomplete = 'new-password';
	var clear = sh.has_password ? webuiEl('label', {style: {display: 'inline-flex', gap: '.3rem', 'margin-left': '.5rem'}},
		webuiEl('input', {type: 'checkbox', id: 'share_clearpass_' + i}), ' ' + webuiL('set.mnt_pass_clear', 'remove')) : null;
	var credentials = webuiEl('div', {className: 'cifs-fields' + (sh.type !== 'nfs' ? ' visible' : '')},
		shareRow(webuiL('set.mnt_user', 'User'), input('user', sh.user)),
		shareRow(webuiL('set.mnt_pass', 'Password'), webuiEl('span', {}, pass, clear)));
	type.addEventListener('change', function() {
		credentials.classList.toggle('visible', type.value !== 'nfs');
	});
	var local = input('local_dir', sh.local_dir);
	var auto = webuiEl('input', {type: 'checkbox', id: 'share_automount_' + i, checked: !!sh.automount});
	var button = function(icon, text, onClick) {
		return webuiEl('button', {type: 'button', className: 'webui-btn', on: {click: function() { onClick(this); }}},
			webuiMarkup(icon), text);
	};
	return [
		shareRow(webuiL('tm.type', 'Type'), type),
		shareRow(webuiL('set.mnt_ip', 'Server IP'), input('host', sh.host)),
		shareRow(webuiL('set.mnt_dir', 'Server directory'), input('remote_dir', sh.remote_dir)),
		shareRow(webuiL('set.mnt_local', 'Local mount point'), webuiEl('span', {}, local, ' ',
			webuiEl('button', {type: 'button', className: 'webui-btn', title: webuiL('picker.title', 'Choose folder'),
				on: {click: function() { webuiPickInto(local.id, local.value || '/media'); }}}, webuiMarkup(FOLDER_ICON)))),
		shareRow('Auto-Mount', webuiEl('label', {className: 'webui-toggle'}, auto, webuiEl('span', {className: 'webui-toggle-slider'}))),
		shareRow(webuiL('set.mnt_opts', 'Mount options') + ' 1', input('options1', sh.options1)),
		shareRow(webuiL('set.mnt_opts', 'Mount options') + ' 2', input('options2', sh.options2)),
		shareRow(webuiL('set.mnt_mac', 'MAC address'), input('mac', sh.mac)),
		credentials,
		shareRow(webuiL('set.mnt_actions', 'Actions'), webuiEl('span', {className: 'webui-row-actions'},
			button(SAVE_ICON, webuiL('bqe.save', 'Save'), function() { saveShare(i); }), ' ',
			button(MOUNT_ICON, webuiL('set.mnt_mount', 'Mount'), function() { shareAction(i, 'mount'); }), ' ',
			button(EJECT_ICON, webuiL('set.mnt_umount', 'Unmount'), function() { shareAction(i, 'unmount'); })))
	];
}

async function saveShare(i) {
	var val = function(key) { return document.getElementById('share_' + key + '_' + i).value.trim(); };
	var body = {
		automount: document.getElementById('share_automount_' + i).checked,
		type: val('type'), local_dir: val('local_dir'), host: val('host'), remote_dir: val('remote_dir'),
		user: val('user'), options1: val('options1'), options2: val('options2'), mac: val('mac')
	};
	// typed: the new one; the box's own left alone otherwise, or taken away
	var pass = document.getElementById('share_password_' + i).value;
	var clear = document.getElementById('share_clearpass_' + i);
	if (pass !== '')
		body.password = pass;
	else if (clear && clear.checked)
		body.password = '';
	if (await onShare(i, function() { return webuiV1('PUT', '/storage/shares/' + i, {body: body}); }))
		webuiShowToast(webuiL('set.mnt_saved', 'Network share saved'));
}

function shareAction(i, act) {
	return onShare(i, function() { return webuiV1('POST', '/storage/shares/' + i + '/' + act); });
}

async function mountAutomaticShares(btn) {
	btn.disabled = true;
	try {
		var res = await webuiV1('POST', '/storage/shares/mount');
		var items = (res && res.items) || [];
		var failed = items.filter(function(o) { return !o.mounted; });
		if (items.length === 0)
			webuiShowToast(webuiL('set.mnt_auto_none', 'No Auto-Mount share is left to mount'));
		else if (failed.length === 0)
			webuiShowToast(items.length + ' ' + webuiL('set.mnt_auto_done', 'mounted'));
		else
			webuiShowToast(failed.map(function(o) { return 'Slot ' + o.slot + ': ' + o.detail; }).join('; '), true);
		// only the summaries: what was typed into a slot and not saved stays
		var now = await webuiV1('GET', '/storage/shares');
		updateShares((now && now.items) || [], -1);
	} catch (e) {
		webuiShowToast(webuiL('msg.error', 'Error') + ': ' + e.message, true);
	}
	btn.disabled = false;
}

// M5b: Sidebar nach SETTINGS_GROUPS gruppiert (Zwischentitel + Icon je
// Kategorie), Mobile-Select mit <optgroup> je Gruppe. Jede Beschriftung
// ist Text, aus der Registry wie aus dem Manifest einer Erweiterung,
// und wird hier escapt.
function buildNavigation() {
	var sidebarHtml = '';
	var mobileHtml = '';
	var groups = {};
	SETTINGS_GROUPS.forEach(function(g) { groups[g.id] = []; });

	v1setCategories().forEach(function(cat) {
		(groups[cat.group] || groups.ext).push({cat: cat});
	});
	Object.keys(EXT_SETTINGS_CATS).forEach(function(cid) {
		var cat = EXT_SETTINGS_CATS[cid];
		(groups[cat.group] || groups.ext).push({cat: cat});
	});

	SETTINGS_GROUPS.forEach(function(g) {
		var entries = groups[g.id];
		if (!entries.length) return;
		sidebarHtml += '<div class="grp">' + escapeHtml(g.label) + '</div>';
		mobileHtml += '<optgroup label="' + escapeHtml(g.label) + '">';
		entries.forEach(function(e) {
			var label = escapeHtml(e.cat.label);
			var icon = e.cat.icon || SETTINGS_DEFAULT_ICON;
			sidebarHtml += '<button data-cat="' + e.cat.id + '"' +
				(e.cat.id === activeCategory ? ' class="active"' : '') +
				' onclick="switchCategory(\'' + e.cat.id + '\')">' + icon +
				'<span>' + label + '</span></button>';
			mobileHtml += '<option value="' + e.cat.id + '"' +
				(e.cat.id === activeCategory ? ' selected' : '') + '>' + label + '</option>';
		});
		mobileHtml += '</optgroup>';
	});

	document.getElementById('settingsSidebar').innerHTML = sidebarHtml;
	document.getElementById('mobileCategorySelect').innerHTML = mobileHtml;
}

function switchCategory(catId) {
	if (activeCategory === catId && document.getElementById('panel-' + catId)) return;
	activeCategory = catId;

	// Update hash without triggering hashchange
	history.replaceState(null, '', '#' + catId);

	// Update sidebar active state
	document.querySelectorAll('#settingsSidebar button').forEach(function(btn) {
		btn.classList.toggle('active', btn.getAttribute('data-cat') === catId);
	});

	// Update mobile select
	var mobileSelect = document.getElementById('mobileCategorySelect');
	if (mobileSelect) mobileSelect.value = catId;

	// Show/hide panels — lazy render on first visit
	var content = document.getElementById('settingsContent');
	var panels = content.querySelectorAll('.settings-panel');
	panels.forEach(function(p) { p.classList.remove('active'); });

	var panel = document.getElementById('panel-' + catId);
	if (!panel) {
		// First visit: render and insert
		panel = document.createElement('div');
		panel.className = 'settings-panel';
		panel.id = 'panel-' + catId;
		var drawn = renderCategory(catId);
		if (drawn instanceof Node)
			panel.appendChild(drawn);
		else
			panel.innerHTML = drawn;
		v1setFillSlots(panel);
		content.appendChild(panel);
		renderedPanels[catId] = true;
		v1setApplyConditions();
	}
	panel.classList.add('active');
}

// M5c: sammelt über ALLE gerenderten Panels (nicht nur das aktive) —
// gecachte Panels behalten ungespeicherte Eingaben beim Kategorie-
// Wechsel, und die zentrale Save-Leiste speichert sie alle zusammen
function collectValues() {
	var values = {};
	var content = document.getElementById('settingsContent');
	if (!content) return values;

	content.querySelectorAll('select[data-key]:not([data-extcfg]), input[type="text"][data-key]:not([data-extcfg]), input[type="number"][data-key]:not([data-extcfg]), input[type="password"][data-key]:not([data-extcfg])').forEach(function(el) {
		values[el.getAttribute('data-key')] = el.value;
	});
	content.querySelectorAll('input[type="checkbox"][data-key]:not([data-extcfg])').forEach(function(el) {
		values[el.getAttribute('data-key')] = el.checked ? el.getAttribute('data-on') : el.getAttribute('data-off');
	});
	return values;
}

// Dirty-Zähler für die Save-Leiste: gleiche Diff-Logik wie
// saveSettings (String-Vergleich gegen configData), dazu die Zeilen
// der Box-Einstellungen (settings-v1.js)
function webuiCountDirty() {
	var values = collectValues();
	var dirty = v1setDirty().count;
	for (var key in values) {
		var origKey = key.replace(/\./g, '_dot_').replace(/-/g, '_bind_');
		if (values[key] !== (configData[origKey] || '')) dirty++;
	}
	return dirty;
}

function webuiUpdateSaveBar() {
	var bar = document.getElementById('settingsSaveBar');
	if (!bar) return;
	var n = webuiCountDirty();
	document.getElementById('saveBarCount').textContent = n;
	bar.hidden = (n === 0);
}

// Verwerfen: alle Panel-Caches wegwerfen und aktive Kategorie aus
// configData neu rendern (die übrigen rendern beim nächsten Besuch)
function discardSettings() {
	v1setDiscard();
	renderedPanels = {};
	document.querySelectorAll('#settingsContent .settings-panel').forEach(function(p) { p.remove(); });
	var cat = activeCategory;
	activeCategory = '';
	switchCategory(cat);
	webuiUpdateSaveBar();
}

// Box settings go to PATCH /api/v1/settings/{section}, what only the
// registry knows to config=neutrino. A key the box refuses keeps its
// input and reason (settings-v1.js) through the redraw afterwards.
async function saveSettings() {
	var values = collectValues();
	var params = 'action=submit';
	var changed = 0;
	var v1dirty = v1setDirty();

	for (var key in values) {
		var origKey = key.replace(/\./g, '_dot_').replace(/-/g, '_bind_');
		if (values[key] !== (configData[origKey] || '')) {
			params += '&' + encodeURIComponent(key) + '=' + encodeURIComponent(values[key]);
			changed++;
		}
	}

	if (changed === 0 && v1dirty.count === 0) {
		webuiShowToast(webuiL('set.nochanges', 'No changes'));
		return;
	}

	// no edits while the box answers and the form is drawn again: a
	// refusal arriving late must not replace a newer input
	settingsBusy(true);
	try {
		await saveSettingsNow(values, params, changed, v1dirty);
	} finally {
		settingsBusy(false);
	}
}

// Locks the form and the save bar while a write is answered (saving,
// clearing a credential)
function settingsBusy(on) {
	var content = document.getElementById('settingsContent');
	content.inert = on;
	if (on) content.setAttribute('aria-busy', 'true');
	else content.removeAttribute('aria-busy');
	document.querySelectorAll('#settingsSaveBar .webui-btn').forEach(function(b) { b.disabled = on; });
}

async function saveSettingsNow(values, params, changed, v1dirty) {
	var v1 = {saved: 0, failed: 0, restart: false};
	if (v1dirty.count)
		v1 = await v1setSave(v1dirty);
	var oldOk = true;
	if (changed) {
		try {
			await webuiApi('config', 'config=neutrino&' + params);
		} catch (e) {
			oldOk = false;
		}
	}

	var saved = v1.saved + (oldOk ? changed : 0);
	if (!oldOk)
		webuiShowToast(webuiL('tm.save_fail', 'Saving failed'), true);
	else if (v1.failed)
		webuiShowToast(webuiL('set.save_some_fail', 'Not every setting was saved, see the marked fields'), true);
	else
		webuiShowToast(saved + ' ' + (saved > 1 ? webuiL('set.saved_many', 'settings saved') : webuiL('set.saved_one', 'setting saved')) +
			(v1.restart ? ' \u2013 ' + webuiL('set.saved_restart', 'some take effect after a restart') : ''));
	// a refused save of the old interface keeps the form as it is
	if (oldOk)
		await loadSettings();
	else
		await v1setRefresh();
	webuiUpdateSaveBar();
}

// Werte, die ausserhalb der Domaene ihres Feldes liegen, auf den Wert
// ziehen, den das Feld ohnehin anzeigt. Ohne das meldet der Dirty-Vergleich
// beim ersten Rendern eine Aenderung, die der Nutzer nie gemacht hat — und
// der naechste Save schriebe sie fest.
//   - access: Neutrino wertet nur die 2 aus (personalize.cpp:961), alles
//     andere heisst 'ohne PIN'. personalize_settingsmager und
//     personalize_media_movieplayer haben sogar die 1 als Default
//     (settings.cpp:69/99).
//   - select: ein nicht angebotener Wert zeigt im Browser die erste Option.
// Geschrieben wird dadurch nichts: saveSettings schickt nur echte Diffs.
function normalizeDomains() {
	SETTINGS_REGISTRY.forEach(function(cat) {
		(cat.sections || []).forEach(function(sec) {
			sec.fields.forEach(function(f) {
				if (!f.key) return;
				var k = f.key.replace(/\./g, '_dot_').replace(/-/g, '_bind_');
				var val = configData[k];
				if (val === undefined) return;
				if (f.type === 'access') {
					if (val !== '2') configData[k] = '0';
				} else if (f.type === 'select' && f.options) {
					var known = f.options.some(function(o) { return o.v === val; });
					if (!known) configData[k] = f.options[0].v;
				}
			});
		});
	});
}

async function loadSettings() {
	try {
		// v4 liefert ein valides Envelope-Dokument - der historische
		// Brace-Zaehler (Workaround fuer das alte Doppel-Dokument) entfaellt.
		// Daneben die Einstellungen der Box ueber /api/v1 (settings-v1.js);
		// ohne sie bleibt die Seite bei der Registry.
		var both = await Promise.all([webuiApi('config', {config: 'neutrino'}), v1setLoad()]);
		var resp = both[0];
		configData = (resp && resp.data && resp.data.config) || {};
	} catch (e) {
		document.getElementById('settingsContent').innerHTML =
			'<div class="settings-loading">' + webuiLH('set.load_fail', 'Loading the settings failed') + '</div>';
		return;
	}

	normalizeDomains();
	originalValues = Object.assign({}, configData);

	// Enabled extensions with a settings definition become categories.
	// Manifest strings prefixed with "L:" are resolved from the
	// extension's language pack (webuiExtL; discovery already loads the
	// dictionary for enabled extensions) — literals stay valid.
	function resolveExtL(dir, node) {
		if (typeof node === 'string')
			return node.indexOf('L:') === 0 ? webuiExtL(dir, node.slice(2), node) : node;
		if (Array.isArray(node))
			return node.map(function(n) { return resolveExtL(dir, n); });
		if (node && typeof node === 'object') {
			var out = {};
			Object.keys(node).forEach(function(k) { out[k] = resolveExtL(dir, node[k]); });
			return out;
		}
		return node;
	}
	EXT_SETTINGS_CATS = {};
	try {
		webuiForgetExtensions(); // frisch entdecken (Enable kann sich geaendert haben)
		(await webuiDiscoverExtensions()).forEach(function(m) {
			if (!m.enabled || !m.settings || !m.settings.sections) return;
			var cid = 'ext-' + m.dir;
			EXT_SETTINGS_CATS[cid] = {
				id: cid,
				dir: m.dir,
				label: resolveExtL(m.dir, m.settings.label || m.name),
				config: m.settings.config || 'webui',
				sections: resolveExtL(m.dir, m.settings.sections)
			};
		});
	} catch (ignore) {}

	// Clear cached panels — config changed
	renderedPanels = {};
	document.getElementById('settingsContent').innerHTML = '';

	// Determine active category from hash or default to first
	var targetCategory = categoryOfHash() || v1setCategories()[0].id;

	activeCategory = '';
	buildNavigation();
	switchCategory(targetCategory);
	webuiUpdateSaveBar();
}

// The category the address names; one dissolved into a section of the
// box (an old bookmark such as #lcd) leads to that section
function categoryOfHash() {
	var hash = location.hash.replace('#', '');
	if (V1SET.dissolved[hash]) return V1SET.dissolved[hash];
	var known = v1setCategories().some(function(c) { return c.id === hash; }) || EXT_SETTINGS_CATS[hash];
	return known ? hash : '';
}

// Hash navigation
window.addEventListener('hashchange', function() {
	var cat = categoryOfHash();
	if (cat) switchCategory(cat);
});

// M5c: Dirty-State live verfolgen (Delegation — Panels werden lazy
// gerendert und gecacht, der Listener auf dem Container sieht alles)
document.getElementById('settingsContent').addEventListener('input', webuiUpdateSaveBar);
document.getElementById('settingsContent').addEventListener('change', webuiUpdateSaveBar);
// Observer-Felder (disabledIf) folgen ihrem uebergeordneten Eintrag,
// die Zeilen der Box ihren Bedingungen
document.getElementById('settingsContent').addEventListener('change', webuiApplyDeps);
document.getElementById('settingsContent').addEventListener('change', function(e) { v1setApplyConditions(e); });
// ein abgelehnter Wert, den der Nutzer wieder bearbeitet, ist erledigt
document.getElementById('settingsContent').addEventListener('input', function(e) { v1setEdited(e.target); });
document.getElementById('settingsContent').addEventListener('change', function(e) { v1setEdited(e.target); });

// The rows' labels are written in settings-registry.js, which is read once
// the texts are in: it states them as it loads, the start pages to choose
// among too, the extensions' with them. settings-v1.js builds its icons with
// the registry's helper, so it comes after.
webuiAfterTexts(async function() {
	DEP_HINT = webuiL('set.dep_locked', 'Controlled by the entry above — no effect while that one is enabled');
	await webuiDiscoverExtensions();
	await webuiLoadScript('/js/settings-registry.js?v=19');
	await webuiLoadScript('/js/settings-v1.js?v=7');
	// Was anderswo gespeichert wird (am Geraet, von einer zweiten Seite),
	// liest die Seite nach
	webuiV1Events({'settings-changed': v1setChanged, open: v1setStreamOpen});
	loadSettings();
});
