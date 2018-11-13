SUBDIRS = \
	fonts \
	icons \
	inetradio \
	initial \
	iso-codes \
	license \
	locale \
	neutrino-scripts \
	pictures \
	policy \
	scripts \
	themes \
	webradio \
	webtv \
	y-web

if BOXTYPE_TRIPLE
SUBDIRS += lcd
endif

configdir = $(CONFIGDIR)

config_DATA = \
	bad_package_pattern.list.sample \
	cables.xml \
	encoding.conf \
	epglanguages.conf \
	myservices.xml \
	providermap.xml \
	satellites.xml \
	settingsupdate.conf \
	terrestrial.xml \
	tobackup.conf
install-data-hook:
	$(INSTALL) -d $(DESTDIR)/$(CONFIGDIR)/zapit
