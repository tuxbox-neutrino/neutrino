# Bouquet editor – the box's bouquets and their channels

`bouquet-editor` lists the box's bouquets beside all its channels and
lets them be put together: bouquets created, renamed, locked, hidden,
sorted and deleted, channels added, moved and taken out, by drag and
drop or by keyboard. It works through `/api/v1/bouquets` and follows
changes made elsewhere through the box's event stream.

It takes the place of the page the interface had for this, so it is
switched on until it is switched off under Settings → Extensions
(`"default": "on"`). It shows as a tile under Tools.

Its texts are in `languages/`; the page marks them as the interface's
pages do and takes the pack into the page's own texts
(`webuiExtTexts('bouquet-editor')`).

## Release

```sh
mkdir -p /tmp/stage/bouquet-editor && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/bouquet-editor/ && \
tar -czf bouquet-editor.tar.gz -C /tmp/stage bouquet-editor
```
