/*
	Neutrino-HD

	Copyright (C) 2025 Thilo Graf

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __SYSTEM_SUPPORTINFO_H__
#define __SYSTEM_SUPPORTINFO_H__

#include <string>

/**
 * @brief Central class for support/contact URLs
 *
 * Provides a single source of truth for homepage, documentation and forum URLs.
 * Used by both GUI (imageinfo) and API (controlapi) to ensure consistency.
 *
 * URL resolution priority:
 * 1. /etc/os-release (freedesktop.org standard: HOME_URL, DOCUMENTATION_URL, SUPPORT_URL)
 * 2. IMAGE_VERSION_FILE (legacy: homepage=, docs=, forum=)
 * 3. Compile-time defaults (PACKAGE_URL, hardcoded wiki/forum URLs)
 */
class CSupportInfo
{
	public:
		/**
		 * @brief Get singleton instance
		 * @return Reference to the singleton instance
		 */
		static CSupportInfo& getInstance();

		/**
		 * @brief Reload URLs from config files
		 * Call this if config files have changed at runtime
		 */
		void reload();

		/**
		 * @brief Get homepage URL
		 * @return Homepage URL string
		 */
		const std::string& getHomepage() const { return homepage; }

		/**
		 * @brief Get documentation/wiki URL
		 * @return Documentation URL string
		 */
		const std::string& getDocs() const { return docs; }

		/**
		 * @brief Get forum/support URL
		 * @return Forum URL string
		 */
		const std::string& getForum() const { return forum; }

		/**
		 * @brief Check if homepage URL is available
		 * @return true if homepage is not empty
		 */
		bool hasHomepage() const { return !homepage.empty(); }

	private:
		CSupportInfo();
		~CSupportInfo() = default;

		// Prevent copying
		CSupportInfo(const CSupportInfo&) = delete;
		CSupportInfo& operator=(const CSupportInfo&) = delete;

		void loadUrls();

		std::string homepage;
		std::string docs;
		std::string forum;
};

#endif // __SYSTEM_SUPPORTINFO_H__
