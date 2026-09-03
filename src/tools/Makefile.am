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

# streamprobe uses the SAME streaminput core neutrino's record and
# stream-relay paths use, so it exists exactly where that core does:
# coolstream builds have no libstb-hal and therefore no core at all.
if HAVE_LIBSTB_HAL
bin_PROGRAMS += streamprobe
streamprobe_SOURCES = streamprobe.c streamprobe_version.h
# The first per-program flags in this directory. A named _CPPFLAGS replaces
# AM_CPPFLAGS instead of adding to it, so it is pulled back in explicitly --
# @HWLIB_CFLAGS@ inside it is what finds streaminput.h.
streamprobe_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	@AVFORMAT_CFLAGS@ \
	@AVCODEC_CFLAGS@ \
	@AVUTIL_CFLAGS@
streamprobe_LDADD = \
	-lstb-hal \
	@AVFORMAT_LIBS@ \
	@AVCODEC_LIBS@ \
	@AVUTIL_LIBS@
streamprobe_LDFLAGS = $(STB_HAL_LIB)
endif

if BOXTYPE_CST
bin_PROGRAMS += drivertool
drivertool_SOURCES = drivertool.c

bin_PROGRAMS += dt
dt_SOURCES = dt.c

bin_PROGRAMS += shutdown_helper
shutdown_helper_SOURCES = shutdown_helper.c
endif
