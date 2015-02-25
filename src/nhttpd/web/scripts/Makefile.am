installdir =  $(DATADIR)/neutrino/httpd/scripts

install_DATA= api.sh _Y_Globals.sh _Y_Library.sh Y_Live.sh Y_Tools.sh

# file manager
install_DATA +=	Y_NAS.sh

install-data-hook:
	chmod 0755 $(DESTDIR)$(DATADIR)/neutrino/httpd/scripts/api.sh
	chmod 0755 $(DESTDIR)$(DATADIR)/neutrino/httpd/scripts/Y_*.sh
