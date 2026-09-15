/*
 * shellcontract.h - what the shell may be asked to do, and with which arguments
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

#ifndef __coreapi_shellcontract_h__
#define __coreapi_shellcontract_h__

#include "coreapi/base/result.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace coreapi
{

enum class ParamType
{
	Int,
	String,
	Bool,
	Enum,
	Path,
	File
};

// An aggregate with no constructor of its own, so a table of these is a constant
// in read only memory rather than something a startup routine builds.
//
// min and max bound an Int, and bound the length of a String, a Path or a File
// when max is above zero. An Int that leaves both at zero therefore accepts only
// zero. root is the directory a Path or a File has to resolve inside; one whose
// table names no root is refused, so forgetting it cannot reach the filesystem.
// A Path accepts any name resolving inside the root, a directory among them; a
// File accepts only a regular file.
struct Param
{
	const char *name;
	ParamType   type;
	bool        required;
	const char *doc;
	long        min;
	long        max;
	const char *values;
	const char *root;
};

// Writes a table's parameter array and its count as one pair, so the two cannot
// disagree: a count one too large reads a Param that is not there.
#define COREAPI_PARAMS(a) (a), (sizeof(a) / sizeof((a)[0]))

// What the caller should expect the script to have written. Nothing here reads
// it; run() hands back the bytes as they came.
enum class OutputFormat
{
	Json,
	Lines,
	KeyValue,
	Raw
};

// Every field has to be given. This is an aggregate, so a table that stops early
// leaves the rest zero, and a zero timeout or a zero output ceiling is refused
// rather than obeyed.
//
// end_options puts an end of options element between the verb and the
// parameters. Set it only for a program that honours the convention: one that
// does not gets the element as data, a shell script reading it as a positional
// argument so everything behind it shifts, and /bin/echo printing it in front of
// every answer.
struct ShellCommand
{
	const char  *script;
	const char  *verb;
	const Param *params;
	size_t       param_count;
	OutputFormat output;
	int          timeout_ms;
	size_t       max_output;
	bool         end_options;
};

namespace shell
{

// Runs script with an argv this layer builds: the script, the verb when the
// table names one, the end of options element when the table asks for one, then
// one element per declared parameter in table order. No shell is involved, so a
// value cannot become syntax, and the order is the table's, so a script keeps
// reading the same positions whatever order the arguments arrived in. A declared
// parameter the caller left out becomes an empty element rather than no element,
// so the positions behind it do not shift.
//
// A name the table does not declare, a second value for a name already given, an
// empty value for a required parameter and a value outside what the table allows
// are all refused, and nothing is started for a refused request. A value never
// reaches the script as the caller wrote it: an Int arrives as its decimal form,
// a Bool as 0 or 1, a Path or a File as what it resolved to.
//
// What a value cannot be kept from meaning. A produced element beginning with an
// ASCII hyphen is refused, because a program that reads options reads them
// wherever they sit and the last argv slot is no protection. That covers the
// hyphen convention and only that: a program with an option syntax of its own
// still reads one, and /bin/date obeys a value of +%Y. Nor is an element one
// word to anyone but this layer: a script that writes $1 without quoting it
// splits it again on whitespace, and no check on an argv element can reach that.
// A table declares a hyphen is meant by asking for the end of options element,
// or, for an Int, by declaring a negative min; the second changes what the
// program does and not only what it reads, because a bare negative number is a
// short option to ls, head, tail and uniq.
//
// What the timeout reaches: the script and its process group. A descendant that
// leaves the group outlives the run. Neither is anything reached in a process
// that has told the kernel to discard what its children exit with, because then
// this cannot tell a group of its own from a number handed on; a run is refused
// outright in that state, and one already running is reported as unsupervised
// rather than as a timeout. A script that leaves a background process holding
// its output behind is cut at its timeout and the group killed even though the
// script itself succeeded.
//
// What a Path or a File check is worth: it answers for a name at the moment it
// is checked, not for what the script later opens under that name. Anything in
// that name can be put back differently in between, the root itself as readily
// as something under it, because replacing a directory needs a write to its
// parent and not to the directory. So the confinement holds only where every
// directory from the filesystem root down to and including the root, and the
// contents of the root, are beyond the reach of anyone who can reach this.
//
// The child is handed an environment this layer names, not the one the server
// happens to be running under.
//
// InvalidArgument answers for the request, Internal for the table, for a script
// that failed and for a run this process cannot supervise, NotFound for a script
// that cannot be started at all and Busy for one that outran its timeout.
Result<std::string> run(const ShellCommand &cmd,
			const std::vector<std::pair<std::string, std::string> > &args);

} // namespace shell
} // namespace coreapi

#endif
