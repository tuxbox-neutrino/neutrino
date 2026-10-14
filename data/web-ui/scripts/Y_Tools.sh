#!/bin/sh
# -----------------------------------------------------------
# Tools (yjogol)
# -----------------------------------------------------------

if echo "$*" | grep -q "%20"; then
	$0 $(echo "$*" | sed 's/%20/ /g')
	exit
fi

. %(PRIVATE_HTTPDDIR)/scripts/_Y_Globals.sh
. %(PRIVATE_HTTPDDIR)/scripts/_Y_Library.sh

BNAME=${0##*/}

PLUGIN_DIRS="%(PLUGINDIR) %(PLUGINDIR_VAR) %(PLUGINDIR_MNT) $(config_get_value_direct $y_config_neutrino 'plugin_hdd_dir')"

# -----------------------------------------------------------
# AutoMount
# deactivate mount "#" replaces "---" and "=" replaced ",,"
# -----------------------------------------------------------
do_automount_list()
{
	i="1"
	sel="checked='checked'"
	cat $1|sed /#/d|sed -n /-fstype/p|\
	while read mountname options share
	do
		mountvalue=`echo "$mountname"|sed -e "s/#/---/g"`
		echo "<input type='radio' name='R1' value='$mountvalue' $sel/>$i: $mountname ($share)<br/>"
		sel=""
		i=`expr $i + 1`
	done
}

# -----------------------------------------------------------
do_automount_getline()
{
	mountname=`echo "$2"|sed -e "s/---/#/g"`
	cat $1 | sed -n "/^[#]*$mountname[^a-zA-Z0-9]/p"
}

# -----------------------------------------------------------
# $1:filename, $2:mountname, $3-*:mountstring
# -----------------------------------------------------------
do_automount_setline()
{
	if ! [ -e $1 ]; then
		cp /var/etc/auto.net $1
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
# -----------------------------------------------------------
# yInstaller
# un-tar uploaded file to /tmp/y-install.
# Execute included install.sh.
# -----------------------------------------------------------
do_installer()
{
	# modern callers (installer extension, /api/v4 upload) pass the
	# uploaded archive as parameter (the dispatcher shifts the case
	# word away); legacy pages keep relying on the fixed upload.tmp
	# path written by the old upload handler
	if [ -n "$1" ] && [ -s "$1" ]
	then
		y_upload_file="$1"
	fi

	# clean up
	if [ -s "$y_out_html" ]
	then
		rm $y_out_html
	fi

	if [ -s "$y_upload_file" ]
	then
		mkdir -p $y_path_install
		cd $y_path_install
		tar -xf "$y_upload_file"
		rm $y_upload_file
		if [ -s "$y_install" ] # look for install.sh
		then
			chmod 755 $y_install
			o=`$y_install` # execute
			rm -f $y_install # clean up
			if [ -s "$y_out_html" ] # html - output?
			then
				echo '<html><head>'
				echo '<link rel="stylesheet" href="/Y_Main.css">'
				echo '<link rel="stylesheet" href="/Y_Dist.css">'
				echo '<link rel="stylesheet" href="/Y_User.css">'
				echo "<meta http-equiv='refresh' content='0; $y_out_html'>"
				echo '</head>'
				echo "<body><a href='$y_out_html'>If automatic forwarding does not go.</a>"
				echo '</body></html>'
				#cat $y_out_html
			else
				echo '<html><head>'
				echo '<link rel="stylesheet" href="/Y_Main.css">'
				echo '<link rel="stylesheet" href="/Y_Dist.css">'
				echo '<link rel="stylesheet" href="/Y_User.css">'
				echo '</head>'
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
# view /proc/$1 Informations
# -----------------------------------------------------------
proc()
{
	msg=`cat /proc/$1`
	msg="<b>proc: $1</b><br><br>$msg"
	y_format_message_html
}

restart_neutrino()
{
	echo "fixme"
	#kill -HUP `pidof neutrino`
}

getLanguage()
{
	Y_LANG=$(cat %(CONFIGDIR)/nhttpd.conf | grep "^Language.selected=" | cut -d= -f2)
}

file2msg() # $1 = file to show; $2 = short description; $3 = maketable or empty
{
	if [ -e $1 ]; then
		echo "ok ($1 found)"
		msg=$(cat $1)
		if [ ! "$msg" ]; then
			msg="no $2 available"
			y_format_message_html
			exit
		fi
		if [ "$3" == "maketable" ]; then
			maketable $1
		else
			y_format_message_html
		fi
	else
		echo "failed ($1 not found)"
	fi
}

maketable() # $1 = file to format
{
        # header
	echo "<div class='work_box'><div class='work_box_head'>"
	echo "<div class='work_box_head_h2'>Formatted results</div></div>"
	echo "<div class='work_box_body' style='overflow: auto; font-family: monospace'>"
	# body
	echo "<table style='border: 0 none;'>"
	while read _a _b _c _d _e _f _g _h _i _j _k _l _m _n _o _p
	do
cat << eo_tbody
	<tr>
		<td>$_a</td>
		<td>$_b</td>
		<td>$_c</td>
		<td>$_d</td>
		<td>$_e</td>
		<td>$_f</td>
		<td>$_g</td>
		<td>$_h</td>
		<td>$_i</td>
		<td>$_j</td>
		<td>$_k</td>
		<td>$_l</td>
		<td>$_m</td>
		<td>$_n</td>
		<td>$_o</td>
		<td>$_p</td>
	</tr>
eo_tbody
	done < $1
	echo "</table>"
	# footer
	echo "</div>"
}

is_mount()
{
        M=$1
        test -e "$M" && cd "$M" && M=$(pwd -P)
	while read _DEV _MTPT _FSTYPE _OPTS _REST
	do
		case "$M" in
			"netfs")
				if [ "$_FSTYPE" == "nfs" ] || [ "$_FSTYPE" == "cifs" ]; then
					RET=0; break
				else
					RET=1; continue
				fi
			;;
			"$_DEV"|"$_MTPT")
				if [ "$_FSTYPE" == "autofs" ]; then
					RET=1; continue
				else
					RET=0; break
				fi
			;;
			*)
				RET=1; continue
			;;
		esac
	done < /etc/mtab
	return $RET
}

is_exec()
{
        F=$1
	test -x $y_path_varbin/$F
	return $?
}

is_running()
{
	D=$1
	pidof $D >/dev/null 2>&1
	return $?
}

# -----------------------------------------------------------
# Main
# -----------------------------------------------------------
# debug
#echo "call:$*" >> "/tmp/debug.txt"
action=$1; shift

case "$action" in
	kernel-stack)			msg=`dmesg`; y_format_message_html ;;
	ps)				msg=`ps aux`; y_format_message_html ;;
	free)				f=`free`; p=`df -h`; msg="RAM Memory use\n-------------------\n$f\n\nPartitions\n-------------------\n$p"; y_format_message_html ;;
	yreboot)			reboot; echo "Reboot..." ;;
	check_yWeb_conf) 		check_yWeb_conf ;;
	installer)			do_installer $* 2>&1 ;;
	proc)				proc $* ;;
	automount_list)			do_automount_list $* ;;
	automount_getline)		do_automount_getline $* ;;
	automount_setline)		do_automount_setline $* ;;
	restart_neutrino)		restart_neutrino ;;
	have_plugin_scripts)	 	find %(PLUGINDIR_VAR) -name '*.sh' ;;

	restart_sectionsd)
		killall sectionsd
		sectionsd >/dev/null 2>&1
		msg="sectionsd reboot. ok."
		y_format_message_html
	;;
	get_synctimer_channels)
		if [ -e "$y_path_config/channels.txt" ]
		then
			cat $y_path_config/channels.txt
		else
			cat $y_path_httpd/channels.txt
		fi
	;;
	mtd_space)
		df | while read filesystem blocks used available percent mounted ; do
			case "$filesystem" in
				mtd*)
					echo "$filesystem" "$blocks" "$used" "$available" "$percent" "$mounted"
					break
				;;
			esac
			case $mounted in
				"/")
					echo "$filesystem" "$blocks" "$used" "$available" "$percent" "$mounted"
					break
				;;
			esac
		done
	;;
	tmp_space)
		df /tmp|grep /tmp
	;;
	get_hostname)
		echo -n $(hostname)
	;;
	getline)
		FILE=$1
		LINE=${2:-1}
		if [ -f $FILE ]; then
			tmp=$(sed -ne ''"${LINE}"'p' $FILE)
			printf "%s" "$tmp"
		fi
	;;
	is_exec)
		FILE=$1
		is_exec $FILE && printf "%s" "true" || printf "%s" "false"
	;;
	is_running)
		DEAMON=$1
		is_running $DEAMON && printf "%s" "true" || printf "%s" "false"
	;;
	get_update_txt)
		# Update check disabled - no generic update server available
		echo "version=n/a" > /tmp/update.txt
	;;
	rm_update_txt)
		rm -f /tmp/update.txt
	;;
	get_flash_info)
		MTPT=""
		case "$1" in
			"var")		MTPT="/var"	;;
			"root"|*)	MTPT="/"	;;
		esac
		df ${MTPT} | while read fs total used free used_percent mtpt; do
			case ${mtpt} in
				${MTPT})
					used_percent=${used_percent%\%}
					free_percent=$(($free*100/$total))
					total=$(($total/1024))
					used=$(($used/1024))
					free=$(($free/1024))
					case $2 in
						used_percent)	printf "%d" "$used_percent";;
						free_percent)	printf "%d" "$free_percent";;
						total)		printf "%d" "$total";;
						used)		printf "%d" "$used";;
						free)		printf "%d" "$free";;
					esac
					break
				;;
			esac
		done
	;;
	get_mem_info)
		while read _desc _size _unit; do
			case $_desc in
				"MemTotal:")	total=$_size	;;
				"MemFree:")	free=$_size	;;
				"Buffers:")	buffers=$_size	;;
				"Cached:")	cached=$_size	;;
			esac
		done < /proc/meminfo
		free=$(($free+$buffers+$cached))
		used=$(($total-$free))
		#used_percent=$(($used*100/$total))
		used_percent=$((($used*200+total)/2/$total))
		free_percent=$(($free*100/$total))
		total=$(($total/1024))
		used=$(($used/1024))
		free=$(($free/1024))
		case $1 in
			used_percent)	printf "%d" "$used_percent";;
			free_percent)	printf "%d" "$free_percent";;
			total)		printf "%d" "$total";;
			used)		printf "%d" "$used";;
			free)		printf "%d" "$free";;
		esac
	;;
	get_mtd_info)
		while read dev size erasesize name; do
			case "$dev" in
				mtd*)
					test "$1" = "dev" && printf "%s<br/>" "$dev"
					test "$1" = "name" && printf "%s<br/>" "$(echo "$name" | tr -d '"')"
				;;
			esac
		done < /proc/mtd
	;;
	get_cpu_info)
		for i in 1 2; do
			while read _cpu _user _nice _system _idle _rest; do
				case $_cpu in
					cpu)
						case $i in
							1)
								fst_all=$(($_user + $_nice + $_system + $_idle))
								fst_idle=$_idle
							;;
							2)
								snd_all=$(($_user + $_nice + $_system + $_idle))
								snd_idle=$_idle
							;;
						esac
					;;
				esac
			done < /proc/stat
			test $i = 1 && sleep 1
		done

		diff_all=$(($snd_all - $fst_all))
		diff_idle=$(($snd_idle - $fst_idle))
		_idle_percent=$(($diff_idle * 100 / $diff_all))
		_used_percent=$((100 - $_idle_percent))

		case $1 in
			used_percent)	printf "%s" "$_used_percent";;
		esac
	;;

	# camds
	#softcamstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh stop;;
	#softcamstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh start;;
	softcamreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh reset;;
	mgcamdstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh mgcamd_stop;;
	mgcamdstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh mgcamd_start;;
	mgcamdreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh mgcamd_reset;;
	gboxstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh gbox_stop;;
	gboxstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh gbox_start;;
	gboxreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh gbox_reset;;
	oscamstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh oscam_stop;;
	oscamstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh oscam_start;;
	oscamreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh oscam_reset;;
	osmodstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh osmod_stop;;
	osmodstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh osmod_start;;
	osmodreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh osmod_reset;;
	ncamstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh ncam_stop;;
	ncamstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh ncam_start;;
	ncamreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh ncam_reset;;
	cccamstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh cccam_stop;;
	cccamstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh cccam_start;;
	cccamreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh cccam_reset;;
	doscamstop)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh doscam_stop;;
	doscamstart)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh doscam_start;;
	doscamreset)	%(PRIVATE_HTTPDDIR)/scripts/Y_Camds.sh doscam_reset;;

	vinfo)
	        if ! [ -x $y_path_varbin/$1 ]; then
			printf "%s" "n/a"
			exit
	        fi
		CAM=$(echo $1 | tr [:lower:] [:upper:])
		if [ "$CAM" = "GBOX" ]; then
			CAM=GBOX.NET
		fi
		V=$(vinfo $CAM $y_path_varbin/$1)
		printf "%s" "$V" | sed 's/keine Informationen gefunden//g'
	;;

	# syntax	file2msg file "short-description" (maketable)
	mgshareinfo)	file2msg /tmp/mgshare.info "n-lines" maketable;;
	mginfo)		file2msg /tmp/mg.info "cs-info" maketable;;
	cccamdinfo)	file2msg /tmp/cccamd.info "c-lines" maketable;;
	shareonl)	file2msg /tmp/share.onl "online-share-info" maketable;;
	shareinfo)	file2msg /tmp/share.info "cs-info" maketable;;
	sc01info)	file2msg /tmp/sc01.info "slot1-info";;
	sc02info)	file2msg /tmp/sc02.info "slot2-info";;
	ecminfo)	file2msg /tmp/ecm.info "ecm-info";;
	pidinfo)	file2msg /tmp/pid.info "pid-info";;
	oscamversion)	file2msg /tmp/.oscam/oscam.version "oscam-info";;
	ncamversion)	file2msg /tmp/.ncam/ncam.version "ncam-info";;
	ccccamversion)	file2msg /tmp/.cccam/cccam.version "cccam-info";;
	doscamversion)	file2msg /tmp/.doscam/doscam.version "doscam-info";;

	get_oscam_webif_port)
		if [ -e %(CONFIGDIR)/oscam.conf ]; then
			_port=$(grep -m 1 -i "^[:space:]*httpport" %(CONFIGDIR)/oscam.conf | cut -d'=' -f2)
			_port=$(echo $_port | dos2unix -u)
		fi
		printf "%s" ${_port:-8080}
	;;

	get_osmod_webif_port)
		if [ -e %(CONFIGDIR)/oscam.conf ]; then
			_port=$(grep -m 1 -i "^[:space:]*httpport" %(CONFIGDIR)/oscam.conf | cut -d'=' -f2)
			_port=$(echo $_port | dos2unix -u)
		fi
		printf "%s" ${_port:-8888}
	;;

	get_ncam_webif_port)
		if [ -e %(CONFIGDIR)/ncam.conf ]; then
			_port=$(grep -m 1 -i "^[:space:]*httpport" %(CONFIGDIR)/ncam.conf | cut -d'=' -f2)
			_port=$(echo $_port | dos2unix -u)
		fi
		printf "%s" ${_port:-8181}
	;;

	get_cccam_webif_port)
		if [ -e %(CONFIGDIR)/CCcam.cfg ]; then
			_port=$(grep -m 1 -i "^[:space:]*WEBINFO LISTEN PORT" /var/etc/CCcam.cfg | cut -d':' -f2)
			_port=$(echo $_port | dos2unix -u)
		fi
		printf "%s" ${_port:-16001}
	;;

	get_doscam_webif_port)
		if [ -e %(CONFIGDIR)/doscam.cfg ]; then
			_port=$(grep -m 1 -i "^[:space:]*httpport" %(CONFIGDIR)/doscam.cfg | cut -d'=' -f2)
			_port=$(echo $_port | dos2unix -u)
		fi
		printf "%s" ${_port:-8080}
	;;

	get_minidlnad_webif_port)
		if [ -e /etc/minidlna.conf ]; then
			_port=$(grep -m 1 "^[:space:]*port=" /etc/minidlna.conf | cut -d'=' -f2)
			_port=$(echo $_port | dos2unix -u)
		fi
		printf "%s" $_port
	;;

	get_xupnpd_webif_port)
		if [ -e /share/xupnpd/xupnpd.lua ]; then
			_port=$(grep -m 1 "^[:space:]*cfg.http_port" /share/xupnpd/xupnpd.lua | cut -d'=' -f2)
			_port=$(echo $_port | dos2unix -u)
		fi
		printf "%s" $_port
	;;

	countcards)
		getLanguage
		if [ "$Y_LANG" == "Deutsch" ]
		then
			Y_L_count="Karten sind online."
			Y_L_recount="können weitergegeben werden."
		else
			Y_L_count="cards online."
			Y_L_recount="can be shared."
		fi
		FILE=$1
		COUNT=0
		_DIST=""
		_LEV=""
		if [ -f $FILE ]; then
			COUNT=$(cat $FILE | wc -l | sed 's/^ *//')
			echo "<b>$COUNT</b> $Y_L_count"
			test $COUNT = 0 && exit

			RECOUNT=$(cat $FILE | grep -v Lev:0 | wc -l | sed 's/^ *//')
			echo "<b>$RECOUNT</b> $Y_L_recount"

			for i in $(seq 0 9); do
				DIST=$(cat $FILE | grep -H dist:$i | wc -l | sed 's/^ *//')
				test $DIST = 0 || _DIST=$(echo -e "$_DIST\n\tDist. $i: $DIST")
			done
			for i in $(seq 0 9); do
				LEV=$(cat $FILE | grep -H Lev:$i | wc -l | sed 's/^ *//')
				test $LEV = 0 || _LEV=$(echo -e "$_LEV\n\tLevel $i: $LEV")
			done

			echo "<pre>$_LEV<br/>$_DIST</pre>"
		else
			echo "failed ($FILE not found)"
			exit
		fi

		test $COUNT = 0 && echo "Es sind derzeit keine Karten online!"
	;;

	# netfs
	is_mount)
		MTPT=$1
		is_mount $MTPT && printf "%s" "true" || printf "%s" "false"
	;;
	do_mount_all)
		msg=$(/etc/init.d/fstab start_netfs)
		y_format_message_html
	;;
	do_umount_all)
		msg=$(/etc/init.d/fstab stop_netfs)
		y_format_message_html
	;;
	do_mount)
	        MTPT=$1
	        test $MTPT || exit 1
		test -d $MTPT || mkdir -p $MTPT;
		FLAG="/var/etc/.srv"
		if OUT=$(mount $MTPT 2>&1 >/dev/null); then
			RET=$?
			msg="mount: $MTPT - success ($RET)"
			test -e $FLAG || touch $FLAG
		else
			RET=$?
			msg="mount: $MTPT - failed ($RET)<br>$OUT"
		fi
		y_format_message_html
	;;
	do_umount)
	        MTPT=$1
	        test $MTPT || exit 1
		FLAG="/var/etc/.srv"
		rm -f $FLAG
		if OUT=$(umount $MTPT 2>&1 >/dev/null); then
			RET=$?
			msg="umount: $MTPT - success ($RET)"
		else
			RET=$?
			msg="umount: $MTPT - failed ($RET)<br>$OUT"
		fi
		is_mount netfs && touch $FLAG
		y_format_message_html
	;;

	# automounter
	do_autofs)
		case $1 in
			start|stop|restart|reload)
				msg=$(service autofs $1)
				y_format_message_html
			;;
		esac
	;;

	# infoicons
	rm_icons_flag)
		while [ $1 ]; do
			if [ -e $1 ]; then
				rm -f $1 && echo "ok ($1 removed)<br />" || echo "failed (can't remove $1)<br />"
			else
				echo "nothing to do ($1 not found)<br />"
			fi
			shift
		done
	;;
	touch_icons_flag)
		while [ $1 ]; do
			if [ ! -e $1 ]; then
				touch $1 && echo "ok ($1 created)<br />" || echo "failed (can't create $1)<br />"
			else
				echo "nothing to do ($1 always exist)<br />"
			fi
			shift
		done
	;;
	read_icons_flag)
		test -z $1 && exit 1
		test -e $1 && S=$(cat $1)
		printf "%s" "$S"
	;;

	# plugins show/hide
	p_show)
		PLUGIN=$1
		for PLUGIN_DIR in $PLUGIN_DIRS
		do
			cd $PLUGIN_DIR
			test -w "$PLUGIN" || continue
			echo "[$BNAME] modifying $PLUGIN_DIR/$PLUGIN"
			sed -i "s/^\(hide=\).*$/\1"0"/" $PLUGIN
		done
	;;
	p_hide)
		PLUGIN=$1
		for PLUGIN_DIR in $PLUGIN_DIRS
		do
			cd $PLUGIN_DIR
			test -w "$PLUGIN" || continue
			echo "[$BNAME] modifying $PLUGIN_DIR/$PLUGIN"
			# fix for missing trailing linefeed in cfg-file
			test "$(tail -c1 $PLUGIN)" != "" && echo "" >> $PLUGIN
			if grep -q "^hide=" $PLUGIN
			then
				sed -i "s/^\(hide=\).*$/\1"1"/" $PLUGIN
			else
				echo "hide=1" >> $PLUGIN
			fi
		done
	;;
	p_list)
		getLanguage
		if [ "$Y_LANG" == "Deutsch" ]
		then
			Y_L_show="Anzeigen"
			Y_L_hide="Verstecken"
		else
			Y_L_show="Show"
			Y_L_hide="Hide"
		fi
		for PLUGIN_DIR in $PLUGIN_DIRS
		do
			test -e $PLUGIN_DIR || continue
			cd $PLUGIN_DIR
			PLUGINS=$(ls -1 *.cfg 2>/dev/null)
			for PLUGIN in $PLUGINS
			do
				if [ "$1" == "lua" ]
				then
					# lua-plugins don't need the executable flag
					test -e ${PLUGIN%%.*}.${1} || continue
				else
					test -x ${PLUGIN%%.*}.${1} || continue
				fi
				NAME=$(cat $PLUGIN | grep "^name=" | cut -d= -f2)
				HIDE=$(cat $PLUGIN | grep "^hide=" | cut -d= -f2)
				case $HIDE in
					1)
						IMG="<img src='images/x_red.png' class='status'>"
						INP="<input type='button' value='$Y_L_show' onclick='Y_Tools(\"p_show $PLUGIN\", 1000);' />"
					;;
					*)
						IMG="<img src='images/check_green.png' class='status'>"
						INP="<input type='button' value='$Y_L_hide' onclick='Y_Tools(\"p_hide $PLUGIN\", 1000);' />"
					;;
				esac
cat << eoPLUGIN
			<!-- $NAME -->
			<tr class="list">
				<td>
					$IMG
					<span title="$PLUGIN_DIR/${PLUGIN%%.*}.${1}">
						$NAME
					</span>
				</td>
				<td>
					$INP
				</td>
			</tr>
eoPLUGIN
			done
		done
	;;
	*)
		echo "[$BNAME] Parameter wrong: $action $*"
	;;
esac