#!/bin/sh

. /etc/init.d/globals

BAKF="/var/backup_flash.tar.gz"

if [ -e ${BAKF} ]; then
	SHOWINFO "restore settings from ${BAKF} ..."
	cd / && tar -xzf "${BAKF}"
	sync
	rm -rf "${BAKF}"
	SHOWINFO "done."
else
	SHOWINFO "${BAKF} not found. nothing to restore!"
fi
