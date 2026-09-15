// The shapes of the API, written out of the document the server writes about itself.
//
// The server already states every answer, every body and every parameter of every
// route, with its type, its bounds and lately its enumerations, in
// src/httpd/doc/openapi.cpp. Writing those shapes a second time for the page would be
// two descriptions of one thing, and this tree has paid for that twice: a field
// renamed on one side and not the other is a screen that draws undefined, and nothing
// reports it.
//
// So nothing here is transcribed. This reads the document and prints the
// declarations, and the check that calls it prints them again on every run from the
// document that build just produced.
//
// What it will not do is guess. A construct this does not understand stops it with a
// message naming the place, because a generator that skips what it cannot read
// produces a file that type checks the parts nobody added lately.

'use strict';

const fs = require('fs');

const SCALARS = { string: 'string', integer: 'number', number: 'number', boolean: 'boolean' };

// Every keyword this reads. One that turns up and is not here is a keyword the
// server started writing and this stopped understanding, which is the moment
// to extend this rather than the moment to let it through.
const KNOWN = [
	'type', 'description', 'properties', 'required', 'additionalProperties',
	'items', 'enum', 'format', 'minimum', 'maximum', 'minLength', 'maxLength',
	'pattern', 'default', 'example', '$ref', 'x-max-bytes', 'title', 'nullable'
];

// Keywords that bound a value without saying anything about its type: an array of
// strings with at most 4096 of them is still string[]. The server states these so a
// caller is refused before the box allocates, and a type cannot carry a count. They are
// listed rather than let through unnamed, because the difference between a keyword that
// shapes a type and one that only bounds a value is the whole reason the list above
// refuses what it does not know.
const BOUNDS = ['minItems', 'maxItems', 'minProperties', 'maxProperties'];

let source = '';

function die(where, what) {
	process.stderr.write('openapi-types.js: ' + where + ': ' + what + '\n');
	process.exit(1);
}

// channel-page becomes ChannelPage. The document names its shapes the way its
// addresses are spelt and TypeScript names types the way TypeScript does, and
// this is the one place the two meet.
function typeName(name) {
	return name.split(/[-_/]/).filter(Boolean).map(function (part) {
		return part.charAt(0).toUpperCase() + part.slice(1);
	}).join('');
}

function quote(text) {
	return "'" + String(text).replace(/\\/g, '\\\\').replace(/'/g, "\\'") + "'";
}

function indent(depth) {
	return '\t'.repeat(depth);
}

// The description the document carries, put where a reader of the type sees
// it: an editor shows this beside the field, which is the whole reason the
// server writes prose into its schema at all.
function comment(schema, depth) {
	if (!schema || typeof schema.description !== 'string' || schema.description === '') {
		return '';
	}
	return indent(depth) + '/** ' + schema.description.replace(/\*\//g, '* /') + ' */\n';
}

function check(schema, where) {
	for (const key of Object.keys(schema)) {
		if (KNOWN.indexOf(key) === -1 && BOUNDS.indexOf(key) === -1) {
			die(where, 'the schema carries ' + key + ', which this does not read');
		}
	}
	if (schema.nullable !== undefined) {
		die(where, 'nullable is OpenAPI 3.0 and this document says 3.1');
	}
}

function render(schema, where, depth) {
	if (schema === undefined || schema === null) {
		die(where, 'there is no schema here');
	}
	check(schema, where);

	if (schema.$ref !== undefined) {
		const prefix = '#/components/schemas/';
		if (schema.$ref.indexOf(prefix) !== 0) {
			die(where, schema.$ref + ' does not name a schema of this document');
		}
		return typeName(schema.$ref.slice(prefix.length));
	}

	if (schema.enum !== undefined) {
		if (schema.type !== 'string') {
			die(where, 'an enumeration of ' + schema.type + ', which this only reads for strings');
		}
		if (!Array.isArray(schema.enum) || schema.enum.length === 0) {
			die(where, 'an enumeration with nothing in it');
		}
		return schema.enum.map(quote).join(' | ');
	}

	if (schema.type === 'array') {
		// An array the document states no element type for says nothing about
		// its elements, and unknown is what that is. Not a guess and not a
		// refusal: the document is readable, it just stops there, and a reader
		// of the type is then made to look before using one.
		if (schema.items === undefined) {
			return 'unknown[]';
		}
		const inner = render(schema.items, where + '[]', depth);
		// Parenthesised because a union of literals followed by [] binds the
		// wrong way round, and the two spellings mean different things.
		return /[|&]/.test(inner) ? '(' + inner + ')[]' : inner + '[]';
	}

	if (schema.type === 'object') {
		return renderObject(schema, where, depth);
	}

	if (SCALARS[schema.type] !== undefined) {
		return SCALARS[schema.type];
	}

	die(where, 'type ' + JSON.stringify(schema.type) + ', which this does not read');
	return '';
}

function renderObject(schema, where, depth) {
	const properties = schema.properties || {};
	const names = Object.keys(properties);
	const required = schema.required || [];

	if (names.length === 0) {
		// A body or an answer stated as an object with nothing in it. Not the
		// same as one with no shape at all: this one is an object, and a
		// caller may not put a number there.
		if (schema.additionalProperties === false) {
			return 'Record<string, never>';
		}
		// A map: the names are the caller's, so the document states no
		// properties, and what it does state is what every value is. Reading
		// only the object and not the value would turn a map of numbers into a
		// map of unknown, which is a shape the server states and this dropped.
		if (schema.additionalProperties !== undefined && schema.additionalProperties !== true) {
			return 'Record<string, ' + render(schema.additionalProperties, where + '.*', depth) + '>';
		}
		return 'Record<string, unknown>';
	}

	let out = '{\n';
	for (const name of names) {
		const field = properties[name];
		const optional = required.indexOf(name) === -1 ? '?' : '';
		out += comment(field, depth + 1);
		out += indent(depth + 1) + spell(name) + optional + ': ' + render(field, where + '.' + name, depth + 1) + ';\n';
	}
	// A shape that admits fields it does not name admits them here too, so
	// that a caller may read one without the check calling it a typing slip.
	if (schema.additionalProperties !== undefined && schema.additionalProperties !== false) {
		const extra = schema.additionalProperties === true
			? 'unknown'
			: render(schema.additionalProperties, where + '.*', depth + 1);
		out += indent(depth + 1) + '[name: string]: ' + extra + ';\n';
	}
	out += indent(depth) + '}';
	return out;
}

// A field whose name is not an identifier is quoted rather than dropped.
function spell(name) {
	return /^[A-Za-z_$][A-Za-z0-9_$]*$/.test(name) ? name : quote(name);
}

// The parameters of one operation, split the way the client splits them: the
// pieces of the template on one side, the query on the other.
function renderParameters(operation, where, wanted, depth) {
	const taken = (operation.parameters || []).filter(function (one) { return one.in === wanted; });
	if (taken.length === 0) {
		return 'never';
	}
	let out = '{\n';
	for (const one of taken) {
		if (one.in !== 'path' && one.in !== 'query') {
			die(where, 'a parameter in ' + one.in + ', which this does not read');
		}
		// A piece of the template is always required; the document says so and
		// the client throws without it.
		const optional = (one.required === true || one.in === 'path') ? '' : '?';
		if (one.description) {
			out += indent(depth + 1) + '/** ' + one.description.replace(/\*\//g, '* /') + ' */\n';
		}
		out += indent(depth + 1) + spell(one.name) + optional + ': '
			+ render(one.schema, where + ' ' + one.in + ' ' + one.name, depth + 1) + ';\n';
	}
	out += indent(depth) + '}';
	return out;
}

function renderBody(operation, where, depth) {
	if (operation.requestBody === undefined) {
		return 'never';
	}
	const content = operation.requestBody.content || {};
	const kinds = Object.keys(content);
	if (kinds.length !== 1) {
		die(where, 'a body of ' + kinds.join(', ') + ', and one call sends one kind');
	}
	if (kinds[0] !== 'application/json') {
		/* A body that is not a document. The one route of that shape puts a file on the
		   box as the bytes themselves, and a run of bytes is the whole of what the
		   document can say about it. What a caller may hand over is whatever fetch takes.
		 *
		   Held to exactly that spelling rather than let through on the content type
		   alone, so a body somebody starts writing in a third kind is still an error
		   here. */
		const raw = content[kinds[0]].schema || {};
		if (raw.type !== 'string' || raw.format !== 'binary') {
			die(where, 'a body of ' + kinds[0] + ', and this client sends JSON');
		}
		return operation.requestBody.required === true ? 'BodyInit' : 'BodyInit | undefined';
	}
	const shape = render(content['application/json'].schema, where + ' body', depth);
	return operation.requestBody.required === true ? shape : shape + ' | undefined';
}

// What a caller gets back. An answer with nothing in it is null and not void:
// the client hands back null for a 204 and for an empty body, and a screen
// that assigns it somewhere should have to say so.
function renderResult(operation, where, depth) {
	const responses = operation.responses || {};
	const ok = responses['2XX'] || responses['200'];
	if (ok === undefined) {
		die(where, 'no answer under 2XX, so there is nothing to say a caller gets');
	}
	const content = ok.content || {};
	const kinds = Object.keys(content);
	if (kinds.length === 0) {
		return 'null';
	}
	if (kinds.length !== 1) {
		die(where, 'more than one content type in one answer: ' + kinds.join(', '));
	}
	if (kinds[0] === 'text/event-stream') {
		// Not a value a caller reads: this one is opened as a stream and never
		// goes through the one call.
		return 'never';
	}
	if (kinds[0] !== 'application/json') {
		die(where, 'an answer of ' + kinds[0] + ', which this does not read');
	}
	return render(content['application/json'].schema, where + ' answer', depth);
}

function main() {
	const from = process.argv[2];
	const to = process.argv[3];
	if (!from || !to) {
		process.stderr.write('usage: openapi-types.js <openapi.json> <out.d.ts>\n');
		process.exit(2);
	}
	source = from;

	let document;
	try {
		document = JSON.parse(fs.readFileSync(from, 'utf8'));
	} catch (e) {
		die(from, 'is not readable as JSON: ' + e.message);
	}

	if (typeof document.openapi !== 'string' || document.openapi.indexOf('3.1') !== 0) {
		die(from, 'says OpenAPI ' + document.openapi + ' and this reads 3.1');
	}

	const schemas = (document.components || {}).schemas || {};
	const paths = document.paths || {};
	const schemaNames = Object.keys(schemas);
	const pathNames = Object.keys(paths);

	// A document that lost its schemas or its routes would otherwise produce a
	// file that declares nothing and type checks everything.
	if (schemaNames.length < 20) {
		die(from, 'names ' + schemaNames.length + ' schemas, and this API has more than that');
	}
	if (pathNames.length < 40) {
		die(from, 'names ' + pathNames.length + ' addresses, and this API has more than that');
	}

	let out = '';
	out += '// The shapes of this box\'s API, as the box states them.\n';
	out += '//\n';
	out += '// WRITTEN BY test/web/openapi-types.js. Nothing here is edited by hand and\n';
	out += '// nothing here is committed: it is printed again on every run of\n';
	out += '// test/web/check-web-types.sh out of the openapi.json that build produced,\n';
	out += '// so it cannot disagree with the server. To change a shape, change\n';
	out += '// src/httpd/doc/openapi.cpp and the route behind it.\n';
	out += '\n';
	out += 'declare namespace Api {\n';

	for (const name of schemaNames) {
		const schema = schemas[name];
		out += '\n';
		out += comment(schema, 1);
		out += indent(1) + 'type ' + typeName(name) + ' = ' + render(schema, name, 1) + ';\n';
	}

	// Every route, keyed the way the client asks for one: the method and the
	// template, spelt exactly as the document spells them, so that a call site
	// which named an address this API does not have is a call site with no
	// answer type rather than one nobody checked.
	out += '\n';
	out += indent(1) + '/** Every route of this API, keyed as the one call spells it. */\n';
	out += indent(1) + 'interface Ops {\n';
	let operations = 0;
	for (const path of pathNames) {
		for (const method of Object.keys(paths[path])) {
			const operation = paths[path][method];
			const where = method.toUpperCase() + ' ' + path;
			out += '\n';
			if (operation.summary) {
				out += indent(2) + '/** ' + operation.summary.replace(/\*\//g, '* /') + ' */\n';
			}
			out += indent(2) + quote(where) + ': {\n';
			out += indent(3) + 'params: ' + renderParameters(operation, where, 'path', 3) + ';\n';
			out += indent(3) + 'query: ' + renderParameters(operation, where, 'query', 3) + ';\n';
			out += indent(3) + 'body: ' + renderBody(operation, where, 3) + ';\n';
			out += indent(3) + 'result: ' + renderResult(operation, where, 3) + ';\n';
			out += indent(2) + '};\n';
			operations += 1;
		}
	}
	out += indent(1) + '}\n';
	out += '}\n';

	fs.writeFileSync(to, out);
	process.stdout.write('openapi-types.js: ' + schemaNames.length + ' shapes and '
		+ operations + ' routes out of ' + from + '\n');
}

main();
