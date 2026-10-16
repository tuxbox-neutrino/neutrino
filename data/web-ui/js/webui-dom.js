// webui-dom.js - the small DOM helpers the modern pages share (load before
// the page's own script; uses the language texts of webui-header.js when it
// is loaded, else English).
//
//   webuiEl('div', {className: 'row', title: name}, name, ' ', child)
//       -> an element. Strings and numbers among the children become text
//          nodes, never markup: a title out of the EPG stays a title, even
//          when it holds "<img onerror=...>".
//   webuiMarkup(ICON_SVG)
//       -> a fragment parsed from markup. Only for markup written into the
//          page source (icons), never for anything the box or a broadcast
//          delivered.
//   webuiSync(list, items, keyOf, render, update)
//       -> brings the children of list into the order and number of items.
//          A row whose key is already there is kept (update(node, item) if
//          given, else it is replaced only when render(item) differs), so an
//          unchanged row neither flickers nor loses focus or selection.
//          A kept row keeps its handlers too: when they need more of the
//          item than the row shows, pass update, or keep what they need
//          in data-* of the row. Keys are meant to be unique; a key that
//          comes again in one call gets a running number appended.
//
// Properties webuiEl understands: className, id, title, hidden, type, value,
// disabled, checked, htmlFor, tabIndex; data: {key: value} for data-*,
// attrs: {name: value} for other attributes, style: {prop: value}, and
// on: {event: function}. Event handlers only as functions: an attribute
// whose name starts with "on" is refused, and so are srcdoc and a style
// attribute (styles go through style:). An attribute that takes an address
// (href, src, srcset, action, formaction, poster, ...) is set only when the
// address is relative or uses http, https, mailto or tel, and a style value
// that loads something (url(...)) is left out. So text from outside cannot
// become code through them. The tag name itself comes from the page code.

var _webuiElProps = ['className', 'id', 'title', 'hidden', 'type', 'value',
	'disabled', 'checked', 'htmlFor', 'tabIndex'];

// attributes that take an address; their value may only be a relative address
// or one with a scheme that cannot run anything
var _webuiUrlAttrs = ['href', 'src', 'action', 'formaction', 'xlink:href', 'poster',
	'background', 'cite', 'data', 'codebase', 'ping', 'longdesc', 'lowsrc', 'dynsrc',
	'manifest', 'archive', 'usemap', 'profile'];
var _webuiSafeSchemes = ['http', 'https', 'mailto', 'tel'];

function _webuiSafeUrl(value) {
	// what the browser would see after stripping blanks and control characters
	var v = String(value).replace(/[\u0000-\u0020\u007f]/g, '');
	var m = /^([a-z][a-z0-9+.\-]*):/i.exec(v);
	return !m || _webuiSafeSchemes.indexOf(m[1].toLowerCase()) >= 0;
}

function _webuiSafeAttr(name, value) {
	var n = String(name).trim().toLowerCase();
	if (n.indexOf('on') === 0 || n === 'srcdoc' || n === 'style')
		return false;
	if (n === 'srcset' || n === 'imagesrcset')
		return String(value).split(',').every(function(c) {
			return _webuiSafeUrl(c.trim().split(/\s+/)[0]);
		});
	if (_webuiUrlAttrs.indexOf(n) >= 0)
		return _webuiSafeUrl(value);
	return true;
}

function _webuiAppend(parent, child) {
	if (child === null || child === undefined || child === false)
		return;
	if (Array.isArray(child)) {
		for (var i = 0; i < child.length; i++)
			_webuiAppend(parent, child[i]);
		return;
	}
	if (child instanceof Node)
		parent.appendChild(child);
	else
		parent.appendChild(document.createTextNode(String(child)));
}

function webuiEl(tag, props) {
	var node = document.createElement(tag);
	props = props || {};
	for (var i = 0; i < _webuiElProps.length; i++) {
		var p = _webuiElProps[i];
		if (props[p] !== undefined && props[p] !== null)
			node[p] = props[p];
	}
	var k;
	if (props.data)
		for (k in props.data)
			if (Object.prototype.hasOwnProperty.call(props.data, k))
				node.dataset[k] = String(props.data[k]);
	if (props.attrs)
		for (k in props.attrs)
			if (Object.prototype.hasOwnProperty.call(props.attrs, k) && _webuiSafeAttr(k, props.attrs[k]))
				node.setAttribute(k, String(props.attrs[k]));
	if (props.style)
		for (k in props.style)
			if (Object.prototype.hasOwnProperty.call(props.style, k) &&
			    !/url\s*\(|expression\s*\(|javascript:/i.test(String(props.style[k])))
				node.style.setProperty(k, String(props.style[k]));
	if (props.on)
		for (k in props.on)
			if (Object.prototype.hasOwnProperty.call(props.on, k) && typeof props.on[k] === 'function')
				node.addEventListener(k, props.on[k]);
	for (var c = 2; c < arguments.length; c++)
		_webuiAppend(node, arguments[c]);
	return node;
}

function webuiMarkup(markup) {
	var t = document.createElement('template');
	t.innerHTML = markup;
	return t.content.cloneNode(true);
}

function webuiSync(list, items, keyOf, render, update) {
	// maps without a prototype, so a key such as __proto__ is just a key
	var old = Object.create(null);
	for (var n = list.firstElementChild; n; n = n.nextElementSibling)
		if (n.dataset && n.dataset.webuiKey !== undefined)
			old[n.dataset.webuiKey] = n;

	// Every row that belongs goes in front of this node, in order; whatever
	// is still at or after it once all rows are placed does not belong.
	var cursor = list.firstChild;
	var seen = Object.create(null);
	for (var i = 0; i < items.length; i++) {
		var k = String(keyOf(items[i], i));
		if (Object.prototype.hasOwnProperty.call(seen, k))
			k += '\u0000' + (++seen[k]);
		else
			seen[k] = 0;
		var node = Object.prototype.hasOwnProperty.call(old, k) ? old[k] : null;
		if (node) {
			delete old[k];
			if (update) {
				update(node, items[i]);
			} else {
				var fresh = render(items[i]);
				fresh.dataset.webuiKey = k;
				if (!fresh.isEqualNode(node)) {
					if (node === cursor)
						cursor = fresh;
					list.replaceChild(fresh, node);
					node = fresh;
				}
			}
		} else {
			node = render(items[i]);
			node.dataset.webuiKey = k;
		}
		if (node === cursor)
			cursor = cursor.nextSibling;
		else
			list.insertBefore(node, cursor);
	}
	while (cursor) {
		var next = cursor.nextSibling;
		list.removeChild(cursor);
		cursor = next;
	}
}

// webuiSheet({title, body, actions, onClose})
//   -> {dialog, body, close(result)}: a modal sheet on top of the page.
//   title is text; body is a node or an array of nodes (webuiEl); actions are
//   buttons in the footer: {label, value, primary, danger, keepOpen,
//   onClick(sheet, event)}. A button closes the sheet with its value, unless
//   keepOpen is set or onClick returns false. onClose(result) is called once,
//   with the value of the button, or 'cancel' for Escape, the close button or
//   a click beside the sheet.
//   The sheet is a native <dialog> opened modally, so the rest of the page
//   takes no clicks and no focus; Tab and Shift+Tab also wrap inside it.
//   A browser without modal dialogs gets it opened in place instead.
//   Focus goes to the first [autofocus] element, else the first field or
//   button of the body, else the first action, and back to where it was
//   when the sheet closes. The dialog leaves the document when it closes.

// a text of the language files when webui-header.js is loaded, else the English one
function _webuiText(key, en) {
	return typeof webuiL === 'function' ? webuiL(key, en) : en;
}

var _webuiFocusable = 'a[href], button:not([disabled]), input:not([disabled]):not([type="hidden"]), ' +
	'select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])';

function _webuiFocusables(root) {
	return Array.prototype.filter.call(root.querySelectorAll(_webuiFocusable), function(n) {
		return !n.hidden && n.getClientRects().length > 0;
	});
}

function webuiSheet(opts) {
	opts = opts || {};
	var before = document.activeElement;
	var closed = false;
	var sheet = {dialog: null, body: null, close: null};

	var closeBtn = webuiEl('button', {type: 'button', attrs: {'aria-label': _webuiText('msg.close', 'Close'), rel: 'prev'}});
	var heading = webuiEl('h3', {}, opts.title || '');
	var body = webuiEl('div', {className: 'webui-sheet-body'}, opts.body || null);
	var footer = webuiEl('footer', {});
	(opts.actions || []).forEach(function(a) {
		var cls = (a.primary ? '' : 'secondary') + (a.danger ? ' danger' : '');
		footer.appendChild(webuiEl('button', {type: 'button', className: cls.trim(), on: {click: function(ev) {
			var keep = a.keepOpen;
			if (a.onClick && a.onClick(sheet, ev) === false)
				keep = true;
			if (!keep)
				sheet.close(a.value !== undefined ? a.value : a.label);
		}}}, a.label));
	});
	var article = webuiEl('article', {}, webuiEl('header', {}, closeBtn, heading), body,
		footer.childNodes.length ? footer : null);
	var dialog = webuiEl('dialog', {className: 'webui-sheet', attrs: {'aria-label': opts.title || ''}}, article);

	sheet.dialog = dialog;
	sheet.body = body;
	sheet.close = function(result) {
		if (closed)
			return;
		closed = true;
		if (dialog.open)
			dialog.close();
		dialog.remove();
		if (before && before.focus && document.contains(before))
			before.focus();
		if (opts.onClose)
			opts.onClose(result);
	};

	closeBtn.addEventListener('click', function() { sheet.close('cancel'); });
	// Escape: closed through the same path, so the dialog is removed too
	dialog.addEventListener('cancel', function(ev) { ev.preventDefault(); sheet.close('cancel'); });
	// a click on the dialog itself and not on its content is a click beside it
	dialog.addEventListener('click', function(ev) { if (ev.target === dialog) sheet.close('cancel'); });
	dialog.addEventListener('keydown', function(ev) {
		if (ev.key === 'Escape') {
			ev.preventDefault();
			sheet.close('cancel');
			return;
		}
		if (ev.key !== 'Tab')
			return;
		var f = _webuiFocusables(dialog);
		if (!f.length)
			return;
		var first = f[0], last = f[f.length - 1];
		if (ev.shiftKey && (document.activeElement === first || !dialog.contains(document.activeElement))) {
			ev.preventDefault();
			last.focus();
		} else if (!ev.shiftKey && (document.activeElement === last || !dialog.contains(document.activeElement))) {
			ev.preventDefault();
			first.focus();
		}
	});

	document.body.appendChild(dialog);
	// without a modal dialog (an old browser) it opens in place; Tab still
	// wraps, Escape still closes, only the page behind stays clickable
	if (typeof dialog.showModal === 'function') {
		try {
			dialog.showModal();
		} catch (e) {
			dialog.setAttribute('open', '');
		}
	} else {
		dialog.setAttribute('open', '');
	}
	var target = dialog.querySelector('[autofocus]') || _webuiFocusables(body)[0] ||
		_webuiFocusables(footer)[0] || closeBtn;
	target.focus();
	return sheet;
}

// webuiDock(key, node) -> the dock: a place in the page header for what the
//   page has to say all the time (unsaved changes, a recording that runs, a
//   lost connection). node is shown under key and replaces what was there
//   under that key; null takes it away. The dock is a polite live region, so
//   a screen reader reads out what is put into it. Without the page header
//   the dock sits at the top of the page.

function webuiDock(key, node) {
	var dock = document.getElementById('webui-dock');
	if (!dock) {
		dock = webuiEl('div', {id: 'webui-dock', className: 'webui-dock',
			attrs: {role: 'status', 'aria-live': 'polite'}});
		var right = document.querySelector('.webui-header-right');
		if (right)
			right.parentNode.insertBefore(dock, right);
		else
			document.body.insertBefore(dock, document.body.firstChild);
	}
	if (key === undefined)
		return dock;
	var k = String(key);
	var old = null;
	for (var n = dock.firstElementChild; n; n = n.nextElementSibling)
		if (n.dataset.webuiDockKey === k) {
			old = n;
			break;
		}
	if (node) {
		var item = webuiEl('span', {className: 'webui-dock-item', data: {webuiDockKey: k}}, node);
		if (old)
			dock.replaceChild(item, old);
		else
			dock.appendChild(item);
	} else if (old) {
		dock.removeChild(old);
	}
	return dock;
}

// webuiRowActions([{label, icon, onClick, danger, disabled}]) -> the buttons
//   of one row. Each is an icon button whose label is its accessible name and
//   tooltip; without an icon the label is its text. icon is markup from the
//   page source, as for webuiMarkup. A click calls onClick(event) and stops
//   there, so a row that opens on a click does not open as well.

function webuiRowActions(actions) {
	var group = webuiEl('div', {className: 'webui-row-actions', attrs: {role: 'group'}});
	(actions || []).forEach(function(a) {
		var b = webuiEl('button', {type: 'button', title: a.label, disabled: !!a.disabled,
			className: a.danger ? 'danger' : '', attrs: {'aria-label': a.label},
			on: {click: function(ev) {
				ev.stopPropagation();
				if (a.onClick)
					a.onClick(ev);
			}}}, a.icon ? webuiMarkup(a.icon) : a.label);
		group.appendChild(b);
	});
	return group;
}
