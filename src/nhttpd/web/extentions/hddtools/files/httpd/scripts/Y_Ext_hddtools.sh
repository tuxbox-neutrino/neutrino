#!/bin/sh
# -----------------------------------------------------------
# HDD (yjogol)
# $Date: 2007-12-29 10:11:37 $
# $Revision: 1.1 $
# -----------------------------------------------------------

# -----------------------------------------------------------
hdd_proc1="/proc/ide/ide0/hda"
hdd_model1="$hdd_proc1/model"
hdd_settings1="$hdd_proc1/settings"
disc1="/dev/ide/host0/bus0/target0/lun0/disc"
# -----------------------------------------------------------
y_format_message_html()
{
	tmp="<html><head><meta http-equiv='Content-Type' content='text/html; charset=windows-1252'>"
	tmp="$tmp <link rel='stylesheet' type='text/css' href='/Y_Main.css'></head>"
	tmp="$tmp <body><div class='work_box'><div class='work_box_head'><div class='work_box_head_h2'>Results</div></div><div class='work_box_body' style='overflow:auto'>"
	tmp="$tmp <pre>\n$msg\n</pre></div></div></body></html>"
	
#	tmp="$tmp <body><div class='y_work_box'><pre>\n$msg\n</pre></div></body></html>"
	echo -e "$tmp"
}
# -----------------------------------------------------------
# Main
# -----------------------------------------------------------
case "$1" in
	hdd_proc)
		msg=`cat $hdd_proc1/$2`
		msg="<b>$3</b><br><br>$msg"
		y_format_message_html
		;;

	hdd_sleep)
		msg=`hdparm -Y $disc1`
		msg="<b>Sleep</b><br><br>$msg"
		y_format_message_html
		;;

	hdd_hdparm)
		msg=`hdparm $2 $disc1`
		msg="<b>$3</b><br><br>$msg"
		y_format_message_html
		;;
	*)
		echo "[Y_HDD.sh] Parameter falsch: $*" ;;
esac



