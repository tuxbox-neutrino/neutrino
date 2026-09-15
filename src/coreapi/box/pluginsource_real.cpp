/*
 * pluginsource_real.cpp - plugin list read from the box plugin manager
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

#include "coreapi/base/deps.h"

namespace coreapi
{

namespace
{

/* Its own translation unit for the reason the sinks beside it have one: the
   whole of what it is made of is defined by the program and by nothing a build
   that only wants this layer links. Nothing here reads the list itself; what
   reads it is a header this layer may not include, so this is the near side of
   that one call and the list arrives already in the words of this layer. */
class RealPluginSource : public PluginSource
{
	public:
		Status list(PluginList &out) const
		{
			return boxPlugins(out);
		}
};

RealPluginSource g_real_plugin_source;

} // anonymous namespace

void installRealPluginSource() { setPluginSource(&g_real_plugin_source); }

} // namespace coreapi
