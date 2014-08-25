# ===========================================================
# yWeb SDK: Extention Unnstaller Include
# Extention Name: filemgr
# by yjogol
# -----------------------------------------------------------
# $Date: 2007-12-29 17:08:16 $
# $Revision: 1.3 $
# ===========================================================

yI_updatename="yWeb-Extension filemgr (02.02.2008)"
yI_Ext_Tag="filemgr"
yI_Ext_Version="1-0-0"
yI_uninstall_log="$y_path_tmp/uninstall_filemgr.log"
yI_uninstaller="filemgr_uninstall.inc"

uninstall()
{
	rm $y_ywebover_dir/Y_Ext_Filemgr.yhtm
	rm $y_ywebover_dir/Y_Ext_Filemgr.css
	rm $y_ywebover_dir/Y_Ext_Filemgr_Edit.yhtm
	rm $y_ywebover_dir/Y_Ext_Filemgr_blocks.txt
#	rm $y_ywebover_dir/accordion.js
#	rm $y_ywebover_dir/effects.js
	rm $y_ywebover_dir/scripts/Y_NAS*
	rm $y_ywebover_dir/images/chmod.png
	rm $y_ywebover_dir/images/copy.gif
	rm $y_ywebover_dir/images/cut.png
#	rm $y_ywebover_dir/images/delete.gif
	rm $y_ywebover_dir/images/dir_up.gif
	rm $y_ywebover_dir/images/download.png
	rm $y_ywebover_dir/images/folder_add.png
	rm $y_ywebover_dir/images/ftype_file.png
	rm $y_ywebover_dir/images/ftype_folder.png
	rm $y_ywebover_dir/images/ftype_link.png
	rm $y_ywebover_dir/images/home.png
	rm $y_ywebover_dir/images/package.png
	rm $y_ywebover_dir/images/page_edit.png
	rm $y_ywebover_dir/images/paste.gif
	rm $y_ywebover_dir/images/picture.png
	rm $y_ywebover_dir/images/rename.gif
	rm $y_ywebover_dir/images/text.png
	rm $y_ywebover_dir/images/upload.png
	rm $y_ywebover_dir/images/work.gif
}
