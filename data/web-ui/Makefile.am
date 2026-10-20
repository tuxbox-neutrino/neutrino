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
	extensions/README.de.md

install_DATA = \
	bouquet-editor.html \
	bouquets.html \
	dashboard.html \
	epg.html \
	login.html \
	password.html \
	settings.html \
	timer.html \
	recordings.html \
	tools.html \
	favicon.ico \
	index.html \
	wizard.html \
	robots.txt \
	Y_Baselib.js \
	Y_Blocks.txt \
	Y_Main.css \
	Y_Settings_Infoicons.yhtm \
	Y_Settings_Plugins_hide.yhtm \
	Y_Tools_NetFS.yhtm \
	Y_Tools_NetFS_edit.yhtm \
	Y_Tools_NetFS_init.yhtm \
	Y_User.css \
	version.txt

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
	@# every extension carried along, by its folder alone (after the
	@# placeholders above: an extension goes in as it is)
	$(SHELL) $(srcdir)/install-extensions.sh $(srcdir)/extensions $(DESTDIR)$(PRIVATE_HTTPDDIR)/extensions
	@# name the scripts and styles the pages load by their content
	$(SHELL) $(srcdir)/stamp-versions.sh $(DESTDIR)$(PRIVATE_HTTPDDIR)

uninstall-hook:
	$(SHELL) $(srcdir)/install-extensions.sh --remove $(srcdir)/extensions $(DESTDIR)$(PRIVATE_HTTPDDIR)/extensions

EXTRA_DIST = stamp-versions.sh install-extensions.sh extensions
