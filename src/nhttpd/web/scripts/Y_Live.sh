#!/bin/sh
# -----------------------------------------------------------
# Live (yjogol)
# $Date$
# $Revision$
# -----------------------------------------------------------

. ./_Y_Globals.sh
. ./_Y_Library.sh

# -----------------------------------------------------------
live_lock()
{
	if [ "$boxtype" != "coolstream" ]; then
		call_webserver "control/lcd?lock=1&clear=1&rect=10,10,110,50,1,0&xpos=20&ypos=27&size=22&font=2&text=%20%20%20%20yWeb%0A%20%20LiveView&update=1" >/dev/null
	fi
	call_webserver "control/rc?lock" >/dev/null
	call_webserver "control/zapto?stopplayback" >/dev/null
}
# -----------------------------------------------------------
live_unlock()
{
	if [ "$boxtype" != "coolstream" ]; then
		call_webserver "control/lcd?lock=0" >/dev/null
	fi
	call_webserver "control/rc?unlock"  >/dev/null
	call_webserver "control/zapto?startplayback" >/dev/null
}
# -----------------------------------------------------------
prepare_tv()
{
	# SPTS on
	if [ "$boxtype" != "coolstream" ]; then
		call_webserver "control/system?setAViAExtPlayBack=spts" >/dev/null
	fi
}
# -----------------------------------------------------------
prepare_radio()
{
	# SPTS off
	if [ "$boxtype" != "coolstream" ]; then
		call_webserver "control/system?setAViAExtPlayBack=pes" >/dev/null
	fi
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
		if [ "$2" = "start" ]
		then
			shift 2
			killall streamts
			killall streampes
			killall udpstreamts
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
		echo "Parameter wrong: $*" ;;
esac



