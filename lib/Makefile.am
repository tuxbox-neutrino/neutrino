SUBDIRS = \
	connection \
	libeventserver \
	sectionsdclient \
	timerdclient \
	libconfigfile \
	libmd5sum \
	libnet \
	xmltree \
	jsoncpp \
	libtuxtxt \
	libiw \
	libdvbsub \
	luaclient

if ENABLE_UPNP
SUBDIRS += \
	libupnpclient
endif

if ENABLE_LCD
SUBDIRS += \
	liblcddisplay
endif

if BOXTYPE_CST
SUBDIRS += \
	hardware/coolstream
endif
