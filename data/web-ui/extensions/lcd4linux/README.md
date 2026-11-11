# LCD4Linux – the front display on a screen of its own

`lcd4linux` shows the picture LCD4Linux draws of the box's front
display, full screen on a dark background, for a tablet or a second
screen beside the box. It asks `GET /api/v1/osd/displays` whether
LCD4Linux draws, and then takes the picture from
`GET /api/v1/osd/displays/lcd4linux/screenshot`, again every few
seconds. The switch "Repeat every … s" beside Full screen sets how often,
as the screenshot window's does: on or off, every 1 to 180 seconds. The
browser keeps the choice (`localStorage`); without it the page repeats
every 15 seconds. The key `screenshot_timeout` in `webui.conf`, which this
page read before, is no longer read here and stays for older pages. A box
where LCD4Linux does not draw says so instead.

It has no header of the interface, as the page it replaces had none;
`webapp.json` lets a phone or tablet keep it on its home screen. The
former address `/lcd4linux/` leads here.

It takes the place of the interface's former `/lcd4linux/` page, so it
is switched on until it is switched off under Settings → Extensions
(`"default": "on"`). It shows as a tile under Tools.

Its texts are in `languages/` (`webuiExtTexts('lcd4linux')`).

## Release

```sh
mkdir -p /tmp/stage/lcd4linux && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/lcd4linux/ && \
tar -czf lcd4linux.tar.gz -C /tmp/stage lcd4linux
```
