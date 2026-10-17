# Neutrino-Web-UI languages — how to add or complete a translation

This directory holds the texts of the Neutrino-Web-UI in two kinds of
files:

- **`<code>.json`** (`en.json`, `de.json`): the interface's own texts,
  which the pages read in the browser (`js/webui-i18n.js`). A language is
  named by its code everywhere the pages and `/api/v1` meet it.
- **Files named after the language** (`English`, `Deutsch`, …): the texts
  the server's template engine puts into the old pages. `nhttpd.conf`
  keeps the active language under this name (`Language.selected`).

## `<code>.json`

One flat JSON object, UTF-8, the key on the left, the text on the right:

```json
{
	"nav.dashboard": "Dashboard",
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
which names the old pages' file and what the language calls itself; `en`,
`de`, `cs`, `pl`, `pt` and `sk` have one.

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
       Deutsch \
       English \
       de.json \
       en.json \
       fr.json
   ```

4. Rebuild and reinstall Neutrino (or, in a dev tree, copy the file into
   the served webroot `.../httpd/languages/`).

## The old pages' files

Plain `key=value`, one entry per line, UTF-8, `#` starting a comment.
They hold the texts of the classic pages taken over from the yWeb
interface (`submit`, `action`, `global.*`, …), which the server's
template engine puts in as it sends those pages; a missing key falls
back to English (`HTTPD_DEFAULT_LANGUAGE`). The interface's own texts
are not in them.

`Czech`, `Polski`, `Portuguese` and `Slovak` have only legacy keys and
no `<code>.json`, so the switcher does not offer them.

## Runtime notes

- The active language is stored as `Language.selected` in `nhttpd.conf`
  and switched at runtime via the header globe button or the setup wizard
  (→ `PUT /api/v1/webui/language`, by the language's code), which has the
  server read its file again. `GET /api/v1/session` says the active
  language's code to anybody, so the sign-in page is shown in it.
- A page reads `en.json` and the active language's file when it loads;
  the header button shows the active language's code.
- Before signing in, only the pages' own files go out: a `<code>.json`
  put into the override directory takes effect once somebody signed in,
  and the sign-in page shows English until then.
