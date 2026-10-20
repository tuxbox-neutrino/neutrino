# m3u-Konverter – Playlisten zu WebTV-XML

*English version: [README.md](README.md)*

`m3u-converter` wandelt m3u-Playlisten (`#EXTINF`) in Neutrinos
WebTV-XML-Format (`webtv_usr.xml`). Die Umwandlung läuft ganz im
Browser; das Ergebnis lässt sich herunterladen oder direkt auf der Box
speichern. Danach die Datei unter Einstellungen → WebTV-Quellen
eintragen.

Die Erweiterung ist aus, bis sie unter Einstellungen → Erweiterungen
eingeschaltet wird; dann erscheint sie als Kachel unter Werkzeuge.

## Aufbau

```
m3u-converter/
├── manifest.json           # Name, Version, Slot, Entry, Icon
├── m3u-convert.html        # die Seite
├── m3u-convert.js          # der Konverter, im Browser
├── icon.svg
├── languages/en.json, de.json
├── README.md / README.de.md
└── LICENSE                 # GPL-2.0 (wie Neutrino)
```

Ihre Texte liegen in `languages/`; die Seite markiert sie wie die Seiten
der Oberfläche und nimmt das Paket in die eigenen Texte der Seite auf
(`webuiExtTexts('m3u-converter')`).

## Release

```sh
mkdir -p /tmp/stage/m3u-converter && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/m3u-converter/ && \
tar -czf m3u-converter.tar.gz -C /tmp/stage m3u-converter
```
