# yweb-extension-m3u-converter

*English version: [README.md](README.md)*

**m3u-Konverter** — eine [yWeb-Erweiterung](https://github.com/tuxbox-neutrino/yweb-extensions)
für Tuxbox-Neutrino: wandelt m3u-Playlisten (`#EXTINF`) in Neutrinos
WebTV-XML-Format (`webtv_usr.xml`). Die Konvertierung läuft komplett im
Browser; das Ergebnis kann heruntergeladen oder direkt ins
Config-Verzeichnis der Box gespeichert werden (`/api/v4/file`-Upload).

## Installation

Einstellungen → **Erweiterungen** → *Installieren*: Release-Tarball
(`m3u-converter.tar.gz` aus dem
[aktuellen Release](../../releases/latest)) hochladen oder dessen
Download-URL einfügen. Danach aktivieren und Seite neu laden — die
Kachel erscheint unter Tools (`slot: tools`). Nach dem Umwandeln die
erzeugte Datei unter Einstellungen → WebTV-Quellen eintragen.

## Aufbau

```
m3u-converter/              ← Tarball-Top-Ordner == Erweiterungs-Ordner
├── manifest.json           # Name, Version (kanonisch), Slot, Entry
├── m3u-convert.html        # Seite inkl. Konverter-Logik (client-seitig)
├── languages/
│   ├── English.json        # flaches key/value, inkl. manifest.name/.description
│   └── Deutsch.json
├── README.md / README.de.md
├── LICENSE                 # GPL-2.0 (wie Neutrino)
└── .github/workflows/      # version-bump + version-line
```

Die Seite nutzt nur die dokumentierte `/api/v4/`-Fläche (`paths`,
`file`-Upload) und referenziert Kern-Assets ohne
`?v=`-Cache-Bust-Parameter. Texte folgen dem Erweiterungs-i18n-Muster:
Englisch inline als Fallback, Übersetzungen aus
`languages/<Language>.json` via `ywebExtL`.

## Versionierung und Releases

Semantisch `v<major>.<minor>.<patch>`; die kanonische Version steht in
`manifest.json`. Die CI bumpt bei jedem Push auf `main` den Patch
(Patch = Commits seit dem `v<maj>.<min>`-Anker-Tag), taggt das Release
und hängt den Tarball als
`releases/latest/download/m3u-converter.tar.gz` an — die stabile URL,
die der Erweiterungs-Manager und der
[yweb-extensions-Index](https://github.com/tuxbox-neutrino/yweb-extensions)
nutzen. Eine neue Minor-Linie startet der `version-line`-Workflow.

## Packaging-Vertrag (Kurzfassung)

Der Release-Tarball enthält genau einen Top-Level-Ordner
(`m3u-converter/`) mit `manifest.json` darin; er entpackt nach
`<webroot>/extensions/`. Vollständiger Vertrag: siehe das Meta-Repository
[yweb-extensions](https://github.com/tuxbox-neutrino/yweb-extensions).
