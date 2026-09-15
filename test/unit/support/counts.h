/*
 * counts.h - counting what the tests touched, for the identity check
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

#ifndef COREAPI_TEST_COUNTS_H
#define COREAPI_TEST_COUNTS_H

#include <cstddef>

/* How much of the program each case here actually compares, held to a file rather than
   to a constant beside each case.

   The cases below this walk generated tables, and a scan that stops matching leaves
   them walking nothing and passing: one section dropped from the joined table once left
   every one of them green over the rows that were left. What stops that is a number,
   and the question is where the number lives. Written as a constant beside each case it
   takes a legitimate deletion four to six hand edits across three files, each of them
   one line saying make the number smaller, which is a habit and not a check.

   So each case states what it compared and one file carries all of them. A number that
   has moved fails the run, in either direction, and says which way. Both directions are
   failures because either can hide the other. */

// What one case compared. The same name twice with two different values is a
// fault of the cases and is reported as one.
void recordCount(const char *name, size_t value);

/* Every recorded name against the file, and the file against them. Writes what this run
   measured to actual_path whatever the answer, so the file to check in is always there.
   False when anything differs, is missing on either side, or the file cannot be read.

   whole says every case ran to its end. After a failure it is false and only the numbers
   a case did record and got wrong are named: a case that stops on its first bad line
   records nothing after it, so the rest are unknown rather than fallen. */
bool coverageCountsAgree(const char *expected_path, const char *actual_path, bool whole);

#endif
