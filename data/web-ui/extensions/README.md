# A yWeb extension in 10 minutes

*Deutsche Fassung: [README.de.md](README.de.md)*

Modern yWeb loads extensions automatically from this directory.
An extension is **a folder with a manifest** — no patching of core
pages, no build required.

## 1. Create a folder

On the box (webroot, e.g. `/share/tuxbox/neutrino/httpd/`):

```
extensions/
└── my-extension/
    ├── manifest.json
    └── index.html
```

## 2. Write the manifest (`manifest.json`)

```json
{
	"name": "My Extension",
	"version": "1.0",
	"slot": "tools",
	"entry": "index.html",
	"description": "Short subtitle for the tile"
}
```

| Field | Meaning |
|------|-----------|
| `name` | Display name (nav link, tile, manager) |
| `version` | free-form, shown in the manager |
| `slot` | Attachment point: `nav` (main navigation), `tools` (tile in the tools grid), `dashboard` (card), `page` (direct URL only) |
| `entry` | Entry file relative to the extension folder |
| `description` | optional subtitle |
| `settings` | optional: own category under Settings (see below) |

### Optional: own settings category (`settings`)

An enabled extension with a `settings` block gets its own category in
the settings sidebar. Fields are declarative (same schema as the core
registry) and are loaded, diffed and saved generically — no custom JS
needed:

```json
"settings": {
	"label": "My Extension",
	"config": "yweb",
	"sections": [
		{ "title": "Section", "fields": [
			{ "key": "my_option", "label": "Option", "type": "text" },
			{ "key": "my_switch", "label": "Switch", "type": "bool01" },
			{ "type": "action", "id": "my-button", "label": "Action",
			  "button": "Go", "api": { "endpoint": "exec",
			  "params": "MyScript&action", "raw": true },
			  "success": "Done" }
		] }
	]
}
```

| Field | Meaning |
|------|-----------|
| `label` | Category name in the sidebar (default: `name`) |
| `config` | Target config for `key` fields: `yweb` (default), `neutrino`, `nhttpd` |
| `sections[].fields[]` | Field types `text`, `number`, `select`, `bool`, `bool01`, `password`, `info`, `action` |

`key` fields are persisted in the chosen config file; `action` fields
fire a v4 call (typically `exec` on a script); without a `success` text
the script output is shown as toast feedback.
Reference: [`hallo-yweb/manifest.json`](hallo-yweb/manifest.json).

### Optional: own language pack (`languages/`)

An extension can localize itself — flat JSON dictionaries per language,
named like the core language files:

```
my-extension/
├── manifest.json
├── index.html
└── languages/
    ├── English.json
    └── Deutsch.json
```

```json
{
	"manifest.name": "My Extension",
	"manifest.description": "Tile subtitle",
	"settings.greeting": "Greeting text",
	"page.title": "Hello!"
}
```

- **Reserved keys** `manifest.name` / `manifest.description` translate
  name and subtitle wherever the extension attaches
  (nav, tile, dashboard, manager).
- **Manifest strings** (settings `label`, section `title`, field
  `label`, `button`, `success`) are translated when their value starts
  with `L:`: `"label": "L:settings.greeting"` → key `settings.greeting`
  from the language pack. Values without `L:` stay literals.
- **On your own pages**: at init `await ywebExtLangLoad('<folder>')`,
  then `ywebExtL('<folder>', 'page.title', 'Hello!')` — the third
  parameter is the English fallback, which also lives inline in the
  markup. Only the active language is fetched (no second fetch of
  English).
- Always keep English + German in step; identical keysets.

Reference: [`hallo-yweb/languages/`](hallo-yweb/languages/).

## 3. Write the page (`index.html`)

Copy template: [`hallo-yweb/hallo.html`](hallo-yweb/hallo.html). Key points:

- Embed the yWeb header/nav:
  `<div class="yweb-header-wrapper" data-page="tools"></div>` +
  `<script src="/yweb-header.js"></script>` + `ywebInitHeader('tools');`
- Styling: link Pico CSS + `/css/yweb.css` — use the ready-made classes
  (`yweb-card`, `yweb-form-row`, `yweb-btn`).
- **Fetch data only through `/api/v4/`**, easiest via the helper:
  `var sys = await ywebApi('sysinfo');`
  Reference of all endpoints: Doxygen page "Neutrino Web-API v4"
  (`doc/nhttpd/apiv4.dox` in the source tree).

## 4. Enable

Settings → **Extensions**: the new extension shows up automatically in
the list → enable the switch, optionally change the slot → save →
reload the page.

Technically the switch lives in `yWeb.conf`
(`ext_<foldername>_enabled=1`, optionally `ext_<foldername>_slot=...`).

## Rules

- No dependencies on core internals — only the documented
  `/api/v4/` surface (plus the `exec` contract for script actions).
- The folder name is the stable key (config keys, URL
  `/extensions/<folder>/...`) — do not change it after publication.
- Extensions are **off** by default (opt-in by the user).
