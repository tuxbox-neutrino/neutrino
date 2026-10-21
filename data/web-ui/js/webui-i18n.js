// Texts of the pages, read in the browser
// ---------------------------------------
// languages/<code>.json of the language the box shows its pages in, laid
// over languages/en.json, so a text a translation lacks shows in English.
// A page writes its English texts into the markup and marks them:
//   data-l="key"          the element's text, after data-l-prefix where one
//                         is given (the page's <title>)
//   data-l-title="key"    an attribute: title, placeholder, aria-label
// and asks webuiL(key, english) in its scripts, webuiLH for markup it
// builds. The texts are set as text, never as markup. They start loading as
// soon as this script does (window.webuiTexts); whatever needs them runs
// through webuiAfterTexts.

// The keys this page has, once webuiI18nLoad has read them.
window.WEBUI_L = window.WEBUI_L || {};

// webui-header.js defines webuiL for the pages that load it; a page without
// the header (sign-in, new password) gets this one.
window.webuiL = window.webuiL || function(key, english) {
	var v = window.WEBUI_L[key];
	return (typeof v === 'string' && v !== '') ? v : (english || key);
};

// The same, for a text a script puts into markup it builds: escaped for it.
window.webuiLH = window.webuiLH || function(key, english) {
	return String(webuiL(key, english)).replace(/[&<>"']/g, function(c) {
		return {'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c];
	});
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
		var prefix = el.getAttribute('data-l-prefix') || '';
		var shown = el.textContent;
		if (prefix && shown.indexOf(prefix) === 0)
			shown = shown.slice(prefix.length);
		el.textContent = prefix + webuiL(el.getAttribute('data-l'), shown);
	});
	WEBUI_I18N_ATTRS.forEach(function(a) {
		root.querySelectorAll('[data-l-' + a + ']').forEach(function(el) {
			el.setAttribute(a, webuiL(el.getAttribute('data-l-' + a), el.getAttribute(a) || ''));
		});
	});
}

function _webuiDomReady() {
	if (document.readyState !== 'loading')
		return Promise.resolve();
	return new Promise(function(done) {
		document.addEventListener('DOMContentLoaded', function() { done(); }, {once: true});
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
	await _webuiDomReady();
	webuiI18nApply();
	clearTimeout(shown);
	html.classList.remove('webui-i18n-pending');
	return now;
}

// The page's texts, on their way from the moment this script loads. Another
// script may chain on it (webui-header.js fills its labels that way), so it
// is read when asked for, not kept.
window.webuiTexts = webuiI18nStart();

/* A refusal by the kind its status says, in the page's language. Here, and
   not in webui-header.js, for the pages that load no header (sign-in, new
   password, wizard); the header's webuiErrText says it with the box's own
   codes first and falls back to this. */
var WEBUI_ERR_KINDS = {
	input: 'The box did not accept the input',
	refused: 'Not allowed',
	not_found: 'Not found on the box',
	busy: 'Not possible right now',
	too_large: 'Too large for the box',
	too_many: 'Too many attempts, wait a moment',
	not_offered: 'The box does not offer this',
	unavailable: 'Not available on the box right now',
	failed: 'The box could not do it'
};

function webuiKindText(s) {
	var kind = (s === 400 || s === 422) ? 'input' : (s === 401 || s === 403) ? 'refused'
		: s === 404 ? 'not_found' : s === 409 ? 'busy' : s === 413 ? 'too_large'
		: s === 429 ? 'too_many' : s === 501 ? 'not_offered' : s >= 502 ? 'unavailable' : 'failed';
	return webuiL('err.kind_' + kind, WEBUI_ERR_KINDS[kind]);
}

// Runs fn once the document is parsed and the texts are in.
function webuiAfterTexts(fn) {
	return Promise.all([_webuiDomReady(), window.webuiTexts]).then(function() { return fn(); });
}
