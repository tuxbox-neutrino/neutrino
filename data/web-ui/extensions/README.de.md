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
| `icon` | optional: ein einfarbiges SVG im Ordner (z.B. `icon.svg`), in der Textfarbe auf dem Nav-Eintrag und der Kachel gezeichnet; ohne es ein Stern |
| `settings` | optional: eigene Kategorie unter Einstellungen (siehe unten) |
| `scripts` | optional: die Shell-Skripte, die die Box für sie ausführt, mit Namen ohne `.sh` (siehe Packaging-Vertrag) |
| `default` | optional: `"on"` schaltet die Erweiterung ein, solange die Einstellungen nichts über sie sagen; der eigene Schalter des Nutzers geht vor. Gedacht für eine Erweiterung, die an die Stelle einer Seite der Oberfläche tritt, damit ein Update diese Seite nicht wegnimmt; sonst weglassen |

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
	"config": "webui",
	"sections": [
		{ "title": "Abschnitt", "fields": [
			{ "key": "meine_option", "label": "Option", "type": "text" },
			{ "key": "mein_schalter", "label": "Schalter", "type": "bool01" },
			{ "type": "action", "id": "mein-knopf", "label": "Aktion",
			  "button": "Los",
			  "api": { "script": "meine-aktion", "args": ["start"] },
			  "success": "Ausgefuehrt" }
		] }
	]
}
```

| Feld | Bedeutung |
|------|-----------|
| `label` | Kategoriename in der Seitenleiste (Default: `name`) |
| `config` | Wo `key`-Felder liegen: `webui` (Default, die eigene `webui.conf` der Weboberfläche), `nhttpd` (der Webserver) oder `settings/<abschnitt>` (ein Abschnitt der Box-Einstellungen: `GET /api/v1/settings/sections` nennt sie, ein Key ist dort die id einer Einstellung) |
| `sections[].fields[]` | Feldtypen `text`, `number`, `select`, `bool`, `bool01`, `password`, `info`, `action` |

`key`-Felder werden in der gewählten Config-Datei persistiert. Ein
`action`-Feld führt eines der eigenen Skripte der Erweiterung aus:
`api.script` nennt es so wie die `scripts`-Liste des Manifests
(`"scripts": ["meine-aktion"]` für das Beispiel), `api.args` ist die
Liste seiner Argumente, jedes kommt als eines beim Skript an, keines
darf mit einem Bindestrich beginnen. Der Aufruf geht über
`POST /api/v1/webui/extensions/<ordner>/scripts/<name>`; ohne
`success`-Text erscheint die Ausgabe des Skripts als Toast-Feedback.

### Optional: eigenes Sprachpaket (`languages/`)

Eine Erweiterung kann sich selbst lokalisieren — flache JSON-Wörterbücher
pro Sprache, benannt nach dem Code der Sprache (`en`, `de`, der Code, den
`GET /api/v1/webui/language` nennt):

```
meine-erweiterung/
├── manifest.json
├── index.html
└── languages/
    ├── en.json
    └── de.json
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
- **Auf eigenen Seiten**: beim Init `await webuiExtLangLoad('<ordner>')`,
  danach `webuiExtL('<ordner>', 'page.title', 'Hello!')` — der dritte
  Parameter ist der englische Fallback, der auch inline im Markup steht.
  Es wird nur die aktive Sprache geladen (kein Zweit-Fetch auf Englisch).
- **Eine Seite, markiert wie die der Oberfläche**: Eine Seite, die ihre
  Texte mit `data-l`, `data-l-title`, `data-l-placeholder` oder
  `data-l-aria-label` markiert und in ihren Skripten
  `webuiL(key, 'English')` fragt, lädt `/js/webui-i18n.js` und ruft
  `await webuiExtTexts('<ordner>')` auf, bevor sie etwas aus Texten
  zeigt: Das Paket kommt zu den Texten der Oberfläche, und die
  markierten Texte werden neu gesetzt. Ein Schlüssel, den die Oberfläche
  hat, bleibt ihrer; deshalb eigene Schlüssel mit einem eigenen Präfix.
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
webuiRegisterRemoteRenderer(function (opts) {
	return '<div class="webui-remote" id="' + opts.id +
		'" data-rc-onkey="' + opts.onKey + '">' +
		'<button type="button" class="webui-rc-btn" ' +
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
- Nie `WEBUI_CONFIG.remoteRenderer` direkt setzen —
  `webuiRegisterRemoteRenderer()` rendert auch Instanzen neu, die vor
  dem Skript-Load gebaut wurden (offene Modals bleiben offen). Sind
  zwei Skins aktiv, gewinnt der alphabetisch spätere Ordner (mit
  console-Warnung).

## 3. Seite schreiben (`index.html`)

Kernpunkte:

- Header/Nav der Neutrino-Web-UI einbinden:
  `<div class="webui-header-wrapper" data-page="ext-<ordner>"></div>` +
  `<script src="/js/webui-header.js"></script>` — das Skript baut den
  Header von selbst in dieses Element; mit `slot: "nav"` ist der
  Nav-Eintrag der Erweiterung dann als gezeigte Seite markiert, mit
  `slot: "tools"` der Eintrag Werkzeuge.
  `webuiInitHeader()` nicht zusätzlich aufrufen: Es setzte einen zweiten
  Header auf die Seite.
- Styling: Pico CSS + `/css/webui.css` verlinken — fertige Klassen
  (`webui-card`, `webui-form-row`, `webui-btn`) nutzen.
- Theme **vor** diesen Stylesheets setzen, sonst malt die Seite erst in
  einem Theme und springt dann ins andere. Hell oder dunkel folgt dem
  Gerät, bis jemand mit dem Schalter im Kopf eines wählt; die Wahl steht
  dann unter `webui-mode` (von den Schlüsseln davor zählt nur ein
  gespeichertes `light`):
  ```html
  <script>
  try { var m = localStorage.getItem('webui-mode'); if (m !== 'light' && m !== 'dark') m = (localStorage.getItem('webui-theme') || localStorage.getItem('yweb-theme')) === 'light' || matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark'; document.documentElement.setAttribute('data-theme', m); } catch (e) {}
  </script>
  ```
- **Mit der Box nur über `/api/v1/`** sprechen, mit dem Helfer aus
  `<script src="/js/webui-v1.js"></script>`:
  `var info = await webuiV1('GET', '/system/info');` — er meldet sich
  bei einer abgelaufenen Sitzung neu an und wirft einen `Error` mit dem
  `code` der Ablehnung. Jede Route beschreibt das Dokument der Box
  selbst, `/api/openapi.json`, durchsuchbar in der Erweiterung
  API-Referenz (`api-docs`), wo das Paket Swagger UI installiert ist. Die alten Routen `/api/v4/`
  und `/control/` gehören nicht zu diesem Vertrag.

## 4. Aktivieren

Einstellungen → **Erweiterungen**: die neue Erweiterung erscheint
automatisch in der Liste → Schalter aktivieren, optional den Slot
ändern → Speichern → Seite neu laden.

Technisch: die Schaltung liegt in `webui.conf`
(`ext_<ordnername>_enabled=1` oder `0`, optional
`ext_<ordnername>_slot=...`). Steht dort nichts, entscheidet `default`
im Manifest: `"on"` ist an, alles andere aus.

## 5. Installieren / Aktualisieren / Deinstallieren (Verwaltung)

Einstellungen → **Erweiterungen** verwaltet auch den Lebenszyklus
(Backend: `/api/v1/webui/extensions`; die Box selbst lädt nichts
herunter):

- **Aus dem signierten Index installieren**: „Liste laden" im
  Installationsdialog. Der Browser lädt Index und Signatur, lädt den
  Index hoch, und die Box prüft die Signatur
  (`POST /api/v1/webui/extension-index`), bevor etwas davon angezeigt
  wird. Ein Eintrag wird installiert, indem der Browser sein Archiv
  lädt und mit dem Index erneut hochlädt; die Box installiert es nur,
  wenn der Index diesen Ordnernamen mit der SHA-256 des Archivs
  anbietet.
- **Eine Archivdatei installieren**: ein `tar.gz` hochladen; es ist
  **nicht signiert**, die Seite fragt deshalb vorher, und die Box nimmt
  es nur mit dieser Zustimmung (`"unsigned": true`). Ein Archiv von
  anderswo im Netz wird heruntergeladen und so installiert.
- **Updates prüfen**: expliziter Knopf; die Box prüft den Index wie
  oben, und die Seite vergleicht die Versionen mit den installierten.
  Ein Update installiert das Archiv aus dem Index unter demselben
  Ordnernamen; alle Config-Keys (Enable/Slot und
  Erweiterungs-Einstellungen) überleben.
- **Deinstallieren**: Zeilen-Knopf mit Bestätigung. Entfernt den
  Erweiterungsordner mit seinen Skripten und die Kopien, die eine
  frühere Version der Verwaltung in `<webroot>/scripts/` abgelegt hat,
  jede nur, solange sie noch die installierte Kopie ist; die
  `ext_<ordner>_*`-Config-Keys werden entfernt.

### Der signierte Index

Der Index liegt unter `ext_index_url` in `webui.conf` (Default: das
Repository `tuxbox-neutrino/yweb-extensions`), seine Signatur daneben
als `<index-url>.sig`; beide müssen das Lesen aus einer Webseite
erlauben (Raw-Dateien auf GitHub tun das).

```json
{
	"format": 1,
	"extensions": [
		{ "id": "meine-erweiterung", "name": "Meine Erweiterung", "version": "1.2.0",
		  "url": "https://github.com/…/releases/download/v1.2.0/meine-erweiterung.tar.gz",
		  "sha256": "<64 Hex-Ziffern dieses Archivs>",
		  "description": "Kurzer Untertitel" }
	]
}
```

`id` ist der Ordnername (einmal je Index), `url` eine https-Adresse,
`version` Buchstaben, Ziffern, Punkte, Plus- und Bindestriche; `name`
und `description` sind optional. Die Signatur ist Ed25519 über die
Bytes der Datei, wie sie veröffentlicht ist, in base64. Der öffentliche
Schlüssel gehört zum Image, als PEM in
`<datadir>/tuxbox/neutrino/webui-index.pub`; der private bleibt bei
dem, der den Index veröffentlicht (ein Secret des Index-Repositorys).
Ein Image ohne den Schlüssel installiert aus keinem Index.

```sh
openssl genpkey -algorithm ed25519 -out index-key.pem       # privat, geheim halten
openssl pkey -in index-key.pem -pubout -out webui-index.pub  # öffentlich, ins Image
sha256sum meine-erweiterung.tar.gz                           # sha256 des Eintrags
openssl pkeyutl -sign -rawin -inkey index-key.pem -in index.json | base64 -w0 > index.json.sig
```

### Packaging-Vertrag (`<ordner>.tar.gz`)

- Das Archiv enthält **genau einen Top-Level-Ordner**; sein Name ist
  der Erweiterungs-Schlüssel (`^[a-zA-Z0-9][a-zA-Z0-9_-]*$`) und er
  muss `manifest.json` enthalten.
- Nur Verzeichnisse und normale Dateien: keine Links, keine Geräte,
  kein absoluter Name und keiner, der mit `..` aufsteigt; höchstens
  16 MiB gepackt, 2000 Namen und 64 MiB entpackt.
- Es entpackt nach `<webroot>/extensions/<ordner>/`, Dateien für alle
  lesbar (0644), Verzeichnisse 0755.
- Ein Release erscheint als `<ordner>.tar.gz` mit `<ordner>/` darin.
  Ein Update behält den Ordnernamen: die Verwaltung nennt die
  Erweiterung, die sie aktualisiert (`"dir"` in
  `POST /api/v1/webui/extensions`), und die Box weist ein Archiv ab,
  das eine andere enthält.
- Die Shell-Skripte, die die Box für sie ausführt, sind die, die
  `manifest.json` unter `scripts` nennt: eine Liste von höchstens 64
  Namen aus Buchstaben, Ziffern, Punkten, Bindestrichen und
  Unterstrichen, die weder mit einem Bindestrich noch mit einem Punkt
  beginnen; jeder muss als normale Datei `<ordner>/scripts/<name>.sh`
  im Archiv liegen, sonst wird die Installation abgewiesen. Diese und
  keine andere Datei werden ausführbar (0755). Sie bleiben im Ordner
  und laufen von dort, über
  `POST /api/v1/webui/extensions/<ordner>/scripts/<name>` (Stufe
  System, die Argumente als JSON-Liste, ohne Shell, höchstens 10
  Sekunden und 64 KiB Ausgabe); nach `<webroot>/scripts/` wird nichts
  kopiert, ein Name muss also nur innerhalb der Erweiterung eindeutig
  sein.
- Skripte laufen **unverändert**: Beim Installieren wird in einer
  Erweiterung nichts umgeschrieben, ein Skript findet die eigenen
  Dateien der Erweiterung also über den eigenen Pfad:
  ```sh
  SCRIPTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
  EXTDIR=$(dirname -- "$SCRIPTDIR")
  ```
- Bau-Kommando (nutzen auch die Release-Workflows):

```sh
mkdir -p /tmp/stage/<ordner> && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/<ordner>/ && \
tar -czf <ordner>.tar.gz -C /tmp/stage <ordner>
```

## Regeln

- Keine Abhängigkeiten auf Kern-Interna — nur die dokumentierte
  `/api/v1/`-Fläche, die `webui-*`-Klassen und -Helfer der Seiten und
  die Skript-Route oben für Aktionen.
- Der Ordnername ist der stabile Schlüssel (Config-Keys, URL
  `/extensions/<ordner>/...`) — nach der Veröffentlichung nicht ändern.
- Erweiterungen sind standardmäßig **aus** (Opt-in durch den Nutzer); nur
  eine, die an die Stelle einer Seite der Oberfläche tritt, verlangt
  `"default": "on"`.

## Offizielle Erweiterungen

Die offiziellen Erweiterungen werden gegen `/api/v1` neu gebaut; jede
bekommt ein eigenes versioniertes Repository und einen Eintrag in einem
Update-Index, den die Verwaltung liest. Die bisherigen Repositories
`github.com/tuxbox-neutrino/yweb-extension-<name>` (installer,
m3u-converter, camds, infoicons, hallo-yweb) samt ihrem Index
`yweb-extensions` gehören zum alten Skript-Vertrag; ihre Erweiterungen
laufen mit dieser Version nicht.
