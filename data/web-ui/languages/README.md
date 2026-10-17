# Neutrino-Web-UI languages — how to add or complete a translation

This directory holds the translation files for the Neutrino-Web-UI.
Each file is one language; the **file name is the language name** shown in
the header language switcher (e.g. `Deutsch`, `English`, `Français`).

## File format

Plain `key=value`, one entry per line, UTF-8:

```
# comment lines start with a hash
yw.nav.dashboard=Dashboard
submit=Submit
```

- Left of `=` is the **key** — never change or translate it.
- Right of `=` is the **value** — translate this.
- **No double quotes (`"`) in values.** Values are embedded verbatim into
  the JavaScript dictionary (`Y_Lang.yjs` → `window.WEBUI_L`) and into the
  switcher's `<option>` list; a `"` would break them.
- A missing key falls back to **English** (`HTTPD_DEFAULT_LANGUAGE`), so a
  partial translation never shows a broken page — untranslated strings
  simply appear in English.

## Two key namespaces

- **Legacy keys** (`submit`, `action`, `global.*`, …) — the classic pages taken over from the
  yWeb interface.
- **Modern keys** (`yw.*`) — the current, reworked UI: navigation,
  settings registry, dashboard, wizard, directory picker, etc.
  `English` is the reference and always has the complete set.

## When does a language appear in the switcher?

The switcher lists a language **only once its file contains the modern
`yw.*` keys** — the generator probes for the sentinel key
`yw.nav.dashboard` (see `func_get_languages_as_dropdown` in
`src/nhttpd/yhttpd_mods/mod_yparser.cpp`). This keeps legacy-only files
(which would render the whole modern UI in English) out of the menu. Add
the modern keys and the language shows up automatically. The currently
active language is always listed, even if incomplete.

## Add a new language

1. Copy the reference file to the new language name:

   ```
   cp English Français
   ```

2. Translate the values in `Français`. Keep every key unchanged. At
   minimum translate the `yw.*` block so the language appears in the
   switcher (English fallback covers whatever you leave).

3. Ship it: add the file to `install_DATA` in this directory's
   `Makefile.am` (only complete languages are installed):

   ```
   install_DATA = \
       Deutsch \
       English \
       Français
   ```

4. Rebuild and reinstall Neutrino (or, in a dev tree, copy the file into
   the served webroot `.../httpd/languages/` and restart neutrino so the
   generator re-reads the directory).

## Complete an existing legacy-only language

`Czech`, `Polski`, `Portuguese` and `Slovak` currently have the legacy
keys but **none** of the modern `yw.*` keys, so they are hidden from the
switcher. To bring one back:

1. Append the `yw.*` block from `English` to the file and translate it
   (a quick way to list the missing keys):

   ```
   comm -23 <(grep '^yw\.' English | sort) <(grep '^yw\.' Czech | sort)
   ```

2. Add the file to `install_DATA` in `Makefile.am`.
3. Rebuild/reinstall (or dev-copy + restart) as above.

## Runtime notes

- The active language is stored as `Language.selected` in `nhttpd.conf`
  and switched at runtime via the header globe button or the setup wizard
  (→ `PUT /api/v1/webui/language`, by the language's code), which has the
  server read its file again. `GET /api/v1/webui/language` lists the
  languages whose file carries the `yw.*` keys, and `GET /api/v1/session`
  says the active one's code.
- No client-side changes are needed: `Y_Lang.yjs` and `webuiL()` read the
  same keys, and the header button shows the active language's code.
