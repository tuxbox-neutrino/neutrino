# Screenshot – what the box shows, with the remote control beside it

`screenshot` takes a picture of what the box shows, the on-screen
display, the video or both, through `/api/v1`, saves it, and offers the
remote control beside it, so the box can be worked from the browser
while watching the result.

It takes the place of the page the interface had for this, so it is
switched on until it is switched off under Settings → Extensions
(`"default": "on"`). It shows as a tile under Tools, and the
dashboard's screenshot button leads to it while it is on.

Its texts are in `languages/`; the page marks them as the interface's
pages do and takes the pack into the page's own texts
(`webuiExtTexts('screenshot')`). The remote control is the
interface's own (`webuiRenderRemote`), so a remote skin extension
changes it here too.

## Release

```sh
mkdir -p /tmp/stage/screenshot && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/screenshot/ && \
tar -czf screenshot.tar.gz -C /tmp/stage screenshot
```
