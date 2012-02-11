SUBDIRS = \
	connection \
	libeventserver \
	sectionsdclient \
	timerdclient \
	libconfigfile \
	libmd5sum \
	libnet \
	xmltree \
	libtuxtxt \
	libdvbsub

if ENABLE_UPNP
SUBDIRS += \
	libupnpclient
endif

if USE_STB_HAL
else
if BOXTYPE_TRIPLE
SUBDIRS += \
	libtriple
endif
endif
