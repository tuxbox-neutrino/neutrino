# ===========================================================
# yWeb SDK: Extention Unnstaller Include
# Extention Name: boxinfo
# by yjogol
# -----------------------------------------------------------
# $Date: 2007-12-29 17:08:16 $
# $Revision: 1.2 $
# ===========================================================

yI_updatename="yWeb-Extension boxinfo (08.01.2008)"
yI_Ext_Tag="boxinfo"
yI_Ext_Version="1-0-1"
yI_uninstall_log="$y_path_tmp/uninstall_boxinfo.log"
yI_uninstaller="boxinfo_uninstall.inc"

uninstall()
{
	rm $y_ywebover_dir/Y_Ext_BoxInfo.yhtm
	rm $y_ywebover_dir/scripts/Y_Ext_BoxInfo.sh
}
