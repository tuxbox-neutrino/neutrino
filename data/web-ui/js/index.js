// The script of index.html.

// First run (webui.conf startup != true) -> setup wizard,
// otherwise jump to the configured start page. Any error
// falls back to the dashboard so the UI is always reachable.
(function () {
	var fallback = '/dashboard.html';
	// the interface's own pages a start page can be
	var core = ['dashboard', 'bouquets', 'epg', 'timer', 'recordings', 'tools',
		'settings', 'sysinfo', 'screenshot', 'bouquet-editor'];
	// what the classic settings page and the shipped yWeb.conf
	// write ("none" there), mapped onto the modern pages
	var classic = {
		none: 'dashboard.html', bouquets: 'bouquets.html',
		control: 'dashboard.html', remote: 'screenshot.html',
		screenshot: 'screenshot.html',
		// pages that became extensions (WEBUI_MOVED_PAGES in webui-header.js)
		'files.html': 'extensions/files/index.html'
	};
	// the web interface's own settings, readable from the home network
	fetch('/api/v1/webui/settings', {credentials: 'same-origin', cache: 'no-store'})
		.then(function (r) {
			/* off the home network without a session: the sign-in;
			   a session that still owes the owner's own password: the
			   page that sets it. Both come back here to decide. */
			if (r.status === 403) {
				return r.json().catch(function () { return null; }).then(function (p) {
					var owed = p && p.type === '/errors/password-change-required';
					location.replace((owed ? '/password.html' : '/login.html') + '?next=%2F');
					throw new Error('sign-in');
				});
			}
			if (!r.ok) throw new Error(r.status);
			return r.json();
		})
		.then(function (data) {
			var cfg = {};
			((data && data.items) || []).forEach(function (it) { cfg[it.key] = it.value; });
			if (cfg.startup !== 'true') {
				location.replace('/wizard.html');
				return;
			}
			var page = (cfg.start_page || '').replace(/^\//, '');
			if (Object.prototype.hasOwnProperty.call(classic, page))
				page = classic[page];
			// one of the interface's own pages, never an address elsewhere
			// and never this page itself (it would load itself forever)
			var own = /^([A-Za-z0-9_-]+)\.html$/.exec(page);
			if (own && core.indexOf(own[1]) >= 0) {
				location.replace('/' + page);
				return;
			}
			/* or the page of an extension that is installed and switched
			   on, the one its manifest names; one taken out or switched off
			   since leaves the dashboard. Switched on is what the settings
			   say, 1 or 0, and where they say nothing the manifest's
			   "default" (webuiExtSwitchedOn in webui-header.js) */
			var ext = /^extensions\/([A-Za-z0-9_-]+)\/([A-Za-z0-9_.%-]+)$/.exec(page);
			var said = ext ? cfg['ext_' + ext[1] + '_enabled'] : undefined;
			if (!ext || said === '0') {
				location.replace(fallback);
				return;
			}
			return fetch('/extensions/' + ext[1] + '/manifest.json', {credentials: 'same-origin', cache: 'no-cache'})
				.then(function (r) { return r.ok ? r.json() : null; })
				.then(function (m) {
					var on = said === '1' || (m && m['default'] === 'on');
					var named = on && m && typeof m.entry === 'string' && encodeURIComponent(m.entry) === ext[2];
					location.replace(named ? '/' + page : fallback);
				});
		})
		.catch(function (e) {
			// on the way to the sign-in already
			if (e && e.message === 'sign-in') return;
			location.replace(fallback);
		});
})();
