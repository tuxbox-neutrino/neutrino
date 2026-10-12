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
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/klog.h>
#include <sys/stat.h>
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

} // namespace system
} // namespace coreapi
