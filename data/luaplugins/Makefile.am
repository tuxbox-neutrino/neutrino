installdir = $(LUAPLUGINDIR)

install_SCRIPTS = \
	nlab.lua

install-data-hook:
	ln -sf $(LUAPLUGINDIR)/nlab.lua $(DESTDIR)$(bindir)/nlab

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/nlab
