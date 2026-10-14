# Neutrino-Web-UI-Erweiterung in 10 Minuten

*English version: [README.md](README.md)*

Die Neutrino-Web-UI lädt Erweiterungen automatisch aus diesem Verzeichnis.
Eine Erweiterung ist **ein Ordner mit einem Manifest** — kein Patchen von
Kernseiten, kein Build nötig.

## 1. Ordner anlegen

Auf der Box (Webroot, z.B. `/share/tuxbox/neutrino/httpd/`):

```
extensions/
└── meine-erweiterung/
    ├── manifest.json
    └── index.html
```

## 2. Manifest schreiben (`manifest.json`)

```json
{
	"name": "Meine Erweiterung",
	"version": "1.0",
	"slot": "tools",
	"entry": "index.html",
	"description": "Kurze Beschreibung fuer die Kachel"
}
```

| Feld | Bedeutung |
|------|-----------|
| `name` | Anzeigename (Nav-Link, Kachel, Verwaltung) |
| `version` | frei, wird in der Verwaltung angezeigt |
| `slot` | Andockpunkt: `nav` (Hauptnavigation), `tools` (Kachel im Tools-Grid), `dashboard` (Karte), `page` (nur Direktaufruf) |
| `entry` | Einstiegsdatei relativ zum Erweiterungsordner |
| `entry_js` | optional: Laufzeit-Skript, wird auf jeder modernen Seite geladen (siehe unten) |
| `description` | optionaler Untertitel |
| `settings` | optional: eigene Kategorie unter Einstellungen (siehe unten) |

Mindestens eines von `entry`, `entry_js` oder `settings.sections` muss
vorhanden sein, sonst wird das Manifest ignoriert.

### Optional: eigene Einstellungs-Kategorie (`settings`)

Eine aktivierte Erweiterung mit `settings`-Block bekommt eine eigene
Kategorie in der Einstellungs-Seitenleiste. Die Felder sind deklarativ
(gleiches Schema wie die Kern-Registry) und werden generisch geladen,
verglichen und gespeichert — kein eigener JS-Code nötig:

```json
"settings": {
	"label": "Meine Erweiterung",
	"config": "yweb",
	"sections": [
		{ "title": "Abschnitt", "fields": [
			{ "key": "meine_option", "label": "Option", "type": "text" },
			{ "key": "mein_schalter", "label": "Schalter", "type": "bool01" },
			{ "type": "action", "id": "mein-knopf", "label": "Aktion",
			  "button": "Los", "api": { "endpoint": "exec",
			  "params": "MeinScript&aktion", "raw": true },
			  "success": "Ausgefuehrt" }
		] }
	]
}
```

| Feld | Bedeutung |
|------|-----------|
| `label` | Kategoriename in der Seitenleiste (Default: `name`) |
| `config` | Ziel-Config fuer `key`-Felder: `yweb` (Default), `neutrino`, `nhttpd` |
| `sections[].fields[]` | Feldtypen `text`, `number`, `select`, `bool`, `bool01`, `password`, `info`, `action` |

`key`-Felder werden in der gewählten Config-Datei persistiert;
`action`-Felder feuern einen v4-Call (typisch `exec` auf ein Script);
ohne `success`-Text erscheint die Script-Ausgabe als Toast-Feedback.

### Optional: eigenes Sprachpaket (`languages/`)

Eine Erweiterung kann sich selbst lokalisieren — flache JSON-Wörterbücher
pro Sprache, benannt wie die Kern-Sprachdateien:

```
meine-erweiterung/
├── manifest.json
├── index.html
└── languages/
    ├── English.json
    └── Deutsch.json
```

```json
{
	"manifest.name": "Meine Erweiterung",
	"manifest.description": "Kachel-Untertitel",
	"settings.greeting": "Begrüßungstext",
	"page.title": "Hallo!"
}
```

- **Reserved Keys** `manifest.name` / `manifest.description` übersetzen
  Name und Untertitel überall dort, wo die Erweiterung andockt
  (Nav, Kachel, Dashboard, Verwaltung).
- **Manifest-Strings** (Settings-`label`, Section-`title`, Feld-`label`,
  `button`, `success`) werden übersetzt, wenn ihr Wert mit `L:` beginnt:
  `"label": "L:settings.greeting"` → Key `settings.greeting` aus dem
  Sprachpaket. Werte ohne `L:` bleiben Literale.
- **Auf eigenen Seiten**: beim Init `await ywebExtLangLoad('<ordner>')`,
  danach `ywebExtL('<ordner>', 'page.title', 'Hello!')` — der dritte
  Parameter ist der englische Fallback, der auch inline im Markup steht.
  Es wird nur die aktive Sprache geladen (kein Zweit-Fetch auf Englisch).
- Englisch + Deutsch immer paarig pflegen; gleiche Keysets.


### Optional: Laufzeit-Skript (`entry_js`)

Für jede *aktivierte* Erweiterung, deren Manifest eine
`entry_js`-Datei nennt, lädt jede moderne Seite
`<script src="/extensions/<ordner>/<entry_js>?v=<version>">` —
unabhängig vom `slot` (der Slot regelt nur die UI-Verlinkung; ein
Manifest *nur* mit `entry_js` ist gültig). Die Skripte werden in
alphabetischer Ordner-Reihenfolge mit `async=false` eingefügt, die
Ausführungsreihenfolge ist also deterministisch. Es gibt **keine
Timing-Garantie** relativ zum Seiten-Code: Hooks registrieren, nicht
pollen.

Der wichtigste Hook ist der **Fernbedienungs-Skin-Renderer** — ein
hardware-spezifisches Layout für die Bildschirm-Fernbedienung:

```js
// skin.js — Manifest: { "name": "…", "version": "1.0", "entry_js": "skin.js" }
ywebRegisterRemoteRenderer(function (opts) {
	return '<div class="yweb-remote" id="' + opts.id +
		'" data-rc-onkey="' + opts.onKey + '">' +
		'<button type="button" class="yweb-rc-btn" ' +
			'onclick="' + opts.onKey + "('KEY_OK')\">OK</button>" +
		'</div>';
});
```

Vertrag:

- Genau ein Wurzelelement mit `id=opts.id` und
  `data-rc-onkey=opts.onKey` zurückgeben; `opts.onKey` ist der **Name**
  einer globalen Funktion, die einen `KEY_*`-String entgegennimmt.
- Gültige Key-Namen stammen aus der generierten `rcsim.h`-Tabelle
  (Quelle: `src/driver/rcinput.h`); Achtung: **`KEY_TIME`** (nicht
  `KEY_TIMER`) und `KEY_FAVORITES`.
- Echte `<button type="button">`-Elemente verwenden; reine Icon-Tasten
  brauchen `aria-label`, Inline-SVGs `aria-hidden="true"`. Tasten
  melden sich mit `data-rc-repeat="1"` + `data-rc-key="KEY_…"` für
  Auto-Repeat und Press-Feedback an.
- Nie `YWEB_CONFIG.remoteRenderer` direkt setzen —
  `ywebRegisterRemoteRenderer()` rendert auch Instanzen neu, die vor
  dem Skript-Load gebaut wurden (offene Modals bleiben offen). Sind
  zwei Skins aktiv, gewinnt der alphabetisch spätere Ordner (mit
  console-Warnung).

## 3. Seite schreiben (`index.html`)

Kernpunkte:

- Header/Nav der Neutrino-Web-UI einbinden:
  `<div class="yweb-header-wrapper" data-page="tools"></div>` +
  `<script src="/yweb-header.js"></script>` + `ywebInitHeader('tools');`
- Styling: Pico CSS + `/css/yweb.css` verlinken — fertige Klassen
  (`yweb-card`, `yweb-form-row`, `yweb-btn`) nutzen.
- Theme **vor** diesen Stylesheets setzen, sonst malt die Seite erst
  dunkel und springt dann auf das Theme des Benutzers:
  ```html
  <script>
  try { document.documentElement.setAttribute('data-theme', localStorage.getItem('yweb-theme') || 'dark'); } catch (e) {}
  </script>
  ```
- **Daten nur über `/api/v4/`** holen, am einfachsten mit dem Helfer:
  `var sys = await ywebApi('sysinfo');`
  Referenz aller Endpoints: Doxygen-Seite „Neutrino Web-API v4"
  (`doc/nhttpd/apiv4.dox` im Quellbaum).

## 4. Aktivieren

Einstellungen → **Erweiterungen**: die neue Erweiterung erscheint
automatisch in der Liste → Schalter aktivieren, optional den Slot
ändern → Speichern → Seite neu laden.

Technisch: die Schaltung liegt in `yWeb.conf`
(`ext_<ordnername>_enabled=1`, optional `ext_<ordnername>_slot=...`).

## 5. Installieren / Aktualisieren / Deinstallieren (Verwaltung)

Einstellungen → **Erweiterungen** verwaltet auch den Lebenszyklus
(Backend: `/api/v1/webui/extensions`; die Box selbst lädt nichts
herunter):

- **Installieren**: `tar.gz`-Archiv hochladen oder eine Download-URL
  einfügen (z.B. ein GitHub-Release-Asset). Bei einer URL lädt der
  Browser das Archiv und lädt es wie eine Datei hoch; die Adresse muss
  das Lesen aus einer Webseite erlauben (GitHub tut das), sonst das
  Archiv herunterladen und als Datei installieren. Ein Update ist
  einfach die erneute Installation derselben Erweiterung — der Ordner
  wird ersetzt, sobald der neue ganz entpackt ist, alle Config-Keys
  (Enable/Slot und Erweiterungs-Einstellungen) überleben.
- **Updates prüfen**: expliziter Knopf; der Browser vergleicht die
  installierten Versionen gegen eine gehostete `index.json`
  (Default: das Repository `tuxbox-neutrino/yweb-extensions`,
  überschreibbar per `yWeb.conf`-Key `ext_index_url`). Verfügbare
  Updates zeigen Badge und Update-Knopf in der Zeile.
- **Deinstallieren**: Zeilen-Knopf mit Bestätigung. Entfernt den
  Erweiterungsordner und die mitgelieferten Skripte, jedes nur, solange
  es noch die installierte Kopie ist; die `ext_<ordner>_*`-Config-Keys
  werden entfernt.

### Packaging-Vertrag (`<ordner>.tar.gz`)

- Das Archiv enthält **genau einen Top-Level-Ordner**; sein Name ist
  der Erweiterungs-Schlüssel (`^[a-zA-Z0-9][a-zA-Z0-9_-]*$`) und er
  muss `manifest.json` enthalten.
- Nur Verzeichnisse und normale Dateien: keine Links, keine Geräte,
  kein absoluter Name und keiner, der mit `..` aufsteigt; höchstens
  16 MiB gepackt, 2000 Namen und 64 MiB entpackt.
- Es entpackt nach `<webroot>/extensions/<ordner>/`, Dateien für alle
  lesbar (0644), Verzeichnisse 0755.
- Shell-Skripte in `<ordner>/scripts/*.sh` (Buchstaben, Ziffern,
  Punkte, Bindestriche, Unterstriche) werden bei der Installation nach
  `<webroot>/scripts/` kopiert, ausführbar (für `exec`-/`action`-Felder
  verfügbar), und bei der Deinstallation entfernt. Ein Skript darf
  keinen Namen tragen, den die Seiten schon haben — ein eigenes der Box
  oder das einer anderen Erweiterung —, also eindeutig benennen; ersetzt
  wird nur die unveränderte frühere Kopie derselben Erweiterung.
- Skripte werden **unverändert** kopiert — Build-Zeit-Platzhalter wie
  `%(PRIVATE_HTTPDDIR)` werden *nicht* expandiert (das passiert nur
  beim `make install` des Kerns). Geteilte Libraries stattdessen über
  den Skript-Pfad finden:
  ```sh
  SCRIPTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
  [ -r "$SCRIPTDIR/_Y_Globals.sh" ] && . "$SCRIPTDIR/_Y_Globals.sh"
  ```
- Bau-Kommando (nutzen auch die Release-Workflows):

```sh
mkdir -p /tmp/stage/<ordner> && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/<ordner>/ && \
tar -czf <ordner>.tar.gz -C /tmp/stage <ordner>
```

## Regeln

- Keine Abhängigkeiten auf Kern-Interna — nur die dokumentierte
  `/api/v4/`-Fläche (plus `exec`-Vertrag für Script-Aktionen).
- Der Ordnername ist der stabile Schlüssel (Config-Keys, URL
  `/extensions/<ordner>/...`) — nach der Veröffentlichung nicht ändern.
- Erweiterungen sind standardmäßig **aus** (Opt-in durch den Nutzer).

## Offizielle Erweiterungen

Die offiziellen Erweiterungen werden gegen `/api/v1` neu gebaut; jede
bekommt ein eigenes versioniertes Repository und einen Eintrag in einem
Update-Index, den die Verwaltung liest. Die bisherigen Repositories
`github.com/tuxbox-neutrino/yweb-extension-<name>` (installer,
m3u-converter, camds, infoicons, hallo-yweb) samt ihrem Index
`yweb-extensions` gehören zum alten Skript-Vertrag; ihre Erweiterungen
laufen mit dieser Version nicht.
