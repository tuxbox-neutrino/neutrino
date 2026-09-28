/*
 * mount.h - the legacy surface's counting seam, as nhttpd feeds it
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * This is not the source's compat/mount.h. That header declares their whole
 * reimplementation of the copied control API; on nhttpd that surface is
 * CControlAPI's own, alive in src/nhttpd, and nothing of the mount is
 * wanted here - except the count /api/v1/system/legacy-usage answers
 * with, which ep_system.cpp reads as written, through the same names
 * (compat::Usage, compat::usage()). This file states that one seam and
 * the feeding half nhttpd uses, and it is this tree's file, kept under
 * the include path the verbatim reader expects.
 *
 * What the counts mean here differs from the source in one stated way: the
 * mount there counts a name before its own credential gate, so a
 * stranger it turns away still shows demand. On nhttpd authentication
 * happens in front of the hook chain (mod_auth), so a caller nhttpd
 * turned away never reaches the module that counts. These counts are
 * the demand that reached CControlAPI, on /control/ and its /cgi-bin/
 * alias alike.
 */

#ifndef __httpd_compat_mount_h__
#define __httpd_compat_mount_h__

#include <cstddef>
#include <string>
#include <vector>

namespace httpd
{
namespace compat
{

// One name the legacy surface has been asked for since this program
// started, and how many times: a name the known table holds, or one it
// does not (usage() below is what tells the two apart).
struct Usage
{
	std::string   name;
	unsigned long calls;
};

/* The ceiling on names outside the known table. Generous next to the
   table itself, and still a hard bound on what asking for ever new
   names can cost this program, at no charge to whoever asks. */
extern const std::size_t kMaxUnknownLegacyNames;

/* One name of the known table. CControlAPI hands its dispatch table in
   here, one name per call, in the table's own order; a name already
   known keeps its first seat, so seeding twice cannot double the
   listing. */
void knowName(const char *name);

// One call for a name, known or not. What CControlAPI resolves is what
// it records, before the lookup decides whether anything answers.
void record(const std::string &name);

/* Every name this seam has counted a call for: the known table first,
   in its own order and each carried even at zero, then every other
   name still remembered, in the order each was first seen, up to
   kMaxUnknownLegacyNames of them. Read by /api/v1/system/legacy-usage
   (ep_system.cpp) and by nothing else that ships. */
std::vector<Usage> usage();

// The suite's clean slate: forgets the known table and every count.
void resetForTest();

} // namespace compat
} // namespace httpd

#endif
