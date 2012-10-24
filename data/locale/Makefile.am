installdir = $(DATADIR)/neutrino/locale

locale = \
	deutsch.locale \
	english.locale

locale_unmaintained = \
	unmaintained/bayrisch.locale \
	unmaintained/bosanski.locale \
	unmaintained/ch-baslerdeutsch.locale \
	unmaintained/ch-berndeutsch.locale \
	unmaintained/czech.locale \
	unmaintained/francais.locale \
	unmaintained/italiano.locale \
	unmaintained/nederlands.locale \
	unmaintained/polski.locale \
	unmaintained/portugues.locale \
	unmaintained/russkij.locale \
	unmaintained/slovak.locale \
	unmaintained/suomi.locale \
	unmaintained/svenska.locale

install_DATA  = $(locale)
install_DATA += $(locale_unmaintained)


if MAINTAINER_MODE

locals: sort-locals locals.h locals_intern.h install-locals

sort-locals: $(top_srcdir)/data/locale/deutsch.locale $(top_srcdir)/data/locale/english.locale
	cat $(top_srcdir)/data/locale/deutsch.locale | LC_ALL=C sort | uniq > deutsch.locale
	cat $(top_srcdir)/data/locale/english.locale | LC_ALL=C sort | uniq > english.locale
	cp -f deutsch.locale english.locale $(top_srcdir)/data/locale

ordercheck: $(top_srcdir)/data/locale/deutsch.locale
	cut -d' ' -f1 $(top_srcdir)/data/locale/deutsch.locale | LC_ALL=C sort | uniq > /tmp/log
	cut -d' ' -f1 $(top_srcdir)/data/locale/deutsch.locale | uniq  | diff - /tmp/log || \
		(echo "ERROR: deutsch.locale not ordered or contains empty lines" && false)

locals.h: ordercheck $(top_srcdir)/data/locale/deutsch.locale
	cut -d' ' -f1 $(top_srcdir)/data/locale/deutsch.locale | LC_ALL=C sort | uniq | tr [:lower:] [:upper:] | tr \. \_  | tr \- \_ | tr -d \? | $(top_srcdir)/data/locale/create.locals.h

locals_intern.h: ordercheck $(top_srcdir)/data/locale/deutsch.locale
	cut -d' ' -f1 $(top_srcdir)/data/locale/deutsch.locale | LC_ALL=C sort | uniq | $(top_srcdir)/data/locale/create.locals_intern.h

check: locals.h locals_intern.h
	diff locals.h $(top_srcdir)/src/system
	diff locals_intern.h $(top_srcdir)/src/system

install-locals: locals.h locals_intern.h
	cp locals.h locals_intern.h $(top_srcdir)/src/system
	@echo "Consider committing src/system/[locals.h,locals_intern.h]"
	cp -f deutsch.locale english.locale $(top_srcdir)/data/locale
	@echo "Consider committing data/locale/[deutsch.locale,english.locale]"

locals-clean:
	rm -f locals.h locals_intern.h deutsch.locale english.locale

endif
