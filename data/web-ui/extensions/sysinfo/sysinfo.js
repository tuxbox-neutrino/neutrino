// The script of the system information's page (extension sysinfo).

// label is text; value is markup, escaped by the caller where it carries data
function row(label, value) {
	return '<span class="sysinfo-label">' + escapeHtml(label) + '</span><span class="sysinfo-value">' + value + '</span>';
}

function bar(label, percent, text) {
	return '<span class="sysinfo-label">' + label + '</span>' +
		'<span class="sysinfo-value">' +
			'<div class="sysinfo-bar"><div class="sysinfo-bar-fill" style="width:' + percent + '%"></div></div>' +
			'<small>' + text + ' (' + percent + '%)</small>' +
		'</span>';
}

async function loadSysInfo() {
	try {
		// over /api/v1, in the shape the page has always read (webui-header.js)
		var sys = await webuiSystemInfo(true);
		if (!sys) throw new Error('/api/v1/system/info');
		var ver = await webuiVersions(sys);

		// Hardware
		var hw = '';
		if (sys.cpu_model) hw += row('CPU', escapeHtml(sys.cpu_model));
		hw += row(webuiL('si.cores', 'Cores'), sys.cpu.cores);
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
				var dlabel = (dsk.mount === '/') ? 'Disk /' : escapeHtml(dsk.mount);
				mem += bar(dlabel, dsk.percent, webuiFormatKB(dsk.free) + ' ' + webuiLH('si.free_of', 'free of') + ' ' + webuiFormatKB(dsk.total));
			}
		} else if (sys.disk)
			mem += bar('Disk', sys.disk.percent, webuiFormatKB(sys.disk.free) + ' ' + webuiLH('si.free_of', 'free of') + ' ' + webuiFormatKB(sys.disk.total));
		document.getElementById('sysinfo-mem').innerHTML = mem;

		// System
		var s = '';
		s += row('Hostname', escapeHtml(sys.hostname || ''));
		s += row('Uptime', webuiFormatUptime(sys.uptime));
		s += row('Kernel', escapeHtml(sys.kernel || ''));
		if (sys.os) s += row('OS', escapeHtml(sys.os));
		if (sys.load) s += row('Load', sys.load[0].toFixed(2) + ' / ' + sys.load[1].toFixed(2) + ' / ' + sys.load[2].toFixed(2));
		if (sys.processes) s += row(webuiL('si.processes', 'Processes'), sys.processes.running + ' ' + webuiLH('si.active', 'active') + ' / ' + sys.processes.total + ' ' + webuiLH('si.total', 'total'));
		document.getElementById('sysinfo-sys').innerHTML = s;

		// Software
		var sw = '';
		if (ver.neutrino) {
			var nv = ver.neutrino_name ? ver.neutrino_name + ' ' + ver.neutrino : ver.neutrino;
			if (ver.neutrino_git) nv += ' (' + ver.neutrino_git.substring(0, 8) + ')';
			sw += row('Neutrino', escapeHtml(nv));
		}
		if (ver.webui) sw += row('Neutrino-Web-UI', escapeHtml(ver.webui));
		if (ver.api) sw += row('API', escapeHtml(ver.api));
		if (ver.server) sw += row('Server', ver.server);
		document.getElementById('sysinfo-sw').innerHTML = sw;

		// Netzwerk
		var net = '';
		if (sys.interfaces && sys.interfaces.length) {
			for (var i = 0; i < sys.interfaces.length; i++) {
				var iface = sys.interfaces[i];
				net += '<div class="sysinfo-iface">' +
					'<span class="sysinfo-iface-status ' + (iface.up ? 'up' : 'down') + '"></span>' +
					'<span class="sysinfo-iface-name">' + escapeHtml(iface.name) + '</span>' +
					'<span>' + escapeHtml(iface.ip || '–') + '</span>' +
					'<span class="sysinfo-iface-detail">' + escapeHtml(iface.mac || '') + '</span>' +
				'</div>';
			}
		} else {
			net = '<span class="sysinfo-label">' + webuiLH('si.no_ifaces', 'No interfaces') + '</span>';
		}
		document.getElementById('sysinfo-net').innerHTML = net;
	} catch (e) {
		document.getElementById('sysinfo-hw').innerHTML = '<span class="sysinfo-value" style="color:var(--webui-danger)">' + webuiLH('msg.error', 'Error') + ': ' + escapeHtml(e.message) + '</span>';
	}
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
	if (pre.dataset.loaded) return;
	pre.dataset.loaded = '1';

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
	} catch (e) {
		pre.textContent = webuiL('msg.error', 'Error') + ': ' + e.message;
	}
}

webuiExtTexts('sysinfo').then(loadSysInfo);
