# The warnings are on for the same reason they are on in the layer under
# test: a table written as an aggregate that stops short of the last member
# leaves the rest zero and says nothing, and this is the other side that
# writes such tables. That one is an error rather than a warning, because a
# warning in a parallel build scrolls past and the build still succeeds, and a
# guard that does not stop anything is not a guard.
AM_CXXFLAGS = -fno-rtti -fexceptions -std=c++11 -Wall -Wextra -Werror=missing-field-initializers

AM_CPPFLAGS = \
	-D__STDC_FORMAT_MACROS \
	-D__STDC_LIMIT_MACROS \
	-I$(srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/nhttpd \
	-I$(top_srcdir)/src/nhttpd/yhttpd_core \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/lib/timerdclient \
	-DCOREAPI_PAIRS_FILE='"$(abs_builddir)/settings-pairs.txt"' \
	-DCOREAPI_KEYS_FILE='"$(abs_builddir)/settings-keys.txt"' \
	-DCOREAPI_BOUNDS_FILE='"$(abs_builddir)/settings-bounds.txt"' \
	-DCOREAPI_LOCALES_FILE='"$(abs_builddir)/settings-locales.txt"' \
	-DCOREAPI_CHOICES_FILE='"$(abs_builddir)/settings-choices.txt"' \
	-DCOREAPI_LABELS_FILE='"$(abs_builddir)/settings-labels.txt"' \
	-DCOREAPI_APPLIED_FILE='"$(abs_builddir)/settings-applied.txt"' \
	-DCOREAPI_ERRORCODES_FILE='"$(abs_builddir)/errorcodes.txt"' \
	-DCOREAPI_STATUSES_FILE='"$(abs_builddir)/statuses.txt"' \
	-DCOREAPI_ENUM_SCRIPT='"$(srcdir)/scan/extract-enum.sh"' \
	-DCOREAPI_COUNTS_FILE='"$(srcdir)/support/counts.txt"' \
	-DCOREAPI_COUNTS_ACTUAL='"$(abs_builddir)/counts.actual"' \
	-DHTTPD_OPENAPI_DUMP='"$(abs_builddir)/openapi.json"' \
	-DCOREAPI_ANSWERS_ACTUAL='"$(abs_builddir)/answers.actual"' \
	-DSERVER_CONF_TEMPLATE='"$(top_srcdir)/src/httpd/nhttpd.conf.in"' \
	@HWLIB_CFLAGS@

AUTOMAKE_OPTIONS = subdir-objects

if USE_PUGIXML
endif

check_PROGRAMS = coreapi_tests remotebox_client

coreapi_tests_SOURCES = \
	support/main.cpp \
	coreapi/test_archive.cpp \
	coreapi/test_playback.cpp \
	coreapi/test_result.cpp \
	lib/connection/test_basicclient.cpp \
	lib/libeventserver/test_eventserver_outbox.cpp \
	coreapi/test_channelsource.cpp \
	zapit/test_zapitchannel_copy.cpp \
	coreapi/test_channelstreams.cpp \
	zapit/test_bouquetname.cpp \
	zapit/test_zapitclient_getpids.cpp \
	zapit/test_zapitclient_bouquetwrite.cpp \
	system/test_atomicfile.cpp \
	system/test_atomicfile_link.cpp \
	driver/test_postmsg.cpp \
	driver/test_eventframe.cpp \
	coreapi/test_eventbus.cpp \
	coreapi/test_messagebridge.cpp \
	coreapi/test_channels.cpp \
	coreapi/test_bouquetedit.cpp \
	coreapi/test_bouquetorder.cpp \
	coreapi/test_epg.cpp \
	coreapi/test_timers.cpp \
	coreapi/test_sse.cpp \
	coreapi/test_screenshot.cpp \
	coreapi/test_commandsink.cpp \
	coreapi/test_postpayload.cpp \
	coreapi/test_system.cpp \
	coreapi/test_rc.cpp \
	coreapi/test_shellcontract.cpp \
	system/test_mysystem.cpp \
	timerd/test_timerwake.cpp \
	lib/timerdclient/test_timerdannounce.cpp \
	coreapi/test_storage.cpp \
	coreapi/test_storage_move.cpp \
	coreapi/test_system_proc.cpp \
	coreapi/test_storage_write.cpp \
	coreapi/test_shelltable.cpp \
	coreapi/test_daemons.cpp \
	coreapi/test_settingssource.cpp \
	coreapi/test_settings.cpp \
	coreapi/test_schema.cpp \
	coreapi/test_settingspairs.cpp \
	coreapi/test_settingschoices.cpp \
	coreapi/test_settingstable.cpp \
	coreapi/test_settingsappliers.cpp \
	coreapi/test_browserplay.cpp \
	coreapi/test_settingsfields.cpp \
	coreapi/test_endpoints_read.cpp \
	coreapi/test_endpoints_write.cpp \
	system/test_mysystem_local.cpp \
	nhttpd/test_yhttpd_api.cpp \
	nhttpd/test_bridge.cpp \
	nhttpd/test_yhttpd_config.cpp \
	nhttpd/test_yhttpd_static.cpp \
	httpd/test_openapi.cpp \
	httpd/test_openapi_basic.cpp \
	httpd/test_router.cpp \
	httpd/test_httpjson.cpp \
	httpd/test_byterange.cpp \
	httpd/test_credentials.cpp \
	httpd/test_legacy_usage.cpp \
	httpd/test_login_bridge.cpp \
	httpd/test_netmatch.cpp \
	httpd/test_webconfig.cpp \
	nhttpd/yhttpd_stubs.cpp \
	support/counts.cpp \
	support/answers.cpp \
	support/helperstubs.cpp

# The channel object is linked rather than reimplemented, so a change to its copy
# behaviour fails the suite. It is the only object of the channel stack that links
# on its own. Everything the archive holds is pulled except the four adapters that
# bind to the running box: channelsource_real.o, systemsource_real.o,
# tunersource_real.o and eventsink_real.o all need the whole application, and none
# of them is reached because nothing here calls the install function each defines.
# settingssource_real.o is the one adapter that is driven: it needs the program's
# settings struct and a table, both of which a case builds.
# rcinput.o carries the message queue under test; abstime.o is the only object it
# needs besides the stubs in test_postmsg.cpp. commandsink_real.o comes out of the
# archive with it, which is why that adapter lives in a file of its own.
# The timer daemon's client is linked for the same reason as the channel object:
# what a list read does with an answer that stops early is its own logic.
# timerwake.o is the one object of the timer daemon that links on its own.
# helpers.o carries the spawn helper the shipped program uses, so the case that
# pins what it does with an ampersand drives that object and not a copy. debug.o,
# the json archive and the network library are what it needs besides;
# helperstubs.cpp answers for the screen it also draws on.
# The server archive is listed ahead of the layer under it, because a static
# archive only answers symbols asked for by something already on the line.
# The settings file reader is named because the server archive reads its own
# configuration through the one the rest of the tree uses, and that library is
# built beside it rather than found on the system.
# The crypto library is named here rather than left to the environment: this suite
# is linked by hand, so the derivation and the constant time comparison would
# otherwise be undefined symbols at the end of the line.
# The xml library is named because the compatibility layer escapes a value with
# the tree's own converter, and that converter shares an object with the parser.
# The zapit client archive is what getPIDS lives in, so the case that pins what it
# does with a daemon that answers short drives that code and not a copy; it needs
# the connection library after it.
coreapi_tests_LDADD = \
	$(top_builddir)/src/httpd/libhttpd.a \
	$(top_builddir)/src/coreapi/libcoreapi.a \
	$(top_builddir)/src/nhttpd/yhttpd_mods/libyhttpdmods.a \
	$(top_builddir)/src/nhttpd/yhttpd_core/libyhttpd.a \
	$(top_builddir)/lib/xmltree/libxmltree.a \
	$(top_builddir)/src/system/helpers.o \
	$(top_builddir)/src/timerd/timerwake.o \
	$(top_builddir)/src/system/debug.o \
	$(top_builddir)/lib/jsoncpp/libjsoncpp.a \
	$(top_builddir)/src/zapit/channel.o \
	$(top_builddir)/src/zapit/client/libzapitclient.a \
	$(top_builddir)/src/driver/rcinput.o \
	$(top_builddir)/src/driver/abstime.o \
	$(top_builddir)/lib/libmd5sum/libmd5sum.a \
	$(top_builddir)/lib/libconfigfile/libconfigfile.a \
	$(top_builddir)/lib/timerdclient/libtimerdclient.a \
	$(top_builddir)/lib/connection/libconnection.a \
	$(top_builddir)/lib/libeventserver/libeventserver.a \
	-lOpenThreads \
	-lcurl \
	-lcrypto \
	-lz \
	-lpthread

# remotebox_client wraps the client another box speaks to this box with, compiled
# from that tree's own files under interop/peer (kept word for word, refreshed by
# path and never edited; see the README there) and driven by
# test/http/check-remotebox.sh against a running box. It is built with the suite
# and never run by it, because it needs a box to talk to - hence TESTS names the
# suite alone. The peer directory is searched first so that the client's own
# <system/httptool.h> is the one it was written against and not the one this
# tree ships; the tree's helpers, json reader and timer client answer the rest.
remotebox_client_SOURCES = \
	interop/remotebox_client.cpp \
	interop/peer/system/remotetimer.cpp \
	interop/peer/system/remotetimer.h \
	interop/peer/system/httptool.cpp \
	interop/peer/system/httptool.h \
	support/helperstubs.cpp

# The progress window the http tool draws on reaches the framebuffer header, and
# that one the signal library; the json reader the client parses answers with
# lives in lib. Both flags are the ones src/system compiles the tree's own http
# tool and json helper with.
remotebox_client_CPPFLAGS = -I$(srcdir)/interop/peer $(AM_CPPFLAGS) -I$(top_srcdir)/lib/jsoncpp @SIGC_CFLAGS@

remotebox_client_LDADD = \
	$(top_builddir)/src/system/helpers.o \
	$(top_builddir)/src/system/debug.o \
	$(top_builddir)/lib/jsoncpp/libjsoncpp.a \
	$(top_builddir)/lib/timerdclient/libtimerdclient.a \
	$(top_builddir)/lib/connection/libconnection.a \
	$(top_builddir)/lib/libmd5sum/libmd5sum.a \
	-lOpenThreads \
	-lcurl \
	-lz \
	-lpthread

TESTS = coreapi_tests

# Catch2 passes a case that ran no assertion at all unless it is asked to object,
# so a case whose only check sits behind a loop nobody enters reads as green. The
# flag has to follow the program name and the harness puts what it is handed in
# front of it, hence the shell.
# support/run-tests.sh carries -w NoAssertions and the exclusions this tree
# needs, one line each with what brings the case back.
LOG_COMPILER = $(SHELL) $(srcdir)/support/run-tests.sh

EXTRA_DIST = interop/peer/README support/catch.hpp support/run-tests.sh support/fakes.h support/rcfixture.h support/counts.h support/answers.h support/counts.txt \
	scan/extract-enum.sh support/merge-counts.sh \
	scan/check-hook.sh scan/check-fieldtypes.sh \
	scan/check-hardware.sh scan/check-appliers.sh scan/check-denominator.sh \
	scan/check-choices.sh \
	scan/check-sections.sh \
	scan/check-channellist-invalidation.sh \
	scan/check-reload-forgets-first.sh \
	scan/check-bouquet-rename.sh scan/check-zapit-write-checked.sh \
	scan/check-locale-catalog.sh \
	scan/check-bouquet-save-atomic.sh \
	scan/check-access-store.sh \
	scan/check-mute-source.sh \
	scan/check-channel-content-lock.sh \
	scan/check-bouquet-vector-lock.sh \
	scan/check-list-reader-lock.sh \
	scan/check-settings-text-lock.sh \
	scan/check-settings-text-copies.sh \
	scan/check-exception-boundary.sh \
	scan/check-standby-display.sh \
	scan/check-standby-abort.sh \
	scan/check-standby-wake.sh \
	scan/check-zap-standby.sh \
	scan/check-timeshift-start.sh \
	scan/check-wire-arms-top-level.sh \
	scan/check-settings-input-open.sh \
	scan/check-xmltv-auto-lock.sh \
	scan/check-channellist-announce.sh \
	scan/check-storage-write-link.sh \
	scan/check-channel-writer-lock.sh \
	scan/check-channel-lock-reentry.sh \
	scan/strip-comments.awk scan/blank-if0.awk scan/bounds.awk scan/pairs.awk scan/fields.awk \
	scan/keyvals.awk scan/choices.awk scan/enums.awk scan/labels.awk scan/branches.awk \
	scan/settingstext.awk scan/channel-holds.awk \
	scan/extract-pairs.sh scan/extract-bounds.sh scan/extract-defines.sh \
	scan/extract-choices.sh scan/extract-constants.sh scan/extract-labels.sh \
	scan/extract-applied.sh scan/extract-httpcodes.sh \
	scan/check-narrowing.sh support/narrow-probe.cpp

# What the program says about its own settings, read out of its source so a
# declared row can be held to it. Built before anything else here.
#
# Rebuilt every time and never from a stamp: a table read against yesterday's
# extraction is a check on nothing in particular, and the extraction costs a
# second. Each script refuses to print a short answer, so a regex that stopped
# matching stops the build here rather than leaving the cases comparing nothing.
# The generated table of key names is read by a case here for the same reason the
# layer under test reads it. A file that has never been written is not one the
# recorded dependencies can bring into being, so it is asked for rather than left
# to the order the directories are built in.
BUILT_SOURCES = $(top_builddir)/src/tools/rcsim.h openapi.json settings-pairs.txt settings-keys.txt settings-bounds.txt settings-locales.txt \
	settings-choices.txt settings-labels.txt settings-applied.txt errorcodes.txt statuses.txt
CLEANFILES = openapi.json api.d.ts.tmp settings-pairs.txt settings-keys.txt settings-bounds.txt settings-locales.txt \
	settings-choices.txt settings-labels.txt settings-applied.txt errorcodes.txt statuses.txt \
	counts.actual answers.actual

# The members each of the two enums declares, so that a walk of one in the suite
# is answerable to the header it came from. The error codes are walked from a
# list written out by hand and this is what holds that list to the header; the
# statuses are walked from this file itself, so adding one moves a number here
# rather than leaving the last of them outside a hand written bound.
errorcodes.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-enum.sh $(top_srcdir) src/coreapi/base/errors.h ErrorCode 60 > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

statuses.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-enum.sh $(top_srcdir) src/coreapi/base/result.h Status 6 > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

# The codes the server has names for, so that the pair which turns one into a
# domain status and back is walked rather than transcribed. The floor is under
# what the header holds today and above what a scan that stopped matching would
# print.
settings-pairs.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-pairs.sh $(top_srcdir)/src/neutrino.cpp $(top_srcdir)/src > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

settings-keys.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-pairs.sh -k $(top_srcdir)/src > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

settings-bounds.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-bounds.sh $(top_srcdir)/src > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

settings-locales.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-bounds.sh -l $(top_srcdir)/src > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

# The hardware libraries are read beside the tree's own source, because a screen
# states a choice under a name one of them declares.
settings-choices.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-choices.sh $(top_srcdir)/src $(top_srcdir)/lib > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

# The label the screens state beside a setting they offer as neither a range nor
# a table of choices, which is what the two scans above cannot reach.
settings-labels.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-labels.sh $(top_srcdir)/src > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

# The key each applier lists beside the option its notifier branches on, so that
# a list transcribed from a notifier by hand is answerable to it.
settings-applied.txt: FORCE
	@$(SHELL) $(srcdir)/scan/extract-applied.sh $(top_srcdir)/src > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

# The document the server answers with, written out by the target that owns it
# and read here so that the two are held to being the same bytes. Made through
# that target rather than copied from wherever it was last left, because the
# file is what a client's types come out of and one built before the change
# under test would be compared against nothing in particular. Making it there
# also rebuilds the archive it is written out of, which building this directory
# on its own does not.
$(top_builddir)/src/tools/rcsim.h:
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src/tools rcsim.h >/dev/null

# The document the server answers with, written out by the target that owns it
# and read here so that the two are held to being the same bytes. Made through
# that target rather than copied from wherever it was last left, because the
# file is what a client's types come out of and one built before the change
# under test would pass a check it should fail.
openapi.json: FORCE
	@$(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/src/httpd openapi.json >/dev/null
	@cp -f $(top_builddir)/src/httpd/openapi.json $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp $@; exit 1; }

FORCE:
.PHONY: FORCE

# The TypeScript shapes a page reads are written out of the document by
# The source's generator, taken as written. It refuses a document naming fewer
# than forty addresses or twenty shapes, so it only became runnable with the
# tables of slice 2a. Run where node is and said to be skipped where it is
# not: a box build carries no node, and a check that has not run is not a
# check that passed.
check-api-types: openapi.json
	@if command -v node >/dev/null 2>&1; then \
		node $(top_srcdir)/test/web/openapi-types.js openapi.json api.d.ts.tmp && rm -f api.d.ts.tmp; \
	else \
		echo "check-api-types: no node on this host, the type generator was not run"; \
	fi
.PHONY: check-api-types

# check-narrowing.sh is shipped beside these and is deliberately not run here.
# It drives the two guards in the router that compare a value with itself
# narrowed to a long, and where a long is eight bytes the compiler removes them:
# a run on this host would report a pass about code that is not in the binary,
# so the script refuses to run there. It is run by hand in the image this
# project keeps for the oldest compiler it supports, which is also the only
# environment here with the target's four byte long, and its header says how.

# A test file no source list mentions is compiled by nothing and would pass unnoticed.
#
# Named as a prerequisite rather than left beside check-TESTS: automake asks for the
# two of them in one make, which under -j runs them at the same time, and these scans
# start compilers and write files while the cases are running. What that cost the
# suite was cases that read the clock or the free space going red on a loaded machine
# and green on a quiet one. Ordered here rather than in check-am, because check-am is
# automake's and this is not.
check-local: check-TESTS
	@orphan=`cd $(srcdir) && find . -name 'test_*.cpp' | sed 's|^\./||' | sort | while read f; do \
		case " $(coreapi_tests_SOURCES) " in *" $$f "*) ;; *) echo $$f;; esac; done`; \
	test -z "$$orphan" || { echo "not in coreapi_tests_SOURCES: $$orphan" >&2; exit 1; }
	@$(SHELL) $(srcdir)/scan/check-fieldtypes.sh "$(CXX)" $(DEFS) $(AM_CPPFLAGS) $(AM_CXXFLAGS) -I$(top_srcdir)/src/coreapi
	@$(SHELL) $(srcdir)/scan/check-hardware.sh "$(CXX)" $(top_builddir)/config.h $(top_srcdir)/src/coreapi/settings \
		$(DEFS) $(AM_CPPFLAGS) $(AM_CXXFLAGS) -I$(top_srcdir)/src/coreapi
	@$(SHELL) $(srcdir)/scan/check-denominator.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-appliers.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-choices.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-sections.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-locale-catalog.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-channellist-invalidation.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-reload-forgets-first.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-bouquet-rename.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-zapit-write-checked.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-bouquet-save-atomic.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-access-store.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-mute-source.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-channel-content-lock.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-bouquet-vector-lock.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-list-reader-lock.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-settings-text-lock.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-settings-text-copies.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-exception-boundary.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-standby-display.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-standby-abort.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-standby-wake.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-zap-standby.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-timeshift-start.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-wire-arms-top-level.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-settings-input-open.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-xmltv-auto-lock.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-channellist-announce.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-storage-write-link.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-channel-writer-lock.sh $(top_srcdir)
	@$(SHELL) $(srcdir)/scan/check-channel-lock-reentry.sh $(top_srcdir)
	@$(MAKE) $(AM_MAKEFLAGS) check-api-types
