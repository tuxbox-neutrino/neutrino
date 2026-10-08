/*
 * boundedrows.h - a number row whose range the box states at run time
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

#ifndef __support_boundedrows_h__
#define __support_boundedrows_h__

#include "coreapi/settings/settingsfield.h"
#include "support/fakes.h"

/* The one row the cases about runtime bounds share: a number inside the constant range 0 to 1000
   whose providers are the ones BoundedProvider steers. */
static const coreapi::Descriptor kBoundedRows[] =
{
	{
		"t_bounded", coreapi::ValueType::Int, "fixture", "label", NULL,
		0, 1000, NULL, 0, 10, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NUMBER_FIELD(repeat_blocker),
		NULL, NULL, NULL, NULL, boundedLowNow, boundedHighNow, NULL
	},
};
static const size_t kBoundedRowsCount = sizeof(kBoundedRows) / sizeof(kBoundedRows[0]);

#endif
