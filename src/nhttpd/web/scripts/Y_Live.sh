#!/bin/sh
# -----------------------------------------------------------
# Live (yjogol)
# $Date: 2007/02/21 17:41:04 $
# $Revision: 1.2 $
# -----------------------------------------------------------

. ./_Y_Globals.sh
. ./_Y_Library.sh

# -----------------------------------------------------------
live_lock()
{
	call_webserver "control/lcd?lock=1&clear=1&rect=10,10,110,50,1,0&xpos=20&ypos=27&size=22&font=2&text=%20%20%20%20yWeb%0A%20%20LiveView&update=1" >/dev/null
	call_webserver "control/rc?lock" >/dev/null
	call_webserver "control/zapto?stopplayback" >/dev/null
}
# -----------------------------------------------------------
live_unlock()
{
	call_webserver "control/lcd?lock=0" >/dev/null
	call_webserver "control/rc?unlock"  >/dev/null
	call_webserver "control/zapto?startplayback" >/dev/null
}
# -----------------------------------------------------------
prepare_tv()
{
	# SPTS on
	call_webserver "control/system?setAViAExtPlayBack=spts" >/dev/null
}
# -----------------------------------------------------------
prepare_radio()
{
	# SPTS off
	call_webserver "control/system?setAViAExtPlayBack=pes" >/dev/null
}

# -----------------------------------
# Main
# -----------------------------------
echo "$1" >/tmp/debug.txt
echo "$*"
case "$1" in
	zapto)
		if [ "$2" != "" ]
		then
			call_webserver "control/zapto?$2" >/dev/null
		fi
		;;

	switchto)
		if [ "$2" = "Radio" ]
		then
			call_webserver "control/setmode?radio" >/dev/null
		else
			call_webserver "control/setmode?tv" >/dev/null
		fi
		;;

	url)
		url=`buildStreamingRawURL`
		echo "$url" ;;

	audio-url)
		url=`buildStreamingAudioRawURL`
		echo "$url" ;;

	live_lock)
		live_lock ;;

	live_unlock)
		live_unlock ;;

	dboxIP)
		buildLocalIP ;;

	prepare_radio)
		prepare_radio
		Y_APid=`call_webserver "control/yweb?radio_stream_pid"`
		url="http://$2:31338/$Y_APid"
		echo "$url" > $y_tmp_m3u
		echo "$url" > $y_tmp_pls
		;;

	prepare_tv)
		prepare_tv
		;;
		
	udp_stream)
echo "udp stream"
		if [ "$2" = "start" ]
		then
			shift 2
echo "kill all streams"
			killall streamts
			killall streampes
			killall udpstreamts
echo "start udpstream"
			if [ -e /var/bin/udpstreamts ]
			then
				/var/bin/udpstreamts $* &
			else
				udpstreamts $* &
			fi
			pidof udpstreamts >/tmp/udpstreamts.pid
		fi
		if [ "$2" = "stop" ]
		then
			killall udpstreamts
		fi
		;;

	*)
		echo "Parameter falsch: $*" ;;
esac



