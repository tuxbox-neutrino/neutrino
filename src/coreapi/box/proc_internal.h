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

} // namespace internal
} // namespace system
} // namespace coreapi

#endif
