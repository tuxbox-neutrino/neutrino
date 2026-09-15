# Built with exceptions for the same reason the layer below it is: a handler
# reached from a C callback has to unwind into one boundary rather than end the
# process, and the aggregate warning is an error because a table that stops
# short of its last member leaves the rest zero and says nothing.
#
# An unhandled enumerator is an error here for the same reason: this is where a
# domain status becomes an answer on a wire, and a status nobody wrote a case
# for would otherwise leave the build green and answer whatever the line after
# the switch answers. A warning in a parallel build scrolls past.
#
# A null constant used as a number is an error here because the JSON writer
# offers a number for every width, so a caller meaning the absence of a value
# and writing the null macro gets a zero in the body instead. It is the one
# mistake in this directory that produces a wrong answer rather than no answer.
AM_CXXFLAGS = -std=c++11 -fno-rtti -fexceptions -Wall -Wextra -Werror=missing-field-initializers -Werror=switch -Werror=conversion-null -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# The build directory comes first, so that a header configure generated is the
# one found. Nothing here is named after one any more: the header this
# directory used to call config.h is webconfig.h, because two headers of one
# name in one include path is settled by the order of these flags, and a file
# asking for the feature macros and being handed the other one compiles clean
# and carries none of them. Removing the name is what closes that; the order
# below is no longer the only thing holding it shut.
#
# The entries from here on are for the copied control API: it carries what is
# left of the include list the old server's tuxboxapi/Makefile.am gave it,
# because it is the same source and reaches into the same parts of the Neutrino
# tree (GUI, drivers, event server). The three that pointed into that server's
# own directories are gone with it, and so is everything here that was reached
# through them.
AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/libconfigfile \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/include \
	@MHD_CFLAGS@ \
	@HWLIB_CFLAGS@ \
	-I$(top_srcdir)/lib/connection \
	-I$(top_srcdir)/lib/libeventserver \
	-I$(top_srcdir)/lib/xmltree \
	@SIGC_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@AVFORMAT_CFLAGS@ \
	@LUA_CFLAGS@

# The compatibility layer is a subdirectory of this one and not a library of
# its own: it is built with the flags above, and a second archive would be a
# second thing for every link line in the tree to name. Its objects stay in
# their own directory so that the two rules at the bottom can say which
# directory an object came from.
AUTOMAKE_OPTIONS = subdir-objects

# xmltree's own escape for a value needs pugixml's node type to compile
# against; openapi_dump links xmltree for the same reason it links helpers.o,
# and needs the same library there for the same, unrelated-to-this-program
# reason coreapi_tests already does.
if USE_PUGIXML
PUGIXML_LIBS = -lpugixml
endif

noinst_LIBRARIES = libhttpd.a

# Every source under compat/, whether or not this configuration actually
# compiles it. Named once here so check-sources below still recognises each
# of them by name with --disable-legacy-api set: without this, that check
# would read a switched-off file as an orphan no list mentions, which is
# exactly the fault it exists to catch, and not what turning the layer off
# on purpose is.
COMPAT_SOURCES = \
	compat/controlapi.cpp \
	compat/helper.cpp \
	compat/hook.cpp \
	compat/hookout.cpp \
	compat/legacybridge.cpp \
	compat/legacyconfig.cpp \
	compat/levels.cpp \
	compat/mount.cpp \
	compat/neutrinoapi.cpp \
	compat/query.cpp \
	compat/scriptrunner.cpp

if ENABLE_LEGACY_API
LEGACY_SOURCES = $(COMPAT_SOURCES)
endif

libhttpd_a_SOURCES = \
	apppaths.cpp \
	auth.cpp \
	$(LEGACY_SOURCES) \
	byterange.cpp \
	credentials.cpp \
	ep/ep_auth.cpp \
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
	ep/ep_stream.cpp \
	ep/ep_system.cpp \
	ep/ep_webtv.cpp \
	ep/ep_timers.cpp \
	ep/ep_tuner.cpp \
	ep/ep_webserver.cpp \
	events.cpp \
	hlsrewrite.cpp \
	livestream.cpp \
	http.cpp \
	json.cpp \
	netmatch.cpp \
	doc/openapi.cpp \
	request.cpp \
	router.cpp \
	routes.cpp \
	server.cpp \
	static.cpp \
	status.cpp \
	webconfig.cpp \
	webtv.cpp

noinst_HEADERS = \
	apppaths.h \
	auth.h \
	byterange.h \
	compat/controlapi.h \
	compat/helper.h \
	compat/hook.h \
	compat/hookout.h \
	compat/legacybridge.h \
	compat/legacyconfig.h \
	compat/levels.h \
	compat/mount.h \
	compat/neutrinoapi.h \
	compat/query.h \
	compat/scriptrunner.h \
	credentials.h \
	endpoint.h \
	endpoints.h \
	events.h \
	hlsrewrite.h \
	livestream.h \
	http.h \
	json.h \
	netmatch.h \
	doc/openapi.h \
	randomsource.h \
	router.h \
	schema.h \
	server.h \
	static.h \
	status.h \
	webconfig.h \
	webtv.h

# Two things about this directory that the build is the only place to hold.
#
# A source no list mentions is compiled by nothing. That ordinarily surfaces as
# an undefined reference at the link, except for the one shape below, where it
# surfaces as nothing at all.
#
# And an object carrying a global constructor. Neutrino links about thirty
# convenience archives, and the linker never pulls a member out of one unless
# something already on the line asked for a symbol it defines. A translation
# unit whose only effect is a constructor before main defines no such symbol, so
# it is dropped, and what was in that constructor is gone with no message from
# anything: the endpoint tables are written the way they are for exactly this
# reason. The rule is that no object here has an .init_array section, and until
# now it was held by somebody remembering to run objdump.
#
# compat/controlapi.o and compat/neutrinoapi.o are the two named exceptions,
# not a hole in the rule. controlapi.o defines Execute, and neutrinoapi.o is
# pulled in after it the same way: `nm -u compat/controlapi.o` lists twenty
# CNeutrinoAPI methods it calls unqualified, and every one of them is defined
# nowhere but neutrinoapi.o. Neither is on every link line that carries this
# archive, though: compat/mount.o answers /control/ through a function
# pointer (see mount.h) precisely so a link with nothing behind that pointer
# never has to ask for Execute at all, and coreapi_tests and openapi_dump are
# both such links, which is why `nm` finds neither object in either binary.
# The program that installs the real pointer, neutrino.cpp, is what asks for
# Execute, and once it does, the same archive rule pulls neutrinoapi.o in
# after it. Neither object's constructor is why either is linked, there or
# anywhere: both carry a handful of static std::string members copied
# unchanged from the files they replace, and that is what runs, not what
# pulls them in. Every other object here still answers to the rule unchanged,
# except the three openapi-dump*.o objects below, excluded for a different
# reason stated where openapi_dump is declared.
#
# The tool is the one the build was configured with, so a cross build reads its
# own objects rather than being read by the host's binutils. An object whose
# sections it cannot list at all is a refusal and not a pass: a check that
# answers nothing when its tool is missing is a check that cannot fail.
check-sources:
	@orphan=`cd $(srcdir) && for f in *.cpp compat/*.cpp ep/*.cpp doc/*.cpp; do \
		test -f "$$f" || continue; \
		case " $(libhttpd_a_SOURCES) $(openapi_dump_SOURCES) $(COMPAT_SOURCES) " in \
		*" $$f "*) ;; \
		*) echo $$f;; \
		esac; \
	done`; \
	test -z "$$orphan" || { echo "in src/httpd and in no source list:$$orphan" >&2; exit 1; }

# A sanitizer build is the one place this rule has to stand aside, and it says
# so out loud rather than passing or failing quietly. The address sanitizer
# plants a module constructor of its own in every object it compiles, so every
# object here trips a rule about constructors and a plain make in such a tree
# cannot finish. What the rule is about is a unit this directory wrote whose
# only effect is a constructor; a constructor the instrumentation put there says
# nothing about that, and a sanitizer build is not what is shipped. Standing
# aside in silence is the shape this branch has spent its whole length hunting,
# so the three lines that say it are the point of the case below.
check-no-constructors: $(noinst_LIBRARIES)
	@case " $(CXXFLAGS) $(AM_CXXFLAGS) $(CPPFLAGS) $(AM_CPPFLAGS) " in \
	*-fsanitize*) \
		echo "src/httpd: the check for global constructors is standing aside," >&2; \
		echo "  because this build carries -fsanitize and the instrumentation" >&2; \
		echo "  puts a constructor of its own in every object it compiles" >&2; \
		exit 0;; \
	esac; \
	for o in *.$(OBJEXT) compat/*.$(OBJEXT) ep/*.$(OBJEXT) doc/*.$(OBJEXT); do \
		test -f "$$o" || continue; \
		case "$$o" in compat/controlapi.$(OBJEXT)|compat/neutrinoapi.$(OBJEXT)) continue;; esac; \
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

# A member of an answer that states the whole of what it can be, against the
# code that writes it. Here and not with the cases, because what it reads is
# this directory: the row is beside the handler and so is the naming function
# the row has to agree with, and the comparison is over the two of them.
#
# The comment blanker is the suite's, named across rather than copied. A word
# behind // has to count as absent in both halves of the comparison, and a
# second reader of C comments in this tree would be a second thing that can be
# wrong about one.
check-answer-sets:
	@$(SHELL) $(srcdir)/check-answer-sets.sh $(srcdir) $(top_srcdir)/test/unit/scan/strip-comments.awk

.PHONY: check-sources check-no-constructors check-answer-sets

all-local: check-sources check-no-constructors check-answer-sets

# nhttpd.conf.in is the removed server's configuration template, kept for the
# reason its own head gives. Distributed and installed nowhere: nothing is
# generated from it any more, and the one reader it has is the suite.
EXTRA_DIST = check-answer-sets.sh nhttpd.conf.in

# The document the route above answers, written out without starting anything.
#
# A program of its own and not part of the daemon, because what reads the
# document is a step of the build of the pages: a client's types come out of it,
# and a step that needed a running box to get at it would put a box in front of
# every compile. The tables are constants the linker wrote, so the same walk
# over them runs here as runs behind the route and the bytes are the same ones.
#
# EXTRA_PROGRAMS and not noinst_PROGRAMS, so nothing builds it unless it is
# asked for by name. It is a link of the whole server against the layer under
# it, which is not a cost to add to every build for a file nothing in the
# product reads.
#
# check-no-constructors skips openapi-dump*.o for a reason that has nothing
# to do with compat/controlapi.o and compat/neutrinoapi.o's exception above:
# these three are never members of a convenience archive at all, they are
# object files named directly in this program's own SOURCES, and a linker
# never drops a directly named object the way it drops an unwanted archive
# member. openapi-dump-globals.o's global carries a real constructor
# (SNeutrinoSettings has std::string fields) and it runs, every time, exactly
# as written; the rule this file polices does not apply to it.
EXTRA_PROGRAMS = openapi_dump

openapi_dump_SOURCES = doc/openapi-dump.cpp doc/openapi-dump-stubs.cpp doc/openapi-dump-globals.cpp

# The archive that owns the tables first and the layer under it after, because
# a static archive only answers symbols something already on the line asked
# for. The settings file reader is named for the reason the suite names it: the
# server reads its own configuration through the library the tree already has
# and that library is built here rather than found on the system. The crypto
# library is named because the derivation and the constant time comparison are
# in the archive and nothing else on this line brings them.
#
# helpers.o and debug.o are named for the same reason coreapi_tests names
# them: the mount point in server.cpp builds a CyhookHandler on every call,
# which pulls in Serializer's out-of-line outValue, and that escapes a value
# with the tree's own UTF-8 conversions rather than a copy of them. helpers.o
# also carries a URL fetcher and a JSON parser nothing here calls, so curl and
# the JSON archive are named for the same reason the settings reader is: to
# answer a symbol the object needs, not because this program uses either.
# md5sum answers the one hash helpers.o computes over a whole file.
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
	@MHD_LIBS@ \
	-lcrypto \
	-lz \
	-lpthread

# Written through a temporary and moved, so a run that stopped half way leaves
# the file that was there rather than half of a new one for the next reader to
# take as whole.
openapi.json: openapi_dump$(EXEEXT)
	@./openapi_dump$(EXEEXT) > $@.tmp && mv -f $@.tmp $@ || { rm -f $@.tmp; exit 1; }

openapi-dump: openapi.json
	@echo "$(abs_builddir)/openapi.json"

.PHONY: openapi-dump

CLEANFILES = openapi_dump$(EXEEXT) openapi.json openapi.json.tmp
