#!/usr/bin/env python3
# format-openapi.py - writes the API document in the layout doc/api/openapi.json
# is kept in
#
# Copyright (C) 2026 Thilo Graf
#
# License: GPL-2.0-or-later
#
# usage: format-openapi.py <document> <out>
#
# The server writes the document on a single line. Kept like that, every change
# to a route would be a change to that one line and a diff could not show what
# moved. The file is therefore kept one member per line, indented with tabs,
# with the members in the order the server writes them and nothing else
# changed. Before anything is written, the layout is read back and written
# compact again, and that has to give the server's bytes exactly: a document
# this layout cannot carry without loss is refused here rather than turned
# into a file that differs from what the box answers.
#
# Only the Python standard library is used.

import json
import os
import sys


def fail(what):
	sys.stderr.write('format-openapi.py: ' + what + '\n')
	return 1


def main(argv):
	if len(argv) != 3:
		sys.stderr.write('usage: format-openapi.py <document> <out>\n')
		return 2
	source, target = argv[1], argv[2]

	try:
		with open(source, 'rb') as f:
			raw = f.read()
	except OSError as e:
		return fail('cannot read %s: %s' % (source, e.strerror))

	try:
		document = json.loads(raw.decode('utf-8'))
	except (UnicodeDecodeError, ValueError) as e:
		return fail('%s is not a JSON document: %s' % (source, e))
	if not isinstance(document, dict) or 'openapi' not in document:
		return fail('%s is not an OpenAPI document' % source)

	text = json.dumps(document, ensure_ascii=False, indent='\t') + '\n'

	compact = json.dumps(json.loads(text), ensure_ascii=False, separators=(',', ':'))
	if compact.encode('utf-8') != raw:
		return fail('%s does not come back byte for byte from this layout; '
		            'the layout would change the document' % source)

	# Written beside the target and renamed over it, so an interrupted run
	# leaves the old file rather than half of a new one.
	partial = target + '.part'
	try:
		with open(partial, 'w', encoding='utf-8', newline='\n') as f:
			f.write(text)
		os.replace(partial, target)
	except OSError as e:
		try:
			os.unlink(partial)
		except OSError:
			pass
		return fail('cannot write %s: %s' % (target, e.strerror))
	return 0


if __name__ == '__main__':
	sys.exit(main(sys.argv))
