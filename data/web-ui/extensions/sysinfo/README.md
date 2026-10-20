# System information – what the box is and how it is doing

`sysinfo` shows the box's hardware and software, its memory, flash and
temperature, its network interfaces, and on request its process list
and kernel log, all read through `/api/v1/system`. An address ending in
`#network` (or another card's name) opens it at that card.

It takes the place of the page the interface had for this, so it is
switched on until it is switched off under Settings → Extensions
(`"default": "on"`). It shows as a tile under Tools, and the
dashboard's cards for the box's figures and its network open it while
it is on.

Its texts are in `languages/`; the page marks them as the interface's
pages do and takes the pack into the page's own texts
(`webuiExtTexts('sysinfo')`).

## Release

```sh
mkdir -p /tmp/stage/sysinfo && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/sysinfo/ && \
tar -czf sysinfo.tar.gz -C /tmp/stage sysinfo
```
