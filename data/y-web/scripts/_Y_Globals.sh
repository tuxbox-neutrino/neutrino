#!/bin/sh
# -----------------------------------------------------------
# Y Globals (yjogol)
# -----------------------------------------------------------

# -----------------------------------------------------------
# Definitions
# -----------------------------------------------------------
#boxtype="coolstream"

# -----------------------------------------------------------
# Paths
# -----------------------------------------------------------
#y_path_httpd="%(PRIVATE_HTTPDDIR)"
y_path_httpd=".."
y_path_scripts="$y_path_httpd/scripts"
y_path_bin="/bin"
y_path_usrbin="/usr/bin"
y_path_varbin="/var/bin"
y_path_sbin="/sbin"
y_path_usrsbin="/usr/sbin"
y_path_config="%(CONFIGDIR)"
y_path_tmp="/tmp"
y_path_install="$y_path_tmp/y-install"
y_path_zapit="%(ZAPITDIR)"

y_url_control="http://localhost/control"

# -----------------------------------------------------------
# Files
# -----------------------------------------------------------
y_config_Y_Web="$y_path_config/Y-Web.conf"
y_config_nhttpd="$y_path_config/nhttpd.conf"
y_config_neutrino="$y_path_config/neutrino.conf"
y_upload_file="$y_path_tmp/upload.tmp"
y_tmp="$y_path_tmp/y.tmp"
y_tmp_m3u="$y_path_tmp/y.m3u"
y_tmp_pls="$y_path_tmp/y.pls"

y_out_html="$y_path_tmp/y_out.yhtm"
y_install="$y_path_install/install.sh"
