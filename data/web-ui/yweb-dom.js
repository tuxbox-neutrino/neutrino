// yweb-dom.js - the small DOM helpers the modern pages share (load before
// the page's own script; needs nothing else).
//
//   ywebEl('div', {className: 'row', title: name}, name, ' ', child)
//       -> an element. Strings and numbers among the children become text
//          nodes, never markup: a title out of the EPG stays a title, even
//          when it holds "<img onerror=...>".
//   ywebMarkup(ICON_SVG)
//       -> a fragment parsed from markup. Only for markup written into the
//          page source (icons), never for anything the box or a broadcast
//          delivered.
//   ywebSync(list, items, keyOf, render, update)
//       -> brings the children of list into the order and number of items.
//          A row whose key is already there is kept (update(node, item) if
//          given, else it is replaced only when render(item) differs), so an
//          unchanged row neither flickers nor loses focus or selection.
//          A kept row keeps its handlers too: when they need more of the
//          item than the row shows, pass update, or keep what they need
//          in data-* of the row. Keys are meant to be unique; a key that
//          comes again in one call gets a running number appended.
//
// Properties ywebEl understands: className, id, title, hidden, type, value,
// disabled, checked, htmlFor, tabIndex; data: {key: value} for data-*,
// attrs: {name: value} for other attributes, style: {prop: value}, and
// on: {event: function}. Event handlers only as functions: an attribute
// whose name starts with "on" is refused, and so are srcdoc and a style
// attribute (styles go through style:). An attribute that takes an address
// (href, src, srcset, action, formaction, poster, ...) is set only when the
// address is relative or uses http, https, mailto or tel, and a style value
// that loads something (url(...)) is left out. So text from outside cannot
// become code through them. The tag name itself comes from the page code.

var _ywebElProps = ['className', 'id', 'title', 'hidden', 'type', 'value',
	'disabled', 'checked', 'htmlFor', 'tabIndex'];

// attributes that take an address; their value may only be a relative address
// or one with a scheme that cannot run anything
var _ywebUrlAttrs = ['href', 'src', 'action', 'formaction', 'xlink:href', 'poster',
	'background', 'cite', 'data', 'codebase', 'ping', 'longdesc', 'lowsrc', 'dynsrc',
	'manifest', 'archive', 'usemap', 'profile'];
var _ywebSafeSchemes = ['http', 'https', 'mailto', 'tel'];

function _ywebSafeUrl(value) {
	// what the browser would see after stripping blanks and control characters
	var v = String(value).replace(/[\u0000-\u0020\u007f]/g, '');
	var m = /^([a-z][a-z0-9+.\-]*):/i.exec(v);
	return !m || _ywebSafeSchemes.indexOf(m[1].toLowerCase()) >= 0;
}

function _ywebSafeAttr(name, value) {
	var n = String(name).trim().toLowerCase();
	if (n.indexOf('on') === 0 || n === 'srcdoc' || n === 'style')
		return false;
	if (n === 'srcset' || n === 'imagesrcset')
		return String(value).split(',').every(function(c) {
			return _ywebSafeUrl(c.trim().split(/\s+/)[0]);
		});
	if (_ywebUrlAttrs.indexOf(n) >= 0)
		return _ywebSafeUrl(value);
	return true;
}

function _ywebAppend(parent, child) {
	if (child === null || child === undefined || child === false)
		return;
	if (Array.isArray(child)) {
		for (var i = 0; i < child.length; i++)
			_ywebAppend(parent, child[i]);
		return;
	}
	if (child instanceof Node)
		parent.appendChild(child);
	else
		parent.appendChild(document.createTextNode(String(child)));
}

function ywebEl(tag, props) {
	var node = document.createElement(tag);
	props = props || {};
	for (var i = 0; i < _ywebElProps.length; i++) {
		var p = _ywebElProps[i];
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
			if (Object.prototype.hasOwnProperty.call(props.attrs, k) && _ywebSafeAttr(k, props.attrs[k]))
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
		_ywebAppend(node, arguments[c]);
	return node;
}

function ywebMarkup(markup) {
	var t = document.createElement('template');
	t.innerHTML = markup;
	return t.content.cloneNode(true);
}

function ywebSync(list, items, keyOf, render, update) {
	// maps without a prototype, so a key such as __proto__ is just a key
	var old = Object.create(null);
	for (var n = list.firstElementChild; n; n = n.nextElementSibling)
		if (n.dataset && n.dataset.ywebKey !== undefined)
			old[n.dataset.ywebKey] = n;

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
				fresh.dataset.ywebKey = k;
				if (!fresh.isEqualNode(node)) {
					if (node === cursor)
						cursor = fresh;
					list.replaceChild(fresh, node);
					node = fresh;
				}
			}
		} else {
			node = render(items[i]);
			node.dataset.ywebKey = k;
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

