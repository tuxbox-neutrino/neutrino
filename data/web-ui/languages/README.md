# Neutrino-Web-UI languages — how to add or complete a translation

This directory holds the texts of the Neutrino-Web-UI, one file per
language named by its code (`en.json`, `de.json`), which the pages read
in the browser (`js/webui-i18n.js`). A language is named by its code
everywhere the pages and `/api/v1` meet it; `nhttpd.conf` keeps the
active one under the name the table below gives it
(`Language.selected`).

## `<code>.json`

One flat JSON object, UTF-8, the key on the left, the text on the right:

```json
{
	"nav.sub_overview": "Overview",
	"login.title": "Sign in"
}
```

- Never change or translate a key; translate the text.
- Texts are set as text, never as markup: write `<` and `&` as they are.
- A key missing in a translation shows in English: the pages lay the
  language's file over `en.json`, which is the reference and always has
  every key.

## When does a language appear in the switcher?

Once `<code>.json` is installed beside the others: `GET /api/v1/webui/language`
lists every language the directory holds the interface's texts for. The
code needs a row in the table in `src/httpd/webconfig.cpp` (`kLanguages`),
which names the name `nhttpd.conf` keeps it under and what the language
calls itself; `en`, `de`, `cs`, `pl`, `pt` and `sk` have one.

## Add a new language

1. Copy the reference and translate the texts:

   ```
   cp en.json fr.json
   ```

2. Add a row for the code to `kLanguages` in `src/httpd/webconfig.cpp`
   if it has none.

3. Ship it: add the file to `install_DATA` in this directory's
   `Makefile.am`:

   ```
   install_DATA = \
       de.json \
       en.json \
       fr.json
   ```

4. Rebuild and reinstall Neutrino (or, in a dev tree, copy the file into
   the served webroot `.../httpd/languages/`).

## Runtime notes

- The active language is stored as `Language.selected` in `nhttpd.conf`
  and switched at runtime via the header globe button or the setup wizard
  (→ `PUT /api/v1/webui/language`, by the language's code).
  `GET /api/v1/session` says the active
  language's code to anybody, so the sign-in page is shown in it.
- A page reads `en.json` and the active language's file when it loads;
  the header button shows the active language's code.
- Before signing in, only the pages' own files go out: a `<code>.json`
  put into the override directory takes effect once somebody signed in,
  and the sign-in page shows English until then.
