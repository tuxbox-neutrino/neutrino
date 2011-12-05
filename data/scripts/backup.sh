#!/bin/sh
DATE=`date +%Y-%m-%d-%H:%M:%S`
BACKUPLIST=/bin/backup.list

echo Backup to $1/settings_$DATE.tar
cd /

if [ ! -e $BACKUPLIST ]; then
	tar cf $1/settings_$DATE.tar /var/tuxbox/config/
else
	tar cf $1/settings_$DATE.tar -T $BACKUPLIST
fi
