/**
 * yWeb Common Header Component
 * Provides consistent header across all yWeb pages
 */

// Central configuration
const YWEB_CONFIG = {
	logo: '/images/logo.png',
	title: 'Neutrino-Web-UI'
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
	recordings: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M7 4v16M17 4v16M2 9h5M2 15h5M17 9h5M17 15h5"/></svg>',
	timer: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="9"/><path d="M12 6v6l4 2"/></svg>',
	remote: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="7" y="2" width="10" height="20" rx="2"/><circle cx="12" cy="18" r="1"/></svg>',
	settings: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>',
	tools: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z"/></svg>'
};

// Shared modal close button
function _ywebCloseBtn(onclick) {
	return '<button type="button" class="yweb-modal-close" aria-label="Close" onclick="' + onclick + '">' +
		'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true" focusable="false"><path d="M6 18L18 6M6 6l12 12"/></svg>' +
	'</button>';
}

// Footer HTML Template
const YWEB_FOOTER_HTML = `
<footer class="yweb-footer">
	Neutrino-Web-UI <span id="yweb-footer-version">--</span> ·
	<a href="#" onclick="ywebShowAbout(); return false;">${ywebL('about.link', 'About')}</a> ·
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
					<span class="yweb-about-label">Neutrino-Web-UI</span>
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

// Generate header HTML with optional active page
function ywebGetHeaderHTML(activePage) {
	const nav = YWEB_CONFIG.navigation || [
		{ page: 'dashboard', href: '/dashboard.yhtm', icon: 'dashboard', label: ywebL('nav.dashboard', 'Dashboard') },
		{ page: 'bouquets', href: '/bouquets.yhtm', icon: 'bouquets', label: ywebL('nav.bouquets', 'Bouquets') },
		{ page: 'epg', href: '/epg.yhtm', icon: 'epg', label: ywebL('nav.epg', 'EPG') },
		{ page: 'timer', href: '/timer.yhtm', icon: 'timer', label: ywebL('nav.timer', 'Timer') },
		{ page: 'recordings', href: '/recordings.yhtm', icon: 'recordings', label: ywebL('nav.recordings', 'Recordings') },
		{ page: 'settings', href: '/settings.yhtm', icon: 'settings', label: ywebL('nav.settings', 'Settings') },
		{ page: 'tools', href: '/tools.yhtm', icon: 'tools', label: ywebL('nav.tools', 'Tools') }
	];

	const navItems = nav.map(item => {
		const active = item.page === activePage ? ' class="active"' : '';
		return `<li><a href="${item.href}" data-page="${item.page}"${active}>${YWEB_ICONS[item.icon]}<span>${item.label}</span></a></li>`;
	}).join('\n\t\t\t');

	return `
<header class="yweb-header">
	<nav>
		<a href="/dashboard.yhtm" class="yweb-logo">
			<img src="${YWEB_CONFIG.logo}" alt="" onerror="this.remove()">
			<span>${YWEB_CONFIG.title}</span>
		</a>

		<ul class="yweb-nav-links" id="yweb-nav-menu">
			${navItems}
		</ul>

		<div class="yweb-header-right">
			<span class="yweb-hostname" id="yweb-hostname">--</span>
			<span class="yweb-clock" id="yweb-clock">--:--</span>
			<button class="yweb-theme-toggle yweb-lang-btn" onclick="ywebToggleLangMenu(event)" title="${ywebL('lang.title', 'Language')}">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<circle cx="12" cy="12" r="9" stroke-width="2"/>
					<path stroke-width="2" d="M3 12h18M12 3a15 15 0 010 18M12 3a15 15 0 000 18"/>
				</svg>
				<span class="yweb-lang-code">${ywebActiveLangLabel()}</span>
			</button>
			<button class="yweb-theme-toggle" onclick="ywebToggleTheme()" title="${ywebL('theme.toggle', 'Toggle theme')}">
				<svg id="yweb-theme-icon-dark" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20.354 15.354A9 9 0 018.646 3.646 9.003 9.003 0 0012 21a9.003 9.003 0 008.354-5.646z"/>
				</svg>
				<svg id="yweb-theme-icon-light" class="yweb-hidden" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z"/>
				</svg>
			</button>
			<button class="yweb-theme-toggle" id="yweb-logout-btn" hidden onclick="ywebLogout()" aria-label="${ywebL('hdr.logout', 'Log out')}" title="${ywebL('hdr.logout', 'Log out')}">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/>
					<polyline stroke-linecap="round" stroke-linejoin="round" stroke-width="2" points="16 17 21 12 16 7"/>
					<line stroke-linecap="round" stroke-linejoin="round" stroke-width="2" x1="21" y1="12" x2="9" y2="12"/>
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

// ═══ Sprachwechsler ═══
// Sprachliste kommt aus Y_Lang.yjs (window.YWEB_LANGS_HTML, yparsed
// option-Liste des languages/-Verzeichnisses, aktive Sprache selected)
// Kuerzel der aktiven Sprache fuer den Header-Button (EN, DE, ...).
// Der Globus allein war nicht als Sprachumschalter erkennbar; das aktive
// Kuerzel daneben macht den Button eindeutig. Bekannte Sprachen werden auf
// ihr ISO-Kuerzel gemappt, unbekannte auf die ersten zwei Buchstaben.
function ywebActiveLangLabel() {
	var langs = ywebGetLanguages();
	var name = '';
	for (var i = 0; i < langs.length; i++)
		if (langs[i].active) { name = langs[i].name; break; }
	if (!name && langs.length) name = langs[0].name;
	if (!name) return '';
	var codes = {
		English: 'EN', Deutsch: 'DE', Czech: 'CS', Polski: 'PL',
		Slovak: 'SK', Portuguese: 'PT'
	};
	return codes[name] || name.slice(0, 2).toUpperCase();
}

function ywebGetLanguages() {
	var langs = [];
	if (!window.YWEB_LANGS_HTML) return langs;
	var sel = document.createElement('select');
	sel.innerHTML = window.YWEB_LANGS_HTML;
	for (var i = 0; i < sel.options.length; i++)
		langs.push({name: sel.options[i].value, active: sel.options[i].hasAttribute('selected')});
	return langs;
}

// ═══ Per-extension language packs ═══
// Extensions ship their own dictionaries as flat JSON:
// <ext>/languages/<Language>.json — named like the core language file
// (English.json, Deutsch.json, ...). English texts live inline in the
// markup/manifest as fallback; there is deliberately no second fetch
// of English.json. Reserved keys manifest.name/manifest.description
// localize tile/nav/manager (hook in ywebDiscoverExtensions).
var _ywebExtLang = {};

function ywebActiveLangName() {
	var langs = ywebGetLanguages();
	for (var i = 0; i < langs.length; i++)
		if (langs[i].active) return langs[i].name;
	return 'English';
}

async function ywebExtLangLoad(dir) {
	if (_ywebExtLang[dir]) return _ywebExtLang[dir];
	var dict = {};
	try {
		var resp = await fetch('/extensions/' + encodeURIComponent(dir) +
			'/languages/' + encodeURIComponent(ywebActiveLangName()) + '.json',
			{cache: 'no-cache'});
		if (resp.ok) dict = (await resp.json()) || {};
	} catch (ignore) {} // a missing language pack is NOT an error
	_ywebExtLang[dir] = dict;
	return dict;
}

function ywebExtL(dir, key, fallbackEN) {
	var d = _ywebExtLang[dir];
	var v = d && d[key];
	return v || fallbackEN || key;
}

function ywebToggleLangMenu(ev) {
	ev.stopPropagation();
	var menu = document.getElementById('yweb-lang-menu');
	if (menu) { menu.remove(); return; }
	var langs = ywebGetLanguages();
	if (!langs.length) {
		ywebShowToast(ywebL('lang.none', 'No languages installed'), true);
		return;
	}
	menu = document.createElement('div');
	menu.id = 'yweb-lang-menu';
	menu.className = 'yweb-lang-menu';
	langs.forEach(function(l) {
		var b = document.createElement('button');
		b.type = 'button';
		b.textContent = l.name;
		if (l.active) b.classList.add('active');
		b.onclick = function() { ywebSetLanguage(l.name); };
		menu.appendChild(b);
	});
	var r = ev.currentTarget.getBoundingClientRect();
	menu.style.top = (r.bottom + 4) + 'px';
	menu.style.right = Math.max(4, window.innerWidth - r.right) + 'px';
	document.body.appendChild(menu);
	setTimeout(function() {
		document.addEventListener('click', function close() {
			document.removeEventListener('click', close);
			var m = document.getElementById('yweb-lang-menu');
			if (m) m.remove();
		});
	}, 0);
}

async function ywebSetLanguage(lang) {
	try {
		// set-lang.yhtm ruft set_language: schreibt nhttpd.conf und
		// laedt die Sprachdateien im Webserver neu; danach Reload
		await fetch('/set-lang.yhtm?' + encodeURIComponent(lang), {cache: 'no-store'});
		location.reload();
	} catch (e) {
		ywebShowToast('Error: ' + e.message, true);
	}
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

	ywebCheckDefaultPassword();
}

// Default-Passwort-Gate: aktiver Login mit Werkspasswort ist faktisch
// ungeschuetzt (root:tuxbox ist oeffentlich bekannt). Einmal pro Session
// pruefen und zum Wizard-Login-Schritt umleiten — Analogie zum
// must_change_password-Gate des Online-Update-Portals, hier clientseitig,
// weil Basic-Auth stateless ist. wizard.yhtm laedt dieses Skript nicht,
// eine Redirect-Schleife ist damit ausgeschlossen.
function ywebCheckDefaultPassword() {
	// Sentinel gesetzt: Fetch sparen, Logout-Button aus dem beim ersten
	// Load gemerkten Auth-Zustand schalten
	try {
		if (sessionStorage.getItem('yweb-pwgate') === 'ok') {
			ywebRefreshLogoutBtn();
			return;
		}
	} catch (e) {}
	ywebApi('config', {config: 'nhttpd'}).then(function(resp) {
		var cfg = (resp && resp.data && resp.data.config) || {};
		var authed = cfg['mod_auth_dot_authenticate'] === 'true';
		if (authed && cfg['mod_auth_dot_password_default'] === 'true') {
			location.replace('/wizard.yhtm?step=login&forced=1');
			return;
		}
		try {
			sessionStorage.setItem('yweb-pwgate', 'ok');
			sessionStorage.setItem('yweb-authed', authed ? 'true' : 'false');
		} catch (e) {}
		ywebRefreshLogoutBtn();
	}).catch(function() { ywebRefreshLogoutBtn(); });
}

// The button shows while the pages ask for a login, and also while a
// session of the one login is open: with the page login switched off a
// change still asks for a sign-in, and what was signed into can be ended.
function ywebRefreshLogoutBtn() {
	var authed = false;
	try { authed = sessionStorage.getItem('yweb-authed') === 'true'; } catch (e) {}
	if (authed) {
		ywebUpdateLogoutBtn(true);
		return;
	}
	fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'})
		.then(function(r) { return r.ok ? r.json() : null; })
		.then(function(s) { ywebUpdateLogoutBtn(!!(s && s.authenticated)); })
		.catch(function() {});
}

function ywebUpdateLogoutBtn(show) {
	var btn = document.getElementById('yweb-logout-btn');
	if (btn) btn.hidden = !show;
}

// /logout ends the session and tells the browser to forget its cookie.
// With the page login on it answers 401 to whatever still carries Basic
// credentials, which makes the browser drop them; with it off it goes
// back to the start page.
function ywebLogout() {
	try {
		sessionStorage.removeItem('yweb-pwgate');
		sessionStorage.removeItem('yweb-authed');
	} catch (e) {}
	location.href = '/logout';
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
		throw new Error((data.error && data.error.msg) ? data.error.msg : (ywebL('msg.error', 'Error') + ': ' + endpoint));
	return data;
}

// A key of the remote control. Over /api/v1 where the page loads yweb-v1.js
// (session, CSRF token), else over the old interface, which extensions and
// older pages still use.
async function ywebSendKey(key) {
	var v1 = typeof ywebV1 === 'function';
	var send = v1
		? function() { return ywebV1('POST', '/osd/remote/key', {body: {name: key}}); }
		: function() { return ywebApi('rcem', key, {raw: true}); };
	if (key === 'KEY_POWER') {
		try {
			var awake = v1
				? (await ywebV1('GET', '/system/standby')).on === false
				: (await ywebApi('standby', {status: ''})).standby === false;
			if (awake) {
				// Rueckfrage als yweb-Dialog statt nativem confirm()
				ywebShowConfirm('Standby', ywebL('sb.confirm', 'Put the box into standby?'), 'Standby',
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
 * @param {string} [opts.onKey='ywebSendKey'] - Global function NAME called with a KEY_* string
 * @param {string} [opts.id='yweb-remote'] - id of the returned root element
 *
 * Custom renderer contract (hardware-specific remote skins):
 *   Register with ywebRegisterRemoteRenderer(function(opts) { return html; })
 *   from an extension runtime script (manifest key entry_js) - never set
 *   YWEB_CONFIG.remoteRenderer directly, the register call also re-renders
 *   already-built instances (registration timing is not guaranteed).
 *   The returned HTML must have a single root element with id=opts.id and
 *   data-rc-onkey=opts.onKey. Buttons call the opts.onKey global with a
 *   valid KEY_* name (generated rcsim.h table; note KEY_TIME, not
 *   KEY_TIMER). Opt into auto-repeat/press feedback via data-rc-repeat +
 *   data-rc-key attributes. Use real <button type="button"> elements with
 *   aria-labels on icon-only keys and aria-hidden SVGs.
 *   See extensions/README.md for a minimal renderer example.
 */
function ywebGetRemoteHTML(opts) {
	opts = opts || {};
	var onKey = opts.onKey || 'ywebSendKey';
	var id = opts.id || 'yweb-remote';

	if (YWEB_CONFIG.remoteRenderer) {
		return YWEB_CONFIG.remoteRenderer(opts);
	}

	function btn(cls, key, label, title) {
		return '<button type="button" class="yweb-rc-btn ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + escapeHtml(title || label || key) + '">' + escapeHtml(label) + '</button>';
	}
	function iconBtn(cls, key, title, svg) {
		var t = escapeHtml(title);
		return '<button type="button" class="yweb-rc-btn ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + t + '" aria-label="' + t + '">' +
			svg.replace('<svg ', '<svg aria-hidden="true" focusable="false" ') + '</button>';
	}
	function repBtn(cls, key, label, title) {
		var t = escapeHtml(title || label || key);
		return '<button type="button" class="yweb-rc-btn ' + cls + '" title="' + t + '" aria-label="' + t + '" data-rc-repeat="1" data-rc-key="' + key + '">' + escapeHtml(label) + '</button>';
	}
	function colorBtn(cls, key, title) {
		var t = escapeHtml(title);
		return '<button type="button" class="yweb-rc-color ' + cls + '" onclick="' + onKey + "('" + key + "')\" title=\"" + t + '" aria-label="' + t + '"></button>';
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
			iconBtn('yweb-rc-power', 'KEY_POWER', ywebL('rc.power', 'Power'), svgPower) +
			iconBtn('', 'KEY_MUTE', ywebL('rc.mute', 'Mute'), svgMute) +
		'</div>' +

		/* Zone B: Core thumb zone (VOL | D-Pad | CH) */
		'<div class="yweb-rc-core">' +
			'<div class="yweb-rc-rocker">' +
				'<span class="yweb-rc-rocker-label">VOL</span>' +
				repBtn('yweb-rc-rocker-top', 'KEY_VOLUMEUP', '+', ywebL('rc.vol_up', 'Volume up')) +
				repBtn('yweb-rc-rocker-bot', 'KEY_VOLUMEDOWN', '\u2212', ywebL('rc.vol_down', 'Volume down')) +
			'</div>' +
			'<div class="yweb-rc-dpad">' +
				'<div></div>' + iconBtn('yweb-rc-arrow', 'KEY_UP', ywebL('rc.up', 'Up'), svgUp) + '<div></div>' +
				iconBtn('yweb-rc-arrow', 'KEY_LEFT', ywebL('rc.left', 'Left'), svgLeft) +
				btn('yweb-rc-ok', 'KEY_OK', 'OK', ywebL('rc.ok', 'OK')) +
				iconBtn('yweb-rc-arrow', 'KEY_RIGHT', ywebL('rc.right', 'Right'), svgRight) +
				'<div></div>' + iconBtn('yweb-rc-arrow', 'KEY_DOWN', ywebL('rc.down', 'Down'), svgDown) + '<div></div>' +
			'</div>' +
			'<div class="yweb-rc-rocker">' +
				'<span class="yweb-rc-rocker-label">CH</span>' +
				repBtn('yweb-rc-rocker-top', 'KEY_PAGEUP', '+', ywebL('rc.ch_up', 'Channel up')) +
				repBtn('yweb-rc-rocker-bot', 'KEY_PAGEDOWN', '\u2212', ywebL('rc.ch_down', 'Channel down')) +
			'</div>' +
		'</div>' +

		/* Zone C: Navigation */
		'<div class="yweb-rc-row yweb-rc-nav">' +
			btn('yweb-rc-func', 'KEY_EXIT', 'Back', ywebL('rc.back', 'Back')) +
			btn('yweb-rc-func', 'KEY_HOME', 'Home', ywebL('rc.home', 'Home')) +
			btn('yweb-rc-func', 'KEY_SETUP', 'Menu', ywebL('rc.menu', 'Menu')) +
			btn('yweb-rc-func', 'KEY_HELP', 'Info', ywebL('rc.info', 'Info')) +
		'</div>' +

		/* Zone D: Media */
		'<div class="yweb-rc-row yweb-rc-media">' +
			iconBtn('', 'KEY_REWIND', ywebL('rc.rewind', 'Rewind'), '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="19 20 9 12 19 4"/><polygon points="14 20 4 12 14 4"/></svg>') +
			iconBtn('', 'KEY_PLAY', ywebL('rc.play', 'Play'), '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21"/></svg>') +
			iconBtn('', 'KEY_FORWARD', ywebL('rc.forward', 'Forward'), '<svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 4 15 12 5 20"/><polygon points="10 4 20 12 10 20"/></svg>') +
			iconBtn('', 'KEY_STOP', ywebL('rc.stop', 'Stop'), '<svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="6" width="12" height="12"/></svg>') +
		'</div>' +

		/* Zone E: Colour keys (always visible) */
		'<div class="yweb-rc-row yweb-rc-colors">' +
			colorBtn('yweb-rc-red', 'KEY_RED', ywebL('rc.red', 'Red')) +
			colorBtn('yweb-rc-green', 'KEY_GREEN', ywebL('rc.green', 'Green')) +
			colorBtn('yweb-rc-yellow', 'KEY_YELLOW', ywebL('rc.yellow', 'Yellow')) +
			colorBtn('yweb-rc-blue', 'KEY_BLUE', ywebL('rc.blue', 'Blue')) +
		'</div>' +

		/* Zone F: EPG / Text / Fav / Timer (always visible) */
		'<div class="yweb-rc-row yweb-rc-nav">' +
			btn('yweb-rc-func', 'KEY_EPG', 'EPG', ywebL('rc.epg', 'EPG')) +
			btn('yweb-rc-func', 'KEY_TEXT', 'Text', ywebL('rc.text', 'Teletext')) +
			btn('yweb-rc-func', 'KEY_FAVORITES', ywebL('rc.fav', 'Fav')) +
			btn('yweb-rc-func', 'KEY_TIME', ywebL('rc.time', 'Timer')) +
		'</div>' +

		/* Numpad trigger — reveals the numpad sheet on every viewport */
		'<button type="button" class="yweb-rc-btn yweb-rc-sheet-trigger" aria-label="' + escapeHtml(ywebL('rc.numpad', 'Numpad')) + '" aria-expanded="false" onclick="ywebToggleRcSheet(\'' + id + '\')">123' +
			'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" aria-hidden="true" focusable="false"><polyline points="6 9 12 15 18 9"/></svg>' +
		'</button>' +

		/* Numpad sheet (bottom sheet on mobile, inline panel on desktop) */
		'<div class="yweb-rc-sheet-backdrop" id="' + id + '-sheet" onclick="ywebToggleRcSheet(\'' + id + '\')">' +
			'<div class="yweb-rc-sheet" onclick="event.stopPropagation()">' +
				'<div class="yweb-rc-sheet-handle"></div>' +
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

/* Numpad sheet toggle. Works on every viewport: a bottom sheet on
   mobile, an inline panel on desktop (CSS decides the presentation via
   the media query; the JS only flips display). */
function ywebToggleRcSheet(id) {
	var backdrop = document.getElementById(id + '-sheet');
	if (!backdrop) return;
	var root = document.getElementById(id);
	var trigger = root ? root.querySelector('.yweb-rc-sheet-trigger') : null;

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

/* Long-press auto-repeat for VOL/CH buttons + visual press feedback */
function ywebInitRemoteRepeat(containerId) {
	var container = document.getElementById(containerId);
	if (!container) return;

	/* Delegated press feedback: pointerdown-driven class, because a quick
	   tap's :active phase is too short to be visible on touch. Purely
	   visual - never sends a key. */
	if (!container._pressInit) {
		container._pressInit = true;
		container.addEventListener('pointerdown', function(e) {
			var el = e.target.closest('.yweb-rc-btn, .yweb-rc-color');
			if (!el || !container.contains(el)) return;
			if (e.pointerType === 'touch' && navigator.vibrate) navigator.vibrate(10);
			/* Restart-safe: cancel the previous removal hooks before
			   re-triggering, then force a reflow so the animation restarts. */
			if (el._pressTimer) { clearTimeout(el._pressTimer); el._pressTimer = null; }
			if (el._pressEnd) { el.removeEventListener('animationend', el._pressEnd); el._pressEnd = null; }
			el.classList.remove('yweb-rc-pressed');
			void el.offsetWidth;
			el.classList.add('yweb-rc-pressed');
			var clear = function() {
				el.classList.remove('yweb-rc-pressed');
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
var _ywebRemoteInstances = {};

function ywebRenderRemote(hostId, opts) {
	var host = document.getElementById(hostId);
	if (!host) {
		delete _ywebRemoteInstances[hostId];
		return;
	}
	opts = opts || {};
	host.innerHTML = ywebGetRemoteHTML(opts);
	ywebInitRemoteRepeat(opts.id || 'yweb-remote');
	_ywebRemoteInstances[hostId] = opts;
}

/**
 * Registers a hardware-specific remote skin renderer (see the contract
 * at ywebGetRemoteHTML). Last registration wins; already-rendered
 * instances are re-rendered in place, an open remote modal stays open.
 */
function ywebRegisterRemoteRenderer(fn) {
	if (YWEB_CONFIG.remoteRenderer && YWEB_CONFIG.remoteRenderer !== fn)
		console.warn('yweb: remote renderer already registered - overwriting (last one wins)');
	YWEB_CONFIG.remoteRenderer = fn;

	var modal = document.getElementById('yweb-remote-modal');
	if (modal && getComputedStyle(modal).display !== 'none') {
		/* Open modal: swap the panel in place; scroll lock and dialog
		   stay untouched. Restore focus if it sat in the replaced tree. */
		ywebRenderRemote('yweb-remote-modal-host',
			_ywebRemoteInstances['yweb-remote-modal-host'] ||
			{ id: 'yweb-remote-modal-panel', onKey: _ywebRemoteModalOnKey || 'ywebSendKey' });
		if (!modal.contains(document.activeElement)) {
			var closeBtn = modal.querySelector('.yweb-modal-close');
			if (closeBtn) closeBtn.focus();
		}
	} else if (modal) {
		/* Closed modal: drop it so the next open builds with the skin */
		if (modal._keyHandler) document.removeEventListener('keydown', modal._keyHandler);
		modal.remove();
		_ywebRemoteModalOnKey = null;
		delete _ywebRemoteInstances['yweb-remote-modal-host'];
	}

	/* Re-render all remaining registered instances (covers the
	   screenshot page, whose remote is built before async discovery) */
	Object.keys(_ywebRemoteInstances).forEach(function(hostId) {
		if (hostId === 'yweb-remote-modal-host') return;
		if (!document.getElementById(hostId)) {
			delete _ywebRemoteInstances[hostId];
			return;
		}
		ywebRenderRemote(hostId, _ywebRemoteInstances[hostId]);
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
		if (modal._keyHandler) document.removeEventListener('keydown', modal._keyHandler);
		modal.remove();
		delete _ywebRemoteInstances['yweb-remote-modal-host'];
		modal = null;
	}

	if (!modal) {
		_ywebRemoteModalOnKey = onKey;
		var html = '<div class="yweb-remote-backdrop" id="yweb-remote-modal" onclick="ywebCloseRemote(event)">' +
			'<div class="yweb-remote-dialog" role="dialog" aria-modal="true" aria-labelledby="yweb-remote-modal-title" onclick="event.stopPropagation()">' +
				'<div class="yweb-remote-modal-header">' +
					'<span id="yweb-remote-modal-title">' + YWEB_ICONS.remote + ' ' + ywebL('tools.remote_sub', 'Remote control') + '</span>' +
					_ywebCloseBtn('ywebCloseRemote()') +
				'</div>' +
				'<div id="yweb-remote-modal-host"></div>' +
			'</div>' +
		'</div>';
		document.body.insertAdjacentHTML('beforeend', html);
		modal = document.getElementById('yweb-remote-modal');
		ywebRenderRemote('yweb-remote-modal-host', { id: 'yweb-remote-modal-panel', onKey: onKey });
	}
	modal._opener = document.activeElement;
	modal.style.display = 'flex';
	document.body.classList.add('yweb-remote-open');

	/* ESC with sheet priority + Tab focus trap. Registered before any
	   sheet ESC handler, so on Escape the sheet is closed here first and
	   its own listener is removed mid-dispatch (never runs for the same
	   event). The standalone remote (screenshot page) keeps relying on
	   the sheet's own handler. */
	if (!modal._keyHandler) {
		modal._keyHandler = function(e) {
			if (e.key === 'Escape') {
				var sheet = document.getElementById('yweb-remote-modal-panel-sheet');
				if (sheet && sheet.style.display === 'flex') {
					ywebToggleRcSheet('yweb-remote-modal-panel');
					return;
				}
				ywebCloseRemote();
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
					el.classList.add('yweb-rc-kbd-focus');
					el.addEventListener('blur', function() {
						el.classList.remove('yweb-rc-kbd-focus');
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
	var closeBtn = modal.querySelector('.yweb-modal-close');
	if (closeBtn) closeBtn.focus();
}

function ywebCloseRemote(event) {
	if (event && event.target !== event.currentTarget) return;
	var modal = document.getElementById('yweb-remote-modal');
	if (modal) {
		modal.style.display = 'none';
		var sheet = modal.querySelector('.yweb-rc-sheet-backdrop');
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

// Companion des geteilten .yweb-file-Controls (yweb.css): zeigt den
// gewaehlten Dateinamen im Control an — onchange="ywebFileName(this)"
function ywebFileName(inp) {
	var box = inp.closest('.yweb-file');
	var n = box && box.querySelector('.yweb-file-name');
	if (n) n.textContent = (inp.files && inp.files.length) ? inp.files[0].name : '–';
}

// ═══ Verzeichnis-/Datei-Picker (nutzt FileCGI action=list) ═══
// Elternpfad von p (POSIX), '/' bleibt '/'
function ywebParentPath(p) {
	if (!p || p === '/') return '/';
	p = p.replace(/\/+$/, '');
	var i = p.lastIndexOf('/');
	return i <= 0 ? '/' : p.substring(0, i);
}

var _ywebPickerState = null;

// ywebShowPathPicker(startPath, onPick, opts)
//   onPick(path) erhaelt den gewaehlten Ordner (oder mit opts.filesToo:true
//   auch eine Datei). opts.filesToo zeigt zusaetzlich Dateien an.
function ywebShowPathPicker(startPath, onPick, opts) {
	opts = opts || {};
	_ywebPickerState = {onPick: onPick, filesToo: !!opts.filesToo, path: startPath || '/'};
	ywebModal('yweb-picker', ywebL('picker.title', 'Choose folder'),
		'<div id="yweb-picker-crumb" style="font-size:0.8rem;color:var(--pico-muted-color);margin-bottom:0.5rem;word-break:break-all"></div>' +
		'<ul id="yweb-picker-list" style="list-style:none;margin:0;padding:0;max-height:50vh;overflow:auto"></ul>' +
		'<div class="yweb-form-row" style="margin-top:0.8rem"><label></label>' +
		'<button type="button" class="yweb-btn primary" onclick="ywebPickerChoose()">' +
		ywebL('picker.choose_folder', 'Choose this folder') + '</button></div>');
	ywebPickerLoad(_ywebPickerState.path);
}

/* Over /api/v1 where the page loads yweb-v1.js: the picker then reaches the
   box's roots and what lies below them, as the file manager does. Its top
   level ('') is the list of roots, and a start outside them leads there. */
async function _ywebPickerRows(path) {
	if (path) {
		try {
			var listed = await ywebV1('GET', '/storage/files', {query: {path: path}});
			var roots0 = await ywebV1('GET', '/storage/roots');
			var isRoot = ((roots0 && roots0.items) || []).some(function(r) { return r.present && r.id === path; });
			return {path: path, parent: isRoot ? '' : ywebParentPath(path),
				entries: ((listed && listed.items) || []).map(function(e) {
					return {name: e.id, dir: e.kind === 'dir', full: path.replace(/\/+$/, '') + '/' + e.id};
				})};
		} catch (e) {
			if (e.code !== 'outside-roots' && e.code !== 'no-such-name') throw e;
		}
	}
	var roots = await ywebV1('GET', '/storage/roots');
	return {path: '', parent: null, entries: ((roots && roots.items) || []).filter(function(r) { return r.present; })
		.map(function(r) { return {name: r.id, dir: true, full: r.id}; })};
}

async function ywebPickerLoad(path) {
	var listEl = document.getElementById('yweb-picker-list');
	var crumbEl = document.getElementById('yweb-picker-crumb');
	if (!listEl) return;
	listEl.innerHTML = '<li style="padding:0.5rem;color:var(--pico-muted-color)">' + ywebL('msg.loading', 'Loading...') + '</li>';
	// both: a page that loads yweb-v1.js without yweb-dom.js keeps the old list
	if (typeof ywebV1 === 'function' && typeof ywebEl === 'function') {
		var rows;
		try {
			rows = await _ywebPickerRows(path);
		} catch (e) {
			ywebShowToast(ywebL('picker.not_readable', 'Directory not readable') + ': ' + e.message, true);
			return;
		}
		_ywebPickerState.path = rows.path;
		if (crumbEl) crumbEl.textContent = rows.path || ywebL('picker.roots', 'Permitted folders');
		var chooseBtn = document.querySelector('#yweb-picker .yweb-btn.primary');
		if (chooseBtn) chooseBtn.disabled = !rows.path;	// the list of roots is no folder
		// Rows as elements with their handlers, so a folder name is text
		// whatever it holds (yweb-dom.js comes with yweb-v1.js)
		var row = function(label, onClick) {
			return ywebEl('li', {}, ywebEl('button', {type: 'button', className: 'yweb-btn',
				style: {width: '100%', 'text-align': 'left', border: 'none'}, on: {click: onClick}}, label));
		};
		var items = [];
		if (rows.parent !== null)
			items.push(row('↑ ..', function() { ywebPickerLoad(rows.parent); }));
		rows.entries.forEach(function(e) {
			if (!e.dir && !_ywebPickerState.filesToo) return;
			items.push(row((e.dir ? '📁' : '📄') + ' ' + e.name,
				function() { if (e.dir) ywebPickerLoad(e.full); else ywebPickerPickFile(e.full); }));
		});
		if (!items.length)
			items.push(ywebEl('li', {style: {padding: '0.5rem', color: 'var(--pico-muted-color)'}}, ywebL('picker.empty', 'empty')));
		listEl.replaceChildren.apply(listEl, items);
		return;
	}
	var data;
	try {
		data = await ywebApi('file', {action: 'list', path: path});
	} catch (e) {
		ywebShowToast(ywebL('picker.not_readable', 'Directory not readable') + ': ' + e.message, true);
		return;
	}
	_ywebPickerState.path = path;
	if (crumbEl) crumbEl.textContent = path;
	var entries = (data.data && data.data.filelist) || data.filelist || [];
	var html = '';
	if (path !== '/')
		html += '<li><button type="button" class="yweb-btn" style="width:100%;text-align:left;border:none" ' +
			'onclick="ywebPickerLoad(' + escapeHtml(JSON.stringify(ywebParentPath(path))) + ')">↑ ..</button></li>';
	entries.forEach(function(e) {
		if (!e.name || e.name === '.' || e.name === '..') return;
		var isDir = (e.type_str === 'dir' || e.type === 4 || e.type === '4');
		if (!isDir && !_ywebPickerState.filesToo) return;
		var full = e.fullname || (path.replace(/\/+$/, '') + '/' + e.name);
		// escaped as a whole: the attribute is decoded before the handler is
		// read, so an & in a name must not become a quote there
		var arg = escapeHtml(JSON.stringify(full));
		var icon = isDir ? '📁' : '📄';
		var action = isDir ? 'ywebPickerLoad(' + arg + ')' : 'ywebPickerPickFile(' + arg + ')';
		html += '<li><button type="button" class="yweb-btn" style="width:100%;text-align:left;border:none" onclick="' + action + '">' +
			icon + ' ' + escapeHtml(e.name) + '</button></li>';
	});
	listEl.innerHTML = html || '<li style="padding:0.5rem;color:var(--pico-muted-color)">' + ywebL('picker.empty', 'empty') + '</li>';
}

function ywebPickerChoose() {
	var st = _ywebPickerState;
	ywebCloseModal('yweb-picker');
	if (st && typeof st.onPick === 'function') st.onPick(st.path);
}

function ywebPickerPickFile(full) {
	var st = _ywebPickerState;
	ywebCloseModal('yweb-picker');
	if (st && typeof st.onPick === 'function') st.onPick(full);
}

// The EPG dialog, recording and streaming of a channel live in yweb-v1.js,
// which the pages offering them load after this file.

// Timer constants — shared across all pages. Interne Typnamen (EN,
// stabil fuer Logik); Anzeige-Labels kommen lokalisiert via ywebL —
// Y_Lang.yjs ist vor dieser Datei geladen, daher sind die Konstanten
// hier bereits uebersetzt.
const YWEB_TIMER_TYPES = {
	1: 'Shutdown', 2: 'Nextprogram', 3: 'Zapto', 4: 'Standby',
	5: 'Record', 6: 'Remind', 7: 'Sleeptimer', 8: 'Plugin', 9: 'Immediate Record'
};

const YWEB_TIMER_TYPE_LABELS = {
	1: ywebL('timer.type.1', 'Shutdown'), 3: ywebL('timer.type.3', 'Zap'),
	4: ywebL('timer.type.4', 'Standby'), 5: ywebL('timer.type.5', 'Record'),
	6: ywebL('timer.type.6', 'Reminder'), 7: ywebL('timer.type.7', 'Sleep timer'),
	8: ywebL('timer.type.8', 'Plugin'), 9: ywebL('timer.type.9', 'Instant record')
};

const YWEB_TIMER_STATE_LABELS = {
	0: ywebL('timer.state.0', 'Scheduled'), 1: ywebL('timer.state.1', 'Announced'),
	2: ywebL('timer.state.2', 'Active'), 3: ywebL('timer.state.3', 'Finished'),
	4: ywebL('timer.state.4', 'Cancelled')
};

const YWEB_REPEAT_LABELS = {
	'once': ywebL('timer.rep.once', 'Once'), 'daily': ywebL('timer.rep.daily', 'Daily'),
	'weekly': ywebL('timer.rep.weekly', 'Weekly'), 'biweekly': ywebL('timer.rep.biweekly', 'Every 2 weeks'),
	'fourweekly': ywebL('timer.rep.fourweekly', 'Every 4 weeks'),
	'monthly': ywebL('timer.rep.monthly', 'Monthly'), 'byeventdescription': ywebL('timer.rep.bydesc', 'By description')
};

// Timer helper functions — shared across all pages
function ywebGetTimerType(timer) {
	var n = parseInt(timer.type_number) || 0;
	return YWEB_TIMER_TYPE_LABELS[n] || YWEB_TIMER_TYPES[n] || 'Timer';
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
// opts.variant 'ok': Akzent-Knopf fuer nicht-destruktive Rueckfragen
// (Standby, Neustart-Frage o.ae.); Default bleibt rot (destruktiv)
function ywebShowConfirm(title, message, confirmLabel, onConfirm, opts) {
	var confirmClass = (opts && opts.variant === 'ok') ? 'confirm-ok' : 'confirm-delete';
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
				'<button class="confirm-cancel">' + ywebL('msg.cancel', 'Cancel') + '</button>' +
				'<button class="' + confirmClass + '">' + confirmLabel + '</button>' +
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
			btn.textContent = ywebL('msg.error', 'Error');
			setTimeout(function() { backdrop.remove(); }, 1500);
		}
	};
	document.body.appendChild(backdrop);
}

// Prompt dialog — shared: Textfeld mit OK/Abbrechen im Look des
// Confirm-Dialogs, Ersatz fuer natives prompt(). onOk(value) darf
// async sein; Enter bestaetigt, Escape/Backdrop bricht ab.
function ywebShowPrompt(title, label, value, onOk) {
	var backdrop = document.createElement('div');
	backdrop.className = 'yweb-confirm-backdrop';
	backdrop.onclick = function(e) {
		if (e.target === backdrop) backdrop.remove();
	};
	backdrop.innerHTML =
		'<div class="yweb-confirm-dialog">' +
			'<h4></h4>' +
			'<label class="yweb-prompt-label"></label>' +
			'<input type="text" class="yweb-prompt-input" maxlength="255">' +
			'<div class="yweb-confirm-actions">' +
				'<button class="confirm-cancel">' + ywebL('msg.cancel', 'Cancel') + '</button>' +
				'<button class="confirm-ok">OK</button>' +
			'</div>' +
		'</div>';
	backdrop.querySelector('h4').textContent = title;
	var lab = backdrop.querySelector('.yweb-prompt-label');
	if (label) lab.textContent = label; else lab.remove();
	var inp = backdrop.querySelector('.yweb-prompt-input');
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
			btn.textContent = ywebL('msg.error', 'Error');
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
function ywebShowMessageDialog() {
	var pop = document.getElementById('yweb-message-pop');
	if (pop) { pop.classList.toggle('open'); return; }

	pop = document.createElement('div');
	pop.id = 'yweb-message-pop';
	pop.className = 'yweb-popover open';
	pop.innerHTML =
		'<button type="button" class="yweb-popover-close" onclick="document.getElementById(\'yweb-message-pop\').classList.remove(\'open\')" aria-label="Close">\u00d7</button>' +
		'<form onsubmit="return false">' +
			'<input type="text" id="yweb-msg-text" placeholder="' + ywebL('tools.message_ph', 'Enter message\u2026') + '" maxlength="255">' +
			'<div class="yweb-popover-actions">' +
				'<small id="yweb-msg-result">\u00a0</small>' +
				'<button type="button" class="yweb-btn" onclick="ywebSendMessage(\'nmsg\')">' + ywebL('tools.message', 'Message') + '</button>' +
				'<button type="button" class="yweb-btn primary" onclick="ywebSendMessage(\'popup\')">Popup</button>' +
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
		if (typeof ywebV1 === 'function') {
			// nmsg: a hint that goes by itself; popup: a box that waits
			await ywebV1('POST', '/osd/message', {body: {text: text, kind: mode === 'popup' ? 'box' : 'hint'}});
		} else {
			var params = {};
			params[mode] = text;
			await ywebApi('message', params);
		}
		result.textContent = 'Gesendet';
		result.style.color = 'var(--yweb-success)';
		input.value = '';
		setTimeout(function() { result.textContent = '\u00a0'; result.style.color = ''; }, 2500);
	} catch (e) {
		result.textContent = ywebL('msg.error', 'Error') + ': ' + e.message;
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
				linksEl.innerHTML = html || '<span class="yweb-about-label">' + ywebL('about.no_links', 'No links available') + '</span>';
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
				// entry is mandatory unless the extension only ships a
				// settings category (settings-only) or only a runtime
				// script (script-only, entry_js)
				if (!m.entry && !m.entry_js && !(m.settings && m.settings.sections)) continue;
				m.dir = e.name;
				// Config-Ausgabe encodiert '-' als _bind_ (ConfigCGI)
				var ckBase = ('ext_' + m.dir).replace(/-/g, '_bind_');
				m.enabled = (yCfg[ckBase + '_enabled'] === '1');
				m.slot = yCfg[ckBase + '_slot'] || m.slot || 'tools';
				m.url = m.entry ? ('/extensions/' + encodeURIComponent(m.dir) + '/' + encodeURIComponent(m.entry)) : '';
				// Load the language pack for enabled extensions only;
				// reserved keys translate name/description
				if (m.enabled) {
					var xl = await ywebExtLangLoad(m.dir);
					if (xl['manifest.name']) m.name = xl['manifest.name'];
					if (xl['manifest.description']) m.description = xl['manifest.description'];
				}
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
			var ul = document.querySelector('.yweb-nav-links');
			if (ul) {
				var li = document.createElement('li');
				li.innerHTML = '<a href="' + escapeHtml(m.url) + '" data-page="ext-' + escapeHtml(m.dir) + '">' + escapeHtml(m.name) + '</a>';
				ul.appendChild(li);
			}
		} else if (m.slot === 'tools') {
			var grid = document.querySelector('.yweb-tools-grid');
			if (grid) {
				var a = document.createElement('a');
				a.href = m.url;
				a.className = 'yweb-tool-card';
				a.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2l3 6 6 1-4.5 4.4 1 6.6-5.5-3-5.5 3 1-6.6L3 9l6-1z"/></svg>' +
					'<span>' + escapeHtml(m.name) + '</span><small>' + escapeHtml(m.description || ywebL('ext.extension', 'Extension')) + '</small>';
				grid.appendChild(a);
			}
		} else if (m.slot === 'dashboard') {
			var dash = document.querySelector('.yweb-dashboard');
			if (dash) {
				var card = document.createElement('div');
				card.className = 'yweb-card';
				card.innerHTML = '<h3>' + escapeHtml(m.name) + '</h3>' +
					'<p><a href="' + escapeHtml(m.url) + '">' + escapeHtml(m.description || ywebL('ext.open', 'Open extension')) + '</a></p>';
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

		ywebCheckDefaultPassword();
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
function ywebBalanceGrid(selector, opts) {
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
/* dashboard.yhtm uses navigateToCard — alias */
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
