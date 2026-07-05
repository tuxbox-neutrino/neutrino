#!/bin/sh
# -----------------------------------------------------------
# BoxInfo (yjogol)
# $Date: 2007-12-29 17:08:16 $
# $Revision: 1.1 $
# -----------------------------------------------------------

do_line()
{
	echo "------------------------------------------------------------------------------------"
}
do_dline()
{
	echo "===================================================================================="
}
do_header()
{
	do_line
	echo "          $1"
	do_line
}
do_proc()
{
	do_header "$1"
	cat /proc/$2
}
do_cmd()
{
	do_header "$1"
	shift 1
	$*
}

case "$1" in
# $2=l (long)
	proc)
		do_proc Kernel version
		if [ "$2" == "l" ]; then
			do_proc CPU cpuinfo
			do_proc 'Memory usage overview' meminfo
			do_proc Partitions partitions
			do_proc 'mtd list' mtd
			do_proc 'installed Filesystems' filesystems
			do_proc 'loaded modules' modules
			do_proc 'system uptime' uptime
			do_cmd "kernel stack" dmesg
		fi
		do_cmd 'free space' df
		do_cmd 'running processes' ps
		do_cmd 'busybox version and installed commands' busybox --help
		if [ "$2" == "l" ]; then
			do_cmd 'Commands in /bin' ls /bin
			do_cmd 'Commands in /sbin' ls /sbin
			do_cmd 'mounts' cat /etc/fstab
			if [ -e /etc/smb.conf ]; then
				do_cmd 'Samba Server configuration' cat /etc/smb.conf
			fi
			if [ -e /var/etc/auto.net ]; then
				do_cmd 'auto mounter configuration' cat /var/etc/auto.net
			fi
		fi
		;;
		avia_version) cat /proc/bus/avia_version ;;
		tuxinfo)	shift 1; tuxinfo $* ;;
		i2c)		cat /proc/bus/i2c ;;
		free)		free ;;
		avswitch)	/bin/switch ;;
		cpuinfo) 	cat /proc/cpuinfo ;;
		fb)			cat /proc/fb ;;
		uname)		shift 1; uname $* ;;
		busybox_help) busybox --help ;;
		proc_version) cat /proc/version ;;
		do_proc) 	cat /proc/$2 ;;
		*)
		echo "[Y_BoxInfo.sh] Parameter falsch: $*" ;;
esac
