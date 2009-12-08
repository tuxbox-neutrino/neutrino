#!/bin/sh
# -----------------------------------------------------------
# API Script (yjogol)
# $Date: 2007/02/21 17:41:04 $
# $Revision: 1.1 $
# -----------------------------------------------------------
path_httpd=".."
path_scripts="$path_httpd/scripts"
path_usrbin="/var/bin"
path_config="/var/tuxbox/config"
path_tmp="/tmp"

# -----------------------------------------------------------
do_udp_stream()
{
echo "para: $*"
	up="no"
	if [ -e /var/bin/udpstreamts ];	then
		up="/var/bin/udpstreamts"
	else
		up="udpstreamts"
	fi
	case "$1" in
		installed)
			echo "$up" ;;		
		start)
			shift 1
			killall streamts
			killall streampes
			killall udpstreamts
			trap "" 1;$up $* &
			;;
		stop)
			killall udpstreamts ;;
	esac
}
# -----------------------------------------------------------
# Main
# -----------------------------------------------------------
case "$1" in
	version)
		echo '$Revision: 1.1 $' ;;
		
	udp_stream)
		shift 1
		do_udp_stream $*
		;;
	*)
		echo "[api.sh] Parameter wrong: $*" ;;
esac



