SUBDIRS = \
	buttons \
	filetypes \
	headers \
	hints \
	locale \
	movieplayer \
	radar \
	slider \
	status \
	various \
	weather

if ENABLE_GRAPHLCD
SUBDIRS += \
	oled
endif

install-data-hook:
	$(INSTALL) -d $(DESTDIR)/$(ICONSDIR)/logo
