#!/bin/sh
echo Restore settings from $1
cd /
tar xf $1
sync
sync
reboot -f
