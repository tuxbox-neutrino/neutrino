AM_CXXFLAGS = -fno-rtti -fno-exceptions -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	@HWLIB_CFLAGS@

BUILT_SOURCES = rcsim.h
rcsim.h: $(top_srcdir)/src/driver/rcinput.h $(top_srcdir)/src/driver/rcinput_fake.h $(srcdir)/rcsim_h-creation.sh
	sh "$(srcdir)/rcsim_h-creation.sh" "$(top_srcdir)" > $@.tmp && mv $@.tmp $@

bin_PROGRAMS = rcsim
rcsim_SOURCES = rcsim.c rcsim.h

bin_PROGRAMS += mdev_helper
mdev_helper_SOURCES = mdev_helper.cpp

if BOXTYPE_CST
bin_PROGRAMS += drivertool
drivertool_SOURCES = drivertool.c

bin_PROGRAMS += dt
dt_SOURCES = dt.c

bin_PROGRAMS += shutdown_helper
shutdown_helper_SOURCES = shutdown_helper.c
endif
