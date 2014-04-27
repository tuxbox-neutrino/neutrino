SUBDIRS = fonts icons inetradio initial iso-codes license locale scripts themes neutrino-scripts

if BOXTYPE_TRIPLE
SUBDIRS += lcd
endif

configdir = $(CONFIGDIR)
config_DATA = cables.xml satellites.xml encoding.conf tobackup.conf providermap.xml settingsupdate.conf terrestrial.xml

install-data-hook:
	$(INSTALL) -d $(DESTDIR)/$(CONFIGDIR)/zapit
