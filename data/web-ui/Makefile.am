SUBDIRS = \
	css \
	favicon \
	images \
	info \
	jquery \
	languages \
	lcd4linux \
	prototype \
	scripts \
	styles

installdir = $(PRIVATE_HTTPDDIR)

nobase_install_DATA = \
	extensions/README.md \
	extensions/camds/manifest.json \
	extensions/hallo-yweb/manifest.json \
	extensions/hallo-yweb/hallo.html \
	extensions/infoicons/manifest.json

install_DATA = \
	bouquet-editor.yhtm \
	bouquets.yhtm \
	cmd.yhtm \
	dashboard.yhtm \
	epg.yhtm \
	files.yhtm \
	m3u-convert.yhtm \
	screenshot.yhtm \
	set-lang.yhtm \
	settings.yhtm \
	settings-registry.js \
	sysinfo.yhtm \
	timer.yhtm \
	tools.yhtm \
	extensions.txt \
	favicon.ico \
	index.html \
	index_legacy.html \
	wizard.yhtm \
	robots.txt \
	Y_About_Image.yhtm \
	Y_About_yWeb.yhtm \
	Y_Baselib.js \
	Y_Blank.yhtm \
	Y_Blocks.txt \
	Y_Blocks_Neutrino.txt \
	Y_Boxcontrol_Bouquet_Editor_Edit.yhtm \
	Y_Boxcontrol_Bouquet_Editor_Main.yhtm \
	Y_Boxcontrol_Bouquetlist.yhtm \
	Y_Boxcontrol_Bouquets.yhtm \
	Y_Boxcontrol_Channellist.yhtm \
	Y_Boxcontrol_Menue.yhtm \
	Y_Boxcontrol_Messages.yhtm \
	Y_Boxcontrol_PZapit.yhtm \
	Y_Dist.css \
	Y_Dyn_Pages.yhtm \
	Y_EPG.js \
	Y_EPG_Plus.yhtm \
	Y_Ext_Menue.yhtm \
	Y_Ext_Settings.yhtm \
	Y_Ext_Uninstall.yhtm \
	Y_Ext_Update.js \
	Y_Ext_Update.yhtm \
	Y_Ext_Update_refresh.yhtm \
	Y_Filemgr.yhtm \
	Y_Filemgr_Edit.yhtm \
	Y_Info_Help.yhtm \
	Y_Info_Menue.yhtm \
	Y_Lang.yjs \
	Y_Live_EPG.yhtm \
	Y_Live_EPG_Info.yhtm \
	Y_Main.css \
	Y_Menue.yhtm \
	Y_Refresh.yhtm \
	Y_Settings_Backup.yhtm \
	Y_Settings_Camds.yhtm \
	Y_Settings_Infoicons.yhtm \
	Y_Settings_Menue.yhtm \
	Y_Settings_Plugins.yhtm \
	Y_Settings_Plugins_hide.yhtm \
	Y_Settings_automount.yhtm \
	Y_Settings_automount_list.yhtm \
	Y_Settings_buttons.yhtm \
	Y_Settings_glcd.yhtm \
	Y_Settings_lcd.yhtm \
	Y_Settings_mount.yhtm \
	Y_Settings_mount_list.yhtm \
	Y_Settings_neutrino_forms.yhtm \
	Y_Settings_nhttpd.yhtm \
	Y_Settings_personalize.yhtm \
	Y_Settings_umount_list.yhtm \
	Y_Settings_video_audio.yhtm \
	Y_Settings_wol.yhtm \
	Y_Settings_xmltv.yhtm \
	Y_Settings_yWeb.yhtm \
	Y_Settings_zapit.yhtm \
	Y_StreamInfo.yhtm \
	Y_Timer_Edit.yhtm \
	Y_Timer_List.yhtm \
	Y_Tools_Boxcontrol.yhtm \
	Y_Tools_Check_Install.yhtm \
	Y_Tools_Cmd.yhtm \
	Y_Tools_Info_Menue.yhtm \
	Y_Tools_Installer.yhtm \
	Y_Tools_Menue.yhtm \
	Y_Tools_NetFS.yhtm \
	Y_Tools_NetFS_edit.yhtm \
	Y_Tools_NetFS_init.yhtm \
	Y_Tools_Rcsim.yhtm \
	Y_Tools_Screenshot.yhtm  \
	Y_Tools_Screenshot_GraphLCD.yhtm \
	Y_Tools_Screenshot_LCD4Linux.yhtm  \
	Y_Tools_m3uConvert.yhtm \
	Y_Tools_m3uConvert_help.yhtm \
	Y_User.css \
	Y_Version.txt \
	Y_Wait.yhtm \
	Y_yWeb.js \
	yweb-header.js

configdir = $(CONFIGDIR)
config_DATA = yWeb.conf

install-data-hook:
	@# ===========================================================================
	@# DEPRECATION NOTICE:
	@# The following build-time path placeholder replacements (e.g., %(CONFIGDIR),
	@# %(LOGODIR), etc.) are DEPRECATED and maintained only for backward compatibility.
	@#
	@# For new code in web interfaces and plugins, use the runtime API endpoints:
	@#   - /control/paths   - Returns all system paths as JSON
	@#   - /control/sysinfo - Returns system information as JSON
	@#
	@# Example migration:
	@#   Old: var configDir = "%(CONFIGDIR)";
	@#   New: const paths = await fetch('/control/paths').then(r => r.json());
	@#        const configDir = paths.configdir;
	@#
	@# See: doc/nhttpd/nhttpd_controlapi.html for full API documentation.
	@# ===========================================================================
	@printf '\033[1;33mNOTE:\033[0m Build-time path placeholders are deprecated. Use /control/paths API instead.\n'
	find $(DESTDIR)$(PRIVATE_HTTPDDIR)/ -type f -print0 | xargs --no-run-if-empty -0 \
		sed -i \
			-e 's|%(CONFIGDIR)|$(CONFIGDIR)|g' \
			-e 's|%(ZAPITDIR)|$(ZAPITDIR)|g' \
			-e 's|%(DATADIR)|$(DATADIR)|g' \
			-e 's|%(DATADIR_VAR)|$(DATADIR_VAR)|g' \
			-e 's|%(CONTROLDIR)|$(CONTROLDIR)|g' \
			-e 's|%(CONTROLDIR_VAR)|$(CONTROLDIR_VAR)|g' \
			-e 's|%(FONTDIR)|$(FONTDIR)|g' \
			-e 's|%(FONTDIR_VAR)|$(FONTDIR_VAR)|g' \
			-e 's|%(LIBDIR)|$(LIBDIR)|g' \
			-e 's|%(GAMESDIR)|$(GAMESDIR)|g' \
			-e 's|%(ICONSDIR)|$(ICONSDIR)|g' \
			-e 's|%(ICONSDIR_VAR)|$(ICONSDIR_VAR)|g' \
			-e 's|%(LOCALEDIR)|$(LOCALEDIR)|g' \
			-e 's|%(LOCALEDIR_VAR)|$(LOCALEDIR_VAR)|g' \
			-e 's|%(PLUGINDIR)|$(PLUGINDIR)|g' \
			-e 's|%(PLUGINDIR_MNT)|$(PLUGINDIR_MNT)|g' \
			-e 's|%(PLUGINDIR_VAR)|$(PLUGINDIR_VAR)|g' \
			-e 's|%(LUAPLUGINDIR)|$(LUAPLUGINDIR)|g' \
			-e 's|%(LUAPLUGINDIR_VAR)|$(LUAPLUGINDIR_VAR)|g' \
			-e 's|%(THEMESDIR)|$(THEMESDIR)|g' \
			-e 's|%(THEMESDIR_VAR)|$(THEMESDIR_VAR)|g' \
			-e 's|%(WEBRADIODIR)|$(WEBRADIODIR)|g' \
			-e 's|%(WEBRADIODIR_VAR)|$(WEBRADIODIR_VAR)|g' \
			-e 's|%(WEBTVDIR)|$(WEBTVDIR)|g' \
			-e 's|%(WEBTVDIR_VAR)|$(WEBTVDIR_VAR)|g' \
			-e 's|%(LOGODIR)|$(LOGODIR)|g' \
			-e 's|%(LOGODIR_VAR)|$(LOGODIR_VAR)|g' \
			-e 's|%(PRIVATE_HTTPDDIR)|$(PRIVATE_HTTPDDIR)|g' \
			-e 's|%(PUBLIC_HTTPDDIR)|$(PUBLIC_HTTPDDIR)|g' \
			-e 's|%(HOSTED_HTTPDDIR)|$(HOSTED_HTTPDDIR)|g' \
			-e 's|%(FLAGDIR)|$(FLAGDIR)|g' \
	;
	@# install dark style as default
	cp $(DESTDIR)$(PRIVATE_HTTPDDIR)/styles/Y_Dist-dark.css $(DESTDIR)$(PRIVATE_HTTPDDIR)/Y_Dist.css
