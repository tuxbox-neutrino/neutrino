# Info screen – what runs now, full screen

`info` shows on a screen of its own what the box plays and what runs on
it, for a tablet or a second screen beside the box, in the look of the
page it replaces: at the top ECM (a softcam answers for the channel; the
page it replaces never lit it, as LCD4Linux's rule for it was switched
off), CI+ (a module is sent it), TIMER (a zap or a recording is still to
come), REC (the box records) and TS (it keeps its shift of what it
shows); in the middle the channel's logo, or its name where the box has
none, or the word Standby while the box is in standby; at the bottom the
event, how far it is, the clock and the minutes done of all.

Everything comes from `/api/v1`: `system/standby`, `channels/current`,
`epg/current`, `channels/{id}/logo`, `system/decryption`, `timers` and
`recordings`. The page reads again on the box's events (`zap`,
`standby`, `record-start`, `record-stop`, `timer-changed`, `epg-updated`
for the channel shown) and every 15 seconds besides, and moves the clock
and the bar in between.

It has no header of the interface, as the page it replaces had none;
`webapp.json` lets a phone or tablet keep it on its home screen. The
former address `/info/` leads here.

It is switched on until it is switched off under Settings → Extensions
(`"default": "on"`), and shows as a tile under Tools.

Its texts are in `languages/` (`webuiExtTexts('info')`).

## Release

```sh
mkdir -p /tmp/stage/info && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/info/ && \
tar -czf info.tar.gz -C /tmp/stage info
```
