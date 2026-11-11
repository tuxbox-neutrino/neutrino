# System shares – what the box's system mounts at start and on demand

`netfs` edits the box's two files for the network shares its system
mounts itself, also without Neutrino, through `/api/v1/storage/netfs`
(the shares Neutrino mounts are under Settings → Network shares):

- **Mounted at start** (`/var/etc/fstab`): what the box mounts when it
  starts. Mount all and Unmount all run the box's own start up script,
  and the page shows what it wrote.
- **Automounter** (`/var/etc/auto.net`): what the automounter offers
  under `/mnt/autofs` once somebody looks there. It reads its file only
  when it starts, so after a change the page offers to restart it.

Each file has 8 slots; an entry is NFS or CIFS (Windows, Samba), with a
server, a directory or share on it, where it lands and its options. A
stored password is never read back: the field stays empty, and left
empty the stored one stays. It comes off when the slot is emptied. A
slot's summary says whether its entry is mounted now
(`/api/v1/storage/mounts`).

The shares Neutrino mounts itself, the eight the screen at the box
edits, are not these: they are in the interface's settings under
Network shares, and the page links there.

It takes the place of the interface's former page for these files, so
it is switched on until it is switched off under Settings → Extensions
(`"default": "on"`). It shows as a tile under Tools; an address ending
in `#automount` opens it at the automounter's file.

Its texts are in `languages/`; the page marks them as the interface's
pages do and takes the pack into the page's own texts
(`webuiExtTexts('netfs')`).

## Release

```sh
mkdir -p /tmp/stage/netfs && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/netfs/ && \
tar -czf netfs.tar.gz -C /tmp/stage netfs
```
