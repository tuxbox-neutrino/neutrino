#!/bin/sh
DATE=`date +%Y-%m-%d-%H:%M:%S`
echo Backup to $1/settings_$DATE.tar
cd /
tar cf $1/settings_$DATE.tar /var/tuxbox/config/
