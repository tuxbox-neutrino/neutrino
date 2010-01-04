SUBDIRS = images scripts

installdir = $(DATADIR)/neutrino/httpd-y

%.gz: %
	gzip $< -c > $@

all: channels.js.gz Y_Baselib.js.gz Y_VLC.js.gz

install_DATA =	Y_About.yhtm Y_Boxcontrol_Bouquet_Editor_Edit.yhtm \
		Y_Boxcontrol_Bouquet_Editor_Main.yhtm \
		Y_Boxcontrol_Bouquetlist.yhtm Y_Boxcontrol_Bouquets.yhtm \
		Y_Boxcontrol_Channellist.yhtm Y_Boxcontrol_Menue.yhtm \
		Y_Boxcontrol_Messages.yhtm Y_Dyn_Pages.yhtm Y_EPG_Plus.yhtm \
		Y_Ext_Menue.yhtm Y_Ext_Settings.yhtm \
		Y_Ext_Update_refresh.yhtm Y_Ext_Update.yhtm Y_Info_Help.yhtm \
		Y_Info_Menue.yhtm Y_Info_Updates.yhtm Y_Live_DoubleView.yhtm \
		Y_Live_EPG_Info.yhtm Y_Live_EPG.yhtm Y_Live_Menue.yhtm \
		Y_Live_Msg.yhtm Y_Live_Record.yhtm Y_Live_Transcode.yhtm \
		Y_LiveViewFull.yhtm Y_Live.yhtm Y_Menue.yhtm \
		Y_Settings_automount_liste.yhtm Y_Settings_automount.yhtm \
		Y_Settings_Backup.yhtm Y_Settings_Live.yhtm \
		Y_Settings_Menue.yhtm Y_Settings_mount_liste.yhtm \
		Y_Settings_mount.yhtm Y_Settings_neutrino_forms.yhtm \
		Y_Settings_nhttpd.yhtm Y_Settings_Skin.yhtm \
		Y_Settings_Timer.yhtm Y_Settings_ucodes.yhtm \
		Y_Settings_umount_liste.yhtm Y_Settings_VNC.yhtm \
		Y_Settings_wol.yhtm Y_Settings.yhtm Y_Settings_yWeb.yhtm \
		Y_Settings_zapit.yhtm Y_StreamInfo.yhtm Y_Timer_Edit.yhtm \
		Y_Timer_List.yhtm Y_Timer_Menue.yhtm Y_Tools_Bootlogo.yhtm \
		Y_Tools_Boxcontrol.yhtm Y_Tools_Check_Install.yhtm \
		Y_Tools_Cmd.yhtm Y_Tools_fbshot.yhtm \
		Y_Tools_Flash_Menue.yhtm Y_Tools_Flash_Upload.yhtm \
		Y_Tools_Info_Menue.yhtm Y_Tools_Installer.yhtm \
		Y_Tools_Menue.yhtm Y_Tools_NetStream.yhtm Y_Tools_Rcsim.yhtm \
		Y_Tools_remote_osd.yhtm Y_Tools_Timer_Sync.yhtm \
		Y_blank.htm \
		index.html \
		Y_Main.css \
		channels.js.gz Y_Baselib.js.gz Y_VLC.js.gz \
		channels.txt extentions.txt robots.txt Y_Blocks.txt \
		Y_neutrino_Blocks.txt Y_Version.txt \
		favicon.ico
