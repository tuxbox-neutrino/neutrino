# m3u Converter – playlists to WebTV XML

*Deutsche Fassung: [README.de.md](README.de.md)*

`m3u-converter` turns m3u playlists (`#EXTINF`) into Neutrino's WebTV
XML format (`webtv_usr.xml`). The conversion runs entirely in the
browser; the result can be downloaded or saved straight to the box.
Then add the file under Settings → WebTV sources.

It is off until it is switched on under Settings → Extensions; it then
shows as a tile under Tools.

## Layout

```
m3u-converter/
├── manifest.json           # name, version, slot, entry, icon
├── m3u-convert.html        # the page
├── m3u-convert.js          # the converter, in the browser
├── icon.svg
├── languages/en.json, de.json
├── README.md / README.de.md
└── LICENSE                 # GPL-2.0 (same as Neutrino)
```

Its texts are in `languages/`; the page marks them as the interface's
pages do and takes the pack into the page's own texts
(`webuiExtTexts('m3u-converter')`).

## Release

```sh
mkdir -p /tmp/stage/m3u-converter && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/m3u-converter/ && \
tar -czf m3u-converter.tar.gz -C /tmp/stage m3u-converter
```
