/*
 * couple_lcd4l.cpp - what the LCD4Linux panel limits
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

#include "couple.h"

namespace coreapi
{
namespace settings
{

long lcd4lBrightnessCeiling(long display_type)
{
	/* The Pearl panel, and any type not known, seven; every other panel the driver
	   knows ten: the Samsung ones, the VU+ Solo 4K one and the PNG outputs,
	   src/driver/lcd4l.cpp:269. */
	return (display_type >= 1 && display_type <= 7) ? 10 : 7;
}

} // namespace settings
} // namespace coreapi
