/*
 * localesource_real.cpp - locale strings taken from the running GUI
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

#include <global.h>
#include <system/localize.h>

namespace coreapi
{

namespace
{

/* The one real catalog: whatever CLocaleManager::getLocale finds the name under,
   read back through whatever g_Locale currently has loaded. Both are reached
   fresh on every call rather than bound once at install time, because the
   language loaded can change while the box runs.

   Its own translation unit for the reason every other adapter here has one: a
   binary that only wants the accessor in deps.h must not have to link
   CLocaleManager or resolve g_Locale at all. */
class RealLocaleSource : public LocaleSource
{
	public:
		Status text(const char *key, std::string &out) const
		{
			const neutrino_locale_t locale = CLocaleManager::getLocale(key);
			if (locale == NONEXISTANT_LOCALE)
				return Status::NotFound;

			/* A build that links this adapter without starting the box it
			   belongs to, such as the one that writes out this API's own
			   document, carries no CLocaleManager instance at all. Answering
			   NotFound rather than reading through a null pointer is what lets
			   that stay true by construction. */
			if (g_Locale == NULL)
				return Status::NotFound;

			/* The copying accessor and not the one that hands the pointer
			   over: this runs on the request's own thread, and a language
			   being loaded frees the block that pointer names. */
			out = g_Locale->getString(locale);
			return Status::Ok;
		}
};

RealLocaleSource g_real_locale;

} // anonymous namespace

void installRealLocaleSource()
{
	setLocaleSource(&g_real_locale);
}

} // namespace coreapi
