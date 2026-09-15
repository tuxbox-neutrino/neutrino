/*
 * shellcontract_internal.h - shell contract internals shared by the table and the check
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

#ifndef __coreapi_shellcontract_internal_h__
#define __coreapi_shellcontract_internal_h__

// Not part of the contract the consumers of this layer see. It exists so the
// suite can drive the descriptor path the box's kernel is the only one to take.

namespace coreapi
{
namespace shell
{
namespace internal
{

// The kernels this ships to have no close_range, so the loop behind it is the
// only way the box closes what the child must not keep. Read once per run, in
// the calling thread and before the fork; write it only with no run in flight.
extern bool try_close_range;

} // namespace internal
} // namespace shell
} // namespace coreapi

#endif
