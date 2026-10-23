// The script of the system information's page (extension sysinfo).

// label is text; value is markup, escaped by the caller where it carries data
function row(label, value) {
	return '<span class="sysinfo-label">' + escapeHtml(label) + '</span><span class="sysinfo-value">' + value + '</span>';
}

// the bar says nothing the text after it does not, so it is not read out
function bar(label, percent, text) {
	return '<span class="sysinfo-label">' + label + '</span>' +
		'<span class="sysinfo-value">' +
			'<div class="sysinfo-bar" aria-hidden="true"><div class="sysinfo-bar-fill" style="width:' + percent + '%"></div></div>' +
			'<small>' + text + ' (' + percent + '%)</small>' +
		'</span>';
}

/* The cards stand once the box has answered; until then, and when it does
   not, one block above them says so, with Reload. A refresh keeps the cards
   it has until the new answer is in. */
async function loadSysInfo() {
	var state = document.getElementById('sysinfo-state');
	var cards = document.getElementById('sysinfo-cards');
	if (cards.hidden)
		state.replaceChildren(webuiState('loading', webuiL('msg.loading', 'Loading...')));
	try {
		// over /api/v1, in the shape the page has always read (webui-header.js)
		var sys = await webuiSystemInfo(true);
		if (!sys) {
			// the shared reader says only that it got nothing; the box's own
			// answer says why (and asks for a sign-in where one is wanted)
			await webuiV1('GET', '/system/info');
			sys = await webuiSystemInfo(true);
			if (!sys)
				throw new Error(webuiL('state.failed', 'Could not be read'));
		}
		var ver = await webuiVersions(sys);

		// Hardware
		var hw = '';
		if (sys.cpu_model) hw += row('CPU', escapeHtml(sys.cpu_model));
		hw += row(webuiL('si.cores', 'Cores'), sys.cpu && sys.cpu.cores !== undefined ? sys.cpu.cores : '–');
		// the load where the box could measure it, else its average, else nothing
		hw += row(webuiL('si.load', 'Load'), sys.cpu && sys.cpu.percent !== undefined ? sys.cpu.percent + '%'
			: (sys.load ? sys.load[0].toFixed(2) : '–'));
		if (sys.temperature !== null && sys.temperature !== undefined)
			hw += row(webuiL('si.temperature', 'Temperature'), sys.temperature + '°C');
		if (sys.box) hw += row('Box', escapeHtml(sys.box.vendor + ' ' + sys.box.name + ' (' + sys.box.arch + ')'));
		document.getElementById('sysinfo-hw').innerHTML = hw;

		// Speicher
		var mem = '';
		if (sys.memory)
			mem += bar('RAM', sys.memory.percent, webuiFormatKB(sys.memory.used) + ' / ' + webuiFormatKB(sys.memory.total));
		// One bar per mounted storage filesystem; older backends only
		// deliver the single root-fs "disk" object
		if (sys.disks && sys.disks.length) {
			for (var d = 0; d < sys.disks.length; d++) {
				var dsk = sys.disks[d];
				var dlabel = (dsk.mount === '/') ? webuiLH('si.root', 'System (/)') : escapeHtml(dsk.mount);
				if (dsk.answered === false)
					mem += '<span class="sysinfo-label">' + dlabel + '</span><span class="sysinfo-value webui-text-muted">' +
						webuiLH('si.no_answer', 'not answering') + '</span>';
				else
					mem += bar(dlabel, dsk.percent, webuiFormatKB(dsk.free) + ' ' + webuiLH('si.free_of', 'free of') + ' ' + webuiFormatKB(dsk.total));
			}
		} else if (sys.disk)
			mem += bar(webuiLH('si.root', 'System (/)'), sys.disk.percent, webuiFormatKB(sys.disk.free) + ' ' + webuiLH('si.free_of', 'free of') + ' ' + webuiFormatKB(sys.disk.total));
		document.getElementById('sysinfo-mem').innerHTML = mem;

		// System
		var s = '';
		s += row(webuiL('si.hostname', 'Host name'), escapeHtml(sys.hostname || ''));
		s += row(webuiL('si.uptime', 'Uptime'), webuiFormatUptime(sys.uptime));
		s += row(webuiL('si.kernel', 'Kernel'), escapeHtml(sys.kernel || ''));
		if (sys.os) s += row('OS', escapeHtml(sys.os));
		if (sys.load) s += row(webuiL('si.load_avg', 'Load average'), sys.load[0].toFixed(2) + ' / ' + sys.load[1].toFixed(2) + ' / ' + sys.load[2].toFixed(2));
		if (sys.processes) s += row(webuiL('si.processes', 'Processes'), sys.processes.running + ' ' + webuiLH('si.active', 'active') + ' / ' + sys.processes.total + ' ' + webuiLH('si.total', 'total'));
		document.getElementById('sysinfo-sys').innerHTML = s;

		// Software, as the About window gives it: the interface (a fork with
		// its own name and what it is based on), its web server, its API and
		// Neutrino with the revision it was built from
		var sw = '';
		if (ver.webui)
			sw += row(ver.webui_name || 'Neutrino-Web-UI', escapeHtml(ver.webui + (ver.webui_base ?
				' · ' + webuiL('about.based_on', 'based on Neutrino-Web-UI %s').replace('%s', ver.webui_base) : '')));
		if (ver.webserver) sw += row(webuiL('about.webserver', 'Web server'), escapeHtml(ver.webserver));
		if (ver.api) sw += row(webuiL('about.api', 'Interface (API)'), escapeHtml(ver.api));
		if (ver.neutrino) {
			var nv = ver.neutrino_name ? ver.neutrino_name + ' ' + ver.neutrino : ver.neutrino;
			if (ver.neutrino_git) nv += ' (' + ver.neutrino_git.split(' ')[0] + ')';
			sw += row('Neutrino', escapeHtml(nv));
		}
		document.getElementById('sysinfo-sw').innerHTML = sw;

		// Netzwerk
		var net = '';
		if (sys.interfaces && sys.interfaces.length) {
			for (var i = 0; i < sys.interfaces.length; i++) {
				var iface = sys.interfaces[i];
				// the dot in words too, for who cannot see its colour
				var up = iface.up ? webuiLH('si.if_up', 'up') : webuiLH('si.if_down', 'down');
				net += '<div class="sysinfo-iface">' +
					'<span class="sysinfo-iface-status ' + (iface.up ? 'up' : 'down') + '" title="' + up + '"></span>' +
					'<span class="webui-visually-hidden">' + up + ': </span>' +
					'<span class="sysinfo-iface-name">' + escapeHtml(iface.name) + '</span>' +
					'<span>' + escapeHtml(iface.ip || '–') + '</span>' +
					'<span class="sysinfo-iface-detail">' + escapeHtml(iface.mac || '') + '</span>' +
				'</div>';
			}
		} else {
			net = '<span class="sysinfo-label">' + webuiLH('si.no_ifaces', 'No interfaces') + '</span>';
		}
		document.getElementById('sysinfo-net').innerHTML = net;
		state.replaceChildren();
		cards.hidden = false;
	} catch (e) {
		// no half-filled cards: the answer that is missing says why
		cards.hidden = true;
		state.replaceChildren(webuiStateFor(e, {retry: loadSysInfo}));
		return;
	}
	// a list opened below is read again with the rest
	document.querySelectorAll('.sysinfo-details[open]').forEach(function(d) {
		delete d.querySelector('pre').dataset.loaded;
		sysInfoToggle(d, d.dataset.kind);
	});
	revealHashTarget();
}

// Jump to the card named in the URL hash (the dashboard network card
// links here as #network). Runs after the data has been rendered, so
// the browser scrolls to the filled card rather than the empty one.
function revealHashTarget() {
	var id = location.hash.slice(1);
	if (!id) return;
	var card = document.getElementById(id);
	if (!card || !card.classList.contains('sysinfo-card')) return;
	card.scrollIntoView({block: 'center', behavior: 'smooth'});
	card.classList.add('sysinfo-card-target');
	card.focus({preventScroll: true});
}

async function sysInfoToggle(details, type) {
	if (!details.open) return;
	var pre = details.querySelector('pre');
	// read once, and one read at a time: a later one would race the first
	if (pre.dataset.loaded || pre.getAttribute('aria-busy') === 'true') return;
	// marked once read: a read that failed is tried again on the next opening
	pre.setAttribute('aria-busy', 'true');

	// Read through the two routes that answer only these, as text
	try {
		var text;
		if (type === 'ps') {
			var ps = await webuiV1('GET', '/system/processes');
			var pad = function(v, n) { v = String(v); while (v.length < n) v = ' ' + v; return v; };
			text = '  PID  PPID   UID S    RSS(k) COMMAND\n' + ((ps && ps.items) || []).map(function(p) {
				return pad(p.id, 5) + ' ' + pad(p.ppid, 5) + ' ' + pad(p.uid, 5) + ' ' + p.state + ' ' +
					pad(Math.round(p.rss / 1024), 9) + ' ' + p.command;
			}).join('\n');
		} else {
			var log = await webuiV1('GET', '/system/kernel-log');
			text = ((log && log.items) || []).map(function(l) { return l.text; }).join('\n');
		}
		pre.textContent = text.trim() || webuiL('si.no_output', '(no output)');
		pre.dataset.loaded = '1';
	} catch (e) {
		pre.textContent = webuiL('msg.error', 'Error') + ': ' + webuiErrText(e);
	}
	pre.removeAttribute('aria-busy');
}

webuiExtTexts('sysinfo').then(loadSysInfo);
