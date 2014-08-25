# ===========================================================
# yWeb SDK: Extention Installer Include
# Extention Name: boxinfo
# by yjogol
# -----------------------------------------------------------
# $Date: 2007-12-29 10:11:37 $
# $Revision: 1.1 $
# ===========================================================

yI_updatename="yWeb-Extension boxinfo (08.01.2008)"
yI_Ext_Tag="boxinfo"
yI_Ext_Version="1-0-1"
yI_install_log="$y_path_tmp/install_boxinfo.log"
yI_uninstaller="boxinfo_uninstall.sh"

install() 
{
	cp -r /tmp/files/httpd/* $y_ywebover_dir
	chmod 755 $y_ywebover_dir/scripts/*.sh
}
