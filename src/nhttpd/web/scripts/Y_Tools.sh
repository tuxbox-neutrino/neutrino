#!/bin/sh
# -----------------------------------------------------------
# Tools (yjogol)
# $Date: 2007/02/21 17:41:04 $
# $Revision: 1.2 $
# -----------------------------------------------------------
. ./_Y_Globals.sh
. ./_Y_Library.sh
# ===========================================================
# Settings : Skins
# ===========================================================
# -----------------------------------------------------------
# Skin Liste
# -----------------------------------------------------------
skin_get()
{
	check_Y_Web_conf
	active_skin=`config_get_value_direct $y_config_Y_Web 'skin'`
	html_option_list=""
	skin_list=`find $y_path_httpd -name 'Y_Main-*'`
	for f in $skin_list
	do
		skin=`echo "$f"|sed -e s/^.*Y_Main-//g|sed -e s/.css//g`
		if [ "$skin" = "$active_skin" ]
		then
			selec="selected"
		else
			selec=""
		fi
		opt="<option $selec value='$skin'>$skin</option>"
		html_option_list="$html_option_list $opt"
	done
	echo "$html_option_list"
}
# -----------------------------------------------------------
# Skin setzen : css ueberschreiben  $1=Skin-Name
# -----------------------------------------------------------
skin_set()
{
	cd $y_path_httpd
	cp Y_Main-$1.css Y_Main.css
	if [ -e global-$1.css ]
	then
		cp global-$1.css global.css
	else
		cp global-Standard.css global.css
	fi
	config_set_value_direct $y_config_Y_Web 'skin' $1

	msg="Skin changed - Now browsers Refresh/actualization explain"
	y_format_message_html
}
# -----------------------------------------------------------
# Image Backup - build form
# -----------------------------------------------------------
image_upload()
{
	if [ -s "$y_upload_file" ]
	then
		msg="<b>Image upload ok</b><br>"
		msg="$msg <script language='JavaScript' type='text/javascript'>window.setTimeout('parent.do_image_upload_ready()',1000)</script>"
	else
		msg="Upload-Problem.<br>Please, try again."
		msg="$msg <script language='JavaScript' type='text/javascript'>window.setTimeout('parent.do_image_upload_ready_error()',1000)</script>"
	fi
	y_format_message_html
}
# ===========================================================
# Flashimage
# ===========================================================
# -----------------------------------
# Flash-Backup ($1=mtd Nummer)
# -----------------------------------
image_backup_mtd()
{
	rm /tmp/*.img
	cat /dev/mtd/$1 > /tmp/flash_mtd$1.img
}
# -----------------------------------
# Sende Download-Page ($1=mtd Nummer)
# -----------------------------------
# -----------------------------------
image_delete_download_page()
{
	rm -r /tmp/*.img
#	msg="<div class='y_work_box'><b>The image file in tmp was extinguished.</b></div>"
#	y_format_message_html
}
# -----------------------------------------------------------
# Flash ($1=mtd Nummer) Upload-File $2=true/false =simulate
# -----------------------------------------------------------
flash_mtd()
{
	simulate="true"
	if [ "$2" = "false" ]
	then
		simulate="false"
	fi
	rm /tmp/*.img
	if [ -s "$y_upload_file" ]
	then
		echo "" >/tmp/e.txt
		msg_nmsg "Image%20%20flashing!"
		if [ "$simulate" = "false" ]
		then
			fcp -v "$y_upload_file" /dev/mtd/$1 >/tmp/e.txt
		else #simulation/DEMO
			i="0"
			while test $i -le 10
			do
				p=`expr $i \* 10`
				b=`expr $i \* 63`
				b=`expr $b / 10`
				echo -e "\rDEMO: Erasing blocks: $b/63 ($p%)" >>/tmp/e.txt
				i=`expr $i + 1`	
				sleep 1
			done
			i="0"
			while test $i -le 20
			do
				p=`expr $i \* 5`
				b=`expr $i \* 8064`
				b=`expr $b / 20`
				echo -e "\rDEMO: Writing data: $b k/8064k ($p%)" >>/tmp/e.txt
				i=`expr $i + 1`	
				sleep 2
			done
			i="0"
			while test $i -le 5
			do
				p=`expr $i \* 20`
				b=`expr $i \* 8064`
				b=`expr $b / 5`
				echo -e "\rDEMO: Verifying data: $b k/8064k ($p%)" >>/tmp/e.txt
				i=`expr $i + 1`	
				sleep 1
			done
		fi
		msg_nmsg "flashing%20ready.%20Reboot..."
		msg="flashing done ... please reboot box now ..."
		msg="$msg <script language='JavaScript' type='text/javascript'>window.setTimeout('parent.do_image_flash_ready()',1000)</script>"
		y_format_message_html
		
		if [ "$simulate" = "false" ]
		then
			busybox reboot -d10
		fi
	else
		msg="Upload-Problem.<br>Try again, please."
		msg="$msg <script language='JavaScript' type='text/javascript'>window.setTimeout('parent.do_image_flash_ready()',1000)</script>"
		y_format_message_html
	fi
}
# -----------------------------------------------------------
# copies Uploadfile to $1
# -----------------------------------------------------------
upload_copy()
{
	if [ -s "$y_upload_file" ]
	then
		cp "$y_upload_file" "$1"
	else
		msg="Upload-Problem.<br>Try again, please."
	fi
}
# -----------------------------------------------------------
bootlogo_upload()
{
	msg="Boot-Logo neu gesetzt"
	upload_copy "$y_boot_logo"
	y_format_message_html
}
# -----------------------------------------------------------
bootlogo_lcd_upload()
{
	msg="Boot-Logo-LCD neu gesetzt"
	upload_copy "$y_boot_logo_lcd"
	y_format_message_html
}
# -----------------------------------------------------------
ucodes_upload()
{
	msg="$1 hochgeladen<br><a href='/Y_Settings_ucodes.htm'><u>next file</u></a>"
	upload_copy "$y_path_ucodes/$1"
	y_format_message_html
}
# -----------------------------------------------------------
zapit_upload()
{
	msg="$1 hochgeladen<br><a href='/Y_Settings_zapit.htm'><u>next file</u></a>"
	upload_copy "$y_path_zapit/$1"
	y_format_message_html
}
# -----------------------------------------------------------
# Mount from Neutrino-Settings $1=nr
# -----------------------------------------------------------
do_mount()
{
	config_open $y_config_neutrino
	fstype=`config_get_value "network_nfs_type_$1"`
	ip=`config_get_value "network_nfs_ip_$1"`
	local_dir=`config_get_value "network_nfs_local_dir_$1"`
	dir=`config_get_value "network_nfs_dir_$1"`
	options1=`config_get_value "network_nfs_mount_options1_$1"`
	options2=`config_get_value "network_nfs_mount_options2_$1"`
	username=`config_get_value "network_nfs_username_$1"`
	password=`config_get_value "network_nfs_password_$1"`

	# check options
	if [ "$options1" = "" ]
	then
		options1=options2
		options2=""
	fi

	# default options
	if [ "$options1" = "" ]
	then
		if [ "$options2" = "" ]
		then
			if [ "$fstype" = "0" ]
			then
				options1="ro,soft,udp"
				options2="nolock,rsize=8192,wsize=8192"
			fi
			if [ "$fstype" = "1" ]
			then
				options1="ro"
			fi
		fi
	fi
	# build mount command
	case "$fstype" in
		0) #nfs
			cmd="mount -t nfs $ip:$dir $local_dir -o $options1"
			;;
		1)
			cmd="mount -t cifs //$ip/$dir $local_dir -o username=$username,password=$password,unc=//$ip/$dir,$options1";
			;;
		2)
			cmd="lufsd none $local_dir -o fs=ftpfs,username=$username,password=$password,host=$ip,root=/$dir,$options1";
			;;
		default)
			echo "mount type not supported"
	esac

	if [ "$options2" != "" ]
	then
		cmd="$cmd,$options2"
	fi

	res=`$cmd`
	echo "$res"
	echo "view mounts"
	m=`mount`
	msg="mount cmd:$cmd<br><br>res=$res<br>view Mounts;<br>$m"
	y_format_message_html
}
# -----------------------------------------------------------
# unmount $1=local_dir
# -----------------------------------------------------------
do_unmount()
{
	umount $1
}
# -----------------------------------------------------------
# AutoMount
# deaktive mount "#" replaces "---" and "=" replaced ",,"
# -----------------------------------------------------------
do_automount_list()
{
	i="1"
	sel="checked='checked'"
	cat $1|sed -n /-fstype/p|\
	while read mountname options share
	do
		mountvalue=`echo "$mountname"|sed -e "s/#/---/g"`
		echo "<input type='radio' name='R1' value='$mountvalue' $sel/>$i: $mountname ($share)<br/>"
		sel=""
		i=`expr $i + 1`
	done
}
do_automount_getline()
{
	mountname=`echo "$2"|sed -e "s/---/#/g"`
	cat $1|sed -n "/^[#]*$mountname[^a-zA-Z0-9]/p"
}
# $1:filename, $2:mountname, $3-*:mountstring
do_automount_setline()
{
	if ! [ -e $1 ]; then
		cp /etc/auto.net $1
	fi
	filename=$1
	mountname=`echo "$2"|sed -e "s;---;;g"`
	shift 2
	mountset=`echo "$*"|sed -e "s;---;#;g" -e "s;,,;=;g"`
	cp $filename "$filename.old"
	chmod ou+rwss $filename
	ex=`cat $filename|sed -n "/^$mountname[^a-zA-Z0-9].*$/p"`
	if [ "$ex" = "" ]; then
		echo "$mountset" >>$filename
	else
		tmp=`cat "$filename"|sed -e "s;^$mountname[^a-zA-Z0-9].*$;$mountset;g"`
	echo "$tmp" >$filename
	fi

	kill -HUP `cat /var/run/automount.pid`
}
# -----------------------------------------------------------
# Execute shell command
# 1: directory 2: append [true|false] 3+: cmd
# -----------------------------------------------------------
do_cmd()
{
	cd $1
	pw1="$1"
	app="$2"
	shift 2

	if [ "$1" = "cd" ]
	then
		cd $2
	else
		tmp=`$*` #Execute command
	fi
	pw=`pwd`
	echo '<html><body><form name="o"><textarea name="ot">'
	echo "$pw1>$*"
	echo "$tmp"
	echo '</textarea></form>'
	echo '<script language="JavaScript" type="text/javascript">'
	if [ "$app" = "true" ]
	then
		echo 'parent.document.co.cmds.value += document.o.ot.value;'
	else
		echo 'parent.document.co.cmds.value = document.o.ot.value;'
	fi
	echo "parent.set_pwd('$pw');"
	echo 'parent.setCaretToEnd(parent.document.co.cmds);'
	echo 'parent.document.f.cmd.focus();'
	echo '</script></body></html>'
}
# -----------------------------------------------------------
# yInstaller
# un-tar uploaded file to /tmp. Execute included install.sh
# -----------------------------------------------------------
do_installer()
{
	# clean up
	if [ -s "$y_out_html" ]
	then
		rm $y_out_html
	fi

	if [ -s "$y_upload_file" ]
	then
		# unpack /tmp/upload.tmp
		cd $y_path_tmp
		tar -xf "$y_upload_file"
		rm $y_upload_file
		if [ -s "$y_install" ] #look for install.sh
		then		
			chmod 755 $y_install
			o=`$y_install` # execute
			rm -f $y_install # clean up
			if [ -s "$y_out_html" ] #html - output?
			then	
				echo '<html><head><link rel="stylesheet" type="text/css" href="/Y_Main.css">'
				echo "<meta http-equiv='refresh' content='0; $y_out_html'></head>"
				echo "<body><a href='$y_out_html'>If automatic forwarding does not go.</a>"
				echo '</body></html>'
#				cat $y_out_html
			else
				echo '<html><head><link rel="stylesheet" type="text/css" href="/Y_Main.css"></head>'
				echo '<body>'
				echo "$o"
				echo '</body></html>'
			fi
		else
			msg="$y_install not found"
			y_format_message_html
		fi
	else
		msg="Upload-Problem.<br>Try again, please."
		y_format_message_html
	fi
}
# -----------------------------------------------------------
# extention Installer $1=URL
# -----------------------------------------------------------
do_ext_installer()
{
	rm $y_upload_file
	wgetlog=`wget -O $y_upload_file $1 2>/tmp/err.log`
	if [ -s "$y_upload_file" ];then
		cd $y_path_tmp
		tar -xf "$y_upload_file"
		rm $y_upload_file
		if [ -s "$y_install" ] #look for install.sh
		then		
			chmod 755 $y_install
			o=`$y_install` # execute
			rm -f $y_install # clean up
			echo "ok: wget=$wgetlog"
		fi
	else
		e=`cat /tmp/err.log`
		echo "error: $y_install not found. wget=$wgetlog $e"
	fi
}
# -----------------------------------------------------------
# view /proc/$1 Informations
# -----------------------------------------------------------
proc()
{
	msg=`cat /proc/$1`
	msg="<b>proc: $1</b><br><br>$msg"
	y_format_message_html
}
# -----------------------------------------------------------
# wake up $1=MAC
# -----------------------------------------------------------
wol()
{
	msg=`etherwake $1`
	msg="<b>Wake on LAN $1</b><br><br>$msg"
	y_format_message_html
}
# -----------------------------------------------------------
# -----------------------------------------------------------
dofbshot()
{
	rm -r /tmp/a.png
	fbshot -q /tmp/a.png >/dev/null
	msg="<img src='' name="fb" id="fb">"
	msg="$msg <script language='JavaScript' type='text/javascript'>document.fb.src='/tmp/a.png?hash=' + Math.random();window.setTimeout('parent.do_ready()',1000)</script>"
	y_format_message_html_plain
}

# -----------------------------------------------------------
# Settings Backup/Restore
# -----------------------------------------------------------
do_settings_backup_restore()
{
	workdir="$y_path_tmp/y_save_settings"
	yI_Version="0.1"
	case "$1" in
		backup)
		mkdir $workdir >/dev/null
		cp -r $y_path_config $workdir >/dev/null
		t=`date +%y%m%d_%H%M%S`
		filename="$y_path_tmp/y_Save_Settings_$t.tar"
		cd $workdir
		tar -cvf $filename ./*  >/dev/null
		rm -r $workdir  >/dev/null
		echo "$filename"
		;;

		restore)
		msg="restore settings"
		if [ -s "$y_upload_file" ]
		then
			# unpack /tmp/upload.tmp
			cd $y_path_tmp
			tar -xf "$y_upload_file"
			rm $y_upload_file
			cp -rf ./config /var/tuxbox/
			rm -r ./config
			msg="$msg ok"
			y_format_message_html
			sync
			sync
			umount -a
			reboot -f
		else
			msg="$msg error: no upload file"
		fi
		y_format_message_html
		;;
	esac
}
# -----------------------------------------------------------
# Main
# -----------------------------------------------------------
#debug
echo "call:$*" >> "/tmp/debug.txt"
case "$1" in
	skin_set)		skin_set $2 ;;
	skin_get)		skin_get ;;
	image_upload)		image_upload ;;
	image_backup)		image_backup_mtd $2; echo "/tmp/flash_mtd$2.img" ;;
	image_flash)		shift 1; flash_mtd $* ;;
	image_flash_free_tmp)	rm -r /tmp/*.img ;;
	image_delete)		image_delete_download_page ;;
	bootlogo_upload)	bootlogo_upload ;;
	bootlogo_lcd_upload)	bootlogo_lcd_upload ;;
	ucodes_upload)		ucodes_upload $2 ;;
	zapit_upload)		zapit_upload $2 ;;
	kernel-stack)		msg=`dmesg`; y_format_message_html ;;
	ps)			msg=`ps -c`; y_format_message_html ;;
	free)			f=`free`; p=`df -h`; msg="RAM Memory use\n-------------------\n$f\n\nPartitions\n-------------------\n$p"
				y_format_message_html ;;
	yreboot)		yreboot; echo "Reboot..." ;;
	check_yWeb_conf) 	check_Y_Web_conf ;;
	rcsim)			rcsim $2 >/dev/null ;;
	domount)		shift 1; do_mount $* ;;
	dounmount)		shift 1; do_unmount $* ;;
	cmd)			shift 1; do_cmd $* ;;
	installer)		shift 1; do_installer $* ;;
	ext_installer)	shift 1; do_ext_installer $* ;;
	proc)			shift 1; proc $* ;;
	wol)			shift 1; wol $* ;;
	dofbshot)		dofbshot ;;
	get_update_version)	wget -O /tmp/version.txt "http://www.yjogol.de/download/Y_Version.txt" ;;
	settings_backup_restore)	shift 1; do_settings_backup_restore $* ;;
	exec_cmd)		shift 1; $* ;;
	automount_list)		shift 1; do_automount_list $* ;;
	automount_getline)	shift 1; do_automount_getline $* ;;
	automount_setline)	shift 1; do_automount_setline $* ;;
		
	timer_get_tvinfo)
		shift 1
		res=`wget -O /tmp/tvinfo.xml "http://www.tvinfo.de/share/vidac/rec_info.php?username=$1&password=$2"`
		if  ! [ -s /tmp/tvinfo.xml ]
		then
			res="$res File empty!"
		fi
		echo "$res"
		;;

	timer_get_klack)
		config_open $y_config_Y_Web
		url=`config_get_value "klack_url"`
		klack_url=`echo "$url"|sed -e 's/;/\&/g'`
		wget -O /tmp/klack.xml "$klack_url" ;;
		
	get_synctimer_channels)
		if [ -e "$y_path_config/channels.txt" ]
		then
			cat $y_path_config/channels.txt 
		else
			cat $y_path_httpd/channels.txt 
		fi
		;;

	get_extention_list)
		if [ -e "$y_path_config/extentions.txt" ]
		then
			cat $y_path_config/extentions.txt
		else
			cat $y_path_httpd/extentions.txt
		fi
		;;

	write_extention_list)
		shift 1
		echo  "$*" >$y_path_config/extentions.txt
		;;
	
	url_get)
		shift 1
		res=`wget -O /tmp/$2 "$1" >/tmp/url.log`
		cat /tmp/$2
		;;

	standby_status)
		status=`switch -s|grep "FNC: 0"`
		if [ "$status" = "" ]
		then
			echo "off"
		else
			echo "on"
		fi
		;;

	*)
		echo "[Y_Tools.sh] Parameter falsch: $*" ;;
esac
