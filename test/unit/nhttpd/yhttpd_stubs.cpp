/*
 * yhttpd_stubs.cpp - what libyhttpd.a reaches for that no test needs
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The core archive is linked into the test binary for CyhookHandler,
 * CWebserverRequest and CySocket. Two of its objects name statics that
 * live in the server and in the control API, neither of which is linked
 * here: the server-wide config list, and the plugin directories the
 * script runner searches. Both are defined empty so the link closes.
 */

#include <string>

#include "yhttpd.h"
#include "tuxboxapi/controlapi.h"

CStringList Cyhttpd::ConfigList;
std::string CControlAPI::PLUGIN_DIRS[CControlAPI::PLUGIN_DIR_COUNT];
