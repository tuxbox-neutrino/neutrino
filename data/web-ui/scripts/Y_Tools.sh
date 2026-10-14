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

getLanguage()
{
	Y_LANG=$(cat %(CONFIGDIR)/nhttpd.conf | grep "^Language.selected=" | cut -d= -f2)
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

# -----------------------------------------------------------
# Main
# -----------------------------------------------------------
# debug
#echo "call:$*" >> "/tmp/debug.txt"
action=$1; shift

case "$action" in
	getline)
		FILE=$1
		LINE=${2:-1}
		if [ -f $FILE ]; then
			tmp=$(sed -ne ''"${LINE}"'p' $FILE)
			printf "%s" "$tmp"
		fi
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