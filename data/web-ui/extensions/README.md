# yWeb-Erweiterung in 10 Minuten

Das moderne yWeb lädt Erweiterungen automatisch aus diesem Verzeichnis.
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
| `description` | optionaler Untertitel |

## 3. Seite schreiben (`index.html`)

Kopiervorlage: [`hallo-yweb/hallo.html`](hallo-yweb/hallo.html). Kernpunkte:

- yWeb-Header/Nav einbinden:
  `<div class="yweb-header-wrapper" data-page="tools"></div>` +
  `<script src="/yweb-header.js"></script>` + `ywebInitHeader('tools');`
- Styling: Pico CSS + `/css/yweb.css` verlinken — fertige Klassen
  (`settings-card`, `yweb-form-row`, `yweb-btn`) nutzen.
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

## Regeln

- Keine Abhängigkeiten auf Kern-Interna — nur die dokumentierte
  `/api/v4/`-Fläche (plus `exec`-Vertrag für Script-Aktionen).
- Der Ordnername ist der stabile Schlüssel (Config-Keys, URL
  `/extensions/<ordner>/...`) — nach der Veröffentlichung nicht ändern.
- Erweiterungen sind standardmäßig **aus** (Opt-in durch den Nutzer).
