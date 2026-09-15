/*
 * shelltable.h - the shell commands the box offers, one row each
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

#ifndef __coreapi_shelltable_h__
#define __coreapi_shelltable_h__

#include "coreapi/base/result.h"
#include "shellcontract.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace coreapi
{
namespace shell
{

// A table entry carries everything a ShellCommand does except the script's
// directory. The runner refuses a script that is not absolute, so an entry
// cannot name one by its bare name; and a literal absolute name would be the one
// the box installs to, which no host running the suite has. So an entry keeps
// the tail alone and the absolute name is joined per call out of a root the
// caller sets.
//
// An aggregate with no constructor, so the table below is a constant in read
// only memory rather than something a startup routine builds.
struct TableEntry
{
	const char  *name;
	const char  *script;
	const char  *verb;
	const Param *params;
	size_t       param_count;
	OutputFormat output;
	int          timeout_ms;
	size_t       max_output;
	bool         end_options;
};

// Defaults to the directory the image installs them in. A trailing separator is
// dropped, so the joined name never carries a doubled one.
void setScriptRoot(const std::string &dir);
std::string scriptRoot();

// NULL for a name the table does not carry.
const TableEntry *find(const std::string &name);

const TableEntry *entryAt(size_t index);
size_t entryCount();

// Joins the entry's script to the script root and hands the whole thing to the
// runner, which is where every check on the request happens.
Result<std::string> runNamed(const std::string &name,
			     const std::vector<std::pair<std::string, std::string> > &args);

} // namespace shell
} // namespace coreapi

#endif
