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
| `entry_js` | optional: runtime script, loaded on every modern page (see below) |
| `description` | optional subtitle |
| `settings` | optional: own category under Settings (see below) |

At least one of `entry`, `entry_js` or `settings.sections` must be
present, otherwise the manifest is ignored.

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

### Optional: runtime script (`entry_js`)

For every *enabled* extension whose manifest names an `entry_js` file,
every modern page loads
`<script src="/extensions/<folder>/<entry_js>?v=<version>">` —
independent of the `slot` (the slot only governs UI links; a manifest
with *only* `entry_js` is valid). Scripts are injected in alphabetical
folder order with `async=false`, so execution order is deterministic.
There is **no timing guarantee** relative to page code: register hooks,
don't poll.

The primary hook is the **remote control skin renderer** — a
hardware-specific layout for the on-screen remote:

```js
// skin.js — manifest: { "name": "…", "version": "1.0", "entry_js": "skin.js" }
ywebRegisterRemoteRenderer(function (opts) {
	return '<div class="yweb-remote" id="' + opts.id +
		'" data-rc-onkey="' + opts.onKey + '">' +
		'<button type="button" class="yweb-rc-btn" ' +
			'onclick="' + opts.onKey + "('KEY_OK')\">OK</button>" +
		'</div>';
});
```

Contract:

- Return a single root element with `id=opts.id` and
  `data-rc-onkey=opts.onKey`; `opts.onKey` is the **name** of a global
  function taking a `KEY_*` string.
- Valid key names come from the generated `rcsim.h` table (source:
  `src/driver/rcinput.h`); note **`KEY_TIME`** (not `KEY_TIMER`) and
  `KEY_FAVORITES`.
- Use real `<button type="button">` elements; icon-only keys need
  `aria-label`, inline SVGs `aria-hidden="true"`. Buttons opt into
  auto-repeat and press feedback via `data-rc-repeat="1"` +
  `data-rc-key="KEY_…"`.
- Never set `YWEB_CONFIG.remoteRenderer` directly —
  `ywebRegisterRemoteRenderer()` also re-renders instances that were
  built before the script loaded (open modals stay open). If two skins
  are active, the alphabetically later folder wins (with a console
  warning).

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

## 5. Install / update / uninstall (manager)

Settings → **Extensions** also manages the lifecycle
(backend: `scripts/Y_Extensions.sh` via the `exec` contract):

- **Install**: upload a `tar.gz` archive, or paste a download URL
  (e.g. a GitHub release asset). An update is simply installing the
  same extension again — the folder is replaced, all config keys
  (enable/slot and extension settings) survive.
- **Check for updates**: explicit button; compares the installed
  versions against a hosted `index.json`
  (default: the `tuxbox-neutrino/yweb-extensions` repository,
  overridable via the `yWeb.conf` key `ext_index_url`). Available
  updates show a badge and an update button in the row.
- **Uninstall**: row button with confirmation. Removes the extension
  folder and the scripts it shipped; the `ext_<folder>_*` config keys
  are blanked (the config API cannot delete keys — empty values are
  inert leftovers).

### Packaging contract (`<folder>.tar.gz`)

- The archive contains **exactly one top-level folder**; its name is
  the extension key (`^[a-zA-Z0-9][a-zA-Z0-9_-]*$`) and it must
  contain `manifest.json`.
- It unpacks to `<webroot>/extensions/<folder>/`.
- Shell scripts in `<folder>/scripts/*.sh` are copied to
  `<webroot>/scripts/` on install (available for `exec`/`action`
  fields) and removed on uninstall — name them uniquely.
- Scripts are copied **verbatim** — build-time placeholders like
  `%(PRIVATE_HTTPDDIR)` are *not* expanded (that only happens during
  the core's `make install`). Locate shared libraries from the script
  path instead:
  ```sh
  SCRIPTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
  [ -r "$SCRIPTDIR/_Y_Globals.sh" ] && . "$SCRIPTDIR/_Y_Globals.sh"
  ```
- Build command (used by the release workflows):

```sh
mkdir -p /tmp/stage/<folder> && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/<folder>/ && \
tar -czf <folder>.tar.gz -C /tmp/stage <folder>
```

## Rules

- No dependencies on core internals — only the documented
  `/api/v4/` surface (plus the `exec` contract for script actions).
- The folder name is the stable key (config keys, URL
  `/extensions/<folder>/...`) — do not change it after publication.
- Extensions are **off** by default (opt-in by the user).

## Official extensions

The official extensions live in their own versioned repositories
under `github.com/tuxbox-neutrino/yweb-extension-<name>` (installer,
m3u-converter, camds, infoicons, hallo-yweb) and are installable
through the manager; the update index lives in the
`yweb-extensions` meta repository. Only **hallo-yweb** additionally
stays bundled here as an offline copy-paste example — its repository
is the source of truth.
