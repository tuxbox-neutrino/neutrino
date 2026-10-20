SUBDIRS = \
	css \
	favicon \
	img \
	info \
	js \
	languages \
	lcd4linux

installdir = $(PRIVATE_HTTPDDIR)

nobase_install_DATA = \
	extensions/README.md \
	extensions/README.de.md

install_DATA = \
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
	version.txt

install-data-hook:
	@# what an earlier version installed and this one no longer has
	$(SHELL) $(srcdir)/remove-old-files.sh $(srcdir)/old-files.txt $(DESTDIR)$(PRIVATE_HTTPDDIR)
	@# every extension carried along, by its folder alone, as it is
	$(SHELL) $(srcdir)/install-extensions.sh $(srcdir)/extensions $(DESTDIR)$(PRIVATE_HTTPDDIR)/extensions
	@# name the scripts and styles the pages load by their content
	$(SHELL) $(srcdir)/stamp-versions.sh $(DESTDIR)$(PRIVATE_HTTPDDIR)

uninstall-hook:
	$(SHELL) $(srcdir)/install-extensions.sh --remove $(srcdir)/extensions $(DESTDIR)$(PRIVATE_HTTPDDIR)/extensions

EXTRA_DIST = stamp-versions.sh install-extensions.sh remove-old-files.sh old-files.txt extensions
