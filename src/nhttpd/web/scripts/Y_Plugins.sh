#!/bin/sh
# -----------------------------------------------------------
# Plugins (yjogol)
# $Date: 2006/09/16 14:52:23 $
# $Revision: 1.1 $
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
# Main
# -----------------------------------------------------------

case "$1" in
	skin_set)
		skin_set $2 ;;

	skin_get)
		skin_get ;;
	*)
		echo "Parameter falsch: $*" ;;
esac

