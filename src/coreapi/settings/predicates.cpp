/*
 * predicates.cpp - availability tests for settings and for entries of an option list
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

#include "coreapi/settings/predicates.h"

#include "coreapi/base/deps.h"

#include <config.h>

#include <string.h>

namespace coreapi
{

namespace
{

// Zero when the box cannot say, so every test below that asks for a capability
// fails closed. ok says whether it could, for the tests on a number a zero is
// also a valid answer for.
BoxCapabilities capabilities(bool *ok = NULL)
{
	BoxCapabilities caps;
	memset(&caps, 0, sizeof(caps));
	const bool asked = systemSourceInstalled() && systemSource().capabilities(caps) == Status::Ok;
	if (!asked)
		memset(&caps, 0, sizeof(caps));
	if (ok != NULL)
		*ok = asked;
	return caps;
}

} // anonymous namespace

bool hasHdmi()
{
	return capabilities().has_HDMI;
}

bool hasFan()
{
	return capabilities().has_fan;
}

bool canCec()
{
	return capabilities().can_cec;
}

bool canCpufreq()
{
	return capabilities().can_cpufreq;
}

bool canSetBrightness()
{
	return capabilities().display_can_set_brightness;
}

bool pipUsable()
{
	const BoxCapabilities caps = capabilities();
	return caps.can_pip && caps.pip_boot_mode_ok;
}

bool canShutdown()
{
	return capabilities().can_shutdown;
}

bool hasFormatButton()
{
	return capabilities().has_button_vformat;
}

bool displayFitsPlaytime()
{
	return capabilities().display_xres >= 8;
}

bool takesZappingMode()
{
	return capabilities().video_zapmode;
}

bool takesHdmiColorimetry()
{
	return capabilities().video_hdmi_colorimetry;
}

bool analogOutputsSplit()
{
	return capabilities().board_revision > 0x06;
}

bool hasAnalogCinch()
{
	// The newer library drives SCART and Cinch together, as one item.
#if defined(BOXMODEL_CST_HD2) && defined(ANALOG_MODE)
	return false;
#else
	return analogOutputsSplit();
#endif
}

/* The tests below on a revision are the screens' own, which pass revision 0:
   it is a revision. A box that cannot be asked is told apart by the status. */
bool hasDbdr()
{
	bool ok;
	const BoxCapabilities caps = capabilities(&ok);
	return ok && caps.board_revision != 1;
}

bool hasLedMenu()
{
	return capabilities().board_revision > 7;
}

bool hasBacklight()
{
	return capabilities().board_revision == 9;
}

namespace
{
bool revisionHasPanel(unsigned int rev)
{
	return rev != 10 && rev != 11;
}
} // anonymous namespace

bool vfdEnabled()
{
	bool ok;
	const BoxCapabilities caps = capabilities(&ok);
	return ok && revisionHasPanel(caps.board_revision);
}

bool canSetPanelBrightness()
{
	bool ok;
	const BoxCapabilities caps = capabilities(&ok);
	return ok && caps.display_can_set_brightness && revisionHasPanel(caps.board_revision);
}

bool canSelectRemote()
{
	return capabilities().rc_hw_select;
}

bool ciSlotFitted(unsigned slot)
{
	unsigned n = 0;
	if (!systemSourceInstalled() || systemSource().ciSlotCount(n) != Status::Ok)
		return false;
	return slot < n;
}

bool severalTunersFitted()
{
	return capabilities().frontend_count > 1;
}

// The settings of the module slots as a whole are offered where the box has a slot.
bool hasCiSlot()
{
	return ciSlotFitted(0);
}

} // namespace coreapi
