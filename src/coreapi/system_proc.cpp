/*
 * system_proc.cpp - the processes running on the box and the kernel's log
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "system.h"
#include "coreapi/base/errors.h"
#include "coreapi/box/proc_internal.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <dirent.h>
#include <fcntl.h>
#include <sys/klog.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* What the web interface showed by running "ps aux" and "dmesg" through a
   route that ran any command at all. Read here out of /proc and from the
   kernel itself, so nothing is started and nothing but these two can be
   asked for. */

namespace coreapi
{
namespace system
{
namespace internal
{
const char *proc_root = "/proc";
KernelLogReader kernel_log_reader = NULL;
const char *sys_root = "/sys";
const char *etc_root = "/etc";
} // namespace internal

namespace
{

// The whole of a small file under /proc, which has no size to ask for
// beforehand; empty for one that is gone, which a process can be by now.
std::string readSmall(const std::string &path)
{
	std::string out;
	const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return out;
	char buf[4096];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0 && out.size() < 64 * 1024)
		out.append(buf, (size_t) n);
	close(fd);
	return out;
}

bool allDigits(const char *s)
{
	if (*s == '\0')
		return false;
	for (; *s; ++s)
		if (*s < '0' || *s > '9')
			return false;
	return true;
}

/* One process out of its stat line: "pid (name) state ppid ...". The name
   can hold spaces and brackets of its own, so the fields after it are counted
   from the last closing bracket and not from the start. */
bool readProcess(const std::string &dir, int pid, ProcessInfo &p)
{
	const std::string line = readSmall(dir + "/stat");
	const size_t open_at = line.find('(');
	const size_t close_at = line.rfind(')');
	if (open_at == std::string::npos || close_at == std::string::npos || close_at < open_at)
		return false;

	p.pid = pid;
	p.name = line.substr(open_at + 1, close_at - open_at - 1);

	// Fields from the state on, which is the third of the line.
	std::vector<std::string> f;
	size_t i = close_at + 1;
	while (i < line.size())
	{
		while (i < line.size() && line[i] == ' ')
			++i;
		const size_t start = i;
		while (i < line.size() && line[i] != ' ' && line[i] != '\n')
			++i;
		if (i > start)
			f.push_back(line.substr(start, i - start));
		if (i < line.size() && line[i] == '\n')
			break;
	}
	// state is field 3, ppid 4, vsize 23 and rss 24 of the line: here 0, 1, 20, 21
	if (f.size() < 22)
		return false;
	p.state = f[0].empty() ? '?' : f[0][0];
	p.ppid = atoi(f[1].c_str());
	p.vsize_bytes = strtoull(f[20].c_str(), NULL, 10);
	const long page = sysconf(_SC_PAGESIZE);
	p.rss_bytes = strtoull(f[21].c_str(), NULL, 10) * (unsigned long long) (page > 0 ? page : 4096);

	struct stat st;
	p.uid = stat(dir.c_str(), &st) == 0 ? (unsigned) st.st_uid : 0;

	// The command line with its arguments apart by spaces; a kernel thread has
	// none and is shown by its name in brackets, as ps does.
	std::string cmd = readSmall(dir + "/cmdline");
	while (!cmd.empty() && cmd[cmd.size() - 1] == '\0')
		cmd.erase(cmd.size() - 1);
	std::replace(cmd.begin(), cmd.end(), '\0', ' ');
	p.command = cmd.empty() ? "[" + p.name + "]" : cmd;
	return true;
}

Result<std::string> readKernelLog()
{
	// SYSLOG_ACTION_SIZE_BUFFER and SYSLOG_ACTION_READ_ALL, which leaves the
	// log as it is for every other reader.
	const int size = klogctl(10, NULL, 0);
	if (size <= 0)
		return fail(Status::NotSupported, ErrorCode::KernelLogUnreadable,
			    "the kernel does not hand its log to this program");
	std::vector<char> buf((size_t) size);
	const int n = klogctl(3, &buf[0], size);
	if (n < 0)
		return fail(Status::NotSupported, ErrorCode::KernelLogUnreadable,
			    "the kernel does not hand its log to this program");
	return ok(std::string(&buf[0], (size_t) n));
}

} // namespace

Result<std::vector<ProcessInfo> > processes()
{
	const std::string root(internal::proc_root);
	DIR *d = opendir(root.c_str());
	if (d == NULL)
		return fail(Status::NotSupported, ErrorCode::ProcessesUnreadable,
			    "there is no process list to read at " + root);

	std::vector<ProcessInfo> out;
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
	{
		if (!allDigits(e->d_name))
			continue;
		ProcessInfo p;
		// One that has ended since the directory was read is left out.
		if (readProcess(root + "/" + e->d_name, atoi(e->d_name), p))
			out.push_back(p);
	}
	closedir(d);

	std::sort(out.begin(), out.end(), [](const ProcessInfo &a, const ProcessInfo &b) { return a.pid < b.pid; });
	return ok(std::move(out));
}

Result<std::vector<KernelLogLine> > kernelLog()
{
	Result<std::string> got = internal::kernel_log_reader != NULL ? internal::kernel_log_reader() : readKernelLog();
	if (!got.ok())
		return fail(got.error());

	/* Each line as the kernel keeps it, "<level>text": the level taken off the
	   front, where there is one, and the rest left as written. */
	std::vector<KernelLogLine> out;
	const std::string &all = got.value();
	size_t i = 0;
	while (i < all.size())
	{
		size_t e = all.find('\n', i);
		if (e == std::string::npos)
			e = all.size();
		std::string line = all.substr(i, e - i);
		i = e + 1;
		if (line.empty())
			continue;
		KernelLogLine l;
		l.level = -1;
		if (line.size() > 2 && line[0] == '<')
		{
			const size_t gt = line.find('>');
			if (gt != std::string::npos && gt > 1 && gt < 5 &&
			    allDigits(line.substr(1, gt - 1).c_str()))
			{
				l.level = atoi(line.substr(1, gt - 1).c_str()) & 7;
				line.erase(0, gt + 1);
			}
		}
		l.text = line;
		out.push_back(l);
	}
	return ok(std::move(out));
}

namespace internal
{

void readCpu(std::string &model, unsigned &cores)
{
	std::ifstream in((std::string(proc_root) + "/cpuinfo").c_str());
	std::string line, arm;
	while (std::getline(in, line))
	{
		std::string value;
		const size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			const size_t start = line.find_first_not_of(" \t", colon + 1);
			if (start != std::string::npos)
				value = line.substr(start);
		}
		if (line.compare(0, 9, "processor") == 0)
			++cores;
		else if (line.compare(0, 10, "model name") == 0 && model.empty())
			model = value;
		else if ((line.compare(0, 9, "Processor") == 0 || line.compare(0, 8, "Hardware") == 0) && arm.empty())
			arm = value;
	}
	if (model.empty())
		model = arm;
	if (cores == 0)
		cores = 1;
}

bool cpuTimes(unsigned long long &total, unsigned long long &idle)
{
	std::FILE *f = std::fopen((std::string(proc_root) + "/stat").c_str(), "r");
	if (f == NULL)
		return false;
	// user nice system idle iowait irq softirq steal: guest time is counted in user already
	unsigned long long v[8] = { 0 };
	const int got = std::fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3],
				    &v[4], &v[5], &v[6], &v[7]);
	std::fclose(f);
	if (got < 4)
		return false;
	total = 0;
	for (int i = 0; i < 8; ++i)
		total += v[i];
	idle = v[3] + v[4];
	return true;
}

int loadBetween(unsigned long long total1, unsigned long long idle1, unsigned long long total2,
		unsigned long long idle2)
{
	if (total2 <= total1 || idle2 < idle1)
		return -1;
	const unsigned long long spent = total2 - total1;
	const unsigned long long idle = idle2 - idle1 < spent ? idle2 - idle1 : spent;
	return (int) ((spent - idle) * 100 / spent);
}

namespace
{

OpenThreads::Mutex &loadLock()
{
	static OpenThreads::Mutex m;
	return m;
}

struct LoadReading
{
	bool valid;
	long long at_ms;
	unsigned long long total, idle;
	LoadReading() : valid(false), at_ms(0), total(0), idle(0) {}
};

LoadReading &lastReading()
{
	static LoadReading r;
	return r;
}

long long nowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

} // namespace

int cpuLoadPercent()
{
	unsigned long long t2, i2;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> lock(loadLock());
		LoadReading &last = lastReading();
		const long long now = nowMs();
		if (last.valid && now - last.at_ms >= 200 && now - last.at_ms <= 10000)
		{
			if (!cpuTimes(t2, i2))
				return -1;
			const int percent = loadBetween(last.total, last.idle, t2, i2);
			last.at_ms = now;
			last.total = t2;
			last.idle = i2;
			if (percent >= 0)
				return percent;
		}
	}
	unsigned long long t1, i1;
	if (!cpuTimes(t1, i1))
		return -1;
	usleep(100000);
	if (!cpuTimes(t2, i2))
		return -1;
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(loadLock());
	LoadReading &last = lastReading();
	last.valid = true;
	last.at_ms = nowMs();
	last.total = t2;
	last.idle = i2;
	return loadBetween(t1, i1, t2, i2);
}

void forgetLoadReadingForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> lock(loadLock());
	lastReading() = LoadReading();
}

void readLoad(double avg[3], unsigned &running, unsigned &total)
{
	avg[0] = avg[1] = avg[2] = 0;
	running = total = 0;
	std::FILE *f = std::fopen((std::string(proc_root) + "/loadavg").c_str(), "r");
	if (f == NULL)
		return;
	if (std::fscanf(f, "%lf %lf %lf %u/%u", &avg[0], &avg[1], &avg[2], &running, &total) != 5)
	{
		avg[0] = avg[1] = avg[2] = 0;
		running = total = 0;
	}
	std::fclose(f);
}

bool readTemperature(int &celsius)
{
	const std::string dir = std::string(sys_root) + "/class/thermal";
	DIR *d = opendir(dir.c_str());
	if (d == NULL)
		return false;
	const int none = -1000;
	int package = none, core = none, first = none, first_zone = -1;
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
	{
		if (std::strncmp(e->d_name, "thermal_zone", 12) != 0 || !allDigits(e->d_name + 12))
			continue;
		const int zone = std::atoi(e->d_name + 12);
		const std::string base = dir + "/" + e->d_name;
		std::ifstream type_in((base + "/type").c_str());
		std::string type;
		std::getline(type_in, type);
		std::ifstream temp_in((base + "/temp").c_str());
		long milli = 0;
		if (!(temp_in >> milli))
			continue;
		const int t = (int) (milli / 1000);
		if (type.find("pkg") != std::string::npos)
			package = t;
		else if (type.find("core") != std::string::npos || type.find("Core") != std::string::npos)
			core = core > t ? core : t;
		else if (first_zone < 0 || zone < first_zone)
		{
			first = t;
			first_zone = zone;
		}
	}
	closedir(d);
	celsius = package != none ? package : core != none ? core : first;
	return celsius != none;
}

std::string readOsName()
{
	std::ifstream in((std::string(etc_root) + "/os-release").c_str());
	std::string line;
	while (std::getline(in, line))
	{
		if (line.compare(0, 12, "PRETTY_NAME=") != 0)
			continue;
		std::string v = line.substr(12);
		if (v.size() >= 2 && (v[0] == '"' || v[0] == '\'') && v[v.size() - 1] == v[0])
			v = v.substr(1, v.size() - 2);
		return v;
	}
	return std::string();
}

} // namespace internal

} // namespace system
} // namespace coreapi
