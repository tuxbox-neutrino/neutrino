/*
 * test_system_proc.cpp - the process list and the kernel's log
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "support/catch.hpp"

#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"
#include "coreapi/system.h"
#include "coreapi/box/proc_internal.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

/* What GET /api/v1/system/processes and GET /api/v1/system/kernel-log answer
   through: a process table read out of a directory shaped like /proc, and the
   kernel's log taken apart into its lines and levels. */

using namespace coreapi;

namespace
{

// A directory shaped like /proc, and the reader put back afterwards.
struct FakeProc
{
	std::string dir;
	const char *saved_root;
	system::internal::KernelLogReader saved_reader;

	FakeProc() : saved_root(system::internal::proc_root), saved_reader(system::internal::kernel_log_reader)
	{
		char tmpl[] = "/tmp/coreapi_proc_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		system::internal::proc_root = dir.c_str();
	}

	~FakeProc()
	{
		system::internal::proc_root = saved_root;
		system::internal::kernel_log_reader = saved_reader;
		if (!dir.empty())
			(void) std::system(("rm -rf '" + dir + "'").c_str());
	}

	void put(const std::string &name, const std::string &bytes)
	{
		const std::string path = dir + "/" + name;
		const size_t slash = path.rfind('/');
		(void) mkdir(path.substr(0, slash).c_str(), 0755);
		FILE *f = std::fopen(path.c_str(), "w");
		if (f != NULL)
		{
			std::fwrite(bytes.data(), 1, bytes.size(), f);
			std::fclose(f);
		}
	}

	// A stat line with the fields this reads at their places: state, ppid,
	// vsize (23rd) and rss in pages (24th).
	static std::string stat(int pid, const std::string &name, char state, int ppid,
	                        unsigned long long vsize, unsigned long long rss_pages)
	{
		char buf[512];
		std::snprintf(buf, sizeof(buf),
			      "%d (%s) %c %d 1 1 0 -1 4194560 100 0 0 0 5 3 0 0 20 0 1 0 100 %llu %llu 18446744073709551615\n",
			      pid, name.c_str(), state, ppid, vsize, rss_pages);
		return buf;
	}

	private:
		FakeProc(const FakeProc &);
		FakeProc &operator=(const FakeProc &);
};


Result<std::string> logOfThree()
{
	return ok(std::string("<6>[    0.000000] Booting Linux\n<3>[    1.5] usb 1-1: device not accepting address\n"
	                      "no level here\n\n<14>user facility, level 6\n"));
}

Result<std::string> logRefused()
{
	return fail(Status::NotSupported, ErrorCode::KernelLogUnreadable, "the kernel does not hand its log to this program");
}

} // namespace

TEST_CASE("the process list is read out of the process table, by number", "[system][processes]")
{
	FakeProc p;
	REQUIRE_FALSE(p.dir.empty());

	p.put("42/stat", FakeProc::stat(42, "neutrino", 'S', 1, 300000000ULL, 1000));
	p.put("42/cmdline", std::string("/usr/bin/neutrino\0-v\0", 21));
	// A name with spaces and brackets of its own, counted from the last bracket.
	p.put("7/stat", FakeProc::stat(7, "odd (name) x", 'R', 0, 0, 0));
	p.put("7/cmdline", "");
	// Neither a process nor one that can be read.
	p.put("self/stat", FakeProc::stat(1, "not a number", 'S', 0, 0, 0));
	p.put("99/stat", "99 (broken");

	const Result<std::vector<system::ProcessInfo> > got = system::processes();
	REQUIRE(got.ok());
	const std::vector<system::ProcessInfo> &all = got.value();
	REQUIRE(all.size() == 2);

	REQUIRE(all[0].pid == 7);
	REQUIRE(all[0].name == "odd (name) x");
	REQUIRE(all[0].state == 'R');
	// A kernel thread has no command line and shows its name in brackets.
	REQUIRE(all[0].command == "[odd (name) x]");

	REQUIRE(all[1].pid == 42);
	REQUIRE(all[1].ppid == 1);
	REQUIRE(all[1].state == 'S');
	REQUIRE(all[1].name == "neutrino");
	REQUIRE(all[1].command == "/usr/bin/neutrino -v");
	REQUIRE(all[1].vsize_bytes == 300000000ULL);
	REQUIRE(all[1].rss_bytes == 1000ULL * (unsigned long long) sysconf(_SC_PAGESIZE));
	REQUIRE(all[1].uid == (unsigned) getuid());
}

TEST_CASE("a box without a process table says so", "[system][processes]")
{
	FakeProc p;
	REQUIRE_FALSE(p.dir.empty());
	const std::string gone = p.dir + "/not-there";
	system::internal::proc_root = gone.c_str();
	const Result<std::vector<system::ProcessInfo> > got = system::processes();
	system::internal::proc_root = p.dir.c_str();
	REQUIRE_FALSE(got.ok());
	REQUIRE(got.error().status == Status::NotSupported);
	REQUIRE(got.error().code == ErrorCode::ProcessesUnreadable);
}

TEST_CASE("the kernel's log comes back line by line with each line's level", "[system][kernellog]")
{
	FakeProc p;
	system::internal::kernel_log_reader = &logOfThree;

	const Result<std::vector<system::KernelLogLine> > got = system::kernelLog();
	REQUIRE(got.ok());
	const std::vector<system::KernelLogLine> &all = got.value();
	REQUIRE(all.size() == 4);
	REQUIRE(all[0].level == 6);
	REQUIRE(all[0].text == "[    0.000000] Booting Linux");
	REQUIRE(all[1].level == 3);
	REQUIRE(all[2].level == -1);
	REQUIRE(all[2].text == "no level here");
	// The facility is above the level in the same number; the level is kept.
	REQUIRE(all[3].level == 6);

	system::internal::kernel_log_reader = &logRefused;
	const Result<std::vector<system::KernelLogLine> > refused = system::kernelLog();
	REQUIRE_FALSE(refused.ok());
	REQUIRE(refused.error().code == ErrorCode::KernelLogUnreadable);
}

namespace
{

/* /proc, /sys and /etc of a case's own for the readings the box information
   carries, and the real ones put back afterwards. */
struct FakeRoots
{
	std::string dir, proc, sys, etc;
	const char *saved_proc, *saved_sys, *saved_etc;

	FakeRoots()
		: saved_proc(system::internal::proc_root), saved_sys(system::internal::sys_root),
		  saved_etc(system::internal::etc_root)
	{
		char tmpl[] = "/tmp/coreapi_roots_XXXXXX";
		if (mkdtemp(tmpl) != NULL)
			dir = tmpl;
		proc = dir + "/proc";
		sys = dir + "/sys";
		etc = dir + "/etc";
		system::internal::proc_root = proc.c_str();
		system::internal::sys_root = sys.c_str();
		system::internal::etc_root = etc.c_str();
		system::internal::forgetLoadReadingForTest();
	}

	~FakeRoots()
	{
		system::internal::proc_root = saved_proc;
		system::internal::sys_root = saved_sys;
		system::internal::etc_root = saved_etc;
		system::internal::forgetLoadReadingForTest();
		if (!dir.empty())
			(void) std::system(("rm -rf '" + dir + "'").c_str());
	}

	void put(const std::string &path, const std::string &bytes)
	{
		(void) std::system(("mkdir -p \"$(dirname '" + path + "')\"").c_str());
		FILE *f = std::fopen(path.c_str(), "w");
		REQUIRE(f != NULL);
		std::fwrite(bytes.data(), 1, bytes.size(), f);
		std::fclose(f);
	}

	void zone(int n, const std::string &type, long milli)
	{
		char base[64];
		std::snprintf(base, sizeof(base), "/class/thermal/thermal_zone%d", n);
		put(sys + base + "/type", type + "\n");
		char temp[32];
		std::snprintf(temp, sizeof(temp), "%ld\n", milli);
		put(sys + base + "/temp", temp);
	}
};

} // namespace

TEST_CASE("the processor and its system are read the way their kernels write them", "[system]")
{
	FakeRoots r;
	std::string model;
	unsigned cores = 0;

	// an ARM box names its processor in a line of its own, before or after the cores
	r.put(r.proc + "/cpuinfo", "Processor\t: ARMv7 Processor rev 1 (v7l)\nprocessor\t: 0\nprocessor\t: 1\n"
	      "Hardware\t: BCM7251S\n");
	system::internal::readCpu(model, cores);
	REQUIRE(model == "ARMv7 Processor rev 1 (v7l)");
	REQUIRE(cores == 2);

	// a PC once per core
	r.put(r.proc + "/cpuinfo", "processor\t: 0\nmodel name\t: Intel(R) Core(TM) i7\nprocessor\t: 1\n"
	      "model name\t: Intel(R) Core(TM) i7\n");
	model.clear();
	cores = 0;
	system::internal::readCpu(model, cores);
	REQUIRE(model == "Intel(R) Core(TM) i7");
	REQUIRE(cores == 2);

	// an ARM64 kernel may name it nowhere, and a cpuinfo with no cores still has one
	r.put(r.proc + "/cpuinfo", "BogoMIPS\t: 50.00\n");
	model.clear();
	cores = 0;
	system::internal::readCpu(model, cores);
	REQUIRE(model.empty());
	REQUIRE(cores == 1);

	r.put(r.etc + "/os-release", "NAME=Tuxbox\nPRETTY_NAME='Tuxbox OS 4.0'\nVERSION_ID=4.0\n");
	REQUIRE(system::internal::readOsName() == "Tuxbox OS 4.0");
	r.put(r.etc + "/os-release", "PRETTY_NAME=\"Debian GNU/Linux 13 (trixie)\"\n");
	REQUIRE(system::internal::readOsName() == "Debian GNU/Linux 13 (trixie)");
}

TEST_CASE("the load is what the processors spent busy between two readings", "[system]")
{
	FakeRoots r;
	unsigned long long total = 0, idle = 0;

	// ten fields, as a current kernel writes them: guest time is in user already
	r.put(r.proc + "/stat", "cpu  100 0 50 800 50 0 0 0 7 7\ncpu0 1 2 3 4\n");
	REQUIRE(system::internal::cpuTimes(total, idle));
	REQUIRE(total == 1000);
	REQUIRE(idle == 850);

	REQUIRE(system::internal::loadBetween(1000, 850, 2000, 1600) == 25);
	REQUIRE(system::internal::loadBetween(1000, 850, 1000, 850) == -1);   // no time between
	REQUIRE(system::internal::loadBetween(2000, 900, 3000, 800) == -1);   // idle going back
	REQUIRE(system::internal::loadBetween(1000, 0, 1100, 500) == 0);      // idle beyond the time spent

	// unreadable: said as such
	r.put(r.proc + "/stat", "intr 1 2 3\n");
	REQUIRE(system::internal::cpuLoadPercent() == -1);

	double avg[3];
	unsigned running = 9, all = 9;
	r.put(r.proc + "/loadavg", "0.50 0.25 0.12 2/143 4321\n");
	system::internal::readLoad(avg, running, all);
	REQUIRE(avg[0] == 0.5);
	REQUIRE(avg[2] == 0.12);
	REQUIRE(running == 2);
	REQUIRE(all == 143);
	r.put(r.proc + "/loadavg", "garbage\n");
	system::internal::readLoad(avg, running, all);
	REQUIRE(avg[0] == 0);
	REQUIRE(all == 0);
}

TEST_CASE("the temperature comes from whichever thermal zones there are", "[system]")
{
	FakeRoots r;
	int celsius = 0;
	REQUIRE_FALSE(system::internal::readTemperature(celsius));

	// zones numbered with gaps: the lowest-numbered one where nothing names the processor
	r.zone(3, "soc-thermal", 52000);
	r.zone(1, "board", 48000);
	REQUIRE(system::internal::readTemperature(celsius));
	REQUIRE(celsius == 48);

	// cores: the hottest of them
	r.zone(5, "Core 0", 55000);
	r.zone(6, "Core 1", 57000);
	REQUIRE(system::internal::readTemperature(celsius));
	REQUIRE(celsius == 57);

	// a package zone before any core
	r.zone(9, "x86_pkg_temp", 60000);
	REQUIRE(system::internal::readTemperature(celsius));
	REQUIRE(celsius == 60);
}
