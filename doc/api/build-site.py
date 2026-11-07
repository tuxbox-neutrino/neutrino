#!/usr/bin/env python3
# build-site.py - builds the published documentation out of doc/
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# usage: build-site.py [--document <openapi.json>] <out-dir>
#
# Copies doc/ into <out-dir> as it is, except doc/api/, and writes the API
# reference into <out-dir>/api/ out of doc/api/openapi.json: an index of every
# group and route, one page per group, one page for the schemas, the document
# itself and the style sheet. Every link the written pages carry is checked
# afterwards, and the run fails when one of them leads nowhere.
#
# The reference is static HTML without scripts or anything fetched from
# elsewhere, so it reads the same from a disk as from the web. The content
# comes from the document; this script adds the layout and the short labels
# for access levels and credentials below.
#
# <out-dir> has to be missing, empty, or a folder this script wrote before
# (it leaves a marker file there); anything else is refused, because the
# folder is cleared before it is written.
#
# Only the Python standard library is used.

import html
import html.parser
import json
import os
import re
import shutil
import sys
import urllib.parse

HERE = os.path.dirname(os.path.realpath(__file__))
DOC_ROOT = os.path.dirname(HERE)
STYLE = os.path.join(DOC_ROOT, 'assets', 'doc.css')
MARKER = '.build-site'

METHODS = ('get', 'put', 'post', 'delete', 'options', 'head', 'patch', 'trace')
LEVEL_TEXT = {
	'public': 'anyone who reaches the box',
	'read': 'a caller on a trusted network, a session or a token of level read or above',
	'write': 'a session or a token of level write or above',
	'system': 'a session or a token of level system',
}
SCHEME_TEXT = {
	'session': 'session cookie',
	'token': 'bearer token',
	'query_token': 'media token in the query',
}


def fail(what):
	sys.stderr.write('build-site.py: ' + what + '\n')
	sys.exit(1)


def esc(text):
	return html.escape(str(text), quote=True)


def first_upper(text):
	return text[:1].upper() + text[1:]


class Reference:
	"""The document and the lookups the pages are written from."""

	def __init__(self, document):
		self.doc = document
		components = document.get('components', {})
		self.schemas = components.get('schemas', {})
		self.info = document.get('info', {})

		# Groups in the order the document declares them, then any a route
		# names without a declaration, in the order they first appear.
		self.groups = []
		self.group_text = {}
		for tag in document.get('tags', []):
			name = tag.get('name')
			if name and name not in self.group_text:
				self.groups.append(name)
				self.group_text[name] = tag.get('description', '')

		self.ops = []          # (group, method, path, operation, anchor, parameters)
		self.by_route = {}     # (METHOD, path) -> (group, anchor)
		used_ids = set()
		for path, item in document.get('paths', {}).items():
			shared = item.get('parameters', [])
			for method in item:
				if method not in METHODS:
					continue
				op = item[method]
				group = (op.get('tags') or ['other'])[0]
				if group not in self.group_text:
					self.groups.append(group)
					self.group_text[group] = ''
				anchor = slug(op.get('operationId') or (method + path))
				while anchor in used_ids:
					anchor += '-'
				used_ids.add(anchor)
				params = list(shared) + list(op.get('parameters', []))
				self.ops.append((group, method, path, op, anchor, params))
				self.by_route[(method.upper(), path)] = (group, anchor)

	def ops_of(self, group):
		return [o for o in self.ops if o[0] == group]

	def resolve(self, ref):
		"""The object a local reference names, or None."""
		if not ref.startswith('#/'):
			return None
		node = self.doc
		for part in ref[2:].split('/'):
			part = part.replace('~1', '/').replace('~0', '~')
			if not isinstance(node, dict) or part not in node:
				return None
			node = node[part]
		return node


def slug(text):
	s = re.sub(r'[^A-Za-z0-9_-]+', '-', text).strip('-')
	return s or 'x'


def group_file(group):
	return 'group-' + slug(group) + '.html'


# --- text --------------------------------------------------------------------

CODE_SPAN = re.compile(r'`([^`]+)`')
STRONG = re.compile(r'\*\*(.+?)\*\*')
ROUTE = re.compile(r'^(GET|PUT|POST|DELETE|HEAD|PATCH) (/[^\s?]*)')
TABLE_RULE = re.compile(r'^\|(\s*:?-+:?\s*\|)+\s*$')


class Text:
	"""The small part of Markdown the document's texts use: paragraphs, lists
	led by "- ", pipe tables, **strong** and `code`. A code span that names a
	route this document holds becomes a link to it."""

	def __init__(self, ref):
		self.ref = ref

	def code(self, content):
		m = ROUTE.match(content)
		if m and (m.group(1), m.group(2)) in self.ref.by_route:
			group, anchor = self.ref.by_route[(m.group(1), m.group(2))]
			return '<a href="%s#%s"><code>%s</code></a>' % (group_file(group), anchor, esc(content))
		return '<code>' + esc(content) + '</code>'

	def inline(self, text):
		out = []
		pos = 0
		for m in CODE_SPAN.finditer(text):
			out.append(self.plain(text[pos:m.start()]))
			out.append(self.code(m.group(1)))
			pos = m.end()
		out.append(self.plain(text[pos:]))
		return ''.join(out)

	@staticmethod
	def plain(text):
		return STRONG.sub(lambda m: '<strong>' + m.group(1) + '</strong>', esc(text))

	@staticmethod
	def cells(line):
		line = line.strip()
		if line.startswith('|'):
			line = line[1:]
		if line.endswith('|'):
			line = line[:-1]
		return [c.strip() for c in line.split('|')]

	def blocks(self, text):
		if not text:
			return ''
		lines = text.split('\n')
		out = []
		i = 0
		n = len(lines)

		def table_starts(k):
			return lines[k].lstrip().startswith('|') and k + 1 < n and TABLE_RULE.match(lines[k + 1].strip())

		while i < n:
			line = lines[i]
			if not line.strip():
				i += 1
				continue
			if line.startswith('- '):
				items = []
				while i < n and lines[i].startswith('- '):
					item = lines[i][2:]
					i += 1
					while i < n and lines[i].startswith('  ') and lines[i].strip():
						item += ' ' + lines[i].strip()
						i += 1
					items.append(item)
				out.append('<ul>' + ''.join('<li>' + self.inline(x) + '</li>' for x in items) + '</ul>')
				continue
			if table_starts(i):
				head = self.cells(lines[i])
				i += 2
				rows = []
				while i < n and lines[i].lstrip().startswith('|'):
					rows.append(self.cells(lines[i]))
					i += 1
				t = ['<div class="table-scroll"><table><thead><tr>']
				t.extend('<th scope="col">' + self.inline(c) + '</th>' for c in head)
				t.append('</tr></thead><tbody>')
				for r in rows:
					t.append('<tr>' + ''.join('<td>' + self.inline(c) + '</td>' for c in r) + '</tr>')
				t.append('</tbody></table></div>')
				out.append(''.join(t))
				continue
			para = []
			while i < n and lines[i].strip() and not lines[i].startswith('- ') and not table_starts(i):
				para.append(lines[i].strip())
				i += 1
			out.append('<p>' + self.inline(' '.join(para)) + '</p>')
		return '\n'.join(out)


# --- shapes ------------------------------------------------------------------

def schema_name(ref):
	prefix = '#/components/schemas/'
	return ref[len(prefix):] if ref.startswith(prefix) else None


def schema_link(name, base):
	return '<a href="%sschemas.html#schema-%s">%s</a>' % (base, slug(name), esc(name))


def type_html(s, base, links=True):
	"""The type of a value as HTML; a named schema is a link to it, or only its
	name where the text sits inside a link of its own."""
	if not isinstance(s, dict):
		return 'any'
	if '$ref' in s:
		name = schema_name(s['$ref'])
		if not name:
			return esc(s['$ref'])
		return schema_link(name, base) if links else esc(name)
	for key in ('oneOf', 'anyOf'):
		if key in s:
			return ' or '.join(type_html(x, base, links) for x in s[key])
	t = s.get('type')
	if isinstance(t, list):
		return ' or '.join(esc(x) for x in t)
	if t == 'array':
		return 'array of ' + type_html(s.get('items', {}), base, links)
	if t is None:
		return 'object' if 'properties' in s else 'any'
	out = esc(t)
	if 'format' in s:
		out += ' (' + esc(s['format']) + ')'
	return out


def number(v):
	return esc(v)


def constraints(s, text):
	"""What a value is held to beside its type, as short phrases."""
	parts = []
	if not isinstance(s, dict):
		return parts
	if 'enum' in s and 'x-enum-descriptions' not in s:
		parts.append('one of ' + ', '.join(text.code(json.dumps(v)) if not isinstance(v, str) else text.code(v) for v in s['enum']))
	lo, hi = s.get('minimum'), s.get('maximum')
	if lo is not None and hi is not None:
		parts.append('from %s to %s' % (number(lo), number(hi)))
	elif lo is not None:
		parts.append('at least ' + number(lo))
	elif hi is not None:
		parts.append('at most ' + number(hi))
	if 'exclusiveMinimum' in s:
		parts.append('above ' + number(s['exclusiveMinimum']))
	if 'exclusiveMaximum' in s:
		parts.append('below ' + number(s['exclusiveMaximum']))
	lo, hi = s.get('minLength'), s.get('maxLength')
	if lo is not None and hi is not None:
		parts.append('%s to %s characters' % (number(lo), number(hi)))
	elif lo is not None:
		parts.append('at least %s characters' % number(lo))
	elif hi is not None:
		parts.append('at most %s characters' % number(hi))
	if 'x-max-bytes' in s:
		parts.append('at most %s bytes' % number(s['x-max-bytes']))
	if 'minItems' in s or 'maxItems' in s:
		lo, hi = s.get('minItems'), s.get('maxItems')
		if lo is not None and hi is not None:
			parts.append('%s to %s entries' % (number(lo), number(hi)))
		elif lo is not None:
			parts.append('at least %s entries' % number(lo))
		else:
			parts.append('at most %s entries' % number(hi))
	if 'pattern' in s:
		parts.append('matches ' + text.code(s['pattern']))
	if 'default' in s:
		parts.append('default ' + text.code(json.dumps(s['default'])))
	if s.get('additionalProperties') is False and 'properties' not in s:
		parts.append('no members')
	return parts


def inline_object(s):
	"""The properties of an object written out where it stands, or None."""
	if not isinstance(s, dict) or '$ref' in s:
		return None
	if s.get('type') == 'array':
		return inline_object(s.get('items'))
	if 'properties' in s:
		return s
	return None


def fields_html(obj, text, base, depth=0):
	props = obj.get('properties', {})
	if not props:
		return '<p class="muted small">No members.</p>'
	required = set(obj.get('required', []))
	out = ['<ul class="fields">']
	for name, s in props.items():
		out.append(field_html(name, s, name in required, text, base, depth))
	out.append('</ul>')
	return ''.join(out)


def field_html(name, s, required, text, base, depth, where=None):
	out = ['<li class="field"><div class="field-head">']
	out.append('<code class="field-name">%s</code>' % esc(name))
	out.append('<span class="field-type">%s</span>' % type_html(s, base))
	if where:
		out.append('<span class="tag">%s</span>' % esc(where))
	if required:
		out.append('<span class="tag tag-required">required</span>')
	out.append('</div>')
	doc = s.get('description', '') if isinstance(s, dict) else ''
	if doc:
		out.append('<div class="field-doc">%s</div>' % text.blocks(doc))
	meta = constraints(s, text)
	if isinstance(s, dict) and s.get('type') == 'array':
		meta += ['each ' + m for m in constraints(s.get('items', {}), text)]
	if meta:
		out.append('<p class="field-meta">%s</p>' % '; '.join(meta))
	nested = inline_object(s)
	if nested is not None and depth < 6:
		out.append(fields_html(nested, text, base, depth + 1))
	out.append('</li>')
	return ''.join(out)


def body_fields(s, ref, text, base):
	"""The members of a request or answer body, with a named schema opened
	one level so the reader sees the members without leaving the route."""
	if not isinstance(s, dict):
		return ''
	if '$ref' in s:
		name = schema_name(s['$ref'])
		target = ref.schemas.get(name) if name else None
		if isinstance(target, dict) and 'properties' in target:
			return fields_html(target, text, base)
		return ''
	if s.get('type') == 'array':
		items = s.get('items', {})
		inner = body_fields(items, ref, text, base)
		return inner
	if 'properties' in s:
		return fields_html(s, text, base)
	return ''


def placeholder(word):
	return '<' + word + '>'


def sample(s, ref, depth=0, seen=()):
	"""A value of the given shape for reading it at a glance. Every value the
	document gives no example for is a placeholder in angle brackets, so the
	sample cannot be taken for an answer of a box."""
	if not isinstance(s, dict):
		return placeholder('any')
	if depth > 12:
		return placeholder('...')
	if '$ref' in s:
		name = schema_name(s['$ref'])
		if not name or name in seen or name not in ref.schemas:
			return placeholder(name or 'any')
		return sample(ref.schemas[name], ref, depth + 1, seen + (name,))
	if 'example' in s:
		return s['example']
	if 'enum' in s and s['enum']:
		values = [v if isinstance(v, str) else json.dumps(v) for v in s['enum']]
		text = '|'.join(values)
		if len(text) > 48:
			text = '|'.join(values[:3]) + '|...'
		return placeholder(text)
	for key in ('oneOf', 'anyOf'):
		if key in s and s[key]:
			return sample(s[key][0], ref, depth + 1, seen)
	t = s.get('type')
	if isinstance(t, list):
		t = next((x for x in t if x != 'null'), None)
	extra = s.get('additionalProperties')
	if t == 'object' or (t is None and ('properties' in s or isinstance(extra, dict))):
		out = dict((k, sample(v, ref, depth + 1, seen)) for k, v in s.get('properties', {}).items())
		# A map: its keys are not named by the shape, one stands for them all.
		if isinstance(extra, dict):
			out[placeholder('name')] = sample(extra, ref, depth + 1, seen)
		return out
	if t == 'array':
		return [sample(s.get('items', {}), ref, depth + 1, seen)]
	if t == 'boolean':
		return placeholder('true|false')
	if t in ('integer', 'number', 'string'):
		return placeholder(t + (' ' + s['format'] if 'format' in s else ''))
	return placeholder('any')


def json_block(value):
	return '<pre><code>%s</code></pre>' % esc(json.dumps(value, indent=2, ensure_ascii=False))


# --- pages -------------------------------------------------------------------

def path_html(path, drop=''):
	"""A route's path with its parameters marked and a break allowed after
	every slash, so a long path wraps between its segments."""
	if drop and path.startswith(drop + '/'):
		path = path[len(drop):]
	out = []
	for part in re.split(r'(\{[^}]+\})', path):
		if not part:
			continue
		text = esc(part).replace('/', '/<wbr>')
		out.append('<span class="param">%s</span>' % text if part.startswith('{') else text)
	return ''.join(out)


def common_prefix(paths):
	"""The leading path segments most routes share, such as /api/v1, which the
	narrow list beside the page leaves out."""
	counts = {}
	for p in paths:
		segs = p.split('/')
		if len(segs) > 3:
			key = '/'.join(segs[:3])
			counts[key] = counts.get(key, 0) + 1
	if not counts:
		return ''
	best = max(counts, key=lambda k: counts[k])
	return best if counts[best] * 2 > len(paths) else ''


def method_html(method):
	m = method.lower()
	return '<span class="method method-%s">%s</span>' % (esc(m), esc(m.upper()))


def short_summary(op):
	s = op.get('summary', '')
	cut = s.find('; ')
	return first_upper(s[:cut] if cut > 0 else s)


class Site:
	def __init__(self, ref, out):
		self.ref = ref
		self.text = Text(ref)
		self.out = out
		self.api = os.path.join(out, 'api')
		# The way back to the overview, worked out rather than written as
		# "../", so the reference can be written one folder deeper as well.
		self.home = os.path.relpath(out, self.api).replace(os.sep, '/') + '/'
		self.version = ref.info.get('version', '')
		self.prefix = common_prefix([o[2] for o in ref.ops])

	def page(self, filename, title, current, body, sidebar_group=None):
		nav = [('index.html', 'Reference', 'index'), ('schemas.html', 'Schemas', 'schemas'),
		       ('openapi.json', 'openapi.json', 'json')]
		home = self.home + 'index.html'
		links = ['<a href="%s">Overview</a>' % home]
		for href, label, key in nav:
			cur = ' aria-current="page"' if key == current else ''
			links.append('<a href="%s"%s>%s</a>' % (href, cur, esc(label)))
		sidebar = self.sidebar(sidebar_group)
		doc = [
			'<!DOCTYPE html>',
			'<html lang="en">',
			'<head>',
			'<meta charset="utf-8">',
			'<meta name="viewport" content="width=device-width, initial-scale=1">',
			'<meta name="color-scheme" content="light dark">',
			'<title>%s</title>' % esc(title),
			'<link rel="stylesheet" href="doc.css">',
			'</head>',
			'<body>',
			'<a class="skip" href="#main">Skip to content</a>',
			'<header class="site-header"><div class="bar">',
			'<a class="brand" href="%s">Neutrino HTTP API <span>%s</span></a>' % (home, esc(self.version_label())),
			'<nav class="site-nav" aria-label="Site">%s</nav>' % ''.join(links),
			'</div></header>',
			'<div class="page"><div class="layout">',
			'<nav class="sidebar" aria-label="Groups">%s</nav>' % sidebar,
			'<main id="main">',
			'<details class="groups-menu"><summary>Groups</summary><div class="nav-body">%s</div></details>' % sidebar,
			body,
			'</main>',
			'</div></div>',
			'<footer class="site-footer"><div class="bar">',
			'<p>Built from <code>doc/api/openapi.json</code> in the Neutrino source tree.'
			' Each box serves the document for its own build at <code>/api/openapi.json</code>;'
			' for that box, its document is the one that counts.</p>',
			'</div></footer>',
			'</body>',
			'</html>',
			'',
		]
		with open(os.path.join(self.api, filename), 'w', encoding='utf-8', newline='\n') as f:
			f.write('\n'.join(doc))

	def version_label(self):
		return ('version ' + self.version) if self.version else ''

	def sidebar(self, current_group):
		out = ['<p class="nav-title">Groups</p><ul class="nav-groups">']
		for g in self.ref.groups:
			ops = self.ref.ops_of(g)
			if not ops:
				continue
			cur = ' aria-current="page"' if g == current_group else ''
			out.append('<li><a href="%s"%s>%s</a>' % (group_file(g), cur, esc(g)))
			if g == current_group:
				if self.prefix:
					out.append('<p class="nav-note">Paths below <code>%s</code></p>' % esc(self.prefix))
				out.append('<ul class="nav-ops">')
				for _, method, path, op, anchor, _ in ops:
					out.append('<li><a href="#%s">%s<span class="path">%s</span></a></li>'
					           % (anchor, method_html(method), path_html(path, self.prefix)))
				out.append('</ul>')
			out.append('</li>')
		out.append('</ul>')
		return ''.join(out)

	def route_list(self, ops):
		out = ['<ul class="route-list">']
		for group, method, path, op, anchor, _ in ops:
			out.append('<li><a href="%s#%s">%s<span class="path">%s</span><span class="text">%s</span></a></li>'
			           % (group_file(group), anchor, method_html(method), path_html(path),
			              esc(short_summary(op))))
		out.append('</ul>')
		return ''.join(out)

	def index(self):
		body = ['<h1>Reference</h1>']
		body.append('<p class="lead">Every route of the Neutrino HTTP API, grouped as the document'
		            ' groups them%s.</p>' % (' (document version %s)' % esc(self.version) if self.version else ''))
		intro = self.ref.info.get('description', '')
		if intro:
			body.append('<div class="prose">%s</div>' % self.text.blocks(intro))
		body.append('<p class="small muted">%d routes in %d groups. '
		            'The machine-readable form is <a href="openapi.json">openapi.json</a> (OpenAPI %s).</p>'
		            % (len(self.ref.ops), len([g for g in self.ref.groups if self.ref.ops_of(g)]),
		               esc(self.ref.doc.get('openapi', ''))))
		for g in self.ref.groups:
			ops = self.ref.ops_of(g)
			if not ops:
				continue
			body.append('<section class="section" aria-labelledby="g-%s">' % slug(g))
			body.append('<div class="group-head"><h2 id="g-%s"><a href="%s">%s</a></h2></div>'
			            % (slug(g), group_file(g), esc(g)))
			if self.ref.group_text.get(g):
				body.append('<div class="prose">%s</div>' % self.text.blocks(self.ref.group_text[g]))
			body.append(self.route_list(ops))
			body.append('</section>')
		self.page('index.html', 'Reference - Neutrino HTTP API', 'index', '\n'.join(body))

	def group(self, g):
		ops = self.ref.ops_of(g)
		body = ['<h1>%s</h1>' % esc(g)]
		if self.ref.group_text.get(g):
			body.append('<div class="prose lead">%s</div>' % self.text.blocks(self.ref.group_text[g]))
		body.append(self.route_list(ops))
		for item in ops:
			body.append(self.operation(*item))
		self.page(group_file(g), '%s - Neutrino HTTP API' % g, None, '\n'.join(body), sidebar_group=g)

	def access(self, op):
		level = op.get('x-auth-level')
		parts = []
		if level:
			parts.append('<span class="level level-%s">%s</span> %s'
			             % (esc(level), esc(level), esc(LEVEL_TEXT.get(level, ''))))
		schemes = []
		for requirement in op.get('security', []):
			for name in requirement:
				label = SCHEME_TEXT.get(name, name)
				if name == 'session' and op.get('x-csrf-header'):
					label += ' with the <code>%s</code> header' % esc(op['x-csrf-header'])
				else:
					label = esc(label)
				if label not in schemes:
					schemes.append(label)
		out = ''
		if parts:
			out += '<div><dt>Access</dt><dd>%s</dd></div>' % ''.join(parts)
		if schemes:
			out += '<div><dt>Credentials</dt><dd>%s</dd></div>' % ', '.join(schemes)
		return out

	def parameters(self, params):
		if not params:
			return ''
		out = ['<h3 class="part-title">Parameters</h3><ul class="fields">']
		for p in params:
			if '$ref' in p:
				p = self.ref.resolve(p['$ref']) or {}
			s = dict(p.get('schema', {}))
			if p.get('description'):
				s['description'] = p['description']
			out.append(field_html(p.get('name', ''), s, p.get('required', False), self.text, '',
			                      0, where=p.get('in')))
		out.append('</ul>')
		return ''.join(out)

	def request_body(self, rb):
		if not rb:
			return ''
		if '$ref' in rb:
			rb = self.ref.resolve(rb['$ref']) or {}
		out = ['<h3 class="part-title">Request body</h3>']
		if rb.get('description'):
			out.append('<div class="prose">%s</div>' % self.text.blocks(rb['description']))
		for ctype, media in rb.get('content', {}).items():
			s = media.get('schema', {})
			out.append('<p class="response-head"><span class="ctype">%s</span>%s<span class="field-type">%s</span></p>'
			           % (esc(ctype), ' <span class="tag tag-required">required</span>' if rb.get('required') else '',
			              type_html(s, '')))
			out.append(body_fields(s, self.ref, self.text, ''))
			if 'example' in media:
				out.append('<figure class="example"><figcaption>Example</figcaption>%s</figure>'
				           % json_block(media['example']))
		return ''.join(out)

	def responses(self, op):
		answers = []
		refusals = []
		for code, r in op.get('responses', {}).items():
			if code == 'default':
				continue
			resolved = self.ref.resolve(r['$ref']) if '$ref' in r else r
			resolved = resolved or {}
			if code.isdigit() and int(code) >= 400:
				refusals.append((code, resolved))
			else:
				answers.append((code, resolved))
		out = []
		if answers:
			out.append('<h3 class="part-title">Responses</h3>')
		for code, r in answers:
			out.append('<div class="response"><p class="response-head"><span class="status">%s</span> %s</p>'
			           % (esc(code), self.text.inline(first_upper(r.get('description', '')))))
			for ctype, media in r.get('content', {}).items():
				s = media.get('schema', {})
				out.append('<p class="response-head"><span class="ctype">%s</span><span class="field-type">%s</span></p>'
				           % (esc(ctype), type_html(s, '')))
				if ctype == 'application/json':
					out.append(body_fields(s, self.ref, self.text, ''))
					value = media['example'] if 'example' in media else sample(s, self.ref)
					label = 'Example' if 'example' in media else 'Shape of the answer, with placeholder values'
					if value is not None:
						out.append('<details class="example"><summary>%s</summary>%s</details>'
						           % (label, json_block(value)))
			out.append('</div>')
		if refusals:
			out.append('<h3 class="part-title">Refusals</h3><ul class="refusals">')
			for code, r in refusals:
				listed = False
				for ctype, media in r.get('content', {}).items():
					for key, ex in media.get('examples', {}).items():
						value = (self.ref.resolve(ex['$ref']) if '$ref' in ex else ex) or {}
						value = value.get('value', {})
						kind = value.get('type', '')
						name = kind.rsplit('/', 1)[-1] if kind else key
						out.append('<li><span class="status">%s</span><span><code>%s</code></span>'
						           '<span class="detail">%s</span></li>'
						           % (esc(code), esc(name), self.text.inline(value.get('detail', ''))))
						listed = True
				if not listed:
					out.append('<li><span class="status">%s</span><span>%s</span></li>'
					           % (esc(code), self.text.inline(first_upper(r.get('description', '')))))
			out.append('</ul>')
			out.append('<p class="small muted">Every refusal is a <a href="schemas.html#schema-problem">problem</a>'
			           ' document (<code>application/problem+json</code>); its <code>type</code> ends in the code above.'
			           ' Any other refusal has the same shape.</p>')
		return ''.join(out)

	def operation(self, group, method, path, op, anchor, params):
		out = ['<section class="op" id="%s" aria-labelledby="%s-h">' % (anchor, anchor)]
		out.append('<h2 class="op-title" id="%s-h">%s<span class="path">%s</span>'
		           '<a class="anchor" href="#%s" aria-label="Link to %s %s">#</a></h2>'
		           % (anchor, method_html(method), path_html(path), anchor, esc(method.upper()), esc(path)))
		if op.get('summary'):
			out.append('<p class="summary">%s</p>' % self.text.inline(first_upper(op['summary'])))
		facts = self.access(op)
		if facts:
			out.append('<dl class="facts">%s</dl>' % facts)
		if op.get('description'):
			out.append('<div class="prose">%s</div>' % self.text.blocks(op['description']))
		out.append(self.parameters(params))
		out.append(self.request_body(op.get('requestBody')))
		out.append(self.responses(op))
		out.append('</section>')
		return ''.join(out)

	def schemas(self):
		body = ['<h1>Schemas</h1>',
		        '<p class="lead">The named shapes the routes take and answer with. A route links here'
		        ' where a member holds one of them.</p>']
		names = list(self.ref.schemas.keys())
		body.append('<ul class="route-list">')
		for name in names:
			body.append('<li><a href="#schema-%s"><span class="field-type">%s</span><span class="path">%s</span></a></li>'
			            % (slug(name), type_html(self.ref.schemas[name], '', links=False), esc(name)))
		body.append('</ul>')
		for name in names:
			s = self.ref.schemas[name]
			body.append('<section class="op" id="schema-%s" aria-labelledby="schema-%s-h">' % (slug(name), slug(name)))
			body.append('<h2 class="op-title" id="schema-%s-h"><span class="path">%s</span>'
			            '<a class="anchor" href="#schema-%s" aria-label="Link to %s">#</a></h2>'
			            % (slug(name), esc(name), slug(name), esc(name)))
			body.append('<p class="response-head"><span class="field-type">%s</span></p>' % type_html(s, ''))
			if s.get('description'):
				body.append('<div class="prose">%s</div>' % self.text.blocks(s['description']))
			meta = constraints(s, self.text)
			if meta:
				body.append('<p class="field-meta">%s</p>' % '; '.join(meta))
			obj = inline_object(s)
			if obj is not None:
				body.append(fields_html(obj, self.text, ''))
			body.append('</section>')
		self.page('schemas.html', 'Schemas - Neutrino HTTP API', 'schemas', '\n'.join(body))

	def build(self, document_path):
		os.makedirs(self.api)
		shutil.copyfile(STYLE, os.path.join(self.api, 'doc.css'))
		shutil.copyfile(document_path, os.path.join(self.api, 'openapi.json'))
		self.index()
		for g in self.ref.groups:
			if self.ref.ops_of(g):
				self.group(g)
		self.schemas()


# --- the folder --------------------------------------------------------------

def prepare(out):
	if os.path.lexists(out) and not os.path.isdir(out):
		fail('%s exists and is not a folder' % out)
	if os.path.isdir(out) and os.listdir(out):
		if not os.path.isfile(os.path.join(out, MARKER)):
			fail('%s is not empty and was not written by this script; name a new folder' % out)
		# A marked folder holds what this script wrote; a repository found in it
		# was put there since, and is not cleared with it.
		for top, dirs, files in os.walk(out):
			if '.git' in dirs or '.git' in files:
				fail('%s holds a git repository (%s); name a new folder' % (out, top))
		shutil.rmtree(out)
	os.makedirs(out, exist_ok=True)
	with open(os.path.join(out, MARKER), 'w', encoding='utf-8') as f:
		f.write('written by doc/api/build-site.py; the folder is cleared on the next run\n')


def copy_doc(out):
	"""doc/ as it is, without doc/api/, which holds the sources of the reference."""
	for top, dirs, files in os.walk(DOC_ROOT):
		rel = os.path.relpath(top, DOC_ROOT)
		if rel == 'api' or rel.startswith('api' + os.sep):
			dirs[:] = []
			continue
		dirs[:] = sorted(d for d in dirs if d != '__pycache__' and not d.startswith('.'))
		target = out if rel == '.' else os.path.join(out, rel)
		os.makedirs(target, exist_ok=True)
		for name in sorted(files):
			if name.startswith('.'):
				continue
			shutil.copyfile(os.path.join(top, name), os.path.join(target, name))


# --- links -------------------------------------------------------------------

class Anchors(html.parser.HTMLParser):
	def __init__(self):
		super().__init__(convert_charrefs=True)
		self.ids = set()
		self.links = []

	def handle_starttag(self, tag, attrs):
		for k, v in attrs:
			if k in ('id', 'name') and v:
				if k == 'id' or tag == 'a':
					self.ids.add(v)
			if k in ('href', 'src') and v is not None:
				self.links.append(v)


def read_page(path):
	with open(path, 'rb') as f:
		data = f.read()
	try:
		text = data.decode('utf-8')
	except UnicodeDecodeError:
		text = data.decode('latin-1')
	p = Anchors()
	p.feed(text)
	return p


def check_links(out, strict):
	"""Every relative link and anchor in the written pages, followed. A broken
	link in a page this script wrote, or in one listed in strict, fails the
	run; one in a page copied from doc/ as it is only gets reported."""
	pages = {}
	for top, _, files in os.walk(out):
		for name in files:
			if name.endswith('.html'):
				full = os.path.join(top, name)
				pages[os.path.normpath(full)] = read_page(full)
	errors, notes = [], []
	checked = 0
	for page, parsed in sorted(pages.items()):
		rel_page = os.path.relpath(page, out)
		must = rel_page in strict or rel_page.startswith('api' + os.sep)
		for link in parsed.links:
			u = urllib.parse.urlsplit(link)
			if u.scheme or link.startswith('//') or link.startswith('mailto:'):
				continue
			checked += 1
			target = page if not u.path else os.path.normpath(
				os.path.join(os.path.dirname(page), urllib.parse.unquote(u.path)))
			if u.path.endswith('/') or os.path.isdir(target):
				target = os.path.join(target, 'index.html')
			problem = None
			if os.path.commonpath([os.path.abspath(target), os.path.abspath(out)]) != os.path.abspath(out):
				problem = 'leads out of the site'
			elif not os.path.isfile(target):
				problem = 'no file %s' % os.path.relpath(target, out)
			elif u.fragment:
				t = pages.get(os.path.normpath(target))
				if t is not None and urllib.parse.unquote(u.fragment) not in t.ids:
					problem = 'no anchor #%s in %s' % (u.fragment, os.path.relpath(target, out))
			if problem:
				(errors if must else notes).append('%s: %s (%s)' % (rel_page, link, problem))
	return checked, errors, notes


def main(argv):
	args = argv[1:]
	document_path = os.path.join(HERE, 'openapi.json')
	if len(args) == 3 and args[0] == '--document':
		document_path = args[1]
		args = args[2:]
	if len(args) != 1 or args[0].startswith('-'):
		sys.stderr.write('usage: build-site.py [--document <openapi.json>] <out-dir>\n')
		return 2
	if os.path.islink(args[0].rstrip(os.sep) or args[0]):
		fail('%s is a symbolic link; name the folder itself' % args[0])
	# Compared as real paths, so a link on the way cannot lead into doc/.
	out = os.path.realpath(args[0])
	if os.path.commonpath([out, DOC_ROOT]) in (out, DOC_ROOT):
		fail('the output folder may not be doc/, lie inside it or hold it')

	try:
		with open(document_path, encoding='utf-8') as f:
			document = json.load(f)
	except (OSError, ValueError) as e:
		fail('cannot read %s: %s' % (document_path, e))
	if not isinstance(document, dict) or not str(document.get('openapi', '')).startswith('3.'):
		fail('%s is not an OpenAPI 3 document' % document_path)

	prepare(out)
	copy_doc(out)
	Site(Reference(document), out).build(document_path)

	checked, errors, notes = check_links(out, {'index.html'})
	for n in notes:
		sys.stderr.write('build-site.py: note: ' + n + '\n')
	if errors:
		for e in errors:
			sys.stderr.write('build-site.py: broken link: ' + e + '\n')
		fail('%d broken links in the pages written here' % len(errors))
	print('build-site.py: wrote %s (%d links followed, %d in copied pages lead nowhere)'
	      % (out, checked, len(notes)))
	return 0


if __name__ == '__main__':
	sys.exit(main(sys.argv))
