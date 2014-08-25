# ===========================================================
# yWeb SDK: Extention Installer Include
# Extention Name: filemgr
# by yjogol
# -----------------------------------------------------------
# $Date: 2007-12-29 10:11:37 $
# $Revision: 1.2 $
# ===========================================================

yI_updatename="yWeb-Extension filemgr (02.02.2008)"
yI_Ext_Tag="filemgr"
yI_Ext_Version="1-0-0"
yI_install_log="$y_path_tmp/install_filemgr.log"
yI_uninstaller="filemgr_uninstall.sh"

install() 
{
	cp -r /tmp/files/httpd/* $y_ywebover_dir
	chmod 755 $y_ywebover_dir/scripts/*.sh
}
