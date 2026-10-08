/*
 * couple_epg.cpp - what the guide settings imply of each other
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

/* A guide that is saved has to be read back, or the next start throws away what the
   save kept. Writing the save on turns the read on with it, and a write that also turns
   the read off asks for two things that cannot both hold. */
void coupleEpg(CoupledBatch &b)
{
	long save = 0;
	if (b.writtenNumber("epg_save", save) && save != 0)
		b.imply("epg_read", "1", "epg_save");
}

} // namespace settings
} // namespace coreapi
