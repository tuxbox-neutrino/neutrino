/**
 * Neutrino-Web-UI common header
 * Provides the same header on every page of the interface
 */

// Central configuration
const WEBUI_CONFIG = {
	logo: '/img/logo.png',
	title: 'Neutrino-Web-UI'
};

// i18n helper: reads the texts webui-i18n.js loaded (window.WEBUI_L, the
// language's file laid over en.json). Second argument is the English
// fallback for a page that loads no texts and for a key that is missing.
function webuiL(key, fallbackEN) {
	var d = window.WEBUI_L;
	var v = d && d[key];
	if (v && v === '# yw.' + key + ' #') v = ''; // CLanguage missing-marker
	return v || fallbackEN || key;
}

// The same, for a text put into markup: escaped for it (webui-i18n.js has
// one too, for the pages without this header).
function webuiLH(key, fallbackEN) {
	return escapeHtml(webuiL(key, fallbackEN));
}

// SVG Icons for menu items
const WEBUI_ICONS = {
	dashboard: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg>',
	bouquets: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5,3 19,12 5,21"/></svg>',
	epg: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/></svg>',
	recordings: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M7 4v16M17 4v16M2 9h5M2 15h5M17 9h5M17 15h5"/></svg>',
	timer: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/></svg>',
	remote: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="7" y="2" width="10" height="20" rx="2"/><circle cx="12" cy="18" r="1"/></svg>',
	settings: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>',
	tools: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z"/></svg>'
};

// Shared modal close button
function _webuiCloseBtn(onclick) {
	return '<button type="button" class="webui-modal-close" aria-label="Close" onclick="' + onclick + '">' +
		'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true" focusable="false"><path d="M6 18L18 6M6 6l12 12"/></svg>' +
	'</button>';
}

// Footer HTML, built once the texts are in
function webuiFooterHTML() {
	return `
<footer class="webui-footer">
	Neutrino-Web-UI <span id="webui-footer-version">--</span> ·
	<a href="#" onclick="webuiShowAbout(); return false;">${webuiLH('about.link', 'About')}</a> ·
	<span id="webui-footer-copyright">Copyright © Tuxbox Project</span>
</footer>
`;
}

// About modal HTML, built once the texts are in
function webuiAboutHTML() {
	return `
<div class="webui-modal-backdrop" id="webui-about-modal" onclick="webuiCloseAbout(event)">
	<div class="webui-modal" onclick="event.stopPropagation()">
		<div class="webui-modal-header">
			<h3><img src="${WEBUI_CONFIG.logo}" alt="" onerror="this.remove()"> ${WEBUI_CONFIG.title}</h3>
			${_webuiCloseBtn('webuiCloseAbout()')}
		</div>
		<div class="webui-modal-body">
			<div class="webui-about-section">
				<h4>Versionen</h4>
				<div class="webui-about-grid">
					<span class="webui-about-label">Neutrino-Web-UI</span>
					<span class="webui-about-value" id="about-webui">--</span>
					<span class="webui-about-label">Neutrino</span>
					<span class="webui-about-value" id="about-neutrino">--</span>
					<span class="webui-about-label">API</span>
					<span class="webui-about-value" id="about-api">--</span>
				</div>
			</div>
			<div class="webui-about-section">
				<h4>System</h4>
				<div class="webui-about-grid">
					<span class="webui-about-label">Box</span>
					<span class="webui-about-value" id="about-box">--</span>
					<span class="webui-about-label">Architektur</span>
					<span class="webui-about-value" id="about-arch">--</span>
					<span class="webui-about-label">Betriebssystem</span>
					<span class="webui-about-value" id="about-os">--</span>
				</div>
			</div>
			<div class="webui-about-section">
				<h4>Links</h4>
				<div class="webui-about-links" id="about-links">
					<span class="webui-about-label">Laden...</span>
				</div>
			</div>
			<div class="webui-about-section">
				<h4>Lizenz &amp; Copyright</h4>
				<div class="webui-about-legal">
					<p><strong>Neutrino-Web-UI</strong> - based on yWeb by yjogol<br>
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
}

// Generate header HTML with optional active page
function webuiGetHeaderHTML(activePage) {
	const nav = WEBUI_CONFIG.navigation || [
		{ page: 'dashboard', href: '/dashboard.html', icon: 'dashboard', label: webuiL('nav.dashboard', 'Dashboard') },
		{ page: 'bouquets', href: '/bouquets.html', icon: 'bouquets', label: webuiL('nav.bouquets', 'Bouquets') },
		{ page: 'epg', href: '/epg.html', icon: 'epg', label: webuiL('nav.epg', 'EPG') },
		{ page: 'timer', href: '/timer.html', icon: 'timer', label: webuiL('nav.timer', 'Timer') },
		{ page: 'recordings', href: '/recordings.html', icon: 'recordings', label: webuiL('nav.recordings', 'Recordings') },
		{ page: 'settings', href: '/settings.html', icon: 'settings', label: webuiL('nav.settings', 'Settings') },
		{ page: 'tools', href: '/tools.html', icon: 'tools', label: webuiL('nav.tools', 'Tools') }
	];

	const navItems = nav.map(item => {
		const active = item.page === activePage ? ' class="active"' : '';
		return `<li><a href="${item.href}" data-page="${item.page}"${active}>${WEBUI_ICONS[item.icon]}<span>${escapeHtml(item.label)}</span></a></li>`;
	}).join('\n\t\t\t');

	return `
<header class="webui-header">
	<nav>
		<a href="/dashboard.html" class="webui-logo">
			<img src="${WEBUI_CONFIG.logo}" alt="" onerror="this.remove()">
			<span>${WEBUI_CONFIG.title}</span>
		</a>

		<ul class="webui-nav-links" id="webui-nav-menu">
			${navItems}
		</ul>

		<div class="webui-header-right">
			<span class="webui-hostname" id="webui-hostname">--</span>
			<span class="webui-clock" id="webui-clock">--:--</span>
			<button class="webui-theme-toggle webui-lang-btn" onclick="webuiToggleLangMenu(event)" title="${webuiLH('lang.title', 'Language')}">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<circle cx="12" cy="12" r="9" stroke-width="2"/>
					<path stroke-width="2" d="M3 12h18M12 3a15 15 0 010 18M12 3a15 15 0 000 18"/>
				</svg>
				<span class="webui-lang-code" id="webui-lang-code"></span>
			</button>
			<button class="webui-theme-toggle" onclick="webuiToggleTheme()" title="${webuiLH('theme.toggle', 'Toggle theme')}">
				<svg id="webui-theme-icon-dark" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20.354 15.354A9 9 0 018.646 3.646 9.003 9.003 0 0012 21a9.003 9.003 0 008.354-5.646z"/>
				</svg>
				<svg id="webui-theme-icon-light" class="webui-hidden" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z"/>
				</svg>
			</button>
			<button class="webui-theme-toggle" id="webui-logout-btn" hidden onclick="webuiLogout()" aria-label="${webuiLH('hdr.logout', 'Log out')}" title="${webuiLH('hdr.logout', 'Log out')}">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/>
					<polyline stroke-linecap="round" stroke-linejoin="round" stroke-width="2" points="16 17 21 12 16 7"/>
					<line stroke-linecap="round" stroke-linejoin="round" stroke-width="2" x1="21" y1="12" x2="9" y2="12"/>
				</svg>
			</button>
			<button class="webui-menu-toggle" onclick="webuiToggleMenu()" aria-label="Menü">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h16"/>
				</svg>
			</button>
		</div>
	</nav>
</header>`;
}

// Theme Management. The choice was kept under 'yweb-theme' before the
// interface took its own name; it is read from there until it is set again.
function webuiGetTheme() {
	return localStorage.getItem('webui-theme') || localStorage.getItem('yweb-theme') || 'dark';
}

function webuiSetTheme(theme) {
	document.documentElement.setAttribute('data-theme', theme);
	localStorage.setItem('webui-theme', theme);
	localStorage.removeItem('yweb-theme');
	webuiUpdateThemeIcon(theme);
}

// ═══ Sprachwechsler ═══
// The language the pages are shown in comes as its code (en, de) with the
// session document (GET /api/v1/session, language), which every page with
// this header asks for anyway; the header button shows that code beside the
// globe, which alone was not recognisable as the language switch. The list
// of languages is asked for when the menu opens (GET /api/v1/webui/language).
var _webuiSession = null;

// The session document, asked once per page: whether a password is owed,
// whether a session is open, and the language of the pages.
function webuiSessionInfo() {
	if (!_webuiSession)
		_webuiSession = fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'})
			.then(function(r) { return r.ok ? r.json() : null; })
			.catch(function() { return null; });
	return _webuiSession;
}

async function webuiActiveLangCode() {
	var s = await webuiSessionInfo();
	return (s && s.language) || 'en';
}

// ═══ Per-extension language packs ═══
// Extensions ship their own dictionaries as flat JSON:
// <ext>/languages/<code>.json — named by the language's code (en.json,
// de.json, ...). English texts live inline in the markup/manifest as
// fallback; there is deliberately no second fetch of en.json. Reserved
// keys manifest.name/manifest.description localize tile/nav/manager
// (hook in webuiDiscoverExtensions).
var _webuiExtLang = {};

async function webuiExtLangLoad(dir) {
	if (_webuiExtLang[dir]) return _webuiExtLang[dir];
	var dict = {};
	try {
		var resp = await fetch('/extensions/' + encodeURIComponent(dir) +
			'/languages/' + encodeURIComponent(await webuiActiveLangCode()) + '.json',
			{cache: 'no-cache'});
		if (resp.ok) dict = (await resp.json()) || {};
	} catch (ignore) {} // a missing language pack is NOT an error
	_webuiExtLang[dir] = dict;
	return dict;
}

function webuiExtL(dir, key, fallbackEN) {
	var d = _webuiExtLang[dir];
	var v = d && d[key];
	return v || fallbackEN || key;
}

async function webuiToggleLangMenu(ev) {
	ev.stopPropagation();
	var menu = document.getElementById('webui-lang-menu');
	if (menu) { menu.remove(); return; }
	var r = ev.currentTarget.getBoundingClientRect();
	var answer = null;
	try {
		answer = await _webuiV1Read('/webui/language');
	} catch (ignore) {}
	var langs = (answer && answer.items) || [];
	if (!langs.length) {
		webuiShowToast(webuiL('lang.none', 'No languages installed'), true);
		return;
	}
	if (document.getElementById('webui-lang-menu')) return;
	menu = document.createElement('div');
	menu.id = 'webui-lang-menu';
	menu.className = 'webui-lang-menu';
	langs.forEach(function(l) {
		var b = document.createElement('button');
		b.type = 'button';
		b.textContent = l.name;
		b.lang = l.code;
		if (l.code === answer.selected) b.classList.add('active');
		b.onclick = function() { webuiSetLanguage(l.code); };
		menu.appendChild(b);
	});
	menu.style.top = (r.bottom + 4) + 'px';
	menu.style.right = Math.max(4, window.innerWidth - r.right) + 'px';
	document.body.appendChild(menu);
	setTimeout(function() {
		document.addEventListener('click', function close() {
			document.removeEventListener('click', close);
			var m = document.getElementById('webui-lang-menu');
			if (m) m.remove();
		});
	}, 0);
}

// The server writes the language into its own file and reads that again,
// so the old pages follow; this page reads its texts when it loads. A page
// that loads the header alone (an extension's) has no webuiV1 and sends the
// write itself, with the session's second token.
async function webuiSetLanguage(code) {
	try {
		if (typeof webuiV1 === 'function') {
			await webuiV1('PUT', '/webui/language', {body: {language: code}});
		} else {
			var s = (await webuiSessionInfo()) || {};
			var headers = {'Content-Type': 'application/json'};
			if (s.csrf_header) headers[s.csrf_header] = s.csrf;
			var r = await fetch('/api/v1/webui/language', {method: 'PUT', credentials: 'same-origin',
				cache: 'no-store', headers: headers, body: JSON.stringify({language: code})});
			if (!r.ok) throw new Error('HTTP ' + r.status);
		}
		location.reload();
	} catch (e) {
		webuiShowToast('Error: ' + e.message, true);
	}
}

function webuiToggleTheme() {
	const current = webuiGetTheme();
	webuiSetTheme(current === 'dark' ? 'light' : 'dark');
}

function webuiUpdateThemeIcon(theme) {
	const darkIcon = document.getElementById('webui-theme-icon-dark');
	const lightIcon = document.getElementById('webui-theme-icon-light');
	if (darkIcon) darkIcon.classList.toggle('webui-hidden', theme === 'light');
	if (lightIcon) lightIcon.classList.toggle('webui-hidden', theme === 'dark');
}

// Mobile Menu
function webuiToggleMenu() {
	const menu = document.getElementById('webui-nav-menu');
	let backdrop = document.getElementById('webui-menu-backdrop');
	let menuHeader = document.getElementById('webui-menu-header');

	// Create backdrop if it doesn't exist
	if (!backdrop) {
		backdrop = document.createElement('div');
		backdrop.id = 'webui-menu-backdrop';
		backdrop.className = 'webui-menu-backdrop';
		backdrop.onclick = webuiCloseMenu;
		document.body.appendChild(backdrop);
	}

	// Create menu header with logo and close button
	if (!menuHeader && menu) {
		menuHeader = document.createElement('div');
		menuHeader.id = 'webui-menu-header';
		menuHeader.className = 'webui-menu-header';
		menuHeader.innerHTML = `
			<div class="webui-menu-logo">
				<img src="${WEBUI_CONFIG.logo}" alt="" onerror="this.remove()">
				<span>${WEBUI_CONFIG.title}</span>
			</div>
			<button class="webui-menu-close" onclick="webuiCloseMenu()">
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

function webuiCloseMenu() {
	const menu = document.getElementById('webui-nav-menu');
	const backdrop = document.getElementById('webui-menu-backdrop');

	if (menu) menu.classList.remove('open');
	if (backdrop) backdrop.classList.remove('open');
	document.body.style.overflow = '';
}

function webuiOpenMenu() {
	const menu = document.getElementById('webui-nav-menu');
	let backdrop = document.getElementById('webui-menu-backdrop');
	let menuHeader = document.getElementById('webui-menu-header');

	// Create backdrop if needed
	if (!backdrop) {
		backdrop = document.createElement('div');
		backdrop.id = 'webui-menu-backdrop';
		backdrop.className = 'webui-menu-backdrop';
		backdrop.onclick = webuiCloseMenu;
		document.body.appendChild(backdrop);
	}

	// Create menu header if needed
	if (!menuHeader && menu) {
		menuHeader = document.createElement('div');
		menuHeader.id = 'webui-menu-header';
		menuHeader.className = 'webui-menu-header';
		menuHeader.innerHTML = `
			<div class="webui-menu-logo">
				<img src="${WEBUI_CONFIG.logo}" alt="" onerror="this.remove()">
				<span>${WEBUI_CONFIG.title}</span>
			</div>
			<button class="webui-menu-close" onclick="webuiCloseMenu()">
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

function webuiMenuTouchStart(e) {
	const touch = e.touches[0];
	menuTouchStartX = touch.clientX;
	menuTouchStartY = touch.clientY;
	menuTouchCurrentX = touch.clientX;
	menuIsSwiping = false;
}

function webuiMenuTouchMove(e) {
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

function webuiMenuTouchEnd(e) {
	if (!menuIsSwiping) return;

	const deltaX = menuTouchCurrentX - menuTouchStartX;
	const menu = document.getElementById('webui-nav-menu');

	if (menu && menu.classList.contains('open')) {
		// Menu is open - swipe left to close
		if (deltaX < -MENU_SWIPE_THRESHOLD) {
			webuiCloseMenu();
		}
	}

	menuIsSwiping = false;
}

// Edge swipe to open menu
function webuiEdgeTouchStart(e) {
	const touch = e.touches[0];
	menuTouchStartX = touch.clientX;
	menuTouchStartY = touch.clientY;
	menuTouchCurrentX = touch.clientX;
	menuIsSwiping = false;
}

function webuiEdgeTouchMove(e) {
	if (!e.touches[0]) return;
	const touch = e.touches[0];
	menuTouchCurrentX = touch.clientX;
	const deltaY = Math.abs(touch.clientY - menuTouchStartY);

	// Only track horizontal swipes from edge
	if (menuTouchStartX < EDGE_SWIPE_ZONE && deltaY < 50) {
		menuIsSwiping = true;
	}
}

function webuiEdgeTouchEnd(e) {
	if (!menuIsSwiping) return;

	const deltaX = menuTouchCurrentX - menuTouchStartX;
	const menu = document.getElementById('webui-nav-menu');

	// Swipe right from left edge to open
	if (menuTouchStartX < EDGE_SWIPE_ZONE && deltaX > MENU_SWIPE_THRESHOLD) {
		if (!menu || !menu.classList.contains('open')) {
			webuiOpenMenu();
		}
	}

	menuIsSwiping = false;
}

// Initialize menu swipe gestures
function webuiInitMenuSwipe() {
	if (window.innerWidth > 480) return;

	const menu = document.getElementById('webui-nav-menu');

	// Swipe on menu to close
	if (menu) {
		menu.addEventListener('touchstart', webuiMenuTouchStart, { passive: true });
		menu.addEventListener('touchmove', webuiMenuTouchMove, { passive: true });
		menu.addEventListener('touchend', webuiMenuTouchEnd, { passive: true });
	}

	// Edge swipe to open - listen on body
	document.body.addEventListener('touchstart', webuiEdgeTouchStart, { passive: true });
	document.body.addEventListener('touchmove', webuiEdgeTouchMove, { passive: true });
	document.body.addEventListener('touchend', webuiEdgeTouchEnd, { passive: true });
}

// Clock
function webuiUpdateClock() {
	const clock = document.getElementById('webui-clock');
	if (clock) clock.textContent = webuiFormatTime(new Date());
}

/* What the box says about itself, over /api/v1, in the shape the pages
   have always read (the old sysinfo answer): sizes in kilobytes, the root
   filesystem as disk, every storage filesystem in disks, the interfaces with
   ip. Readable without a sign-in from the home network; null where the box
   does not answer. full asks for the filesystems and the interfaces as well,
   which only the information page shows. */
async function webuiSystemInfo(full) {
	var read = async function(path) {
		try {
			var r = await fetch('/api/v1' + path, {credentials: 'same-origin', cache: 'no-store'});
			return r.ok ? r.json() : null;
		} catch (e) { return null; }
	};
	var all = await Promise.all([read('/system/info'), full ? read('/storage/mounts') : null,
		full ? read('/network/interfaces') : null]);
	var info = all[0];
	if (!info) return null;
	var kb = function(bytes) { return Math.round((bytes || 0) / 1024); };
	var pct = function(used, total) { return total > 0 ? Math.round(used * 100 / total) : 0; };
	var memTotal = kb(info.memory_total), memUsed = memTotal - kb(info.memory_free);
	var rootTotal = kb(info.root_total), rootFree = kb(info.root_free);
	// storage the way the old page chose it: devices, flash, network shares and /
	var disks = [], seen = {};
	((all[1] && all[1].items) || []).forEach(function(m) {
		var flash = ['ubifs', 'jffs2', 'yaffs2'].indexOf(m.fstype) >= 0;
		var net = ['nfs', 'nfs4', 'cifs', 'smb3'].indexOf(m.fstype) >= 0;
		if (!(String(m.device).indexOf('/dev/') === 0 || flash || net || m.id === '/')) return;
		if (seen[m.device] || !m.total) return;
		seen[m.device] = true;
		var total = kb(m.total), free = kb(m.free);
		disks.push({device: m.device, mount: m.id, fstype: m.fstype, total: total, free: free,
			used: total - free, percent: pct(total - free, total)});
	});
	return {
		hostname: info.hostname,
		uptime: info.uptime,
		kernel: info.kernel,
		cpu_model: info.cpu ? info.cpu.model : '',
		cpu: info.cpu && info.cpu.load_percent >= 0 ? {percent: info.cpu.load_percent, cores: info.cpu.cores}
			: (info.cpu ? {cores: info.cpu.cores} : undefined),
		load: info.load_average,
		processes: {running: info.processes_running, total: info.processes_total},
		temperature: info.temperature_known ? info.temperature : null,
		memory: {total: memTotal, used: memUsed, percent: pct(memUsed, memTotal)},
		disk: {total: rootTotal, free: rootFree, used: rootTotal - rootFree, percent: pct(rootTotal - rootFree, rootTotal)},
		disks: disks,
		box: {vendor: info.vendor, name: info.model, arch: info.chipset},
		os: info.os,
		interfaces: ((all[2] && all[2].items) || []).map(function(i) {
			return {name: i.name, ip: i.ipv4, mac: i.mac, up: i.up};
		}),
		info: info
	};
}

/* The versions and links in the shape of the old versions answer: the
   program's and the interface's from /api/v1/system/info, the pages' own
   from the Y_Version.txt they ship. */
async function webuiVersions(sys) {
	var info = sys ? sys.info : null;
	if (!info) {
		try {
			var r = await fetch('/api/v1/system/info', {credentials: 'same-origin', cache: 'no-store'});
			info = r.ok ? await r.json() : {};
		} catch (e) { info = {}; }
	}
	var out = {
		neutrino: info.neutrino_version || '',
		neutrino_git: info.neutrino_git || '',
		api: info.api ? info.api.major + '.' + info.api.minor : '',
		links: info.support || {},
		copyright: {text: info.support ? info.support.copyright : ''}
	};
	try {
		var v = await fetch('/Y_Version.txt', {cache: 'no-cache'});
		if (v.ok) (await v.text()).split('\n').forEach(function(line) {
			if (line.indexOf('version=') === 0) out.webui = line.slice(8).trim();
			else if (line.indexOf('date=') === 0) out.webui_date = line.slice(5).trim();
		});
	} catch (e) { /* no version file */ }
	return out;
}

// One reading per page for the header's name and the footer: each reading
// measures the processor for a tenth of a second.
var _webuiSystemInfoOnce = null;
function webuiSystemInfoOnce() {
	if (!_webuiSystemInfoOnce) _webuiSystemInfoOnce = webuiSystemInfo(false);
	return _webuiSystemInfoOnce;
}

// Load hostname
function webuiLoadHostname() {
	const el = document.getElementById('webui-hostname');
	if (!el) return;
	webuiSystemInfoOnce()
		.then(data => {
			el.textContent = (data && (data.hostname || (data.box && data.box.name))) || 'neutrino';
		})
		.catch(() => { el.textContent = 'neutrino'; });
}

// Set active navigation link
function webuiSetActivePage(pageName) {
	const links = document.querySelectorAll('.webui-nav-links a');
	links.forEach(link => {
		if (link.getAttribute('data-page') === pageName) {
			link.classList.add('active');
		} else {
			link.classList.remove('active');
		}
	});
}

// Initialize header
function webuiInitHeader(pageName) {
	// Insert header at beginning of body
	const headerContainer = document.getElementById('webui-header-container');
	if (headerContainer) {
		headerContainer.innerHTML = webuiGetHeaderHTML();
	} else {
		document.body.insertAdjacentHTML('afterbegin', webuiGetHeaderHTML());
	}

	// Set theme
	webuiSetTheme(webuiGetTheme());

	// Set active page
	if (pageName) {
		webuiSetActivePage(pageName);
	}

	// Start clock
	webuiUpdateClock();
	setInterval(webuiUpdateClock, 10000);

	// Load hostname
	webuiLoadHostname();
	// Erweiterungen in die Slots dieser Seite injizieren (async, still)
	if (typeof webuiInjectExtensions === 'function')
		setTimeout(function() { webuiInjectExtensions(pageName); }, 0);

	webuiCheckDefaultPassword();
}

// A session opened with the password the box shipped with is to set one of
// its own first: the box refuses it everything else until then
// (password_change_required in GET /api/v1/session), so the page goes to
// where that is done. Asked on every page load, which costs one request; a
// page without a session is not asked, the server having sent it to the
// sign-in already (or exempted it). password.html and login.html do not
// load this script, so this cannot loop.
function webuiCheckDefaultPassword() {
	webuiSessionInfo()
		.then(function(s) {
			var code = document.getElementById('webui-lang-code');
			if (code) code.textContent = ((s && s.language) || 'en').toUpperCase();
			if (s && s.password_change_required) {
				location.replace('/password.html?next=' + encodeURIComponent(location.pathname + location.search));
				return;
			}
			webuiUpdateLogoutBtn(!!(s && s.authenticated));
		})
		.catch(function() {});
}

// The button shows while a session of the one login is open (the page
// login opens one too): what was signed into can be ended. Set from the
// session document the password check above reads.
function webuiUpdateLogoutBtn(show) {
	var btn = document.getElementById('webui-logout-btn');
	if (btn) btn.hidden = !show;
}

// /logout ends the session and tells the browser to forget its cookie.
// With the page login on it answers 401 to whatever still carries Basic
// credentials, which makes the browser drop them; with it off it goes
// back to the start page.
function webuiLogout() {
	location.href = '/logout';
}

// Update spacer height based on actual header dimensions
function webuiUpdateHeaderSpacer() {
	const wrapper = document.querySelector('.webui-header-wrapper');
	const spacer = document.querySelector('.webui-header-spacer');

	if (wrapper && spacer) {
		const height = wrapper.offsetHeight;
		spacer.style.height = height + 'px';
	}
}

// Initialize footer
function webuiInitFooter() {
	// Check if footer already exists
	if (document.querySelector('.webui-footer')) return;

	// Insert footer at end of body
	document.body.insertAdjacentHTML('beforeend', webuiFooterHTML());

	// Insert about modal
	if (!document.getElementById('webui-about-modal')) {
		document.body.insertAdjacentHTML('beforeend', webuiAboutHTML());
	}

	// Load versions from API
	webuiSystemInfoOnce()
		.then(sys => webuiVersions(sys))
		.then(data => {
			if (data.webui) {
				const el = document.getElementById('webui-footer-version');
				if (el) el.textContent = data.webui;
			}
			if (data.api) {
				const el = document.getElementById('webui-footer-api');
				if (el) el.textContent = data.api;
			}
			if (data.copyright && data.copyright.text) {
				const el = document.getElementById('webui-footer-copyright');
				if (el) el.textContent = data.copyright.text;
			}
		})
		.catch(() => {});
}

// Shared helper functions
function webuiPad2(n) {
	return String(n).padStart(2, '0');
}

function webuiFormatTime(date) {
	return webuiPad2(date.getHours()) + ':' + webuiPad2(date.getMinutes());
}

function webuiFormatDateInput(date) {
	return date.getFullYear() + '-' + webuiPad2(date.getMonth() + 1) + '-' + webuiPad2(date.getDate());
}

// Fetch helpers — simple wrappers for common patterns
async function webuiFetchText(url) {
	var resp = await fetch(url);
	return resp.text();
}

async function webuiFetchJson(url) {
	var resp = await fetch(url);
	return resp.json();
}

// Central /api/v4/ fetch helper — the ONLY way modern pages talk to the box.
//   webuiApi('timer')                            -> parsed JSON payload
//   webuiApi('timer', {action:'new', type:5})    -> envelope-checked action
//   webuiApi('setmode', {status:''})             -> key-only query param
//   webuiApi('exec', 'Y_Tools&getline&/tmp/lcd/service', {raw:true}) -> raw text (exec/rcem)
// Throws Error on HTTP failure or {"success":"false"} envelopes; the
// error message carries error.msg from the box when available.
async function webuiApi(endpoint, params, opts) {
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
	// opts.post sends the parameters in the body instead, for what must not
	// end up in an address: a log keeps the address, never the body. The
	// type is named exactly, the server parses no other spelling of it.
	var resp;
	if (opts.post) {
		var cut = url.indexOf('?');
		resp = await fetch(cut < 0 ? url : url.substring(0, cut), {
			method: 'POST', credentials: 'same-origin',
			headers: {'Content-Type': 'application/x-www-form-urlencoded'},
			body: cut < 0 ? '' : url.substring(cut + 1)
		});
	} else {
		resp = await fetch(url);
	}
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
		throw new Error((data.error && data.error.msg) ? data.error.msg : (webuiL('msg.error', 'Error') + ': ' + endpoint));
	return data;
}

// A key of the remote control. Over /api/v1 where the page loads webui-v1.js
// (session, CSRF token), else over the old interface, which extensions and
// older pages still use.
async function webuiSendKey(key) {
	var v1 = typeof webuiV1 === 'function';
	var send = v1
		? function() { return webuiV1('POST', '/osd/remote/key', {body: {name: key}}); }
		: function() { return webuiApi('rcem', key, {raw: true}); };
	if (key === 'KEY_POWER') {
		try {
			var awake = v1
				? (await webuiV1('GET', '/system/standby')).on === false
				: (await webuiApi('standby', {status: ''})).standby === false;
			if (awake) {
				// Rueckfrage als webui-Dialog statt nativem confirm()
				webuiShowConfirm('Standby', webuiLH('sb.confirm', 'Put the box into standby?'), 'Standby',
					send, {variant: 'ok'});
				return;
			}
		} catch (e) {
			// a sign-in that was cancelled is not a yes to the key
			if (v1 && e.status === 403) return;
		}
	}
	return send();
}

// ═══ Remote Control Component ═══

/**
 * Returns generic remote control HTML.
 * @param {Object} [opts]
 * @param {string} [opts.onKey='webuiSendKey'] - Global function NAME called with a KEY_* string
 * @param {string} [opts.id='webui-remote'] - id of the returned root element
 *
 * Custom renderer contract (hardware-specific remote skins):
 *   Register with webuiRegisterRemoteRenderer(function(opts) { return html; })
 *   from an extension runtime script (manifest key entry_js) - never set
 *   WEBUI_CONFIG.remoteRenderer directly, the register call also re-renders
 *   already-built instances (registration timing is not guaranteed).
 *   The returned HTML must have a single root element with id=opts.id and
 *   data-rc-onkey=opts.onKey. Buttons call the opts.onKey global with a
 *   valid KEY_* name (generated rcsim.h table; note KEY_TIME, not
 *   KEY_TIMER). Opt into auto-repeat/press feedback via data-rc-repeat +
 *   data-rc-key attributes. Use real <button type="button"> elements with
 *   aria-labels on icon-only keys and aria-hidden SVGs.
 *   See extensions/README.md for a minimal renderer example.
 */
function webuiGetRemoteHTML(opts) {
	opts = opts || {};
	var onKey = opts.onKey || 'webuiSendKey';
	var id = opts.id || 'webui-remote';

	if (WEBUI_CONFIG.remoteRenderer) {
		return WEBUI_CONFIG.remoteRenderer(opts);
	}

	function btn(cls, key, label, title) {
		return '<button type="button" class="webui-rc-btn ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + escapeHtml(title || label || key) + '">' + escapeHtml(label) + '</button>';
	}
	function iconBtn(cls, key, title, svg) {
		var t = escapeHtml(title);
		return '<button type="button" class="webui-rc-btn ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + t + '" aria-label="' + t + '">' +
			svg.replace('<svg ', '<svg aria-hidden="true" focusable="false" ') + '</button>';
	}
	function repBtn(cls, key, label, title) {
		var t = escapeHtml(title || label || key);
		return '<button type="button" class="webui-rc-btn ' + cls + '" title="' + t + '" aria-label="' + t + '" data-rc-repeat="1" data-rc-key="' + key + '">' + escapeHtml(label) + '</button>';
	}
	function colorBtn(cls, key, title) {
		var t = escapeHtml(title);
		return '<button type="button" class="webui-rc-color ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + t + '" aria-label="' + t + '"></button>';
	}

	var svgUp = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="18 15 12 9 6 15"/></svg>';
	var svgDown = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="6 9 12 15 18 9"/></svg>';
	var svgLeft = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="15 18 9 12 15 6"/></svg>';
	var svgRight = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="9 18 15 12 9 6"/></svg>';
	var svgPower = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18.36 6.64a9 9 0 11-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/></svg>';
	var svgMute = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"/><line x1="23" y1="9" x2="17" y2="15"/><line x1="17" y1="9" x2="23" y2="15"/></svg>';

	return '<div class="webui-remote" id="' + id + '" data-rc-onkey="' + onKey + '">' +

		/* Zone A: Power / Mute */
		'<div class="webui-rc-row webui-rc-top">' +
			iconBtn('webui-rc-power', 'KEY_POWER', webuiL('rc.power', 'Power'), svgPower) +
			iconBtn('', 'KEY_MUTE', webuiL('rc.mute', 'Mute'), svgMute) +
		'</div>' +

		/* Zone B: Core thumb zone (VOL | D-Pad | CH) */
		'<div class="webui-rc-core">' +
			'<div class="webui-rc-rocker">' +
				'<span class="webui-rc-rocker-label">VOL</span>' +
				repBtn('webui-rc-rocker-top', 'KEY_VOLUMEUP', '+', webuiL('rc.vol_up', 'Volume up')) +
				repBtn('webui-rc-rocker-bot', 'KEY_VOLUMEDOWN', '\u2212', webuiL('rc.vol_down', 'Volume down')) +
			'</div>' +
			'<div class="webui-rc-dpad">' +
				'<div></div>' + iconBtn('webui-rc-arrow', 'KEY_UP', webuiL('rc.up', 'Up'), svgUp) + '<div></div>' +
				iconBtn('webui-rc-arrow', 'KEY_LEFT', webuiL('rc.left', 'Left'), svgLeft) +
				btn('webui-rc-ok', 'KEY_OK', 'OK', webuiL('rc.ok', 'OK')) +
				iconBtn('webui-rc-arrow', 'KEY_RIGHT', webuiL('rc.right', 'Right'), svgRight) +
				'<div></div>' + iconBtn('webui-rc-arrow', 'KEY_DOWN', webuiL('rc.down', 'Down'), svgDown) + '<div></div>' +
			'</div>' +
			'<div class="webui-rc-rocker">' +
				'<span class="webui-rc-rocker-label">CH</span>' +
				repBtn('webui-rc-rocker-top', 'KEY_PAGEUP', '+', webuiL('rc.ch_up', 'Channel up')) +
				repBtn('webui-rc-rocker-bot', 'KEY_PAGEDOWN', '\u2212', webuiL('rc.ch_down', 'Channel down')) +
			'</div>' +
		'</div>' +

		/* Zone C: Navigation */
		'<div class="webui-rc-row webui-rc-nav">' +
			btn('webui-rc-func', 'KEY_EXIT', 'Back', webuiL('rc.back', 'Back')) +
			btn('webui-rc-func', 'KEY_HOME', 'Home', webuiL('rc.home', 'Home')) +
			btn('webui-rc-func', 'KEY_SETUP', 'Menu', webuiL('rc.menu', 'Menu')) +
			btn('webui-rc-func', 'KEY_HELP', 'Info', webuiL('rc.info', 'Info')) +
		'</div>' +

		/* Zone D: Media */
		'<div class="webui-rc-row webui-rc-media">' +
			iconBtn('', 'KEY_REWIND', webuiL('rc.rewind', 'Rewind'), '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="19 20 9 12 19 4"/><polygon points="14 20 4 12 14 4"/></svg>') +
			iconBtn('', 'KEY_PLAY', webuiL('rc.play', 'Play'), '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21"/></svg>') +
			iconBtn('', 'KEY_FORWARD', webuiL('rc.forward', 'Forward'), '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 4 15 12 5 20"/><polygon points="10 4 20 12 10 20"/></svg>') +
			iconBtn('', 'KEY_STOP', webuiL('rc.stop', 'Stop'), '<svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="6" width="12" height="12"/></svg>') +
		'</div>' +

		/* Zone E: Colour keys (always visible) */
		'<div class="webui-rc-row webui-rc-colors">' +
			colorBtn('webui-rc-red', 'KEY_RED', webuiL('rc.red', 'Red')) +
			colorBtn('webui-rc-green', 'KEY_GREEN', webuiL('rc.green', 'Green')) +
			colorBtn('webui-rc-yellow', 'KEY_YELLOW', webuiL('rc.yellow', 'Yellow')) +
			colorBtn('webui-rc-blue', 'KEY_BLUE', webuiL('rc.blue', 'Blue')) +
		'</div>' +

		/* Zone F: EPG / Text / Fav / Timer (always visible) */
		'<div class="webui-rc-row webui-rc-nav">' +
			btn('webui-rc-func', 'KEY_EPG', 'EPG', webuiL('rc.epg', 'EPG')) +
			btn('webui-rc-func', 'KEY_TEXT', 'Text', webuiL('rc.text', 'Teletext')) +
			btn('webui-rc-func', 'KEY_FAVORITES', webuiL('rc.fav', 'Fav')) +
			btn('webui-rc-func', 'KEY_TIME', webuiL('rc.time', 'Timer')) +
		'</div>' +

		/* Numpad trigger — reveals the numpad sheet on every viewport */
		'<button type="button" class="webui-rc-btn webui-rc-sheet-trigger" aria-label="' + escapeHtml(webuiL('rc.numpad', 'Numpad')) + '" aria-expanded="false" onclick="webuiToggleRcSheet(\'' + id + '\')">123' +
			'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" aria-hidden="true" focusable="false"><polyline points="6 9 12 15 18 9"/></svg>' +
		'</button>' +

		/* Numpad sheet (bottom sheet on mobile, inline panel on desktop) */
		'<div class="webui-rc-sheet-backdrop" id="' + id + '-sheet" onclick="webuiToggleRcSheet(\'' + id + '\')">' +
			'<div class="webui-rc-sheet" onclick="event.stopPropagation()">' +
				'<div class="webui-rc-sheet-handle"></div>' +
				'<div class="webui-rc-numpad">' +
					btn('', 'KEY_1', '1') + btn('', 'KEY_2', '2') + btn('', 'KEY_3', '3') +
					btn('', 'KEY_4', '4') + btn('', 'KEY_5', '5') + btn('', 'KEY_6', '6') +
					btn('', 'KEY_7', '7') + btn('', 'KEY_8', '8') + btn('', 'KEY_9', '9') +
					'<div></div>' + btn('', 'KEY_0', '0') + '<div></div>' +
				'</div>' +
			'</div>' +
		'</div>' +

	'</div>';
}

/* Numpad sheet toggle. Works on every viewport: a bottom sheet on
   mobile, an inline panel on desktop (CSS decides the presentation via
   the media query; the JS only flips display). */
function webuiToggleRcSheet(id) {
	var backdrop = document.getElementById(id + '-sheet');
	if (!backdrop) return;
	var root = document.getElementById(id);
	var trigger = root ? root.querySelector('.webui-rc-sheet-trigger') : null;

	var isOpen = backdrop.style.display === 'flex';
	if (isOpen) {
		backdrop.style.display = 'none';
		if (trigger) trigger.setAttribute('aria-expanded', 'false');
		if (backdrop._escHandler) {
			document.removeEventListener('keydown', backdrop._escHandler);
			backdrop._escHandler = null;
		}
	} else {
		backdrop.style.display = 'flex';
		if (trigger) trigger.setAttribute('aria-expanded', 'true');
		/* Desktop: the sheet is an inline region inside a scrolling
		   panel (modal body / card body) - bring it into view */
		if (window.innerWidth > 768) backdrop.scrollIntoView({ block: 'nearest' });
		backdrop._escHandler = function(e) {
			if (e.key === 'Escape') webuiToggleRcSheet(id);
		};
		document.addEventListener('keydown', backdrop._escHandler);
		/* Init swipe-to-dismiss on handle */
		if (!backdrop._swipeInit) {
			backdrop._swipeInit = true;
			var panel = backdrop.querySelector('.webui-rc-sheet');
			var handle = backdrop.querySelector('.webui-rc-sheet-handle');
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
					if (diff > 80) webuiToggleRcSheet(id);
				});
			}
		}
	}
}

/* Long-press auto-repeat for VOL/CH buttons + visual press feedback */
function webuiInitRemoteRepeat(containerId) {
	var container = document.getElementById(containerId);
	if (!container) return;

	/* Delegated press feedback: pointerdown-driven class, because a quick
	   tap's :active phase is too short to be visible on touch. Purely
	   visual - never sends a key. */
	if (!container._pressInit) {
		container._pressInit = true;
		container.addEventListener('pointerdown', function(e) {
			var el = e.target.closest('.webui-rc-btn, .webui-rc-color');
			if (!el || !container.contains(el)) return;
			if (e.pointerType === 'touch' && navigator.vibrate) navigator.vibrate(10);
			/* Restart-safe: cancel the previous removal hooks before
			   re-triggering, then force a reflow so the animation restarts. */
			if (el._pressTimer) { clearTimeout(el._pressTimer); el._pressTimer = null; }
			if (el._pressEnd) { el.removeEventListener('animationend', el._pressEnd); el._pressEnd = null; }
			el.classList.remove('webui-rc-pressed');
			void el.offsetWidth;
			el.classList.add('webui-rc-pressed');
			var clear = function() {
				el.classList.remove('webui-rc-pressed');
				if (el._pressTimer) { clearTimeout(el._pressTimer); el._pressTimer = null; }
				if (el._pressEnd) { el.removeEventListener('animationend', el._pressEnd); el._pressEnd = null; }
			};
			el._pressEnd = clear;
			el.addEventListener('animationend', clear, { once: true });
			/* Fallback for prefers-reduced-motion, where animationend never fires */
			el._pressTimer = setTimeout(clear, 250);
		});
	}

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

/* Registry of rendered remote instances, keyed by HOST element id (the
   wrapper the remote is rendered into; opts.id names the inner root).
   Lets a late-registered skin renderer replace existing instances. */
var _webuiRemoteInstances = {};

function webuiRenderRemote(hostId, opts) {
	var host = document.getElementById(hostId);
	if (!host) {
		delete _webuiRemoteInstances[hostId];
		return;
	}
	opts = opts || {};
	host.innerHTML = webuiGetRemoteHTML(opts);
	webuiInitRemoteRepeat(opts.id || 'webui-remote');
	_webuiRemoteInstances[hostId] = opts;
}

/**
 * Registers a hardware-specific remote skin renderer (see the contract
 * at webuiGetRemoteHTML). Last registration wins; already-rendered
 * instances are re-rendered in place, an open remote modal stays open.
 */
function webuiRegisterRemoteRenderer(fn) {
	if (WEBUI_CONFIG.remoteRenderer && WEBUI_CONFIG.remoteRenderer !== fn)
		console.warn('webui: remote renderer already registered - overwriting (last one wins)');
	WEBUI_CONFIG.remoteRenderer = fn;

	var modal = document.getElementById('webui-remote-modal');
	if (modal && getComputedStyle(modal).display !== 'none') {
		/* Open modal: swap the panel in place; scroll lock and dialog
		   stay untouched. Restore focus if it sat in the replaced tree. */
		webuiRenderRemote('webui-remote-modal-host',
			_webuiRemoteInstances['webui-remote-modal-host'] ||
			{ id: 'webui-remote-modal-panel', onKey: _webuiRemoteModalOnKey || 'webuiSendKey' });
		if (!modal.contains(document.activeElement)) {
			var closeBtn = modal.querySelector('.webui-modal-close');
			if (closeBtn) closeBtn.focus();
		}
	} else if (modal) {
		/* Closed modal: drop it so the next open builds with the skin */
		if (modal._keyHandler) document.removeEventListener('keydown', modal._keyHandler);
		modal.remove();
		_webuiRemoteModalOnKey = null;
		delete _webuiRemoteInstances['webui-remote-modal-host'];
	}

	/* Re-render all remaining registered instances (covers the
	   screenshot page, whose remote is built before async discovery) */
	Object.keys(_webuiRemoteInstances).forEach(function(hostId) {
		if (hostId === 'webui-remote-modal-host') return;
		if (!document.getElementById(hostId)) {
			delete _webuiRemoteInstances[hostId];
			return;
		}
		webuiRenderRemote(hostId, _webuiRemoteInstances[hostId]);
	});
}

// Remote Modal
var _webuiRemoteModalOnKey = null;

function webuiShowRemote(opts) {
	opts = opts || {};
	var onKey = opts.onKey || 'webuiSendKey';
	var modal = document.getElementById('webui-remote-modal');

	// Rebuild modal if onKey changed
	if (modal && _webuiRemoteModalOnKey !== onKey) {
		if (modal._keyHandler) document.removeEventListener('keydown', modal._keyHandler);
		modal.remove();
		delete _webuiRemoteInstances['webui-remote-modal-host'];
		modal = null;
	}

	if (!modal) {
		_webuiRemoteModalOnKey = onKey;
		var html = '<div class="webui-remote-backdrop" id="webui-remote-modal" onclick="webuiCloseRemote(event)">' +
			'<div class="webui-remote-dialog" role="dialog" aria-modal="true" aria-labelledby="webui-remote-modal-title" onclick="event.stopPropagation()">' +
				'<div class="webui-remote-modal-header">' +
					'<span id="webui-remote-modal-title">' + WEBUI_ICONS.remote + ' ' + webuiLH('tools.remote_sub', 'Remote control') + '</span>' +
					_webuiCloseBtn('webuiCloseRemote()') +
				'</div>' +
				'<div id="webui-remote-modal-host"></div>' +
			'</div>' +
		'</div>';
		document.body.insertAdjacentHTML('beforeend', html);
		modal = document.getElementById('webui-remote-modal');
		webuiRenderRemote('webui-remote-modal-host', { id: 'webui-remote-modal-panel', onKey: onKey });
	}
	modal._opener = document.activeElement;
	modal.style.display = 'flex';
	document.body.classList.add('webui-remote-open');

	/* ESC with sheet priority + Tab focus trap. Registered before any
	   sheet ESC handler, so on Escape the sheet is closed here first and
	   its own listener is removed mid-dispatch (never runs for the same
	   event). The standalone remote (screenshot page) keeps relying on
	   the sheet's own handler. */
	if (!modal._keyHandler) {
		modal._keyHandler = function(e) {
			if (e.key === 'Escape') {
				var sheet = document.getElementById('webui-remote-modal-panel-sheet');
				if (sheet && sheet.style.display === 'flex') {
					webuiToggleRcSheet('webui-remote-modal-panel');
					return;
				}
				webuiCloseRemote();
				return;
			}
			if (e.key === 'Tab') {
				var items = Array.prototype.filter.call(
					modal.querySelectorAll('button'),
					function(b) { return b.getClientRects().length > 0; });
				if (!items.length) return;
				/* Programmatic focus() does not trigger :focus-visible, so
				   the ring would vanish exactly at the wrap point; mark the
				   target with a class that joins the focus-ring selector. */
				var kbdFocus = function(el) {
					el.focus();
					el.classList.add('webui-rc-kbd-focus');
					el.addEventListener('blur', function() {
						el.classList.remove('webui-rc-kbd-focus');
					}, { once: true });
				};
				var first = items[0];
				var last = items[items.length - 1];
				if (e.shiftKey && document.activeElement === first) {
					e.preventDefault();
					kbdFocus(last);
				} else if (!e.shiftKey && (document.activeElement === last || !modal.contains(document.activeElement))) {
					e.preventDefault();
					kbdFocus(first);
				}
			}
		};
		document.addEventListener('keydown', modal._keyHandler);
	}
	var closeBtn = modal.querySelector('.webui-modal-close');
	if (closeBtn) closeBtn.focus();
}

function webuiCloseRemote(event) {
	if (event && event.target !== event.currentTarget) return;
	var modal = document.getElementById('webui-remote-modal');
	if (modal) {
		modal.style.display = 'none';
		var sheet = modal.querySelector('.webui-rc-sheet-backdrop');
		if (sheet) {
			/* Clear the inline style instead of forcing display:none - the
			   desktop media query shows this block inline via display:flex,
			   and an inline none would stick on reopen (modal node persists). */
			sheet.style.display = '';
			if (sheet._escHandler) {
				document.removeEventListener('keydown', sheet._escHandler);
				sheet._escHandler = null;
			}
		}
		if (modal._keyHandler) {
			document.removeEventListener('keydown', modal._keyHandler);
			modal._keyHandler = null;
		}
		if (modal._opener && typeof modal._opener.focus === 'function') {
			modal._opener.focus();
		}
		modal._opener = null;
	}
	document.body.classList.remove('webui-remote-open');
}

// Also escapes quotes so the result is safe in attribute values.
// Loads a script and answers once it has run, or fails where it cannot be
// read.
function webuiLoadScript(src) {
	return new Promise(function(done, failed) {
		var s = document.createElement('script');
		s.src = src;
		s.onload = function() { done(); };
		s.onerror = function() { failed(new Error('cannot load ' + src)); };
		document.head.appendChild(s);
	});
}

function escapeHtml(text) {
	if (!text) return '';
	const div = document.createElement('div');
	div.textContent = text;
	return div.innerHTML.replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

// For a string inside a quoted JavaScript string; put into an attribute it
// needs escapeHtml as well. The pages pass escapeHtml(JSON.stringify(value)).
function escapeJs(text) {
	if (!text) return '';
	return text.replace(/\\/g, '\\\\').replace(/'/g, "\\'").replace(/"/g, '\\"');
}

// Generic modal on the EPG-modal backdrop pattern. Reuses the element
// per id and replaces title/body on every call (reset on reopen).
function webuiModal(id, title, bodyHtml) {
	var backdrop = document.getElementById(id);
	if (!backdrop) {
		backdrop = document.createElement('div');
		backdrop.id = id;
		backdrop.className = 'webui-epg-modal-backdrop';
		backdrop.innerHTML = '<div class="webui-epg-modal" onclick="event.stopPropagation()">' +
			'<div class="webui-epg-modal-header"><h3></h3>' +
			_webuiCloseBtn('webuiCloseModal(\'' + id + '\')') + '</div>' +
			'<div class="webui-epg-modal-body"></div></div>';
		backdrop.addEventListener('click', function(e) {
			if (e.target === backdrop) webuiCloseModal(id);
		});
		document.body.appendChild(backdrop);
	}
	backdrop.querySelector('.webui-epg-modal-header h3').textContent = title;
	backdrop.querySelector('.webui-epg-modal-body').innerHTML = bodyHtml;
	backdrop.classList.add('open');
	document.body.style.overflow = 'hidden';
	return backdrop;
}

function webuiCloseModal(id) {
	var backdrop = document.getElementById(id);
	if (backdrop) backdrop.classList.remove('open');
	document.body.style.overflow = '';
}

// Companion des geteilten .webui-file-Controls (webui.css): zeigt den
// gewaehlten Dateinamen im Control an — onchange="webuiFileName(this)"
function webuiFileName(inp) {
	var box = inp.closest('.webui-file');
	var n = box && box.querySelector('.webui-file-name');
	if (n) n.textContent = (inp.files && inp.files.length) ? inp.files[0].name : '–';
}

// ═══ Verzeichnis-/Datei-Picker (nutzt FileCGI action=list) ═══
// Elternpfad von p (POSIX), '/' bleibt '/'
function webuiParentPath(p) {
	if (!p || p === '/') return '/';
	p = p.replace(/\/+$/, '');
	var i = p.lastIndexOf('/');
	return i <= 0 ? '/' : p.substring(0, i);
}

var _webuiPickerState = null;

// webuiShowPathPicker(startPath, onPick, opts)
//   onPick(path) erhaelt den gewaehlten Ordner (oder mit opts.filesToo:true
//   auch eine Datei). opts.filesToo zeigt zusaetzlich Dateien an.
function webuiShowPathPicker(startPath, onPick, opts) {
	opts = opts || {};
	_webuiPickerState = {onPick: onPick, filesToo: !!opts.filesToo, path: startPath || '/'};
	webuiModal('webui-picker', webuiL('picker.title', 'Choose folder'),
		'<div id="webui-picker-crumb" style="font-size:0.8rem;color:var(--pico-muted-color);margin-bottom:0.5rem;word-break:break-all"></div>' +
		'<ul id="webui-picker-list" style="list-style:none;margin:0;padding:0;max-height:50vh;overflow:auto"></ul>' +
		'<div class="webui-form-row" style="margin-top:0.8rem"><label></label>' +
		'<button type="button" class="webui-btn primary" onclick="webuiPickerChoose()">' +
		webuiLH('picker.choose_folder', 'Choose this folder') + '</button></div>');
	webuiPickerLoad(_webuiPickerState.path);
}

/* Over /api/v1 where the page loads webui-v1.js: the picker then reaches the
   box's roots and what lies below them, as the file manager does. Its top
   level ('') is the list of roots, and a start outside them leads there. */
async function _webuiPickerRows(path) {
	if (path) {
		try {
			var listed = await webuiV1('GET', '/storage/files', {query: {path: path}});
			var roots0 = await webuiV1('GET', '/storage/roots');
			var isRoot = ((roots0 && roots0.items) || []).some(function(r) { return r.present && r.id === path; });
			return {path: path, parent: isRoot ? '' : webuiParentPath(path),
				entries: ((listed && listed.items) || []).map(function(e) {
					return {name: e.id, dir: e.kind === 'dir', full: path.replace(/\/+$/, '') + '/' + e.id};
				})};
		} catch (e) {
			if (e.code !== 'outside-roots' && e.code !== 'no-such-name') throw e;
		}
	}
	var roots = await webuiV1('GET', '/storage/roots');
	return {path: '', parent: null, entries: ((roots && roots.items) || []).filter(function(r) { return r.present; })
		.map(function(r) { return {name: r.id, dir: true, full: r.id}; })};
}

async function webuiPickerLoad(path) {
	var listEl = document.getElementById('webui-picker-list');
	var crumbEl = document.getElementById('webui-picker-crumb');
	if (!listEl) return;
	listEl.innerHTML = '<li style="padding:0.5rem;color:var(--pico-muted-color)">' + webuiLH('msg.loading', 'Loading...') + '</li>';
	// both: a page that loads webui-v1.js without webui-dom.js keeps the old list
	if (typeof webuiV1 === 'function' && typeof webuiEl === 'function') {
		var rows;
		try {
			rows = await _webuiPickerRows(path);
		} catch (e) {
			webuiShowToast(webuiL('picker.not_readable', 'Directory not readable') + ': ' + e.message, true);
			return;
		}
		_webuiPickerState.path = rows.path;
		if (crumbEl) crumbEl.textContent = rows.path || webuiL('picker.roots', 'Permitted folders');
		var chooseBtn = document.querySelector('#webui-picker .webui-btn.primary');
		if (chooseBtn) chooseBtn.disabled = !rows.path;	// the list of roots is no folder
		// Rows as elements with their handlers, so a folder name is text
		// whatever it holds (webui-dom.js comes with webui-v1.js)
		var row = function(label, onClick) {
			return webuiEl('li', {}, webuiEl('button', {type: 'button', className: 'webui-btn',
				style: {width: '100%', 'text-align': 'left', border: 'none'}, on: {click: onClick}}, label));
		};
		var items = [];
		if (rows.parent !== null)
			items.push(row('↑ ..', function() { webuiPickerLoad(rows.parent); }));
		rows.entries.forEach(function(e) {
			if (!e.dir && !_webuiPickerState.filesToo) return;
			items.push(row((e.dir ? '📁' : '📄') + ' ' + e.name,
				function() { if (e.dir) webuiPickerLoad(e.full); else webuiPickerPickFile(e.full); }));
		});
		if (!items.length)
			items.push(webuiEl('li', {style: {padding: '0.5rem', color: 'var(--pico-muted-color)'}}, webuiL('picker.empty', 'empty')));
		listEl.replaceChildren.apply(listEl, items);
		return;
	}
	var data;
	try {
		data = await webuiApi('file', {action: 'list', path: path});
	} catch (e) {
		webuiShowToast(webuiL('picker.not_readable', 'Directory not readable') + ': ' + e.message, true);
		return;
	}
	_webuiPickerState.path = path;
	if (crumbEl) crumbEl.textContent = path;
	var entries = (data.data && data.data.filelist) || data.filelist || [];
	var html = '';
	if (path !== '/')
		html += '<li><button type="button" class="webui-btn" style="width:100%;text-align:left;border:none" ' +
			'onclick="webuiPickerLoad(' + escapeHtml(JSON.stringify(webuiParentPath(path))) + ')">↑ ..</button></li>';
	entries.forEach(function(e) {
		if (!e.name || e.name === '.' || e.name === '..') return;
		var isDir = (e.type_str === 'dir' || e.type === 4 || e.type === '4');
		if (!isDir && !_webuiPickerState.filesToo) return;
		var full = e.fullname || (path.replace(/\/+$/, '') + '/' + e.name);
		// escaped as a whole: the attribute is decoded before the handler is
		// read, so an & in a name must not become a quote there
		var arg = escapeHtml(JSON.stringify(full));
		var icon = isDir ? '📁' : '📄';
		var action = isDir ? 'webuiPickerLoad(' + arg + ')' : 'webuiPickerPickFile(' + arg + ')';
		html += '<li><button type="button" class="webui-btn" style="width:100%;text-align:left;border:none" onclick="' + action + '">' +
			icon + ' ' + escapeHtml(e.name) + '</button></li>';
	});
	listEl.innerHTML = html || '<li style="padding:0.5rem;color:var(--pico-muted-color)">' + webuiLH('picker.empty', 'empty') + '</li>';
}

function webuiPickerChoose() {
	var st = _webuiPickerState;
	webuiCloseModal('webui-picker');
	if (st && typeof st.onPick === 'function') st.onPick(st.path);
}

function webuiPickerPickFile(full) {
	var st = _webuiPickerState;
	webuiCloseModal('webui-picker');
	if (st && typeof st.onPick === 'function') st.onPick(full);
}

// The EPG dialog, recording and streaming of a channel live in webui-v1.js,
// which the pages offering them load after this file.

// Timer constants — shared across all pages. Interne Typnamen (EN,
// stabil fuer Logik); Anzeige-Labels kommen lokalisiert via webuiL —
// The labels are filled once the texts are in (webuiFillTimerLabels, run
// before any page's own start); a page may keep a reference to the objects.
const WEBUI_TIMER_TYPES = {
	1: 'Shutdown', 2: 'Nextprogram', 3: 'Zapto', 4: 'Standby',
	5: 'Record', 6: 'Remind', 7: 'Sleeptimer', 8: 'Plugin', 9: 'Immediate Record'
};

const WEBUI_TIMER_TYPE_LABELS = {};
const WEBUI_TIMER_STATE_LABELS = {};
const WEBUI_REPEAT_LABELS = {};

function webuiFillTimerLabels() {
	Object.assign(WEBUI_TIMER_TYPE_LABELS, {
		1: webuiL('timer.type.1', 'Shutdown'), 3: webuiL('timer.type.3', 'Zap'),
		4: webuiL('timer.type.4', 'Standby'), 5: webuiL('timer.type.5', 'Record'),
		6: webuiL('timer.type.6', 'Reminder'), 7: webuiL('timer.type.7', 'Sleep timer'),
		8: webuiL('timer.type.8', 'Plugin'), 9: webuiL('timer.type.9', 'Instant record')
	});
	Object.assign(WEBUI_TIMER_STATE_LABELS, {
		0: webuiL('timer.state.0', 'Scheduled'), 1: webuiL('timer.state.1', 'Announced'),
		2: webuiL('timer.state.2', 'Active'), 3: webuiL('timer.state.3', 'Finished'),
		4: webuiL('timer.state.4', 'Cancelled')
	});
	Object.assign(WEBUI_REPEAT_LABELS, {
		'once': webuiL('timer.rep.once', 'Once'), 'daily': webuiL('timer.rep.daily', 'Daily'),
		'weekly': webuiL('timer.rep.weekly', 'Weekly'), 'biweekly': webuiL('timer.rep.biweekly', 'Every 2 weeks'),
		'fourweekly': webuiL('timer.rep.fourweekly', 'Every 4 weeks'),
		'monthly': webuiL('timer.rep.monthly', 'Monthly'), 'byeventdescription': webuiL('timer.rep.bydesc', 'By description')
	});
}
webuiFillTimerLabels();
// filled again in the page's language before any page's own start
if (window.webuiTexts)
	window.webuiTexts = window.webuiTexts.then(function(code) { webuiFillTimerLabels(); return code; });

// Timer helper functions — shared across all pages
function webuiGetTimerType(timer) {
	var n = parseInt(timer.type_number) || 0;
	return WEBUI_TIMER_TYPE_LABELS[n] || WEBUI_TIMER_TYPES[n] || 'Timer';
}

function webuiGetTimerAlarm(timer) {
	if (Array.isArray(timer.alarm) && timer.alarm.length > 0)
		return timer.alarm[0];
	return timer.alarm && timer.alarm.normal ? timer.alarm.normal : {};
}

function webuiGetTimerStop(timer) {
	if (Array.isArray(timer.stop) && timer.stop.length > 0)
		return timer.stop[0];
	return timer.stop || {};
}

function webuiGetTimerStopSec(timer) {
	var stop = webuiGetTimerStop(timer);
	return parseInt(stop.digits) || 0;
}

function webuiResolveRepeatText(raw) {
	if (!raw) return 'Einmalig';
	var m = raw.match(/\{=L:timerlist\.repeat\.(\w+)=\}/);
	if (m) return WEBUI_REPEAT_LABELS[m[1]] || m[1];
	return WEBUI_REPEAT_LABELS[raw.toLowerCase()] || raw;
}

function webuiGetTimerIconHtml(type, cssPrefix) {
	var cls = cssPrefix || 'webui-timer-type-icon';
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
function webuiParseTimerList(data) {
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
let _webuiBouquetCache = null;
async function webuiFetchBouquets() {
	if (_webuiBouquetCache) return _webuiBouquetCache;
	const data = await webuiApi('bouquets');
	const root = data.data || data;
	_webuiBouquetCache = root.bouquets || [];
	return _webuiBouquetCache;
}

// Fetch channels for a bouquet — cached per bouquet number
let _webuiChannelCache = {};
async function webuiFetchChannels(bouquetNr) {
	if (_webuiChannelCache[bouquetNr]) return _webuiChannelCache[bouquetNr];
	const data = await webuiApi('bouquet', {bouquet: bouquetNr});
	const root = data.data || data;
	_webuiChannelCache[bouquetNr] = root.channels || [];
	return _webuiChannelCache[bouquetNr];
}

// Confirm dialog — shared across all pages
// Returns a Promise that resolves when confirmed, rejects when cancelled
// onConfirm: async function(btn) — called when user clicks confirm, btn shows loading state
// opts.variant 'ok': Akzent-Knopf fuer nicht-destruktive Rueckfragen
// (Standby, Neustart-Frage o.ae.); Default bleibt rot (destruktiv)
// title and confirmLabel are text; message is markup, and a caller escapes
// what it puts in (webuiLH for a text, escapeHtml for a name).
function webuiShowConfirm(title, message, confirmLabel, onConfirm, opts) {
	var confirmClass = (opts && opts.variant === 'ok') ? 'confirm-ok' : 'confirm-delete';
	var backdrop = document.createElement('div');
	backdrop.className = 'webui-confirm-backdrop';
	backdrop.onclick = function(e) {
		if (e.target === backdrop) backdrop.remove();
	};
	backdrop.innerHTML =
		'<div class="webui-confirm-dialog">' +
			'<h4>' + escapeHtml(title) + '</h4>' +
			'<p>' + message + '</p>' +
			'<div class="webui-confirm-actions">' +
				'<button class="confirm-cancel">' + webuiLH('msg.cancel', 'Cancel') + '</button>' +
				'<button class="' + confirmClass + '">' + escapeHtml(confirmLabel) + '</button>' +
			'</div>' +
		'</div>';
	backdrop.querySelector('.confirm-cancel').onclick = function() { backdrop.remove(); };
	backdrop.querySelector('.' + confirmClass).onclick = async function() {
		var btn = this;
		btn.textContent = '...';
		btn.disabled = true;
		try {
			await onConfirm(btn);
			backdrop.remove();
		} catch (e) {
			console.error('Confirm action failed:', e);
			btn.textContent = webuiL('msg.error', 'Error');
			setTimeout(function() { backdrop.remove(); }, 1500);
		}
	};
	document.body.appendChild(backdrop);
}

// Prompt dialog — shared: Textfeld mit OK/Abbrechen im Look des
// Confirm-Dialogs, Ersatz fuer natives prompt(). onOk(value) darf
// async sein; Enter bestaetigt, Escape/Backdrop bricht ab.
function webuiShowPrompt(title, label, value, onOk) {
	var backdrop = document.createElement('div');
	backdrop.className = 'webui-confirm-backdrop';
	backdrop.onclick = function(e) {
		if (e.target === backdrop) backdrop.remove();
	};
	backdrop.innerHTML =
		'<div class="webui-confirm-dialog">' +
			'<h4></h4>' +
			'<label class="webui-prompt-label"></label>' +
			'<input type="text" class="webui-prompt-input" maxlength="255">' +
			'<div class="webui-confirm-actions">' +
				'<button class="confirm-cancel">' + webuiLH('msg.cancel', 'Cancel') + '</button>' +
				'<button class="confirm-ok">OK</button>' +
			'</div>' +
		'</div>';
	backdrop.querySelector('h4').textContent = title;
	var lab = backdrop.querySelector('.webui-prompt-label');
	if (label) lab.textContent = label; else lab.remove();
	var inp = backdrop.querySelector('.webui-prompt-input');
	inp.value = value || '';
	var ok = async function(btn) {
		// Enter while the first OK is still running would send it twice
		if (btn.disabled) return;
		if (!inp.value.trim()) { inp.focus(); return; }
		btn.disabled = true;
		try {
			await onOk(inp.value.trim());
			backdrop.remove();
		} catch (e) {
			console.error('Prompt action failed:', e);
			btn.textContent = webuiL('msg.error', 'Error');
			setTimeout(function() { backdrop.remove(); }, 1500);
		}
	};
	backdrop.querySelector('.confirm-cancel').onclick = function() { backdrop.remove(); };
	backdrop.querySelector('.confirm-ok').onclick = function() { ok(this); };
	inp.onkeydown = function(e) {
		if (e.key === 'Enter') ok(backdrop.querySelector('.confirm-ok'));
		if (e.key === 'Escape') backdrop.remove();
	};
	document.body.appendChild(backdrop);
	inp.focus();
	inp.select();
}

// Message popover — send ticker or popup messages to the box
function webuiShowMessageDialog() {
	var pop = document.getElementById('webui-message-pop');
	if (pop) { pop.classList.toggle('open'); return; }

	pop = document.createElement('div');
	pop.id = 'webui-message-pop';
	pop.className = 'webui-popover open';
	pop.innerHTML =
		'<button type="button" class="webui-popover-close" onclick="document.getElementById(\'webui-message-pop\').classList.remove(\'open\')" aria-label="Close">\u00d7</button>' +
		'<form onsubmit="return false">' +
			'<input type="text" id="webui-msg-text" placeholder="' + webuiLH('tools.message_ph', 'Enter message\u2026') + '" maxlength="255">' +
			'<div class="webui-popover-actions">' +
				'<small id="webui-msg-result">\u00a0</small>' +
				'<button type="button" class="webui-btn" onclick="webuiSendMessage(\'nmsg\')">' + webuiLH('tools.message', 'Message') + '</button>' +
				'<button type="button" class="webui-btn primary" onclick="webuiSendMessage(\'popup\')">Popup</button>' +
			'</div>' +
		'</form>';
	document.body.appendChild(pop);

	var input = pop.querySelector('#webui-msg-text');
	input.onkeydown = function(e) { if (e.key === 'Enter') webuiSendMessage('nmsg'); };
	input.focus();

	// Close on outside click
	setTimeout(function() {
		document.addEventListener('click', function close(e) {
			if (!pop.contains(e.target) && !e.target.closest('[onclick*="webuiShowMessageDialog"]')) {
				pop.classList.remove('open');
				document.removeEventListener('click', close);
			}
		});
	}, 10);
}

async function webuiSendMessage(mode) {
	var input = document.getElementById('webui-msg-text');
	var text = input.value.trim();
	if (!text) return;

	var result = document.getElementById('webui-msg-result');
	try {
		if (typeof webuiV1 === 'function') {
			// nmsg: a hint that goes by itself; popup: a box that waits
			await webuiV1('POST', '/osd/message', {body: {text: text, kind: mode === 'popup' ? 'box' : 'hint'}});
		} else {
			var params = {};
			params[mode] = text;
			await webuiApi('message', params);
		}
		result.textContent = 'Gesendet';
		result.style.color = 'var(--webui-success)';
		input.value = '';
		setTimeout(function() { result.textContent = '\u00a0'; result.style.color = ''; }, 2500);
	} catch (e) {
		result.textContent = webuiL('msg.error', 'Error') + ': ' + e.message;
		result.style.color = 'var(--webui-danger)';
	}
}

// Format helpers (shared across pages)
function webuiFormatKB(kb) {
	if (kb < 1024) return kb + ' KB';
	if (kb < 1048576) return (kb / 1024).toFixed(1) + ' MB';
	return (kb / 1048576).toFixed(1) + ' GB';
}

function webuiFormatUptime(seconds) {
	var days = Math.floor(seconds / 86400);
	var hours = Math.floor((seconds % 86400) / 3600);
	var mins = Math.floor((seconds % 3600) / 60);
	if (days > 0) return days + 'd ' + hours + 'h';
	if (hours > 0) return hours + 'h ' + mins + 'm';
	return mins + 'm';
}

// Toast notification
function webuiShowToast(message, isError) {
	// Remove existing toast
	const existing = document.getElementById('webui-toast');
	if (existing) existing.remove();

	const toast = document.createElement('div');
	toast.id = 'webui-toast';
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
function webuiShowAbout() {
	const modal = document.getElementById('webui-about-modal');
	if (!modal) return;

	// Load data from API (versions and links)
	webuiSystemInfoOnce()
		.then(sys => webuiVersions(sys))
		.then(data => {
			const setEl = (id, val) => {
				const el = document.getElementById(id);
				if (el && val) el.textContent = val;
			};
			setEl('about-webui', data.webui ? data.webui + (data.webui_date ? ' (' + data.webui_date + ')' : '') : '--');
			setEl('about-neutrino', data.neutrino || '--');
			setEl('about-api', data.api || '--');

			// Build links from API data
			const linksEl = document.getElementById('about-links');
			if (linksEl && data.links) {
				let html = '';
				var link = function(url, text) {
					return /^https?:\/\//.test(url) ? '<a href="' + escapeHtml(url) + '" target="_blank" rel="noopener">' + text + '</a>' : '';
				};
				html += link(data.links.homepage, 'Homepage') + link(data.links.docs, 'Dokumentation') +
					link(data.links.forum, 'Forum');
				linksEl.innerHTML = html || '<span class="webui-about-label">' + webuiLH('about.no_links', 'No links available') + '</span>';
			}

			// Update copyright from API
			if (data.copyright && data.copyright.text) {
				setEl('about-copyright', data.copyright.text);
				setEl('webui-footer-copyright', data.copyright.text);
			}
		})
		.catch(() => {});

	// the box and its system, over /api/v1/system/info
	webuiSystemInfoOnce()
		.then(data => {
			if (!data) return;
			const boxEl = document.getElementById('about-box');
			if (boxEl) boxEl.textContent = (data.box.vendor && data.box.name) ? data.box.vendor + ' ' + data.box.name : data.box.name || '--';
			const archEl = document.getElementById('about-arch');
			if (archEl) archEl.textContent = data.box.arch || '--';
			const osEl = document.getElementById('about-os');
			if (osEl) osEl.textContent = data.os || '--';
		})
		.catch(() => {});

	modal.classList.add('open');
	document.body.style.overflow = 'hidden';
}

function webuiCloseAbout(event) {
	if (event && event.target !== event.currentTarget) return;
	const modal = document.getElementById('webui-about-modal');
	if (modal) modal.classList.remove('open');
	document.body.style.overflow = '';
}

// ═══ Extension-Loader (WORK-146 Spec §4) ═══
// Konvention: <webroot>/extensions/<name>/manifest.json mit
//   {"name","version","slot":"nav|tools|dashboard|page","entry","description"?}
// Schaltung: webui.conf ext_<name>_enabled=1, optional ext_<name>_slot
// (Slot-Override). Which are installed comes from GET /api/v1/webui/extensions,
// the switches from GET /api/v1/webui/settings, both readable without a
// sign-in from the home network; a page that may read neither shows none.
var _webuiExtensions = null;		// the list, once found
var _webuiExtensionsAsked = null;	// the finding, while and after it runs

// The interface's own pages a start page can be: page, text key, English.
var WEBUI_START_PAGES = [
	['dashboard.html', 'nav.dashboard', 'Dashboard'],
	['bouquets.html', 'nav.bouquets', 'Bouquets'],
	['epg.html', 'nav.epg', 'EPG'],
	['timer.html', 'nav.timer', 'Timer'],
	['recordings.html', 'nav.recordings', 'Recordings'],
	['tools.html', 'nav.tools', 'Tools'],
	['settings.html', 'nav.settings', 'Settings'],
	['files.html', 'tools.files', 'Files'],
	['sysinfo.html', 'tools.sysinfo', 'System information'],
	['screenshot.html', 'tools.screenshot', 'Screenshot'],
	['bouquet-editor.html', 'tools.bqedit', 'Bouquet editor']
];

/* What a start page can be, for a choice: the pages above and the page of
   every extension that is switched on (webuiDiscoverExtensions must have
   run), as {v: page, l: label}, both text. index.js holds a stored start
   page to the same. */
function webuiStartPages() {
	var out = WEBUI_START_PAGES.map(function(p) {
		return {v: p[0], l: webuiL(p[1], p[2])};
	});
	(_webuiExtensions || []).forEach(function(m) {
		if (m.enabled && m.url)
			out.push({v: m.url.replace(/^\//, ''), l: m.name});
	});
	return out;
}

async function _webuiV1Read(path) {
	var r = await fetch('/api/v1' + path, {credentials: 'same-origin', cache: 'no-store'});
	return r.ok ? r.json() : null;
}

/* Asked once per page: whoever asks while the answer is on its way waits for
   the same answer instead of taking a list still being filled. */
function webuiDiscoverExtensions() {
	if (!_webuiExtensionsAsked)
		_webuiExtensionsAsked = _webuiFindExtensions().then(function(found) {
			_webuiExtensions = found;
			return found;
		});
	return _webuiExtensionsAsked;
}

// Asked anew by the next webuiDiscoverExtensions (an extension was installed,
// taken out or switched).
function webuiForgetExtensions() {
	_webuiExtensions = null;
	_webuiExtensionsAsked = null;
}

async function _webuiFindExtensions() {
	var found = [];
	try {
		var list = await _webuiV1Read('/webui/extensions');
		var entries = (list && list.items) || [];
		if (!entries.length) return found;
		var settings = await _webuiV1Read('/webui/settings');
		var yCfg = {};
		((settings && settings.items) || []).forEach(function(it) { yCfg[it.key] = it.value; });
		for (var i = 0; i < entries.length; i++) {
			var e = {name: entries[i].dir};
			try {
				// no-cache: revalidieren, sonst ueberlebt ein altes Manifest
				// im Browser-Cache jede Aenderung auf der Box
				var mResp = await fetch('/extensions/' + encodeURIComponent(e.name) + '/manifest.json', {cache: 'no-cache'});
				if (!mResp.ok) continue;
				var m = await mResp.json();
				if (!m || !m.name) continue;
				// entry is mandatory unless the extension only ships a
				// settings category (settings-only) or only a runtime
				// script (script-only, entry_js)
				if (!m.entry && !m.entry_js && !(m.settings && m.settings.sections)) continue;
				m.dir = e.name;
				m.scripts = entries[i].scripts || [];
				var ckBase = 'ext_' + m.dir;
				m.enabled = (yCfg[ckBase + '_enabled'] === '1');
				m.slot = yCfg[ckBase + '_slot'] || m.slot || 'tools';
				m.url = m.entry ? ('/extensions/' + encodeURIComponent(m.dir) + '/' + encodeURIComponent(m.entry)) : '';
				// Load the language pack for enabled extensions only;
				// reserved keys translate name/description
				if (m.enabled) {
					var xl = await webuiExtLangLoad(m.dir);
					if (xl['manifest.name']) m.name = xl['manifest.name'];
					if (xl['manifest.description']) m.description = xl['manifest.description'];
				}
				found.push(m);
			} catch (ignore) {}
		}
	} catch (ignore) {}
	return found;
}

var _webuiExtInjected = false;

async function webuiInjectExtensions(pageName) {
	// Nur einmal pro Seitenaufruf injizieren, egal ueber welchen Init-Pfad
	if (_webuiExtInjected) return;
	_webuiExtInjected = true;
	var exts = await webuiDiscoverExtensions();

	// Runtime scripts (entry_js): loaded for every enabled extension on
	// every modern page, independent of slot/url (the slot only governs
	// UI links). async=false keeps execution in insertion order; sorting
	// by dir makes the registration order deterministic, so last-wins
	// between two skins does not depend on network timing.
	exts.slice().sort(function(a, b) {
		return a.dir < b.dir ? -1 : (a.dir > b.dir ? 1 : 0);
	}).forEach(function(m) {
		if (!m.enabled || !m.entry_js) return;
		var s = document.createElement('script');
		s.async = false;
		s.src = '/extensions/' + encodeURIComponent(m.dir) + '/' +
			encodeURIComponent(m.entry_js) + '?v=' + encodeURIComponent(m.version || '0');
		document.head.appendChild(s);
	});

	exts.forEach(function(m) {
		if (!m.enabled || !m.url) return; // settings-only hat keine Seite
		if (m.slot === 'nav') {
			var ul = document.querySelector('.webui-nav-links');
			if (ul) {
				var li = document.createElement('li');
				li.innerHTML = '<a href="' + escapeHtml(m.url) + '" data-page="ext-' + escapeHtml(m.dir) + '">' + escapeHtml(m.name) + '</a>';
				ul.appendChild(li);
			}
		} else if (m.slot === 'tools') {
			var grid = document.querySelector('.webui-tools-grid');
			if (grid) {
				var a = document.createElement('a');
				a.href = m.url;
				a.className = 'webui-tool-card';
				a.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2l3 6 6 1-4.5 4.4 1 6.6-5.5-3-5.5 3 1-6.6L3 9l6-1z"/></svg>' +
					'<span>' + escapeHtml(m.name) + '</span><small>' + escapeHtml(m.description || webuiL('ext.extension', 'Extension')) + '</small>';
				grid.appendChild(a);
			}
		} else if (m.slot === 'dashboard') {
			var dash = document.querySelector('.webui-dashboard');
			if (dash) {
				var card = document.createElement('div');
				card.className = 'webui-card';
				card.innerHTML = '<h3>' + escapeHtml(m.name) + '</h3>' +
					'<p><a href="' + escapeHtml(m.url) + '">' + escapeHtml(m.description || webuiL('ext.open', 'Open extension')) + '</a></p>';
				dash.appendChild(card);
			}
		}
		// slot 'page': keine Injektion, erreichbar ueber die
		// Erweiterungs-Verwaltung oder direkten Link
	});
}

// Pull-to-refresh
let webuiRefreshCallback = null;
let webuiPullStartY = 0;
let webuiPullCurrentY = 0;
let webuiIsPulling = false;
let webuiIsRefreshing = false;
const WEBUI_PULL_THRESHOLD = 60;
const WEBUI_PULL_MAX = 80;

function webuiSetRefreshCallback(fn) {
	webuiRefreshCallback = fn;
}

function webuiCreatePullIndicator() {
	if (document.getElementById('webui-pull-indicator')) return;

	const indicator = document.createElement('div');
	indicator.id = 'webui-pull-indicator';
	indicator.className = 'webui-pull-indicator';
	indicator.innerHTML = `
		<div class="webui-pull-icon">
			<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
				<path d="M12 5v14M5 12l7 7 7-7"/>
			</svg>
		</div>
		<span class="webui-pull-text"></span>
	`;
	document.body.insertBefore(indicator, document.body.firstChild);
}

function webuiUpdatePullIndicator(distance) {
	const indicator = document.getElementById('webui-pull-indicator');
	if (!indicator) return;

	const progress = Math.min(distance / WEBUI_PULL_THRESHOLD, 1);
	const translateY = Math.min(distance, WEBUI_PULL_MAX) - 50;
	const textEl = indicator.querySelector('.webui-pull-text');
	const iconEl = indicator.querySelector('.webui-pull-icon');

	indicator.style.transform = `translateY(${translateY}px)`;
	indicator.classList.toggle('ready', distance >= WEBUI_PULL_THRESHOLD);

	if (iconEl) {
		iconEl.style.transform = `rotate(${progress * 180}deg)`;
	}

	if (textEl) {
		textEl.textContent = distance >= WEBUI_PULL_THRESHOLD ? 'Loslassen' : 'Ziehen...';
	}
}

function webuiShowRefreshing() {
	const indicator = document.getElementById('webui-pull-indicator');
	if (!indicator) return;

	indicator.classList.remove('ready');
	indicator.classList.add('refreshing');
	indicator.style.transform = 'translateY(10px)';

	const textEl = indicator.querySelector('.webui-pull-text');
	if (textEl) textEl.textContent = 'Aktualisiere...';
}

function webuiHidePullIndicator() {
	const indicator = document.getElementById('webui-pull-indicator');
	if (!indicator) return;

	indicator.classList.remove('ready', 'refreshing');
	indicator.style.transform = 'translateY(-50px)';

	const iconEl = indicator.querySelector('.webui-pull-icon');
	if (iconEl) iconEl.style.transform = 'rotate(0deg)';
}

// Check if at scroll top - can be overridden by pages with custom scroll containers
function webuiDefaultIsAtScrollTop() {
	// If page defines custom check, use it
	if (typeof window.webuiIsAtScrollTop === 'function') {
		return window.webuiIsAtScrollTop();
	}
	// Default: check window scroll and main element
	if (window.scrollY > 5) return false;
	const main = document.querySelector('main');
	if (main && main.scrollTop > 5) return false;
	return true;
}

function webuiPullTouchStart(e) {
	if (webuiIsRefreshing) return;
	if (window.innerWidth > 768) return;
	if (!webuiDefaultIsAtScrollTop()) return;

	webuiPullStartY = e.touches[0].clientY;
	webuiPullCurrentY = webuiPullStartY;
	webuiIsPulling = false;
}

function webuiPullTouchMove(e) {
	if (webuiIsRefreshing) return;
	if (window.innerWidth > 768) return;
	if (webuiPullStartY === 0) return;

	const touch = e.touches[0];
	const deltaY = touch.clientY - webuiPullStartY;
	const deltaX = Math.abs(touch.clientX - (webuiPullCurrentY ? touch.clientX : 0));

	// Only pull down, not up, and only if mostly vertical
	if (deltaY > 10 && webuiDefaultIsAtScrollTop() && deltaX < 30) {
		webuiIsPulling = true;
		webuiPullCurrentY = touch.clientY;

		// Dampen the pull distance
		const pullDistance = Math.pow(deltaY, 0.8);
		webuiUpdatePullIndicator(pullDistance);

		// Prevent default scroll to enable pull effect
		if (deltaY > 0 && webuiDefaultIsAtScrollTop()) {
			e.preventDefault();
		}
	}
}

function webuiPullTouchEnd(e) {
	if (webuiIsRefreshing) return;
	if (!webuiIsPulling) {
		webuiPullStartY = 0;
		return;
	}

	const deltaY = webuiPullCurrentY - webuiPullStartY;
	const pullDistance = Math.pow(deltaY, 0.8);

	if (pullDistance >= WEBUI_PULL_THRESHOLD && webuiRefreshCallback) {
		webuiIsRefreshing = true;
		webuiShowRefreshing();

		// Execute refresh callback
		const result = webuiRefreshCallback();

		// Handle both Promise and non-Promise returns
		if (result && typeof result.then === 'function') {
			result.finally(() => {
				webuiIsRefreshing = false;
				webuiHidePullIndicator();
			});
		} else {
			// Non-promise: assume quick operation, hide after delay
			setTimeout(() => {
				webuiIsRefreshing = false;
				webuiHidePullIndicator();
			}, 500);
		}
	} else {
		webuiHidePullIndicator();
	}

	webuiPullStartY = 0;
	webuiPullCurrentY = 0;
	webuiIsPulling = false;
}

function webuiInitPullRefresh() {
	if (window.innerWidth > 768) return;

	webuiCreatePullIndicator();

	document.addEventListener('touchstart', webuiPullTouchStart, { passive: true });
	document.addEventListener('touchmove', webuiPullTouchMove, { passive: false });
	document.addEventListener('touchend', webuiPullTouchEnd, { passive: true });
}

// Initialize the header and its components
function webuiInit() {
	const headerWrapper = document.querySelector('.webui-header-wrapper');
	const existingHeader = document.querySelector('.webui-header');

	// Check for dynamic header generation (wrapper with data-page, no header yet)
	if (headerWrapper && !existingHeader) {
		const activePage = headerWrapper.dataset.page || null;
		// Prepend header before any existing content (e.g., subheaders)
		headerWrapper.insertAdjacentHTML('afterbegin', webuiGetHeaderHTML(activePage));

		// Create spacer if not present
		if (!document.querySelector('.webui-header-spacer')) {
			headerWrapper.insertAdjacentHTML('afterend', '<div class="webui-header-spacer"></div>');
		}
	}

	// Initialize if header exists (either pre-rendered or just generated)
	if (document.querySelector('.webui-header')) {
		// Set theme
		webuiSetTheme(webuiGetTheme());

		// Start clock
		webuiUpdateClock();
		setInterval(webuiUpdateClock, 10000);

		// Load hostname
		webuiLoadHostname();

		// Calculate and set spacer height
		webuiUpdateHeaderSpacer();

		// Update on window resize
		window.addEventListener('resize', webuiUpdateHeaderSpacer);

		// Close menu when clicking on nav links (mobile)
		const navLinks = document.querySelectorAll('.webui-nav-links a');
		navLinks.forEach(link => {
			link.addEventListener('click', webuiCloseMenu);
		});

		// Initialize footer (if not already present)
		webuiInitFooter();

		// Always insert about modal if not present
		if (!document.getElementById('webui-about-modal')) {
			document.body.insertAdjacentHTML('beforeend', webuiAboutHTML());
		}

		// Initialize mobile menu swipe gestures
		webuiInitMenuSwipe();

		// Initialize pull-to-refresh
		webuiInitPullRefresh();

		// Erweiterungen in die Slots dieser Seite injizieren (async, still).
		// Host-Seiten booten ueber diesen Pfad, nicht ueber webuiInitHeader.
		if (typeof webuiInjectExtensions === 'function') {
			setTimeout(function() {
				webuiInjectExtensions(headerWrapper ? headerWrapper.dataset.page : null);
			}, 0);
		}

		webuiCheckDefaultPassword();
	}
}

/* ============================================
   Grid-Balancer (shared, 12-Track-Raster)
   ============================================ */

/**
 * Restreihen-Balancer: verteilt Karten eines 12-Track-Grids so, dass
 * jede Reihe die Breite fuellt (Kartenzahl variabel, Erweiterungen
 * injizieren asynchron). Idempotent; unterhalb des Breakpoints
 * uebernehmen die Media-Queries der Seite. Das Grid braucht
 * `grid-template-columns: repeat(12, 1fr)` und auf den Karten
 * `grid-column: span var(--dash-span, 4)`.
 * @param {string} selector - CSS-Selektor des Grids
 * @param {Object} [opts]
 * @param {number} [opts.cardMin=300] - Ziel-Kartenbreite fuer Karten/Reihe
 * @param {number} [opts.maxPer=6] - Deckel Karten je Reihe (span >= 2)
 * @param {number} [opts.breakpoint=1100] - darunter Spans entfernen
 * @param {string} [opts.wideClass] - Klasse fuer Karten ab span >= 6
 * @param {boolean} [opts.even] - Reihen glatt teilen, wenn die
 *        Kartenzahl es hergibt (verhindert gestreckte Restreihen)
 */
function webuiBalanceGrid(selector, opts) {
	opts = opts || {};
	var grid = document.querySelector(selector);
	if (!grid) return;
	var cards = Array.prototype.filter.call(grid.children, function(c) {
		return c.offsetParent !== null; // nur sichtbare zaehlen
	});
	if (window.innerWidth <= (opts.breakpoint || 1100)) {
		cards.forEach(function(c) {
			if (opts.wideClass) c.classList.remove(opts.wideClass);
			c.style.removeProperty('--dash-span');
		});
		return;
	}
	var per = Math.max(2, Math.min(opts.maxPer || 6,
		Math.floor(window.innerWidth / (opts.cardMin || 300))));
	if (opts.even) {
		var p = per;
		while (p > 2 && cards.length % p) p--;
		if (cards.length % p === 0) per = p; // nur wenn glatt aufgeht
	}
	var span = Math.floor(12 / per);          // Grundspan
	var rest = cards.length % per;            // Karten in letzter Reihe
	cards.forEach(function(c, i) {
		var s = span;
		if (rest && i >= cards.length - rest)
			s = Math.floor(12 / rest);        // letzte Reihe fuellt auf
		c.style.setProperty('--dash-span', s);
		if (opts.wideClass) c.classList.toggle(opts.wideClass, s >= 6);
	});
}

/* ============================================
   Mobile Accordion (shared card toggle/swipe)
   ============================================ */
var _webuiAcc = null; /* accordion config */
var _webuiAccTouchStart = {x: 0, y: 0};

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
function webuiInitMobileAccordion(cfg) {
	cfg.breakpoint = cfg.breakpoint || 800;
	cfg.mode = cfg.mode || 'collapsed';
	_webuiAcc = cfg;

	initMobileAccordion();
	buildCardIndicator();

	var container = document.querySelector(cfg.containerSelector);
	if (container) {
		container.addEventListener('touchstart', function(e) {
			_webuiAccTouchStart.x = e.changedTouches[0].screenX;
			_webuiAccTouchStart.y = e.changedTouches[0].screenY;
		}, {passive: true});
		container.addEventListener('touchend', function(e) {
			if (!_webuiAcc || window.innerWidth > _webuiAcc.breakpoint) return;
			var dx = e.changedTouches[0].screenX - _webuiAccTouchStart.x;
			var dy = Math.abs(e.changedTouches[0].screenY - _webuiAccTouchStart.y);
			if (Math.abs(dx) > 50 && dy < 100) navigateCard(dx > 0 ? -1 : 1);
		}, {passive: true});
	}

	var resizeTimer;
	window.addEventListener('resize', function() {
		clearTimeout(resizeTimer);
		resizeTimer = setTimeout(initMobileAccordion, 250);
	});
}

function _webuiAccGetActive() {
	var id;
	try { id = localStorage.getItem(_webuiAcc.storageKey); } catch(e) {}
	return id || _webuiAcc.defaultCard;
}

function _webuiAccSetCard(card, cardId) {
	if (_webuiAcc.mode === 'collapsed') {
		document.querySelectorAll('.webui-card').forEach(function(c) { c.classList.add('collapsed'); });
		card.classList.remove('collapsed');
	} else {
		document.querySelectorAll('.webui-card.expanded').forEach(function(c) { c.classList.remove('expanded'); });
		card.classList.add('expanded');
	}
	try { localStorage.setItem(_webuiAcc.storageKey, cardId); } catch(e) {}
	updateCardIndicator();
}

function toggleCard(cardId) {
	if (!_webuiAcc || window.innerWidth > _webuiAcc.breakpoint) return;
	var card = document.getElementById(cardId);
	if (!card) return;

	if (_webuiAcc.mode === 'collapsed') {
		var isCollapsed = card.classList.contains('collapsed');
		document.querySelectorAll('.webui-card').forEach(function(c) { c.classList.add('collapsed'); });
		if (isCollapsed) card.classList.remove('collapsed');
	} else {
		document.querySelectorAll('.webui-card.expanded').forEach(function(c) {
			if (c !== card) c.classList.remove('expanded');
		});
		card.classList.toggle('expanded');
	}
	var isActive = _webuiAcc.mode === 'collapsed' ? !card.classList.contains('collapsed') : card.classList.contains('expanded');
	if (isActive) {
		try { localStorage.setItem(_webuiAcc.storageKey, cardId); } catch(e) {}
	}
	updateCardIndicator();
}

function navigateCard(delta) {
	if (!_webuiAcc || window.innerWidth > _webuiAcc.breakpoint) return;
	var currentId = _webuiAccGetActive();
	var idx = _webuiAcc.cardOrder.indexOf(currentId);
	if (idx === -1) return;
	var newIdx = idx + delta;
	if (newIdx < 0) newIdx = _webuiAcc.cardOrder.length - 1;
	if (newIdx >= _webuiAcc.cardOrder.length) newIdx = 0;
	var newId = _webuiAcc.cardOrder[newIdx];
	var newCard = document.getElementById(newId);
	if (newCard) {
		_webuiAccSetCard(newCard, newId);
		newCard.scrollIntoView({behavior: 'smooth', block: 'nearest'});
	}
}

function selectCard(cardId) {
	if (!_webuiAcc || window.innerWidth > _webuiAcc.breakpoint) return;
	var card = document.getElementById(cardId);
	if (card) {
		_webuiAccSetCard(card, cardId);
		card.scrollIntoView({behavior: 'smooth', block: 'nearest'});
	}
}
/* dashboard.html uses navigateToCard — alias */
var navigateToCard = selectCard;

function buildCardIndicator() {
	if (!_webuiAcc) return;
	var el = document.getElementById(_webuiAcc.indicatorId);
	if (!el) return;
	el.innerHTML = _webuiAcc.cardOrder.map(function(id) {
		return '<div class="webui-card-dot" data-card="' + id + '" onclick="selectCard(\'' + id + '\')"></div>';
	}).join('');
	updateCardIndicator();
}

function updateCardIndicator() {
	if (!_webuiAcc) return;
	var activeId = _webuiAccGetActive();
	document.querySelectorAll('.webui-card-dot').forEach(function(dot) {
		var dotCard = dot.dataset ? dot.dataset.card : null;
		if (!dotCard) {
			var idx = Array.prototype.indexOf.call(dot.parentNode.children, dot);
			dotCard = _webuiAcc.cardOrder[idx];
		}
		dot.classList.toggle('active', dotCard === activeId);
	});
}

function initMobileAccordion() {
	if (!_webuiAcc) return;
	if (window.innerWidth > _webuiAcc.breakpoint) {
		if (_webuiAcc.mode === 'collapsed') {
			document.querySelectorAll('.webui-card').forEach(function(c) { c.classList.remove('collapsed'); });
		} else {
			document.querySelectorAll('.webui-card').forEach(function(c) { c.classList.add('expanded'); });
		}
	} else {
		var savedCard = _webuiAccGetActive();
		if (_webuiAcc.mode === 'collapsed') {
			document.querySelectorAll('.webui-card').forEach(function(c) { c.classList.add('collapsed'); });
			var active = document.getElementById(savedCard);
			if (active) active.classList.remove('collapsed');
		} else {
			document.querySelectorAll('.webui-card').forEach(function(c) { c.classList.remove('expanded'); });
			var active = document.getElementById(savedCard);
			if (active) active.classList.add('expanded');
		}
	}
	updateCardIndicator();
}

// Auto-detect page from URL and initialize, once the document is parsed and,
// where the page loads webui-i18n.js, its texts are in.
if (typeof webuiAfterTexts === 'function') {
	webuiAfterTexts(webuiInit);
} else if (document.readyState === 'loading') {
	document.addEventListener('DOMContentLoaded', webuiInit);
} else {
	// DOM already loaded, init immediately
	webuiInit();
}
