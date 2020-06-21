installdir = $(FONTDIR)

# NOTE: tuxtxt.otb is the non-ttf font for tuxtxt (UseTTF 0 in tuxtxt2.conf)
#	Variables neutrino_ttf and tuxtxt_ttf describe aliases for neutrino.ttf and tuxtxt.ttf.

neutrino_ttf = neutrino.ttf
tuxtxt_ttf = DejaVuLGCSansMono-Bold.ttf

install_DATA = \
	$(neutrino_ttf) \
	$(tuxtxt_ttf) \
	LiberationSans-Regular.ttf \
	md_khmurabi_10.ttf \
	micron_bold.ttf \
	micron_italic.ttf \
	micron.ttf \
	tuxtxt.otb


if BOXMODEL_CS_HD2
install_DATA += UnDotum.ttf
endif

install-data-hook:
	cd $(DESTDIR)$(FONTDIR); \
		mv $(neutrino_ttf) neutrino.ttf; \
		mv $(tuxtxt_ttf) tuxtxt.ttf; \
		ln -sf tuxtxt.ttf shell.ttf

uninstall-hook:
	cd $(DESTDIR)$(FONTDIR); \
		rm -f neutrino.ttf; \
		rm -f tuxtxt.ttf; \
		rm -f shell.ttf
