/*
 * Wireless networks for the setup menu, through iwd or wpa_supplicant
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <config.h>

#include "wlan_client.h"
#ifdef ENABLE_IWD
#include "iwd_client.h"
#else
#include "wpa_client.h"
#endif

CWlanClient *CWlanClient::getInstance()
{
#ifdef ENABLE_IWD
	return CIwdClient::getInstance();
#else
	return CWpaClient::getInstance();
#endif
}

void CWlanClient::wipe(std::string &secret)
{
	volatile char *p = secret.empty() ? NULL : &secret[0];
	for (size_t i = 0; p && i < secret.length(); i++)
		p[i] = 0;
	secret.clear();
}
