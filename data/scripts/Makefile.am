installdir = $(bindir)

install_DATA = \
	backup.sh \
	restore.sh \
	install.sh 

install-data-hook:
	chmod 0755 $(DESTDIR)$(bindir)/backup.sh
	chmod 0755 $(DESTDIR)$(bindir)/restore.sh
	chmod 0755 $(DESTDIR)$(bindir)/install.sh
