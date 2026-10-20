# A Neutrino-Web-UI extension in 10 minutes

*Deutsche Fassung: [README.de.md](README.de.md)*

The Neutrino-Web-UI loads extensions automatically from this directory.
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
| `icon` | optional: a monochrome SVG in the folder (e.g. `icon.svg`), drawn in the text colour on the nav entry and the tile; a star without it |
| `settings` | optional: own category under Settings (see below) |
| `scripts` | optional: the shell scripts the box runs for it, by name without `.sh` (see the packaging contract) |
| `default` | optional: `"on"` switches the extension on as long as the settings say nothing about it; the user's own switch wins. Meant for an extension that takes the place of a page the interface had, so an update does not take that page away; leave it out otherwise |

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
	"config": "webui",
	"sections": [
		{ "title": "Section", "fields": [
			{ "key": "my_option", "label": "Option", "type": "text" },
			{ "key": "my_switch", "label": "Switch", "type": "bool01" },
			{ "type": "action", "id": "my-button", "label": "Action",
			  "button": "Go",
			  "api": { "script": "my-action", "args": ["run"] },
			  "success": "Done" }
		] }
	]
}
```

| Field | Meaning |
|------|-----------|
| `label` | Category name in the sidebar (default: `name`) |
| `config` | Where `key` fields are kept: `webui` (default, the web interface's own `webui.conf`), `nhttpd` (the web server), or `settings/<section>` (one section of the box's settings: `GET /api/v1/settings/sections` names them, a key is a setting's id there) |
| `sections[].fields[]` | Field types `text`, `number`, `select`, `bool`, `bool01`, `password`, `info`, `action` |

`key` fields are persisted in the chosen config file. An `action`
field runs one of the extension's own scripts: `api.script` names it
as the manifest's `scripts` list does (`"scripts": ["my-action"]` for
the example), `api.args` is the list of its arguments, each reaching
the script as one and none beginning with a dash. It goes over
`POST /api/v1/webui/extensions/<folder>/scripts/<name>`; without a
`success` text the script's output is shown as toast feedback.

### Optional: own language pack (`languages/`)

An extension can localize itself — flat JSON dictionaries per language,
named by the language's code (`en`, `de`, the code
`GET /api/v1/webui/language` names):

```
my-extension/
├── manifest.json
├── index.html
└── languages/
    ├── en.json
    └── de.json
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
- **On your own pages**: at init `await webuiExtLangLoad('<folder>')`,
  then `webuiExtL('<folder>', 'page.title', 'Hello!')` — the third
  parameter is the English fallback, which also lives inline in the
  markup. Only the active language is fetched (no second fetch of
  English).
- **A page marked like the interface's own**: a page that marks its texts
  with `data-l`, `data-l-title`, `data-l-placeholder` or
  `data-l-aria-label` and asks `webuiL(key, 'English')` in its scripts
  loads `/js/webui-i18n.js` and calls `await webuiExtTexts('<folder>')`
  before it shows anything built from texts: the pack is laid beside the
  interface's texts and the marked texts are set again. A key the
  interface has stays the interface's, so give yours a prefix of your
  own.
- Always keep English + German in step; identical keysets.


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
webuiRegisterRemoteRenderer(function (opts) {
	return '<div class="webui-remote" id="' + opts.id +
		'" data-rc-onkey="' + opts.onKey + '">' +
		'<button type="button" class="webui-rc-btn" ' +
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
- Never set `WEBUI_CONFIG.remoteRenderer` directly —
  `webuiRegisterRemoteRenderer()` also re-renders instances that were
  built before the script loaded (open modals stay open). If two skins
  are active, the alphabetically later folder wins (with a console
  warning).

## 3. Write the page (`index.html`)

Key points:

- Embed the Neutrino-Web-UI header/nav:
  `<div class="webui-header-wrapper" data-page="ext-<folder>"></div>` +
  `<script src="/js/webui-header.js"></script>` — the script builds the
  header into that element on its own; with `slot: "nav"` the
  extension's nav entry is then marked as the page shown. Do not call
  `webuiInitHeader()` as well: it would put a second header on the
  page.
- Styling: link Pico CSS + `/css/webui.css` — use the ready-made classes
  (`webui-card`, `webui-form-row`, `webui-btn`).
- Apply the theme **before** those stylesheets, otherwise the page paints
  dark first and then jumps to the user's theme:
  ```html
  <script>
  try { document.documentElement.setAttribute('data-theme', localStorage.getItem('webui-theme') || 'dark'); } catch (e) {}
  </script>
  ```
- **Talk to the box only through `/api/v1/`**, with the helper from
  `<script src="/js/webui-v1.js"></script>`:
  `var info = await webuiV1('GET', '/system/info');` — it signs in
  again on a stale session and throws an `Error` carrying the refusal's
  `code`. Every route is described in the box's own document,
  `/api/openapi.json`, browsable in the API reference extension
  (`api-docs`) where the Swagger UI package is installed. The old `/api/v4/` and `/control/` routes are
  not part of this contract.

## 4. Enable

Settings → **Extensions**: the new extension shows up automatically in
the list → enable the switch, optionally change the slot → save →
reload the page.

Technically the switch lives in `webui.conf`
(`ext_<foldername>_enabled=1` or `0`, optionally
`ext_<foldername>_slot=...`). Where it says nothing, the manifest's
`default` decides: `"on"` is on, anything else off.

## 5. Install / update / uninstall (manager)

Settings → **Extensions** also manages the lifecycle
(backend: `/api/v1/webui/extensions`; the box itself never downloads
anything):

- **Install from the signed index**: "Load list" in the install
  dialog. The browser loads the index and its signature, uploads the
  index, and the box checks the signature
  (`POST /api/v1/webui/extension-index`) before anything of it is
  shown. Installing an entry loads its archive in the browser and
  uploads it with the index again; the box installs it only if the
  index offers that folder name with the archive's SHA-256.
- **Install an archive file**: upload a `tar.gz`; it is **not signed**,
  so the page asks first, and the box takes it only with that consent
  (`"unsigned": true`). An archive found elsewhere on the web is
  downloaded and installed this way.
- **Check for updates**: explicit button; the box checks the index as
  above and the page compares the versions with the installed ones.
  An update installs the index's archive under the same folder name;
  all config keys (enable/slot and extension settings) survive.
- **Uninstall**: row button with confirmation. Removes the extension
  folder with its scripts, and the copies an earlier version of the
  manager left in `<webroot>/scripts/`, each only where it is still the
  copy it installed; the `ext_<folder>_*` config keys are taken out.

### The signed index

The index lives at `ext_index_url` in `webui.conf` (default: the
`tuxbox-neutrino/yweb-extensions` repository), its signature beside it
as `<index-url>.sig`; both have to let a web page read them (GitHub
raw files do).

```json
{
	"format": 1,
	"extensions": [
		{ "id": "my-extension", "name": "My Extension", "version": "1.2.0",
		  "url": "https://github.com/…/releases/download/v1.2.0/my-extension.tar.gz",
		  "sha256": "<64 hex digits of that archive>",
		  "description": "Short subtitle" }
	]
}
```

`id` is the folder name (once per index), `url` an https address,
`version` letters, digits, dots, pluses and dashes; `name` and
`description` are optional. The signature is Ed25519 over the file's
bytes as published, in base64. The public key is part of the image, as
PEM in `<datadir>/tuxbox/neutrino/webui-index.pub`; the private key
stays with whoever publishes the index (a secret of the index
repository). An image without the key installs from no index.

```sh
openssl genpkey -algorithm ed25519 -out index-key.pem       # private, keep it secret
openssl pkey -in index-key.pem -pubout -out webui-index.pub  # public, into the image
sha256sum my-extension.tar.gz                                # the entry's sha256
openssl pkeyutl -sign -rawin -inkey index-key.pem -in index.json | base64 -w0 > index.json.sig
```

### Packaging contract (`<folder>.tar.gz`)

- The archive contains **exactly one top-level folder**; its name is
  the extension key (`^[a-zA-Z0-9][a-zA-Z0-9_-]*$`) and it must
  contain `manifest.json`.
- Only directories and regular files: no links, no devices, no name
  that is absolute or climbs with `..`; at most 16 MiB packed, 2000
  names and 64 MiB unpacked.
- It unpacks to `<webroot>/extensions/<folder>/`, files readable by
  all (0644), directories 0755.
- A release is published as `<folder>.tar.gz` holding `<folder>/`.
  An update keeps the folder name: the manager names the extension it
  updates (`"dir"` in `POST /api/v1/webui/extensions`), and the box
  refuses an archive that holds another one.
- The shell scripts the box runs for it are the ones `manifest.json`
  names under `scripts`: a list of at most 64 names of letters, digits,
  dots, dashes and underscores, not beginning with a dash or a dot;
  each has to be the regular file `<folder>/scripts/<name>.sh` in the
  archive, or the install is refused. These and no other file are made
  executable (0755). They stay in the folder and run from there over
  `POST /api/v1/webui/extensions/<folder>/scripts/<name>` (system level,
  the arguments as a JSON list, no shell, at most 10 seconds and 64 KiB
  of output); nothing is copied to `<webroot>/scripts/`, so a name only
  has to be unique within the extension.
- Scripts run **verbatim** — build-time placeholders like
  `%(PRIVATE_HTTPDDIR)` are *not* expanded (that only happens during
  the core's `make install`). Find the extension's own files from the
  script path instead:
  ```sh
  SCRIPTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
  EXTDIR=$(dirname -- "$SCRIPTDIR")
  ```
- Build command (used by the release workflows):

```sh
mkdir -p /tmp/stage/<folder> && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/<folder>/ && \
tar -czf <folder>.tar.gz -C /tmp/stage <folder>
```

## Rules

- No dependencies on core internals — only the documented `/api/v1/`
  surface, the `webui-*` classes and helpers of the pages, and the
  script route above for actions.
- The folder name is the stable key (config keys, URL
  `/extensions/<folder>/...`) — do not change it after publication.
- Extensions are **off** by default (opt-in by the user); only one that
  takes the place of a page the interface had asks for `"default": "on"`.

## Official extensions

The official extensions are being rebuilt against `/api/v1`; each
will get a versioned repository of its own and an entry in an update
index the manager reads. The earlier repositories
`github.com/tuxbox-neutrino/yweb-extension-<name>` (installer,
m3u-converter, camds, infoicons, hallo-yweb) and their index
`yweb-extensions` were written for the old script contract; their
extensions do not work with this version.
