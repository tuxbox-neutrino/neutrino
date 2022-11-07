#!/bin/sh

. %(PRIVATE_HTTPDDIR)/scripts/_Y_Globals.sh
. %(PRIVATE_HTTPDDIR)/scripts/_Y_Library.sh

BNAME=${0##*/}

case "$1" in
	start)
	;;
	stop)
	;;
	reset)
	;;
	fcm_start)
		echo "[$BNAME] fritzcallmonitor start"
		touch /var/etc/.fritzcallmonitor
		service fritzcallmonitor start >/dev/console
	;;
	fcm_stop)
		echo "[$BNAME] fritzcallmonitor stop"
		service fritzcallmonitor stop >/dev/console
		rm -f /var/etc/.fritzcallmonitor
	;;
	nfs_start)
		echo "[$BNAME] nfs-server start"
		touch /var/etc/.nfsd
		service nfsd start >/dev/console
	;;
	nfs_stop)
		echo "[$BNAME] nfs-server stop"
		service nfsd stop >/dev/console
		rm -f /var/etc/.nfsd
	;;
	samba_start)
		echo "[$BNAME] samba-server start"
		touch /var/etc/.samba
		service samba start >/dev/console
	;;
	samba_stop)
		echo "[$BNAME] samba-server stop"
		service samba stop >/dev/console
		rm -f /var/etc/.samba
	;;
	tuxcald_start)
		echo "[$BNAME] tuxcal start"
		touch /var/etc/.tuxcald
		service tuxcald start >/dev/console
	;;
	tuxcald_stop)
		echo "[$BNAME] tuxcal stop"
		service tuxcald stop >/dev/console
		rm -f /var/etc/.tuxcald
	;;
	tuxmaild_start)
		echo "[$BNAME] tuxmail start"
		touch /var/etc/.tuxmaild
		service tuxmaild start >/dev/console
	;;
	tuxmaild_stop)
		echo "[$BNAME] tuxmail stop"
		service tuxmaild stop >/dev/console
		rm -f /var/etc/.tuxmaild
	;;
	inadyn_start)
		echo "[$BNAME] inadyn start"
		touch /var/etc/.inadyn
		service inadyn start >/dev/console
	;;
	inadyn_stop)
		echo "[$BNAME] inadyn stop"
		service inadyn stop >/dev/console
		rm -f /var/etc/.inadyn
	;;
	dropbear_start)
		echo "[$BNAME] dropbear start"
		touch /var/etc/.dropbear
		service dropbear start >/dev/console
	;;
	dropbear_stop)
		echo "[$BNAME] dropbear stop"
		service dropbear stop >/dev/console
		rm -f /var/etc/.dropbear
	;;
	ushare_start)
		echo "[$BNAME] ushare start"
		touch /var/etc/.ushare
		service ushare start >/dev/console
	;;
	ushare_stop)
		echo "[$BNAME] ushare stop"
		service ushare stop >/dev/console
		rm -f /var/etc/.ushare
	;;
	djmount_start)
		echo "[$BNAME] djmount start"
		touch /var/etc/.djmount
		service djmount start >/dev/console
	;;
	djmount_stop)
		echo "[$BNAME] djmount stop"
		service djmount stop >/dev/console
		rm -f /var/etc/.djmount
	;;
	minidlna_start)
		echo "[$BNAME] minidlna start"
		touch /var/etc/.minidlnad
		service minidlnad start >/dev/console
	;;
	minidlna_stop)
		echo "[$BNAME] minidlna stop"
		service minidlnad stop >/dev/console
		rm -f /var/etc/.minidlnad
	;;
	xupnpd_start)
		echo "[$BNAME] xupnpd start"
		touch /var/etc/.xupnpd
		service xupnpd start >/dev/console
	;;
	xupnpd_stop)
		echo "[$BNAME] xupnpd stop"
		service xupnpd stop >/dev/console
		rm -f /var/etc/.xupnpd
	;;
	crond_start)
		echo "[$BNAME] crond start"
		touch /var/etc/.crond
		service crond start >/dev/console
	;;
	crond_stop)
		echo "[$BNAME] crond stop"
		service crond stop >/dev/console
		rm -f /var/etc/.crond
	;;
	*)
		echo "[$BNAME] Parameter wrong: $*"
	;;
esac

exit 0
