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

# The source's compatibility layer is not here: /control is answered by nhttpd's
# own CControlAPI, which feeds the one piece of that layer this build keeps -
# the counting seam behind /api/v1/system/legacy-usage (compat/mount.h is
# this tree's bridge header, stubs/legacy_usage.cpp its counts).
AM_CPPFLAGS = \
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
	@CURL_CFLAGS@ \
	@LUA_CFLAGS@

AUTOMAKE_OPTIONS = subdir-objects

if USE_PUGIXML
PUGIXML_LIBS = -lpugixml
endif

noinst_LIBRARIES = libhttpd.a

libhttpd_a_SOURCES = \
	auth.cpp \
	ep/ep_auth.cpp \
	ep/ep_channels.cpp \
	ep/ep_config.cpp \
	ep/ep_daemons.cpp \
	ep/ep_epg.cpp \
	ep/ep_netfs.cpp \
	ep/ep_network.cpp \
	ep/ep_osd.cpp \
	ep/ep_playback.cpp \
	ep/ep_plugins.cpp \
	ep/ep_recordings.cpp \
	ep/ep_settings.cpp \
	ep/ep_storage.cpp \
	ep/ep_stream.cpp \
	ep/ep_system.cpp \
	ep/ep_timers.cpp \
	ep/ep_tuner.cpp \
	ep/ep_webserver.cpp \
	ep/ep_webui.cpp \
	ep/ep_webtv.cpp \
	byterange.cpp \
	carry.cpp \
	credentials.cpp \
	hlsrewrite.cpp \
	http.cpp \
	json.cpp \
	livestream.cpp \
	login_bridge.cpp \
	netmatch.cpp \
	nhttpd_bridge.cpp \
	doc/openapi.cpp \
	request.cpp \
	router.cpp \
	routes.cpp \
	status.cpp \
	stubs/descriptor.cpp \
	events.cpp \
	stubs/legacy_usage.cpp \
	stubs/server_nhttpd.cpp \
	stubs/static_stub.cpp \
	webconfig.cpp \
	webtv.cpp

noinst_HEADERS = \
	auth.h \
	byterange.h \
	carry.h \
	compat/mount.h \
	credentials.h \
	endpoint.h \
	endpoints.h \
	events.h \
	hlsrewrite.h \
	randomsource.h \
	http.h \
	json.h \
	livestream.h \
	login_bridge.h \
	netmatch.h \
	nhttpd_bridge.h \
	doc/openapi.h \
	router.h \
	schema.h \
	server.h \
	server_control.h \
	static.h \
	status.h \
	webconfig.h \
	webtv.h

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

# A member of an answer that states the whole of what it can be, held to the
# code that writes it (the scan's own head says how). The row is beside the
# handler and so is the naming function the row has to agree with, and the
# comparison is over the two of them.
#
# The comment blanker is the suite's, named across rather than copied. A word
# behind // has to count as absent in both halves of the comparison, and a
# second reader of C comments in this tree would be a second thing that can be
# wrong about one.
check-answer-sets:
	@$(SHELL) $(srcdir)/check-answer-sets.sh $(srcdir) $(top_srcdir)/test/unit/scan/strip-comments.awk

.PHONY: check-sources check-no-constructors check-answer-sets
all-local: check-sources check-no-constructors check-answer-sets

# nhttpd.conf.in is the old server's configuration template. Here it is not
# a leftover: nhttpd is alive and this is the shape of its file, which the
# migration reader (webconfig.cpp) reads a box's credentials out of on the
# first start without a nhttpd-access.conf, and the load reads the port and
# the address out of on every start. The suite holds both to this template
# (SERVER_CONF_TEMPLATE).
EXTRA_DIST = check-answer-sets.sh nhttpd.conf.in

# The document without a server: the same tables, the same bytes. Built on
# request (make openapi-dump), used by the unit tests and by the type
# generator under test/web. libtimerdclient answers the announce time the
# timer layer fills in.
EXTRA_PROGRAMS = openapi_dump
openapi_dump_SOURCES = doc/openapi-dump.cpp doc/openapi-dump-stubs.cpp doc/openapi-dump-globals.cpp
openapi_dump_LDADD = \
	libhttpd.a \
	$(top_builddir)/src/coreapi/libcoreapi.a \
	$(top_builddir)/lib/timerdclient/libtimerdclient.a \
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

# The same document kept in the tree as doc/api/openapi.json, which is what the
# published reference is built from without a configured tree, held to the one
# this build writes. The file is the document laid out one member per line by
# doc/api/format-openapi.py, so a change to a route reads in a diff as the lines
# that changed; the script refuses a layout that would not give these bytes
# back.
#
# Run by make check and never by a plain build. API_DOC_UNFIT names the first
# reason this build cannot speak for the file, and both targets ask it before
# they make the dump: a build compiled with one of the three switches below
# writes a different document by design (no prose, Basic instead of sessions,
# no legacy counter); the dump of a cross build is a program of the target and
# does not run here; a tree without doc/api, such as a release archive, has
# nothing to compare; and the layout needs python3. The check then says so and
# passes, and the refresh refuses, so a reduced document never reaches the tree.
API_DOC = $(top_srcdir)/doc/api/openapi.json
API_DOC_FORMAT = $(top_srcdir)/doc/api/format-openapi.py
API_DOC_UNFIT = \
	case " $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) $(CXXFLAGS) $(AM_CXXFLAGS) " in \
	*DISABLE_API_DOC*|*HTTPD_AUTH_LEGACY_BASIC*|*DISABLE_LEGACY_API*) \
		echo "this build writes a reduced document";; \
	*) \
		if test "$(build)" != "$(host)"; then \
			echo "this is a cross build, whose dump does not run here"; \
		elif test ! -f $(API_DOC_FORMAT); then \
			echo "this tree has no doc/api"; \
		elif ! command -v python3 >/dev/null 2>&1; then \
			echo "this host has no python3"; \
		fi;; \
	esac

check-api-doc:
	@why=`$(API_DOC_UNFIT)`; \
	if test -n "$$why"; then \
		echo "check-api-doc: doc/api/openapi.json was not compared: $$why"; \
		exit 0; \
	fi; \
	$(MAKE) $(AM_MAKEFLAGS) openapi.json || exit 1; \
	python3 -I $(API_DOC_FORMAT) openapi.json openapi.json.laid-out || exit 1; \
	if cmp -s openapi.json.laid-out $(API_DOC); then \
		rm -f openapi.json.laid-out; \
		exit 0; \
	fi; \
	{ \
		echo "check-api-doc: doc/api/openapi.json is not the document this code writes:"; \
		diff -u $(API_DOC) openapi.json.laid-out | head -n 40; \
		echo "  When the change to the routes is intended, refresh the file and commit it with the change:"; \
		echo "    make -C $(abs_builddir) update-api-doc"; \
	} >&2; \
	rm -f openapi.json.laid-out; \
	exit 1

# One recipe line per step, so make -n runs only the line naming $(MAKE), which
# inherits -n, and prints the write into the source tree instead of doing it.
# The write asks the question again, so make -i, which goes on past the first
# line's refusal, still does not write a reduced document.
update-api-doc:
	@why=`$(API_DOC_UNFIT)`; \
	if test -n "$$why"; then \
		echo "update-api-doc: doc/api/openapi.json was not written: $$why" >&2; \
		exit 1; \
	fi
	@$(MAKE) $(AM_MAKEFLAGS) openapi.json
	@why=`$(API_DOC_UNFIT)`; test -z "$$why" && \
	python3 -I $(API_DOC_FORMAT) openapi.json $(API_DOC) && \
	echo "wrote $(API_DOC)"

check-local: check-api-doc
.PHONY: check-api-doc update-api-doc

CLEANFILES = openapi_dump$(EXEEXT) openapi.json openapi.json.tmp openapi.json.laid-out
