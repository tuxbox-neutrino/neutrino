#!/bin/sh
# -----------------------------------------------------------
# Y Globals (yjogol)
# $Date$
# $Revision$
# -----------------------------------------------------------

# -----------------------------------------------------------
# Definitions
# -----------------------------------------------------------
boxtype="coolstream"

# -----------------------------------------------------------
# Pathes
# -----------------------------------------------------------
#y_path_httpd="/share/tuxbox/neutrino/httpd-y"
y_path_httpd=".."
y_path_scripts="$y_path_httpd/scripts"
y_path_usrbin="/var/bin"
y_path_config="/var/tuxbox/config"
y_path_tmp="/tmp"
y_path_ucodes="/var/tuxbox/ucodes"
y_path_zapit="/var/tuxbox/config/zapit"

y_path_plugin_tuxnew="/var/tuxbox/config/tuxnews"

y_url_control="http://localhost/control"

# -----------------------------------------------------------
# Files
# -----------------------------------------------------------
y_config_Y_Web="$y_path_config/Y-Web.conf"
y_config_vnc="$y_path_config/vnc.conf"
y_config_nhttpd="$y_path_config/nhttpd.conf"
y_config_neutrino="$y_path_config/neutrino.conf"
y_upload_file="$y_path_tmp/upload.tmp"
y_boot_logo="/var/tuxbox/boot/logo-fb"
y_boot_logo_lcd="/var/tuxbox/boot/logo-lcd"
y_tmp="$y_path_tmp/y.tmp"
y_wait_live="$y_path_httpd/Y_Live_Wait.yhtm"
y_tmp_m3u="$y_path_tmp/y.m3u"
y_tmp_pls="$y_path_tmp/y.pls"

y_out_html="$y_path_tmp/y_out.yhtm"
y_install="$y_path_tmp/install.sh"


