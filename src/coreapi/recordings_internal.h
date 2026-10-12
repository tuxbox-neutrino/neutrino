/*
 * recordings_internal.h - recording folder walk internals the tests reach
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_recordings_internal_h__
#define __coreapi_recordings_internal_h__

#include <stddef.h>

#include <string>

namespace coreapi
{
namespace recordings
{
namespace internal
{

// How long a request waits for the folder walk; a negative value puts it back.
const long kFolderWaitMs = 3000;
void setFolderWaitForTest(long wait_ms);

// How many directory entries one walk reads in all, files among them.
const size_t kFolderEntryBudget = 16384;

/* Called on the walk's thread before it touches a directory, a root or one
   below it, so a case can stand in for a share that never answers. Cleared by
   passing NULL. */
typedef void (*FolderProbe)(const std::string &dir);
void setFolderProbeForTest(FolderProbe p);

} // namespace internal
} // namespace recordings
} // namespace coreapi

#endif
