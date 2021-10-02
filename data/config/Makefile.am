EXTRA_DIST = \
	tobackup.conf tobackup.conf.in

installdir = $(CONFIGDIR)

install_DATA = \
	bad_package_pattern.list.sample \
	cables.xml \
	encoding.conf \
	epglanguages.conf \
	providermap.xml \
	satellites.xml \
	terrestrial.xml \
	tobackup.conf

if ENABLE_EXTUPDATE
EXTRA_DIST += \
	settingsupdate.conf settingsupdate.conf.in

install_DATA = \
	settingsupdate.conf
endif
