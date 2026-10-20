# Services – start and stop what runs in the background

`services` lists the services the box can drive, such as NFS, Samba or
cron, through `GET /api/v1/daemons`, and starts, stops and restarts them
through `POST /api/v1/daemons/{name}/start|stop|restart`. A row says
whether a process of the service's name runs right now; after each
request the page reads the list again, and once more two seconds later,
because a service that dies right after its start was started and is not
running. What the image was built without is folded away below the list,
named, so nobody goes looking for it.

Reading the list needs no sign-in from the home network; starting or
stopping a service does. Softcams are not in the list.

It takes the place of the services part of the interface's former
plugin page, so it is switched on until it is switched off under
Settings → Extensions (`"default": "on"`). It shows as a tile under
Tools.

Its texts are in `languages/`; the page marks them as the interface's
pages do and takes the pack into the page's own texts
(`webuiExtTexts('services')`).

## Release

```sh
mkdir -p /tmp/stage/services && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/services/ && \
tar -czf services.tar.gz -C /tmp/stage services
```
