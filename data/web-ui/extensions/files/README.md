# Files – the file manager of the Neutrino-Web-UI

`files` lists, uploads, downloads, renames and deletes files in the
folders the box names as its roots: where it records, where its movie
browser looks, its plugin folders and the usual media and temporary
directories, and nothing else on the box. It works through
`/api/v1/storage`; a file larger than one request goes up in pieces.

It takes the place of the file manager the interface had as a page of
its own, so it is switched on until it is switched off under
Settings → Extensions (`"default": "on"`). It shows as a tile under
Tools, and the dashboard's button for files leads to it while it is
on.

Its texts are in `languages/`; the page marks them as the interface's
pages do and takes the pack into the page's own texts
(`webuiExtTexts('files')`).

## Release

```sh
mkdir -p /tmp/stage/files && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/files/ && \
tar -czf files.tar.gz -C /tmp/stage files
```
