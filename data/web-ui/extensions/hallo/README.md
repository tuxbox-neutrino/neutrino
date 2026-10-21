# Hello – an example to start a Neutrino-Web-UI extension from

`hallo` is a complete, small extension. Copy it, rename it and change it;
everything an extension can carry is here once:

| File | What it shows |
|------|---------------|
| `manifest.json` | name, version, the slot (a tile under Tools), the entry page, an icon, the script the box may run, and a settings category |
| `index.html` | a page with the header of the interface, data read through `/api/v1`, a setting of its own and its own texts |
| `languages/en.json`, `languages/de.json` | the extension's texts in English and German, with the same keys |
| `scripts/greet.sh` | a script the box runs for it, started from the settings |
| `icon.svg` | a single-colour icon for the tile and the nav entry |
| `LICENSE` | the licence (GPL-2.0) |

The extension is off until it is switched on under Settings →
Extensions. The contract it follows is in the core's
`extensions/README.md`.

## Start your own from it

1. Copy the folder and give it the name of your extension, for example
   `my-extension`: letters, digits, `_` and `-`. The folder name is the
   extension's key and does not change once it is published.
2. In `index.html` put that name in `data-page="ext-my-extension"` and in
   `webuiExtTexts('my-extension')`. Mark every visible word with a
   `data-l` key (`data-l-title`, `data-l-placeholder` and
   `data-l-aria-label` for attributes) and ask `webuiL(key, 'English')`
   in scripts; a read of the box that fails goes into
   `webuiStateFor(e, {retry: …})`, which says why in the page's language.
3. In `manifest.json` change `name`, `description` and `version`, and the
   keys of your settings (a prefix of your own, like `my_extension_…`,
   keeps them apart from everyone else's in `webui.conf`).
4. Keep `languages/en.json` and `languages/de.json` in step.

## The script

`scripts/greet.sh` is named in the manifest under `scripts` (without
`.sh`), so the box runs it over
`POST /api/v1/webui/extensions/hallo/scripts/greet` with the arguments
as a list, without a shell, for at most ten seconds. The settings'
action does that; what the script prints shows as the answer. A script
the manifest does not name does not run.

## Release

A release is an archive named after the folder that holds exactly that
folder:

```sh
mkdir -p /tmp/stage/hallo && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/hallo/ && \
tar -czf hallo.tar.gz -C /tmp/stage hallo
```

Settings → Extensions installs it from a file, or from the signed index
once the index names it with the archive's SHA-256.
