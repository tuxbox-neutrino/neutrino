# src/httpd: the source's transport-free REST layer, served by nhttpd.
#
# What is here names no server. The endpoint tables, the router, the OpenAPI
# generator and the two route modules of slice 1 come from the source as written;
# the bridge that hands them nhttpd's requests, and the stubs that stand in
# for the source's own server, are this tree's. Exceptions are on because the
# layers above can throw and the catch has to be reachable through these
# frames; the bridge is the frame that catches, because the caller in
# nhttpd is built without them.
#
# -Werror=missing-field-initializers is load-bearing: an Endpoint row that
# stops short of its last member would otherwise read as complete.
AM_CXXFLAGS = -std=c++11 -fno-rtti -fexceptions -Wall -Wextra -Werror=missing-field-initializers -Werror=switch -Werror=conversion-null -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# DISABLE_LEGACY_API: the source's compatibility layer is not here, /control is
# answered by nhttpd's own CControlAPI. The system module then reports an
# empty legacy-usage list, which is the truth of this build.
AM_CPPFLAGS = \
	-DDISABLE_LEGACY_API \
	-DHTTPD_AUTH_LEGACY_BASIC \
	-I$(top_builddir) \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/nhttpd \
	-I$(top_srcdir)/src/nhttpd/yhttpd_core \
	@HWLIB_CFLAGS@ \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/xmltree \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@LUA_CFLAGS@

AUTOMAKE_OPTIONS = subdir-objects

if USE_PUGIXML
PUGIXML_LIBS = -lpugixml
endif

noinst_LIBRARIES = libhttpd.a

libhttpd_a_SOURCES = \
	ep/ep_channels.cpp \
	ep/ep_config.cpp \
	ep/ep_daemons.cpp \
	ep/ep_epg.cpp \
	ep/ep_netfs.cpp \
	ep/ep_osd.cpp \
	ep/ep_plugins.cpp \
	ep/ep_recordings.cpp \
	ep/ep_settings.cpp \
	ep/ep_storage.cpp \
	ep/ep_system.cpp \
	ep/ep_timers.cpp \
	ep/ep_tuner.cpp \
	http.cpp \
	json.cpp \
	nhttpd_bridge.cpp \
	doc/openapi.cpp \
	request.cpp \
	router.cpp \
	routes.cpp \
	status.cpp \
	stubs/auth_names.cpp \
	stubs/descriptor.cpp \
	stubs/events_stub.cpp \
	stubs/static_stub.cpp \
	stubs/webconfig_stub.cpp

noinst_HEADERS = \
	auth.h \
	endpoint.h \
	endpoints.h \
	events.h \
	http.h \
	json.h \
	netmatch.h \
	nhttpd_bridge.h \
	doc/openapi.h \
	router.h \
	schema.h \
	server.h \
	static.h \
	status.h \
	webconfig.h

# Every source in the directory is in a list, or the build says which is not.
check-sources:
	@orphan=`cd $(srcdir) && for f in *.cpp ep/*.cpp doc/*.cpp stubs/*.cpp; do \
		test -f "$$f" || continue; \
		case " $(libhttpd_a_SOURCES) $(openapi_dump_SOURCES) " in \
		*" $$f "*) ;; \
		*) echo $$f;; \
		esac; \
	done`; \
	test -z "$$orphan" || { echo "in src/httpd and in no source list:$$orphan" >&2; exit 1; }

# No object here may carry a global constructor: the program links this as a
# convenience archive, and the linker drops an archive member whose only
# effect is a constructor, with nothing said about it. routes.cpp names the
# tables by hand for the same reason.
check-no-constructors: $(noinst_LIBRARIES)
	@case " $(CXXFLAGS) $(AM_CXXFLAGS) $(CPPFLAGS) $(AM_CPPFLAGS) " in \
	*-fsanitize*) \
		echo "src/httpd: the check for global constructors is standing aside," >&2; \
		echo "  because this build carries -fsanitize and the instrumentation" >&2; \
		echo "  puts a constructor of its own in every object it compiles" >&2; \
		exit 0;; \
	esac; \
	for o in *.$(OBJEXT) ep/*.$(OBJEXT) doc/*.$(OBJEXT) stubs/*.$(OBJEXT); do \
		test -f "$$o" || continue; \
		case "$$o" in doc/openapi-dump*.$(OBJEXT)) continue;; esac; \
		sections=`$(OBJDUMP) -h "$$o" 2>/dev/null`; \
		case "$$sections" in \
		*.text*) ;; \
		*) echo "cannot read the sections of $$o with $(OBJDUMP)" >&2; exit 1;; \
		esac; \
		case "$$sections" in \
		*.init_array*) \
			echo "$$o carries a global constructor, which the linker drops" >&2; \
			echo "  from a convenience archive with nothing said about it" >&2; \
			exit 1;; \
		esac; \
	done

.PHONY: check-sources check-no-constructors
all-local: check-sources check-no-constructors

# The document without a server: the same tables, the same bytes. Built on
# request (make openapi-dump), used by the unit tests and by the type
# generator under test/web.
EXTRA_PROGRAMS = openapi_dump
openapi_dump_SOURCES = doc/openapi-dump.cpp doc/openapi-dump-stubs.cpp doc/openapi-dump-globals.cpp
openapi_dump_LDADD = \
	libhttpd.a \
	$(top_builddir)/src/coreapi/libcoreapi.a \
	$(top_builddir)/src/system/helpers.o \
	$(top_builddir)/src/system/debug.o \
	$(top_builddir)/lib/libconfigfile/libconfigfile.a \
	$(top_builddir)/lib/jsoncpp/libjsoncpp.a \
	$(top_builddir)/lib/libmd5sum/libmd5sum.a \
	$(top_builddir)/lib/xmltree/libxmltree.a \
	$(PUGIXML_LIBS) \
	-lOpenThreads \
	-lcurl \
	-lcrypto \
	-lz \
	-lpthread

openapi.json: openapi_dump$(EXEEXT)
	@./openapi_dump$(EXEEXT) > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp; exit 1; }

openapi-dump: openapi.json
	@echo "$(abs_builddir)/openapi.json"
.PHONY: openapi-dump

CLEANFILES = openapi_dump$(EXEEXT) openapi.json openapi.json.tmp
