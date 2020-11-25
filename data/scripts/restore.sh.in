#!/bin/sh
echo Restore settings from $1

tar t -f $1 | grep ^config/ > /dev/null
if [ $? -eq 0 ]; then
	cd /var/tuxbox
else
	cd /
fi

# check if $1 ends with "gz"
if [ "$1" != "${1%gz}" ]; then
	tar -xzf $1
else
	tar -xf $1
fi

sync
sync
reboot -f
