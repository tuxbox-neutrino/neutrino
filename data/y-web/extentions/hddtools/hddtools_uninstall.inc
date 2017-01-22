# ===========================================================
# yWeb SDK: Extention Unnstaller Include
# Extention Name: hddtools
# by yjogol
# -----------------------------------------------------------
# $Date: 2007-12-29 10:11:37 $
# $Revision: 1.1 $
# ===========================================================

yI_updatename="yWeb-Extension hddtools (08.01.2008)"
yI_Ext_Tag="hddtools"
yI_Ext_Version="1-0-1"
yI_uninstall_log="$y_path_tmp/uninstall_hddtools.log"
yI_uninstaller="hddtools_uninstall.inc"

uninstall()
{
	rm $y_ywebover_dir/Y_Ext_hddtools.yhtm
	rm $y_ywebover_dir/scripts/Y_Ext_hddtools.sh
}
