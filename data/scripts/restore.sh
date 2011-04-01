#!/bin/sh
echo Restore settings from $1

tar t -f $1 | grep ^config/ > /dev/null
if [ $? -eq 0 ]; then
	cd /var/tuxbox
else
	cd /
fi

tar xf $1
sync
sync
reboot -f
