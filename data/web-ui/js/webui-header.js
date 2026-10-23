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
	tools: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z"/></svg>',
	files: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/></svg>',
	more: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><path d="M5 12h.01M12 12h.01M19 12h.01"/></svg>',
	tv: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="7" width="20" height="15" rx="2"/><polyline points="17 2 12 7 7 2"/></svg>',
	power: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3v9"/><path d="M6.3 6.3a8 8 0 1 0 11.4 0"/></svg>',
	plug: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 2v6M15 2v6M6 8h12v4a6 6 0 0 1-12 0z"/><path d="M12 18v4"/></svg>',
	puzzle: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10 3h4v3a2 2 0 1 0 4 0V3h3v7h-3a2 2 0 1 0 0 4h3v7H3V3h7z"/></svg>',
	server: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="2" width="20" height="8" rx="2"/><rect x="2" y="14" width="20" height="8" rx="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/></svg>',
	info: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="9"/><path d="M12 16v-4M12 8h.01"/></svg>',
	collapse: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="11 17 6 12 11 7"/><polyline points="18 17 13 12 18 7"/></svg>'
};

// Shared modal close button; its name is set once the texts are in
// (webuiDialogA11y), as some dialogs are built before
function _webuiCloseBtn(onclick) {
	return '<button type="button" class="webui-modal-close" aria-label="Close" data-l-aria-label="msg.close" onclick="' + onclick + '">' +
		'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true" focusable="false"><path d="M6 18L18 6M6 6l12 12"/></svg>' +
	'</button>';
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
				<h4>${webuiLH('about.versions', 'Versions')}</h4>
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
				<h4>${webuiLH('about.system', 'System')}</h4>
				<div class="webui-about-grid">
					<span class="webui-about-label">Box</span>
					<span class="webui-about-value" id="about-box">--</span>
					<span class="webui-about-label">${webuiLH('about.arch', 'Architecture')}</span>
					<span class="webui-about-value" id="about-arch">--</span>
					<span class="webui-about-label">${webuiLH('about.os', 'Operating system')}</span>
					<span class="webui-about-value" id="about-os">--</span>
				</div>
			</div>
			<div class="webui-about-section">
				<h4>${webuiLH('about.links', 'Links')}</h4>
				<div class="webui-about-links" id="about-links">
					<span class="webui-about-label">${webuiLH('msg.loading', 'Loading...')}</span>
				</div>
			</div>
			<div class="webui-about-section">
				<h4>${webuiLH('about.license', 'License & copyright')}</h4>
				<div class="webui-about-legal">
					<p><strong>Neutrino-Web-UI</strong> - ${webuiLH('about.based_on', 'based on yWeb by yjogol')}<br>
					<span id="about-copyright">Copyright © Tuxbox Project</span></p>
					<p>${webuiLH('about.licensed', 'Licensed under the')} <a href="https://www.gnu.org/licenses/old-licenses/gpl-2.0.html" target="_blank" rel="noopener">GNU GPL v2</a></p>
					<p><strong>${webuiLH('about.icons', 'Icons')}:</strong> ${webuiLH('about.icons_from', 'Parts of the icons are based on')} <a href="https://www.famfamfam.com/lab/icons/silk/" target="_blank" rel="noopener">Silk Icons</a> ${webuiLH('about.icons_by', 'by famfamfam, licensed under')} <a href="https://creativecommons.org/licenses/by/2.5/" target="_blank" rel="noopener">CC BY 2.5</a></p>
					<p><strong>${webuiLH('about.css', 'CSS framework')}:</strong> <a href="https://picocss.com" target="_blank" rel="noopener">Pico.css</a> (${webuiLH('about.mit', 'MIT License')})</p>
				</div>
			</div>
		</div>
	</div>
</div>
`;
}

/* The areas of the navigation and the pages of each (Phase 4). A page
   belongs to one area. On a phone the first four areas are a tab bar at the
   bottom, the rest come under "More", and the pages of the area shown stand
   in a row below the head when it has more than one; on a wider screen every
   page stands in the side bar, in the groups of WEBUI_SIDE_GROUPS. An
   extension's page joins the area its manifest or the extension manager
   names (webuiInjectExtensions); in Files it comes before the core's page
   there, elsewhere after them. An area opens its home: the page it names
   where that is there, else its first page of the interface itself, so which
   extensions are installed and how they are called does not change where a
   click on the area leads; the home stands first in the row. settings.html
   belongs to Files or System for three of its categories. */
const WEBUI_AREAS = [
	{id: 'now', key: 'nav.area_now', en: 'Dashboard', icon: 'dashboard', tab: true,
	 pages: [{page: 'dashboard', href: '/dashboard.html', key: 'nav.sub_overview', en: 'Dashboard', icon: 'dashboard'}]},
	{id: 'channels', key: 'nav.area_channels', en: 'Channels', icon: 'tv', tab: true,
	 pages: [{page: 'bouquets', href: '/bouquets.html', key: 'nav.sub_channels', en: 'Channels', icon: 'tv'}]},
	{id: 'epg', key: 'nav.epg', en: 'EPG', icon: 'epg', tab: true,
	 pages: [{page: 'epg', href: '/epg.html', key: 'nav.sub_guide', en: 'EPG', icon: 'epg',
		tipKey: 'nav.tip_guide', tipEn: 'Programme guide on a time line'}]},
	{id: 'timers', key: 'nav.timer', en: 'Timer', icon: 'timer', tab: true,
	 pages: [{page: 'timer', href: '/timer.html', key: 'nav.sub_timers', en: 'Timers', icon: 'timer'}]},
	{id: 'recordings', key: 'nav.recordings', en: 'Recordings', icon: 'recordings',
	 pages: [{page: 'recordings', href: '/recordings.html', key: 'nav.sub_archive', en: 'Recordings', icon: 'recordings'}]},
	{id: 'files', key: 'nav.area_files', en: 'Files', icon: 'files', extFirst: true, home: 'ext-files',
	 pages: [{page: 'settings#mounts', href: '/settings.html#mounts', key: 'reg.mounts', en: 'Network mounts', icon: 'server'}]},
	{id: 'system', key: 'nav.area_system', en: 'System', icon: 'tools',
	 pages: [{page: 'tools', href: '/tools.html', key: 'nav.sub_boxcontrol', en: 'Box control', icon: 'power'},
		{page: 'settings#plugins', href: '/settings.html#plugins', key: 'nav.sub_plugins', en: 'Plugins', icon: 'plug'},
		{page: 'settings#extensions', href: '/settings.html#extensions', key: 'nav.sub_extensions', en: 'Extensions', icon: 'puzzle'}]},
	{id: 'settings', key: 'nav.settings', en: 'Settings', icon: 'settings',
	 pages: [{page: 'settings', href: '/settings.html', key: 'nav.settings', en: 'Settings', icon: 'settings'}]}
];
// the areas an extension may name (its own area aside)
const WEBUI_EXT_AREAS = ['now', 'channels', 'epg', 'timers', 'recordings', 'files', 'system'];

/* The groups of the side bar: the interface's own television pages; the
   tools, which are the extensions' pages of the television areas, the file
   manager and those with an area of their own; the system with its own
   pages, the extensions that set up network shares beside Network mounts,
   and its extensions. Settings stands at the foot, About below it. core
   names the areas whose own pages a group takes, ext those whose extension
   pages: 'own' the areas of their own, 'files:home' the page Files opens
   with, 'files:rest' the others there. */
const WEBUI_SIDE_GROUPS = [
	{id: 'tv', key: 'nav.group_tv', en: 'Television', core: ['now', 'channels', 'epg', 'timers', 'recordings'], ext: []},
	{id: 'tools', key: 'nav.group_tools', en: 'Tools', core: [], ext: ['now', 'channels', 'epg', 'timers', 'recordings', 'files:home', 'own']},
	{id: 'system', key: 'nav.group_system', en: 'System', core: ['system', 'files'], ext: ['files:rest', 'system']}
];

var _webuiNavShown = '';	// the page the navigation was last drawn for
var _webuiNavExts = null;	// the extensions' places, once known

// the page shown, as the navigation names it: settings.html by the
// category its address names, where that category lives in another area
function _webuiNavPage(pageName) {
	if (pageName === 'settings') {
		var h = (location.hash || '').replace('#', '');
		if (h === 'mounts' || h === 'plugins' || h === 'extensions')
			return 'settings#' + h;
	}
	return pageName || '';
}

/* The extensions' places from the last discovery of this browser tab, so a
   page draws them at once and the navigation does not jump when the
   discovery comes in; [] where none was kept. */
function _webuiNavExtsKept() {
	if (_webuiNavExts)
		return _webuiNavExts;
	try {
		var kept = JSON.parse(sessionStorage.getItem('webui-nav-exts') || '[]');
		if (Array.isArray(kept))
			return kept;
	} catch (e) { /* none kept */ }
	return [];
}

// The areas with the extensions' pages put in, an extension with an area of
// its own (slot nav) as one more area at the end.
function _webuiNavAreas(exts) {
	var areas = WEBUI_AREAS.map(function(a) {
		return {id: a.id, label: webuiL(a.key, a.en), icon: a.icon, tab: !!a.tab, extFirst: !!a.extFirst, home: a.home || '',
			pages: a.pages.map(function(p) {
				return {page: p.page, href: p.href, label: webuiL(p.key, p.en), icon: p.icon || '',
					tip: p.tipKey ? webuiL(p.tipKey, p.tipEn) : ''};
			})};
	});
	var byId = {};
	areas.forEach(function(a) { byId[a.id] = a; });
	var added = {};
	(exts || []).forEach(function(x) {
		var entry = {page: 'ext-' + x.dir, href: x.url, label: x.name, ext: true, extIcon: x.icon || ''};
		if (x.own) {
			areas.push({id: 'ext-' + x.dir, label: x.name, icon: null, extIcon: x.icon || '', own: true, pages: [entry]});
			return;
		}
		var a = byId[x.area] || byId.system;
		(added[a.id] = added[a.id] || []).push(entry);
	});
	areas.forEach(function(a) {
		var ext = (added[a.id] || []).sort(function(p, q) { return p.label.localeCompare(q.label); });
		a.pages = a.extFirst ? ext.concat(a.pages) : a.pages.concat(ext);
		var home = a.pages.filter(function(p) { return p.page === a.home; })[0]
			|| a.pages.filter(function(p) { return !p.ext; })[0];
		if (home)
			a.pages = [home].concat(a.pages.filter(function(p) { return p !== home; }));
	});
	return areas;
}

function _webuiNavAreaOf(areas, page) {
	for (var i = 0; i < areas.length; i++)
		for (var k = 0; k < areas[i].pages.length; k++)
			if (areas[i].pages[k].page === page)
				return areas[i];
	return null;
}

function _webuiNavIcon(a) {
	if (a.icon && WEBUI_ICONS[a.icon])
		return WEBUI_ICONS[a.icon];
	return a.extIcon || WEBUI_ICONS.more;
}

// A page's link in the side bar: its icon and its name, the name its
// tooltip as well, which is all a narrow side bar shows of it
function _webuiSideLink(p, area, page) {
	var on = p.page === page;
	var a = document.createElement('a');
	a.href = p.href;
	a.className = 'webui-side-link' + (on ? ' active' : '');
	a.setAttribute('data-page', p.page);
	a.setAttribute('data-area', area);
	if (on)
		a.setAttribute('aria-current', 'page');
	a.title = p.tip || p.label;
	a.innerHTML = p.ext ? (p.extIcon || WEBUI_EXT_STAR) : (WEBUI_ICONS[p.icon] || WEBUI_ICONS.more);
	var label = document.createElement('span');
	label.className = 'webui-side-label';
	label.textContent = p.label;
	a.appendChild(label);
	var li = document.createElement('li');
	li.appendChild(a);
	return li;
}

// The side bar's groups with their pages, and Settings at its foot
function _webuiRenderSide(areas, page) {
	var host = document.getElementById('webui-side-groups');
	if (!host)
		return;
	var byId = {};
	areas.forEach(function(a) { byId[a.id] = a; });
	host.replaceChildren();
	WEBUI_SIDE_GROUPS.forEach(function(g) {
		var items = [];
		g.core.forEach(function(id) {
			((byId[id] || {}).pages || []).forEach(function(p) {
				if (!p.ext) items.push(_webuiSideLink(p, id, page));
			});
		});
		g.ext.forEach(function(spec) {
			var id = spec.split(':')[0], part = spec.split(':')[1] || '';
			(id === 'own' ? areas.filter(function(a) { return a.own; }) : [byId[id]]).forEach(function(a) {
				((a || {}).pages || []).forEach(function(p) {
					var home = p.page === a.home;
					if (p.ext && (!part || (part === 'home') === home))
						items.push(_webuiSideLink(p, a.id, page));
				});
			});
		});
		if (!items.length)
			return;
		var label = document.createElement('span');
		label.className = 'webui-side-group';
		label.id = 'webui-side-group-' + g.id;
		label.textContent = webuiL(g.key, g.en);
		var list = document.createElement('ul');
		list.className = 'webui-side-links';
		list.setAttribute('aria-labelledby', label.id);
		list.append.apply(list, items);
		host.append(label, list);
	});
	var end = document.getElementById('webui-side-end');
	if (end)
		end.replaceChildren.apply(end, ((byId.settings || {}).pages || []).map(function(p) {
			return _webuiSideLink(p, 'settings', page);
		}));
}

// The page's name in the head: as the navigation calls it, else as the
// page's own title does without the interface's name before it
function _webuiPageTitle(areas, page) {
	for (var i = 0; i < areas.length; i++)
		for (var k = 0; k < areas[i].pages.length; k++)
			if (areas[i].pages[k].page === page)
				return areas[i].pages[k].label;
	return document.title.replace(/^Neutrino-Web-UI\s*-\s*/, '');
}

// Draws the side bar, the page's name, the row of the shown area's pages
// and the tab bar.
function webuiRenderNav(pageName) {
	var page = _webuiNavPage(pageName);
	_webuiNavShown = pageName || '';
	var areas = _webuiNavAreas(_webuiNavExtsKept());
	var current = _webuiNavAreaOf(areas, page);
	var link = function(href, attrs, inner) {
		var a = document.createElement('a');
		a.href = href;
		Object.keys(attrs).forEach(function(k) { if (attrs[k] !== null) a.setAttribute(k, attrs[k]); });
		a.innerHTML = inner;
		return a;
	};

	_webuiRenderSide(areas, page);
	var title = document.getElementById('webui-page-title');
	if (title)
		title.textContent = _webuiPageTitle(areas, page);

	var sub = document.getElementById('webui-subnav');
	if (sub) {
		sub.replaceChildren();
		var pages = current ? current.pages : [];
		sub.hidden = pages.length < 2;
		sub.setAttribute('aria-label', current ? webuiL('nav.pages_of', 'Pages in %s').replace('%s', current.label) : '');
		pages.forEach(function(p) {
			var on = p.page === page;
			var li = document.createElement('li');
			li.appendChild(link(p.href, {'data-page': p.page, 'aria-current': on ? 'page' : null, 'class': on ? 'active' : null},
				escapeHtml(p.label)));
			sub.appendChild(li);
		});
	}

	var tabs = document.getElementById('webui-tabbar');
	if (tabs) {
		tabs.replaceChildren();
		var inTabs = false;
		areas.filter(function(a) { return a.tab; }).forEach(function(a) {
			var on = current === a;
			inTabs = inTabs || on;
			tabs.appendChild(link(a.pages[0].href, {'data-area': a.id, 'aria-current': on ? 'page' : null, 'class': on ? 'active' : null},
				_webuiNavIcon(a) + '<span>' + escapeHtml(a.label) + '</span>'));
		});
		var more = document.createElement('button');
		more.type = 'button';
		more.id = 'webui-tab-more';
		more.setAttribute('aria-haspopup', 'true');
		more.setAttribute('aria-expanded', 'false');
		if (current && !inTabs)
			more.className = 'active';
		more.innerHTML = WEBUI_ICONS.more + '<span>' + webuiLH('nav.more', 'More') + '</span>';
		more.addEventListener('click', webuiToggleMoreMenu);
		tabs.appendChild(more);
	}
	// the row of pages changes the head's height
	if (typeof webuiUpdateHeaderSpacer === 'function')
		webuiUpdateHeaderSpacer();
}

// "More" on the phone: the areas the tab bar has no room for, above it
function webuiToggleMoreMenu(ev) {
	ev.stopPropagation();
	var btn = document.getElementById('webui-tab-more');
	var menu = document.getElementById('webui-more-menu');
	if (menu) {
		menu.remove();
		if (btn) btn.setAttribute('aria-expanded', 'false');
		return;
	}
	var areas = _webuiNavAreas(_webuiNavExtsKept());
	var current = _webuiNavAreaOf(areas, _webuiNavPage(_webuiNavShown));
	menu = document.createElement('div');
	menu.id = 'webui-more-menu';
	menu.className = 'webui-more-menu';
	areas.filter(function(a) { return !a.tab; }).forEach(function(a) {
		var item = document.createElement('a');
		item.href = a.pages.length ? a.pages[0].href : '#';
		item.setAttribute('data-area', a.id);
		item.innerHTML = _webuiNavIcon(a) + '<span>' + escapeHtml(a.label) + '</span>';
		if (current === a) {
			item.className = 'active';
			item.setAttribute('aria-current', 'page');
		}
		menu.appendChild(item);
	});
	// About, which the side bar of a wider screen has at its foot
	var about = document.createElement('button');
	about.type = 'button';
	about.innerHTML = WEBUI_ICONS.info + '<span>' + webuiLH('about.link', 'About') + '</span>';
	about.addEventListener('click', function() {
		menu.remove();
		if (btn) btn.setAttribute('aria-expanded', 'false');
		webuiShowAbout();
	});
	menu.appendChild(about);
	document.body.appendChild(menu);
	if (btn) btn.setAttribute('aria-expanded', 'true');
	// Escape closes it and hands the focus back to "More", and so does Tab
	// on its last entry, as the menu is the last thing on the page; leaving
	// it by Shift+Tab or a click closes it as well
	menu.addEventListener('keydown', function(e) {
		var links = menu.querySelectorAll('a, button');
		var out = e.key === 'Tab' && !e.shiftKey && document.activeElement === links[links.length - 1];
		if (e.key !== 'Escape' && !out)
			return;
		e.preventDefault();
		menu.remove();
		if (btn) {
			btn.setAttribute('aria-expanded', 'false');
			btn.focus();
		}
	});
	menu.addEventListener('focusout', function(e) {
		if (!e.relatedTarget || menu.contains(e.relatedTarget) || e.relatedTarget === btn)
			return;
		menu.remove();
		if (btn)
			btn.setAttribute('aria-expanded', 'false');
	});
	var first = menu.querySelector('a');
	if (first) first.focus();
	setTimeout(function() {
		document.addEventListener('click', function close() {
			document.removeEventListener('click', close);
			var m = document.getElementById('webui-more-menu');
			if (m) m.remove();
			var b = document.getElementById('webui-tab-more');
			if (b) b.setAttribute('aria-expanded', 'false');
		});
	}, 0);
}

/* The side bar of a wider screen: the interface's name and the box's, the
   pages in their groups (webuiRenderNav), Settings, About and the switch to
   a narrow bar of icons, which the browser keeps (webuiToggleSide). */
function webuiGetSideHTML() {
	return `
<aside class="webui-side" id="webui-side">
	<a href="/" class="webui-side-brand" title="${WEBUI_CONFIG.title}">
		<span class="webui-mark" aria-hidden="true">N</span>
		<span class="webui-side-name"><b>${WEBUI_CONFIG.title}</b><small id="webui-side-host"></small></span>
	</a>
	<nav class="webui-side-nav" aria-label="${webuiLH('nav.pages', 'Pages')}">
		<div class="webui-side-groups" id="webui-side-groups"></div>
		<ul class="webui-side-links webui-side-end" id="webui-side-end"></ul>
	</nav>
	<div class="webui-side-foot">
		<button type="button" class="webui-side-link" onclick="webuiShowAbout()" title="${webuiLH('about.tip', 'Versions, licence and links')}">
			${WEBUI_ICONS.info}<span class="webui-side-label">${webuiLH('about.link', 'About')}</span>
		</button>
		<button type="button" class="webui-side-link webui-side-toggle" id="webui-side-toggle" onclick="webuiToggleSide()" aria-pressed="false" title="${webuiLH('nav.narrow_tip', 'Side bar narrow or wide')}">
			${WEBUI_ICONS.collapse}<span class="webui-side-label">${webuiLH('nav.narrow', 'Collapse')}</span>
		</button>
	</div>
</aside>`;
}

// The side bar narrow or wide, as it was last left in this browser
function _webuiSideNarrow() {
	try { return localStorage.getItem('webui-side') === 'narrow'; } catch (e) { return false; }
}

function _webuiSideShow(narrow) {
	document.documentElement.classList.toggle('webui-side-narrow', narrow);
	var t = document.getElementById('webui-side-toggle');
	if (t) t.setAttribute('aria-pressed', narrow ? 'true' : 'false');
}

function webuiToggleSide() {
	var narrow = !document.documentElement.classList.contains('webui-side-narrow');
	try {
		if (narrow) localStorage.setItem('webui-side', 'narrow');
		else localStorage.removeItem('webui-side');
	} catch (e) { /* not kept then */ }
	_webuiSideShow(narrow);
}

// The side bar put before the page, once
function _webuiInsertSide() {
	if (document.getElementById('webui-side'))
		return;
	document.body.insertAdjacentHTML('afterbegin', webuiGetSideHTML());
	document.documentElement.classList.add('webui-with-side');
	_webuiSideShow(_webuiSideNarrow());
}

// Generate header HTML; the navigation in it is drawn by webuiRenderNav
function webuiGetHeaderHTML(activePage) {
	return `
<header class="webui-header">
	<div class="webui-head-bar">
		<a href="/" class="webui-head-brand" aria-label="${WEBUI_CONFIG.title}" title="${WEBUI_CONFIG.title}">
			<span class="webui-mark" aria-hidden="true">N</span>
		</a>
		<h1 class="webui-page-title" id="webui-page-title"></h1>

		<div class="webui-header-right">
			<span class="webui-clock" id="webui-clock">--:--</span>
			<button type="button" class="webui-theme-toggle webui-lang-btn" id="webui-lang-btn" onclick="webuiToggleLangMenu(event)" title="${webuiLH('lang.title', 'Language')}" aria-label="${webuiLH('lang.title', 'Language')}" aria-haspopup="true" aria-expanded="false">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true" focusable="false">
					<circle cx="12" cy="12" r="9" stroke-width="2"/>
					<path stroke-width="2" d="M3 12h18M12 3a15 15 0 010 18M12 3a15 15 0 000 18"/>
				</svg>
				<span class="webui-lang-code" id="webui-lang-code"></span>
			</button>
			<button type="button" class="webui-theme-toggle" onclick="webuiToggleTheme()" title="${webuiLH('theme.toggle', 'Toggle theme')}" aria-label="${webuiLH('theme.toggle', 'Toggle theme')}">
				<svg id="webui-theme-icon-dark" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M20.354 15.354A9 9 0 018.646 3.646 9.003 9.003 0 0012 21a9.003 9.003 0 008.354-5.646z"/>
				</svg>
				<svg id="webui-theme-icon-light" class="webui-hidden" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z"/>
				</svg>
			</button>
			<button type="button" class="webui-theme-toggle" id="webui-logout-btn" hidden onclick="webuiLogout()" aria-label="${webuiLH('hdr.logout', 'Log out')}" title="${webuiLH('hdr.logout', 'Log out')}">
				<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor" aria-hidden="true">
					<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/>
					<polyline stroke-linecap="round" stroke-linejoin="round" stroke-width="2" points="16 17 21 12 16 7"/>
					<line stroke-linecap="round" stroke-linejoin="round" stroke-width="2" x1="21" y1="12" x2="9" y2="12"/>
				</svg>
			</button>
		</div>
	</div>
	<nav class="webui-subnav-nav"><ul class="webui-subnav" id="webui-subnav" hidden></ul></nav>
	<nav class="webui-tabbar" id="webui-tabbar" aria-label="${webuiLH('nav.areas', 'Areas')}"></nav>
</header>`;
}

/* Theme Management. Light or dark is the device's setting
   (prefers-color-scheme) and follows it when it changes, until somebody
   picks one with the toggle: that choice is kept under 'webui-mode', and
   choosing what the device says anyway drops it again. The keys before it,
   'webui-theme' and 'yweb-theme', were written on every page view, so only
   a light kept there was somebody's choice (dark was the default); it counts
   until a choice is made again. The pages set the same before their first
   paint (the snippet in each page's head). */
function webuiSystemTheme() {
	try {
		return window.matchMedia && window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark';
	} catch (e) {
		return 'dark';
	}
}

function webuiChosenTheme() {
	try {
		var t = localStorage.getItem('webui-mode');
		if (t === 'light' || t === 'dark')
			return t;
		if ((localStorage.getItem('webui-theme') || localStorage.getItem('yweb-theme')) === 'light')
			return 'light';
	} catch (e) { /* no site data: the device decides */ }
	return null;
}

function webuiGetTheme() {
	return webuiChosenTheme() || webuiSystemTheme();
}

// shown, not kept
function webuiApplyTheme(theme) {
	document.documentElement.setAttribute('data-theme', theme);
	webuiUpdateThemeIcon(theme);
}

// somebody's choice: kept, unless it is what the device says anyway
function webuiSetTheme(theme) {
	try {
		if (theme === webuiSystemTheme())
			localStorage.removeItem('webui-mode');
		else
			localStorage.setItem('webui-mode', theme);
		localStorage.removeItem('webui-theme');
		localStorage.removeItem('yweb-theme');
	} catch (e) { /* not kept then */ }
	webuiApplyTheme(theme);
}

// while nobody chose, the page follows the device as it changes
(function() {
	try {
		var mq = window.matchMedia('(prefers-color-scheme: light)');
		var follow = function() {
			if (!webuiChosenTheme())
				webuiApplyTheme(webuiSystemTheme());
		};
		if (mq.addEventListener)
			mq.addEventListener('change', follow);
		else if (mq.addListener)
			mq.addListener(follow);
	} catch (e) { /* an old browser keeps what it showed */ }
})();

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

/* An extension page's texts taken into the page's own: its language pack for
   the language the box shows its pages in, laid beside the interface's texts,
   so the page marks its texts with data-l as the interface's pages do and asks
   webuiL in its scripts. A key the interface has stays the interface's. Done
   once the interface's texts and the pack are in and the page's marked texts
   are set again; a page runs what shows texts after it. */
async function webuiExtTexts(dir) {
	await window.webuiTexts;
	var pack = await webuiExtLangLoad(dir);
	var texts = window.WEBUI_L || (window.WEBUI_L = {});
	Object.keys(pack).forEach(function(k) {
		if (!Object.prototype.hasOwnProperty.call(texts, k)) texts[k] = pack[k];
	});
	if (typeof webuiI18nApply === 'function') webuiI18nApply();
}

async function webuiToggleLangMenu(ev) {
	ev.stopPropagation();
	var opener = ev.currentTarget;
	var menu = document.getElementById('webui-lang-menu');
	var shut = function(back) {
		var m = document.getElementById('webui-lang-menu');
		if (m) m.remove();
		opener.setAttribute('aria-expanded', 'false');
		if (back) opener.focus();
	};
	if (menu) { shut(false); return; }
	var r = opener.getBoundingClientRect();
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
	menu.setAttribute('role', 'group');
	menu.setAttribute('aria-label', webuiL('lang.title', 'Language'));
	var chosen = null;
	langs.forEach(function(l) {
		var b = document.createElement('button');
		b.type = 'button';
		b.textContent = l.name;
		b.lang = l.code;
		if (l.code === answer.selected) {
			b.classList.add('active');
			b.setAttribute('aria-current', 'true');
			chosen = b;
		}
		b.onclick = function() { webuiSetLanguage(l.code); };
		menu.appendChild(b);
	});
	// Escape closes it and hands the focus back to the button, and so does
	// Tab on its last entry, as the menu is the last thing on the page;
	// leaving it by Shift+Tab closes it as well
	menu.addEventListener('keydown', function(e) {
		var all = menu.querySelectorAll('button');
		if (e.key === 'Escape' || (e.key === 'Tab' && !e.shiftKey && document.activeElement === all[all.length - 1])) {
			e.preventDefault();
			shut(true);
		}
	});
	menu.addEventListener('focusout', function(e) {
		if (e.relatedTarget && !menu.contains(e.relatedTarget) && e.relatedTarget !== opener)
			shut(false);
	});
	menu.style.top = (r.bottom + 4) + 'px';
	menu.style.right = Math.max(4, window.innerWidth - r.right) + 'px';
	document.body.appendChild(menu);
	opener.setAttribute('aria-expanded', 'true');
	(chosen || menu.querySelector('button')).focus();
	setTimeout(function() {
		document.addEventListener('click', function close() {
			document.removeEventListener('click', close);
			shut(false);
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
		webuiToastError(webuiL('msg.error', 'Error'), e);
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
   which the information page shows; {interfaces: true} asks for the
   interfaces alone, as the dashboard's network card needs them. */
async function webuiSystemInfo(full) {
	var want = (full && typeof full === 'object') ? full : {mounts: !!full, interfaces: !!full};
	var read = async function(path) {
		try {
			var r = await fetch('/api/v1' + path, {credentials: 'same-origin', cache: 'no-store'});
			return r.ok ? r.json() : null;
		} catch (e) { return null; }
	};
	var all = await Promise.all([read('/system/info'), want.mounts ? read('/storage/mounts') : null,
		want.interfaces ? read('/network/interfaces') : null]);
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
		if (seen[m.device]) return;
		// a share that did not say how large it is in time stays in the list,
		// as one that is not answering, rather than going missing
		if (m.answered === false) {
			seen[m.device] = true;
			disks.push({device: m.device, mount: m.id, fstype: m.fstype, answered: false});
			return;
		}
		if (!m.total) return;
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
   from the version.txt they ship. */
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
		var v = await fetch('/version.txt', {cache: 'no-cache'});
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

// The box's name under the interface's in the side bar
function webuiLoadHostname() {
	const el = document.getElementById('webui-side-host');
	if (!el) return;
	webuiSystemInfoOnce()
		.then(data => {
			el.textContent = (data && (data.hostname || (data.box && data.box.name))) || 'neutrino';
		})
		.catch(() => { el.textContent = 'neutrino'; });
}

// The page shown, marked in the navigation: drawn anew for it
function webuiSetActivePage(pageName) {
	webuiRenderNav(pageName);
}

// settings.html moves between areas with the category its address names
window.addEventListener('hashchange', function() {
	if (_webuiNavShown === 'settings')
		webuiRenderNav('settings');
});

// Initialize header
function webuiInitHeader(pageName) {
	_webuiToastHost();
	// Insert header at beginning of body
	const headerContainer = document.getElementById('webui-header-container');
	if (headerContainer) {
		headerContainer.innerHTML = webuiGetHeaderHTML();
	} else {
		document.body.insertAdjacentHTML('afterbegin', webuiGetHeaderHTML());
	}
	_webuiInsertSide();
	if (!document.getElementById('webui-about-modal'))
		document.body.insertAdjacentHTML('beforeend', webuiAboutHTML());

	// Set theme
	webuiApplyTheme(webuiGetTheme());

	// the navigation, marked for this page
	webuiRenderNav(pageName);

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

// Shared helper functions
function webuiPad2(n) {
	return String(n).padStart(2, '0');
}

/* A time of the guide, a timer or a recording (seconds since 1970) on the
   clock as the pages show it, "--:--" for none */
function webuiClock(sec) {
	return sec ? webuiFormatTime(new Date(sec * 1000)) : '--:--';
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

// The /api/v4/ fetch helper, for the few calls the pages still make there
// (the pages talk to /api/v1 through webui-v1.js):
//   webuiApi('timer')                            -> parsed JSON payload
//   webuiApi('timer', {action:'new', type:5})    -> envelope-checked action
//   webuiApi('setmode', {status:''})             -> key-only query param
//   webuiApi('rcem', 'KEY_OK', {raw:true})       -> raw text
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
		var err = new Error(errText || (endpoint + ': HTTP ' + resp.status));
		err.status = resp.status;
		throw err;
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
				webuiShowConfirm(webuiL('dash.standby', 'Standby'), webuiLH('sb.confirm', 'Put the box into standby?'), webuiL('dash.standby', 'Standby'),
					send, {variant: 'ok'});
				return;
			}
		} catch (e) {
			// a sign-in that was cancelled is not a yes to the key
			if (v1 && e.status === 403) return;
		}
	}
	// a key the box did not take says so (a cancelled sign-in does not)
	try {
		return await send();
	} catch (e) {
		webuiToastError(webuiL('rc.key_fail', 'The key was not sent'), e);
	}
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

		/* Zone B: Core thumb zone (VOL | D-Pad | CH); the pad's corners hold
		   Menu and Exit above it, Info and EPG below it, where the remote
		   controls of the boxes have them */
		'<div class="webui-rc-core">' +
			'<div class="webui-rc-rocker">' +
				'<span class="webui-rc-rocker-label">' + escapeHtml(webuiL('rc.vol', 'VOL')) + '</span>' +
				repBtn('webui-rc-rocker-top', 'KEY_VOLUMEUP', '+', webuiL('rc.vol_up', 'Volume up')) +
				repBtn('webui-rc-rocker-bot', 'KEY_VOLUMEDOWN', '\u2212', webuiL('rc.vol_down', 'Volume down')) +
			'</div>' +
			'<div class="webui-rc-pad">' +
				'<div class="webui-rc-pair">' +
					btn('webui-rc-func', 'KEY_SETUP', webuiL('rc.menu', 'Menu')) +
					btn('webui-rc-func', 'KEY_EXIT', webuiL('rc.exit', 'Exit')) +
				'</div>' +
				'<div class="webui-rc-dpad">' +
					'<div></div>' + iconBtn('webui-rc-arrow', 'KEY_UP', webuiL('rc.up', 'Up'), svgUp) + '<div></div>' +
					iconBtn('webui-rc-arrow', 'KEY_LEFT', webuiL('rc.left', 'Left'), svgLeft) +
					btn('webui-rc-ok', 'KEY_OK', webuiL('rc.ok', 'OK')) +
					iconBtn('webui-rc-arrow', 'KEY_RIGHT', webuiL('rc.right', 'Right'), svgRight) +
					'<div></div>' + iconBtn('webui-rc-arrow', 'KEY_DOWN', webuiL('rc.down', 'Down'), svgDown) + '<div></div>' +
				'</div>' +
				'<div class="webui-rc-pair">' +
					btn('webui-rc-func', 'KEY_HELP', webuiL('rc.info', 'Info')) +
					btn('webui-rc-func', 'KEY_EPG', webuiL('rc.epg', 'EPG')) +
				'</div>' +
			'</div>' +
			'<div class="webui-rc-rocker">' +
				'<span class="webui-rc-rocker-label">' + escapeHtml(webuiL('rc.ch', 'CH')) + '</span>' +
				repBtn('webui-rc-rocker-top', 'KEY_PAGEUP', '+', webuiL('rc.ch_up', 'Channel up')) +
				repBtn('webui-rc-rocker-bot', 'KEY_PAGEDOWN', '\u2212', webuiL('rc.ch_down', 'Channel down')) +
			'</div>' +
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

		/* Zone F: Text / Fav / Timer (always visible) */
		'<div class="webui-rc-row webui-rc-nav">' +
			btn('webui-rc-func', 'KEY_TEXT', webuiL('rc.text_key', 'Text'), webuiL('rc.text', 'Teletext')) +
			btn('webui-rc-func', 'KEY_FAVORITES', webuiL('rc.fav', 'Fav')) +
			btn('webui-rc-func', 'KEY_TIME', webuiL('rc.time', 'Timer')) +
		'</div>' +

		/* Numpad trigger — reveals the numpad sheet on every viewport */
		'<button type="button" class="webui-rc-btn webui-rc-sheet-trigger" aria-label="' + escapeHtml(webuiL('rc.numpad', 'Numpad')) + '" title="' + escapeHtml(webuiL('tip.numpad', 'The number keys and the other keys of the remote')) + '" aria-expanded="false" onclick="webuiToggleRcSheet(\'' + id + '\')">123' +
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
		// a keyboard's Enter or Space: one press, no repeat (a pointer's
		// click has sent its key on pointerdown already)
		btn.addEventListener('click', function(e) {
			if (e.detail === 0)
				fn(key);
		});
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

var _webuiDialogSeq = 0;

/* Makes a dialog drawn by hand behave as one: its role, named by its title,
   the focus inside it (on opts.focus, else on its first control), Tab kept
   within it, Escape calling opts.close, and once released the focus back
   where it was. opts.alert makes it an alertdialog, for a question that
   wants an answer. Returns release(). */
function webuiDialogA11y(dialog, opts) {
	opts = opts || {};
	var before = document.activeElement;
	dialog.setAttribute('role', opts.alert ? 'alertdialog' : 'dialog');
	dialog.setAttribute('aria-modal', 'true');
	var title = dialog.querySelector('h1, h2, h3, h4');
	if (title) {
		if (!title.id)
			title.id = 'webui-dialog-' + (++_webuiDialogSeq);
		dialog.setAttribute('aria-labelledby', title.id);
	}
	dialog.querySelectorAll('[data-l-aria-label]').forEach(function(el) {
		el.setAttribute('aria-label', webuiL(el.getAttribute('data-l-aria-label'), el.getAttribute('aria-label') || ''));
	});
	var controls = function() {
		return Array.prototype.filter.call(
			dialog.querySelectorAll('a[href], button, input, select, textarea, [tabindex]:not([tabindex="-1"])'),
			function(el) { return !el.disabled && el.offsetParent !== null; });
	};
	var onKey = function(e) {
		if (e.key === 'Escape' && opts.close) {
			e.preventDefault();
			e.stopPropagation();
			opts.close();
			return;
		}
		if (e.key !== 'Tab')
			return;
		var all = controls();
		if (!all.length) {
			e.preventDefault();
			return;
		}
		if (e.shiftKey && document.activeElement === all[0]) {
			e.preventDefault();
			all[all.length - 1].focus();
		} else if (!e.shiftKey && document.activeElement === all[all.length - 1]) {
			e.preventDefault();
			all[0].focus();
		}
	};
	dialog.addEventListener('keydown', onKey);
	var start = opts.focus || controls()[0];
	if (start) {
		start.focus();
	} else {
		dialog.tabIndex = -1;
		dialog.focus();
	}
	// back only to what can still take the focus: an opener that went or
	// was hidden meanwhile leaves it to the caller
	return function release() {
		dialog.removeEventListener('keydown', onKey);
		if (before && typeof before.focus === 'function' && document.contains(before) && before.getClientRects().length)
			before.focus();
	};
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
	if (backdrop._webuiRelease)
		backdrop._webuiRelease();
	backdrop._webuiRelease = webuiDialogA11y(backdrop.querySelector('.webui-epg-modal'),
		{close: function() { webuiCloseModal(id); }});
	return backdrop;
}

function webuiCloseModal(id) {
	var backdrop = document.getElementById(id);
	if (backdrop) {
		backdrop.classList.remove('open');
		if (backdrop._webuiRelease) {
			backdrop._webuiRelease();
			backdrop._webuiRelease = null;
		}
	}
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
		'<div class="webui-form-row" style="margin-top:0.8rem"><span></span>' +
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
	listEl.setAttribute('aria-busy', 'true');
	// both: a page that loads webui-v1.js without webui-dom.js keeps the old list
	var v1 = typeof webuiV1 === 'function' && typeof webuiEl === 'function';
	/* A folder that cannot be read says so in the list, where the loading
	   text stood, with the way back to the folders the box offers. */
	var refused = function(e) {
		listEl.removeAttribute('aria-busy');
		listEl.innerHTML = '<li role="alert" style="padding:0.5rem;color:var(--webui-danger)">' +
			webuiLH('picker.not_readable', 'Directory not readable') + ': ' + escapeHtml(webuiErrText(e)) + '</li>' +
			(path && path !== '/' ? '<li><button type="button" class="webui-btn" style="width:100%;text-align:left;border:none" onclick="webuiPickerLoad(' + (v1 ? '\'\'' : '\'/\'') + ')">↑ ' +
				webuiLH('picker.roots', 'Permitted folders') + '</button></li>' : '');
	};
	if (v1) {
		var rows;
		try {
			rows = await _webuiPickerRows(path);
		} catch (e) {
			refused(e);
			return;
		}
		listEl.removeAttribute('aria-busy');
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
		refused(e);
		return;
	}
	listEl.removeAttribute('aria-busy');
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
// onConfirm: async function(btn) — called when user clicks confirm, btn shows loading state
// opts.variant 'ok': Akzent-Knopf fuer nicht-destruktive Rueckfragen
// (Standby, Neustart-Frage o.ae.); Default bleibt rot (destruktiv), and
// then the focus starts on Cancel, so Enter does not destroy anything.
// title and confirmLabel are text; message is markup, and a caller escapes
// what it puts in (webuiLH for a text, escapeHtml for a name). Where
// onConfirm throws, the dialog stays and says why below the question.
function webuiShowConfirm(title, message, confirmLabel, onConfirm, opts) {
	var confirmClass = (opts && opts.variant === 'ok') ? 'confirm-ok' : 'confirm-delete';
	var backdrop = document.createElement('div');
	backdrop.className = 'webui-confirm-backdrop';
	backdrop.innerHTML =
		'<div class="webui-confirm-dialog">' +
			'<h4>' + escapeHtml(title) + '</h4>' +
			'<p>' + message + '</p>' +
			'<p class="webui-confirm-why" role="alert" hidden></p>' +
			'<div class="webui-confirm-actions">' +
				'<button type="button" class="confirm-cancel">' + webuiLH('msg.cancel', 'Cancel') + '</button>' +
				'<button type="button" class="' + confirmClass + '">' + escapeHtml(confirmLabel) + '</button>' +
			'</div>' +
		'</div>';
	var release = null;
	var close = function() {
		backdrop.remove();
		if (release) release();
	};
	backdrop.onclick = function(e) {
		if (e.target === backdrop) close();
	};
	var cancelBtn = backdrop.querySelector('.confirm-cancel');
	var okBtn = backdrop.querySelector('.' + confirmClass);
	var why = backdrop.querySelector('.webui-confirm-why');
	cancelBtn.onclick = close;
	okBtn.onclick = async function() {
		var btn = this;
		btn.disabled = true;
		btn.setAttribute('aria-busy', 'true');
		why.hidden = true;
		try {
			await onConfirm(btn);
			close();
		} catch (e) {
			console.error('Confirm action failed:', e);
			btn.disabled = false;
			btn.removeAttribute('aria-busy');
			if (!e || !e.cancelled) {
				why.textContent = webuiErrText(e);
				why.hidden = false;
			}
			btn.focus();
		}
	};
	document.body.appendChild(backdrop);
	release = webuiDialogA11y(backdrop.querySelector('.webui-confirm-dialog'),
		{alert: true, close: close, focus: confirmClass === 'confirm-ok' ? okBtn : cancelBtn});
}

// Prompt dialog — shared: Textfeld mit OK/Abbrechen im Look des
// Confirm-Dialogs, Ersatz fuer natives prompt(). onOk(value) darf
// async sein; Enter bestaetigt, Escape/Backdrop bricht ab.
// The field is named by label, else by the title; an empty field or a
// refusal keeps the dialog open and what was typed, and says why.
function webuiShowPrompt(title, label, value, onOk) {
	var backdrop = document.createElement('div');
	backdrop.className = 'webui-confirm-backdrop';
	var fieldId = 'webui-prompt-' + (++_webuiDialogSeq);
	backdrop.innerHTML =
		'<div class="webui-confirm-dialog">' +
			'<h4></h4>' +
			'<label class="webui-prompt-label" for="' + fieldId + '"></label>' +
			'<input type="text" class="webui-prompt-input" id="' + fieldId + '" maxlength="255">' +
			'<p class="webui-confirm-why" role="alert" hidden></p>' +
			'<div class="webui-confirm-actions">' +
				'<button type="button" class="confirm-cancel">' + webuiLH('msg.cancel', 'Cancel') + '</button>' +
				'<button type="button" class="confirm-ok">' + webuiLH('msg.ok', 'OK') + '</button>' +
			'</div>' +
		'</div>';
	var dialog = backdrop.querySelector('.webui-confirm-dialog');
	dialog.querySelector('h4').textContent = title;
	var lab = backdrop.querySelector('.webui-prompt-label');
	var inp = backdrop.querySelector('.webui-prompt-input');
	var why = backdrop.querySelector('.webui-confirm-why');
	if (label) {
		lab.textContent = label;
	} else {
		lab.remove();
		inp.setAttribute('aria-label', title);
	}
	inp.value = value || '';
	var release = null;
	var close = function() {
		backdrop.remove();
		if (release) release();
	};
	var say = function(text) {
		why.textContent = text;
		why.hidden = !text;
		if (text)
			inp.setAttribute('aria-invalid', 'true');
		else
			inp.removeAttribute('aria-invalid');
	};
	var ok = async function(btn) {
		// Enter while the first OK is still running would send it twice
		if (btn.disabled) return;
		if (!inp.value.trim()) {
			say(webuiL('msg.empty_field', 'Please fill this in'));
			inp.focus();
			return;
		}
		say('');
		btn.disabled = true;
		btn.setAttribute('aria-busy', 'true');
		try {
			await onOk(inp.value.trim());
			close();
		} catch (e) {
			console.error('Prompt action failed:', e);
			btn.disabled = false;
			btn.removeAttribute('aria-busy');
			if (!e || !e.cancelled)
				say(webuiErrText(e));
			inp.focus();
		}
	};
	backdrop.onclick = function(e) {
		if (e.target === backdrop) close();
	};
	backdrop.querySelector('.confirm-cancel').onclick = close;
	backdrop.querySelector('.confirm-ok').onclick = function() { ok(this); };
	inp.onkeydown = function(e) {
		if (e.key === 'Enter') ok(backdrop.querySelector('.confirm-ok'));
	};
	document.body.appendChild(backdrop);
	release = webuiDialogA11y(dialog, {close: close, focus: inp});
	inp.select();
}

// Message popover — send ticker or popup messages to the box. A small
// dialog beside the page: named, the field labelled, Escape closes it and
// gives the focus back.
var _webuiMessageOpener = null;

function _webuiCloseMessage() {
	var pop = document.getElementById('webui-message-pop');
	if (!pop || !pop.classList.contains('open'))
		return;
	pop.classList.remove('open');
	if (_webuiMessageOpener && document.contains(_webuiMessageOpener))
		_webuiMessageOpener.focus();
	_webuiMessageOpener = null;
}

// An element with a tip (title) of what it does, handed back for a list
// of children
function webuiTitled(el, text) {
	if (el && text)
		el.title = text;
	return el;
}

/* A help text behind a button, shared by the pages:
     <span class="webui-help">
       <button type="button" class="webui-btn webui-help-btn" aria-expanded="false"
         aria-controls="ID" onclick="webuiHelpToggle(this)">…</button>
       <div class="webui-help-panel" id="ID" hidden>…</div>
     </span>
   The panel opens below the button; the button again, Escape or a click
   elsewhere closes it, Escape giving the focus back to the button. */
function webuiHelpToggle(btn) {
	var panel = document.getElementById(btn.getAttribute('aria-controls'));
	if (!panel)
		return;
	if (btn._webuiHelpClose) {
		btn._webuiHelpClose(false);
		return;
	}
	var away = function(e) {
		if (!panel.contains(e.target) && !btn.contains(e.target))
			close(false);
	};
	var key = function(e) {
		if (e.key === 'Escape') {
			e.preventDefault();
			close(true);
		}
	};
	var close = function(back) {
		panel.hidden = true;
		btn.setAttribute('aria-expanded', 'false');
		document.removeEventListener('click', away, true);
		document.removeEventListener('keydown', key, true);
		btn._webuiHelpClose = null;
		if (back)
			btn.focus();
	};
	panel.hidden = false;
	btn.setAttribute('aria-expanded', 'true');
	btn._webuiHelpClose = close;
	// the click that opened it is past the document already
	document.addEventListener('click', away, true);
	document.addEventListener('keydown', key, true);
}

function webuiShowMessageDialog() {
	var pop = document.getElementById('webui-message-pop');
	if (pop && pop.classList.contains('open')) {
		_webuiCloseMessage();
		return;
	}
	_webuiMessageOpener = document.activeElement;
	if (pop) {
		pop.classList.add('open');
		pop.querySelector('#webui-msg-text').focus();
		return;
	}

	pop = document.createElement('div');
	pop.id = 'webui-message-pop';
	pop.className = 'webui-popover open';
	pop.setAttribute('role', 'dialog');
	pop.setAttribute('aria-label', webuiL('tools.message', 'Message'));
	pop.innerHTML =
		'<button type="button" class="webui-popover-close" onclick="_webuiCloseMessage()" aria-label="' + webuiLH('msg.close', 'Close') + '">\u00d7</button>' +
		'<form onsubmit="return false">' +
			'<input type="text" id="webui-msg-text" aria-label="' + webuiLH('tools.message', 'Message') + '" placeholder="' + webuiLH('tools.message_ph', 'Enter message\u2026') + '" maxlength="255">' +
			'<div class="webui-popover-actions">' +
				'<small id="webui-msg-result" role="status">\u00a0</small>' +
				'<button type="button" class="webui-btn" onclick="webuiSendMessage(\'nmsg\')">' + webuiLH('tools.message', 'Message') + '</button>' +
				'<button type="button" class="webui-btn primary" onclick="webuiSendMessage(\'popup\')">' + webuiLH('msg.popup', 'Popup') + '</button>' +
			'</div>' +
		'</form>';
	document.body.appendChild(pop);
	pop.addEventListener('keydown', function(e) {
		if (e.key === 'Escape') {
			e.preventDefault();
			_webuiCloseMessage();
		}
	});

	var input = pop.querySelector('#webui-msg-text');
	input.onkeydown = function(e) { if (e.key === 'Enter') webuiSendMessage('nmsg'); };
	input.focus();

	// Close on outside click; listened for after the click that opened it
	setTimeout(function() {
		document.addEventListener('click', function(e) {
			if (pop.classList.contains('open') && !pop.contains(e.target) && !e.target.closest('[onclick*="webuiShowMessageDialog"]'))
				_webuiCloseMessage();
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
		result.textContent = webuiL('msg.sent', 'Sent');
		result.style.color = 'var(--webui-success)';
		input.value = '';
		setTimeout(function() { result.textContent = '\u00a0'; result.style.color = ''; }, 2500);
	} catch (e) {
		result.textContent = webuiL('msg.error', 'Error') + ': ' + webuiErrText(e);
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

/* Why the box refused, in the language of the page. A refusal a user meets
   in everyday use has its own words; any other is told by the kind of
   answer, except on an English page, which keeps the box's own reason: it
   is English and says more. A request the box never answered says so, and
   an error the page made itself keeps its message. */
var WEBUI_ERR_TEXTS = {
	'playback-running': 'A file is playing on the box',
	'recording-running': 'A recording is running',
	'timeshift-running': 'Timeshift is running',
	'recording-playing': 'This recording is playing',
	'box-in-standby': 'The box is in standby',
	'recording-holds-tuner': 'A recording holds the tuner',
	'plugin-running': 'A plugin is still running on the box',
	'no-running-channel': 'No channel is running',
	'timer-exists': 'This timer exists already',
	'timer-in-the-past': 'The time lies in the past',
	'timer-without-channel': 'The timer needs a channel',
	'recording-without-duration': 'The recording needs an end',
	'name-taken': 'The name is taken',
	'bad-name': 'This name is not allowed',
	'not-empty': 'The folder is not empty',
	'medium-read-only': 'The medium is read-only',
	'outside-roots': 'Outside the folders the box offers',
	'is-a-root': 'A folder the box offers cannot be changed itself',
	'into-itself': 'A folder cannot go into itself',
	'cross-device': 'Cannot be moved to another drive',
	'empty-message': 'The message is empty',
	'message-too-long': 'The message is too long',
	'value-too-long': 'The value is too long',
	'out-of-range': 'The value is out of range',
	'not-a-number': 'Not a number',
	'not-a-listed-value': 'Not one of the values offered',
	'body-too-large': 'Too large for the box',
	'too-many-attempts': 'Too many attempts, wait a moment',
	'too-many-streams': 'Too many streams at once',
	'not-playable-in-browser': 'This channel cannot be played in the browser',
	'upstream-unreachable': 'The source could not be reached',
	'not-permitted': 'Not permitted',
	'no-authority': 'Not allowed from here',
	'backup-failed': 'The backup could not be made',
	'restore-refused': 'The box refused the backup file',
	'wake-not-sent': 'The wake-up packet was not sent',
	'script-missing': 'The box has no script for this',
	// one code for several refusals: its words by the kind of answer too
	'extension-refused.400': 'Signature or archive do not match',
	'extension-refused.409': 'Another extension is being installed or removed',
	'extension-refused.501': 'This image cannot check signed extensions'
};

function webuiErrText(e) {
	if (!e)
		return webuiL('err.unknown', 'Something went wrong');
	// a page that shows nothing after the sign-in was cancelled says why
	if (e.cancelled)
		return webuiL('err.sign_in', 'Only after signing in');
	var own = e.code && (WEBUI_ERR_TEXTS[e.code + '.' + e.status] ? e.code + '.' + e.status : WEBUI_ERR_TEXTS[e.code] ? e.code : '');
	if (own)
		return webuiL('err.' + own, WEBUI_ERR_TEXTS[own]);
	if (!e.status) {
		if (e instanceof TypeError && /fetch|network|load failed/i.test(e.message || ''))
			return webuiL('err.no_answer', 'The box did not answer');
		return e.message || webuiL('err.unknown', 'Something went wrong');
	}
	if ((document.documentElement.lang || 'en') === 'en' && e.message)
		return e.message;
	return webuiKindText(e.status);
}

/* A block that says why a list or a card holds nothing (Phase 4 S3): kind
   'loading', 'empty', 'error' or 'sign-in'. title and text are text;
   opts.retry adds a Reload button, opts.action ({label, onClick}) another
   one. An error is announced, a loading block marked busy. */
var WEBUI_STATE_ICONS = {
	loading: '<path d="M21 12a9 9 0 1 1-6.2-8.6"/>',
	empty: '<circle cx="12" cy="12" r="9"/><path d="M8 12h8"/>',
	error: '<path d="M12 3 2 21h20zM12 10v4M12 18h.01"/>',
	'sign-in': '<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>'
};

function webuiState(kind, title, text, opts) {
	opts = opts || {};
	var box = document.createElement('div');
	box.className = 'webui-state ' + kind;
	if (kind === 'loading')
		box.setAttribute('aria-busy', 'true');
	if (kind === 'error')
		box.setAttribute('role', 'alert');
	box.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">' +
		(WEBUI_STATE_ICONS[kind] || WEBUI_STATE_ICONS.empty) + '</svg>';
	if (title) {
		var strong = document.createElement('strong');
		strong.textContent = title;
		box.appendChild(strong);
	}
	if (text) {
		var p = document.createElement('p');
		p.textContent = text;
		box.appendChild(p);
	}
	var button = function(label, onClick, primary, tip) {
		var b = document.createElement('button');
		b.type = 'button';
		b.className = 'webui-btn' + (primary ? ' primary' : '');
		b.textContent = label;
		if (tip)
			b.title = tip;
		b.addEventListener('click', onClick);
		box.appendChild(b);
	};
	if (opts.action)
		button(opts.action.label, opts.action.onClick, true, opts.action.tip);
	if (opts.retry)
		button(webuiL('bc.reload', 'Reload'), opts.retry, !opts.action, webuiL('tip.retry', 'Ask the box again'));
	return box;
}

/* The block for a read that failed: a sign-in to offer where the user
   cancelled it (and the read again once signed in), else why, in the page's
   language, with Reload where opts.retry is given. opts.title names what
   could not be read. */
function webuiStateFor(e, opts) {
	opts = opts || {};
	if (e && e.cancelled && typeof webuiV1SignIn === 'function')
		return webuiState('sign-in', webuiL('state.sign_in', 'This needs a sign-in'), '', {
			action: {label: webuiL('state.sign_in_btn', 'Sign in'), onClick: async function() {
				if (await webuiV1SignIn() && opts.retry) opts.retry();
			}}});
	return webuiState('error', opts.title || webuiL('state.failed', 'Could not be read'), webuiErrText(e), {retry: opts.retry});
}

// One result of a request that writes several keys at once ({status, code,
// detail}), as the Error webuiErrText reads.
function webuiResultError(r) {
	r = r || {};
	var e = new Error(r.detail || r.code || (r.status ? String(r.status) : ''));
	e.status = r.status;
	e.code = r.code || '';
	return e;
}

// A refusal as a message, after what was tried (prefix, may be empty);
// nothing where the user cancelled the sign-in the refusal asked for. The
// box's own words go to the console where the message says something else.
function webuiToastError(prefix, e) {
	if (e && e.cancelled)
		return;
	var why = webuiErrText(e);
	webuiShowToast(prefix ? prefix + ': ' + why : why, true);
	if (e && e.message && e.message !== why)
		console.warn((prefix || 'refused') + ':', e.message);
}

/* Where the messages appear: one place for the page, there before the first
   message, so a screen reader announces what comes into it (an error at
   once, as an alert). */
function _webuiToastHost() {
	var host = document.getElementById('webui-toast-host');
	if (!host && document.body) {
		host = document.createElement('div');
		host.id = 'webui-toast-host';
		host.className = 'webui-toast-host';
		host.setAttribute('aria-live', 'polite');
		document.body.appendChild(host);
	}
	return host;
}

// Toast notification: one at a time, at the bottom and above the phone's
// tab bar; an error stays twice as long, and a click takes a message away.
function webuiShowToast(message, isError) {
	const existing = document.getElementById('webui-toast');
	if (existing) existing.remove();

	const toast = document.createElement('div');
	toast.id = 'webui-toast';
	toast.className = 'webui-toast ' + (isError ? 'error' : 'ok');
	if (isError) toast.setAttribute('role', 'alert');
	toast.textContent = message;
	toast.addEventListener('click', function() { toast.remove(); });
	(_webuiToastHost() || document.body).appendChild(toast);
	const shown = isError ? 6000 : 3000;
	setTimeout(() => { toast.classList.add('leaving'); }, shown - 500);
	setTimeout(() => { toast.remove(); }, shown);
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
				html += link(data.links.homepage, webuiLH('about.homepage', 'Homepage')) + link(data.links.docs, webuiLH('about.docs', 'Documentation')) +
					link(data.links.forum, webuiLH('about.forum', 'Forum'));
				linksEl.innerHTML = html || '<span class="webui-about-label">' + webuiLH('about.no_links', 'No links available') + '</span>';
			}

			// Update copyright from API
			if (data.copyright && data.copyright.text)
				setEl('about-copyright', data.copyright.text);
		})
		.catch(() => {
			// nothing to offer then, instead of a loading text for good
			const linksEl = document.getElementById('about-links');
			if (linksEl)
				linksEl.innerHTML = '<span class="webui-about-label">' + webuiLH('about.no_links', 'No links available') + '</span>';
		});

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
	if (modal._webuiRelease)
		modal._webuiRelease();
	modal._webuiRelease = webuiDialogA11y(modal.querySelector('.webui-modal'), {close: function() { webuiCloseAbout(); }});
}

function webuiCloseAbout(event) {
	if (event && event.target !== event.currentTarget) return;
	const modal = document.getElementById('webui-about-modal');
	if (modal) {
		modal.classList.remove('open');
		if (modal._webuiRelease) {
			modal._webuiRelease();
			modal._webuiRelease = null;
		}
	}
	document.body.style.overflow = '';
}

// ═══ Extension-Loader (WORK-146 Spec §4) ═══
// Konvention: <webroot>/extensions/<name>/manifest.json mit
//   {"name","version","slot":"nav|tools|dashboard|page","area"?,"entry","description"?}
// "tools" puts the entry among the pages of the area "area" names (System
// when it names none or one the interface does not have), "nav" makes it an
// area of its own. Schaltung: webui.conf ext_<name>_enabled=1 or 0, else the
// manifest's "default" ("on"); ext_<name>_slot and ext_<name>_area override
// the manifest's place. Which are installed comes from GET /api/v1/webui/extensions,
// the switches from GET /api/v1/webui/settings, both readable without a
// sign-in from the home network; a page that may read neither shows none.
var _webuiExtensions = null;		// the list, once found
var _webuiExtensionsAsked = null;	// the finding, while and after it runs
var _webuiExtensionsUnread = null;	// why the list could not be read, else null

// The interface's own pages a start page can be: page, text key, English;
// named as the navigation names them, its categories of settings.html aside.
var WEBUI_START_PAGES = WEBUI_AREAS.reduce(function(out, a) {
	a.pages.forEach(function(p) {
		if (p.href.indexOf('#') < 0)
			out.push([p.href.substring(1), p.key, p.en]);
	});
	return out;
}, []);

/* Start pages stored under another name: what the classic settings page and
   the shipped yWeb.conf write ("none" there), and pages of the interface that
   became extensions, each standing for the page it is today. index.js keeps
   the same list, as the start page loads nothing but its own script. */
var WEBUI_MOVED_PAGES = {
	none: 'dashboard.html',
	control: 'dashboard.html',
	bouquets: 'bouquets.html',
	remote: 'extensions/screenshot/index.html',
	screenshot: 'extensions/screenshot/index.html',
	'files.html': 'extensions/files/index.html',
	'sysinfo.html': 'extensions/sysinfo/index.html',
	'screenshot.html': 'extensions/screenshot/index.html',
	'bouquet-editor.html': 'extensions/bouquet-editor/index.html'
};

/* A stored start page as the page it stands for, without a leading slash. */
function webuiStartPageOf(stored) {
	var page = String(stored || '').replace(/^\//, '');
	return Object.prototype.hasOwnProperty.call(WEBUI_MOVED_PAGES, page) ?
		WEBUI_MOVED_PAGES[page] : page;
}

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

/* Whether an extension is switched on: what the web interface's settings say
   for it, 1 or 0, and where they say nothing, what its manifest asks for. An
   extension that takes the place of a page of the interface asks for
   "default": "on", so an update does not take that page away; switching it off
   writes 0, which then stays. index.js holds a start page to the same rule. */
function webuiExtSwitchedOn(m, cfg) {
	var v = cfg['ext_' + m.dir + '_enabled'];
	if (v === '1') return true;
	if (v === '0') return false;
	return m['default'] === 'on';
}

// Asked anew by the next webuiDiscoverExtensions (an extension was installed,
// taken out or switched).
function webuiForgetExtensions() {
	_webuiExtensions = null;
	_webuiExtensionsAsked = null;
	_webuiExtensionsUnread = null;
}

async function _webuiFindExtensions() {
	var found = [];
	_webuiExtensionsUnread = null;
	try {
		// read here and not by _webuiV1Read: a refusal or a failure of the
		// box is not an empty list, and says why (a sign-in, for one)
		var lr = await fetch('/api/v1/webui/extensions', {credentials: 'same-origin', cache: 'no-store'});
		if (!lr.ok)
			throw typeof _webuiV1Error === 'function' ? await _webuiV1Error(lr, 'GET', '/webui/extensions') :
				Object.assign(new Error('HTTP ' + lr.status), {status: lr.status});
		var list = await lr.json();
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
				m.enabled = webuiExtSwitchedOn(m, yCfg);
				// the place its manifest gives, and the one the manager may
				// have set over it: the area of the navigation its page joins
				// (webuiRenderNav)
				m.ownSlot = m.slot || 'tools';
				m.ownArea = m.area || '';
				m.slot = yCfg[ckBase + '_slot'] || m.ownSlot;
				m.area = yCfg[ckBase + '_area'] || m.ownArea;
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
	} catch (e) {
		// no list is not an empty one: the extension manager says which
		_webuiExtensionsUnread = e;
	}
	return found;
}

var _webuiExtInjected = false;

// The icon an extension's link carries where its manifest names none.
var WEBUI_EXT_STAR = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M12 2l3 6 6 1-4.5 4.4 1 6.6-5.5-3-5.5 3 1-6.6L3 9l6-1z"/></svg>';

/* An extension's own icon, the monochrome SVG its manifest names as "icon"
   in its folder, drawn in the colour of the text around it: a mask, so
   nothing of the file becomes markup. null for none, or for a name no path
   inside the folder can carry. */
function webuiExtIcon(m) {
	var icon = String(m.icon || '');
	if (!/^[A-Za-z0-9_-][A-Za-z0-9._-]*(\/[A-Za-z0-9_-][A-Za-z0-9._-]*)*\.svg$/.test(icon)) return null;
	var span = document.createElement('span');
	span.className = 'webui-ext-icon';
	span.setAttribute('aria-hidden', 'true');
	var url = '/extensions/' + encodeURIComponent(m.dir) + '/' + icon.split('/').map(encodeURIComponent).join('/');
	span.style.setProperty('--webui-ext-icon', 'url("' + url + '")');
	return span;
}

async function webuiInjectExtensions(pageName) {
	// Nur einmal pro Seitenaufruf injizieren, egal ueber welchen Init-Pfad
	if (_webuiExtInjected) return;
	_webuiExtInjected = true;
	var exts = await webuiDiscoverExtensions();

	/* A link of the interface's own into the page of an extension, marked
	   data-ext-link="<folder>" and hidden in the markup, shows only while that
	   extension is installed and switched on, and leads to the page its
	   manifest names, with data-ext-hash after it where one is given. */
	document.querySelectorAll('[data-ext-link]').forEach(function(el) {
		var m = exts.find(function(x) { return x.dir === el.getAttribute('data-ext-link'); });
		if (!m || !m.enabled || !m.url) return;
		if (el.tagName === 'A')
			el.setAttribute('href', m.url + (el.getAttribute('data-ext-hash') || ''));
		el.hidden = false;
	});

	/* An element of the interface that opens the page of an extension when it
	   is clicked, marked data-ext-open="<folder>" (with data-ext-hash where
	   one is given), does so only while that extension is installed and
	   switched on, and is then a link to the keyboard as well. Otherwise it is
	   only what it shows, and its title, which offers the page, goes. */
	document.querySelectorAll('[data-ext-open]').forEach(function(el) {
		var m = exts.find(function(x) { return x.dir === el.getAttribute('data-ext-open'); });
		if (!m || !m.enabled || !m.url) {
			el.removeAttribute('data-l-title');
			el.removeAttribute('title');
			return;
		}
		var to = m.url + (el.getAttribute('data-ext-hash') || '');
		el.classList.add('webui-ext-open');
		el.setAttribute('role', 'link');
		el.tabIndex = 0;
		el.addEventListener('click', function() { location.href = to; });
		el.addEventListener('keydown', function(ev) {
			if (ev.key === 'Enter') {
				ev.preventDefault();
				location.href = to;
			}
		});
	});

	webuiNavKeepPlaces(exts, pageName);

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
		if (m.slot === 'dashboard') {
			var dash = document.querySelector('.webui-dashboard');
			if (dash) {
				// a tile like the overview's own, its name and its link as text
				var card = document.createElement('section');
				card.className = 'webui-card';
				card.innerHTML = '<header class="webui-card-header"><h2 class="webui-card-title">' + escapeHtml(m.name) + '</h2></header>' +
					'<div class="webui-card-body"><a class="webui-btn" href="' + escapeHtml(m.url) + '">' +
					escapeHtml(m.description || webuiL('ext.open', 'Open extension')) + '</a></div>';
				dash.appendChild(card);
			}
		}
		// slot 'page': keine Injektion, erreichbar ueber die
		// Erweiterungs-Verwaltung oder direkten Link
	});
}

/* Where each switched-on extension with a page goes in the navigation: the
   area the extension manager or its manifest names (area), System where
   neither names one of the areas an extension may join, an area of its own
   for slot nav; none for a card on the overview (dashboard) or a page
   reached by its address alone (page). Its icon goes with it, for the side
   bar and for an area of its own. */
/* The places of the extensions' pages in the navigation, kept for the next
   page of this tab so it draws them at once; drawn anew only where they
   changed. Also called by the extension manager after it saved. */
function webuiNavKeepPlaces(exts, pageName) {
	var places = _webuiNavPlaces(exts);
	var keptJson = JSON.stringify(_webuiNavExtsKept());
	_webuiNavExts = places;
	try { sessionStorage.setItem('webui-nav-exts', JSON.stringify(places)); } catch (e) { /* drawn late then */ }
	if (JSON.stringify(places) !== keptJson)
		webuiRenderNav(_webuiNavShown || pageName);
}

function _webuiNavPlaces(exts) {
	var out = [];
	(exts || []).forEach(function(m) {
		if (!m.enabled || !m.url || m.slot === 'dashboard' || m.slot === 'page')
			return;
		out.push({dir: m.dir, name: m.name, url: m.url, own: m.slot === 'nav',
			area: WEBUI_EXT_AREAS.indexOf(m.area) >= 0 ? m.area : 'system',
			icon: (webuiExtIcon(m) || {}).outerHTML || WEBUI_EXT_STAR});
	});
	return out;
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
	_webuiToastHost();
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
		_webuiInsertSide();

		// Set theme
		webuiApplyTheme(webuiGetTheme());

		// the navigation, marked for this page
		webuiRenderNav(headerWrapper ? headerWrapper.dataset.page : null);

		// Start clock
		webuiUpdateClock();
		setInterval(webuiUpdateClock, 10000);

		// Load hostname
		webuiLoadHostname();

		// Calculate and set spacer height
		webuiUpdateHeaderSpacer();

		// Update on window resize
		window.addEventListener('resize', webuiUpdateHeaderSpacer);

		// Always insert about modal if not present
		if (!document.getElementById('webui-about-modal')) {
			document.body.insertAdjacentHTML('beforeend', webuiAboutHTML());
		}

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
   Grid balancer (shared)
   ============================================ */

/**
 * Shares the cards of a grid out over as few rows as the grid's width
 * allows, evenly: five cards where four fit a row stand three and two, the
 * two sharing their row's width, not four and one stretched across. Each
 * card is given its span as --dash-span; the grid has opts.tracks columns
 * (60 shares out evenly to rows of one to six) and its cards take
 * `grid-column: span var(--dash-span, ...)`. Below the breakpoint the spans
 * are taken away again and the page's media queries decide.
 * @param {string} selector - CSS selector of the grid
 * @param {Object} [opts]
 * @param {number} [opts.tracks=12] - the grid's columns
 * @param {number} [opts.cardMin=300] - the narrowest a card should be, in px
 * @param {number} [opts.maxPer=6] - at most this many cards to a row
 * @param {number} [opts.breakpoint=1100] - at or below this window width, no spans
 * @param {string} [opts.skip] - cards that keep their own place (a band
 *        across the page), matched as a selector
 * @param {string} [opts.wideClass] - a class for cards half the width or more
 * @param {boolean} [opts.watch] - again whenever cards come or go (extensions
 *        add theirs later) and when the window changes its size
 */
function webuiBalanceGrid(selector, opts) {
	opts = opts || {};
	var grid = document.querySelector(selector);
	if (!grid) return;
	if (opts.watch && !grid._webuiBalanced) {
		grid._webuiBalanced = true;
		var again = null;
		// cards that come or go, and cards shown or hidden (hidden on each card)
		var seen = new WeakSet();
		var mo = new MutationObserver(function() { soon(); });
		var watchCards = function() {
			Array.prototype.forEach.call(grid.children, function(c) {
				if (seen.has(c)) return;
				seen.add(c);
				mo.observe(c, {attributes: true, attributeFilter: ['hidden']});
			});
		};
		var soon = function() {
			clearTimeout(again);
			again = setTimeout(function() {
				watchCards();
				webuiBalanceGrid(selector, Object.assign({}, opts, {watch: false}));
			}, 100);
		};
		mo.observe(grid, {childList: true});
		watchCards();
		window.addEventListener('resize', soon);
	}
	var cards = Array.prototype.filter.call(grid.children, function(c) {
		return c.offsetParent !== null && !(opts.skip && c.matches(opts.skip));
	});
	if (window.innerWidth <= (opts.breakpoint || 1100)) {
		cards.forEach(function(c) {
			if (opts.wideClass) c.classList.remove(opts.wideClass);
			c.style.removeProperty('--dash-span');
		});
		return;
	}
	if (!cards.length) return;
	var tracks = opts.tracks || 12;
	var most = Math.max(1, Math.min(opts.maxPer || 6, Math.floor(grid.clientWidth / (opts.cardMin || 300))));
	// as few rows as the widest row allows, the cards shared out evenly over them
	var rows = Math.ceil(cards.length / most);
	var per = Math.ceil(cards.length / rows);
	var last = cards.length - per * (rows - 1);
	cards.forEach(function(c, i) {
		var s = Math.floor(tracks / (i >= per * (rows - 1) ? last : per));
		c.style.setProperty('--dash-span', s);
		if (opts.wideClass) c.classList.toggle(opts.wideClass, s * 2 >= tracks);
	});
}

/* ============================================
   Splitter (shared)
   ============================================ */

/**
 * A handle between two columns of a grid that moves the line between
 * them: dragged with the pointer, or with the arrow keys once it has the
 * focus (Shift for longer steps); Home or a double click put the first
 * column back to its width at first. The width is a custom property of
 * the grid (opts.prop), which the grid's first column takes, and it is
 * kept for this browser under opts.key. The handle is a separator that
 * says its place (aria-valuenow, -min, -max, in px).
 * @param {Element} handle - the element between the two columns
 * @param {Object} opts
 * @param {Element} opts.grid - the grid
 * @param {string} opts.prop - the custom property, e.g. '--fm-side'
 * @param {string} [opts.key] - localStorage key for the width kept
 * @param {number} [opts.start=240] - the width at first, in px
 * @param {number} [opts.min=160] - the narrowest, in px
 * @param {number} [opts.maxShare=0.6] - the widest, as a share of the grid
 */
function webuiSplitter(handle, opts) {
	var grid = opts.grid;
	if (!handle || !grid) return;
	var start = opts.start || 240;
	var min = opts.min || 160;
	var max = function() {
		return Math.max(min, Math.floor(grid.clientWidth * (opts.maxShare || 0.6)));
	};
	var set = function(w, keep) {
		w = Math.round(Math.max(min, Math.min(max(), w)));
		grid.style.setProperty(opts.prop, w + 'px');
		handle.setAttribute('aria-valuenow', String(w));
		handle.setAttribute('aria-valuemin', String(min));
		handle.setAttribute('aria-valuemax', String(max()));
		if (keep && opts.key) {
			try { localStorage.setItem(opts.key, String(w)); } catch (e) { /* not kept then */ }
		}
		return w;
	};
	var kept = 0;
	try { kept = parseInt(localStorage.getItem(opts.key || '') || '', 10) || 0; } catch (e) { /* none kept */ }
	var width = set(kept > 0 ? kept : start, false);
	handle.setAttribute('role', 'separator');
	handle.setAttribute('aria-orientation', 'vertical');
	handle.tabIndex = 0;
	handle.addEventListener('pointerdown', function(e) {
		if (e.button !== 0) return;
		e.preventDefault();
		handle.setPointerCapture(e.pointerId);
		var x0 = e.clientX, w0 = width;
		handle.classList.add('dragging');
		var move = function(ev) { width = set(w0 + ev.clientX - x0, false); };
		var up = function() {
			handle.removeEventListener('pointermove', move);
			handle.removeEventListener('pointerup', up);
			handle.removeEventListener('pointercancel', up);
			handle.classList.remove('dragging');
			width = set(width, true);
		};
		handle.addEventListener('pointermove', move);
		handle.addEventListener('pointerup', up);
		handle.addEventListener('pointercancel', up);
	});
	handle.addEventListener('keydown', function(e) {
		var step = e.shiftKey ? 64 : 16;
		if (e.key === 'ArrowLeft') width = set(width - step, true);
		else if (e.key === 'ArrowRight') width = set(width + step, true);
		else if (e.key === 'Home') width = set(start, true);
		else return;
		e.preventDefault();
	});
	handle.addEventListener('dblclick', function() { width = set(start, true); });
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

// A button per card, named by the card's title, the shown one current
function buildCardIndicator() {
	if (!_webuiAcc) return;
	var el = document.getElementById(_webuiAcc.indicatorId);
	if (!el) return;
	el.replaceChildren.apply(el, _webuiAcc.cardOrder.map(function(id) {
		var card = document.getElementById(id);
		var title = card && card.querySelector('.webui-card-title');
		var b = document.createElement('button');
		b.type = 'button';
		b.className = 'webui-card-dot';
		b.dataset.card = id;
		b.setAttribute('aria-label', title ? title.textContent.trim() : id);
		b.addEventListener('click', function() { selectCard(id); });
		return b;
	}));
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
		if (dotCard === activeId) dot.setAttribute('aria-current', 'true');
		else dot.removeAttribute('aria-current');
	});
	_webuiAccHeads();
}

/* On a phone a card's head opens and closes it. Its title then holds a
   button with the card's state, so a keyboard opens it as a tap does (the
   click reaches the head's own handler) and the title stays a heading;
   elsewhere the title's words stand in it again as they were. */
function _webuiAccHeads() {
	if (!_webuiAcc) return;
	var phone = window.innerWidth <= _webuiAcc.breakpoint;
	_webuiAcc.cardOrder.forEach(function(id) {
		var card = document.getElementById(id);
		var head = card && card.querySelector(':scope > .webui-card-header[onclick]');
		var title = head && head.querySelector('.webui-card-title');
		if (!title) return;
		var toggle = title.querySelector(':scope > .webui-card-toggle');
		if (!phone) {
			if (toggle) toggle.replaceWith.apply(toggle, Array.prototype.slice.call(toggle.childNodes));
			return;
		}
		if (!toggle) {
			toggle = document.createElement('button');
			toggle.type = 'button';
			toggle.className = 'webui-card-toggle';
			toggle.append.apply(toggle, Array.prototype.slice.call(title.childNodes));
			title.appendChild(toggle);
		}
		toggle.setAttribute('aria-expanded', (_webuiAcc.mode === 'collapsed' ? !card.classList.contains('collapsed') : card.classList.contains('expanded')) ? 'true' : 'false');
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
