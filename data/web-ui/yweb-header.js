/**
 * yWeb Common Header Component
 * Provides consistent header across all yWeb pages
 */

// Central configuration
const YWEB_CONFIG = {
	logo: '/images/logo.png',
	title: 'yWeb'
};

// i18n helper: reads the server-rendered dictionary from Y_Lang.yjs
// (window.YWEB_L, CLanguage chain incl. EN-Fallback). Second argument is
// the English fallback for pages that do not load the dictionary and for
// keys the server marks as missing ("# key #").
function ywebL(key, fallbackEN) {
	var d = window.YWEB_L;
	var v = d && d[key];
	if (v && v === '# yw.' + key + ' #') v = ''; // CLanguage missing-marker
	return v || fallbackEN || key;
}

// SVG Icons for menu items
const YWEB_ICONS = {
	dashboard: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg>',
	bouquets: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5,3 19,12 5,21"/></svg>',
	epg: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/></svg>',
	timer: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/></svg>',
	remote: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="7" y="2" width="10" height="20" rx="2"/><circle cx="12" cy="18" r="1"/></svg>',
	settings: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>',
	tools: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z"/></svg>'
};

// Shared modal close button
function _ywebCloseBtn(onclick) {
	return '<button class="yweb-modal-close" onclick="' + onclick + '">' +
		'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M6 18L18 6M6 6l12 12"/></svg>' +
	'</button>';
}

// Footer HTML Template
const YWEB_FOOTER_HTML = `
<footer class="yweb-footer">
	yWeb <span id="yweb-footer-version">--</span> ·
	<a href="#" onclick="ywebShowAbout(); return false;">Über</a> ·
	<span id="yweb-footer-copyright">Copyright © Tuxbox Project</span>
</footer>
`;

// About Modal HTML Template
const YWEB_ABOUT_HTML = `
<div class="yweb-modal-backdrop" id="yweb-about-modal" onclick="ywebCloseAbout(event)">
	<div class="yweb-modal" onclick="event.stopPropagation()">
		<div class="yweb-modal-header">
			<h3><img src="${YWEB_CONFIG.logo}" alt="" onerror="this.remove()"> ${YWEB_CONFIG.title}</h3>
			${_ywebCloseBtn('ywebCloseAbout()')}
		</div>
		<div class="yweb-modal-body">
			<div class="yweb-about-section">
				<h4>Versionen</h4>
				<div class="yweb-about-grid">
					<span class="yweb-about-label">yWeb</span>
					<span class="yweb-about-value" id="about-yweb">--</span>
					<span class="yweb-about-label">Neutrino</span>
					<span class="yweb-about-value" id="about-neutrino">--</span>
					<span class="yweb-about-label">API</span>
					<span class="yweb-about-value" id="about-api">--</span>
				</div>
			</div>
			<div class="yweb-about-section">
				<h4>System</h4>
				<div class="yweb-about-grid">
					<span class="yweb-about-label">Box</span>
					<span class="yweb-about-value" id="about-box">--</span>
					<span class="yweb-about-label">Architektur</span>
					<span class="yweb-about-value" id="about-arch">--</span>
					<span class="yweb-about-label">Betriebssystem</span>
					<span class="yweb-about-value" id="about-os">--</span>
				</div>
			</div>
			<div class="yweb-about-section">
				<h4>Links</h4>
				<div class="yweb-about-links" id="about-links">
					<span class="yweb-about-label">Laden...</span>
				</div>
			</div>
			<div class="yweb-about-section">
				<h4>Lizenz &amp; Copyright</h4>
				<div class="yweb-about-legal">
					<p><strong>yWeb</strong> - Neutrino Web Interface<br>
					<span id="about-copyright">Copyright © Tuxbox Project</span></p>
					<p>Lizenziert unter der <a href="https://www.gnu.org/licenses/old-licenses/gpl-2.0.html" target="_blank">GNU GPL v2</a></p>
					<p><strong>Icons:</strong> Teile der Icons basieren auf <a href="https://www.famfamfam.com/lab/icons/silk/" target="_blank">Silk Icons</a> von famfamfam, lizenziert unter <a href="https://creativecommons.org/licenses/by/2.5/" target="_blank">CC BY 2.5</a></p>
					<p><strong>CSS Framework:</strong> <a href="https://picocss.com" target="_blank">Pico.css</a> (MIT License)</p>
				</div>
			</div>
		</div>
	</div>
</div>
`;

// Generate header HTML with optional active page
function ywebGetHeaderHTML(activePage) {
	const nav = YWEB_CONFIG.navigation || [
		{ page: 'dashboard', href: '/dashboard.html', icon: 'dashboard', label: ywebL('nav.dashboard', 'Dashboard') },
		{ page: 'bouquets', href: '/bouquets.html', icon: 'bouquets', label: ywebL('nav.bouquets', 'Bouquets') },
		{ page: 'epg', href: '/epg.html', icon: 'epg', label: ywebL('nav.epg', 'EPG') },
		{ page: 'timer', href: '/timer.html', icon: 'timer', label: ywebL('nav.timer', 'Timer') },
		{ page: 'settings', href: '/settings.html', icon: 'settings', label: ywebL('nav.settings', 'Settings') },
		{ page: 'tools', href: '/tools.yhtm', icon: 'tools', label: ywebL('nav.tools', 'Tools') }
	];

	const navItems = nav.map(item => {
		const active = item.page === activePage ? ' class="active"' : '';
		return `<li><a href="${item.href}" data-page="${item.page}"${active}>${YWEB_ICONS[item.icon]}<span>${item.label}</span></a></li>`;
	}).join('\n\t\t\t');

	return `
<header class="yweb-header">
	<nav>
		<a href="/dashboard.html" class="yweb-logo">
			<img src="${YWEB_CONFIG.logo}" alt="" onerror="this.remove()">
			<span>${YWEB_CONFIG.title}</span>
		</a>

		<ul class="yweb-nav-links" id="yweb-nav-menu">
			${navItems}
		</ul>

		<div class="yweb-header-right">
			<span class="yweb-hostname" id="yweb-hostname">--</span>
			<span class="yweb-clock" id="yweb-clock">--:--</span>
			<button class="yweb-theme-toggle" onclick="ywebToggleTheme()" title="Theme wechseln">
				<svg id="yweb-theme-icon-dark" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20.354 15.354A9 9 0 018.646 3.646 9.003 9.003 0 0012 21a9.003 9.003 0 008.354-5.646z"/>
				</svg>
				<svg id="yweb-theme-icon-light" class="yweb-hidden" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z"/>
				</svg>
			</button>
			<button class="yweb-menu-toggle" onclick="ywebToggleMenu()" aria-label="Menü">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h16"/>
				</svg>
			</button>
		</div>
	</nav>
</header>`;
}

// Legacy: static header HTML (for backward compatibility)
const YWEB_HEADER_HTML = ywebGetHeaderHTML();

// Theme Management
function ywebGetTheme() {
	return localStorage.getItem('yweb-theme') || 'dark';
}

function ywebSetTheme(theme) {
	document.documentElement.setAttribute('data-theme', theme);
	localStorage.setItem('yweb-theme', theme);
	ywebUpdateThemeIcon(theme);
}

function ywebToggleTheme() {
	const current = ywebGetTheme();
	ywebSetTheme(current === 'dark' ? 'light' : 'dark');
}

function ywebUpdateThemeIcon(theme) {
	const darkIcon = document.getElementById('yweb-theme-icon-dark');
	const lightIcon = document.getElementById('yweb-theme-icon-light');
	if (darkIcon) darkIcon.classList.toggle('yweb-hidden', theme === 'light');
	if (lightIcon) lightIcon.classList.toggle('yweb-hidden', theme === 'dark');
}

// Mobile Menu
function ywebToggleMenu() {
	const menu = document.getElementById('yweb-nav-menu');
	let backdrop = document.getElementById('yweb-menu-backdrop');
	let menuHeader = document.getElementById('yweb-menu-header');

	// Create backdrop if it doesn't exist
	if (!backdrop) {
		backdrop = document.createElement('div');
		backdrop.id = 'yweb-menu-backdrop';
		backdrop.className = 'yweb-menu-backdrop';
		backdrop.onclick = ywebCloseMenu;
		document.body.appendChild(backdrop);
	}

	// Create menu header with logo and close button
	if (!menuHeader && menu) {
		menuHeader = document.createElement('div');
		menuHeader.id = 'yweb-menu-header';
		menuHeader.className = 'yweb-menu-header';
		menuHeader.innerHTML = `
			<div class="yweb-menu-logo">
				<img src="${YWEB_CONFIG.logo}" alt="" onerror="this.remove()">
				<span>${YWEB_CONFIG.title}</span>
			</div>
			<button class="yweb-menu-close" onclick="ywebCloseMenu()">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"/>
				</svg>
			</button>
		`;
		menu.insertBefore(menuHeader, menu.firstChild);
	}

	if (menu) {
		const isOpen = menu.classList.toggle('open');
		backdrop.classList.toggle('open', isOpen);
		// Prevent body scroll when menu is open
		document.body.style.overflow = isOpen ? 'hidden' : '';
	}
}

function ywebCloseMenu() {
	const menu = document.getElementById('yweb-nav-menu');
	const backdrop = document.getElementById('yweb-menu-backdrop');

	if (menu) menu.classList.remove('open');
	if (backdrop) backdrop.classList.remove('open');
	document.body.style.overflow = '';
}

function ywebOpenMenu() {
	const menu = document.getElementById('yweb-nav-menu');
	let backdrop = document.getElementById('yweb-menu-backdrop');
	let menuHeader = document.getElementById('yweb-menu-header');

	// Create backdrop if needed
	if (!backdrop) {
		backdrop = document.createElement('div');
		backdrop.id = 'yweb-menu-backdrop';
		backdrop.className = 'yweb-menu-backdrop';
		backdrop.onclick = ywebCloseMenu;
		document.body.appendChild(backdrop);
	}

	// Create menu header if needed
	if (!menuHeader && menu) {
		menuHeader = document.createElement('div');
		menuHeader.id = 'yweb-menu-header';
		menuHeader.className = 'yweb-menu-header';
		menuHeader.innerHTML = `
			<div class="yweb-menu-logo">
				<img src="${YWEB_CONFIG.logo}" alt="" onerror="this.remove()">
				<span>${YWEB_CONFIG.title}</span>
			</div>
			<button class="yweb-menu-close" onclick="ywebCloseMenu()">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"/>
				</svg>
			</button>
		`;
		menu.insertBefore(menuHeader, menu.firstChild);
	}

	if (menu && !menu.classList.contains('open')) {
		menu.classList.add('open');
		if (backdrop) backdrop.classList.add('open');
		document.body.style.overflow = 'hidden';
	}
}

// Menu swipe gesture handling
let menuTouchStartX = 0;
let menuTouchStartY = 0;
let menuTouchCurrentX = 0;
let menuIsSwiping = false;
const MENU_SWIPE_THRESHOLD = 50;
const EDGE_SWIPE_ZONE = 30;

function ywebMenuTouchStart(e) {
	const touch = e.touches[0];
	menuTouchStartX = touch.clientX;
	menuTouchStartY = touch.clientY;
	menuTouchCurrentX = touch.clientX;
	menuIsSwiping = false;
}

function ywebMenuTouchMove(e) {
	if (!e.touches[0]) return;
	const touch = e.touches[0];
	menuTouchCurrentX = touch.clientX;
	const deltaX = menuTouchCurrentX - menuTouchStartX;
	const deltaY = Math.abs(touch.clientY - menuTouchStartY);

	// Only track horizontal swipes
	if (deltaY < 50 && Math.abs(deltaX) > 10) {
		menuIsSwiping = true;
	}
}

function ywebMenuTouchEnd(e) {
	if (!menuIsSwiping) return;

	const deltaX = menuTouchCurrentX - menuTouchStartX;
	const menu = document.getElementById('yweb-nav-menu');

	if (menu && menu.classList.contains('open')) {
		// Menu is open - swipe left to close
		if (deltaX < -MENU_SWIPE_THRESHOLD) {
			ywebCloseMenu();
		}
	}

	menuIsSwiping = false;
}

// Edge swipe to open menu
function ywebEdgeTouchStart(e) {
	const touch = e.touches[0];
	menuTouchStartX = touch.clientX;
	menuTouchStartY = touch.clientY;
	menuTouchCurrentX = touch.clientX;
	menuIsSwiping = false;
}

function ywebEdgeTouchMove(e) {
	if (!e.touches[0]) return;
	const touch = e.touches[0];
	menuTouchCurrentX = touch.clientX;
	const deltaY = Math.abs(touch.clientY - menuTouchStartY);

	// Only track horizontal swipes from edge
	if (menuTouchStartX < EDGE_SWIPE_ZONE && deltaY < 50) {
		menuIsSwiping = true;
	}
}

function ywebEdgeTouchEnd(e) {
	if (!menuIsSwiping) return;

	const deltaX = menuTouchCurrentX - menuTouchStartX;
	const menu = document.getElementById('yweb-nav-menu');

	// Swipe right from left edge to open
	if (menuTouchStartX < EDGE_SWIPE_ZONE && deltaX > MENU_SWIPE_THRESHOLD) {
		if (!menu || !menu.classList.contains('open')) {
			ywebOpenMenu();
		}
	}

	menuIsSwiping = false;
}

// Initialize menu swipe gestures
function ywebInitMenuSwipe() {
	if (window.innerWidth > 480) return;

	const menu = document.getElementById('yweb-nav-menu');

	// Swipe on menu to close
	if (menu) {
		menu.addEventListener('touchstart', ywebMenuTouchStart, { passive: true });
		menu.addEventListener('touchmove', ywebMenuTouchMove, { passive: true });
		menu.addEventListener('touchend', ywebMenuTouchEnd, { passive: true });
	}

	// Edge swipe to open - listen on body
	document.body.addEventListener('touchstart', ywebEdgeTouchStart, { passive: true });
	document.body.addEventListener('touchmove', ywebEdgeTouchMove, { passive: true });
	document.body.addEventListener('touchend', ywebEdgeTouchEnd, { passive: true });
}

// Clock
function ywebUpdateClock() {
	const clock = document.getElementById('yweb-clock');
	if (clock) clock.textContent = ywebFormatTime(new Date());
}

// Load hostname
function ywebLoadHostname() {
	const el = document.getElementById('yweb-hostname');
	if (!el) return;

	// Prefer sysinfo (real hostname), fallback to boxinfo (boxname)
	ywebApi('sysinfo')
		.then(data => {
			if (data && data.hostname) {
				el.textContent = data.hostname;
			} else {
				return ywebLoadHostnameFallback(el);
			}
		})
		.catch(() => ywebLoadHostnameFallback(el));
}

function ywebLoadHostnameFallback(el) {
	ywebApi('boxinfo')
		.then(data => {
			el.textContent = (data && data.boxname) ? data.boxname : 'neutrino';
		})
		.catch(() => { el.textContent = 'neutrino'; });
}

// Set active navigation link
function ywebSetActivePage(pageName) {
	const links = document.querySelectorAll('.yweb-nav-links a');
	links.forEach(link => {
		if (link.getAttribute('data-page') === pageName) {
			link.classList.add('active');
		} else {
			link.classList.remove('active');
		}
	});
}

// Initialize header
function ywebInitHeader(pageName) {
	// Insert header at beginning of body
	const headerContainer = document.getElementById('yweb-header-container');
	if (headerContainer) {
		headerContainer.innerHTML = YWEB_HEADER_HTML;
	} else {
		document.body.insertAdjacentHTML('afterbegin', YWEB_HEADER_HTML);
	}

	// Set theme
	ywebSetTheme(ywebGetTheme());

	// Set active page
	if (pageName) {
		ywebSetActivePage(pageName);
	}

	// Start clock
	ywebUpdateClock();
	setInterval(ywebUpdateClock, 10000);

	// Load hostname
	ywebLoadHostname();
	// Erweiterungen in die Slots dieser Seite injizieren (async, still)
	if (typeof ywebInjectExtensions === 'function')
		setTimeout(function() { ywebInjectExtensions(pageName); }, 0);
}

// Update spacer height based on actual header dimensions
function ywebUpdateHeaderSpacer() {
	const wrapper = document.querySelector('.yweb-header-wrapper');
	const spacer = document.querySelector('.yweb-header-spacer');

	if (wrapper && spacer) {
		const height = wrapper.offsetHeight;
		spacer.style.height = height + 'px';
	}
}

// Initialize footer
function ywebInitFooter() {
	// Check if footer already exists
	if (document.querySelector('.yweb-footer')) return;

	// Insert footer at end of body
	document.body.insertAdjacentHTML('beforeend', YWEB_FOOTER_HTML);

	// Insert about modal
	if (!document.getElementById('yweb-about-modal')) {
		document.body.insertAdjacentHTML('beforeend', YWEB_ABOUT_HTML);
	}

	// Load versions from API
	ywebApi('versions')
		.then(data => {
			if (data.yweb) {
				const el = document.getElementById('yweb-footer-version');
				if (el) el.textContent = data.yweb;
			}
			if (data.api) {
				const el = document.getElementById('yweb-footer-api');
				if (el) el.textContent = data.api;
			}
			if (data.copyright && data.copyright.text) {
				const el = document.getElementById('yweb-footer-copyright');
				if (el) el.textContent = data.copyright.text;
			}
		})
		.catch(() => {});
}

// Shared helper functions
function ywebPad2(n) {
	return String(n).padStart(2, '0');
}

function ywebFormatTime(date) {
	return ywebPad2(date.getHours()) + ':' + ywebPad2(date.getMinutes());
}

function ywebFormatDateInput(date) {
	return date.getFullYear() + '-' + ywebPad2(date.getMonth() + 1) + '-' + ywebPad2(date.getDate());
}

// Fetch helpers — simple wrappers for common patterns
async function ywebFetchText(url) {
	var resp = await fetch(url);
	return resp.text();
}

async function ywebFetchJson(url) {
	var resp = await fetch(url);
	return resp.json();
}

// Central /api/v4/ fetch helper — the ONLY way modern pages talk to the box.
//   ywebApi('timer')                            -> parsed JSON payload
//   ywebApi('timer', {action:'new', type:5})    -> envelope-checked action
//   ywebApi('setmode', {status:''})             -> key-only query param
//   ywebApi('cmd', 'ps', {raw:true})            -> raw text (cmd/exec/rcem)
// Throws Error on HTTP failure or {"success":"false"} envelopes; the
// error message carries error.msg from the box when available.
async function ywebApi(endpoint, params, opts) {
	opts = opts || {};
	var url = '/api/v4/' + endpoint;
	if (params) {
		if (typeof params === 'string') {
			url += '?' + params;
		} else {
			var q = [];
			for (var k in params) {
				if (!Object.prototype.hasOwnProperty.call(params, k)) continue;
				var v = params[k];
				q.push(v === '' || v === null || v === undefined
					? encodeURIComponent(k)
					: encodeURIComponent(k) + '=' + encodeURIComponent(v));
			}
			if (q.length) url += '?' + q.join('&');
		}
	}
	var resp = await fetch(url);
	if (!resp.ok) {
		var errText = '';
		try {
			var errData = await resp.json();
			if (errData && errData.error && errData.error.msg) errText = errData.error.msg;
		} catch (ignore) {}
		throw new Error(errText || (endpoint + ': HTTP ' + resp.status));
	}
	if (opts.raw) return resp.text();
	var data = await resp.json();
	if (data && data.success === 'false')
		throw new Error((data.error && data.error.msg) ? data.error.msg : ('Fehler: ' + endpoint));
	return data;
}

async function ywebSendKey(key) {
	if (key === 'KEY_POWER') {
		try {
			var st = await ywebApi('standby', {status: ''});
			if (st && st.standby === false && !confirm('Box in Standby versetzen?')) return;
		} catch (e) { /* ignore */ }
	}
	return ywebApi('rcem', key, {raw: true});
}

// ═══ Remote Control Component ═══

/**
 * Returns generic remote control HTML.
 * @param {Object} [opts]
 * @param {string} [opts.onKey='ywebSendKey'] - Callback function name for key presses
 * @param {string} [opts.id='yweb-remote'] - Container element ID
 *
 * Custom renderer interface:
 *   Set YWEB_CONFIG.remoteRenderer = function(opts) { return '<html>'; }
 *   to replace the generic remote with hardware-specific graphics (image maps).
 *   The renderer receives the same opts object and should call opts.onKey for buttons.
 */
function ywebGetRemoteHTML(opts) {
	opts = opts || {};
	var onKey = opts.onKey || 'ywebSendKey';
	var id = opts.id || 'yweb-remote';

	if (YWEB_CONFIG.remoteRenderer) {
		return YWEB_CONFIG.remoteRenderer(opts);
	}

	function btn(cls, key, label, extra) {
		return '<button class="yweb-rc-btn ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + (label || key) + '"' + (extra || '') + '>' + label + '</button>';
	}
	function iconBtn(cls, key, title, svg, extra) {
		return '<button class="yweb-rc-btn ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + title + '"' + (extra || '') + '>' + svg + '</button>';
	}
	function repBtn(cls, key, label) {
		return '<button class="yweb-rc-btn ' + cls + '" title="' + (label || key) + '" data-rc-repeat="1" data-rc-key="' + key + '">' + label + '</button>';
	}

	var svgUp = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="18 15 12 9 6 15"/></svg>';
	var svgDown = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="6 9 12 15 18 9"/></svg>';
	var svgLeft = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="15 18 9 12 15 6"/></svg>';
	var svgRight = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="9 18 15 12 9 6"/></svg>';
	var svgPower = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18.36 6.64a9 9 0 11-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/></svg>';
	var svgMute = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><line x1="23" y1="9" x2="17" y2="15"/><line x1="17" y1="9" x2="23" y2="15"/></svg>';

	return '<div class="yweb-remote" id="' + id + '" data-rc-onkey="' + onKey + '">' +

		/* Zone A: Power / Mute */
		'<div class="yweb-rc-row yweb-rc-top">' +
			iconBtn('yweb-rc-power', 'KEY_POWER', 'Power', svgPower) +
			iconBtn('', 'KEY_MUTE', 'Mute', svgMute) +
		'</div>' +

		/* Zone B: Core thumb zone (VOL | D-Pad | CH) */
		'<div class="yweb-rc-core">' +
			'<div class="yweb-rc-rocker">' +
				'<span class="yweb-rc-rocker-label">VOL</span>' +
				repBtn('yweb-rc-rocker-top', 'KEY_VOLUMEUP', '+') +
				repBtn('yweb-rc-rocker-bot', 'KEY_VOLUMEDOWN', '\u2212') +
			'</div>' +
			'<div class="yweb-rc-dpad">' +
				'<div></div>' + iconBtn('yweb-rc-arrow', 'KEY_UP', 'Up', svgUp) + '<div></div>' +
				iconBtn('yweb-rc-arrow', 'KEY_LEFT', 'Left', svgLeft) +
				btn('yweb-rc-ok', 'KEY_OK', 'OK') +
				iconBtn('yweb-rc-arrow', 'KEY_RIGHT', 'Right', svgRight) +
				'<div></div>' + iconBtn('yweb-rc-arrow', 'KEY_DOWN', 'Down', svgDown) + '<div></div>' +
			'</div>' +
			'<div class="yweb-rc-rocker">' +
				'<span class="yweb-rc-rocker-label">CH</span>' +
				repBtn('yweb-rc-rocker-top', 'KEY_PAGEUP', '+') +
				repBtn('yweb-rc-rocker-bot', 'KEY_PAGEDOWN', '\u2212') +
			'</div>' +
		'</div>' +

		/* Zone C: Navigation */
		'<div class="yweb-rc-row yweb-rc-nav">' +
			btn('yweb-rc-func', 'KEY_EXIT', 'Back') +
			btn('yweb-rc-func', 'KEY_HOME', 'Home') +
			btn('yweb-rc-func', 'KEY_SETUP', 'Menu') +
			btn('yweb-rc-func', 'KEY_HELP', 'Info') +
		'</div>' +

		/* Zone D: Media */
		'<div class="yweb-rc-row yweb-rc-media">' +
			iconBtn('', 'KEY_REWIND', 'Rewind', '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="19 20 9 12 19 4"/><polygon points="14 20 4 12 14 4"/></svg>') +
			iconBtn('', 'KEY_PLAY', 'Play', '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21"/></svg>') +
			iconBtn('', 'KEY_FORWARD', 'Forward', '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 4 15 12 5 20"/><polygon points="10 4 20 12 10 20"/></svg>') +
			iconBtn('', 'KEY_STOP', 'Stop', '<svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="6" width="12" height="12"/></svg>') +
		'</div>' +

		/* Sheet trigger */
		'<button class="yweb-rc-btn yweb-rc-sheet-trigger" onclick="ywebToggleRcSheet(\'' + id + '\')">123</button>' +

		/* Bottom sheet */
		'<div class="yweb-rc-sheet-backdrop" id="' + id + '-sheet" onclick="ywebToggleRcSheet(\'' + id + '\')">' +
			'<div class="yweb-rc-sheet" onclick="event.stopPropagation()">' +
				'<div class="yweb-rc-sheet-handle"></div>' +
				'<div class="yweb-rc-row yweb-rc-colors">' +
					'<button class="yweb-rc-color yweb-rc-red" onclick="' + onKey + "('KEY_RED')\" title=\"Rot\"></button>" +
					'<button class="yweb-rc-color yweb-rc-green" onclick="' + onKey + "('KEY_GREEN')\" title=\"Gruen\"></button>" +
					'<button class="yweb-rc-color yweb-rc-yellow" onclick="' + onKey + "('KEY_YELLOW')\" title=\"Gelb\"></button>" +
					'<button class="yweb-rc-color yweb-rc-blue" onclick="' + onKey + "('KEY_BLUE')\" title=\"Blau\"></button>" +
				'</div>' +
				'<div class="yweb-rc-row yweb-rc-nav">' +
					btn('yweb-rc-func', 'KEY_EPG', 'EPG') +
					btn('yweb-rc-func', 'KEY_TEXT', 'Text') +
				'</div>' +
				'<div class="yweb-rc-numpad">' +
					btn('', 'KEY_1', '1') + btn('', 'KEY_2', '2') + btn('', 'KEY_3', '3') +
					btn('', 'KEY_4', '4') + btn('', 'KEY_5', '5') + btn('', 'KEY_6', '6') +
					btn('', 'KEY_7', '7') + btn('', 'KEY_8', '8') + btn('', 'KEY_9', '9') +
					'<div></div>' + btn('', 'KEY_0', '0') + '<div></div>' +
				'</div>' +
			'</div>' +
		'</div>' +

	'</div>';
}

/* Bottom sheet toggle (mobile only, desktop shows inline via CSS) */
function ywebToggleRcSheet(id) {
	if (window.innerWidth > 768) return;
	var backdrop = document.getElementById(id + '-sheet');
	if (!backdrop) return;

	var isOpen = backdrop.style.display === 'flex';
	if (isOpen) {
		backdrop.style.display = 'none';
		if (backdrop._escHandler) {
			document.removeEventListener('keydown', backdrop._escHandler);
			backdrop._escHandler = null;
		}
	} else {
		backdrop.style.display = 'flex';
		backdrop._escHandler = function(e) {
			if (e.key === 'Escape') ywebToggleRcSheet(id);
		};
		document.addEventListener('keydown', backdrop._escHandler);
		/* Init swipe-to-dismiss on handle */
		if (!backdrop._swipeInit) {
			backdrop._swipeInit = true;
			var panel = backdrop.querySelector('.yweb-rc-sheet');
			var handle = backdrop.querySelector('.yweb-rc-sheet-handle');
			if (panel && handle) {
				var startY = 0, dragging = false;
				handle.addEventListener('touchstart', function(e) {
					startY = e.touches[0].clientY;
					dragging = true;
				}, { passive: true });
				backdrop.addEventListener('touchmove', function(e) {
					if (!dragging) return;
					var diff = e.touches[0].clientY - startY;
					if (diff > 0) {
						e.preventDefault();
						panel.style.transform = 'translateY(' + diff + 'px)';
					}
				}, { passive: false });
				backdrop.addEventListener('touchend', function(e) {
					if (!dragging) return;
					dragging = false;
					var diff = (e.changedTouches[0] ? e.changedTouches[0].clientY : 0) - startY;
					panel.style.transform = '';
					if (diff > 80) ywebToggleRcSheet(id);
				});
			}
		}
	}
}

/* Long-press auto-repeat for VOL/CH buttons */
function ywebInitRemoteRepeat(containerId) {
	var container = document.getElementById(containerId);
	if (!container) return;
	var onKeyName = container.getAttribute('data-rc-onkey');
	if (!onKeyName) return;
	var fn = window[onKeyName];
	if (typeof fn !== 'function') return;

	var btns = container.querySelectorAll('[data-rc-repeat]');
	btns.forEach(function(btn) {
		var key = btn.getAttribute('data-rc-key');
		var timer = null;
		var interval = null;

		function stop() {
			clearTimeout(timer);
			clearInterval(interval);
			timer = null;
			interval = null;
		}

		btn.addEventListener('pointerdown', function(e) {
			e.preventDefault();
			fn(key);
			timer = setTimeout(function() {
				interval = setInterval(function() { fn(key); }, 120);
			}, 350);
		});

		btn.addEventListener('pointerup', stop);
		btn.addEventListener('pointercancel', stop);
		btn.addEventListener('pointerleave', stop);
	});
}

// Remote Modal
var _ywebRemoteModalOnKey = null;

function ywebShowRemote(opts) {
	opts = opts || {};
	var onKey = opts.onKey || 'ywebSendKey';
	var modal = document.getElementById('yweb-remote-modal');

	// Rebuild modal if onKey changed
	if (modal && _ywebRemoteModalOnKey !== onKey) {
		modal.remove();
		modal = null;
	}

	if (!modal) {
		_ywebRemoteModalOnKey = onKey;
		var html = '<div class="yweb-remote-backdrop" id="yweb-remote-modal" onclick="ywebCloseRemote(event)">' +
			'<div class="yweb-remote-dialog" onclick="event.stopPropagation()">' +
				'<div class="yweb-remote-modal-header">' +
					'<span>' + YWEB_ICONS.remote + ' Fernbedienung</span>' +
					_ywebCloseBtn('ywebCloseRemote()') +
				'</div>' +
				ywebGetRemoteHTML({ id: 'yweb-remote-modal-panel', onKey: onKey }) +
			'</div>' +
		'</div>';
		document.body.insertAdjacentHTML('beforeend', html);
		modal = document.getElementById('yweb-remote-modal');
		ywebInitRemoteRepeat('yweb-remote-modal-panel');
	}
	modal.style.display = 'flex';
	document.body.classList.add('yweb-remote-open');
}

function ywebCloseRemote(event) {
	if (event && event.target !== event.currentTarget) return;
	var modal = document.getElementById('yweb-remote-modal');
	if (modal) {
		modal.style.display = 'none';
		var sheet = modal.querySelector('.yweb-rc-sheet-backdrop');
		if (sheet) sheet.style.display = 'none';
	}
	document.body.classList.remove('yweb-remote-open');
}

// Also escapes quotes so the result is safe in attribute values.
function escapeHtml(text) {
	if (!text) return '';
	const div = document.createElement('div');
	div.textContent = text;
	return div.innerHTML.replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function escapeJs(text) {
	if (!text) return '';
	return text.replace(/'/g, "\\'").replace(/"/g, '\\"');
}

// Generic modal on the EPG-modal backdrop pattern. Reuses the element
// per id and replaces title/body on every call (reset on reopen).
function ywebModal(id, title, bodyHtml) {
	var backdrop = document.getElementById(id);
	if (!backdrop) {
		backdrop = document.createElement('div');
		backdrop.id = id;
		backdrop.className = 'yweb-epg-modal-backdrop';
		backdrop.innerHTML = '<div class="yweb-epg-modal" onclick="event.stopPropagation()">' +
			'<div class="yweb-epg-modal-header"><h3></h3>' +
			_ywebCloseBtn('ywebCloseModal(\'' + id + '\')') + '</div>' +
			'<div class="yweb-epg-modal-body"></div></div>';
		backdrop.addEventListener('click', function(e) {
			if (e.target === backdrop) ywebCloseModal(id);
		});
		document.body.appendChild(backdrop);
	}
	backdrop.querySelector('.yweb-epg-modal-header h3').textContent = title;
	backdrop.querySelector('.yweb-epg-modal-body').innerHTML = bodyHtml;
	backdrop.classList.add('open');
	document.body.style.overflow = 'hidden';
	return backdrop;
}

function ywebCloseModal(id) {
	var backdrop = document.getElementById(id);
	if (backdrop) backdrop.classList.remove('open');
	document.body.style.overflow = '';
}

// EPG Info Modal HTML Template
const YWEB_EPG_MODAL_HTML = `
<div class="yweb-epg-modal-backdrop" id="ywebEpgModal" onclick="ywebCloseEpgModal(event)">
	<div class="yweb-epg-modal" onclick="event.stopPropagation()">
		<div class="yweb-epg-modal-header">
			<h3 id="ywebEpgModalChannel">--</h3>
			${_ywebCloseBtn('ywebCloseEpgModal()')}
		</div>
		<div class="yweb-epg-modal-body">
			<div class="yweb-epg-current">
				<div class="yweb-epg-title" id="ywebEpgCurrentTitle">Lade...</div>
				<div class="yweb-epg-time" id="ywebEpgCurrentTime">--:-- - --:--</div>
				<div class="yweb-epg-progress">
					<div class="yweb-epg-progress-bar" id="ywebEpgProgressBar" style="width: 0%"></div>
				</div>
				<div class="yweb-epg-description" id="ywebEpgCurrentDesc"></div>
			</div>
			<div class="yweb-epg-next" id="ywebEpgNextSection" style="display: none;">
				<div class="yweb-epg-next-label">Danach</div>
				<div class="yweb-epg-next-title" id="ywebEpgNextTitle">--</div>
				<div class="yweb-epg-next-time" id="ywebEpgNextTime">--:-- - --:--</div>
			</div>
		</div>
		<div class="yweb-epg-modal-footer">
			<button class="yweb-btn" onclick="ywebCloseEpgModal()">Schließen</button>
			<button class="yweb-btn" id="ywebEpgRecBtn" onclick="ywebEpgModalToggleRec()" style="color:var(--yweb-danger,#ef4444)">
				<svg viewBox="0 0 24 24" fill="currentColor" style="width:1rem;height:1rem;margin-right:0.3rem">
					<circle cx="12" cy="12" r="8"/>
				</svg>
				<span>Aufnehmen</span>
			</button>
			<button class="yweb-btn primary" onclick="ywebStreamChannel()">
				<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width:1rem;height:1rem;margin-right:0.3rem">
					<path d="M5 17H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2h-1"/>
					<polygon points="12,15 17,21 7,21"/>
				</svg>
				Stream
			</button>
		</div>
	</div>
</div>
`;

// Show EPG Info Modal
async function ywebShowEpgModal(channelId, channelName) {
	// Ensure modal exists
	if (!document.getElementById('ywebEpgModal')) {
		document.body.insertAdjacentHTML('beforeend', YWEB_EPG_MODAL_HTML);
	}

	const modal = document.getElementById('ywebEpgModal');
	const titleEl = document.getElementById('ywebEpgCurrentTitle');
	const timeEl = document.getElementById('ywebEpgCurrentTime');
	const descEl = document.getElementById('ywebEpgCurrentDesc');
	const progressBar = document.getElementById('ywebEpgProgressBar');
	const channelEl = document.getElementById('ywebEpgModalChannel');
	const nextSection = document.getElementById('ywebEpgNextSection');
	const nextTitleEl = document.getElementById('ywebEpgNextTitle');
	const nextTimeEl = document.getElementById('ywebEpgNextTime');

	// Store channel info for action buttons
	ywebEpgModalChannelId = channelId;
	ywebEpgModalChannelName = channelName || 'Sendung';
	ywebEpgModalStartSec = 0;
	ywebEpgModalStopSec = 0;

	// Set channel name
	channelEl.textContent = ywebEpgModalChannelName;

	// Reset modal content
	titleEl.textContent = 'Lade...';
	timeEl.textContent = '--:-- - --:--';
	descEl.textContent = '';
	progressBar.style.width = '0%';
	nextSection.style.display = 'none';

	// Open modal
	modal.classList.add('open');
	document.body.style.overflow = 'hidden';

	try {
		const now = Math.floor(Date.now() / 1000);
		const data = await ywebApi('epg', {channelid: channelId, details: 'true', max: 2, starttime: now});
		const progData = data?.data?.epglist?.progData || data?.epglist?.progData;

		if (!progData || progData.length === 0) {
			titleEl.textContent = 'Keine EPG-Daten verfügbar';
			return;
		}

		// Current program
		const current = progData[0];
		const title = current.description || 'Unbekannt';
		const info1 = current.info1 || '';
		const info2 = current.info2 || '';
		const startT = current.start_t || '';
		const stopT = current.stop_t || '';
		const startSec = parseInt(current.start_sec || '0');
		const stopSec = parseInt(current.stop_sec || '0');

		ywebEpgModalStartSec = startSec;
		ywebEpgModalStopSec = stopSec;

		titleEl.textContent = title;
		timeEl.textContent = startT + ' - ' + stopT;

		// Description (combine info1 and info2)
		let desc = info1;
		if (info2) desc += (desc ? '\n\n' : '') + info2;
		descEl.textContent = desc;

		// Progress bar
		if (startSec && stopSec) {
			const nowTs = Math.floor(Date.now() / 1000);
			const duration = stopSec - startSec;
			const elapsed = nowTs - startSec;
			const progress = Math.min(100, Math.max(0, (elapsed / duration) * 100));
			progressBar.style.width = progress + '%';
		}

		// Next program
		if (progData.length > 1) {
			const next = progData[1];
			const nextTitle = next.description || 'Unbekannt';
			const nextStartT = next.start_t || '';
			const nextStopT = next.stop_t || '';

			nextTitleEl.textContent = nextTitle;
			nextTimeEl.textContent = nextStartT + ' - ' + nextStopT;
			nextSection.style.display = 'block';
		}
	} catch (e) {
		titleEl.textContent = 'Fehler beim Laden';
		console.error('Failed to load EPG:', e);
	}

	// Check if this channel is currently being recorded
	ywebEpgModalUpdateRecBtn(false);
	ywebCheckRecordingForChannel(channelId).then(function(isRec) {
		ywebEpgModalUpdateRecBtn(isRec);
	});
}

// Current channel/EPG data for modal buttons
let ywebEpgModalChannelId = null;
let ywebEpgModalChannelName = null;
let ywebEpgModalStartSec = 0;
let ywebEpgModalStopSec = 0;

// Close EPG Modal
function ywebCloseEpgModal(event) {
	if (event && event.target !== event.currentTarget) return;
	const modal = document.getElementById('ywebEpgModal');
	if (modal) modal.classList.remove('open');
	document.body.style.overflow = '';
}

// Stream channel - can be called with params (from channel list) or without (from EPG modal, uses globals)
function ywebStreamChannel(channelId, channelName) {
	channelId = channelId || ywebEpgModalChannelId;
	channelName = channelName || ywebEpgModalChannelName;
	if (!channelId) return;
	const plist = "#EXTM3U\n#EXTINF:-1," + channelName + "\nhttp://" + window.location.hostname + ":31339/id=" + channelId + "\n";
	const blob = new Blob([plist], { type: "application/octet-stream" });
	const a = document.createElement('a');
	a.href = URL.createObjectURL(blob);
	a.download = "livestream.m3u";
	a.click();
	URL.revokeObjectURL(a.href);
}

// Record current EPG event - can be called with params or without (uses modal globals)
async function ywebRecordEvent(channelId, channelName, startSec, stopSec) {
	channelId = channelId || ywebEpgModalChannelId;
	channelName = channelName || ywebEpgModalChannelName;
	startSec = startSec || ywebEpgModalStartSec;
	stopSec = stopSec || ywebEpgModalStopSec;
	if (!channelId) return;

	try {
		// If no EPG times provided, fetch current EPG
		if (!startSec || !stopSec) {
			const now = Math.floor(Date.now() / 1000);
			const epg = await ywebApi('epg', {channelid: channelId, max: 1, starttime: now});
			const prog = epg?.data?.epglist?.progData?.[0] || epg?.epglist?.progData?.[0];
			if (prog) {
				startSec = parseInt(prog.start_sec || '0');
				stopSec = parseInt(prog.stop_sec || '0');
			}
		}

		if (!startSec || !stopSec) {
			ywebShowToast('Keine EPG-Daten verfügbar', true);
			return;
		}

		await ywebApi('timer', {action: 'new', type: 5, alarm: startSec, stop: stopSec, channel_id: channelId});
		ywebShowToast('Aufnahme geplant: ' + (channelName || ''));
	} catch (e) {
		console.error('Record failed:', e);
		ywebShowToast('Fehler: ' + e.message, true);
	}
}

// Stop a recording by timer ID, or find and stop the running one
async function ywebStopRecording(timerId) {
	try {
		if (timerId) {
			await ywebApi('timer', {action: 'remove', id: timerId});
			ywebShowToast('Aufnahme gestoppt');
			return;
		}
		// No ID given: find running recording timer and remove it
		var tData = await ywebApi('timer');
		var timers = ywebParseTimerList(tData);
		var running = timers.filter(function(t) {
			return parseInt(t.state) === 2 && (parseInt(t.type_number) === 5 || parseInt(t.type_number) === 9);
		});
		if (running.length > 0) {
			var id = running[0].id || running[0].eventID;
			if (id) {
				await ywebApi('timer', {action: 'remove', id: id});
				ywebShowToast('Aufnahme gestoppt');
				return;
			}
		}
		// Fallback: try setmode stop (for immediate recordings without timer)
		await ywebApi('setmode', {record: 'stop'});
		// Clean up stale timer entries left by setmode stop
		try {
			var cData = await ywebApi('timer');
			var cTimers = ywebParseTimerList(cData);
			var stale = cTimers.filter(function(t) {
				return parseInt(t.state) === 2 && (parseInt(t.type_number) === 5 || parseInt(t.type_number) === 9);
			});
			for (var i = 0; i < stale.length; i++) {
				var sid = stale[i].id || stale[i].eventID;
				if (sid) await ywebApi('timer', {action: 'remove', id: sid});
			}
		} catch (ignore) {}
		ywebShowToast('Aufnahme gestoppt');
	} catch (e) {
		ywebShowToast('Fehler beim Stoppen: ' + e.message, true);
	}
}

// EPG Modal: toggle between Record and Stop
let ywebEpgModalIsRecording = false;

async function ywebEpgModalToggleRec() {
	if (ywebEpgModalIsRecording) {
		await ywebStopRecording();
		ywebEpgModalUpdateRecBtn(false);
	} else {
		await ywebRecordEvent();
	}
}

function ywebEpgModalUpdateRecBtn(isRecording) {
	ywebEpgModalIsRecording = isRecording;
	const btn = document.getElementById('ywebEpgRecBtn');
	if (!btn) return;
	const label = btn.querySelector('span');
	if (label) label.textContent = isRecording ? 'Aufnahme stoppen' : 'Aufnehmen';
	btn.style.animation = isRecording ? 'yweb-pulse 1.5s infinite' : '';
}

// Check if current channel is being recorded
async function ywebCheckRecordingForChannel(channelId) {
	try {
		const data = await ywebApi('setmode', {status: ''});
		if (!data || !data.recording) return false;
		const tData = await ywebApi('timer');
		const timers = ywebParseTimerList(tData);
		return timers.some(function(t) {
			return (parseInt(t.state) === 2) && (parseInt(t.type_number) === 5 || parseInt(t.type_number) === 9)
				&& t.channel_id && t.channel_id === channelId;
		});
	} catch (e) { return false; }
}

// Timer constants — shared across all pages
const YWEB_TIMER_TYPES = {
	1: 'Shutdown', 2: 'Nextprogram', 3: 'Zapto', 4: 'Standby',
	5: 'Record', 6: 'Remind', 7: 'Sleeptimer', 8: 'Plugin', 9: 'Immediate Record'
};

const YWEB_TIMER_TYPE_LABELS = {
	1: 'Shutdown', 3: 'Umschalten', 4: 'Standby',
	5: 'Aufnahme', 6: 'Erinnerung', 7: 'Sleeptimer', 8: 'Plugin', 9: 'Sofortaufnahme'
};

const YWEB_TIMER_STATE_LABELS = {
	0: 'Geplant', 1: 'Angekündigt', 2: 'Aktiv', 3: 'Beendet', 4: 'Abgebrochen'
};

const YWEB_REPEAT_LABELS = {
	'once': 'Einmalig', 'daily': 'Täglich', 'weekly': 'Wöchentlich',
	'biweekly': 'Zweiwöchentlich', 'fourweekly': 'Vierwöchentlich',
	'monthly': 'Monatlich', 'byeventdescription': 'Nach Beschreibung'
};

// Timer helper functions — shared across all pages
function ywebGetTimerType(timer) {
	return YWEB_TIMER_TYPES[parseInt(timer.type_number) || 0] || 'Timer';
}

function ywebGetTimerAlarm(timer) {
	if (Array.isArray(timer.alarm) && timer.alarm.length > 0)
		return timer.alarm[0];
	return timer.alarm && timer.alarm.normal ? timer.alarm.normal : {};
}

function ywebGetTimerStop(timer) {
	if (Array.isArray(timer.stop) && timer.stop.length > 0)
		return timer.stop[0];
	return timer.stop || {};
}

function ywebGetTimerStopSec(timer) {
	var stop = ywebGetTimerStop(timer);
	return parseInt(stop.digits) || 0;
}

function ywebResolveRepeatText(raw) {
	if (!raw) return 'Einmalig';
	var m = raw.match(/\{=L:timerlist\.repeat\.(\w+)=\}/);
	if (m) return YWEB_REPEAT_LABELS[m[1]] || m[1];
	return YWEB_REPEAT_LABELS[raw.toLowerCase()] || raw;
}

function ywebGetTimerIconHtml(type, cssPrefix) {
	var cls = cssPrefix || 'yweb-timer-type-icon';
	if (type === 'Record' || type === 'Immediate Record')
		return '<div class="' + cls + ' record"><svg viewBox="0 0 24 24" fill="currentColor"><circle cx="12" cy="12" r="8"/></svg></div>';
	if (type === 'Zapto')
		return '<div class="' + cls + '"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5,3 19,12 5,21"/></svg></div>';
	if (type === 'Standby' || type === 'Shutdown')
		return '<div class="' + cls + '"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18.364 5.636a9 9 0 11-12.728 12.728 9 9 0 0112.728-12.728zM12 3v4"/></svg></div>';
	if (type === 'Remind')
		return '<div class="' + cls + '"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M15 17h5l-1.405-1.405A2.032 2.032 0 0118 14.158V11a6 6 0 00-4-5.659V5a2 2 0 10-4 0v.341C7.67 6.165 6 8.388 6 11v3.159c0 .538-.214 1.055-.595 1.436L4 17h5m6 0v1a3 3 0 11-6 0v-1m6 0H9"/></svg></div>';
	return '<div class="' + cls + '"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/></svg></div>';
}

// Parse timer list from API JSON response — shared across all pages
// Handles: {data: {timer: {timer: [{timer_list: [...]}]}}}, {timer: [...]}, etc.
function ywebParseTimerList(data) {
	var root = data.data || data;
	var timerRoot = root.timer ? (root.timer.timer || root.timer) : null;
	if (!timerRoot) return [];
	if (Array.isArray(timerRoot) && timerRoot.length > 0 && timerRoot[0].timer_list)
		return timerRoot[0].timer_list;
	if (Array.isArray(timerRoot))
		return timerRoot;
	if (timerRoot.timer_list)
		return Array.isArray(timerRoot.timer_list) ? timerRoot.timer_list : [];
	return [];
}

// Fetch bouquets — cached, shared across all pages
let _ywebBouquetCache = null;
async function ywebFetchBouquets() {
	if (_ywebBouquetCache) return _ywebBouquetCache;
	const data = await ywebApi('bouquets');
	const root = data.data || data;
	_ywebBouquetCache = root.bouquets || [];
	return _ywebBouquetCache;
}

// Fetch channels for a bouquet — cached per bouquet number
let _ywebChannelCache = {};
async function ywebFetchChannels(bouquetNr) {
	if (_ywebChannelCache[bouquetNr]) return _ywebChannelCache[bouquetNr];
	const data = await ywebApi('bouquet', {bouquet: bouquetNr});
	const root = data.data || data;
	_ywebChannelCache[bouquetNr] = root.channels || [];
	return _ywebChannelCache[bouquetNr];
}

// Confirm dialog — shared across all pages
// Returns a Promise that resolves when confirmed, rejects when cancelled
// onConfirm: async function(btn) — called when user clicks confirm, btn shows loading state
function ywebShowConfirm(title, message, confirmLabel, onConfirm) {
	var backdrop = document.createElement('div');
	backdrop.className = 'yweb-confirm-backdrop';
	backdrop.onclick = function(e) {
		if (e.target === backdrop) backdrop.remove();
	};
	backdrop.innerHTML =
		'<div class="yweb-confirm-dialog">' +
			'<h4>' + title + '</h4>' +
			'<p>' + message + '</p>' +
			'<div class="yweb-confirm-actions">' +
				'<button class="confirm-cancel">Abbrechen</button>' +
				'<button class="confirm-delete">' + confirmLabel + '</button>' +
			'</div>' +
		'</div>';
	backdrop.querySelector('.confirm-cancel').onclick = function() { backdrop.remove(); };
	backdrop.querySelector('.confirm-delete').onclick = async function() {
		var btn = this;
		btn.textContent = '...';
		btn.disabled = true;
		try {
			await onConfirm(btn);
			backdrop.remove();
		} catch (e) {
			console.error('Confirm action failed:', e);
			btn.textContent = 'Fehler';
			setTimeout(function() { backdrop.remove(); }, 1500);
		}
	};
	document.body.appendChild(backdrop);
}

// Message popover — send ticker or popup messages to the box
function ywebShowMessageDialog() {
	var pop = document.getElementById('yweb-message-pop');
	if (pop) { pop.classList.toggle('open'); return; }

	pop = document.createElement('div');
	pop.id = 'yweb-message-pop';
	pop.className = 'yweb-popover open';
	pop.innerHTML =
		'<button type="button" class="yweb-popover-close" onclick="document.getElementById(\'yweb-message-pop\').classList.remove(\'open\')" aria-label="Close">\u00d7</button>' +
		'<form onsubmit="return false">' +
			'<input type="text" id="yweb-msg-text" placeholder="Nachricht eingeben\u2026" maxlength="255">' +
			'<div class="yweb-popover-actions">' +
				'<small id="yweb-msg-result">\u00a0</small>' +
				'<button type="button" class="outline" onclick="ywebSendMessage(\'nmsg\')">Nachricht</button>' +
				'<button type="button" onclick="ywebSendMessage(\'popup\')">Popup</button>' +
			'</div>' +
		'</form>';
	document.body.appendChild(pop);

	var input = pop.querySelector('#yweb-msg-text');
	input.onkeydown = function(e) { if (e.key === 'Enter') ywebSendMessage('nmsg'); };
	input.focus();

	// Close on outside click
	setTimeout(function() {
		document.addEventListener('click', function close(e) {
			if (!pop.contains(e.target) && !e.target.closest('[onclick*="ywebShowMessageDialog"]')) {
				pop.classList.remove('open');
				document.removeEventListener('click', close);
			}
		});
	}, 10);
}

async function ywebSendMessage(mode) {
	var input = document.getElementById('yweb-msg-text');
	var text = input.value.trim();
	if (!text) return;

	var result = document.getElementById('yweb-msg-result');
	try {
		var params = {};
		params[mode] = text;
		await ywebApi('message', params);
		result.textContent = 'Gesendet';
		result.style.color = 'var(--yweb-success)';
		input.value = '';
		setTimeout(function() { result.textContent = '\u00a0'; result.style.color = ''; }, 2500);
	} catch (e) {
		result.textContent = 'Fehler: ' + e.message;
		result.style.color = 'var(--yweb-danger)';
	}
}

// Format helpers (shared across pages)
function ywebFormatKB(kb) {
	if (kb < 1024) return kb + ' KB';
	if (kb < 1048576) return (kb / 1024).toFixed(1) + ' MB';
	return (kb / 1048576).toFixed(1) + ' GB';
}

function ywebFormatUptime(seconds) {
	var days = Math.floor(seconds / 86400);
	var hours = Math.floor((seconds % 86400) / 3600);
	var mins = Math.floor((seconds % 3600) / 60);
	if (days > 0) return days + 'd ' + hours + 'h';
	if (hours > 0) return hours + 'h ' + mins + 'm';
	return mins + 'm';
}

// Toast notification
function ywebShowToast(message, isError) {
	// Remove existing toast
	const existing = document.getElementById('yweb-toast');
	if (existing) existing.remove();

	const toast = document.createElement('div');
	toast.id = 'yweb-toast';
	toast.textContent = message;
	toast.style.cssText = 'position:fixed;bottom:1.5rem;left:50%;transform:translateX(-50%);' +
		'padding:0.6rem 1.2rem;border-radius:0.4rem;font-size:0.85rem;z-index:10000;' +
		'color:#fff;box-shadow:0 4px 12px rgba(0,0,0,0.3);transition:opacity 0.3s;' +
		'background:' + (isError ? '#ef4444' : '#22c55e');
	document.body.appendChild(toast);
	setTimeout(() => { toast.style.opacity = '0'; }, 2500);
	setTimeout(() => { toast.remove(); }, 3000);
}

// About Modal
function ywebShowAbout() {
	const modal = document.getElementById('yweb-about-modal');
	if (!modal) return;

	// Load data from API (versions and links)
	ywebApi('versions')
		.then(data => {
			const setEl = (id, val) => {
				const el = document.getElementById(id);
				if (el && val) el.textContent = val;
			};
			setEl('about-yweb', data.yweb ? data.yweb + (data.yweb_date ? ' (' + data.yweb_date + ')' : '') : '--');
			setEl('about-neutrino', data.neutrino || '--');
			setEl('about-api', data.api || '--');

			// Build links from API data
			const linksEl = document.getElementById('about-links');
			if (linksEl && data.links) {
				let html = '';
				if (data.links.homepage)
					html += '<a href="' + data.links.homepage + '" target="_blank">Homepage</a>';
				if (data.links.docs)
					html += '<a href="' + data.links.docs + '" target="_blank">Dokumentation</a>';
				if (data.links.forum)
					html += '<a href="' + data.links.forum + '" target="_blank">Forum</a>';
				linksEl.innerHTML = html || '<span class="yweb-about-label">Keine Links verfügbar</span>';
			}

			// Update copyright from API
			if (data.copyright && data.copyright.text) {
				setEl('about-copyright', data.copyright.text);
				setEl('yweb-footer-copyright', data.copyright.text);
			}
		})
		.catch(() => {});

	// Load box info (v4 delivers JSON natively, no legacy text fallback needed)
	ywebApi('boxinfo')
		.then(data => {
			const boxEl = document.getElementById('about-box');
			if (boxEl) boxEl.textContent = (data.vendor && data.boxname) ? data.vendor + ' ' + data.boxname : data.boxname || '--';
			const archEl = document.getElementById('about-arch');
			if (archEl) archEl.textContent = data.boxarch || '--';
		})
		.catch(() => {});

	// Load OS info
	ywebApi('osinfo')
		.then(data => {
			const osEl = document.getElementById('about-os');
			if (osEl) osEl.textContent = data.pretty_name || data.name || '--';
		})
		.catch(() => {});

	modal.classList.add('open');
	document.body.style.overflow = 'hidden';
}

function ywebCloseAbout(event) {
	if (event && event.target !== event.currentTarget) return;
	const modal = document.getElementById('yweb-about-modal');
	if (modal) modal.classList.remove('open');
	document.body.style.overflow = '';
}

// ═══ Extension-Loader (WORK-146 Spec §4) ═══
// Konvention: <webroot>/extensions/<name>/manifest.json mit
//   {"name","version","slot":"nav|tools|dashboard|page","entry","description"?}
// Schaltung: config=yweb ext_<name>_enabled=1, optional ext_<name>_slot
// (Slot-Override). Fehlendes extensions-Verzeichnis ist KEIN Fehler.
var _ywebExtensions = null;

async function ywebDiscoverExtensions() {
	if (_ywebExtensions !== null) return _ywebExtensions;
	_ywebExtensions = [];
	try {
		var cfgResp = await ywebApi('config', {config: 'nhttpd'});
		var nh = (cfgResp.data && cfgResp.data.config) || {};
		var webroot = nh['WebsiteMain_dot_directory'];
		if (!webroot) return _ywebExtensions;
		var list;
		try {
			list = await ywebApi('file', {action: 'list', path: webroot + '/extensions'});
		} catch (e) { return _ywebExtensions; }
		var entries = (list.data && list.data.filelist) || list.filelist || [];
		var yCfgResp = await ywebApi('config', {config: 'yweb'});
		var yCfg = (yCfgResp.data && yCfgResp.data.config) || {};
		for (var i = 0; i < entries.length; i++) {
			var e = entries[i];
			var isDir = (e.type_str === 'dir' || e.type === 4 || e.type === '4');
			if (!isDir || !e.name || e.name.charAt(0) === '.') continue;
			try {
				// no-cache: revalidieren, sonst ueberlebt ein altes Manifest
				// im Browser-Cache jede Aenderung auf der Box
				var mResp = await fetch('/extensions/' + encodeURIComponent(e.name) + '/manifest.json', {cache: 'no-cache'});
				if (!mResp.ok) continue;
				var m = await mResp.json();
				if (!m || !m.name) continue;
				// entry ist Pflicht, ausser die Erweiterung liefert nur
				// eine Einstellungs-Kategorie (settings-only, keine Seite)
				if (!m.entry && !(m.settings && m.settings.sections)) continue;
				m.dir = e.name;
				// Config-Ausgabe encodiert '-' als _bind_ (ConfigCGI)
				var ckBase = ('ext_' + m.dir).replace(/-/g, '_bind_');
				m.enabled = (yCfg[ckBase + '_enabled'] === '1');
				m.slot = yCfg[ckBase + '_slot'] || m.slot || 'tools';
				m.url = m.entry ? ('/extensions/' + m.dir + '/' + m.entry) : '';
				_ywebExtensions.push(m);
			} catch (ignore) {}
		}
	} catch (ignore) {}
	return _ywebExtensions;
}

var _ywebExtInjected = false;

async function ywebInjectExtensions(pageName) {
	// Nur einmal pro Seitenaufruf injizieren, egal ueber welchen Init-Pfad
	if (_ywebExtInjected) return;
	_ywebExtInjected = true;
	var exts = await ywebDiscoverExtensions();
	exts.forEach(function(m) {
		if (!m.enabled || !m.url) return; // settings-only hat keine Seite
		if (m.slot === 'nav') {
			var ul = document.querySelector('.yweb-nav-links');
			if (ul) {
				var li = document.createElement('li');
				li.innerHTML = '<a href="' + m.url + '" data-page="ext-' + m.dir + '">' + escapeHtml(m.name) + '</a>';
				ul.appendChild(li);
			}
		} else if (m.slot === 'tools') {
			var grid = document.querySelector('.yweb-tools-grid');
			if (grid) {
				var a = document.createElement('a');
				a.href = m.url;
				a.className = 'yweb-tool-card';
				a.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2l3 6 6 1-4.5 4.4 1 6.6-5.5-3-5.5 3 1-6.6L3 9l6-1z"/></svg>' +
					'<span>' + escapeHtml(m.name) + '</span><small>' + escapeHtml(m.description || 'Erweiterung') + '</small>';
				grid.appendChild(a);
			}
		} else if (m.slot === 'dashboard') {
			var dash = document.querySelector('.yweb-dashboard');
			if (dash) {
				var card = document.createElement('div');
				card.className = 'yweb-card';
				card.innerHTML = '<h3>' + escapeHtml(m.name) + '</h3>' +
					'<p><a href="' + m.url + '">' + escapeHtml(m.description || 'Erweiterung oeffnen') + '</a></p>';
				dash.appendChild(card);
			}
		}
		// slot 'page': keine Injektion, erreichbar ueber die
		// Erweiterungs-Verwaltung oder direkten Link
	});
}

// Pull-to-refresh
let ywebRefreshCallback = null;
let ywebPullStartY = 0;
let ywebPullCurrentY = 0;
let ywebIsPulling = false;
let ywebIsRefreshing = false;
const YWEB_PULL_THRESHOLD = 60;
const YWEB_PULL_MAX = 80;

function ywebSetRefreshCallback(fn) {
	ywebRefreshCallback = fn;
}

function ywebCreatePullIndicator() {
	if (document.getElementById('yweb-pull-indicator')) return;

	const indicator = document.createElement('div');
	indicator.id = 'yweb-pull-indicator';
	indicator.className = 'yweb-pull-indicator';
	indicator.innerHTML = `
		<div class="yweb-pull-icon">
			<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
				<path d="M12 5v14M5 12l7 7 7-7"/>
			</svg>
		</div>
		<span class="yweb-pull-text"></span>
	`;
	document.body.insertBefore(indicator, document.body.firstChild);
}

function ywebUpdatePullIndicator(distance) {
	const indicator = document.getElementById('yweb-pull-indicator');
	if (!indicator) return;

	const progress = Math.min(distance / YWEB_PULL_THRESHOLD, 1);
	const translateY = Math.min(distance, YWEB_PULL_MAX) - 50;
	const textEl = indicator.querySelector('.yweb-pull-text');
	const iconEl = indicator.querySelector('.yweb-pull-icon');

	indicator.style.transform = `translateY(${translateY}px)`;
	indicator.classList.toggle('ready', distance >= YWEB_PULL_THRESHOLD);

	if (iconEl) {
		iconEl.style.transform = `rotate(${progress * 180}deg)`;
	}

	if (textEl) {
		textEl.textContent = distance >= YWEB_PULL_THRESHOLD ? 'Loslassen' : 'Ziehen...';
	}
}

function ywebShowRefreshing() {
	const indicator = document.getElementById('yweb-pull-indicator');
	if (!indicator) return;

	indicator.classList.remove('ready');
	indicator.classList.add('refreshing');
	indicator.style.transform = 'translateY(10px)';

	const textEl = indicator.querySelector('.yweb-pull-text');
	if (textEl) textEl.textContent = 'Aktualisiere...';
}

function ywebHidePullIndicator() {
	const indicator = document.getElementById('yweb-pull-indicator');
	if (!indicator) return;

	indicator.classList.remove('ready', 'refreshing');
	indicator.style.transform = 'translateY(-50px)';

	const iconEl = indicator.querySelector('.yweb-pull-icon');
	if (iconEl) iconEl.style.transform = 'rotate(0deg)';
}

// Check if at scroll top - can be overridden by pages with custom scroll containers
function ywebDefaultIsAtScrollTop() {
	// If page defines custom check, use it
	if (typeof window.ywebIsAtScrollTop === 'function') {
		return window.ywebIsAtScrollTop();
	}
	// Default: check window scroll and main element
	if (window.scrollY > 5) return false;
	const main = document.querySelector('main');
	if (main && main.scrollTop > 5) return false;
	return true;
}

function ywebPullTouchStart(e) {
	if (ywebIsRefreshing) return;
	if (window.innerWidth > 768) return;
	if (!ywebDefaultIsAtScrollTop()) return;

	ywebPullStartY = e.touches[0].clientY;
	ywebPullCurrentY = ywebPullStartY;
	ywebIsPulling = false;
}

function ywebPullTouchMove(e) {
	if (ywebIsRefreshing) return;
	if (window.innerWidth > 768) return;
	if (ywebPullStartY === 0) return;

	const touch = e.touches[0];
	const deltaY = touch.clientY - ywebPullStartY;
	const deltaX = Math.abs(touch.clientX - (ywebPullCurrentY ? touch.clientX : 0));

	// Only pull down, not up, and only if mostly vertical
	if (deltaY > 10 && ywebDefaultIsAtScrollTop() && deltaX < 30) {
		ywebIsPulling = true;
		ywebPullCurrentY = touch.clientY;

		// Dampen the pull distance
		const pullDistance = Math.pow(deltaY, 0.8);
		ywebUpdatePullIndicator(pullDistance);

		// Prevent default scroll to enable pull effect
		if (deltaY > 0 && ywebDefaultIsAtScrollTop()) {
			e.preventDefault();
		}
	}
}

function ywebPullTouchEnd(e) {
	if (ywebIsRefreshing) return;
	if (!ywebIsPulling) {
		ywebPullStartY = 0;
		return;
	}

	const deltaY = ywebPullCurrentY - ywebPullStartY;
	const pullDistance = Math.pow(deltaY, 0.8);

	if (pullDistance >= YWEB_PULL_THRESHOLD && ywebRefreshCallback) {
		ywebIsRefreshing = true;
		ywebShowRefreshing();

		// Execute refresh callback
		const result = ywebRefreshCallback();

		// Handle both Promise and non-Promise returns
		if (result && typeof result.then === 'function') {
			result.finally(() => {
				ywebIsRefreshing = false;
				ywebHidePullIndicator();
			});
		} else {
			// Non-promise: assume quick operation, hide after delay
			setTimeout(() => {
				ywebIsRefreshing = false;
				ywebHidePullIndicator();
			}, 500);
		}
	} else {
		ywebHidePullIndicator();
	}

	ywebPullStartY = 0;
	ywebPullCurrentY = 0;
	ywebIsPulling = false;
}

function ywebInitPullRefresh() {
	if (window.innerWidth > 768) return;

	ywebCreatePullIndicator();

	document.addEventListener('touchstart', ywebPullTouchStart, { passive: true });
	document.addEventListener('touchmove', ywebPullTouchMove, { passive: false });
	document.addEventListener('touchend', ywebPullTouchEnd, { passive: true });
}

// Initialize yWeb header and components
function ywebInit() {
	const headerWrapper = document.querySelector('.yweb-header-wrapper');
	const existingHeader = document.querySelector('.yweb-header');

	// Check for dynamic header generation (wrapper with data-page, no header yet)
	if (headerWrapper && !existingHeader) {
		const activePage = headerWrapper.dataset.page || null;
		// Prepend header before any existing content (e.g., subheaders)
		headerWrapper.insertAdjacentHTML('afterbegin', ywebGetHeaderHTML(activePage));

		// Create spacer if not present
		if (!document.querySelector('.yweb-header-spacer')) {
			headerWrapper.insertAdjacentHTML('afterend', '<div class="yweb-header-spacer"></div>');
		}
	}

	// Initialize if header exists (either pre-rendered or just generated)
	if (document.querySelector('.yweb-header')) {
		// Set theme
		ywebSetTheme(ywebGetTheme());

		// Start clock
		ywebUpdateClock();
		setInterval(ywebUpdateClock, 10000);

		// Load hostname
		ywebLoadHostname();

		// Calculate and set spacer height
		ywebUpdateHeaderSpacer();

		// Update on window resize
		window.addEventListener('resize', ywebUpdateHeaderSpacer);

		// Close menu when clicking on nav links (mobile)
		const navLinks = document.querySelectorAll('.yweb-nav-links a');
		navLinks.forEach(link => {
			link.addEventListener('click', ywebCloseMenu);
		});

		// Initialize footer (if not already present)
		ywebInitFooter();

		// Always insert about modal if not present
		if (!document.getElementById('yweb-about-modal')) {
			document.body.insertAdjacentHTML('beforeend', YWEB_ABOUT_HTML);
		}

		// Initialize mobile menu swipe gestures
		ywebInitMenuSwipe();

		// Initialize pull-to-refresh
		ywebInitPullRefresh();

		// Erweiterungen in die Slots dieser Seite injizieren (async, still).
		// Host-Seiten booten ueber diesen Pfad, nicht ueber ywebInitHeader.
		if (typeof ywebInjectExtensions === 'function') {
			setTimeout(function() {
				ywebInjectExtensions(headerWrapper ? headerWrapper.dataset.page : null);
			}, 0);
		}
	}
}

/* ============================================
   Mobile Accordion (shared card toggle/swipe)
   ============================================ */
var _ywebAcc = null; /* accordion config */
var _ywebAccTouchStart = {x: 0, y: 0};

/**
 * Initialize mobile accordion for card-based pages.
 * @param {Object} cfg
 * @param {string[]} cfg.cardOrder - IDs of cards in display order
 * @param {string} cfg.storageKey - localStorage key for active card
 * @param {string} cfg.defaultCard - default card ID
 * @param {number} cfg.breakpoint - max viewport width for mobile mode (default 800)
 * @param {string} cfg.indicatorId - ID of dot indicator container
 * @param {string} cfg.containerSelector - CSS selector for swipe container
 * @param {string} cfg.mode - 'collapsed' (add to hide) or 'expanded' (add to show)
 */
function ywebInitMobileAccordion(cfg) {
	cfg.breakpoint = cfg.breakpoint || 800;
	cfg.mode = cfg.mode || 'collapsed';
	_ywebAcc = cfg;

	initMobileAccordion();
	buildCardIndicator();

	var container = document.querySelector(cfg.containerSelector);
	if (container) {
		container.addEventListener('touchstart', function(e) {
			_ywebAccTouchStart.x = e.changedTouches[0].screenX;
			_ywebAccTouchStart.y = e.changedTouches[0].screenY;
		}, {passive: true});
		container.addEventListener('touchend', function(e) {
			if (!_ywebAcc || window.innerWidth > _ywebAcc.breakpoint) return;
			var dx = e.changedTouches[0].screenX - _ywebAccTouchStart.x;
			var dy = Math.abs(e.changedTouches[0].screenY - _ywebAccTouchStart.y);
			if (Math.abs(dx) > 50 && dy < 100) navigateCard(dx > 0 ? -1 : 1);
		}, {passive: true});
	}

	var resizeTimer;
	window.addEventListener('resize', function() {
		clearTimeout(resizeTimer);
		resizeTimer = setTimeout(initMobileAccordion, 250);
	});
}

function _ywebAccGetActive() {
	var id;
	try { id = localStorage.getItem(_ywebAcc.storageKey); } catch(e) {}
	return id || _ywebAcc.defaultCard;
}

function _ywebAccSetCard(card, cardId) {
	if (_ywebAcc.mode === 'collapsed') {
		document.querySelectorAll('.yweb-card').forEach(function(c) { c.classList.add('collapsed'); });
		card.classList.remove('collapsed');
	} else {
		document.querySelectorAll('.yweb-card.expanded').forEach(function(c) { c.classList.remove('expanded'); });
		card.classList.add('expanded');
	}
	try { localStorage.setItem(_ywebAcc.storageKey, cardId); } catch(e) {}
	updateCardIndicator();
}

function toggleCard(cardId) {
	if (!_ywebAcc || window.innerWidth > _ywebAcc.breakpoint) return;
	var card = document.getElementById(cardId);
	if (!card) return;

	if (_ywebAcc.mode === 'collapsed') {
		var isCollapsed = card.classList.contains('collapsed');
		document.querySelectorAll('.yweb-card').forEach(function(c) { c.classList.add('collapsed'); });
		if (isCollapsed) card.classList.remove('collapsed');
	} else {
		document.querySelectorAll('.yweb-card.expanded').forEach(function(c) {
			if (c !== card) c.classList.remove('expanded');
		});
		card.classList.toggle('expanded');
	}
	var isActive = _ywebAcc.mode === 'collapsed' ? !card.classList.contains('collapsed') : card.classList.contains('expanded');
	if (isActive) {
		try { localStorage.setItem(_ywebAcc.storageKey, cardId); } catch(e) {}
	}
	updateCardIndicator();
}

function navigateCard(delta) {
	if (!_ywebAcc || window.innerWidth > _ywebAcc.breakpoint) return;
	var currentId = _ywebAccGetActive();
	var idx = _ywebAcc.cardOrder.indexOf(currentId);
	if (idx === -1) return;
	var newIdx = idx + delta;
	if (newIdx < 0) newIdx = _ywebAcc.cardOrder.length - 1;
	if (newIdx >= _ywebAcc.cardOrder.length) newIdx = 0;
	var newId = _ywebAcc.cardOrder[newIdx];
	var newCard = document.getElementById(newId);
	if (newCard) {
		_ywebAccSetCard(newCard, newId);
		newCard.scrollIntoView({behavior: 'smooth', block: 'nearest'});
	}
}

function selectCard(cardId) {
	if (!_ywebAcc || window.innerWidth > _ywebAcc.breakpoint) return;
	var card = document.getElementById(cardId);
	if (card) {
		_ywebAccSetCard(card, cardId);
		card.scrollIntoView({behavior: 'smooth', block: 'nearest'});
	}
}
/* dashboard.html uses navigateToCard — alias */
var navigateToCard = selectCard;

function buildCardIndicator() {
	if (!_ywebAcc) return;
	var el = document.getElementById(_ywebAcc.indicatorId);
	if (!el) return;
	el.innerHTML = _ywebAcc.cardOrder.map(function(id) {
		return '<div class="yweb-card-dot" data-card="' + id + '" onclick="selectCard(\'' + id + '\')"></div>';
	}).join('');
	updateCardIndicator();
}

function updateCardIndicator() {
	if (!_ywebAcc) return;
	var activeId = _ywebAccGetActive();
	document.querySelectorAll('.yweb-card-dot').forEach(function(dot) {
		var dotCard = dot.dataset ? dot.dataset.card : null;
		if (!dotCard) {
			var idx = Array.prototype.indexOf.call(dot.parentNode.children, dot);
			dotCard = _ywebAcc.cardOrder[idx];
		}
		dot.classList.toggle('active', dotCard === activeId);
	});
}

function initMobileAccordion() {
	if (!_ywebAcc) return;
	if (window.innerWidth > _ywebAcc.breakpoint) {
		if (_ywebAcc.mode === 'collapsed') {
			document.querySelectorAll('.yweb-card').forEach(function(c) { c.classList.remove('collapsed'); });
		} else {
			document.querySelectorAll('.yweb-card').forEach(function(c) { c.classList.add('expanded'); });
		}
	} else {
		var savedCard = _ywebAccGetActive();
		if (_ywebAcc.mode === 'collapsed') {
			document.querySelectorAll('.yweb-card').forEach(function(c) { c.classList.add('collapsed'); });
			var active = document.getElementById(savedCard);
			if (active) active.classList.remove('collapsed');
		} else {
			document.querySelectorAll('.yweb-card').forEach(function(c) { c.classList.remove('expanded'); });
			var active = document.getElementById(savedCard);
			if (active) active.classList.add('expanded');
		}
	}
	updateCardIndicator();
}

// Auto-detect page from URL and initialize
// Handle both cases: script in head (DOMContentLoaded) and script at end of body (already ready)
if (document.readyState === 'loading') {
	document.addEventListener('DOMContentLoaded', ywebInit);
} else {
	// DOM already loaded, init immediately
	ywebInit();
}
