/*
 * proc_internal.h - where the process list and the kernel log are read from
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_proc_internal_h__
#define __coreapi_proc_internal_h__

#include "coreapi/base/result.h"

#include <string>

namespace coreapi
{
namespace system
{
namespace internal
{

/* The directory the process list is read out of, /proc on a box. A case
   builds a tree of its own in that shape and points this at it. */
extern const char *proc_root;

/* What reads the kernel's log, NULL for the kernel itself. A case puts a
   reader of its own here, since a host that is not root may be refused the
   real one. */
typedef Result<std::string> (*KernelLogReader)();
extern KernelLogReader kernel_log_reader;

/* The readings the box information carries beside the process list, out of
   proc_root and these two, /sys and /etc on a box; a case points them at a
   tree of its own. */
extern const char *sys_root;
extern const char *etc_root;

// The processor as cpuinfo names it (x86 "model name", else ARM "Processor"
// or "Hardware") and its cores, at least 1.
void readCpu(std::string &model, unsigned &cores);
// The first line of stat: all the time the processors spent, and of it idle.
bool cpuTimes(unsigned long long &total, unsigned long long &idle);
// How busy the processors were between two such readings, in percent; -1 for
// readings that do not follow one another.
int loadBetween(unsigned long long total1, unsigned long long idle1, unsigned long long total2,
		unsigned long long idle2);
/* How busy they were lately: since the last reading where that is between a
   fifth of a second and ten seconds old, so a page asking every few seconds
   waits for nothing; otherwise over a tenth of a second, waited for. -1 where
   stat cannot be read. */
int cpuLoadPercent();
void forgetLoadReadingForTest();
// The averages and the tasks of loadavg; all nought where it cannot be read.
void readLoad(double avg[3], unsigned &running, unsigned &total);
/* The processor's temperature in degrees Celsius out of every thermal zone
   there is, numbered with gaps or not: a package zone first, else the hottest
   core, else the lowest-numbered zone. False where there is none. */
bool readTemperature(int &celsius);
// PRETTY_NAME of os-release, unquoted; empty where there is none.
std::string readOsName();

} // namespace internal
} // namespace system
} // namespace coreapi

#endif
