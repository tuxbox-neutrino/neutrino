/*
 * waitslice.cpp - a bounded wait taken in slices
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
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
 */

#include "waitslice.h"

namespace waitslice
{

int nextSliceMs(int64_t total, int64_t elapsed, int slice)
{
	if (elapsed < 0)
		elapsed = 0;
	const int64_t left = total - elapsed;
	if (left <= 0 || slice <= 0)
		return 0;
	return left < slice ? (int) left : slice;
}

} // namespace waitslice
