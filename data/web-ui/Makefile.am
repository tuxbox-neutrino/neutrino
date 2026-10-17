SUBDIRS = \
	css \
	favicon \
	images \
	img \
	info \
	jquery \
	js \
	languages \
	lcd4linux \
	scripts \
	styles

installdir = $(PRIVATE_HTTPDDIR)

nobase_install_DATA = \
	extensions/README.md \
	extensions/README.de.md \
	swagger/index.html

install_DATA = \
	bouquet-editor.yhtm \
	bouquets.yhtm \
	dashboard.yhtm \
	epg.yhtm \
	files.yhtm \
	login.html \
	password.html \
	screenshot.yhtm \
	settings.yhtm \
	sysinfo.yhtm \
	timer.yhtm \
	recordings.yhtm \
	tools.yhtm \
	favicon.ico \
	index.html \
	wizard.yhtm \
	robots.txt \
	Y_Baselib.js \
	Y_Blocks.txt \
	Y_Lang.yjs \
	Y_Main.css \
	Y_Settings_Infoicons.yhtm \
	Y_Settings_Plugins_hide.yhtm \
	Y_Tools_NetFS.yhtm \
	Y_Tools_NetFS_edit.yhtm \
	Y_Tools_NetFS_init.yhtm \
	Y_User.css \
	Y_Version.txt

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
