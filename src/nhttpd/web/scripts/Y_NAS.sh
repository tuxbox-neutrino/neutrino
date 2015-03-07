#!/bin/sh
# -----------------------------------------------------------
# yWeb Extension: Filemgr (by yjogol)
# yCVS: $Date: 2007-12-29 08:20:30 $
# yCVS: $Revision: 1.1 $
# -----------------------------------------------------------

# -----------------------------------------------------------
# Main
# -----------------------------------------------------------

case "$1" in
	list_users)
		cat $fpasswd|sed 's/^\([^:]*\).*/<input type=radio name=users value=\1>\1<br>/' ;;

	filemgr_list)
		shift 1
		ls -al $* ;;

	filemgr_chmod)
		shift 1
		chmod $*
		;;

	filemgr_mkdir)
		shift 1
		mkdir $*
		;;

	filemgr_rm)
		shift 1
		rm -f $*
		;;

	filemgr_rmdir)
		shift 1
		rm -rf $*
		;;

	filemgr_upload)
		shift 1
		mv /tmp/upload.tmp "$1/$2"
		rm -f /tmp/upload.tmp
		;;

	filemgr_ren)
		shift 1
		mv -f $1 $2
		;;

	filemgr_copy)
		shift 1
		cp -r -f $1 $2
		;;
	nhttpd_can_sendall)
		grep sendAll=true /var/tuxbox/config/nhttpd.conf
		;;
	filemgr_check_movieplayer_xml)
		shift 1
		grep "neutrino commandversion" $*
		;;
	filemgr_vlc_file)
		shift 1
		echo "$*" >/tmp/vlc.m3u
		;; 		

	*)
		echo "[Y_NAS.sh] Parameter falsch: $*" ;;
esac



