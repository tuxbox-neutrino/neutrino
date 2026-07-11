# yweb-extension-m3u-converter

*Deutsche Fassung: [README.de.md](README.de.md)*

**m3u Converter** — a [yWeb extension](https://github.com/tuxbox-neutrino/yweb-extensions)
for Tuxbox-Neutrino: converts m3u playlists (`#EXTINF`) into Neutrino's
WebTV XML format (`webtv_usr.xml`). The conversion runs entirely in the
browser; the result can be downloaded or saved straight to the box's
config directory (`/api/v4/file` upload).

## Install

Settings → **Extensions** → *Install*: upload the release tarball
(`m3u-converter.tar.gz` from the
[latest release](../../releases/latest)) or paste its download URL.
Then enable the extension and reload — the tile appears under Tools
(`slot: tools`). After converting, add the generated file under
Settings → WebTV sources.

## Layout

```
m3u-converter/              ← tarball top-level folder == extension dir
├── manifest.json           # name, version (canonical), slot, entry
├── m3u-convert.html        # page incl. converter logic (client-side)
├── languages/
│   ├── English.json        # flat key/value, incl. manifest.name/.description
│   └── Deutsch.json
├── README.md / README.de.md
├── LICENSE                 # GPL-2.0 (same as Neutrino)
└── .github/workflows/      # version-bump + version-line
```

The page uses only the documented `/api/v4/` surface (`paths`, `file`
upload) and references core assets without `?v=` cache-bust parameters.
Texts follow the extension i18n pattern: English inline as fallback,
translations from `languages/<Language>.json` via `ywebExtL`.

## Versioning and releases

Semantic `v<major>.<minor>.<patch>`; the canonical version lives in
`manifest.json`. CI bumps the patch on every push to `main` (patch =
commits since the `v<maj>.<min>` anchor tag), tags the release and
attaches the tarball as
`releases/latest/download/m3u-converter.tar.gz` — the stable URL used
by the extension manager and the
[yweb-extensions index](https://github.com/tuxbox-neutrino/yweb-extensions).
Start a new minor line with the `version-line` workflow.

## Packaging contract (summary)

The release tarball contains exactly one top-level folder
(`m3u-converter/`) with `manifest.json` inside; it unpacks to
`<webroot>/extensions/`. Full contract: see the
[yweb-extensions](https://github.com/tuxbox-neutrino/yweb-extensions)
meta repository.
