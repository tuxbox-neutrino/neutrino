#!/bin/sh
# -----------------------------------------------------------
# API Script (yjogol)
# for yWeb independent shell calls
# $Date$
# $Revision$
# -----------------------------------------------------------
API_VERSION_MAJOR="1"
API_VERSION_MINOR="0"
API_VERSION_TEXT="$API_VERSION_MAJOR.$API_VERSION_MINOR"
#path_httpd="%(PRIVATE_HTTPDDIR)"
path_httpd=".."
path_scripts="$path_httpd/scripts"
path_bin="/bin"
path_varbin="/var/bin"
path_sbin="/sbin"
path_config="%(CONFIGDIR)"
path_tmp="/tmp"

streaming_client_status="$path_tmp/streaming_client"

# -----------------------------------------------------------
# udp control for neutrinoTV and yWeb LiveTV
do_udp_stream()
{
	up="no"
	if [ -e $path_varbin/udpstreamts ];	then
		up="$path_varbin/udpstreamts"
	else
		if [ -e $path_sbin/udpstreamts ]; then
			up="$path_sbin/udpstreamts"
		fi
	fi

	case "$1" in
		installed)
			echo "$up" ;;
		start)
			shift 1
			killall streamts
			killall udpstreamts
			echo $* > $streaming_client_status
			trap "" 1;$up $* &
			;;
		stop)
			killall udpstreamts
			rm $streaming_client_status
			echo "ok"
			;;
		kill_all_streams)
			killall streamts
			killall streampes
			killall udpstreamts
			rm $streaming_client_status
			echo "ok"
			;;
	esac
}
# -----------------------------------------------------------
# Main
# -----------------------------------------------------------
case "$1" in
	version)
		echo $API_VERSION_TEXT ;;

	udp_stream)
		shift 1
		do_udp_stream $*
		;;
	streaming_status)
		if [ -e $streaming_client_status ]; then
			cat $streaming_client_status
		else
			echo "Streams: "
			ps | grep stream | grep -v grep | grep -v sh | cut -d " " -f 15
		fi
		;;
	streaming_lock)
		shift 1
		echo $* > $streaming_client_status # first parameter should always be the ip of the client
		echo "ok"
		;;
	streaming_unlock)
		rm $streaming_client_status
		echo "ok"
		;;
	*)
		echo "[api.sh] Parameter wrong: $*" ;;
esac
