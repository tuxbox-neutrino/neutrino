/*
 * test_result.cpp - tests for the result type
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "support/catch.hpp"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include "support/counts.h"

#include <set>

#include <cstdlib>
#include <sys/stat.h>

#include <csignal>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

/* One code the suite walks and the name it is spelt by. The name is here so
   that what the header declares and what this walks can be compared as text,
   and the macro is what keeps the two halves of an entry from disagreeing: the
   string cannot end up being the name of some other enumerator. */
struct Walked
{
	ErrorCode   code;
	const char *name;
};

#define CODE(name) { ErrorCode::name, #name }

const Walked kWalked[] = {
	CODE(NoSuchChannel),
	CODE(NoSuchBouquet),
	CODE(NoSuchTimer),
	CODE(NoSuchDaemon),
	CODE(NoSuchVerb),
	CODE(NoSuchCommand),
	CODE(NoSuchRoute),
	CODE(NoSuchMethod),
	CODE(NoSuchParameter),
	CODE(NoSuchTimerType),
	CODE(NoSuchName),
	CODE(NoSuchParent),
	CODE(NoSuchFilesystem),
	CODE(UnknownSetting),
	CODE(ScriptMissing),
	CODE(NoMountTable),
	CODE(NoSuchKey),
	CODE(NoSuchLogo),
	CODE(NoSuchEvent),
	CODE(NoSuchRecording),
	CODE(NoSuchPart),
	CODE(NoRunningChannel),
	CODE(NoMode),
	CODE(NoCurrentEvent),
	CODE(NoTuner),
	CODE(NoTimeshift),
	CODE(EmptyWindow),
	CODE(QueryTooShort),
	CODE(NoRoomForAResult),
	CODE(LimitTooLarge),
	CODE(EmptyMessage),
	CODE(MessageTooLong),
	CODE(VolumeOutOfRange),
	CODE(EmptyPath),
	CODE(RelativePath),
	CODE(PathHasZeroByte),
	CODE(PathTooLong),
	CODE(OutsideRoots),
	CODE(AccessStore),
	CODE(BadName),
	CODE(TimerWithoutId),
	CODE(TimerWithoutChannel),
	CODE(RecordingWithoutDuration),
	CODE(TimerInThePast),
	CODE(NotACommand),
	CODE(DuplicateParameter),
	CODE(MissingParameter),
	CODE(ConflictingParameters),
	CODE(LeadingDash),
	CODE(BadInt),
	CODE(BadBool),
	CODE(BadEnum),
	CODE(AlreadyAtTheEnd),
	CODE(BadString),
	CODE(BadPath),
	CODE(ValueHasZeroByte),
	CODE(ValueTooLong),
	CODE(BodyTooLarge),
	CODE(RangeOutsideFile),
	CODE(TooManyChannels),
	CODE(NotANumber),
	CODE(OutOfRange),
	CODE(NotAListedValue),
	CODE(EmptyCredential),
	CODE(NotACredential),
	CODE(NotPermitted),
	CODE(NoAuthority),
	CODE(NotAWebChannel),
	CODE(AddressRefused),
	CODE(NotAPlaylist),
	CODE(PlaylistUnreadable),
	CODE(TooManyStreams),
	CODE(TooManyConversions),
	CODE(NotPlayableInBrowser),
	CODE(TooManyAttempts),
	CODE(TimerExists),
	CODE(NameTaken),
	CODE(NotAPlainFile),
	CODE(SideFileInTheWay),
	CODE(NotEmpty),
	CODE(TimeshiftRunning),
	CODE(BadScript),
	CODE(BadTable),
	CODE(NoTimeout),
	CODE(NoOutputRoom),
	CODE(NoValues),
	CODE(NoRoot),
	CODE(NoRootToResolve),
	CODE(CommandNotSent),
	CODE(SessionNotOpened),
	CODE(CommandNotPosted),
	CODE(TimerNotCreated),
	CODE(TimerNotChanged),
	CODE(TimerNotRemoved),
	CODE(TimerStillThere),
	CODE(RecordingNotStopped),
	CODE(LogosNotOffered),
	CODE(TunerNotReset),
	CODE(NetfsTableNotWritten),
	CODE(ChangeRefused),
	CODE(SettingNotWritten),
	CODE(BouquetNotChanged),
	CODE(PluginNotChanged),
	CODE(KeyNotSent),
	CODE(ScreenNotCaptured),
	CODE(DisplayNotCaptured),
	CODE(ChannelListUnavailable),
	CODE(UpstreamUnreachable),
	CODE(BouquetListUnavailable),
	CODE(ModeUnavailable),
	CODE(CurrentChannelUnresolved),
	CODE(EpgUnavailable),
	CODE(VolumeUnavailable),
	CODE(MuteUnavailable),
	CODE(SettingUnreadable),
	CODE(BoxUnreadable),
	CODE(DecryptionUnreadable),
	CODE(TimerListUnavailable),
	CODE(ClockUnavailable),
	CODE(NetfsTableUnreadable),
	CODE(FrontendListUnavailable),
	CODE(SignalUnavailable),
	CODE(RemoteLockUnreadable),
	CODE(PluginListUnavailable),
	CODE(RecordingListUnavailable),
	CODE(ConfigDocumentUnreadable),
	CODE(WebserverNotConfigured),
	CODE(CommandNotStarted),
	CODE(CommandOutputUnreadable),
	CODE(Timeout),
	CODE(OutputTooLarge),
	CODE(NotStopped),
	CODE(NoSupervision),
	CODE(SupervisionLost),
	CODE(NoExitStatus),
	CODE(DidNotExit),
	CODE(Killed),
	CODE(ExitStatus),
	CODE(ChoicesUnavailable)
};

#undef CODE

const size_t kWalkedCount = sizeof(kWalked) / sizeof(kWalked[0]);

/* Runs the scan over a header written here rather than over the tree's, so that
   a shape it could pass over is planted and not waited for. The one that
   matters is an enumerator sharing a line with a brace: the scan discarded the
   opening line whole and printed a set short by one, and a set short by one
   that both sides agree on is exactly what this comparison exists to stop. */
struct Extraction
{
	std::vector<std::string> names;
	int                      status;
	// What it said while refusing, because a refusal that names the wrong
	// cause sends whoever reads it to the wrong line and is a defect of its
	// own.
	std::string              said;
};

Extraction runEnumScan(const std::string &header)
{
	Extraction out;
	out.status = -1;

	char root[] = "/tmp/coreapi-enumscan-XXXXXX";
	if (mkdtemp(root) == NULL)
		return out;

	const std::string src = std::string(root) + "/src";
	const std::string dir = src + "/coreapi";
	const std::string path = dir + "/planted.h";

	if (mkdir(src.c_str(), 0700) == 0 && mkdir(dir.c_str(), 0700) == 0)
	{
		FILE *h = fopen(path.c_str(), "w");
		if (h != NULL)
		{
			fwrite(header.data(), 1, header.size(), h);
			fclose(h);

			const std::string errors = std::string(root) + "/said";
			const std::string cmd = std::string("sh ") + COREAPI_ENUM_SCRIPT + " " + root +
			                        " src/coreapi/planted.h ErrorCode 60 2>" + errors;
			FILE *p = popen(cmd.c_str(), "r");
			if (p != NULL)
			{
				std::string line;
				int c;
				for (;;)
				{
					c = fgetc(p);
					if (c != EOF && c != '\n')
					{
						line += (char) c;
						continue;
					}
					if (!line.empty())
						out.names.push_back(line.substr(0, line.find('\t')));
					line.clear();
					if (c == EOF)
						break;
				}
				const int rc = pclose(p);
				out.status = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
			}

			FILE *e = fopen(errors.c_str(), "r");
			if (e != NULL)
			{
				for (;;)
				{
					const int ec = fgetc(e);
					if (ec == EOF)
						break;
					out.said += (char) ec;
				}
				fclose(e);
			}
			unlink(errors.c_str());
			unlink(path.c_str());
		}
		rmdir(dir.c_str());
		rmdir(src.c_str());
	}
	rmdir(root);
	return out;
}

// Enough members to clear the floor the scan refuses to print below, so that a
// planted header is read the way the tree's own is and not as a short answer.
std::string plantedHeader(const char *opening, const char *closing)
{
	std::string h = "namespace coreapi\n{\n";
	h += opening;
	h += "\n";
	for (int i = 0; i < 60; i++)
	{
		char filler[32];
		snprintf(filler, sizeof(filler), "\tFiller%d,\n", i);
		h += filler;
	}
	h += closing;
	h += "\n}\n";
	return h;
}

/* The names the header declares, written out by the script beside this before
   the suite runs. The script prints a position beside each name, which this
   comparison does not need and drops. Unreadable leaves the set empty, which
   fails the case that compares it rather than passing over nothing. */
const std::set<std::string> &declaredCodes()
{
	static std::set<std::string> s;
	static bool done = false;
	if (done)
		return s;
	done = true;

	FILE *f = fopen(COREAPI_ERRORCODES_FILE, "r");
	if (f == NULL)
		return s;

	std::string line;
	int c;
	for (;;)
	{
		c = fgetc(f);
		if (c != EOF && c != '\n')
		{
			line += (char) c;
			continue;
		}
		if (!line.empty())
			s.insert(line.substr(0, line.find('\t')));
		line.clear();
		if (c == EOF)
			break;
	}

	fclose(f);
	return s;
}

} // namespace

/* The strings are the contract and the enumerators are not: a client branches on what
   goes on the wire. No client has seen any of these yet, so nothing is being preserved
   here. What is being fixed is the set the next plan is written against: these eleven
   are what the one endpoint that answers with a code answers with today, and a change
   to one of them after the frontend is written is a break. */
TEST_CASE("the codes the file endpoint answers with are written down", "[result]")
{
	struct Pair { ErrorCode code; const char *wire; };
	static const Pair emitted[] = {
		{ ErrorCode::OutsideRoots,    "outside-roots" },
		{ ErrorCode::AccessStore,     "access-store" },
		{ ErrorCode::EmptyPath,       "empty-path" },
		{ ErrorCode::RelativePath,    "relative-path" },
		{ ErrorCode::PathHasZeroByte, "path-has-zero-byte" },
		{ ErrorCode::PathTooLong,     "path-too-long" },
		{ ErrorCode::NoSuchName,      "no-such-name" },
		{ ErrorCode::NoSuchParent,    "no-such-parent" },
		{ ErrorCode::BadName,         "bad-name" },
		{ ErrorCode::NameTaken,       "name-taken" },
		{ ErrorCode::NotAPlainFile,   "not-a-plain-file" },
		{ ErrorCode::SideFileInTheWay, "side-file-in-the-way" },
		{ ErrorCode::NotEmpty,        "not-empty" },
		{ ErrorCode::ChangeRefused,   "change-refused" }
	};
	for (size_t i = 0; i < sizeof(emitted) / sizeof(emitted[0]); i++)
		REQUIRE(std::string(codeString(emitted[i].code)) == emitted[i].wire);
}

/* One convention for one outcome class, which is the whole reason the set is
   written down in one place. */
TEST_CASE("everything a lookup did not find is spelt the same way", "[result]")
{
	static const ErrorCode absent[] = {
		ErrorCode::NoSuchChannel, ErrorCode::NoSuchBouquet, ErrorCode::NoSuchTimer,
		ErrorCode::NoSuchDaemon, ErrorCode::NoSuchVerb, ErrorCode::NoSuchCommand,
		ErrorCode::NoSuchRoute, ErrorCode::NoSuchMethod,
		ErrorCode::NoSuchParameter, ErrorCode::NoSuchTimerType, ErrorCode::NoSuchName,
		ErrorCode::NoSuchParent, ErrorCode::NoSuchFilesystem,
		ErrorCode::UnknownSetting, ErrorCode::NoSuchRecording
	};
	for (size_t i = 0; i < sizeof(absent) / sizeof(absent[0]); i++)
	{
		const std::string wire = codeString(absent[i]);
		REQUIRE(wire.compare(0, 8, "no-such-") == 0);
	}
}

/* Every code, not a sample: a code with no case in the projection answers with
   the empty string, and nothing else in either build catches that. The list is
   written out rather than derived from the header, because a walk derived from
   the same text the projection sits in would pass over a code that is missing
   from both. The case below is what holds the written list to the header. */
TEST_CASE("every code has its own words to send", "[result]")
{
	std::set<std::string> seen;
	for (size_t i = 0; i < kWalkedCount; i++)
	{
		const std::string wire = codeString(kWalked[i].code);
		INFO(kWalked[i].name);
		REQUIRE(wire.size() > 0);
		// Two codes with one string is the defect this set exists to remove,
		// read from the other end.
		REQUIRE(seen.insert(wire).second);
	}
	REQUIRE(seen.size() == kWalkedCount);
}

/* The list above against the header it was transcribed from, both ways round. Written
   by hand it says nothing about whether it is still the whole set: a code added to the
   enum and not to it is simply unwalked. Reading the header at build time is what makes
   the sentence at the top of errors.h true.

   Both directions, because they fail for different reasons. A name the header declares
   and the list does not is a code somebody added without walking it. A name the list
   holds and the header does not cannot be a deleted enumerator, since the entry would
   not compile, so it is the scan having stopped reading part of the header. */
TEST_CASE("the walk is held to what the header declares", "[result]")
{
	const std::set<std::string> &declared = declaredCodes();
	INFO("read from " << COREAPI_ERRORCODES_FILE);
	REQUIRE(declared.size() > 0);

	std::set<std::string> walked;
	for (size_t i = 0; i < kWalkedCount; i++)
	{
		INFO(kWalked[i].name);
		REQUIRE(walked.insert(kWalked[i].name).second);
	}

	for (std::set<std::string>::const_iterator it = declared.begin(); it != declared.end(); ++it)
	{
		INFO("declared in errors.h and not walked here: " << *it);
		REQUIRE(walked.count(*it) == 1);
	}
	for (std::set<std::string>::const_iterator it = walked.begin(); it != walked.end(); ++it)
	{
		INFO("in the walk and not in what the scan read: " << *it
		     << ", so the scan has stopped reading part of errors.h."
		     << " A deleted enumerator cannot be the cause: the entry would not compile");
		REQUIRE(declared.count(*it) == 1);
	}

	recordCount("error codes the header declares", declared.size());
}

/* The scan itself, over headers written for the occasion. Everything else here
   compares two sets the scan produced one side of, so a shape it reads short is
   invisible from in there: the walk would be missing the same name and the two
   would agree. A member sharing a line with either brace is that shape, and the
   compiler reads both. */
TEST_CASE("the scan reads an enumerator that shares a line with a brace", "[result]")
{
	const Extraction plain = runEnumScan(plantedHeader("enum class ErrorCode\n{", "\tLastOne\n};"));
	INFO("one enumerator per line");
	REQUIRE(plain.status == 0);
	REQUIRE(plain.names.size() == 61);

	const Extraction opening = runEnumScan(plantedHeader("enum class ErrorCode { SneakyCode,", "\tLastOne\n};"));
	INFO("an enumerator after the opening brace");
	REQUIRE(opening.status == 0);
	REQUIRE(opening.names.size() == 62);
	REQUIRE(opening.names.front() == "SneakyCode");

	const Extraction closing = runEnumScan(plantedHeader("enum class ErrorCode\n{", "\tLastOne };"));
	INFO("an enumerator before the closing brace");
	REQUIRE(closing.status == 0);
	REQUIRE(closing.names.size() == 61);
	REQUIRE(closing.names.back() == "LastOne");

	// A value written out makes the position this prints no longer the value the
	// compiler assigns, which a walk that casts it back would get wrong.
	const Extraction valued = runEnumScan(plantedHeader("enum class ErrorCode\n{", "\tLastOne = 900\n};"));
	INFO("an enumerator stating a value of its own");
	REQUIRE(valued.status == 1);
	REQUIRE(valued.names.empty());
	REQUIRE(valued.said.find("states a value of its own") != std::string::npos);

	/* A declaration of the enum ahead of the one that names its members. It
	   states nothing to read and the real one is further down, so it is passed
	   over: a scan that entered it would read the code in between as members
	   and refuse the file over the first equals sign in that code. */
	const Extraction forward =
		runEnumScan(plantedHeader("enum class ErrorCode;\n\nconst int kBetween = 5;\n\n"
		                          "enum class ErrorCode\n{", "\tLastOne\n};"));
	INFO("a declaration ahead of the enum: " + forward.said);
	REQUIRE(forward.status == 0);
	REQUIRE(forward.names.size() == 61);
	REQUIRE(forward.names.front() == "Filler0");

	/* An enum whose closing brace never arrives. What stops such a scan is
	   whatever comes first in the rest of the file, so the refusal has to name
	   the enum and not the line it happened to stop on. */
	const Extraction unclosed =
		runEnumScan(plantedHeader("enum class ErrorCode\n{",
		                          "\tLastOne,\n\nconst int kAfter = 5;"));
	INFO("an enum never closed: " + unclosed.said);
	REQUIRE(unclosed.status == 1);
	REQUIRE(unclosed.names.empty());
	REQUIRE(unclosed.said.find("never closed") != std::string::npos);
	REQUIRE(unclosed.said.find("states a value of its own") == std::string::npos);
}

TEST_CASE("ok carries a value", "[result]")
{
	Result<int> r = ok(42);
	REQUIRE(r.ok());
	REQUIRE(r.value() == 42);
}

TEST_CASE("fail carries a status and message", "[result]")
{
	Result<int> r = fail<int>(Status::NotFound, ErrorCode::NoSuchChannel, "no such channel");
	REQUIRE_FALSE(r.ok());
	REQUIRE(r.error().status == Status::NotFound);
	REQUIRE(r.error().code == ErrorCode::NoSuchChannel);
	REQUIRE(r.error().message == std::string("no such channel"));
}

TEST_CASE("void result reports success without a value", "[result]")
{
	Result<void> good = ok();
	REQUIRE(good.ok());

	Result<void> bad = fail(Status::Busy, ErrorCode::Timeout, "all frontends in use");
	REQUIRE_FALSE(bad.ok());
	REQUIRE(bad.error().status == Status::Busy);
	REQUIRE(bad.error().code == ErrorCode::Timeout);
	REQUIRE(bad.error().message == std::string("all frontends in use"));
}

TEST_CASE("copying a failure keeps the whole error", "[result]")
{
	Result<std::string> a = fail<std::string>(Status::Conflict, ErrorCode::NameTaken, "already recording");
	Result<std::string> b = a;
	REQUIRE_FALSE(b.ok());
	REQUIRE(b.error().status == Status::Conflict);
	REQUIRE(b.error().code == ErrorCode::NameTaken);
	REQUIRE(b.error().message == std::string("already recording"));

	Result<std::string> c = ok(std::string("x"));
	c = a;
	REQUIRE_FALSE(c.ok());
	REQUIRE(c.error().code == ErrorCode::NameTaken);
	c = b = ok(std::string("y"));
	REQUIRE(c.ok());
	REQUIRE(c.value() == "y");
}

static void takeValueOfFailure()
{
	Result<int> r = fail<int>(Status::Internal, ErrorCode::BoxUnreadable, "nothing to read");
	r.value();
}

static void takeErrorOfSuccess()
{
	Result<int> r = ok(1);
	r.error();
}

static void takeErrorOfVoidSuccess()
{
	Result<void> r = ok();
	r.error();
}

// The overloads that move out carry their own guard, which a caller reaches
// only through an rvalue.
static void takeMovedValueOfFailure()
{
	Result<int> r = fail<int>(Status::Internal, ErrorCode::BoxUnreadable, "nothing to read");
	std::move(r).value();
}

static void takeMovedErrorOfSuccess()
{
	Result<int> r = ok(1);
	std::move(r).error();
}

static void takeMovedErrorOfVoidSuccess()
{
	Result<void> r = ok();
	std::move(r).error();
}

// A const rvalue reaches its own overloads, which carry the same guard.
static void takeMovedValueOfConstFailure()
{
	const Result<int> r = fail<int>(Status::Internal, ErrorCode::BoxUnreadable, "nothing to read");
	std::move(r).value();
}

static void takeMovedErrorOfConstSuccess()
{
	const Result<int> r = ok(1);
	std::move(r).error();
}

static void takeMovedErrorOfConstVoidSuccess()
{
	const Result<void> r = ok();
	std::move(r).error();
}

// The child restores the default SIGABRT action and silences its output because
// it inherits the test runner's fatal condition handler, which would otherwise
// report the deliberate abort as a failure.
static bool abortsInChild(void (*body)())
{
	// the child's freopen would otherwise flush the runner's buffered output
	// a second time and make one run look like two
	fflush(0);

	pid_t pid = fork();
	if (pid < 0)
		return false;
	if (pid == 0) {
		signal(SIGABRT, SIG_DFL);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
		body();
		_exit(0);
	}
	int st = 0;
	waitpid(pid, &st, 0);
	return WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
}

TEST_CASE("reading the value of a failure aborts", "[result]")
{
	REQUIRE(abortsInChild(takeValueOfFailure));
}

TEST_CASE("reading the error of a success aborts", "[result]")
{
	REQUIRE(abortsInChild(takeErrorOfSuccess));
}

TEST_CASE("reading the error of a void success aborts", "[result]")
{
	REQUIRE(abortsInChild(takeErrorOfVoidSuccess));
}

TEST_CASE("moving the value out of a failure aborts", "[result]")
{
	REQUIRE(abortsInChild(takeMovedValueOfFailure));
}

TEST_CASE("moving the error out of a success aborts", "[result]")
{
	REQUIRE(abortsInChild(takeMovedErrorOfSuccess));
}

TEST_CASE("moving the error out of a void success aborts", "[result]")
{
	REQUIRE(abortsInChild(takeMovedErrorOfVoidSuccess));
}

TEST_CASE("moving the value out of a const failure aborts", "[result]")
{
	REQUIRE(abortsInChild(takeMovedValueOfConstFailure));
}

TEST_CASE("moving the error out of a const success aborts", "[result]")
{
	REQUIRE(abortsInChild(takeMovedErrorOfConstSuccess));
}

TEST_CASE("moving the error out of a const void success aborts", "[result]")
{
	REQUIRE(abortsInChild(takeMovedErrorOfConstVoidSuccess));
}

static Result<std::string> makeName()
{
	return ok(std::string("Das Erste HD"));
}

static Result<int> makeFailure()
{
	return fail(Status::Conflict, ErrorCode::NameTaken, "already recording");
}

TEST_CASE("a reference into a temporary result stays readable", "[result]")
{
	const std::string &name = makeName().value();
	REQUIRE(name == "Das Erste HD");

	const Error &e = makeFailure().error();
	REQUIRE(e.code == ErrorCode::NameTaken);
	REQUIRE(e.message == std::string("already recording"));
}

static Result<int> lookup(bool found)
{
	if (!found)
		return fail(Status::NotFound, ErrorCode::NoSuchChannel, "unknown channel");
	return ok(7);
}

static Result<std::string> describe(bool found)
{
	Result<int> r = lookup(found);
	if (!r.ok())
		return fail(r.error());
	return ok(std::string("found"));
}

TEST_CASE("fail returns and propagates without naming the payload type", "[result]")
{
	Result<int> missing = lookup(false);
	REQUIRE_FALSE(missing.ok());
	REQUIRE(missing.error().status == Status::NotFound);

	Result<std::string> passed = describe(false);
	REQUIRE_FALSE(passed.ok());
	REQUIRE(passed.error().status == Status::NotFound);
	REQUIRE(passed.error().code == ErrorCode::NoSuchChannel);
	REQUIRE(passed.error().message == std::string("unknown channel"));

	REQUIRE(describe(true).ok());
}

namespace
{

struct Counted
{
	static int copies;

	Counted() {}
	Counted(const Counted &) { ++copies; }
	Counted(Counted &&) {}
	Counted &operator=(const Counted &) { ++copies; return *this; }
	Counted &operator=(Counted &&) { return *this; }
};

int Counted::copies = 0;

}

TEST_CASE("a moved payload is not copied into the result", "[result]")
{
	Counted lvalue;
	Counted::copies = 0;
	Result<Counted> copied = ok(lvalue);
	REQUIRE(copied.ok());
	REQUIRE(Counted::copies > 0);

	Counted rvalue;
	Counted::copies = 0;
	Result<Counted> moved = ok(std::move(rvalue));
	REQUIRE(moved.ok());
	REQUIRE(Counted::copies == 0);
}

TEST_CASE("extracting from an rvalue result moves instead of copying", "[result]")
{
	std::vector<std::string> in;
	in.resize(64, std::string("Das Erste HD"));
	const std::string *buffer = &in[0];

	Result<std::vector<std::string> > r = ok(std::move(in));
	REQUIRE(r.ok());
	// ok() must not have reallocated on the way in.
	REQUIRE(&r.value()[0] == buffer);

	std::vector<std::string> out = std::move(r).value();
	REQUIRE(out.size() == 64);
	// The same buffer, so nothing between here and the source copied it.
	REQUIRE(&out[0] == buffer);

	// Not the buffer address: a copy on write string keeps that through a copy,
	// so only the emptied source tells a move from a copy under both libraries.
	Result<std::string> e = fail<std::string>(Status::Busy, ErrorCode::Timeout, std::string(200, 'x'));
	Error taken = std::move(e).error();
	REQUIRE(taken.status == Status::Busy);
	REQUIRE(taken.message.size() == 200);
	REQUIRE(e.error().message.empty());
}

TEST_CASE("a void result moves its error out as well", "[result]")
{
	Result<void> r = fail(Status::Conflict, ErrorCode::NameTaken, std::string(200, 'z'));
	Error taken = std::move(r).error();
	REQUIRE(taken.status == Status::Conflict);
	REQUIRE(taken.message.size() == 200);
	REQUIRE(r.error().message.empty());
}

TEST_CASE("a const rvalue result still copies out", "[result]")
{
	std::vector<std::string> in;
	in.resize(8, std::string("ZDF HD"));
	const std::string *buffer = &in[0];

	const Result<std::vector<std::string> > r = ok(std::move(in));
	// Moving out of a const result would tear a value the caller still holds.
	std::vector<std::string> out = std::move(r).value();
	REQUIRE(out.size() == 8);
	REQUIRE(&out[0] != buffer);
	REQUIRE(&r.value()[0] == buffer);
}

TEST_CASE("a const rvalue result copies its error out too", "[result]")
{
	const Result<std::string> e = fail<std::string>(Status::Busy, ErrorCode::Timeout, std::string(200, 'x'));
	Error taken = std::move(e).error();
	REQUIRE(taken.status == Status::Busy);
	REQUIRE(taken.message.size() == 200);
	// The source keeps what it had, which a move would have emptied.
	REQUIRE(e.error().message.size() == 200);

	const Result<void> v = fail(Status::Conflict, ErrorCode::NameTaken, std::string(200, 'z'));
	Error vtaken = std::move(v).error();
	REQUIRE(vtaken.status == Status::Conflict);
	REQUIRE(vtaken.message.size() == 200);
	REQUIRE(v.error().message.size() == 200);
}
