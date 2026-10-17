// Texts of the pages, read in the browser
// ---------------------------------------
// languages/<code>.json of the language the box shows its pages in, laid
// over languages/en.json, so a text a translation lacks shows in English.
// A page writes its English texts into the markup and marks them:
//   data-l="key"          the element's text
//   data-l-title="key"    an attribute: title, placeholder, aria-label
// and asks webuiL(key, english) in its scripts. The texts are set as text,
// never as markup.

// The keys this page has, once webuiI18nLoad has read them.
window.WEBUI_L = window.WEBUI_L || {};

// webui-header.js defines webuiL for the pages that load it; a page without
// the header (sign-in, new password) gets this one.
window.webuiL = window.webuiL || function(key, english) {
	var v = window.WEBUI_L[key];
	return (typeof v === 'string' && v !== '') ? v : (english || key);
};

async function _webuiI18nRead(code) {
	try {
		var r = await fetch('/languages/' + encodeURIComponent(code) + '.json',
			{credentials: 'same-origin', cache: 'no-cache'});
		return r.ok ? await r.json() : null;
	} catch (e) {
		return null;
	}
}

// Reads the texts of a language and answers the code the page is now in:
// the one asked for where the box has its file, else English.
async function webuiI18nLoad(code) {
	var en = await _webuiI18nRead('en');
	var own = (code && code !== 'en') ? await _webuiI18nRead(code) : null;
	window.WEBUI_L = Object.assign({}, en || {}, own || {});
	var now = own ? code : 'en';
	document.documentElement.lang = now;
	return now;
}

var WEBUI_I18N_ATTRS = ['title', 'placeholder', 'aria-label'];

// Puts the texts into what root holds (the whole page without one).
function webuiI18nApply(root) {
	root = root || document;
	root.querySelectorAll('[data-l]').forEach(function(el) {
		el.textContent = webuiL(el.getAttribute('data-l'), el.textContent);
	});
	WEBUI_I18N_ATTRS.forEach(function(a) {
		root.querySelectorAll('[data-l-' + a + ']').forEach(function(el) {
			el.setAttribute(a, webuiL(el.getAttribute('data-l-' + a), el.getAttribute(a) || ''));
		});
	});
}

// The language a page is shown in: the one the box shows its pages in, as
// GET /api/v1/session says it to anybody, else the browser's where the box
// has a file for it, else English. The page is hidden (class
// webui-i18n-pending on <html>) until its texts are in, for a second at most.
async function webuiI18nStart() {
	var html = document.documentElement;
	var shown = setTimeout(function() { html.classList.remove('webui-i18n-pending'); }, 1000);
	var code = null;
	try {
		var r = await fetch('/api/v1/session', {credentials: 'same-origin', cache: 'no-store'});
		var s = r.ok ? await r.json() : null;
		code = s && s.language;
	} catch (e) {}
	if (!code)
		code = String((navigator.languages && navigator.languages[0]) || navigator.language || 'en')
			.slice(0, 2).toLowerCase();
	var now = await webuiI18nLoad(code);
	webuiI18nApply();
	clearTimeout(shown);
	html.classList.remove('webui-i18n-pending');
	return now;
}
