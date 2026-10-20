# LCD4Linux – the front display on a screen of its own

`lcd4linux` shows the picture LCD4Linux draws of the box's front
display, full screen on a dark background, for a tablet or a second
screen beside the box. It asks `GET /api/v1/osd/displays` whether
LCD4Linux draws, and then takes the picture from
`GET /api/v1/osd/displays/lcd4linux/screenshot`, again every few
seconds: as often as the interface's screenshot timeout says (Settings
→ Neutrino-Web-UI), no more often than once a second, and every 15
seconds where it says nothing. A box where LCD4Linux does not draw says
so instead.

It has no header of the interface, as the page it replaces had none;
`webapp.json` lets a phone or tablet keep it on its home screen. The
former address `/lcd4linux/` leads here.

It takes the place of the interface's former `/lcd4linux/` page, so it
is switched on until it is switched off under Settings → Extensions
(`"default": "on"`). It shows as a tile under Tools.

Its one text is in `languages/` (`webuiExtTexts('lcd4linux')`).

## Release

```sh
mkdir -p /tmp/stage/lcd4linux && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/lcd4linux/ && \
tar -czf lcd4linux.tar.gz -C /tmp/stage lcd4linux
```
