# m3u-Konverter – Playlisten zu WebTV-XML

*English version: [README.md](README.md)*

`m3u-converter` wandelt m3u-Playlisten (`#EXTINF`) in Neutrinos
WebTV-XML-Format (`webtv_usr.xml`). Die Umwandlung läuft ganz im
Browser; das Ergebnis lässt sich herunterladen oder direkt auf der Box
speichern, in den Ordner, in dem die Box ihre eigenen WebTV-Listen hält
(den `GET /api/v1/channels/sources` nennt), über
`PUT /api/v1/storage/file`. Eine gleichnamige Datei dort wird nur nach
Rückfrage ersetzt. Auf Wunsch kommt die gespeicherte Datei gleich zu den
WebTV-Quellen (`PUT /api/v1/channels/sources/webtv`), und die Box liest
ihre Kanallisten neu.

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
