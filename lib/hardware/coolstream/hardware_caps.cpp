/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012,2016 Stefan Seyfried
 *
 * License: GPL v2 or later
 */
#include "cs_api.h"
#include <stdio.h>
#include <string.h>
#include "hardware_caps.h"

#include <zapit/femanager.h>

static int initialized = 0;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void) {
	if (initialized)
		return &caps;
	int rev = cs_get_revision();
	int chip = cs_get_chip_type();
	caps.has_fan = (rev < 8 && CFEManager::getInstance()->getFE(0)->hasSat()); // only SAT-HD1 before rev 8 has fan
	caps.has_HDMI = 1;
	caps.has_SCART = (rev != 10);
	caps.has_SCART_input = 0;
	caps.has_YUV_cinch = 1;
	caps.can_shutdown = (rev > 7);
	caps.can_cec = 1;
	caps.display_type = HW_DISPLAY_LINE_TEXT;
	caps.display_xres = 12;
	caps.display_yres = 0;
	caps.can_set_display_brightness = 1;
	caps.can_ar_14_9 = 1;
	caps.can_ps_14_9 = 1;
	caps.force_tuner_2G = 0;
	strcpy(caps.boxvendor, "Coolstream");
	switch (rev) {
	case 6:
	case 7: // Black Stallion Edition
		strcpy(caps.boxname, "HD1");
		strcpy(caps.boxarch, "Nevis");
		caps.force_tuner_2G = 1;
		break;
	case 8:
		if (CFEManager::getInstance()->getFrontendCount() < 2)
		{
			strcpy(caps.boxname, "Neo");
		}
		else
		{
			strcpy(caps.boxname, "Neo Twin");
		}
		strcpy(caps.boxarch, "Nevis");
		caps.force_tuner_2G = 1;
		break;
	case 9:
		strcpy(caps.boxname, "Tank");
		strcpy(caps.boxarch, "Apollo");
		break;
	case 10:
		strcpy(caps.boxname, "Zee");
		strcpy(caps.boxarch, "Nevis");
		caps.force_tuner_2G = 1;
		break;
	case 11:
		if (chip == CS_CHIP_SHINER)
		{
			strcpy(caps.boxname, "Trinity");
			strcpy(caps.boxarch, "Shiner");
		}
		else
		{
			strcpy(caps.boxname, "Trinity V2");
			strcpy(caps.boxarch, "Kronos");
		}
		caps.can_set_display_brightness = 0;
		break;
	case 12:
		strcpy(caps.boxname, "Zee2");
		strcpy(caps.boxarch, "Kronos");
		break;
	case 13:
		strcpy(caps.boxname, "Link");
		strcpy(caps.boxarch, "Kronos");
		break;
	case 14:
		strcpy(caps.boxname, "Trinity Duo");
		strcpy(caps.boxarch, "Kronos");
		caps.can_set_display_brightness = 0;
		break;
	default:
		strcpy(caps.boxname, "UNKNOWN_BOX");
		strcpy(caps.boxarch, "Unknown");
		fprintf(stderr, "[%s] unhandled box revision %d\n", __func__, rev);
	}
	initialized = 1;
	return &caps;
}
