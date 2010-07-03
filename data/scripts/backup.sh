#!/bin/sh
DATE=`date +%Y-%m-%d-%H:%M:%S`
echo Backup to $1/settings_$DATE.tar
cd /var/tuxbox/
tar cf $1/settings_$DATE.tar config/
